#pragma once
#include "guest_elf32.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
namespace xperia {
// Original ELF imports Bionic OBJECT symbols; these must be mapped data,
// never addresses of SVC function veneers or host x86 pointers.
class GuestBionicData {
public:
    static constexpr std::uint32_t base=0x74200000u;
    bool install(GuestElf32& guest,std::string& error);
    const std::unordered_map<std::string,std::uint32_t>& symbols() const noexcept {
        return symbols_;
    }
private:
    std::unordered_map<std::string,std::uint32_t> symbols_;
};
}
