#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_jni_vm.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <cstring>
#include <vector>
using namespace xperia;
int main(){
    GuestElf32 elf;
    GuestJniVm vm;
    std::string error;
    elf.jni_exports["Java_com_ea_EAIO_EAIO_Startup"]=0x10001100u;
    assert(vm.install(elf,error));
    assert(!vm.install(elf,error));
    assert(elf.read_u32(vm.vm())==vm.table_address);
    assert(elf.read_u32(vm.env_address)==vm.env_table_address);
    std::uint8_t byte=0;
    const auto getenv_ptr=elf.read_u32(vm.table_address+6*4);
    assert(vm.code_byte(getenv_ptr,byte) && byte==6);
    assert(vm.code_byte(getenv_ptr+3,byte) && byte==0xef);
    const auto version_ptr=elf.read_u32(vm.env_table_address+4*4);
    assert(version_ptr!=0 && vm.code_byte(version_ptr,byte));
    elf.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    std::array<std::uint32_t,16> r{};
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010006u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(r[0]==0 && elf.read_u32(0x10001100)==vm.env_address);
    r[0]=vm.vm();r[1]=0x10001100;r[2]=0x00010009u;
    assert(vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    assert(static_cast<std::int32_t>(r[0])==-3 && elf.read_u32(0x10001100)==0);
    r[0]=vm.env_address;
    assert(vm.dispatch(vm.env_svc_base+4,r,elf,error) && r[0]==0x00010006u);
    r[0]=vm.env_address;
    assert(!vm.dispatch(vm.env_svc_base+6,r,elf,error));
    assert(error.find("6")!=std::string::npos);
    const auto write_string=[&](std::uint32_t at,const char* value){
        auto* dst=elf.writeable(at,std::strlen(value)+1);
        assert(dst);
        std::memcpy(dst,value,std::strlen(value)+1);
    };
    constexpr std::uint32_t base=0x10001000u;
    write_string(base+0x200,"com/eamobile/Query");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x200;
    assert(vm.dispatch(vm.env_svc_base+6,r,elf,error));
    const auto query_class=r[0];
    assert(query_class!=0);
    write_string(base+0x230,"isContentReady");
    write_string(base+0x250,"()Z");
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;
    r[2]=base+0x230;r[3]=base+0x250;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto ready_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;r[2]=ready_method;
    assert(vm.dispatch(vm.env_svc_base+117,r,elf,error));
    assert(r[0]==0); // no actual installed content yet
    vm.set_content_ready(true);
    r[0]=vm.env_address;r[1]=query_class;r[2]=ready_method;
    assert(vm.dispatch(vm.env_svc_base+117,r,elf,error)&&r[0]==1);
    write_string(base+0x270,"getVersion");
    write_string(base+0x290,"()Ljava/lang/String;");
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;
    r[2]=base+0x270;r[3]=base+0x290;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto version_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=query_class;r[2]=version_method;
    assert(vm.dispatch(vm.env_svc_base+114,r,elf,error));
    const auto str=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=str;
    assert(vm.dispatch(vm.env_svc_base+169,r,elf,error));
    assert(std::strcmp(reinterpret_cast<const char*>(elf.read(r[0],7)),
                       "1.1.33")==0);
    write_string(base+0x300,"com/ea/EAIO/EAIO");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x300;
    assert(vm.dispatch(vm.env_svc_base+6,r,elf,error));
    const auto eaio_class=r[0];
    write_string(base+0x340,"Startup");
    write_string(base+0x370,"(Landroid/content/res/AssetManager;)V");
    r.fill(0);r[0]=vm.env_address;r[1]=eaio_class;r[2]=base+0x340;r[3]=base+0x370;
    assert(vm.dispatch(vm.env_svc_base+113,r,elf,error));
    const auto startup_method=r[0];
    r.fill(0);r[0]=vm.env_address;r[1]=eaio_class;r[2]=startup_method;r[3]=0xfaceu;
    assert(vm.dispatch(vm.env_svc_base+141,r,elf,error));
    auto callback=vm.take_callback();
    assert(callback.address==0x10001100u &&
           callback.arguments[0]==vm.env_address &&
           callback.arguments[2]==0xfaceu);
    assert(vm.take_callback().address==0);
    write_string(base+0x300,"com/ea/UNKNOWN");
    r.fill(0);r[0]=vm.env_address;r[1]=base+0x300;
    assert(!vm.dispatch(vm.env_svc_base+6,r,elf,error));
    assert(error.find("com/ea/UNKNOWN")!=std::string::npos);
    r[0]=0x10001000;
    assert(!vm.dispatch(vm.vm_svc_base+6,r,elf,error));
    return 0;
}
