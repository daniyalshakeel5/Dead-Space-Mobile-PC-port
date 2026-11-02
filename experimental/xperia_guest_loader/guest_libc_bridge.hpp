#pragma once
#include "guest_elf32.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xperia {
class GuestHeap;
class GuestFileSystem;
// ABI boundary: service calls exchange only guest AAPCS r0-r3 values.
// Unsupported imports are intentionally absent; never invent success.
class GuestLibcBridge {
public:
    static constexpr std::uint32_t veneer_base=0x70000000u;
    GuestLibcBridge();
    const std::unordered_map<std::string,std::uint32_t>& symbols() const noexcept {
        return symbols_;
    }
    // SVC #id; BX LR, as little-endian A32 instructions.
    bool code_byte(std::uint32_t address,std::uint8_t& value) const noexcept;
    // Called AFTER Dynarmic's SVC callback halts JIT execution.
    bool dispatch(std::uint32_t id,std::array<std::uint32_t,16>& registers,
                  GuestElf32& image,std::string& failure,
                  GuestHeap* heap=nullptr,
                  GuestFileSystem* files=nullptr) const noexcept;
private:
    std::unordered_map<std::string,std::uint32_t> symbols_;
    std::vector<std::uint8_t> code_;
};
}
