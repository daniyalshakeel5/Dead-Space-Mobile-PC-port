#include "ds_port/original_receiver_mode.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
using namespace ds_port::original_runtime;
namespace {
struct Fixture {
    std::array<std::uint8_t,0x8bc> owner{};
    unsigned calls=0;
    bool refuse=false;
};
std::uint32_t get32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool reset(std::uint32_t original_va,void* raw) noexcept {
    auto& fixture=*static_cast<Fixture*>(raw);
    ++fixture.calls;
    assert(original_va==0x2000u);
    // Original ARM 0x26C43C stores mode zero before invoking
    // original vtable+0x28 at 0x26C450.
    assert(fixture.owner[0x8b5u]==0u);
    return !fixture.refuse;
}
}
int main() {
    using S=OriginalReceiverModeStatus0x26c410;
    Fixture f{};
    OriginalReceiverModeInputs0x26c410 in{
        0x2000u,f.owner.data(),f.owner.size(),nullptr,&f};
    auto out=original_set_receiver_mode_0x26c410(in,1u,42u);
    assert(out.status==S::Completed&&out.previous_value==0u);
    assert(out.current_value==1u&&f.owner[0x8b5u]==1u);
    out=original_set_receiver_mode_0x26c410(in,1u,123u);
    assert(out.status==S::Completed&&f.owner[0x8b5u]==1u);
    assert(f.calls==0u);
    // Opcode 2 stores raw 32-bit parser field at +0x8B8.
    out=original_set_receiver_mode_0x26c410(in,2u,0x87654321u);
    assert(out.status==S::Completed&&out.current_value==0x87654321u);
    assert(get32(f.owner.data()+0x8b8u)==0x87654321u);
    // Unrecognized opcode must not alter the receiver.
    out=original_set_receiver_mode_0x26c410(in,3u,0xffffffffu);
    assert(out.status==S::Completed);
    assert(f.owner[0x8b5u]==1u&&
           get32(f.owner.data()+0x8b8u)==0x87654321u);
    // An original mode transition from 1 to zero requires
    // the original receiver virtual+0x28 reset to be reconstructed.
    out=original_set_receiver_mode_0x26c410(in,1u,0u);
    assert(out.status==S::MissingOriginalDisableReset);
    assert(f.owner[0x8b5u]==1u);
    in.native_reset_0x273a0c=reset;
    f.refuse=true;
    out=original_set_receiver_mode_0x26c410(in,1u,0u);
    assert(out.status==S::OriginalDisableResetRefused);
    assert(f.calls==1&&f.owner[0x8b5u]==1u);
    f.refuse=false;
    out=original_set_receiver_mode_0x26c410(in,1u,0u);
    assert(out.status==S::Completed&&f.owner[0x8b5u]==0u);
    assert(f.calls==2);
    // Repeated zero mode is a no-op and never resets again.
    out=original_set_receiver_mode_0x26c410(in,1u,0u);
    assert(out.status==S::Completed&&f.calls==2);
    // Malformed mapped owner must not read or write +0x8B8.
    in.owner_bytes=0x8bbu;
    out=original_set_receiver_mode_0x26c410(in,2u,0u);
    assert(out.status==S::InvalidOriginalOwner);
    assert(get32(f.owner.data()+0x8b8u)==0x87654321u);
    return 0;
}
