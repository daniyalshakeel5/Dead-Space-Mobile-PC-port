#include "ds_port/original_stage_zero_allocating.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do {if(!(x)){std::cerr<<"Original ARM stage-zero allocator integration mismatch "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)

namespace {
std::array<std::uint8_t,0x70> context{};
std::array<std::uint8_t,8> owner{};
std::array<std::uint8_t,20> vtable{};
std::array<std::uint8_t,40> record{};
std::array<std::uint8_t,8> global{};
std::array<std::uint8_t,4> floatbits{{0,0,0x40,0x3f}};
unsigned clock_calls=0, allocator_calls=0;
unsigned stage=0;
bool return_null=false, reject_allocator=false, reject_clock=false;
OriginalAllocatorCall observed{};
std::uint32_t word(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
        (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
void put(std::uint8_t* p,std::uint32_t v) {
    for(int i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(8*i));
}
bool clock_elf_gettimeofday(std::int32_t* sec,std::int32_t* usec) noexcept {
    ++clock_calls;
    if(stage!=0 || reject_clock)return false;
    stage=1;*sec=5;*usec=250000;
    return true;
}
bool invoke_original_allocator(const OriginalAllocatorCall& args,void*,
                               OriginalAllocatorReturn* out) noexcept {
    ++allocator_calls;observed=args;
    if(stage==1 || (stage==0 && word(context.data()+0x40)!=0))stage=2;
    else stage=99;
    // The original type-2 call must run before clock/allocator.
    if(word(context.data()+0x50)!=2u ||
       word(context.data()+0x54)!=0x3f400000u)stage=99;
    if(reject_allocator)return false;
    if(return_null) {
        *out={0,nullptr,0};
    } else {
        *out={0x23334440u,record.data(),0x20};
    }
    // Test original ARM's second LDR [context+4] after first call:
    // allocator may modify the context, which must be observed.
    put(context.data()+4,0x87654321u);
    return true;
}
OriginalStageZeroAllocatingInputs inputs() {
    return {context.data(),context.size(),floatbits.data(),floatbits.size(),
            clock_elf_gettimeofday,0x11223344u,owner.data(),owner.size(),
            0x10203040u,vtable.data(),vtable.size(),
            global.data(),global.size(),invoke_original_allocator,nullptr};
}
void reset() {
    context.fill(0x6d);owner.fill(0xaa);vtable.fill(0xbb);
    record.fill(0xdc);global.fill(0xee);
    for(std::size_t i=0x40;i<0x48;++i)context[i]=0;
    put(context.data()+4,0x11223344u);
    put(owner.data(),0x10203040u);
    put(vtable.data()+12,0x00abcdefu);
    clock_calls=allocator_calls=stage=0;
    return_null=reject_allocator=reject_clock=false;
}
}

int main() {
    reset();
    auto args=inputs();
    args.invoke_original_allocator=nullptr;
    CHECK(original_execute_stage_zero_allocating_prefix(args)
        ==OriginalStageZeroAllocatingResult::MissingOriginalAllocator);
    CHECK(word(context.data()+0x50)==0x6d6d6d6du);
    CHECK(allocator_calls==0);
    args=inputs();
    args.original_owner_arm_address=0;
    CHECK(original_execute_stage_zero_allocating_prefix(args)
        ==OriginalStageZeroAllocatingResult::InvalidOriginalMemory);
    CHECK(word(context.data()+0x50)==0x6d6d6d6du);
    args=inputs();
    CHECK(original_execute_stage_zero_allocating_prefix(args)
        ==OriginalStageZeroAllocatingResult::OriginalAllocatedRecordAndFollowupExecuted);
    CHECK(clock_calls==1 && allocator_calls==1 && stage==2);
    CHECK(observed.original_method_arm_address==0x00abcdefu);
    CHECK(observed.original_owner_arm_address==0x11223344u);
    CHECK(observed.requested_size==32u);
    CHECK(observed.original_type_descriptor_arm_address==0x0042fc70u);
    CHECK(observed.flag==1u);
    CHECK(observed.stack_arg0==4u && observed.stack_arg1==0u);
    CHECK(word(context.data()+0x40)==5250000u);
    CHECK(word(context.data()+0x44)==0u);
    CHECK(word(record.data()+0x18)==0x11223344u);
    CHECK(word(record.data()+0x04)==0x23334444u);
    CHECK(word(global.data())==0x23334440u);
    CHECK(word(context.data()+0x34)==0x87654321u);
    CHECK(word(context.data()+0x38)==0u);
    CHECK(word(context.data()+0x3c)==0x87654321u);
    CHECK(word(context.data()+4)==0x87654321u);

    reset();return_null=true;
    // Already-seeded clock skips gettimeofday while the virtual
    // allocation still executes exactly once.
    put(context.data()+0x40,1);
    CHECK(original_execute_stage_zero_allocating_prefix(inputs())
        ==OriginalStageZeroAllocatingResult::OriginalNullAllocationAndFollowupExecuted);
    CHECK(clock_calls==0 && allocator_calls==1 && stage==2);
    CHECK(word(global.data())==0u);
    CHECK(record[0]==0xdc && record[0x18]==0xdc);
    CHECK(word(context.data()+0x34)==0x87654321u);

    reset();reject_allocator=true;
    CHECK(original_execute_stage_zero_allocating_prefix(inputs())
        ==OriginalStageZeroAllocatingResult::OriginalAllocatorUnavailableAfterClock);
    CHECK(clock_calls==1 && allocator_calls==1);
    CHECK(word(global.data())==0xeeeeeeeeu);
    CHECK(word(context.data()+0x34)==0x6d6d6d6du);

    reset();reject_clock=true;
    CHECK(original_execute_stage_zero_allocating_prefix(inputs())
        ==OriginalStageZeroAllocatingResult::OriginalClockSourceUnavailable);
    CHECK(clock_calls==1 && allocator_calls==0);
    CHECK(word(global.data())==0xeeeeeeeeu);
    CHECK(word(context.data()+0x50)==2u); // setup was performed first

    std::cout<<"Original ARM stage-zero verified allocator callsite integrated end-to-end\n";
    return 0;
}
