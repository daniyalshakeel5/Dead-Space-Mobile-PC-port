#pragma once
#include "guest_elf32.hpp"
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
        const GuestElf32& elf,std::string& error);
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
    static constexpr unsigned last_svc=44;
    static constexpr unsigned once_svc=44;
    static constexpr unsigned finalize_svc=32;
    // Called AFTER Dynarmic's SVC callback halts JIT execution.
    bool dispatch(std::uint32_t id,std::array<std::uint32_t,16>& registers,
                  GuestElf32& image,std::string& failure,
                  GuestHeap* heap=nullptr,
                  GuestFileSystem* files=nullptr) const noexcept;
private:
    std::unordered_map<std::string,std::uint32_t> symbols_;
    std::vector<std::uint8_t> code_;
    std::unordered_map<std::uint32_t,std::string> unresolved_imports_;
    mutable std::unordered_set<std::uint32_t> initializing_guards_;
    std::vector<Destructor> destructors_;
    // Guest ARM address keyed synchronization objects. The guest runtime
    // executes one translated guest thread until scheduling is implemented.
    struct GuestMutex { bool locked=false; };
    std::unordered_map<std::uint32_t,GuestMutex> mutexes_;
    std::unordered_map<std::uint32_t,std::uint32_t> semaphores_;
};
}
