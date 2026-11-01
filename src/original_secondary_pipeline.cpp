#include "ds_port/original_secondary_pipeline.hpp"
namespace ds_port::original_runtime {
namespace {
struct Bridge {
    const OriginalSecondaryPipeline* inputs;
    OriginalPostprocessStatus post_status;
};
bool allocate_bridge(const OriginalSecondaryAllocation& args,void* ctx,
                     std::uint32_t* result) noexcept {
    auto& b=*static_cast<Bridge*>(ctx);
    return b.inputs->allocator(args,b.inputs->callback_context,result);
}
bool post_bridge(std::uint32_t result,void* ctx) noexcept {
    auto& b=*static_cast<Bridge*>(ctx);
    const auto& in=*b.inputs;
    if(result!=in.allocated_object_arm_address) {
        b.post_status=OriginalPostprocessStatus::InvalidMemory;
        return false;
    }
    const OriginalPostprocessInputs post{
        result,in.allocated_object_bytes,in.allocated_object_size,
        in.initial_vtable_arm_address,in.first_setup_arg_arm_address,
        in.second_setup_arg_arm_address,in.third_setup_arg_arm_address,
        in.call_original,in.callback_context
    };
    b.post_status=original_postprocess_0x288688_prefix(post);
    return b.post_status==OriginalPostprocessStatus::Completed;
}
}
OriginalSecondaryPipelineResult original_run_secondary_pipeline(
    const OriginalSecondaryPipeline& in) noexcept {
    OriginalSecondaryPipelineResult result{
        OriginalSecondaryStatus::InvalidMemory,
        OriginalPostprocessStatus::MissingOriginalCallees,0u
    };
    if(!in.allocator || !in.call_original) {
        result.allocation_status=OriginalSecondaryStatus::MissingAllocator;
        return result;
    }
    if(!in.allocated_object_arm_address || !in.allocated_object_bytes ||
       in.allocated_object_size<0x24u) return result;
    Bridge bridge{&in,OriginalPostprocessStatus::MissingOriginalCallees};
    result.allocation_status=original_allocate_0x2849e0(
        in.owner_arm_address,in.owner_bytes,in.owner_size,
        in.vtable_bytes,in.vtable_size,kSecondaryAllocatorOriginalDescriptorVA,
        allocate_bridge,post_bridge,&bridge,&result.original_result_arm_address);
    result.postprocess_status=bridge.post_status;
    return result;
}
}
