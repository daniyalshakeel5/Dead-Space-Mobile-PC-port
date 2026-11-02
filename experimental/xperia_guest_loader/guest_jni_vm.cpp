#include "guest_jni_vm.hpp"
#include <algorithm>
#include <stdexcept>

namespace xperia {
namespace {
void put32(std::uint8_t* dst,std::uint32_t value){
    for(unsigned i=0;i<4;++i)dst[i]=std::uint8_t(value>>(i*8u));
}
constexpr std::uint32_t kJni16=0x00010006u;
}
bool GuestJniVm::install(GuestElf32& guest,std::string& error) {
    if(installed_) {error="JNI VM installed twice";return false;}
    // Avoid silently overlaying original ELF or libc allocations.
    if(guest.read(vm_address,1) || guest.read(vm_address+16383u,1)){
        error="Guest JavaVM address overlaps existing guest memory";
        return false;
    }
    try{
        thunks_.clear();
        guest.segments.push_back({vm_address,std::vector<std::uint8_t>(16384,0)});
        auto* vm=guest.writeable(vm_address,4);
        auto* env=guest.writeable(env_address,4);
        auto* vm_table=guest.writeable(table_address,vm_slots*4u);
        auto* env_table=guest.writeable(env_table_address,env_slots*4u);
        if(!vm||!env||!vm_table||!env_table)
            throw std::runtime_error("JNI ARM32 table allocation failed");
        put32(vm,table_address);
        put32(env,env_table_address);
        for(unsigned i=0;i<vm_slots+env_slots;++i){
            const auto svc=i<vm_slots?vm_svc_base+i:env_svc_base+(i-vm_slots);
            const auto ptr=thunk_base+static_cast<std::uint32_t>(thunks_.size());
            if(i<vm_slots)put32(vm_table+i*4u,ptr);
            else put32(env_table+(i-vm_slots)*4u,ptr);
            const std::uint32_t op[2]={0xef000000u|svc,0xe12fff1eu};
            for(const auto word:op)
                for(unsigned j=0;j<4;++j)thunks_.push_back(std::uint8_t(word>>(j*8u)));
        }
        installed_=true;
        error.clear();return true;
    }catch(const std::exception& e){
        error=e.what();
        thunks_.clear();
        return false;
    }
}
bool GuestJniVm::code_byte(std::uint32_t address,std::uint8_t& value) const noexcept {
    if(!installed_ || address<thunk_base ||
       std::uint64_t(address)-thunk_base>=thunks_.size())return false;
    value=thunks_[address-thunk_base];
    return true;
}
bool GuestJniVm::handles(std::uint32_t id) const noexcept {
    return installed_ && ((id>=vm_svc_base && id<vm_svc_base+vm_slots) ||
           (id>=env_svc_base && id<env_svc_base+env_slots));
}
std::uint32_t GuestJniVm::return_pc(std::uint32_t id) const noexcept {
    if(!handles(id))return 0;
    const auto offset=id>=env_svc_base
        ?vm_slots+(id-env_svc_base):(id-vm_svc_base);
    return thunk_base+static_cast<std::uint32_t>(offset)*8u+4u;
}
bool GuestJniVm::dispatch(std::uint32_t id,
                          std::array<std::uint32_t,16>& regs,
                          GuestElf32& guest,std::string& error) const noexcept {
    if(!handles(id)){error="Unrecognized original JNI call";return false;}
    const auto r0=regs[0],r1=regs[1],r2=regs[2];
    if(id>=vm_svc_base && id<vm_svc_base+vm_slots){
        const auto slot=id-vm_svc_base;
        if(r0!=vm_address){error="JNI JavaVM receiver mismatch";return false;}
        if(slot==6 || slot==4){ // GetEnv / AttachCurrentThread
            auto* out=guest.writeable(r1,4);
            if(!out || (r1&3u)){
                error="JNI GetEnv/AttachCurrentThread output unmapped";
                return false;
            }
            if(slot==6 && r2!=0x00010002u && r2!=0x00010004u &&
               r2!=kJni16){
                put32(out,0);regs[0]=static_cast<std::uint32_t>(-3);
            }else{
                put32(out,env_address);regs[0]=0;
            }
            error.clear();return true;
        }
        if(slot==5){regs[0]=0;error.clear();return true;} // DetachCurrentThread
        error="Unsupported original JavaVM table entry "+std::to_string(slot);
        return false;
    }
    const auto slot=id-env_svc_base;
    if(r0!=env_address){error="JNI JNIEnv receiver mismatch";return false;}
    if(slot==4){regs[0]=kJni16;error.clear();return true;} // GetVersion
    // FindClass is deliberately not spoofed: accurate class registry, fields
    // and method signatures are required to execute EA's Java bootstrap.
    error="Unsupported original JNIEnv table entry "+std::to_string(slot);
    return false;
}
} // namespace xperia
