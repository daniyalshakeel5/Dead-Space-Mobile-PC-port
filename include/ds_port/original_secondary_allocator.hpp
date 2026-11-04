#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// ELF 0x2849E0 uses owner vtable +0x0c, 36-byte request,
// flags (r3=1), stack words (4,0), then invokes 0x288688
// only when its virtual allocator returns a nonzero ARM token.
// Original type-descriptor constant is supplied from separately
// resolved original literal; it is NOT invented here.
struct OriginalSecondaryAllocation {
    std::uint32_t method_arm_address;
    std::uint32_t owner_arm_address;
    std::uint32_t requested_bytes;
    std::uint32_t descriptor_arm_address;
    std::uint32_t flags;
    std::uint32_t stack0;
    std::uint32_t stack1;
};
using OriginalSecondaryDispatch=bool (*)(
    const OriginalSecondaryAllocation&, void*,std::uint32_t*) noexcept;
using OriginalSecondaryPostprocess=bool (*)(std::uint32_t,void*) noexcept;
enum class OriginalSecondaryStatus:std::uint8_t {
    InvalidMemory,InvalidVtable,MissingAllocator,AllocatorRefused,
    MissingPostprocess,PostprocessRefused,Completed
};
OriginalSecondaryStatus original_allocate_0x2849e0(
    std::uint32_t owner_arm_address,
    const void* owner_host_bytes,std::size_t owner_bytes,
    const void* vtable_host_bytes,std::size_t vtable_bytes,
    std::uint32_t descriptor_arm_address,
    OriginalSecondaryDispatch allocator,OriginalSecondaryPostprocess postprocess,
    void* context,std::uint32_t* out_arm_address) noexcept;
}
