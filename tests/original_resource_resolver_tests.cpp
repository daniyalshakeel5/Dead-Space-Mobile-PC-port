#include "ds_port/original_resource_resolver.hpp"
#include "ds_port/original_mode1_resource_factory.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstring>
using namespace ds_port::original_runtime;
namespace {
struct Fixture {
    std::array<std::uint8_t,0x400> buffer{};
    char calls[32]{};unsigned count=0;
    unsigned lookup_result=0,transform_result=0x2710;
    bool deny_lookup=false,deny_pool=false;
    void call(char c){assert(count<sizeof(calls));calls[count++]=c;}
};
bool pool(unsigned bytes,unsigned owner,void* x,unsigned* result) noexcept {
    auto& f=*static_cast<Fixture*>(x);f.call('P');
    assert(bytes==0x14&&owner==0xABCD);
    if(f.deny_pool)return false;
    *result=0x2000;return true;
}
bool pool_init(unsigned pool,unsigned bytes,unsigned flag,
               void* x,void** buffer,std::size_t* size) noexcept {
    auto& f=*static_cast<Fixture*>(x);f.call('I');
    assert(pool==0x2000&&bytes==0x400&&flag==0);
    *buffer=f.buffer.data();*size=f.buffer.size();return true;
}
bool object_alloc(unsigned bytes,void* x,unsigned* ptr) noexcept {
    static_cast<Fixture*>(x)->call('O');
    assert(bytes==0x30);*ptr=0x3000;return true;
}
bool object_init(unsigned obj,unsigned pool,unsigned bytes,
                 unsigned flag,void* x) noexcept {
    static_cast<Fixture*>(x)->call('C');
    return obj==0x3000&&pool==0x2000&&bytes==0x400&&flag==0;
}
bool param(unsigned obj,unsigned mode,unsigned bits,void* x) noexcept {
    static_cast<Fixture*>(x)->call('A');
    return obj==0x3000&&mode==1&&bits==0x3f800000u;
}
bool copy_name(std::string_view name,void* x) noexcept {
    static_cast<Fixture*>(x)->call('N');
    return name=="fixture";
}
bool lookup(unsigned r1,unsigned r2,unsigned r3,unsigned stack,
            void* x,unsigned* result) noexcept {
    auto& f=*static_cast<Fixture*>(x);f.call('L');
    assert(r1==1&&r2==6&&r3==1&&stack==0);
    if(f.deny_lookup)return false;
    *result=f.lookup_result;return true;
}
bool transform(unsigned size,void* x,unsigned* result,
               unsigned* scratch) noexcept {
    auto& f=*static_cast<Fixture*>(x);f.call('T');
    assert(size==0x2710);
    *result=f.transform_result;*scratch=0xB400;
    return true;
}
bool invoke(unsigned object,unsigned slot,unsigned a,unsigned b,
            void* x) noexcept {
    auto& f=*static_cast<Fixture*>(x);
    assert(object==0x3000);
    if(slot==0x3c) {
        f.call('V');assert(a==0xB400&&b==f.transform_result);
    } else {
        f.call('W');assert(slot==0x2c&&a==0&&b==0);
    }
    return true;
}
bool release(void* x) noexcept {
    static_cast<Fixture*>(x)->call('R');return true;
}
OriginalResolverInputs0x28a718 services(Fixture& f) {
    return {pool,pool_init,object_alloc,object_init,param,
            copy_name,lookup,transform,invoke,release,&f};
}
void trace(Fixture& f,const char* expected) {
    assert(f.count==std::strlen(expected));
    assert(std::memcmp(f.calls,expected,f.count)==0);
}
std::uint32_t load32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main(){
    Fixture f{};f.buffer.fill(0xCC);
    auto s=services(f);
    // Critical native behavior: failed lookup still releases scratch
    // and returns a non-null allocated 0x30-byte object.
    auto result=original_execute_resolver_0x28a718(s,0xABCD,"fixture");
    assert(result.status==OriginalResolverStatus0x28a718::Completed);
    assert(result.original_resource_object_va==0x3000);
    trace(f,"PIOCANLR");
    for(auto byte:f.buffer)assert(byte==0);
    // A successful lookup and exactly-sized transform invokes +0x3C,
    // but must NOT invoke original vtable +0x2C.
    f=Fixture{};f.lookup_result=1;s=services(f);
    result=original_execute_resolver_0x28a718(s,0xABCD,"fixture");
    assert(result.status==OriginalResolverStatus0x28a718::Completed);
    trace(f,"PIOCANLTVR");
    // Unequal transform output triggers +0x2C before release.
    f=Fixture{};f.lookup_result=1;f.transform_result=0x2700;s=services(f);
    result=original_execute_resolver_0x28a718(s,0xABCD,"fixture");
    assert(result.status==OriginalResolverStatus0x28a718::Completed);
    trace(f,"PIOCANLTVWR");
    // Refused lookup cannot be counted as original engine success.
    f=Fixture{};f.deny_lookup=true;s=services(f);
    result=original_execute_resolver_0x28a718(s,0xABCD,"fixture");
    assert(result.status==OriginalResolverStatus0x28a718::OriginalLookupBlocked);
    trace(f,"PIOCANL");
    // Missing allocation callback fails before native state changes.
    f=Fixture{};s=services(f);s.pool_alloc=nullptr;
    result=original_execute_resolver_0x28a718(s,0xABCD,"fixture");
    assert(result.status==OriginalResolverStatus0x28a718::MissingOriginalService);
    trace(f,"");
    // Integration: original mode-one handle factory now executes this
    // native wrapper instead of a generic resolver callback.
    f=Fixture{};f.lookup_result=0;s=services(f);
    std::array<std::uint8_t,16> handle{};
    OriginalMode1FactoryInputs in{handle.data(),handle.size(),nullptr,
                                  nullptr,&s};
    auto created=original_construct_mode1_handle_0x28b3a4(
        in,0xABCD,"fixture");
    assert(created.status==
           OriginalMode1FactoryStatus::CompletedOriginalMode1Constructor);
    trace(f,"PIOCANLR");
    assert(load32(handle.data())==kOriginalMode1HandleVtable);
    assert(load32(handle.data()+4)==1u);
    assert(load32(handle.data()+8)==0x3000u);
    assert(load32(handle.data()+12)==0xABCDu);
    return 0;
}
