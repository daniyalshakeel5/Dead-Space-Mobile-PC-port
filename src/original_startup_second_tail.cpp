#include "ds_port/original_startup_second_tail.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t word(const void* src) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(src);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void put(std::uint8_t* dest,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i)dest[i]=std::uint8_t(value>>(i*8));
}
struct Runner {
    const OriginalStartupSecondTailInputs& in;
    OriginalStartupSecondTailStatus status=OriginalStartupSecondTailStatus::InvalidOriginalContext;
    bool mapped(std::uint32_t original_va,std::uint32_t slot,
                OriginalStartupServiceMemory* object,std::uint32_t* method) noexcept {
        if(!in.resolve_object || !original_va ||
           !in.resolve_object(original_va,in.callback_context,object) ||
           object->original_service_arm_address!=original_va ||
           !object->host_object||object->object_bytes<4||
           !object->original_vtable_arm_address ||
           word(object->host_object)!=object->original_vtable_arm_address ||
           !object->host_vtable||object->vtable_bytes<slot+4) {
            status=OriginalStartupSecondTailStatus::InvalidOriginalContext;return false;
        }
        *method=word(static_cast<const std::uint8_t*>(object->host_vtable)+slot);
        if(!*method) {
            status=OriginalStartupSecondTailStatus::MissingOriginalVirtualMethod;return false;
        }
        return true;
    }
    bool invoke(std::uint32_t original_owner,std::uint32_t slot,
                std::uint32_t r1,std::uint32_t r2,std::uint32_t r3,
                std::uint8_t known_mask,
                OriginalStartupVirtualResult* out) noexcept {
        if(!in.invoke_virtual) {
            status=OriginalStartupSecondTailStatus::MissingOriginalVirtualMethod;return false;
        }
        OriginalStartupServiceMemory obj{};
        std::uint32_t method=0;
        if(!mapped(original_owner,slot,&obj,&method))return false;
        const OriginalStartupVirtualCall call{
            original_owner,method,slot,r1,r2,r3,known_mask,nullptr
        };
        if(!in.invoke_virtual(call,in.callback_context,out)) {
            status=OriginalStartupSecondTailStatus::OriginalVirtualRefused;return false;
        }
        return true;
    }
    bool allocate(std::uint32_t bytes,std::uint32_t descriptor,
                  OriginalAllocatorReturn* out) noexcept {
        if(!in.original_allocator_vtable_0x0c) {
            status=OriginalStartupSecondTailStatus::MissingOriginalAllocator;return false;
        }
        const auto owner=word(static_cast<const std::uint8_t*>(in.original_context)+4);
        OriginalStartupServiceMemory obj{};std::uint32_t method=0;
        if(!mapped(owner,0x0cu,&obj,&method))return false;
        const OriginalAllocatorCall args{
            method,owner,bytes,descriptor,1u,4u,0u
        };
        if(!in.original_allocator_vtable_0x0c(
            args,in.callback_context,out)) {
            status=OriginalStartupSecondTailStatus::OriginalAllocatorRefused;return false;
        }
        // Original assembly would dereference returned null allocation;
        // reject it rather than accessing address zero or pretending boot.
        if(!out->original_allocated_record_arm_address) {
            status=OriginalStartupSecondTailStatus::OriginalAllocatorReturnedNull;
            return false;
        }
        if(!out->record_host_bytes||out->accessible_record_bytes<bytes) {
            status=OriginalStartupSecondTailStatus::InvalidOriginalContext;return false;
        }
        return true;
    }
};
}
OriginalStartupSecondTailStatus original_execute_stage_zero_tail_0x280580(
    const OriginalStartupSecondTailInputs& in) noexcept {
    if(!in.original_context||in.context_bytes<0x58||
       !in.original_context_arm_va||!in.resolve_object)
        return OriginalStartupSecondTailStatus::InvalidOriginalContext;
    Runner x{in,OriginalStartupSecondTailStatus::InvalidOriginalContext};
    auto* ctx=static_cast<std::uint8_t*>(in.original_context);
    if(!in.invoke_original_direct)
        return OriginalStartupSecondTailStatus::MissingOriginalDirect;
    // Complete 0x27FD7C wrapper: 27 ordered native calls into the
    // still-unresolved original 0x284178 engine resource handler.
    if(in.native_registration_0x27fd7c) {
        if(in.native_registration_0x27fd7c->original_context_arm_va!=
           in.original_context_arm_va)
            return OriginalStartupSecondTailStatus::InvalidOriginalContext;
        if(original_register_stage_resources_0x27fd7c(
            *in.native_registration_0x27fd7c)!=
             OriginalRegistrationStatus::CompletedOriginalSequence)
            return OriginalStartupSecondTailStatus::OriginalDirectRefused;
    } else if(!in.invoke_original_direct(
                  0x0027fd7cu,in.original_context_arm_va,
                  in.callback_context)) {
        return OriginalStartupSecondTailStatus::OriginalDirectRefused;
    }
    OriginalStartupVirtualResult r{};
    // Original context vtable +0x54 (r1..r3 caller-saved by direct call).
    if(!x.invoke(in.original_context_arm_va,0x54u,0,0,0,0,&r))return x.status;
    if(!in.original_resource_0x2803e4)
        return OriginalStartupSecondTailStatus::MissingOriginalResource;
    std::uint32_t found=0;
    // Resource query 0x2803E4 tries primary, then fallback only if zero.
    if(!in.original_resource_0x2803e4(
          0x002803e4u,in.original_context_arm_va,kStartupResourcePrimary,
          in.callback_context,&found))
        return OriginalStartupSecondTailStatus::OriginalResourceRefused;
    if(!found && !in.original_resource_0x2803e4(
          0x002803e4u,in.original_context_arm_va,kStartupResourceFallback,
          in.callback_context,&found))
        return OriginalStartupSecondTailStatus::OriginalResourceRefused;

    OriginalAllocatorReturn object34{};
    if(!x.allocate(0x34u,kStartupAllocate34Descriptor,&object34))
        return x.status;
    // Exactly one original 0x27E560 implementation remains, with
    // verified GOT binding through original_bind_static_singleton_from_got.
    const auto base_provider=in.native_singleton_0x27e560
        ? original_static_singleton_base_provider
        : in.original_base_0x27e560;
    void* base_context=in.native_singleton_0x27e560
        ? const_cast<OriginalSingletonInputs*>(in.native_singleton_0x27e560)
        : in.callback_context;
    if(!base_provider)
        return OriginalStartupSecondTailStatus::MissingOriginalBaseConstructor;
    if(!original_construct_object34_0x287a94(
        object34.original_allocated_record_arm_address,
        object34.record_host_bytes,object34.accessible_record_bytes,
        base_provider,base_context))
        return OriginalStartupSecondTailStatus::OriginalBaseConstructorRefused;
    // STR r5,[r4,#0xc] and vtable +0x0c(r0=allocated,r1=context,r2=owner).
    put(ctx+0x0c,object34.original_allocated_record_arm_address);
    if(!x.invoke(object34.original_allocated_record_arm_address,0x0cu,
          in.original_context_arm_va,word(ctx+4),0,3,&r))
        return x.status;

    // Original 0x28061C BL 0x2849E0 invokes native secondary allocation,
    // including recovered 0x288688 -> 0x2828AC -> 0x27FB28 -> 0x27FA74.
    if(!in.native_secondary_0x2849e0)
        return OriginalStartupSecondTailStatus::SecondaryPipelineBlocked;
    if(in.native_secondary_0x2849e0->owner_arm_address!=word(ctx+4))
        return OriginalStartupSecondTailStatus::InvalidOriginalContext;
    const auto secondary=original_run_secondary_pipeline(
        *in.native_secondary_0x2849e0);
    if(secondary.allocation_status!=OriginalSecondaryStatus::Completed ||
       !secondary.original_result_arm_address)
        return OriginalStartupSecondTailStatus::SecondaryPipelineBlocked;
    put(ctx+0x14,secondary.original_result_arm_address);
    if(!x.invoke(secondary.original_result_arm_address,0x0cu,
          in.original_context_arm_va,word(ctx+4),0,3,&r))
        return x.status;

    // 0x28063C: direct no-argument global service 0x36FD38.
    if(!in.invoke_original_direct(0x0036fd38u,0,in.callback_context))
        return OriginalStartupSecondTailStatus::OriginalDirectRefused;

    OriginalAllocatorReturn object58{};
    if(!x.allocate(0x58u,kStartupAllocate58Descriptor,&object58))
        return x.status;
    if(!original_construct_object58_0x280a34(
        object58.original_allocated_record_arm_address,
        object58.record_host_bytes,object58.accessible_record_bytes,
        base_provider,base_context))
        return OriginalStartupSecondTailStatus::OriginalBaseConstructorRefused;
    put(ctx+0x10,object58.original_allocated_record_arm_address);
    if(!x.invoke(object58.original_allocated_record_arm_address,0x24u,
          word(ctx+4),in.original_context_arm_va,0,3,&r))
        return x.status;
    return OriginalStartupSecondTailStatus::CompletedOriginalSecondRoutine;
}
}
