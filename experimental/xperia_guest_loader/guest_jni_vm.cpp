#include "guest_jni_vm.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <stdexcept>

namespace xperia {
namespace {
void put32(std::uint8_t* dst,std::uint32_t value){
    for(unsigned i=0;i<4;++i)dst[i]=std::uint8_t(value>>(i*8u));
}
constexpr std::uint32_t kJni16=0x00010006u;
constexpr std::uint32_t kClassBase=GuestJniVm::vm_address+0x3000u;
constexpr std::uint32_t kMethodBase=GuestJniVm::vm_address+0x3400u;
constexpr std::uint32_t kString=GuestJniVm::vm_address+0x3800u;
constexpr std::uint32_t kVersion=GuestJniVm::vm_address+0x3900u;
struct Class {
    const char* name;
};
constexpr Class classes[]={
    {"com/eamobile/Query"},{"com/ea/EAIO/EAIO"},
    {"java/lang/String"},{"android/os/Build"}
};
struct Method {
    unsigned owner;
    const char* name;
    const char* signature;
};
constexpr Method methods[]={
    {0,"isContentReady","()Z"},
    {0,"getTotalMemory","()J"},
    {0,"getVersion","()Ljava/lang/String;"},
    {1,"Startup","(Landroid/content/res/AssetManager;)V"}
};
bool guest_string(GuestElf32& guest,std::uint32_t addr,std::string& output){
    output.clear();
    if(!addr)return false;
    for(unsigned i=0;i<256;++i){
        if(std::uint64_t(addr)+i>0xffffffffull)return false;
        const auto* c=guest.read(addr+i,1);
        if(!c)return false;
        if(!*c)return true;
        output.push_back(static_cast<char>(*c));
    }
    return false;
}
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
        // Immutable JNI string representing the installed Xperia content
        // version. Its UTF8 bytes live at guest addresses, not host pointers.
        const char version[]="1.1.33";
        auto* data=guest.writeable(kVersion,sizeof(version));
        if(!data)throw std::runtime_error("JNI version string unmapped");
        std::memcpy(data,version,sizeof(version));
        put32(guest.writeable(kString,4),kVersion);
        auto it=guest.jni_exports.find("Java_com_ea_EAIO_EAIO_Startup");
        if(it!=guest.jni_exports.end())
            eaio_startup_=it->second;
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
    if(slot==6){ // FindClass: exact known original Xperia Java class names
        std::string name;
        if(!guest_string(guest,r1,name)){
            error="JNI FindClass name unmapped/unterminated";return false;
        }
        for(unsigned i=0;i<sizeof(classes)/sizeof(classes[0]);++i)
            if(name==classes[i].name){
                regs[0]=kClassBase+16u*i;error.clear();return true;
            }
        error="Original Xperia JNI FindClass missing class: "+name;
        return false;
    }
    if(slot==21 || slot==25){ // NewGlobalRef / NewLocalRef
        regs[0]=r1;error.clear();return true;
    }
    if(slot==22 || slot==23 || slot==170){ // Delete refs / ReleaseStringUTFChars
        regs[0]=0;error.clear();return true;
    }
    if(slot==24){ // IsSameObject
        regs[0]=r1==r2?1u:0u;error.clear();return true;
    }
    if(slot==113){ // GetStaticMethodID(jclass, name, signature)
        if(r1<kClassBase || (r1-kClassBase)%16u ||
           (r1-kClassBase)/16u>=sizeof(classes)/sizeof(classes[0])){
            error="JNI GetStaticMethodID unknown class";return false;
        }
        std::string name,signature;
        if(!guest_string(guest,r2,name)||!guest_string(guest,regs[3],signature)){
            error="JNI GetStaticMethodID name/signature invalid";return false;
        }
        const auto owner=(r1-kClassBase)/16u;
        for(unsigned i=0;i<sizeof(methods)/sizeof(methods[0]);++i)
            if(methods[i].owner==owner && name==methods[i].name &&
               signature==methods[i].signature){
                regs[0]=kMethodBase+16u*i;error.clear();return true;
            }
        error="Original Xperia JNI missing static method: "+
              std::string(classes[owner].name)+"."+name+signature;
        return false;
    }
    if(slot==117 || slot==114 || slot==132 || slot==141){
        if(r2<kMethodBase || (r2-kMethodBase)%16u ||
           (r2-kMethodBase)/16u>=sizeof(methods)/sizeof(methods[0])){
            error="JNI static invocation invalid method ID";return false;
        }
        const auto index=(r2-kMethodBase)/16u;
        if(index==0 && slot==117){ // Query.isContentReady()Z
            // Only report installed content when the real asset mount exists.
            regs[0]=content_ready_?1u:0u;error.clear();return true;
        }
        if(index==1 && slot==132){ // Query.getTotalMemory()J
            regs[0]=256u;regs[1]=0u;error.clear();return true;
        }
        if(index==2 && slot==114){ // Query.getVersion()
            regs[0]=kString;error.clear();return true;
        }
        if(index==3 && slot==141){ // EAIO.Startup(AssetManager)V
            if(!eaio_startup_){
                error="Xperia original ELF lacks Java_com_ea_EAIO_EAIO_Startup";return false;
            }
            if(pending_callback_.address){
                error="Nested EAIO Startup callback is not supported";return false;
            }
            pending_callback_={eaio_startup_,{env_address,0u,regs[3],0u}};
            regs[0]=0;error.clear();return true;
        }
        error="Unimplemented Xperia static method execution: "+
              std::string(methods[index].name);return false;
    }
    if(slot==168){ // GetStringUTFLength
        if(r1!=kString){error="Unknown original JNI String instance";return false;}
        regs[0]=6;error.clear();return true;
    }
    if(slot==169){ // GetStringUTFChars
        if(r1!=kString){error="Unknown original JNI String instance";return false;}
        if(r2){
            auto* is_copy=guest.writeable(r2,1);
            if(!is_copy){error="Unmapped JNI GetStringUTFChars isCopy";return false;}
            *is_copy=0;
        }
        regs[0]=kVersion;error.clear();return true;
    }
    error="Unsupported original JNIEnv table entry "+std::to_string(slot);
    return false;
}
} // namespace xperia
