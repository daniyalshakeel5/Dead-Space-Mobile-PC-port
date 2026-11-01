#include "ds_port/original_mode1_resource_factory.hpp"
namespace ds_port::original_runtime {
namespace {
void store32(void* where,std::uint32_t value) noexcept {
    auto* p=static_cast<std::uint8_t*>(where);
    for(unsigned i=0;i<4;++i) p[i]=static_cast<std::uint8_t>(value>>(8u*i));
}
}
OriginalMode1FactoryResult original_construct_mode1_handle_0x28b3a4(
    const OriginalMode1FactoryInputs& in,std::uint32_t owner,
    std::string_view name) noexcept {
    auto fail=[](OriginalMode1FactoryStatus status) noexcept {
        return OriginalMode1FactoryResult{status,{}};
    };
    if(!in.mapped_handle || in.handle_bytes<kOriginalMode1HandleBytes ||
       !owner)
        return fail(OriginalMode1FactoryStatus::InvalidMemory);
    if(!in.original_0x28a718)
        return fail(OriginalMode1FactoryStatus::MissingOriginalResolver);
    // ARM 0x28B3BC..0x28B3D0: original handle layout.
    // The original r2 is exactly 1 at its callsite in 0x280444.
    auto* dest=static_cast<std::uint8_t*>(in.mapped_handle);
    store32(dest+0x0,kOriginalMode1HandleVtable);
    store32(dest+0x4,1);
    store32(dest+0x8,0);
    store32(dest+0xc,owner);
    // In the original mode=1 path, 0x28A718 produces the pointer
    // stored at +8. The internal parsing and allocation remain
    // engine dependencies: refusing them cannot mark engine ready.
    std::uint32_t resource=0;
    if(!in.original_0x28a718(owner,name,in.context,&resource))
        return fail(OriginalMode1FactoryStatus::OriginalResolverRefused);
    store32(dest+0x8,resource);
    return {OriginalMode1FactoryStatus::CompletedOriginalMode1Constructor,
            {in.mapped_handle,in.handle_bytes,0,nullptr,0,0,nullptr,0}};
}

OriginalUnreadyHandleStatus original_cleanup_unready_mode1_0x28a4f8(
    const OriginalResourceHandleInput& in) noexcept {
    if(!in.mapped_handle||in.handle_bytes<kOriginalMode1HandleBytes)
        return OriginalUnreadyHandleStatus::InvalidMemory;
    auto* p=static_cast<std::uint8_t*>(
        const_cast<void*>(in.mapped_handle));
    const auto load=[](const std::uint8_t* x) noexcept -> std::uint32_t {
        return std::uint32_t(x[0])|(std::uint32_t(x[1])<<8)|
               (std::uint32_t(x[2])<<16)|(std::uint32_t(x[3])<<24);
    };
    if(load(p)!=kOriginalMode1HandleVtable||load(p+4)!=1u)
        return OriginalUnreadyHandleStatus::WrongOriginalMode;
    // Original 0x28A4F8 calls a resource vtable +0x0c only if +8 !=0.
    // That unrelated virtual method is not reconstructed; never release
    // an unknown live object with invented behavior.
    if(load(p+8)!=0u)
        return OriginalUnreadyHandleStatus::UnreconstructedNonNullResource;
    // Original destructor's vtable literal was independently proven
    // to be 0x00489B20 (run 41327221816).
    store32(p,kOriginalMode1HandleVtable);
    store32(p+8,0u);
    return OriginalUnreadyHandleStatus::CompletedNullResourceCleanup;
}
} // namespace ds_port::original_runtime
