#pragma once
#include "guest_elf32.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace xperia {
// ARM32 guest-side JavaVM/JNIEnv ABI. This is NOT a general JVM: unsupported
// JNI table slots trap by index and cannot fabricate a successful Java call.
// All objects and function pointers below are 32-bit guest addresses.
class GuestJniVm {
public:
    static constexpr std::uint32_t vm_address=0x74000000u;
    static constexpr std::uint32_t env_address=0x74000004u;
    static constexpr std::uint32_t table_address=0x74000100u;
    static constexpr std::uint32_t env_table_address=0x74000200u;
    static constexpr std::uint32_t thunk_base=0x74100000u;
    static constexpr std::uint32_t vm_svc_base=0x1000u;
    static constexpr std::uint32_t env_svc_base=0x2000u;
    static constexpr unsigned vm_slots=8;
    static constexpr unsigned env_slots=233;
    // JNI v1.6 table layouts use 8 VM and 233 environment entries.
    bool install(GuestElf32& guest,std::string& error);
    bool code_byte(std::uint32_t addr,std::uint8_t& value) const noexcept;
    bool handles(std::uint32_t id) const noexcept;
    std::uint32_t return_pc(std::uint32_t id) const noexcept;
    bool dispatch(std::uint32_t id,std::array<std::uint32_t,16>& regs,
                  GuestElf32& guest,std::string& error) noexcept;
    // Called by the translated ARM runner after JNI dispatch. EAIO.Startup
    // is an actual Java-declared native function in the installed game ELF.
    struct NativeCallback {
        std::uint32_t address=0;
        std::array<std::uint32_t,4> arguments{};
    };
    NativeCallback take_callback() noexcept {
        auto out=pending_callback_;pending_callback_={};return out;
    }
    void set_content_ready(bool ready) noexcept {content_ready_=ready;}
    std::uint32_t vm() const noexcept {return vm_address;}
private:
    std::vector<std::uint8_t> thunks_;
    NativeCallback pending_callback_{};
    std::uint32_t eaio_startup_=0;
    bool content_ready_=false;
    bool installed_=false;
};
} // namespace xperia
