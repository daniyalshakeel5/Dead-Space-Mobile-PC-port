#include "ds_port/original_runtime_globals.hpp"
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
static_assert(kOriginalRecordGlobalVA-kOriginalFrameGlobalVA==
              kOriginalRecordGlobalOffset,"original ARM record slot offset");
static_assert(kOriginalType2FloatVA-kOriginalFrameGlobalVA==
              kOriginalType2FloatOffset,"original ARM runtime float offset");

bool original_bind_known_global_fields(
    void* original_global_host_span, std::size_t accessible_bytes,
    void** mapped_record_slot_0x0049cb94,
    const void** mapped_type2_float_0x0049cd1c) noexcept {
    if(!original_global_host_span ||
       accessible_bytes<kOriginalKnownGlobalSpan ||
       !mapped_record_slot_0x0049cb94 ||
       !mapped_type2_float_0x0049cd1c)
        return false;
    auto* base=static_cast<std::uint8_t*>(original_global_host_span);
    *mapped_record_slot_0x0049cb94=base+kOriginalRecordGlobalOffset;
    *mapped_type2_float_0x0049cd1c=base+kOriginalType2FloatOffset;
    return true;
}
static_assert(kOriginalFrameGlobalVA-kOriginalRegistrySlotVA==
              kOriginalRegistryToFrameOffset,"registry/frame alignment");
static_assert(kOriginalRecordGlobalVA-kOriginalRegistrySlotVA==
              kOriginalRegistryToRecordOffset,"registry/record alignment");
static_assert(kOriginalType2FloatVA-kOriginalRegistrySlotVA==
              kOriginalRegistryToType2FloatOffset,"registry/float alignment");
static_assert(kOriginalRegistryAndGlobalsSpan==
              kOriginalRegistryToType2FloatOffset+4,"mapped original bytes");

bool original_bind_registry_and_runtime_globals(
    void* original_registry_based_host_span,
    std::size_t accessible_bytes,
    void** registry_slot_0x0049caa0,
    void** frame_state_0x0049caa8,
    void** record_slot_0x0049cb94,
    const void** type2_float_0x0049cd1c) noexcept {
    if(!original_registry_based_host_span ||
       accessible_bytes<kOriginalRegistryAndGlobalsSpan ||
       !registry_slot_0x0049caa0 ||
       !frame_state_0x0049caa8 ||
       !record_slot_0x0049cb94 ||
       !type2_float_0x0049cd1c)
        return false;
    auto* base=static_cast<std::uint8_t*>(original_registry_based_host_span);
    *registry_slot_0x0049caa0=base;
    *frame_state_0x0049caa8=base+kOriginalRegistryToFrameOffset;
    *record_slot_0x0049cb94=base+kOriginalRegistryToRecordOffset;
    *type2_float_0x0049cd1c=base+kOriginalRegistryToType2FloatOffset;
    return true;
}
} // namespace ds_port::original_runtime
