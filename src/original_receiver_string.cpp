#include "ds_port/original_receiver_string.hpp"

namespace ds_port::original_runtime {
namespace {
void put32(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8u*i));
}
}
OriginalReceiverStringResult0x27541c
original_construct_receiver_string_0x27541c(
    const OriginalReceiverStringInputs0x27541c& in) noexcept {
    using S=OriginalReceiverStringStatus0x27541c;
    if(!in.original_string_arm_va ||
       in.original_string_arm_va>0xffffffffu-0x218u ||
       !in.mapped_string ||
       in.accessible_bytes<kOriginalReceiverStringBytes)
        return {S::InvalidMemory,0};
    if(!in.native_allocator_pool)
        return {S::MissingAllocator,0};
    // The actual original 0x27541C loads original global 0x49BCA8,
    // and calls 0x27E560 when it is null. 0x271914 implements
    // the exact same singleton getter, so reuse that native code.
    const auto allocated=original_default_pool_0x271914(
        *in.native_allocator_pool);
    if(allocated.status!=OriginalDefaultPoolStatus0x271914::CompletedExisting &&
       allocated.status!=OriginalDefaultPoolStatus0x271914::CompletedNewSingleton)
        return {S::OriginalAllocatorBlocked,0};
    const auto allocator=allocated.original_r0;
    auto* object=static_cast<std::uint8_t*>(in.mapped_string);
    // ARM 0x27543C..0x27545C: initialize 0x21C-byte object.
    // STR r0,[r4,#0xc] allocator;
    // STR #0,[r4,#0x10]; STR r4+0x18 at +0,+4,+0x14;
    // STR r4+0x218 at +8; STRH #0 at +0x18.
    const auto start=in.original_string_arm_va+0x18u;
    put32(object+0x0cu,allocator);
    put32(object+0x10u,0u);
    put32(object+0x14u,start);
    put32(object+0x04u,start);
    put32(object+0x00u,start);
    put32(object+0x08u,in.original_string_arm_va+0x218u);
    object[0x18u]=0;
    object[0x19u]=0;
    return {S::Completed,allocator};
}
} // namespace ds_port::original_runtime
