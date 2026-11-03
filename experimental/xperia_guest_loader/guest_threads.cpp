#include "guest_threads.hpp"
#include <algorithm>
#include <array>
#include <cstring>
namespace xperia {
namespace {
void write4(std::uint8_t* dest,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)dest[i]=std::uint8_t(v>>(8u*i));
}
bool put(GuestElf32& elf,std::uint32_t addr,std::uint32_t value) {
    if((addr&3u)!=0)return false;
    auto* bytes=elf.writeable(addr,4);
    if(!bytes)return false;
    write4(bytes,value);return true;
}
} // namespace
bool GuestThreads::init_attr(GuestElf32& elf,std::uint32_t at,std::string& error){
    if(!at || (at&3u) || !elf.writeable(at,24u)) {
        error="ARM32 pthread_attr_init expects writable 24-byte Bionic object";
        return false;
    }
    auto* bytes=elf.writeable(at,24u);
    std::memset(bytes,0,24u);
    write4(bytes+8u,default_stack);
    write4(bytes+12u,default_guard);
    error.clear();return true;
}
bool GuestThreads::destroy_attr(GuestElf32& elf,std::uint32_t at,std::string& error){
    auto* bytes=elf.writeable(at,24u);
    if(!at || (at&3u) || !bytes){
        error="ARM32 pthread_attr_destroy expects writable 24-byte object";
        return false;
    }
    std::memset(bytes,0,24u);
    error.clear();return true;
}
bool GuestThreads::attr_field(GuestElf32& elf,std::uint32_t at,
                             unsigned off,std::uint32_t& value,std::string& error){
    if(!at || (at&3u) || off>=24u || off%4u || !elf.read(at,24u)){
        error="Invalid original Bionic 24-byte pthread attribute";
        return false;
    }
    value=elf.read_u32(at+off);
    error.clear();return true;
}
bool GuestThreads::set_attr_field(GuestElf32& elf,std::uint32_t at,
                                 unsigned off,std::uint32_t value,std::string& error){
    if(!at || (at&3u) || off>=24u || off%4u || !put(elf,at+off,value)||
       !elf.read(at,24u)){
        error="Invalid original Bionic pthread attribute address";
        return false;
    }
    error.clear();return true;
}
bool GuestThreads::create(GuestElf32& elf,std::uint32_t out,
                          std::uint32_t attr,std::uint32_t entry,std::uint32_t arg,
                          std::uint32_t& rc,std::string& error){
    if(!out || (out&3u) || !elf.writeable(out,4u) ||
       !entry || !elf.read(entry&~1u,2u)){
        error="Original pthread_create thread ID/output or ARM entry invalid";
        return false;
    }
    bool detached=false;
    if(attr){
        std::uint32_t flags=0,stack=0,guard=0;
        if(!attr_field(elf,attr,0u,flags,error) ||
           !attr_field(elf,attr,8u,stack,error) ||
           !attr_field(elf,attr,12u,guard,error))return false;
        if(flags&~1u || stack<16384u || stack>8u*1024u*1024u ||
           guard>1024u*1024u){
            error="Unsupported ARM Bionic detached flag/stack/guard";
            return false;
        }
        detached=bool(flags&1u);
    }
    if(threads_.size()>=max_live || next_id_==0u){
        rc=11u;error.clear();return true; // EAGAIN resource cap
    }
    const auto id=next_id_++;
    if(!put(elf,out,id)){error="Guest pthread_t output unmapped";return false;}
    threads_.emplace(id,Task{id,entry,arg,0u,detached,false,false});
    queue_.push_back(id);
    rc=0;error.clear();return true;
}
bool GuestThreads::take_next(Task& out) noexcept{
    while(!queue_.empty()){
        const auto id=queue_.front();
        queue_.pop_front();
        const auto it=threads_.find(id);
        if(it==threads_.end()||it->second.started||it->second.finished)
            continue;
        it->second.started=true;
        out=it->second;return true;
    }
    return false;
}
bool GuestThreads::complete(std::uint32_t id,std::uint32_t value) noexcept{
    const auto it=threads_.find(id);
    if(it==threads_.end()||!it->second.started||it->second.finished)return false;
    it->second.result=value;
    it->second.finished=true;
    if(it->second.detached)threads_.erase(it);
    return true;
}
bool GuestThreads::exists(std::uint32_t id) const noexcept{
    return threads_.find(id)!=threads_.end();
}
bool GuestThreads::finished(std::uint32_t id) const noexcept{
    const auto it=threads_.find(id);
    return it!=threads_.end() && it->second.finished;
}
std::uint32_t GuestThreads::join(GuestElf32& elf,std::uint32_t id,
                                  std::uint32_t out){
    if(id==current_)return 35u; // EDEADLK
    const auto it=threads_.find(id);
    if(it==threads_.end())return 3u; // ESRCH
    if(it->second.detached)return 22u; // EINVAL
    if(!it->second.finished)return 16u; // EBUSY until scheduler runs it
    if(out && !put(elf,out,it->second.result))return 14u; // EFAULT
    threads_.erase(it);
    return 0u;
}
std::uint32_t GuestThreads::detach(std::uint32_t id) noexcept{
    const auto it=threads_.find(id);
    if(it==threads_.end())return 3u;
    if(it->second.detached)return 22u;
    if(it->second.finished){threads_.erase(it);return 0u;}
    it->second.detached=true;return 0u;
}
} // namespace xperia
