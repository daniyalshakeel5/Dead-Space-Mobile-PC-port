#include "ds_port/original_embedded_initializer.hpp"
#include "ds_port/original_secondary_postprocess.hpp"
#include <initializer_list>
namespace ds_port::original_runtime {
namespace {
void write32(std::uint8_t* p,std::uint32_t x) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(x>>(i*8));
}
std::uint32_t read32(const void* p) noexcept {
    const auto* b=static_cast<const std::uint8_t*>(p);
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
           (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
bool invoke(const OriginalEmbeddedService& s,std::uint32_t slot,
            std::uint32_t r1,std::uint32_t r2,std::uint32_t r3,
            std::uint8_t known_mask,std::uint32_t* saved_r3,
            OriginalEmbeddedVirtualResult* result) noexcept {
    const auto* table=static_cast<const std::uint8_t*>(s.interface_vtable);
    const OriginalEmbeddedVirtualCall call{slot,read32(table+slot),
        s.interface_arm_va,r1,r2,r3,known_mask,saved_r3};
    return s.call_full_virtual(call,s.callback_context,result);
}
}
OriginalEmbeddedInitStatus original_initialize_embedded_0x27fb28(
    void* memory,std::size_t size,std::uint32_t address,
    const OriginalEmbeddedService& svc,std::uint32_t* return_address) noexcept {
    if(!memory||size<12||!address||!return_address||
       !svc.interface_arm_va||!svc.interface_object||
       svc.interface_object_bytes<4||!svc.interface_vtable||
       svc.interface_vtable_bytes<0x88||!svc.interface_vtable_arm_va)
        return OriginalEmbeddedInitStatus::InvalidMemory;
    if(read32(svc.interface_object)!=svc.interface_vtable_arm_va)
        return OriginalEmbeddedInitStatus::InvalidOriginalVtable;
    for(auto slot:{0x18u,0x84u,0x74u,0x54u,0x5cu})
        if(!read32(static_cast<const std::uint8_t*>(svc.interface_vtable)+slot))
            return OriginalEmbeddedInitStatus::InvalidOriginalVtable;
    if(!svc.call_full_virtual||!svc.query_registry)
        return OriginalEmbeddedInitStatus::MissingOriginalVirtualMethods;

    auto* obj=static_cast<std::uint8_t*>(memory);
    // Original 0x27FB3C..0x27FB44: zero +8,+4; install vtable at +0.
    write32(obj+8,0);write32(obj+4,0);
    write32(obj,kOriginal27fb28EmbeddedVtableVA);
    std::uint32_t iface=0;
    const auto registry=original_query_registry_0x27e930(
        svc.registry_bss_slot,svc.registry_bss_slot_bytes,svc.registry_arm_va,
        svc.registry_object,svc.registry_object_bytes,
        svc.registry_vtable_arm_va,svc.registry_vtable,svc.registry_vtable_bytes,
        svc.query_registry,svc.callback_context,&iface);
    if(registry!=OriginalRegistryStatus::Completed)
        return OriginalEmbeddedInitStatus::RegistryLookupRefused;
    if(iface!=svc.interface_arm_va)
        return OriginalEmbeddedInitStatus::UnexpectedInterface;

    OriginalEmbeddedVirtualResult v{0,0};
    if(!invoke(svc,0x18u,kOriginal27fb28FirstArgVA,0,0,1,nullptr,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    const auto resource=v.r0;
    if(!invoke(svc,0x84u,resource,kOriginal27fb28Method84Arg2VA,
               kOriginal27fb28Method84Arg3VA,7,nullptr,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    // 0x27FAF4: native wrapper of slot +0x74, with saved r3 stack word.
    std::uint32_t saved_r3=v.r3_after;
    if(!invoke(svc,0x74u,resource,v.r0,0,3,&saved_r3,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    if(!invoke(svc,0x54u,v.r0,0,0,1,nullptr,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    write32(obj+8,v.r0);
    if(!invoke(svc,0x54u,resource,0,0,1,nullptr,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    write32(obj+4,v.r0);
    if(!invoke(svc,0x5cu,resource,0,0,1,nullptr,&v))
        return OriginalEmbeddedInitStatus::OriginalVirtualMethodRefused;
    *return_address=address; // Original MOV r0,r5; POP {r4,r5,r6,pc}.
    return OriginalEmbeddedInitStatus::Completed;
}
}
