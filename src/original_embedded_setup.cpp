#include "ds_port/original_embedded_setup.hpp"
#include <cstdint>
namespace ds_port::original_runtime {
namespace {
std::uint32_t arm32(const void* pointer) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(pointer);
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
           (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
}
OriginalEmbeddedSetupStatus original_execute_embedded_setup_0x27fa74(
    const void* embedded, std::size_t size,
    std::uint32_t arg1, std::uint32_t arg2,
    const OriginalEmbeddedService& svc, std::uint32_t* output) noexcept {
    if(!embedded || size<8 || !output ||
       !svc.interface_arm_va || !svc.interface_object ||
       svc.interface_object_bytes<4 || !svc.interface_vtable ||
       svc.interface_vtable_bytes<0x88 || !svc.interface_vtable_arm_va)
        return OriginalEmbeddedSetupStatus::InvalidMemory;
    if(arm32(svc.interface_object)!=svc.interface_vtable_arm_va ||
       !arm32(static_cast<const std::uint8_t*>(svc.interface_vtable)+0x84))
        return OriginalEmbeddedSetupStatus::InvalidOriginalInterfaceVtable;
    if(!svc.query_registry)
        return OriginalEmbeddedSetupStatus::MissingOriginalRegistry;
    std::uint32_t returned_interface=0;
    const auto reg=original_query_registry_0x27e930(
        svc.registry_bss_slot,svc.registry_bss_slot_bytes,
        svc.registry_arm_va,svc.registry_object,svc.registry_object_bytes,
        svc.registry_vtable_arm_va,svc.registry_vtable,svc.registry_vtable_bytes,
        svc.query_registry,svc.callback_context,&returned_interface);
    if(reg!=OriginalRegistryStatus::Completed)
        return OriginalEmbeddedSetupStatus::RegistryLookupRefused;
    if(returned_interface!=svc.interface_arm_va)
        return OriginalEmbeddedSetupStatus::UnexpectedInterface;
    if(!svc.call_method84)
        return OriginalEmbeddedSetupStatus::MissingOriginalMethod84;
    const OriginalEmbeddedMethod84Call request{
        arm32(static_cast<const std::uint8_t*>(svc.interface_vtable)+0x84),
        returned_interface,arm32(static_cast<const std::uint8_t*>(embedded)+4),
        arg1,arg2
    };
    std::uint32_t return_value=0;
    if(!svc.call_method84(request,svc.callback_context,&return_value))
        return OriginalEmbeddedSetupStatus::OriginalMethod84Refused;
    *output=return_value;
    return OriginalEmbeddedSetupStatus::Completed;
}
} // namespace ds_port::original_runtime
