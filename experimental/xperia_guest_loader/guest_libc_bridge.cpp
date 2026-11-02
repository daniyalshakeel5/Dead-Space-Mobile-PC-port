#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <stdexcept>

namespace xperia {
namespace {
constexpr std::uint32_t kMaxTransfer=16u*1024u*1024u;
constexpr std::uint32_t kMaxString=1024u*1024u;
std::uint8_t byte(GuestElf32& image,std::uint32_t p) {
    const auto* at=image.read(p,1);
    if(!at)throw std::runtime_error("Unmapped ARM guest string byte");
    return *at;
}
std::uint32_t compare_strings(GuestElf32& image,std::uint32_t lhs,
                              std::uint32_t rhs,std::uint32_t max,bool require_terminator) {
    for(std::uint32_t i=0;i<max;++i) {
        if(std::uint64_t(lhs)+i>UINT32_MAX ||
           std::uint64_t(rhs)+i>UINT32_MAX)
            throw std::runtime_error("ARM string pointer wrap");
        auto a=byte(image,lhs+i),b=byte(image,rhs+i);
        if(a!=b)return std::uint32_t(a<b?-1:1);
        if(a==0)return 0;
    }
    if(require_terminator)
        throw std::runtime_error("Unterminated guest strcmp buffer");
    return 0;
}
}
GuestLibcBridge::GuestLibcBridge() {
    static const char* const names[]={
        "memcpy","memmove","memset","memcmp","strlen","strcmp","strncmp",
        "malloc","calloc","free","realloc"
    };
    for(unsigned i=0;i<11;++i){
        const std::uint32_t id=i+1;
        symbols_.emplace(names[i],veneer_base+i*8);
        const std::uint32_t instructions[2]={0xef000000u|id,0xe12fff1eu};
        for(auto ins:instructions)
            for(unsigned j=0;j<4;++j)
                code_.push_back(std::uint8_t(ins>>(8u*j)));
    }
}
bool GuestLibcBridge::code_byte(std::uint32_t address,
                                std::uint8_t& value) const noexcept {
    if(address<veneer_base ||
       std::uint64_t(address)-veneer_base>=code_.size())return false;
    value=code_[address-veneer_base];return true;
}
bool GuestLibcBridge::dispatch(
    std::uint32_t id,std::array<std::uint32_t,16>& regs,
    GuestElf32& image,std::string& failure,GuestHeap* heap) const noexcept {
    try {
        if(id<1||id>11)
            throw std::runtime_error("Unknown Android libc SVC service");
        const auto a=regs[0],b=regs[1],n=regs[2];
        std::uint32_t result=0;
        switch(id){
        case 1: // memcpy
        case 2: { // memmove (also safe when pointers overlap)
            if(n>kMaxTransfer)
                throw std::runtime_error("ARM memory copy exceeds limit");
            if(n) {
                auto* dest=image.writeable(a,n);
                const auto* src=image.read(b,n);
                if(!dest||!src)throw std::runtime_error("Unmapped ARM copy");
                std::memmove(dest,src,n);
            }
            result=a;break;
        }
        case 3: { // memset
            if(n>kMaxTransfer)
                throw std::runtime_error("ARM memset exceeds limit");
            if(n){
                auto* dest=image.writeable(a,n);
                if(!dest)throw std::runtime_error("Unmapped ARM memset");
                std::memset(dest,int(b&255u),n);
            }
            result=a;break;
        }
        case 4: { // memcmp
            if(n>kMaxTransfer)
                throw std::runtime_error("ARM memcmp exceeds limit");
            if(n){
                const auto* x=image.read(a,n),*y=image.read(b,n);
                if(!x||!y)throw std::runtime_error("Unmapped ARM memcmp");
                const int cmp=std::memcmp(x,y,n);
                result=std::uint32_t(cmp<0?-1:cmp>0?1:0);
            }
            break;
        }
        case 5: { // strlen
            bool ended=false;
            for(std::uint32_t i=0;i<kMaxString;++i){
                if(std::uint64_t(a)+i>UINT32_MAX)
                    throw std::runtime_error("ARM strlen pointer wrap");
                if(!byte(image,a+i)) {
                    result=i;ended=true;break;
                }
            }
            if(!ended)throw std::runtime_error("ARM strlen was unterminated");
            break;
        }
        case 6: result=compare_strings(image,a,b,kMaxString,true);break;
        case 7:
            if(n>kMaxString)
                throw std::runtime_error("ARM strncmp exceeds limit");
            result=compare_strings(image,a,b,n,false);break;
        case 8: // malloc(size)
        case 9: // calloc(count, size)
        case 10: // free(pointer)
        case 11: { // realloc(pointer, size)
            if(!heap)
                throw std::runtime_error("ARM libc heap service has no guest heap");
            if(id==8)result=heap->allocate(a);
            if(id==9)result=heap->allocate_zeroed(a,b);
            if(id==10) {
                if(!heap->release(a))
                    throw std::runtime_error("Invalid ARM guest free");
                result=0;
            }
            if(id==11)result=heap->resize(a,b);
            break;
        }
        }
        regs[0]=result;
        failure.clear();return true;
    }catch(const std::exception& err){
        failure=err.what();return false;
    }
}
} // namespace xperia
