#include "ds_port/original_registry_lookup.hpp"
#include <cstdint>
namespace ds_port::original_runtime {
namespace {
std::uint32_t read_arm32(const void* p) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(p);
    return std::uint32_t(b[0]) | (std::uint32_t(b[1])<<8) |
           (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
}
OriginalRegistryStatus original_query_registry_0x27e930(
    const void* original_bss_slot, std::size_t bss_slot_bytes,
    std::uint32_t original_registry_arm_address,
    const void* original_registry_object, std::size_t registry_object_bytes,
    std::uint32_t original_vtable_arm_address,
    const void* original_vtable_object, std::size_t vtable_bytes,
    OriginalRegistryQueryDispatch original_method, void* original_method_context,
    std::uint32_t* out_interface_arm_address) noexcept {
    if (!original_bss_slot || bss_slot_bytes<4 || !original_registry_object ||
        registry_object_bytes<4 || !original_vtable_object || vtable_bytes<0x1c ||
        !out_interface_arm_address || !original_registry_arm_address ||
        read_arm32(original_bss_slot)!=original_registry_arm_address)
        return OriginalRegistryStatus::InvalidMemory;
    if (!original_vtable_arm_address ||
        read_arm32(original_registry_object)!=original_vtable_arm_address ||
        read_arm32(static_cast<const std::uint8_t*>(original_vtable_object)+0x18)==0)
        return OriginalRegistryStatus::InvalidVtable;
    if (!original_method) return OriginalRegistryStatus::MissingImplementation;
    const OriginalRegistryQuery request{
        original_registry_arm_address,
        read_arm32(static_cast<const std::uint8_t*>(original_vtable_object)+0x18),
        kOriginalRegistryInterfaceId
    };
    // ARM originally initializes [sp+4] to zero and returns its word.
    // Keep output untouched on refusal: no fabricated service pointer.
    std::uint32_t returned=0;
    if (!original_method(request,original_method_context,&returned))
        return OriginalRegistryStatus::CallRefused;
    *out_interface_arm_address=returned;
    return OriginalRegistryStatus::Completed;
}
}
