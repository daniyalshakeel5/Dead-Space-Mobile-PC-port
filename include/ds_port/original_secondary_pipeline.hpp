#pragma once
#include "ds_port/original_secondary_allocator.hpp"
#include "ds_port/original_secondary_postprocess.hpp"
namespace ds_port::original_runtime {
// Joins two original, proven functions: 0x2849e0 -> 0x288688.
// No original allocator/constructor implementation is substituted.
struct OriginalSecondaryPipeline {
    std::uint32_t owner_arm_address;
    const void* owner_bytes;
    std::size_t owner_size;
    const void* vtable_bytes;
    std::size_t vtable_size;
    std::uint32_t allocated_object_arm_address;
    void* allocated_object_bytes;
    std::size_t allocated_object_size;
    std::uint32_t initial_vtable_arm_address;
    std::uint32_t first_setup_arg_arm_address;
    std::uint32_t second_setup_arg_arm_address;
    std::uint32_t third_setup_arg_arm_address;
    OriginalSecondaryDispatch allocator;
    OriginalPostprocessStep call_original;
    void* callback_context;
};
struct OriginalSecondaryPipelineResult {
    OriginalSecondaryStatus allocation_status;
    OriginalPostprocessStatus postprocess_status;
    std::uint32_t original_result_arm_address;
};
OriginalSecondaryPipelineResult original_run_secondary_pipeline(
    const OriginalSecondaryPipeline& input) noexcept;
}
