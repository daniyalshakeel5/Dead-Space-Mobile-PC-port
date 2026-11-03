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
constexpr std::uint32_t kFieldBase=GuestJniVm::vm_address+0x3e00u;
struct Class {
    const char* name;
};
constexpr Class classes[]={
    {"com/eamobile/Query"},{"com/ea/EAIO/EAIO"},
    {"java/lang/String"},{"android/os/Build"},
    {"android/media/AudioTrack"},
    {"com/ea/blast/SystemAndroidDelegate"},
    {"com/ea/blast/DisplayAndroidDelegate"},
    {"com/ea/blast/GetAppDataDirectoryDelegate"},
    {"xtSystem"},
    {"com/ea/blast/xtPlay"}
};
struct Method {
    unsigned owner;
    const char* name;
    const char* signature;
    bool is_static;
    const char* value=nullptr;
};
constexpr Method methods[]={
    {0,"isContentReady","()Z",true},
    {0,"getTotalMemory","()J",true},
    {0,"getVersion","()Ljava/lang/String;",true},
    {1,"Startup","(Landroid/content/res/AssetManager;)V",true},
    {4,"write","([SII)I",false},
    {4,"play","()V",false},
    {4,"stop","()V",false},
    {4,"release","()V",false},
    {4,"getMinBufferSize","(III)I",true},
    {5,"<init>","()V",false},
    {5,"GetAccelerometerCount","()Ljava/lang/String;",false,"1"},
    {5,"IsBatteryStateAvailable","()Ljava/lang/String;",false,"0"},
    {5,"GetCameraCount","()Ljava/lang/String;",false,"0"},
    {5,"GetChipset","()Ljava/lang/String;",false,"0"},
    {5,"GetCompassCount","()Ljava/lang/String;",false,"0"},
    {5,"GetManufacturer","()Ljava/lang/String;",false,"sony"},
    {5,"GetDeviceModel","()Ljava/lang/String;",false,"Xperia PC"},
    {5,"GetDeviceName","()Ljava/lang/String;",false,"R800"},
    {5,"GetPhoneNumber","()Ljava/lang/String;",false,"0"},
    {5,"GetDeviceUniqueId","()Ljava/lang/String;",false,"-1"},
    {5,"GetDisplayCount","()Ljava/lang/String;",false,"1"},
    {5,"GetGyroscopeCount","()Ljava/lang/String;",false,"1"},
    {5,"GetLocationAvailable","()Ljava/lang/String;",false,"true"},
    {5,"GetMicrophoneCount","()Ljava/lang/String;",false,"1"},
    {5,"GetApiLevel","()Ljava/lang/String;",false,"19"},
    {5,"GetPlatformRawName","()Ljava/lang/String;",false,"Android"},
    {5,"GetPlatformStdName","()Ljava/lang/String;",false,"Android"},
    {5,"GetPlatformVersion","()Ljava/lang/String;",false,"4.4.4"},
    {5,"GetPhysicalKeyboardCount","()Ljava/lang/String;",false,"1"},
    {5,"GetProcessorArchitecture","()Ljava/lang/String;",false,"armeabi-v7a"},
    {5,"GetLanguage","()Ljava/lang/String;",false,"en"},
    {5,"GetLocale","()Ljava/lang/String;",false,"en"},
    {5,"GetTotalRAM","()Ljava/lang/String;",false,"-1"},
    {5,"GetTouchPadCount","()Ljava/lang/String;",false,"1"},
    {5,"GetTouchScreenCount","()Ljava/lang/String;",false,"1"},
    {5,"GetTrackBallCount","()Ljava/lang/String;",false,"0"},
    {5,"GetVibratorCount","()Ljava/lang/String;",false,"0"},
    {5,"GetVirtualKeyboardCount","()Ljava/lang/String;",false,"1"},
    {6,"<init>","()V",false},
    {6,"GetDefaultWidth","()I",false},
    {6,"GetDefaultHeight","()I",false},
    {6,"GetDpiX","()F",false},
    {6,"GetDpiY","()F",false},
    {6,"GetStdOrientation","()I",false},
    {6,"SetStdOrientation","(I)V",false},
    {7,"<init>","()V",false},
    {7,"GetAppDataDirectory","()Ljava/lang/String;",false,"deadspace"},
    {7,"GetExternalStorageDirectory","()Ljava/lang/String;",false,"deadspace"},
    {8,"init","(Landroid/app/NativeActivity;)V",true},
    {8,"deinit","()V",true},
    {8,"getDeviceInfo","()Ljava/lang/String;",true},
    {8,"getDisplayDensityInfo","()F",true},
    {8,"getNetworkAvailability","()Z",true},
    {8,"getInputDeviceNameByDeviceId","(I)Ljava/lang/String;",true},
    {8,"launchBrowser","(Ljava/lang/String;)V",true},
    {8,"displayToast","(Ljava/lang/String;)V",true},
    {8,"displayDialog","(Ljava/lang/String;)V",true},
    {9,"init","(Landroid/app/NativeActivity;)V",true},
    {9,"deinit","()V",true},
    {9,"isRewardedVideoAdAvailable","()Z",true},
    {9,"displayRewardedVideoAd","()V",true},
    {9,"fetchVideoAdRewardPending","()Z",true},
    // Additional AudioTrack overloads from the original game-compatible
    // JNI registry. Keep existing IDs stable for arm32 smoke tests.
    {4,"<init>","(IIIIII)V",false},
    {4,"write","([BII)I",false},
    {4,"write","([BIII)I",false}
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
std::uint32_t GuestJniVm::store_string(
    GuestElf32& guest,const std::string& text,std::string& error) {
    constexpr std::uint32_t kBase=0x74800000u;
    if(text.size()>254||next_string_>=128u){
        error="JNI guest string allocation limit";return 0;
    }
    if(next_string_==0) {
        if(guest.read(kBase,1)||guest.read(kBase+65535u,1)){
            error="JNI string arena overlaps guest ELF";return 0;
        }
        guest.segments.push_back({kBase,std::vector<std::uint8_t>(65536,0)});
    }
    const auto handle=kBase+next_string_*16u;
    const auto data=kBase+4096u+next_string_*256u;
    auto* string=guest.writeable(data,text.size()+1u);
    auto* object=guest.writeable(handle,4u);
    if(!string||!object){error="JNI guest string storage unmapped";return 0;}
    std::memcpy(string,text.c_str(),text.size()+1u);
    put32(object,data);
    strings_.emplace(handle,data);
    ++next_string_;
    error.clear();return handle;
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
        put32(guest.writeable(audio_track_address,4),kClassBase+4u*16u);
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
                          GuestElf32& guest,std::string& error) noexcept {
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
    if(slot==15 || slot==228){ // ExceptionOccurred / ExceptionCheck
        // All unsupported guest operations trap rather than setting a
        // synthetic Java exception. There is no pending Java exception.
        regs[0]=0;error.clear();return true;
    }
    if(slot==17){regs[0]=0;error.clear();return true;} // ExceptionClear
    if(slot==219){ // GetJavaVM(JNIEnv*, JavaVM**)
        auto* out=guest.writeable(r1,4u);
        if(!out || (r1&3u)){error="JNI GetJavaVM output invalid";return false;}
        put32(out,vm_address);regs[0]=0;error.clear();return true;
    }
    if(slot==167){ // NewStringUTF(JNIEnv*, const char*)
        std::string text;
        if(!guest_string(guest,r1,text)){
            error="JNI NewStringUTF source outside original ARM memory";
            return false;
        }
        const auto object=store_string(guest,text,error);
        if(!object)return false;
        regs[0]=object;error.clear();return true;
    }
    if(slot==144){ // GetStaticFieldID: original android.os.Build.MANUFACTURER
        std::string name,sig;
        if(!guest_string(guest,r2,name) ||
           !guest_string(guest,regs[3],sig)){
            error="JNI GetStaticFieldID unmapped name/signature";return false;
        }
        if(r1!=kClassBase+3u*16u || name!="MANUFACTURER" ||
           sig!="Ljava/lang/String;"){
            error="Unimplemented JNI static field "+name+sig;return false;
        }
        regs[0]=kFieldBase;error.clear();return true;
    }
    if(slot==145){ // GetStaticObjectField
        if(r1!=kClassBase+3u*16u || r2!=kFieldBase){
            error="Unrecognized JNI static Build field";return false;
        }
        const unsigned id=120u;
        auto cached=cached_method_strings_.find(id);
        if(cached!=cached_method_strings_.end()){
            regs[0]=cached->second;error.clear();return true;
        }
        const auto string=store_string(guest,"sony",error);
        if(!string)return false;
        cached_method_strings_.emplace(id,string);
        regs[0]=string;error.clear();return true;
    }
    if(slot==215){ // RegisterNatives validates the real JNI method descriptors
        if(r1<kClassBase || (r1-kClassBase)%16u ||
           (r1-kClassBase)/16u>=sizeof(classes)/sizeof(classes[0]) ||
           static_cast<std::int32_t>(regs[3])<0 || regs[3]>128u){
            error="JNI RegisterNatives unknown class/count";return false;
        }
        for(std::uint32_t i=0;i<regs[3];++i){
            if(std::uint64_t(r2)+12u*i+12u>0xffffffffull){
                error="JNI RegisterNatives address overflow";return false;
            }
            const auto* method=guest.read(r2+12u*i,12u);
            if(!method){error="JNI RegisterNatives guest descriptors unmapped";return false;}
            auto read_word=[&](unsigned offset){
                return std::uint32_t(method[offset]) |
                    std::uint32_t(method[offset+1])<<8u |
                    std::uint32_t(method[offset+2])<<16u |
                    std::uint32_t(method[offset+3])<<24u;
            };
            std::string name,sig;
            if(!guest_string(guest,read_word(0),name) ||
               !guest_string(guest,read_word(4),sig) || name.empty() ||
               sig.empty() || !guest.read(read_word(8)&~1u,2u)){
                error="JNI RegisterNatives uncallable original ARM method";
                return false;
            }
        }
        // Registration only validates ABI here. These methods are still
        // entered by the Xperia exported Java_* lifecycle bindings.
        regs[0]=0;error.clear();return true;
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
            if(methods[i].owner==owner && methods[i].is_static &&
               name==methods[i].name &&
               signature==methods[i].signature){
                regs[0]=kMethodBase+16u*i;error.clear();return true;
            }
        error="Original Xperia JNI missing static method: "+
              std::string(classes[owner].name)+"."+name+signature;
        return false;
    }
    if(slot==117 || slot==114 || slot==132 || slot==141 ||
       slot==129 || slot==135){
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
        if(index==8 && slot==129){
            regs[0]=4096;error.clear();return true;
        }
        const auto& method=methods[index];
        if(method.owner==8u || method.owner==9u){
            const std::string name=method.name;
            // Both Java facades represent Android system services rather
            // than game-native engine code. Advertising, billing and network
            // access are unavailable in this asset-owned offline PC port.
            if(slot==117u && (name=="isRewardedVideoAdAvailable"||
               name=="fetchVideoAdRewardPending"||
               name=="getNetworkAvailability")){
                regs[0]=0;error.clear();return true;
            }
            if(name=="getDisplayDensityInfo" && slot==117u){
                // This specific Xperia game calls the declared ()F method
                // using CallStaticBooleanMethod, then interprets r0 as 1.
                regs[0]=1;error.clear();return true;
            }
            if(name=="getDisplayDensityInfo" && slot==135u){
                constexpr float scale=1.f;
                std::memcpy(&regs[0],&scale,4);
                error.clear();return true;
            }
            if(slot==114u && (name=="getDeviceInfo"||
               name=="getInputDeviceNameByDeviceId")){
                const auto cached=cached_method_strings_.find(
                    static_cast<unsigned>(index));
                if(cached!=cached_method_strings_.end()){
                    regs[0]=cached->second;error.clear();return true;
                }
                const std::string value=name=="getDeviceInfo"?
                    "Manufacturer:sony\nModel:R800\nDevice:R800\nProduct:deadspace-xperia-windows\nBrand:sony\nHardware:x86_64-arm32\nBoard:pc\nOS:Windows\nRelease:4.4.4\nSDK:19\nABI:armeabi-v7a\nBuild:Xperia PC wrapper":
                    "Sony Xperia Play";
                const auto ref=store_string(guest,value,error);
                if(!ref)return false;
                cached_method_strings_.emplace(static_cast<unsigned>(index),ref);
                regs[0]=ref;error.clear();return true;
            }
            if(slot==141u && (name=="init"||name=="deinit")){
                // No Android ad SDK is initialized or invoked on Windows.
                regs[0]=0;error.clear();return true;
            }
            error="Original Xperia Java system callback not implemented: "+
                  std::string(classes[method.owner].name)+"."+name;
            return false;
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
    if(slot==27 || slot==28 || slot==29 || slot==30){ // AllocObject/NewObject/V/A
        if(r1<kClassBase || (r1-kClassBase)%16u){
            error="JNI allocate with invalid class";return false;
        }
        const auto owner=(r1-kClassBase)/16u;
        if(owner!=4u && (owner<5u||owner>7u)){
            error="JNI object allocation requires Xperia delegate or AudioTrack";
            return false;
        }
        if(slot!=27){
            if(r2<kMethodBase || (r2-kMethodBase)%16u ||
               (r2-kMethodBase)/16u>=sizeof(methods)/sizeof(methods[0])){
                error="JNI object constructor method ID invalid";return false;
            }
            const auto& ctor=methods[(r2-kMethodBase)/16u];
            if(ctor.owner!=owner || std::string(ctor.name)!="<init>"){
                error="JNI constructor does not match requested Xperia class";
                return false;
            }
        }
        if(next_instance_>=32u){error="JNI Xperia delegate object cap";return false;}
        const auto object=vm_address+0x3b00u+next_instance_*16u;
        ++next_instance_;
        auto* ptr=guest.writeable(object,4u);
        if(!ptr){error="JNI delegate object unmapped";return false;}
        put32(ptr,r1);
        instances_.emplace(object,owner);
        regs[0]=object;error.clear();return true;
    }
    if(slot==31){ // GetObjectClass
        if(r1==audio_track_address){
            regs[0]=kClassBase+4u*16u;error.clear();return true;
        }
        const auto instance=instances_.find(r1);
        if(instance==instances_.end()){
            error="JNI GetObjectClass unknown guest Java object";return false;
        }
        regs[0]=kClassBase+16u*instance->second;error.clear();return true;
    }
    if(slot==33){ // GetMethodID(jclass, method, signature)
        if(r1<kClassBase || (r1-kClassBase)%16u ||
           (r1-kClassBase)/16u>=sizeof(classes)/sizeof(classes[0])){
            error="JNI GetMethodID invalid class";return false;
        }
        const auto owner=(r1-kClassBase)/16u;
        std::string name,sig;
        if(!guest_string(guest,r2,name)||
           !guest_string(guest,regs[3],sig)){
            error="JNI GetMethodID invalid name/signature";return false;
        }
        for(unsigned i=0;i<sizeof(methods)/sizeof(methods[0]);++i)
            if(methods[i].owner==owner && !methods[i].is_static &&
               name==methods[i].name && sig==methods[i].signature){
                regs[0]=kMethodBase+16u*i;error.clear();return true;
            }
        error=std::string("Original Xperia instance method missing: ")+
              classes[owner].name+"."+name+sig;return false;
    }
    if(slot==176){ // NewByteArray(jsize), original Android JNI ABI
        if(static_cast<std::int32_t>(r1)<0||r1>1048576u ||
           next_array_>0x75600000u-r1-4096u){
            error="JNI byte array exceeds 1MiB per object or arena";
            return false;
        }
        const auto bytes=16u+r1;
        const auto reserved=(bytes+4095u)&~4095u;
        const auto addr=next_array_;
        if(guest.read(addr,1)||guest.read(addr+reserved-1u,1)){
            error="JNI byte array overlaps original game memory";return false;
        }
        guest.segments.push_back({addr,std::vector<std::uint8_t>(reserved,0)});
        byte_arrays_.emplace(addr,ShortArray{addr+16u,r1});
        next_array_+=reserved;
        regs[0]=addr;error.clear();return true;
    }
    if(slot==184 || slot==192 || slot==200 || slot==208){
        const auto it=byte_arrays_.find(r1);
        if(it==byte_arrays_.end()){error="Unknown JNI byte[] instance";return false;}
        const auto arr=it->second;
        if(slot==184){ // GetByteArrayElements, direct guest storage
            if(r2){
                auto* copied=guest.writeable(r2,1);
                if(!copied){error="JNI byte isCopy address invalid";return false;}
                *copied=0;
            }
            regs[0]=arr.data;error.clear();return true;
        }
        if(slot==192){regs[0]=0;error.clear();return true;}
        const auto start=r2,amount=regs[3];
        if(start>arr.length||amount>arr.length-start){
            error="JNI byte[] region exceeds allocation";return false;
        }
        std::uint32_t source=0;
        try{source=guest.read_u32(regs[13]);}
        catch(...){error="JNI byte[] region ARM stack missing";return false;}
        const auto* from=guest.read(
            slot==208?source:arr.data+start,amount);
        auto* to=guest.writeable(
            slot==208?arr.data+start:source,amount);
        if(!from||!to){error="JNI byte[] region pointer unmapped";return false;}
        std::memmove(to,from,amount);
        regs[0]=0;error.clear();return true;
    }
    if(slot==178){ // NewShortArray(jsize)
        if(static_cast<std::int32_t>(r1)<0||r1>524288u ||
           next_array_>0x75600000u-r1*2u-4096u){
            error="JNI short array exceeds ARM memory limit";return false;
        }
        const auto bytes=16u+r1*2u;
        const auto reserved=(bytes+4095u)&~4095u;
        const auto addr=next_array_;
        if(guest.read(addr,1)||guest.read(addr+reserved-1u,1)){
            error="JNI short array overlaps ARM memory";return false;
        }
        guest.segments.push_back({addr,std::vector<std::uint8_t>(reserved,0)});
        short_arrays_.emplace(addr,ShortArray{addr+16u,r1});
        next_array_+=reserved;
        regs[0]=addr;error.clear();return true;
    }
    if(slot==171){ // Shared JVM GetArrayLength for byte[] and short[]
        const auto shorts=short_arrays_.find(r1);
        const auto bytes=byte_arrays_.find(r1);
        if(shorts!=short_arrays_.end())regs[0]=shorts->second.length;
        else if(bytes!=byte_arrays_.end())regs[0]=bytes->second.length;
        else {error="Unknown JVM array length receiver";return false;}
        error.clear();return true;
    }
    if(slot==186 || slot==194 || slot==210){
        const auto it=short_arrays_.find(r1);
        if(it==short_arrays_.end()){
            error="Unknown guest JNI short array";return false;
        }
        const auto arr=it->second;
        if(slot==186){ // GetShortArrayElements, no host copy
            if(r2){
                auto* flag=guest.writeable(r2,1);
                if(!flag){error="JNI isCopy out of guest bounds";return false;}
                *flag=0;
            }
            regs[0]=arr.data;error.clear();return true;
        }
        if(slot==194){regs[0]=0;error.clear();return true;}
        // SetShortArrayRegion(array,start,len,source)
        const auto start=r2,amount=regs[3];
        if(start>arr.length || amount>arr.length-start){
            error="JNI SetShortArrayRegion out of range";return false;
        }
        std::uint32_t source=0;
        try{source=guest.read_u32(regs[13]);}
        catch(...){error="JNI SetShortArrayRegion ARM stack unmapped";return false;}
        const auto* from=guest.read(source,amount*2u);
        auto* to=guest.writeable(arr.data+start*2u,amount*2u);
        if(!from||!to){error="JNI short region guest pointers unmapped";return false;}
        std::memmove(to,from,amount*2u);
        regs[0]=0;error.clear();return true;
    }
    const auto method_index=[&]()->int {
        if(r2<kMethodBase || (r2-kMethodBase)%16u ||
           (r2-kMethodBase)/16u>=sizeof(methods)/sizeof(methods[0]))
            return -1;
        return static_cast<int>((r2-kMethodBase)/16u);
    };
    if(slot==34){ // CallObjectMethod() - Xperia hardware / data path strings
        const auto obj=instances_.find(r1);
        const int index=method_index();
        if(obj==instances_.end() || index<0 ||
           methods[index].owner!=obj->second || !methods[index].value){
            error="JNI CallObjectMethod unknown delegate/string getter";return false;
        }
        const auto cached=cached_method_strings_.find(static_cast<unsigned>(index));
        if(cached!=cached_method_strings_.end()){
            regs[0]=cached->second;error.clear();return true;
        }
        const auto ref=store_string(guest,methods[index].value,error);
        if(!ref)return false;
        cached_method_strings_.emplace(static_cast<unsigned>(index),ref);
        regs[0]=ref;error.clear();return true;
    }
    if(slot==55){ // CallFloatMethod() - device pixel density
        const int index=method_index();
        const auto obj=instances_.find(r1);
        if(index<0||obj==instances_.end()||obj->second!=6u||
           methods[index].owner!=6u ||
           (std::string(methods[index].name)!="GetDpiX" &&
            std::string(methods[index].name)!="GetDpiY")){
            error="JNI CallFloatMethod unknown Xperia display property";
            return false;
        }
        constexpr float dpi=200.f;
        static_assert(sizeof(dpi)==4,"ARM32 float must occupy one register");
        std::uint32_t bits=0;
        std::memcpy(&bits,&dpi,4);
        regs[0]=bits;error.clear();return true;
    }
    if(slot==49){ // Xperia Display delegate integers / AudioTrack.write
        const auto instance=instances_.find(r1);
        const int index=method_index();
        if(instance!=instances_.end() && instance->second==6u &&
           index>=0 && methods[index].owner==6u){
            const std::string name=methods[index].name;
            if(name=="GetDefaultWidth")regs[0]=544u;
            else if(name=="GetDefaultHeight")regs[0]=960u;
            else if(name=="GetStdOrientation")regs[0]=0u;
            else {error="Unknown Xperia display int getter";return false;}
            error.clear();return true;
        }
        const auto receiver=instances_.find(r1);
        const bool is_audio=r1==audio_track_address ||
            (receiver!=instances_.end()&&receiver->second==4u);
        const int audio_method=method_index();
        if(!is_audio || audio_method<0 ||
           methods[audio_method].owner!=4u ||
           std::string(methods[audio_method].name)!="write"){
            error="Unknown JNI CallIntMethod receiver/method";return false;
        }
        const bool is_short=std::string(methods[audio_method].signature)=="([SII)I";
        const bool is_bytes=std::string(methods[audio_method].signature)=="([BII)I" ||
            std::string(methods[audio_method].signature)=="([BIII)I";
        if(!is_short && !is_bytes){
            error="Unknown AudioTrack.write overload";return false;
        }
        const auto& arrays=is_short?short_arrays_:byte_arrays_;
        const auto it=arrays.find(regs[3]);
        if(it==arrays.end()){
            error="AudioTrack.write unknown guest PCM array";return false;
        }
        std::uint32_t offset=0,number=0;
        try{offset=guest.read_u32(regs[13]);number=guest.read_u32(regs[13]+4u);}
        catch(...){error="AudioTrack.write ARM varargs unmapped";return false;}
        if(offset>it->second.length||number>it->second.length-offset ||
           number>524288u){
            error="AudioTrack.write sample range invalid";return false;
        }
        if(is_bytes &&
           std::string(methods[audio_method].signature)=="([BIII)I"){
            std::uint32_t mode=0;
            try{mode=guest.read_u32(regs[13]+8u);}
            catch(...){error="AudioTrack.write blocking mode unmapped";return false;}
            if(mode>1u){error="Unknown AudioTrack write mode";return false;}
        }
        const auto unit=is_short?2u:1u;
        const auto bytes=number*unit;
        const auto* pcm=guest.read(it->second.data+offset*unit,bytes);
        if(!pcm||!audio_.enqueue){
            error="AudioTrack PCM data or SDL output unavailable";return false;
        }
        // Windows device uses 44.1 kHz stereo S16, so only complete
        // samples reach SDL; do not claim a partial/unaligned write succeeded.
        if(bytes%4u){
            error="AudioTrack.write cannot queue incomplete S16 stereo frame";
            return false;
        }
        if(!audio_.enqueue(audio_.context,pcm,bytes,error))return false;
        regs[0]=number;error.clear();return true;
    }
    if(slot==61){ // Display orientation / AudioTrack.play-stop-release
        const auto obj=instances_.find(r1);
        const int index=method_index();
        if(obj!=instances_.end()&&obj->second==6u &&
           index>=0 && methods[index].owner==6u &&
           std::string(methods[index].name)=="SetStdOrientation"){
            // This is a fixed-landscape SDL window; orientation cannot rotate.
            regs[0]=0;error.clear();return true;
        }
        const auto receiver=instances_.find(r1);
        const bool is_audio=r1==audio_track_address ||
            (receiver!=instances_.end()&&receiver->second==4u);
        if(!is_audio || r2<kMethodBase+5u*16u ||
           r2>kMethodBase+7u*16u || (r2-kMethodBase)%16u){
            error="Unknown JNI CallVoidMethod receiver/method";return false;
        }
        if(!audio_.control){
            error="AudioTrack output control unavailable";return false;
        }
        const auto method=(r2-kMethodBase)/16u;
        if(!audio_.control(audio_.context,static_cast<unsigned>(method-5u),error))
            return false;
        regs[0]=0;error.clear();return true;
    }
    if(slot==168){ // GetStringUTFLength
        const auto found=strings_.find(r1);
        const auto addr=r1==kString?kVersion:
            found!=strings_.end()?found->second:0u;
        if(!addr){error="Unknown original JNI String instance";return false;}
        const auto* bytes=guest.read(addr,255);
        if(!bytes){error="JNI String UTF buffer not mapped";return false;}
        regs[0]=static_cast<std::uint32_t>(std::strlen(
            reinterpret_cast<const char*>(bytes)));
        error.clear();return true;
    }
    if(slot==169){ // GetStringUTFChars
        const auto found=strings_.find(r1);
        const auto addr=r1==kString?kVersion:
            found!=strings_.end()?found->second:0u;
        if(!addr){error="Unknown original JNI String instance";return false;}
        if(r2){
            auto* is_copy=guest.writeable(r2,1);
            if(!is_copy){error="Unmapped JNI GetStringUTFChars isCopy";return false;}
            *is_copy=0;
        }
        regs[0]=addr;error.clear();return true;
    }
    error="Unsupported original JNIEnv table entry "+std::to_string(slot);
    return false;
}
} // namespace xperia
