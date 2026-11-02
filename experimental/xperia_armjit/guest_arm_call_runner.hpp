#pragma once
#include "guest_elf32.hpp"
#include "guest_libc_bridge.hpp"
#include "guest_heap.hpp"
#include "guest_jni_vm.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace xperia {
// Reusable AAPCS/softfp ARM32 execution boundary on Dynarmic's x86-64 JIT.
// Does not pretend to implement Android JNI, OpenGL ES or original audio.
class GuestFileSystem;
class GuestArmCallRunner {
public:
    GuestArmCallRunner(GuestElf32& image,GuestLibcBridge& services,
                       GuestHeap& heap,GuestFileSystem* files=nullptr,
                       GuestJniVm* java=nullptr);
    ~GuestArmCallRunner();
    GuestArmCallRunner(const GuestArmCallRunner&)=delete;
    GuestArmCallRunner& operator=(const GuestArmCallRunner&)=delete;
    bool invoke(std::uint32_t guest_address,
                const std::array<std::uint32_t,4>& aapcs_args,
                std::uint32_t& r0,std::string& error);
    // AAPCS softfp: first four words in r0-r3; the remaining words are
    // placed at [SP] and SP stays 8-byte aligned. Required for Xperia
    // touch events (7 arguments) and audio initialization (6 arguments).
    bool invoke_aapcs(std::uint32_t guest_address,
                      const std::vector<std::uint32_t>& arguments,
                      std::uint32_t& r0,std::string& error);
    bool run_original_initializers(std::string& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
