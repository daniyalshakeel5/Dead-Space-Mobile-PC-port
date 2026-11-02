#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_libc_bridge.hpp"
#include "guest_filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
using namespace xperia;
int main() {
    GuestElf32 image;
    image.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    GuestLibcBridge svc;
    const auto p=0x10001100u,q=0x10001200u;
    auto* src=image.writeable(p,8);
    assert(src);
    std::memcpy(src,"CORRIDOR",8);
    std::array<std::uint32_t,16> regs{};
    std::string error;
    regs[0]=q;regs[1]=p;regs[2]=8;
    assert(svc.dispatch(1,regs,image,error)); // memcpy
    assert(regs[0]==q&&std::memcmp(image.read(q,8),"CORRIDOR",8)==0);
    regs[0]=q;regs[1]=0x41;regs[2]=2;
    assert(svc.dispatch(3,regs,image,error)); // memset
    assert(image.read(q,2)[0]=='A'&&image.read(q,2)[1]=='A');
    regs[0]=q;regs[1]=p;regs[2]=8;
    assert(svc.dispatch(4,regs,image,error)); // memcmp
    assert(static_cast<std::int32_t>(regs[0])<0);
    // Source and destination overlap, should work as memmove.
    regs[0]=p+1;regs[1]=p;regs[2]=7;
    assert(svc.dispatch(2,regs,image,error));
    assert(std::memcmp(image.read(p+1,7),"CORRIDO",7)==0);
    auto* txt=image.writeable(0x10001300,16);
    std::memcpy(txt,"Ishimura",9);
    regs[0]=0x10001300;
    assert(svc.dispatch(5,regs,image,error)); // strlen
    assert(regs[0]==8);
    std::memcpy(image.writeable(0x10001400,12),"Ishimura",9);
    regs[0]=0x10001300;regs[1]=0x10001400;
    assert(svc.dispatch(6,regs,image,error)); // strcmp
    assert(regs[0]==0);
    auto* second=image.writeable(0x10001400,10);
    second[3]='M';
    regs[0]=0x10001300;regs[1]=0x10001400;regs[2]=3;
    assert(svc.dispatch(7,regs,image,error)); // strncmp
    assert(regs[0]==0);
    regs[0]=0x10001300;regs[1]=0x10001400;regs[2]=8;
    assert(svc.dispatch(7,regs,image,error));
    assert(static_cast<std::int32_t>(regs[0])>0);
    // A guest pointer error must not read arbitrary host memory.
    regs[0]=0xeeeeeeee;regs[1]=p;regs[2]=8;
    assert(!svc.dispatch(1,regs,image,error)&&!error.empty());
    regs[0]=0xeeeeeeee;regs[1]=p;regs[2]=0;
    assert(svc.dispatch(1,regs,image,error)); // no dereference for zero size
    assert(!svc.dispatch(500,regs,image,error));
    std::uint8_t byte=0;
    auto veneer=svc.symbols().at("memcpy");
    assert(svc.code_byte(veneer,byte)&&byte==1);
    assert(svc.code_byte(veneer+3,byte)&&byte==0xef); // SVC #1
    assert(svc.code_byte(veneer+4,byte)&&byte==0x1e); // BX LR
    assert(!svc.code_byte(0x1000,byte));
    // Original ARM32 C++ static local initialization ABI.
    const auto guard=0x10001800u;
    regs.fill(0);regs[0]=guard;
    assert(svc.dispatch(25,regs,image,error) && regs[0]==1);
    regs[0]=guard;
    assert(!svc.dispatch(25,regs,image,error)); // reentrant acquire must fail
    regs[0]=guard;
    assert(svc.dispatch(26,regs,image,error)); // release
    assert(image.read(guard,8)[0]==1);
    regs[0]=guard;
    assert(svc.dispatch(25,regs,image,error) && regs[0]==0);
    assert(!svc.dispatch(28,regs,image,error)); // pure virtual traps
    const auto aborted=guard+8;
    regs[0]=aborted;
    assert(svc.dispatch(25,regs,image,error) && regs[0]==1);
    regs[0]=aborted;
    assert(svc.dispatch(27,regs,image,error));
    assert(image.read(aborted,8)[0]==0);
    regs[0]=aborted;
    assert(svc.dispatch(25,regs,image,error) && regs[0]==1);
    regs[0]=aborted;
    assert(svc.dispatch(27,regs,image,error));
    // Android Bionic old NDK atomics use return conventions that differ
    // from std::atomic<bool> CAS and from post-increment arithmetic.
    const auto atomic_ptr=0x10001880u;
    auto* atomic_target=image.writeable(atomic_ptr,4);
    assert(atomic_target);
    std::memset(atomic_target,0,4);
    regs.fill(0);regs[0]=0;regs[1]=17;regs[2]=atomic_ptr;
    assert(svc.dispatch(45,regs,image,error) && regs[0]==0);
    assert(image.read_u32(atomic_ptr)==17);
    regs[0]=0;regs[1]=42;regs[2]=atomic_ptr;
    assert(svc.dispatch(45,regs,image,error) && regs[0]==1);
    assert(image.read_u32(atomic_ptr)==17);
    regs[0]=9;regs[1]=atomic_ptr;
    assert(svc.dispatch(46,regs,image,error) && regs[0]==17);
    assert(image.read_u32(atomic_ptr)==9);
    regs[0]=atomic_ptr;
    assert(svc.dispatch(47,regs,image,error) && regs[0]==9);
    assert(image.read_u32(atomic_ptr)==10);
    regs[0]=atomic_ptr;
    assert(svc.dispatch(48,regs,image,error) && regs[0]==10);
    assert(image.read_u32(atomic_ptr)==9);
    regs[0]=0x10001881u;
    assert(!svc.dispatch(48,regs,image,error));
    // Original Android ARM C++ object lifecycle and errno TLS ABI.
    const auto guest_dtor=0x10001900u;
    regs.fill(0);
    regs[0]=p;regs[1]=guest_dtor;regs[2]=0x111u;
    assert(svc.dispatch(29,regs,image,error)); // __aeabi_atexit
    assert(regs[0]==0);
    regs.fill(0);
    regs[0]=guest_dtor;regs[1]=q;regs[2]=0x222u;
    assert(svc.dispatch(30,regs,image,error)); // __cxa_atexit
    assert(regs[0]==0);
    const auto first=svc.take_destructors(0x111u);
    assert(first.size()==1 && first[0].function==guest_dtor &&
           first[0].argument==p && first[0].dso==0x111u);
    const auto second=svc.take_destructors(0);
    assert(second.size()==1 && second[0].argument==q &&
           second[0].dso==0x222u);
    assert(svc.take_destructors(0).empty());
    regs.fill(0);
    assert(svc.dispatch(31,regs,image,error)); // __errno
    const auto errno_ptr=regs[0];
    assert(errno_ptr==0x72000000u);
    assert(image.writeable(errno_ptr,4));
    image.writeable(errno_ptr,4)[0]=13;
    regs.fill(0);
    assert(svc.dispatch(31,regs,image,error));
    assert(regs[0]==errno_ptr && image.read(errno_ptr,1)[0]==13);
    assert(!svc.dispatch(32,regs,image,error)); // JIT callback required
    // Guest pthread mutex state machine: real uncontended lock and unlock.
    const auto mutex_ptr=0x10001840u;
    regs.fill(0);regs[0]=mutex_ptr;regs[1]=0;
    assert(svc.dispatch(33,regs,image,error)&&regs[0]==0);
    regs[0]=mutex_ptr;
    assert(svc.dispatch(35,regs,image,error)&&regs[0]==0);
    regs[0]=mutex_ptr;
    assert(svc.dispatch(36,regs,image,error)&&regs[0]==16); // EBUSY
    regs[0]=mutex_ptr;
    assert(svc.dispatch(34,regs,image,error)&&regs[0]==16); // locked
    regs[0]=mutex_ptr;
    assert(svc.dispatch(37,regs,image,error)&&regs[0]==0);
    regs[0]=mutex_ptr;
    assert(svc.dispatch(34,regs,image,error)&&regs[0]==0);
    regs[0]=mutex_ptr;
    assert(!svc.dispatch(35,regs,image,error)); // use-after-destroy
    // Guest POSIX semaphore counts and try-wait errno behavior.
    const auto sem_ptr=0x10001850u,sem_value_ptr=0x10001860u;
    regs.fill(0);regs[0]=sem_ptr;regs[1]=0;regs[2]=2;
    assert(svc.dispatch(38,regs,image,error)&&regs[0]==0);
    regs[0]=sem_ptr;
    assert(svc.dispatch(40,regs,image,error)&&regs[0]==0);
    regs[0]=sem_ptr;regs[1]=sem_value_ptr;
    assert(svc.dispatch(43,regs,image,error)&&regs[0]==0);
    assert(image.read_u32(sem_value_ptr)==3);
    for(int i=0;i<3;++i){
        regs[0]=sem_ptr;
        assert(svc.dispatch(41,regs,image,error)&&regs[0]==0);
    }
    assert(image.read_u32(sem_ptr)==0);
    regs[0]=sem_ptr;
    assert(svc.dispatch(42,regs,image,error)&&regs[0]==UINT32_MAX);
    assert(image.read_u32(errno_ptr)==11); // EAGAIN
    regs[0]=sem_ptr;
    assert(!svc.dispatch(41,regs,image,error)); // no guest thread scheduler
    regs[0]=sem_ptr;
    assert(svc.dispatch(39,regs,image,error)&&regs[0]==0);
    assert(!svc.dispatch(44,regs,image,error)); // pthread_once requires JIT
    assert(!svc.symbols().count("glDrawArrays"));
    assert(svc.symbols().count("malloc")); // implemented by bounded GuestHeap
    assert(svc.symbols().count("open") && svc.symbols().count("read"));
    namespace fs=std::filesystem;
    const auto install=fs::temp_directory_path()/"xperia_arm_svc_file_fixture";
    fs::remove_all(install);
    fs::create_directories(install/"assets/published/data");
    {std::ofstream file(install/"assets/published/data/level.sb",
                        std::ios::binary);file<<"STASIS!";}
    GuestFileSystem assets(install);
    const auto guest_path=0x10001500u, guest_output=0x10001600u;
    const char* relative="assets/published/data/level.sb";
    std::memcpy(image.writeable(guest_path,std::strlen(relative)+1),
                relative,std::strlen(relative)+1);
    regs.fill(0);
    regs[0]=guest_path;regs[1]=0;
    assert(svc.dispatch(12,regs,image,error,nullptr,&assets)); // open
    const auto fd=regs[0];
    assert(fd>=3u && assets.handle_count()==1);
    regs[0]=fd;regs[1]=guest_output;regs[2]=7;
    assert(svc.dispatch(13,regs,image,error,nullptr,&assets)); // read
    assert(regs[0]==7u);
    assert(std::memcmp(image.read(guest_output,7),"STASIS!",7)==0);
    regs[0]=fd;regs[1]=0;regs[2]=0;
    assert(svc.dispatch(14,regs,image,error,nullptr,&assets)); // lseek
    assert(regs[0]==0);
    regs[0]=fd;
    assert(svc.dispatch(15,regs,image,error,nullptr,&assets)); // close
    assert(regs[0]==0 && assets.handle_count()==0);
    regs[0]=guest_path;regs[1]=0;
    assert(!svc.dispatch(12,regs,image,error)); // no mount = no fabricated file
    const char* escaped="assets/../outside.txt";
    std::memcpy(image.writeable(guest_path,std::strlen(escaped)+1),
                escaped,std::strlen(escaped)+1);
    regs[0]=guest_path;regs[1]=0;
    assert(svc.dispatch(12,regs,image,error,nullptr,&assets));
    assert(static_cast<std::int32_t>(regs[0])==-1);
    // Stdio functions use the same sandboxed handles and ARM32 ABI.
    const char* mode="rb";
    std::memcpy(image.writeable(guest_output,3),mode,3);
    std::memcpy(image.writeable(guest_path,std::strlen(relative)+1),
                relative,std::strlen(relative)+1);
    regs.fill(0);regs[0]=guest_path;regs[1]=guest_output;
    assert(svc.dispatch(16,regs,image,error,nullptr,&assets)); // fopen
    const auto fileptr=regs[0];
    assert(fileptr>=3u);
    regs.fill(0);
    regs[0]=guest_output+32;regs[1]=2;regs[2]=3;regs[3]=fileptr;
    assert(svc.dispatch(17,regs,image,error,nullptr,&assets)); // fread
    assert(regs[0]==3);
    assert(std::memcmp(image.read(guest_output+32,6),"STASIS",6)==0);
    regs[0]=fileptr;
    assert(svc.dispatch(19,regs,image,error,nullptr,&assets)); // ftell
    assert(regs[0]==6);
    regs[0]=fileptr;regs[1]=0;regs[2]=0;
    assert(svc.dispatch(18,regs,image,error,nullptr,&assets)); // fseek
    assert(regs[0]==0);
    regs[0]=fileptr;
    assert(svc.dispatch(20,regs,image,error,nullptr,&assets)); // fclose
    assert(regs[0]==0);
    // Opening for writing must fail, never mutate original game data.
    std::memcpy(image.writeable(guest_output,3),"wb",3);
    regs[0]=guest_path;regs[1]=guest_output;
    assert(svc.dispatch(16,regs,image,error,nullptr,&assets));
    assert(regs[0]==0);
    // Android log and clock ABI are real guest imports in Xperia v1.1.33.
    const char tag[]="xperia-test";
    const char message[]="engine boot";
    std::memcpy(image.writeable(guest_path,sizeof(tag)),tag,sizeof(tag));
    std::memcpy(image.writeable(guest_output,sizeof(message)),message,sizeof(message));
    regs.fill(0);regs[0]=4;regs[1]=guest_path;regs[2]=guest_output;
    assert(svc.dispatch(21,regs,image,error)); // __android_log_write
    assert(regs[0]==sizeof(message)-1);
    regs.fill(0);regs[0]=guest_output+64;regs[1]=guest_output+80;
    assert(svc.dispatch(22,regs,image,error)); // gettimeofday
    assert(regs[0]==0 && image.read_u32(guest_output+64)>1700000000u);
    assert(image.read_u32(guest_output+68)<1000000u);
    regs.fill(0);regs[0]=1;regs[1]=guest_output+96;
    assert(svc.dispatch(23,regs,image,error)); // CLOCK_MONOTONIC
    assert(regs[0]==0 && image.read_u32(guest_output+100)<1000000000u);
    regs.fill(0);regs[0]=guest_output+112;
    assert(svc.dispatch(24,regs,image,error)); // time
    assert(regs[0]==image.read_u32(guest_output+112));
    fs::remove_all(install);
    return 0;
}
