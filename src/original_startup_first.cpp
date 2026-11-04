#include "ds_port/original_startup_first.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t read32(const void* p) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(p);
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
           (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
struct Runner {
    const OriginalStartupFirstInputs& input;
    OriginalStartupFirstStatus status=OriginalStartupFirstStatus::InvalidOriginalObject;
    bool lookup(std::uint32_t* result) noexcept {
        const auto& r=input.registry;
        if(!r.registry_method) {
            status=OriginalStartupFirstStatus::MissingOriginalRegistry;return false;
        }
        const auto st=original_query_registry_0x27e930(
            r.bss_slot,r.bss_bytes,r.registry_arm_va,
            r.registry_object,r.registry_object_bytes,
            r.registry_vtable_arm_va,r.registry_vtable,r.registry_vtable_bytes,
            r.registry_method,input.context,result);
        if(st!=OriginalRegistryStatus::Completed) {
            status=OriginalStartupFirstStatus::OriginalRegistryRefused;return false;
        }
        if(!*result) {
            status=OriginalStartupFirstStatus::InvalidOriginalObject;return false;
        }
        return true;
    }
    bool call(std::uint32_t owner,std::uint32_t slot,
              std::uint32_t r1,std::uint32_t r2,std::uint32_t r3,
              std::uint8_t mask, std::uint32_t* saved_r3,
              OriginalStartupVirtualResult* result) noexcept {
        if(!input.resolve_service) {
            status=OriginalStartupFirstStatus::InvalidOriginalObject;return false;
        }
        if(!input.call_virtual) {
            status=OriginalStartupFirstStatus::MissingOriginalVirtualMethod;return false;
        }
        OriginalStartupServiceMemory obj{};
        if(!owner || !input.resolve_service(owner,input.context,&obj)||
           obj.original_service_arm_address!=owner ||
           !obj.host_object || obj.object_bytes<4 ||
           !obj.original_vtable_arm_address ||
           read32(obj.host_object)!=obj.original_vtable_arm_address ||
           !obj.host_vtable || obj.vtable_bytes<slot+4) {
            status=OriginalStartupFirstStatus::InvalidOriginalObject;return false;
        }
        const auto target=read32(static_cast<const std::uint8_t*>(obj.host_vtable)+slot);
        if(!target) {
            status=OriginalStartupFirstStatus::MissingOriginalVirtualMethod;return false;
        }
        OriginalStartupVirtualResult reply{};
        const OriginalStartupVirtualCall call{
            owner,target,slot,r1,r2,r3,mask,saved_r3
        };
        if(!input.call_virtual(call,input.context,&reply)) {
            status=OriginalStartupFirstStatus::OriginalVirtualRefused;return false;
        }
        *result=reply;return true;
    }
    bool nested_0x27f124(
        std::uint32_t* a,std::uint32_t* b,std::uint32_t* c) noexcept {
        // ARM 0x27F124: BL 0x27E930, STR output A, virtual +0x18,
        // STR B, virtual +0x1C4, direct BL 0x2068A8, STR C.
        if(!lookup(a))return false;
        OriginalStartupVirtualResult v{};
        if(!call(*a,0x18u,kStartupNestedDescriptor,0,0,1,nullptr,&v))
            return false;
        *b=v.r0;
        if(!call(*a,0x1c4u,*b,kStartupNestedArg2,kStartupNestedArg3,
                 7,nullptr,&v))
            return false;
        if(!input.direct_0x2068a8) {
            status=OriginalStartupFirstStatus::MissingOriginalDirectCall;
            return false;
        }
        if(!input.direct_0x2068a8(*a,*b,v.r0,input.context,c)) {
            status=OriginalStartupFirstStatus::OriginalDirectRefused;
            return false;
        }
        return true;
    }
};
}
OriginalStartupFirstStatus original_execute_startup_first_0x27f790(
    const OriginalStartupFirstInputs& in) noexcept {
    Runner run{in,OriginalStartupFirstStatus::InvalidOriginalObject};
    std::uint32_t r4=0;
    if(!run.lookup(&r4))return run.status;
    OriginalStartupVirtualResult v{};
    // 0x27F7A0..0x27F7B4: first query, vtable +0x18.
    if(!run.call(r4,0x18u,kStartupFirstDescriptor,0,0,1,nullptr,&v))
        return run.status;
    const std::uint32_t r5=v.r0;
    // 0x27F7B8..0x27F7DC: same owner, slot +0x1C4.
    if(!run.call(r4,0x1c4u,r5,kStartupFirstArg2,kStartupFirstArg3,
                 7,nullptr,&v))return run.status;
    const std::uint32_t r6=v.r0;

    // 0x27F7E0..0x27F804: nested call 0x27F124(context,&A,&B,&C).
    std::uint32_t out_a=0,out_b=0,out_c=0;
    if(!run.nested_0x27f124(&out_a,&out_b,&out_c))return run.status;

    // 0x27F808..0x27F82C: nested registry object's vtable +0x84.
    if(!run.call(out_a,0x84u,out_b,kStartupNextArg2,kStartupNextArg3,
                 7,nullptr,&v))return run.status;
    const std::uint32_t result_84=v.r0;
    // 0x2713B8 wrapper uses pointer to the saved r3 register and
    // virtual +0x8C; the callback receives the real mapped host word.
    std::uint32_t saved_r3=v.r3_after;
    if(!run.call(out_a,0x8cu,out_c,result_84,0,3,&saved_r3,&v))
        return run.status;
    // 0xF895C wrapper: original r0=r4,r1=r5,r2=r6,r3=above result,
    // virtual +0x238. Preserve pointer-to-saved-r3 ABI explicitly.
    std::uint32_t final_saved_r3=v.r0;
    if(!run.call(r4,0x238u,r5,r6,0,3,&final_saved_r3,&v))
        return run.status;
    return OriginalStartupFirstStatus::CompletedOriginalFirstRoutine;
}
} // namespace ds_port::original_runtime
