#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Original ELF 0x27E930 requests interface ID 0x10004 from the
// BSS singleton 0x49CAA0 using its virtual method at offset +0x18.
// ARM32 values remain tokens; no host pointer reinterpretation.
inline constexpr std::uint32_t kOriginalRegistryInterfaceId=0x10004u;
struct OriginalRegistryQuery {
    std::uint32_t original_registry_arm_address;
    std::uint32_t original_method_arm_address;
    std::uint32_t interface_id;
};
using OriginalRegistryQueryDispatch = bool (*)(
    const OriginalRegistryQuery&, void*, std::uint32_t*) noexcept;
enum class OriginalRegistryStatus : std::uint8_t {
    InvalidMemory, InvalidVtable, MissingImplementation,
    CallRefused, Completed
};
// Recreates only proven original ABI: service request 0x10004,
// vtable slot 0x18, output word initialized to 0, and returned output.
// Dispatch is a REQUIRED externally verified original-method implementation.
OriginalRegistryStatus original_query_registry_0x27e930(
    const void* original_bss_slot, std::size_t bss_slot_bytes,
    std::uint32_t original_registry_arm_address,
    const void* original_registry_object, std::size_t registry_object_bytes,
    std::uint32_t original_vtable_arm_address,
    const void* original_vtable_object, std::size_t vtable_bytes,
    OriginalRegistryQueryDispatch original_method,
    void* original_method_context,
    std::uint32_t* out_interface_arm_address) noexcept;
}
