#pragma once
#include "guest_elf32.hpp"
#include "guest_threads.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>

namespace xperia {
class GuestHeap;
class GuestFileSystem;
// ABI boundary: service calls exchange only guest AAPCS r0-r3 values.
// Unsupported imports are intentionally absent; never invent success.
class GuestLibcBridge {
public:
    static constexpr std::uint32_t veneer_base=0x70000000u;
    GuestLibcBridge();
    // Bind only observed ELF function symbols to named SVC fail traps.
    // Undefined objects/ambiguous symbols are rejected, never fabricated.
    std::unordered_map<std::string,std::uint32_t> plan_guest_imports(
        const GuestElf32& elf,std::string& error,
        const std::unordered_map<std::string,std::uint32_t>& data_symbols={},
        const std::unordered_map<std::string,std::uint32_t>& host_functions={});
    std::string unresolved_import(std::uint32_t svc_id) const;
    std::size_t unresolved_import_count() const noexcept {
        return unresolved_imports_.size();
    }
    const std::unordered_map<std::string,std::uint32_t>& symbols() const noexcept {
        return symbols_;
    }
    // SVC #id; BX LR, as little-endian A32 instructions.
    bool code_byte(std::uint32_t address,std::uint8_t& value) const noexcept;
    // Registered by the actual ARM constructors; executes ONLY through the
    // guest-call runner's translated ARM callback path, in reverse order.
    struct Destructor { std::uint32_t function, argument, dso; };
    std::vector<Destructor> take_destructors(std::uint32_t dso);
    static constexpr unsigned last_svc=101;
    static constexpr unsigned cond_wait_svc=68;
    static constexpr unsigned cond_timedwait_svc=69;
    static constexpr unsigned create_thread_svc=49;
    static constexpr unsigned join_thread_svc=50;
    static constexpr unsigned yield_thread_svc=62;
    static constexpr unsigned exit_thread_svc=63;
    GuestThreads& guest_threads() noexcept {return guest_threads_;}
    const GuestThreads& guest_threads() const noexcept {return guest_threads_;}
    // Cond-wait releases the ARM mutex, runs a translated worker, and
    // reacquires only after that worker signalled the actual condition.
    bool begin_cond_wait(std::uint32_t cond,std::uint32_t mutex,
                         std::uint32_t& generation,std::string& error);
    bool cond_signalled(std::uint32_t cond,std::uint32_t generation) const noexcept;
    bool end_cond_wait(std::uint32_t mutex,std::string& error);
    bool cancel_cond_wait(std::uint32_t mutex,std::string& error);
    static constexpr unsigned once_svc=44;
    static constexpr unsigned finalize_svc=32;
    // Called AFTER Dynarmic's SVC callback halts JIT execution.
    bool dispatch(std::uint32_t id,std::array<std::uint32_t,16>& registers,
                  GuestElf32& image,std::string& failure,
                  GuestHeap* heap=nullptr,
                  GuestFileSystem* files=nullptr) noexcept;
private:
    std::unordered_map<std::string,std::uint32_t> symbols_;
    std::vector<std::uint8_t> code_;
    std::unordered_map<std::uint32_t,std::string> unresolved_imports_;
    mutable std::unordered_set<std::uint32_t> initializing_guards_;
    std::vector<Destructor> destructors_;
    // Guest ARM address keyed synchronization objects. The guest runtime
    // executes one translated guest thread until scheduling is implemented.
    struct GuestMutex {
        bool locked=false;
        std::uint32_t owner=0,recursion=0,type=0;
    };
    std::unordered_map<std::uint32_t,GuestMutex> mutexes_;
    std::unordered_map<std::uint32_t,std::uint32_t> semaphores_;
    struct GuestCondition { std::uint32_t generation=0; };
    std::unordered_map<std::uint32_t,GuestCondition> conditions_;
    struct GuestTlsKey {
        std::uint32_t destructor=0;
        std::unordered_map<std::uint32_t,std::uint32_t> values;
    };
    std::unordered_map<std::uint32_t,GuestTlsKey> tls_keys_;
    std::uint32_t next_tls_key_=1;
    std::unordered_map<std::uint32_t,std::uint32_t> mutex_attrs_;
    GuestThreads guest_threads_;
};
}
