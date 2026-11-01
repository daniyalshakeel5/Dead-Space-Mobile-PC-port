#include "ds_port/original_engine_startup.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
std::uint32_t read(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct State {
    std::array<std::uint8_t,0x80> frame{};
    std::array<std::uint8_t,4> registry_slot{},registry_object{},first_obj{};
    std::array<std::uint8_t,0x1c> registry_vtable{};
    std::array<std::uint8_t,0x23c> first_table{};
    std::array<std::uint8_t,4> second_owner{};
    std::array<std::uint8_t,0x10> second_vtable{};
    std::array<std::uint8_t,0x20> record{};
    std::array<std::uint8_t,4> published{},float_source{};
    int first_virtual=0,lookup_count=0,second_allocations=0;
    bool fail_first=false,fail_second=false;
};
State* active=nullptr;
bool clock(std::int32_t* sec,std::int32_t* us) noexcept {
    *sec=5;*us=250000;return true;
}
bool query(const OriginalRegistryQuery& q,void* ctx,std::uint32_t* v) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.lookup_count;
    assert(q.interface_id==0x10004 && q.original_method_arm_address==0x4300);
    *v=0x5000;return true;
}
bool resolve(std::uint32_t a,void* ctx,OriginalStartupServiceMemory* r) noexcept {
    auto& s=*static_cast<State*>(ctx);
    assert(a==0x5000);
    *r={a,s.first_obj.data(),s.first_obj.size(),0x5100,
        s.first_table.data(),s.first_table.size()};
    return true;
}
bool virtual_call(const OriginalStartupVirtualCall& c,void* ctx,
                  OriginalStartupVirtualResult* out) noexcept {
    auto& s=*static_cast<State*>(ctx);
    const unsigned slots[]={0x18,0x1c4,0x18,0x1c4,0x84,0x8c,0x238};
    const unsigned vals[]={0x8000,0x8100,0x8200,0x8300,0x8400,0x8500,0x8600};
    assert(s.first_virtual<7&&c.vtable_slot==slots[s.first_virtual]);
    if(s.fail_first)return false;
    const auto step=s.first_virtual++;
    *out={vals[step],step==4?0x5500u:0u};
    return true;
}
bool direct(std::uint32_t a,std::uint32_t b,std::uint32_t c,
            void*,std::uint32_t* out) noexcept {
    assert(a==0x5000&&b==0x8200&&c==0x8300);
    *out=0x9000;return true;
}
bool allocate(const OriginalAllocatorCall& a,void* ctx,
              OriginalAllocatorReturn* out) noexcept {
    auto& s=*static_cast<State*>(ctx);++s.second_allocations;
    assert(a.requested_size==0x20&&a.original_type_descriptor_arm_address==0x42fc70);
    assert(read(s.frame.data()+8+0x50)==2);
    if(s.fail_second)return false;
    *out={0x9000,s.record.data(),s.record.size()};return true;
}
OriginalEngineStartupInputs make_inputs(State& s){
    OriginalStartupFirstInputs first{
        {s.registry_slot.data(),4,0x4000,
         s.registry_object.data(),4,0x4200,
         s.registry_vtable.data(),s.registry_vtable.size(),query},
        resolve,virtual_call,direct,&s
    };
    OriginalStageZeroAllocatingInputs second{
        s.frame.data()+8,s.frame.size()-8,
        s.float_source.data(),4,clock,
        0x7000,s.second_owner.data(),s.second_owner.size(),
        0x7100,s.second_vtable.data(),s.second_vtable.size(),
        s.published.data(),s.published.size(),allocate,&s
    };
    return {s.frame.data(),s.frame.size(),first,second};
}
void prepare(State& s){
    put(s.registry_slot.data(),0x4000);
    put(s.registry_object.data(),0x4200);
    put(s.registry_vtable.data()+0x18,0x4300);
    put(s.first_obj.data(),0x5100);
    for(auto offset:{0x18u,0x1c4u,0x84u,0x8cu,0x238u})
        put(s.first_table.data()+offset,0x6000+offset);
    put(s.frame.data()+8+4,0x7000);
    put(s.second_owner.data(),0x7100);
    put(s.second_vtable.data()+0xc,0x7200);
    put(s.float_source.data(),0x3f400000);
}
}
int main(){
    State s{};prepare(s);
    auto input=make_inputs(s);
    auto r=original_execute_known_engine_startup(input);
    assert(r.status==OriginalEngineStartupStatus::ReachedUnreconstructedOriginalTail);
    assert(r.first_status==OriginalStartupFirstStatus::CompletedOriginalFirstRoutine);
    assert(r.second_status==
           OriginalStageZeroAllocatingResult::OriginalAllocatedRecordAndFollowupExecuted);
    assert(s.lookup_count==2 && s.first_virtual==7 && s.second_allocations==1);
    // Engine not initialized: ARM writes stage 1 only AFTER 0x280520 returns.
    assert(read(s.frame.data())==0);
    assert(read(s.frame.data()+8+0x50)==2);
    assert(read(s.published.data())==0x9000);

    // A present but incomplete tail may NEVER advance the original stage.
    State tail_missing{};prepare(tail_missing);
    auto blocked_input=make_inputs(tail_missing);
    OriginalStartupSecondTailInputs missing_tail{};
    missing_tail.original_context=tail_missing.frame.data()+8;
    missing_tail.context_bytes=tail_missing.frame.size()-8;
    missing_tail.original_context_arm_va=0x0049cab0u;
    blocked_input.original_second_tail_0x280580=&missing_tail;
    const auto blocked=original_execute_known_engine_startup(blocked_input);
    assert(blocked.status==OriginalEngineStartupStatus::OriginalSecondTailBlocked);
    assert(blocked.tail_status==OriginalStartupSecondTailStatus::MissingOriginalDirect);
    assert(read(tail_missing.frame.data())==0);

    State fail{};prepare(fail);fail.fail_first=true;
    auto p=make_inputs(fail);
    auto f=original_execute_known_engine_startup(p);
    assert(f.status==OriginalEngineStartupStatus::FirstStartupRoutineBlocked);
    assert(fail.second_allocations==0&&read(fail.frame.data())==0);
    State fail2{};prepare(fail2);fail2.fail_second=true;
    p=make_inputs(fail2);
    f=original_execute_known_engine_startup(p);
    assert(f.status==OriginalEngineStartupStatus::SecondStartupRoutineBlocked);
    assert(fail2.second_allocations==1&&read(fail2.frame.data())==0);
    put(fail2.frame.data(),1);
    assert(original_execute_known_engine_startup(p).status==
           OriginalEngineStartupStatus::OriginalStageNotZero);
    return 0;
}
