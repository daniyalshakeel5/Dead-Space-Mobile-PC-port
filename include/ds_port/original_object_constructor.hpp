#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Complete original ARM routine 0x002828ac: writes object fields 0..0x12.
// Its vtable is PC-relative and must be resolved from the original ELF.
inline constexpr std::size_t kOriginalConstructorMinimumBytes=0x13u;
bool original_construct_0x2828ac(void* host_object,
    std::size_t accessible_bytes,std::uint32_t resolved_vtable_arm_va) noexcept;
}
