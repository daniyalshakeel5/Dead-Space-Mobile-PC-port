#include "ds_port/original_runtime_globals.hpp"
#include "ds_port/original_stage_zero_setup.hpp"
#include "ds_port/original_record_postalloc.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do {if(!(x)){std::cerr<<"Original global span mismatch "<<__LINE__<<" :"<<#x<<"\n";return 1;}}while(0)
namespace {
std::array<std::uint8_t,0x290> shared{};
std::array<std::uint8_t,0x60> context{};
std::array<std::uint8_t,0x28> record{};
unsigned first_calls=0;
void native_first_followup_test_only(std::uint32_t token) noexcept {
    ++first_calls;
    if(token==0x34567890u) {
        context[4]=0x40;context[5]=0x30;
        context[6]=0x20;context[7]=0x10;
    }
}
std::uint32_t read32(const std::uint8_t* v) {
    return std::uint32_t(v[0]) | (std::uint32_t(v[1])<<8) |
        (std::uint32_t(v[2])<<16) | (std::uint32_t(v[3])<<24);
}
}
int main(){
    static_assert(kOriginalRecordGlobalVA==0x0049cb94u);
    static_assert(kOriginalType2FloatVA==0x0049cd1cu);
    static_assert(kOriginalKnownGlobalSpan==0x278);
    shared.fill(0xa7);context.fill(0);record.fill(0xdd);
    void* record_slot=reinterpret_cast<void*>(0x1);
    const void* float_field=reinterpret_cast<void*>(0x2);
    CHECK(!original_bind_known_global_fields(
        nullptr,shared.size(),&record_slot,&float_field));
    CHECK(!original_bind_known_global_fields(
        shared.data(),0x277,&record_slot,&float_field));
    CHECK(record_slot==reinterpret_cast<void*>(0x1));
    CHECK(float_field==reinterpret_cast<void*>(0x2));
    CHECK(original_bind_known_global_fields(
        shared.data(),shared.size(),&record_slot,&float_field));
    CHECK(record_slot==shared.data()+0xec);
    CHECK(float_field==shared.data()+0x274);
    // Runtime float data must come from actual mapped shared global bytes.
    shared[0x274]=0x00;shared[0x275]=0x00;
    shared[0x276]=0x20;shared[0x277]=0x3f; // bit-exact 0.625f
    context[4]=0x90;context[5]=0x78;context[6]=0x56;context[7]=0x34;
    // Original time slot already nonzero, so clock source is never called.
    context[0x40]=1;
    CHECK(original_execute_stage_zero_setup_with_recovered_type_two(
        context.data(),context.size(),float_field,4,
        nullptr,native_first_followup_test_only)
        ==OriginalStageZeroSetupResult::VerifiedPrefixExecuted);
    CHECK(first_calls==1);
    CHECK(read32(context.data()+0x50)==2);
    CHECK(read32(context.data()+0x54)==0x3f200000u);
    CHECK(read32(context.data()+0x34)==0x10203040u);
    CHECK(read32(context.data()+0x38)==0u);
    CHECK(read32(context.data()+0x3c)==0x10203040u);
    // Null allocator result clears the actual verified ELF global slot.
    CHECK(original_complete_allocator_postreturn_0x281d54(
        nullptr,0,0x34567890u,0,record_slot,4)
        ==OriginalPostAllocateResult::OriginalNullResultPublished);
    CHECK(read32(static_cast<const std::uint8_t*>(record_slot))==0u);
    CHECK(original_complete_allocator_postreturn_0x281d54(
        record.data(),record.size(),0x34567890u,
        0x10203040u,record_slot,4)
        ==OriginalPostAllocateResult::OriginalRecordInitializedAndPublished);
    CHECK(read32(static_cast<const std::uint8_t*>(record_slot))==0x10203040u);
    CHECK(read32(record.data()+0x18)==0x34567890u);
    CHECK(read32(record.data()+0x04)==0x10203044u);
    CHECK(read32(record.data()+0x08)==0x10203044u);
    // Record publication cannot clobber the distinct original float.
    CHECK(read32(static_cast<const std::uint8_t*>(float_field))==0x3f200000u);
    // The original service registry slot precedes the frame state by
    // eight bytes, but its RUNTIME value is unknown and must not be
    // guessed from the uninitialized ELF BSS.
    std::array<std::uint8_t,0x290> registry_based{};
    registry_based.fill(0xe1);
    void* registry=reinterpret_cast<void*>(0x1);
    void* frame=reinterpret_cast<void*>(0x2);
    void* record_slot_full=reinterpret_cast<void*>(0x3);
    const void* float_full=reinterpret_cast<void*>(0x4);
    CHECK(!original_bind_registry_and_runtime_globals(
        registry_based.data(),0x27f,
        &registry,&frame,&record_slot_full,&float_full));
    CHECK(registry==reinterpret_cast<void*>(0x1));
    CHECK(frame==reinterpret_cast<void*>(0x2));
    CHECK(record_slot_full==reinterpret_cast<void*>(0x3));
    CHECK(float_full==reinterpret_cast<void*>(0x4));
    CHECK(original_bind_registry_and_runtime_globals(
        registry_based.data(),registry_based.size(),
        &registry,&frame,&record_slot_full,&float_full));
    CHECK(registry==registry_based.data());
    CHECK(frame==registry_based.data()+0x08);
    CHECK(record_slot_full==registry_based.data()+0xf4);
    CHECK(float_full==registry_based.data()+0x27c);
    CHECK(read32(static_cast<const std::uint8_t*>(registry))==0xe1e1e1e1u);
    // No model-created service registry is written or initialized.
    CHECK(original_complete_allocator_postreturn_0x281d54(
        nullptr,0,0,0,record_slot_full,4)
        ==OriginalPostAllocateResult::OriginalNullResultPublished);
    CHECK(read32(static_cast<const std::uint8_t*>(registry))==0xe1e1e1e1u);
    CHECK(read32(static_cast<const std::uint8_t*>(record_slot_full))==0u);
    CHECK(read32(static_cast<const std::uint8_t*>(float_full))==0xe1e1e1e1u);
    std::cout<<"Three original ELF global addresses mapped to bounded native state\n";
    return 0;
}
