#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace xperia {
// Maps original ARM ELF32 PT_LOAD at guest_base+original vaddr into
// owned sparse guest memory, with relocated pointers using the
// *same* address space as Dynarmic. Resolves R_ARM_RELATIVE only.
// No game code executes; ABS32/GLOB_DAT/JUMP_SLOT remain unbound.
// No original EA binary is included in the module.
struct GuestSegment {
    std::uint32_t start=0;
    std::vector<std::uint8_t> contents;
};
struct PendingImport {
    std::uint32_t target=0;
    std::uint32_t kind=0;
    std::uint32_t addend=0;
    std::string name;
};
class GuestElf32 {
public:
    static constexpr std::uint32_t guest_base=0x10000000u;
    std::vector<GuestSegment> segments;
    std::size_t relative_relocations=0,remaining_relocations=0;
    std::vector<PendingImport> pending_imports;
    // Resolved original ARM constructors, ordered DT_INIT then DT_INIT_ARRAY.
    // These are guest addresses; invoking them still requires a live JIT.
    std::vector<std::uint32_t> initialization_order;
    // Exported original ARM JNI functions, not host-callable pointers.
    std::unordered_map<std::string,std::uint32_t> jni_exports;
    // All-or-nothing guest import binding, using explicit ARM32 guest
    // addresses assigned to host-service trampolines elsewhere.
    // Binding does NOT implement these services or make game executable.
    bool bind_imports(const std::unordered_map<std::string,std::uint32_t>& guest_symbols,
                      std::string& error) noexcept;
    bool open(const std::vector<std::uint8_t>& raw,std::string& error) noexcept;
    const std::uint8_t* read(std::uint32_t vaddr,std::size_t size) const noexcept;
    std::uint8_t* writeable(std::uint32_t vaddr,std::size_t size) noexcept;
    std::uint32_t read_u32(std::uint32_t vaddr) const;
};
} // namespace xperia
