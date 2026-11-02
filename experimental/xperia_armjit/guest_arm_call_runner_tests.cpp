#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_arm_call_runner.hpp"
#include "guest_filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace xperia;
using u32=std::uint32_t;
static void put32(GuestElf32& elf,u32 address,u32 word){
    auto* ptr=elf.writeable(address,4);
    assert(ptr);
    for(int i=0;i<4;++i)ptr[i]=std::uint8_t(word>>(8*i));
}
int main(){
    constexpr u32 base=0x10001000u;
    GuestElf32 elf;
    elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
    GuestLibcBridge libc;
    GuestHeap heap(elf);
    // AAPCS subroutine:
    // PUSH {LR}; BLX R3 (imported memcpy); POP {PC}.
    put32(elf,base,0xe92d4000u);
    put32(elf,base+4,0xe12fff33u);
    put32(elf,base+8,0xe8bd8000u);
    const char original[]="VANDAL";
    std::memcpy(elf.writeable(base+0x200,6),original,6);
    GuestArmCallRunner runner(elf,libc,heap);
    std::string error;
    u32 result=0;
    assert(runner.invoke(base,{base+0x280,base+0x200,6,
           libc.symbols().at("memcpy")},result,error));
    assert(result==base+0x280);
    assert(std::memcmp(elf.read(base+0x280,6),original,6)==0);
    // Realistic game constructor order:
    // MOV R0,#7 or #9; LDR R1,[PC,#4]; STR R0,[R1]; BX LR.
    for(const auto init:std::array<std::pair<u32,u32>,2>{{
          {base+0x400,7},{base+0x420,9}}}){
        put32(elf,init.first,0xe3a00000u|init.second);
        put32(elf,init.first+4,0xe59f1004u);
        put32(elf,init.first+8,0xe5810000u);
        put32(elf,init.first+12,0xe12fff1eu);
        put32(elf,init.first+16,base+0x700);
    }
    elf.initialization_order={base+0x400,base+0x420};
    assert(runner.run_original_initializers(error));
    assert(elf.read_u32(base+0x700)==9u);
    // Original ARM C++ destructor callbacks executed as translated ARM
    // instructions, not host x86 pointers or simulated return values.
    put32(elf,base+0x800,0xe3a0100bu); // MOV R1,#11
    put32(elf,base+0x804,0xe5801000u); // STR R1,[R0]
    put32(elf,base+0x808,0xe12fff1eu); // BX LR
    put32(elf,base+0x820,0xe3a01016u); // MOV R1,#22
    put32(elf,base+0x824,0xe5801000u);
    put32(elf,base+0x828,0xe12fff1eu);
    put32(elf,base+0xa00,0xe92d4000u); // PUSH LR
    put32(elf,base+0xa04,0xe12fff33u); // BLX R3
    put32(elf,base+0xa08,0xe8bd8000u); // POP PC
    std::array<u32,16> finalizer_regs{};
    finalizer_regs[0]=base+0x900;
    finalizer_regs[1]=base+0x800;
    finalizer_regs[2]=0x77;
    assert(libc.dispatch(29,finalizer_regs,elf,error)); // __aeabi_atexit
    finalizer_regs.fill(0);
    finalizer_regs[0]=base+0x820;
    finalizer_regs[1]=base+0x900;
    finalizer_regs[2]=0x99;
    assert(libc.dispatch(30,finalizer_regs,elf,error)); // __cxa_atexit
    assert(runner.invoke(base+0xa00,
        {0x77,0,0,libc.symbols().at("__cxa_finalize")},result,error));
    assert(elf.read_u32(base+0x900)==11u);
    assert(runner.invoke(base+0xa00,
        {0,0,0,libc.symbols().at("__cxa_finalize")},result,error));
    assert(elf.read_u32(base+0x900)==22u);
    // Repeated cleanup must not invoke a registered destructor twice.
    put32(elf,base+0x900,0u);
    assert(runner.invoke(base+0xa00,
        {0,0,0,libc.symbols().at("__cxa_finalize")},result,error));
    assert(elf.read_u32(base+0x900)==0u);
    // Translate the actual guest pthread_once callback via A32, exactly
    // once. A returned zero without executing the function is forbidden.
    put32(elf,base+0xb00,0xe59f000cu); // LDR R0,[PC,#12]
    put32(elf,base+0xb04,0xe5901000u); // LDR R1,[R0]
    put32(elf,base+0xb08,0xe2811001u); // ADD R1,R1,#1
    put32(elf,base+0xb0c,0xe5801000u); // STR R1,[R0]
    put32(elf,base+0xb10,0xe12fff1eu); // BX LR
    put32(elf,base+0xb14,base+0xbc0);
    put32(elf,base+0xb80,0u);
    put32(elf,base+0xbc0,0u);
    assert(runner.invoke(base+0xa00,
        {base+0xb80,base+0xb00,0,libc.symbols().at("pthread_once")},
        result,error));
    assert(result==0u && elf.read_u32(base+0xb80)==2u);
    assert(elf.read_u32(base+0xbc0)==1u);
    assert(runner.invoke(base+0xa00,
        {base+0xb80,base+0xb00,0,libc.symbols().at("pthread_once")},
        result,error));
    assert(elf.read_u32(base+0xbc0)==1u);
    put32(elf,base+0xb80,1u);
    assert(!runner.invoke(base+0xa00,
        {base+0xb80,base+0xb00,0,libc.symbols().at("pthread_once")},
        result,error));
    put32(elf,base+0xb80,2u);
    // Lazy binding allows ORIGINAL guest ARM constructors to execute
    // without falsely implementing every unused GL/JNI function.
    {
        GuestElf32 lazy;
        lazy.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
        lazy.pending_imports.push_back({base+0xc80,22u,0u,"glDrawArrays",2u});
        lazy.pending_imports.push_back({base+0xc84,22u,0u,"glClear",2u});
        GuestLibcBridge lazy_bridge;
        auto symbols=lazy_bridge.plan_guest_imports(lazy,error);
        assert(error.empty() && lazy_bridge.unresolved_import_count()==2u);
        assert(symbols.count("glDrawArrays") && symbols.count("glClear"));
        assert(lazy.bind_imports(symbols,error));
        assert(lazy.read_u32(base+0xc80)==symbols.at("glDrawArrays"));
        put32(lazy,base+0xc00,0xe92d4000u);
        put32(lazy,base+0xc04,0xe12fff33u);
        put32(lazy,base+0xc08,0xe8bd8000u);
        put32(lazy,base+0xd00,0xe3a00009u); // MOV R0,#9
        put32(lazy,base+0xd04,0xe12fff1eu); // BX LR
        lazy.initialization_order.push_back(base+0xd00);
        GuestHeap lazy_heap(lazy);
        GuestArmCallRunner lazy_runner(lazy,lazy_bridge,lazy_heap);
        assert(lazy_runner.run_original_initializers(error));
        assert(lazy_runner.invoke(base+0xc00,
            {0,0,0,symbols.at("memcpy")},result,error));
        assert(error.empty() && result==0u);
        assert(!lazy_runner.invoke(base+0xc00,
            {0,0,0,symbols.at("glDrawArrays")},result,error));
        assert(error.find("glDrawArrays")!=std::string::npos);
        // Missing imported global data must not receive an SVC address.
        GuestElf32 unknown_global;
        unknown_global.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
        unknown_global.pending_imports.push_back(
            {base+0xc80,21u,0u,"stderr",1u}); // STT_OBJECT
        GuestLibcBridge strict;
        assert(strict.plan_guest_imports(unknown_global,error).empty());
        assert(error.find("stderr")!=std::string::npos);
    }
    // A32 integration: actual guest instructions dereference the
    // JavaVM vtable and enter its GetEnv function through an ARM SVC.
    {
        GuestElf32 vm_elf;
        vm_elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
        // LDR R3,[R0]; LDR R3,[R3,#24]; PUSH LR; BLX R3; POP PC.
        put32(vm_elf,base+0xe00,0xe5903000u);
        put32(vm_elf,base+0xe04,0xe5933018u);
        put32(vm_elf,base+0xe08,0xe92d4000u);
        put32(vm_elf,base+0xe0c,0xe12fff33u);
        put32(vm_elf,base+0xe10,0xe8bd8000u);
        GuestLibcBridge vm_libc;
        GuestHeap vm_heap(vm_elf);
        GuestJniVm jni;
        assert(jni.install(vm_elf,error));
        GuestArmCallRunner vm_runner(vm_elf,vm_libc,vm_heap,nullptr,&jni);
        assert(vm_runner.invoke(base+0xe00,
            {jni.vm(),base+0xe80,0x00010006u,0},result,error));
        assert(result==0u && vm_elf.read_u32(base+0xe80)==jni.env_address);
        assert(vm_runner.invoke(base+0xe00,
            {jni.vm(),base+0xe80,0x00010009u,0},result,error));
        assert(static_cast<std::int32_t>(result)==-3);
        // JNIEnv->GetVersion uses index 4, offset 16.
        put32(vm_elf,base+0xe04,0xe5933010u);
        assert(vm_runner.invoke(base+0xe00,
            {jni.env_address,0,0,0},result,error));
        assert(result==0x00010006u);
        // JNIEnv->FindClass index 6 must fail with its *actual* name,
        // not issue a successful fake Java class handle.
        put32(vm_elf,base+0xe04,0xe5933018u);
        assert(!vm_runner.invoke(base+0xe00,
            {jni.env_address,0,0,0},result,error));
        assert(error.find("JNIEnv table entry 6")!=std::string::npos);
    }
    // Genuine ARM32 AAPCS stack parameter handoff: fifth, sixth and seventh
    // parameters are visible in the original ARM instructions at [SP+0/4/8].
    put32(elf,base+0xf00,0xe59d0000u); // LDR R0,[SP]
    put32(elf,base+0xf04,0xe59d1004u); // LDR R1,[SP,#4]
    put32(elf,base+0xf08,0xe0800001u); // ADD R0,R0,R1
    put32(elf,base+0xf0c,0xe59d1008u); // LDR R1,[SP,#8]
    put32(elf,base+0xf10,0xe0800001u); // ADD R0,R0,R1
    put32(elf,base+0xf14,0xe12fff1eu); // BX LR
    assert(runner.invoke_aapcs(base+0xf00,{1,2,3,4,7,9,11},
                               result,error));
    assert(result==27u);
    assert(!runner.invoke_aapcs(base+0xf00,std::vector<u32>(17,1),
                                result,error));
    // Fail closed rather than claim to boot unsupported game services.
    assert(!runner.invoke(0xffffffffu,{0,0,0,0},result,error));
    assert(!error.empty());
    // Actual translated ARM32 instructions exercise a host Windows/Linux
    // read-only game-asset mount: BLX open -> read -> close. The guest
    // program is original-free synthetic A32 and doesn't import EA bytes.
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/"xperia_a32_guest_file_call";
    fs::remove_all(root);
    fs::create_directories(root/"assets/published/data");
    {std::ofstream out(root/"assets/published/data/level.sb",std::ios::binary);
     out<<"ISHIMURA";}
    {
        GuestElf32 file_elf;
        file_elf.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
        put32(file_elf,base,0xe92d4000u); // PUSH LR
        put32(file_elf,base+4,0xe12fff33u); // BLX R3
        put32(file_elf,base+8,0xe8bd8000u); // POP PC
        const char path[]="assets/published/data/level.sb";
        std::memcpy(file_elf.writeable(base+0x300,sizeof(path)),path,sizeof(path));
        GuestHeap file_heap(file_elf);
        GuestFileSystem mount(root);
        GuestArmCallRunner file_runner(file_elf,libc,file_heap,&mount);
        u32 fd=0;
        assert(file_runner.invoke(base,{base+0x300,0,0,
            libc.symbols().at("open")},fd,error));
        assert(fd>=3);
        u32 nread=0;
        assert(file_runner.invoke(base,{fd,base+0x380,8,
            libc.symbols().at("read")},nread,error));
        assert(nread==8);
        assert(std::memcmp(file_elf.read(base+0x380,8),"ISHIMURA",8)==0);
        u32 closed=UINT32_MAX;
        assert(file_runner.invoke(base,{fd,0,0,
            libc.symbols().at("close")},closed,error));
        assert(closed==0&&mount.handle_count()==0);
        const char traversal[]="assets/../private.txt";
        std::memcpy(file_elf.writeable(base+0x300,sizeof(traversal)),
                    traversal,sizeof(traversal));
        assert(file_runner.invoke(base,{base+0x300,0,0,
            libc.symbols().at("open")},fd,error));
        assert(static_cast<std::int32_t>(fd)==-1);
    }
    fs::remove_all(root);
    std::cout<<"ARM32 translated host open/read/close sandbox: PASS\\n";
    std::cout<<"ARM32 guest function call + libc + ordered constructors: PASS\n";
    return 0;
}
