#pragma once
#include "ds_port/original_embedded_setup.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Full ARM function 0x27FB28 (+0x00 through +0xC0) with 0x27FAF4's
// register/stack wrapper; virtual service method bodies are still missing.
inline constexpr std::uint32_t kOriginal27fb28EmbeddedVtableVA=0x004897b8u;
inline constexpr std::uint32_t kOriginal27fb28Method84Arg2VA=0x0042f968u;
inline constexpr std::uint32_t kOriginal27fb28Method84Arg3VA=0x0042f448u;
enum class OriginalEmbeddedInitStatus:std::uint8_t {
    InvalidMemory, InvalidOriginalVtable, MissingOriginalVirtualMethods,
    RegistryLookupRefused, UnexpectedInterface, OriginalVirtualMethodRefused,
    Completed
};
OriginalEmbeddedInitStatus original_initialize_embedded_0x27fb28(
    void* embedded_memory,std::size_t bytes,std::uint32_t original_arm_va,
    const OriginalEmbeddedService& original_service,
    std::uint32_t* original_return_arm_va) noexcept;
}
