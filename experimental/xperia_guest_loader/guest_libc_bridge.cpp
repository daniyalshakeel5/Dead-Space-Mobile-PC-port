#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include "guest_filesystem.hpp"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
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
        "malloc","calloc","free","realloc",
        "open","read","lseek","close",
        "fopen","fread","fseek","ftell","fclose",
        "__android_log_write","gettimeofday","clock_gettime","time",
        "__cxa_guard_acquire","__cxa_guard_release","__cxa_guard_abort",
        "__cxa_pure_virtual", "__aeabi_atexit", "__cxa_atexit",
        "__errno", "__cxa_finalize",
        "pthread_mutex_init","pthread_mutex_destroy",
        "pthread_mutex_lock","pthread_mutex_trylock","pthread_mutex_unlock",
        "sem_init","sem_destroy","sem_post","sem_wait","sem_trywait",
        "sem_getvalue","pthread_once",
        "__atomic_cmpxchg","__atomic_swap","__atomic_inc","__atomic_dec"
    };
    for(unsigned i=0;i<last_svc;++i){
        const std::uint32_t id=i+1;
        symbols_.emplace(names[i],veneer_base+i*8);
        const std::uint32_t instructions[2]={0xef000000u|id,0xe12fff1eu};
        for(auto ins:instructions)
            for(unsigned j=0;j<4;++j)
                code_.push_back(std::uint8_t(ins>>(8u*j)));
    }
}
std::unordered_map<std::string,std::uint32_t>
GuestLibcBridge::plan_guest_imports(
    const GuestElf32& elf,std::string& error,
    const std::unordered_map<std::string,std::uint32_t>& data_symbols,
    const std::unordered_map<std::string,std::uint32_t>& host_functions) {
    std::unordered_map<std::string,std::uint32_t> combined=symbols_;
    try {
        for(const auto& entry:data_symbols) {
            if(!entry.second || !combined.emplace(entry).second)
                throw std::runtime_error("Duplicate/invalid Bionic guest data symbol: "+entry.first);
        }
        for(const auto& entry:host_functions) {
            if(!entry.second || !combined.emplace(entry).second)
                throw std::runtime_error("Duplicate/invalid graphics host symbol: "+entry.first);
        }
        if(!unresolved_imports_.empty())
            throw std::runtime_error("Guest import plan already installed");
        std::map<std::string,unsigned> missing;
        for(const auto& rel:elf.pending_imports) {
            if(combined.count(rel.name))continue;
            // The original ELF uses STT_FUNC=2 for callable external
            // imports. STT_OBJECT / STT_NOTYPE can represent data and must
            // not be redirected into SVC instructions.
            if(rel.symbol_type!=2u)
                throw std::runtime_error("Unimplemented guest data/ambiguous ELF symbol: "+rel.name);
            if(rel.kind==2u && rel.addend!=0u)
                throw std::runtime_error("Unimplemented imported function with ABS32 addend: "+rel.name);
            ++missing[rel.name];
        }
        if(missing.size()>2048u)
            throw std::runtime_error("Guest unsupported function inventory exceeds cap");
        // Stable alphabetical enumeration gives reproducible imported
        // symbols and first-fault reports across operating systems.
        for(const auto& kv:missing) {
            const auto id=static_cast<std::uint32_t>(
                last_svc+unresolved_imports_.size()+1u);
            const auto slot=veneer_base+static_cast<std::uint32_t>(code_.size());
            unresolved_imports_.emplace(id,kv.first);
            combined.emplace(kv.first,slot);
            const std::uint32_t instructions[2]={0xef000000u|id,0xe12fff1eu};
            for(auto ins:instructions)
                for(unsigned byte=0;byte<4;++byte)
                    code_.push_back(std::uint8_t(ins>>(byte*8u)));
        }
        error.clear();return combined;
    }catch(const std::exception& ex){
        error=ex.what();
        return {};
    }
}
std::string GuestLibcBridge::unresolved_import(std::uint32_t id) const {
    const auto it=unresolved_imports_.find(id);
    return it==unresolved_imports_.end()?std::string{}:it->second;
}
bool GuestLibcBridge::code_byte(std::uint32_t address,
                                std::uint8_t& value) const noexcept {
    if(address<veneer_base ||
       std::uint64_t(address)-veneer_base>=code_.size())return false;
    value=code_[address-veneer_base];return true;
}
bool GuestLibcBridge::dispatch(
    std::uint32_t id,std::array<std::uint32_t,16>& regs,
    GuestElf32& image,std::string& failure,GuestHeap* heap,
    GuestFileSystem* files) const noexcept {
    try {
        if(id<1||id>last_svc)
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
        case 12: { // open(filename, O_RDONLY, ignored mode)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            std::string path;
            for(std::uint32_t i=0;i<4096u;++i){
                if(std::uint64_t(a)+i>UINT32_MAX)
                    throw std::runtime_error("ARM open filename pointer wrap");
                const auto character=byte(image,a+i);
                if(!character)break;
                path.push_back(static_cast<char>(character));
            }
            if(path.size()==4096u)
                throw std::runtime_error("Unterminated ARM asset path");
            result=static_cast<std::uint32_t>(files->open_readonly(path,b));
            break;
        }
        case 13: { // read(fd, guest_buffer, byte_count)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            if(n>GuestFileSystem::max_io)
                throw std::runtime_error("ARM read length exceeds bounded transfer");
            if(n==0){result=0;break;}
            auto* destination=image.writeable(b,n);
            if(!destination)throw std::runtime_error("Unmapped ARM read destination");
            result=static_cast<std::uint32_t>(files->read(
                static_cast<std::int32_t>(a),destination,n));
            break;
        }
        case 14: { // lseek(fd, signed_offset, SEEK_SET/CUR/END)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            result=static_cast<std::uint32_t>(files->seek(
                static_cast<std::int32_t>(a),
                static_cast<std::int32_t>(b),static_cast<int>(n)));
            break;
        }
        case 15: { // close(fd)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            result=static_cast<std::uint32_t>(files->close(static_cast<int>(a)));
            break;
        }
        case 16: { // FILE* fopen(path, "r" / "rb")
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            std::string path,mode;
            for(std::uint32_t i=0;i<4096;++i){
                if(std::uint64_t(a)+i>UINT32_MAX)
                    throw std::runtime_error("ARM fopen path overflow");
                const auto v=byte(image,a+i);
                if(!v)break;
                path.push_back(static_cast<char>(v));
            }
            if(path.size()>=4096)
                throw std::runtime_error("Unterminated ARM fopen path");
            for(std::uint32_t i=0;i<16;++i){
                if(std::uint64_t(b)+i>UINT32_MAX)
                    throw std::runtime_error("ARM fopen mode overflow");
                const auto v=byte(image,b+i);
                if(!v)break;
                mode.push_back(static_cast<char>(v));
            }
            if(mode.size()>=16)
                throw std::runtime_error("Unterminated ARM fopen mode");
            if(mode!="r"&&mode!="rb"){result=0;break;}
            const auto handle=files->open_readonly(path,0);
            result=handle<0?0:static_cast<std::uint32_t>(handle);
            break;
        }
        case 17: { // fread(ptr, element_size, count, guest_FILE*)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            const std::uint64_t count=std::uint64_t(b)*n;
            if(count>GuestFileSystem::max_io)
                throw std::runtime_error("Guest fread transfer exceeds bound");
            if(!count||!b){result=0;break;}
            auto* destination=image.writeable(a,static_cast<std::size_t>(count));
            if(!destination)throw std::runtime_error("Unmapped ARM fread destination");
            const auto bytes=files->read(static_cast<int>(regs[3]),
                                         destination,static_cast<std::size_t>(count));
            if(bytes<0)throw std::runtime_error("Invalid guest FILE* in fread");
            result=static_cast<std::uint32_t>(bytes)/b;
            break;
        }
        case 18: { // fseek(FILE*, signed offset, whence)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            const auto position=files->seek(static_cast<int>(a),
                static_cast<std::int32_t>(b),static_cast<int>(n));
            result=position<0?UINT32_MAX:0u;
            break;
        }
        case 19: { // ftell(FILE*)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            result=static_cast<std::uint32_t>(files->seek(
                static_cast<int>(a),0,SEEK_CUR));
            break;
        }
        case 20: { // fclose(FILE*)
            if(!files)throw std::runtime_error("Guest asset mount unavailable");
            result=static_cast<std::uint32_t>(files->close(static_cast<int>(a)));
            break;
        }
        case 21: { // __android_log_write(priority, tag, text)
            auto guest_log_string=[&](std::uint32_t pointer,std::size_t cap){
                std::string value;
                if(!pointer)return value;
                for(std::size_t i=0;i<cap;++i){
                    if(std::uint64_t(pointer)+i>UINT32_MAX)
                        throw std::runtime_error("ARM log pointer wrap");
                    const auto character=byte(image,pointer+static_cast<std::uint32_t>(i));
                    if(!character)return value;
                    value.push_back(character>=32u?char(character):'.');
                }
                throw std::runtime_error("Unterminated ARM log string");
            };
            auto tag=guest_log_string(b,255);
            auto msg=guest_log_string(n,8192);
            std::clog<<"[xperia-android/"<<(a&255u)<<"/"<<tag<<"] "<<msg<<"\n";
            result=static_cast<std::uint32_t>(msg.size());
            break;
        }
        case 22: { // gettimeofday(struct timeval*, struct timezone*)
            if(!a)throw std::runtime_error("ARM gettimeofday requires timeval pointer");
            using namespace std::chrono;
            const auto us=duration_cast<microseconds>(
                system_clock::now().time_since_epoch()).count();
            const auto sec=us/1000000ll;
            if(sec<0||sec>INT32_MAX)
                throw std::runtime_error("ARM32 timeval time_t overflow");
            auto* tv=image.writeable(a,8);
            if(!tv)throw std::runtime_error("ARM gettimeofday timeval unmapped");
            const auto put=[&](std::uint8_t* slot,std::uint32_t value){
                for(int i=0;i<4;++i)slot[i]=static_cast<std::uint8_t>(value>>(8*i));
            };
            put(tv,static_cast<std::uint32_t>(sec));
            put(tv+4,static_cast<std::uint32_t>(us%1000000));
            if(b){
                auto* zone=image.writeable(b,8);
                if(!zone)throw std::runtime_error("ARM timezone pointer unmapped");
                put(zone,0);put(zone+4,0);
            }
            result=0;break;
        }
        case 23: { // clock_gettime(clockid_t, struct timespec*)
            using namespace std::chrono;
            if(!b)throw std::runtime_error("ARM clock_gettime requires timespec");
            std::int64_t ns=0;
            if(a==0u)ns=duration_cast<nanoseconds>(
                system_clock::now().time_since_epoch()).count();
            else if(a==1u||a==4u)ns=duration_cast<nanoseconds>(
                steady_clock::now().time_since_epoch()).count();
            else {result=UINT32_MAX;break;} // EINVAL/unsupported clock id
            auto* ts=image.writeable(b,8);
            if(!ts)throw std::runtime_error("ARM timespec pointer unmapped");
            const auto sec=ns/1000000000ll;
            if(sec<0||sec>INT32_MAX)
                throw std::runtime_error("ARM32 timespec time_t overflow");
            const auto put=[&](std::uint8_t* slot,std::uint32_t value){
                for(int i=0;i<4;++i)slot[i]=static_cast<std::uint8_t>(value>>(8*i));
            };
            put(ts,static_cast<std::uint32_t>(sec));
            put(ts+4,static_cast<std::uint32_t>(ns%1000000000ll));
            result=0;break;
        }
        case 24: { // time(time_t* optional)
            using namespace std::chrono;
            const auto sec=duration_cast<seconds>(
                system_clock::now().time_since_epoch()).count();
            if(sec<0||sec>INT32_MAX)
                throw std::runtime_error("ARM32 time_t overflow");
            if(a){
                auto* out=image.writeable(a,4);
                if(!out)throw std::runtime_error("ARM time(time_t*) pointer unmapped");
                for(int i=0;i<4;++i)out[i]=std::uint8_t(
                    std::uint32_t(sec)>>(8*i));
            }
            result=static_cast<std::uint32_t>(sec);break;
        }
        case 25: { // __cxa_guard_acquire: ARM EABI 4-byte guard
            auto* guard=image.writeable(a,4);
            if(!guard || (a&3u))
                throw std::runtime_error("Invalid or misaligned C++ static guard");
            if(guard[0]){result=0;break;}
            // This bridge does not implement blocking pthread waits.
            // Reentrant/cross-thread initialization must NOT report success.
            if(!initializing_guards_.insert(a).second)
                throw std::runtime_error("Concurrent C++ static initialization unsupported");
            result=1;break;
        }
        case 26: { // __cxa_guard_release
            auto* guard=image.writeable(a,4);
            if(!guard || (a&3u) || !initializing_guards_.erase(a))
                throw std::runtime_error("C++ static guard released without acquire");
            guard[0]=1;
            result=0;break;
        }
        case 27: { // __cxa_guard_abort
            auto* guard=image.writeable(a,4);
            if(!guard || (a&3u) || !initializing_guards_.erase(a))
                throw std::runtime_error("C++ static guard aborted without acquire");
            guard[0]=0;
            result=0;break;
        }
        case 28:
            throw std::runtime_error("Original ARM code invoked __cxa_pure_virtual");
        case 29: // __aeabi_atexit(object, destructor, dso)
        case 30: { // __cxa_atexit(destructor, argument, dso)
            const auto function=id==29?b:a;
            const auto argument=id==29?a:b;
            if(!function || !image.read(function&~1u,2))
                throw std::runtime_error("Unmapped ARM C++ destructor function");
            if(destructors_.size()>=65536u)
                throw std::runtime_error("ARM registered destructor limit");
            destructors_.push_back({function,argument,n});
            result=0;break;
        }
        case 31: { // __errno() -> pointer to guest-owned ARM32 int
            constexpr std::uint32_t kErrno=0x72000000u;
            if(!image.read(kErrno,4)) {
                if(image.read(kErrno,1)||image.read(kErrno+3,1))
                    throw std::runtime_error("Guest errno storage overlaps memory");
                image.segments.push_back({kErrno,std::vector<std::uint8_t>(4,0)});
            }
            result=kErrno;break;
        }
        case 32: // __cxa_finalize requires translated ARM destructors
            throw std::runtime_error("C++ finalizers require guest JIT callback runner");
        case 33: { // pthread_mutex_init(mutex*, default attr)
            auto* guest=image.writeable(a,4);
            if(!guest)throw std::runtime_error("Guest pthread mutex unmapped");
            if(b)throw std::runtime_error("Nondefault pthread mutex attributes unsupported");
            if(mutexes_.count(a)){result=16;break;} // EBUSY
            mutexes_.emplace(a,GuestMutex{});
            std::fill_n(guest,4,0);
            result=0;break;
        }
        case 34: { // pthread_mutex_destroy
            auto* guest=image.writeable(a,4);
            if(!guest)throw std::runtime_error("Guest pthread mutex unmapped");
            auto it=mutexes_.find(a);
            if(it!=mutexes_.end() && it->second.locked){result=16;break;}
            if(it==mutexes_.end() && image.read_u32(a)!=0)
                throw std::runtime_error("Unknown Bionic pthread mutex state");
            if(it!=mutexes_.end())mutexes_.erase(it);
            std::fill_n(guest,4,0xff); // reject use after destroy
            result=0;break;
        }
        case 35: // pthread_mutex_lock
        case 36: { // pthread_mutex_trylock
            if(!image.read(a,4))
                throw std::runtime_error("Guest pthread mutex unmapped");
            auto it=mutexes_.find(a);
            if(it==mutexes_.end()){
                if(image.read_u32(a)!=0)
                    throw std::runtime_error("Unsupported Bionic mutex initial state");
                it=mutexes_.emplace(a,GuestMutex{}).first; // static initializer
            }
            if(it->second.locked) {
                if(id==35)
                    throw std::runtime_error("Blocking pthread mutex requires guest thread scheduler");
                result=16; // EBUSY
            }else{
                it->second.locked=true;
                result=0;
            }
            break;
        }
        case 37: { // pthread_mutex_unlock
            if(!image.read(a,4))
                throw std::runtime_error("Guest pthread mutex unmapped");
            const auto it=mutexes_.find(a);
            if(it==mutexes_.end() || !it->second.locked){result=1;break;} // EPERM
            it->second.locked=false;
            result=0;break;
        }
        case 38: { // sem_init(sem*, pshared, initial_count)
            auto* guest=image.writeable(a,4);
            if(!guest)throw std::runtime_error("Guest semaphore unmapped");
            if(b || n>static_cast<std::uint32_t>(INT_MAX))
                throw std::runtime_error("Unsupported shared/oversized guest semaphore");
            if(semaphores_.count(a))
                throw std::runtime_error("Guest semaphore initialized twice");
            semaphores_.emplace(a,n);
            for(int i=0;i<4;++i)guest[i]=std::uint8_t(n>>(8*i));
            result=0;break;
        }
        case 39: { // sem_destroy
            auto* guest=image.writeable(a,4);
            if(!guest || !semaphores_.erase(a))
                throw std::runtime_error("Unknown guest semaphore");
            std::fill_n(guest,4,0xff);
            result=0;break;
        }
        case 40: // sem_post
        case 41: // sem_wait
        case 42: { // sem_trywait
            const auto it=semaphores_.find(a);
            auto* guest=image.writeable(a,4);
            if(it==semaphores_.end() || !guest)
                throw std::runtime_error("Guest semaphore not initialized");
            auto& count=it->second;
            if(id==40){
                if(count==static_cast<std::uint32_t>(INT_MAX))
                    throw std::runtime_error("Guest semaphore overflow");
                ++count;
                result=0;
            }else if(count){
                --count;result=0;
            }else if(id==41){
                throw std::runtime_error("Blocking sem_wait requires guest thread scheduler");
            }else{
                // sem_trywait returns -1 with errno=EAGAIN (not EBUSY).
                constexpr std::uint32_t kErrno=0x72000000u;
                if(!image.read(kErrno,4))
                    image.segments.push_back({kErrno,std::vector<std::uint8_t>(4,0)});
                auto* ep=image.writeable(kErrno,4);
                if(!ep)throw std::runtime_error("Guest errno storage unavailable");
                ep[0]=11;ep[1]=ep[2]=ep[3]=0;
                result=UINT32_MAX;
            }
            for(int i=0;i<4;++i)guest[i]=std::uint8_t(count>>(8*i));
            break;
        }
        case 43: { // sem_getvalue(sem*, int*)
            const auto it=semaphores_.find(a);
            auto* out=image.writeable(b,4);
            if(it==semaphores_.end() || !out)
                throw std::runtime_error("Invalid guest sem_getvalue");
            for(int i=0;i<4;++i)out[i]=std::uint8_t(it->second>>(8*i));
            result=0;break;
        }
        case 44:
            throw std::runtime_error("pthread_once requires translated ARM callback runner");
        case 45: { // __atomic_cmpxchg(old, desired, volatile int*)
            if((n&3u) || !image.writeable(n,4))
                throw std::runtime_error("ARM atomic CAS pointer invalid");
            const auto previous=image.read_u32(n);
            if(previous==a){
                auto* target=image.writeable(n,4);
                for(unsigned i=0;i<4;++i)target[i]=std::uint8_t(b>>(i*8u));
                result=0; // Bionic: zero means exchange succeeded
            }else result=1;
            break;
        }
        case 46: // __atomic_swap(new, volatile int*)
        case 47: // __atomic_inc(volatile int*)
        case 48: { // __atomic_dec(volatile int*)
            const auto ptr=id==46?b:a;
            if((ptr&3u) || !image.writeable(ptr,4))
                throw std::runtime_error("ARM atomic pointer invalid");
            const auto previous=image.read_u32(ptr);
            const auto updated=id==46?a:id==47?previous+1u:previous-1u;
            auto* target=image.writeable(ptr,4);
            for(unsigned i=0;i<4;++i)target[i]=std::uint8_t(updated>>(i*8u));
            result=previous; // Bionic inc/dec/swap return PREVIOUS value
            break;
        }
        }
        regs[0]=result;
        failure.clear();return true;
    }catch(const std::exception& err){
        failure=err.what();return false;
    }
}
std::vector<GuestLibcBridge::Destructor>
GuestLibcBridge::take_destructors(std::uint32_t dso) {
    std::vector<Destructor> selected;
    // Reverse original registration order, with exactly-once removal.
    for(auto it=destructors_.rbegin();it!=destructors_.rend();++it)
        if(dso==0 || it->dso==dso)selected.push_back(*it);
    destructors_.erase(std::remove_if(destructors_.begin(),destructors_.end(),
        [dso](const Destructor& item){return dso==0||item.dso==dso;}),
        destructors_.end());
    return selected;
}
} // namespace xperia
