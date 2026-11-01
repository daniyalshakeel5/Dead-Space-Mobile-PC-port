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
} // namespace ds_port::original_runtime
