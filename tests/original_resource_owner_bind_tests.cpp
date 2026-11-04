#include "ds_port/original_resource_owner_bind.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>

using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::uint32_t get(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Fixture {
    std::array<std::uint8_t,0x8d4> owner{};
    std::uint32_t seen_tree=0,seen_root=0;
    unsigned releases=0;
    bool refuse=false;
};
bool release(std::uint32_t tree,std::uint32_t root,void* opaque) noexcept {
    auto& f=*static_cast<Fixture*>(opaque);
    f.seen_tree=tree;f.seen_root=root;++f.releases;
    return !f.refuse;
}
}
int main() {
    using S=OriginalOwnerBindStatus0x278778;
    using A=OriginalReceiverActivationStatus0x278e58;
    Fixture f{};
    OriginalOwnerBindInputs0x278778 in{0x2000,f.owner.data(),
        f.owner.size(),nullptr,&f};
    // Original constructor leaves owner+0x8B6=0.
    f.owner[0x8b6]=0;
    f.owner[0x8cc]=1;
    put(f.owner.data()+0x8d0,0x1234);
    auto r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::ReachedOriginalNullProviderExit);
    assert(r.activation_status==A::CompletedEmptyProviderReset);
    assert(r.original_provider_va==0);
    assert(get(f.owner.data()+0x8c0)==0x28c0);
    assert(get(f.owner.data()+0x8c4)==0x28c0);
    assert(get(f.owner.data()+0x8c8)==0);
    assert(f.owner[0x8b6]==0&&f.owner[0x8cc]==0);
    assert(get(f.owner.data()+0x8d0)==0);

    // Original active branch contains provider-backed allocations,
    // resource copying, and callback invocation; it is never
    // reported as loaded before those are reconstructed.
    f.owner[0x8b6]=1;
    put(f.owner.data()+0x8d0,0x3333);
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::UnreconstructedActiveTreeBinding);
    assert(f.owner[0x8b6]==1&&get(f.owner.data()+0x8d0)==0x3333);

    // The previous tree requires an actual native cleanup service
    // before its mapped pointers may be overwritten.
    f.owner[0x8b6]=0;
    put(f.owner.data()+0x8c8,0x4000);
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::OriginalTreeActivationBlocked);
    assert(r.activation_status==A::MissingOriginalTreeDestructor);
    assert(get(f.owner.data()+0x8c8)==0x4000);
    f.refuse=true;
    in.release_original_tree=release;
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::OriginalTreeActivationBlocked);
    assert(r.activation_status==A::OriginalTreeReleaseRefused);
    assert(f.seen_tree==0x28bc&&f.seen_root==0x4000);
    assert(get(f.owner.data()+0x8c8)==0x4000);
    f.refuse=false;
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::ReachedOriginalNullProviderExit);
    assert(f.releases==2&&get(f.owner.data()+0x8c8)==0);
    // An original non-null provider enters vtable+0x14, +0x24,
    // +0x70 and resource-stream logic not yet reconstructed.
    put(f.owner.data()+0x8ac,0x7100);
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::UnreconstructedProviderBinding);
    assert(r.activation_status==A::UnreconstructedProviderActivation);
    assert(r.original_provider_va==0x7100);
    // Corrupt original mapping fails before touching the owner.
    in.owner_bytes=0x8d3;
    r=original_bind_resource_owner_0x278778(in);
    assert(r.status==S::InvalidOriginalOwner);
    assert(get(f.owner.data()+0x8ac)==0x7100);
    return 0;
}
