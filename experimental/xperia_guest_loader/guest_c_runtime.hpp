#pragma once
#include "guest_elf32.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace xperia {
class GuestHeap;
// Original Android ARM32 libc entrypoints: no host x64 pointer is ever
// dereferenced as an ARM guest pointer; format strings are parsed with bounds.
class GuestCRuntime {
public:
    static constexpr unsigned first_svc=85u,last_svc=101u;
    static bool dispatch(unsigned id,std::array<std::uint32_t,16>& regs,
                         GuestElf32& guest,GuestHeap* heap,
                         std::string& error) noexcept;
};
}
