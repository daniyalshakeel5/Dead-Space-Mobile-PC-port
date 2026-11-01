#include "ds_port/original_secondary_allocator.hpp"
namespace ds_port::original_runtime {
namespace {
std::uint32_t le32(const void* v) noexcept {
    const auto* p=static_cast<const std::uint8_t*>(v);
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
OriginalSecondaryStatus original_allocate_0x2849e0(
    std::uint32_t owner_arm_address,
    const void* owner_host_bytes,std::size_t owner_bytes,
    const void* vtable_host_bytes,std::size_t vtable_bytes,
    std::uint32_t descriptor_arm_address,
    OriginalSecondaryDispatch allocator,OriginalSecondaryPostprocess postprocess,
    void* context,std::uint32_t* out_arm_address) noexcept {
    if(!owner_arm_address || !owner_host_bytes || owner_bytes<4 ||
       !vtable_host_bytes || vtable_bytes<16 || !descriptor_arm_address ||
       !out_arm_address)return OriginalSecondaryStatus::InvalidMemory;
    const std::uint32_t method=le32(static_cast<const std::uint8_t*>(vtable_host_bytes)+12);
    if(!le32(owner_host_bytes)||!method)return OriginalSecondaryStatus::InvalidVtable;
    if(!allocator)return OriginalSecondaryStatus::MissingAllocator;
    const OriginalSecondaryAllocation args{
        method,owner_arm_address,0x24u,descriptor_arm_address,1u,4u,0u
    };
    std::uint32_t original_result=0;
    if(!allocator(args,context,&original_result))
        return OriginalSecondaryStatus::AllocatorRefused;
    if(original_result!=0){
        if(!postprocess)return OriginalSecondaryStatus::MissingPostprocess;
        if(!postprocess(original_result,context))
            return OriginalSecondaryStatus::PostprocessRefused;
    }
    *out_arm_address=original_result;
    return OriginalSecondaryStatus::Completed;
}
}
