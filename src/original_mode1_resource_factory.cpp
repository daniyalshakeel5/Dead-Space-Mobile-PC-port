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
} // namespace ds_port::original_runtime
