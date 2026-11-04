#include "ds_port/original_resource_resolver.hpp"
#include <cstring>
namespace ds_port::original_runtime {
OriginalResolverResult0x28a718 original_execute_resolver_0x28a718(
    const OriginalResolverInputs0x28a718& s,
    std::uint32_t owner,std::string_view name) noexcept {
    using Status=OriginalResolverStatus0x28a718;
    auto fail=[](Status status) noexcept {
        return OriginalResolverResult0x28a718{status,0};
    };
    if(!owner||!s.pool_alloc||!s.pool_init||!s.object_alloc||
       !s.object_init||!s.object_param||!s.copy_name||!s.lookup||
       !s.transform||!s.invoke_virtual||!s.release_temporary)
        return fail(Status::MissingOriginalService);
    // Original 0x28A744..0x28A76C:
    // r1=[handle+0x0C], r0=0x14, BL 0x276CB4;
    // BL 0x275680(pool,0x400,0);
    // memset(*(pool+8),0,0x400).
    std::uint32_t pool=0;
    if(!s.pool_alloc(0x14u,owner,s.context,&pool)||!pool)
        return fail(Status::OriginalPoolBlocked);
    void* buffer=nullptr;std::size_t accessible=0;
    if(!s.pool_init(pool,0x400u,0u,s.context,&buffer,&accessible))
        return fail(Status::OriginalPoolBlocked);
    if(!buffer||accessible<0x400)
        return fail(Status::InvalidOriginalBuffer);
    std::memset(buffer,0,0x400u);
    // Original 0x28A770..0x28A7A0, allocate and initialize a
    // distinct 0x30-byte object. Original float bits: 1.0f.
    std::uint32_t object=0;
    if(!s.object_alloc(0x30u,s.context,&object)||!object||
       !s.object_init(object,pool,0x400u,0u,s.context)||
       !s.object_param(object,1u,0x3f800000u,s.context))
        return fail(Status::OriginalObjectBlocked);
    // Original 0x27B204: copy input resource-name string into
    // temporary ARM stack object. 0x272DDC takes r1=1,r2=6,r3=1,
    // stack0=0. A 0 result BRANCHES TO CLEANUP, not a null return.
    if(!s.copy_name(name,s.context))
        return fail(Status::OriginalNameBlocked);
    std::uint32_t lookup=0;
    if(!s.lookup(1u,6u,1u,0u,s.context,&lookup))
        return fail(Status::OriginalLookupBlocked);
    if(lookup) {
        std::uint32_t produced=0,scratch_token=0;
        // ARM 0x28A7F0: 0x2716F8(name,scratch,0x2710).
        if(!s.transform(0x2710u,s.context,&produced,&scratch_token))
            return fail(Status::OriginalTransformBlocked);
        // ARM 0x28A810: vtable +0x3C (r1=scratch,r2=produced).
        // The callback maps the original stack scratch into a native
        // token, never an invented ARM pointer.
        if(!s.invoke_virtual(object,0x3cu,scratch_token,produced,s.context))
            return fail(Status::OriginalVirtualBlocked);
        // ARM 0x28A818: compare produced with 0x2710, and only
        // when unequal call vtable +0x2C (r1=0,r2=0).
        if(produced!=0x2710u &&
           !s.invoke_virtual(object,0x2cu,0u,0u,s.context))
            return fail(Status::OriginalVirtualBlocked);
    }
    // Original 0x28A838 BL 0x27744C then r0=r4 (the allocated
    // 0x30-byte object) and return. A zero lookup still returns r4.
    if(!s.release_temporary(s.context))
        return fail(Status::OriginalTemporaryCleanupBlocked);
    return {Status::Completed,object};
}
} // namespace ds_port::original_runtime
