#include "ds_port/original_resource_registration.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
struct Test {
    std::array<std::array<std::uint8_t,4>,27> global_slots{};
    std::array<std::array<std::uint8_t,4>,27> global_words{};
    std::array<std::uint8_t,4> character_map_slot{};
    std::array<std::uint8_t,kOriginalRegistrationCharMapMinBytes> character_map{};
    int lookups=0,prechecks=0,matched=0,inserts=0;
    int fail_insert_at=-1;
    bool refuse_match=false;
    bool bad_string=false;
    bool bad_map=false;
    int next_step=0;
};
constexpr std::uint8_t kSource[]="ENERGY";
constexpr std::uint8_t kDescriptor[]="BEAM";
bool resolve_strings(std::uint32_t va,void* c,
                     OriginalRegistrationStringBytes* r) noexcept {
    auto& t=*static_cast<Test*>(c);
    if(t.bad_string)return false;
    if(va>=0x7000&&va<0x7000+27){
        *r={kSource,sizeof(kSource)};return true;
    }
    for(const auto& row:kOriginalRegistrationSequence0x27fd7c)
        if(va==row.descriptor_arm_va){
            *r={kDescriptor,sizeof(kDescriptor)};return true;
        }
    return false;
}
bool precheck(const OriginalRegistrationCall& call,
              void* ctx,std::uint32_t* result) noexcept {
    auto& t=*static_cast<Test*>(ctx);
    const int step=t.prechecks++;
    assert(call.original_global_value==0x7000u+unsigned(step));
    assert(call.original_subcontext_va==0x1018u);
    assert(t.next_step==0);
    *result=(step%2)==1?1u:0u;
    t.next_step=*result?1:2;
    return true;
}
bool matched(const OriginalRegistrationCall& call,void* ctx) noexcept {
    auto& t=*static_cast<Test*>(ctx);
    ++t.matched;
    assert(call.original_subcontext_va==0x1018u);
    assert(t.next_step==1);t.next_step=2;
    return !t.refuse_match;
}
bool insert_pair(const OriginalRegistrationCall& call,
                 std::uint32_t container,std::string_view converted,
                 std::string_view descriptor,void* ctx) noexcept {
    auto& t=*static_cast<Test*>(ctx);
    const int i=t.inserts++;
    assert(call.original_callsite_va==
           kOriginalRegistrationSequence0x27fd7c[unsigned(i)].callsite_arm_va);
    assert(container==0x101cu);
    assert(converted=="energy");
    assert(descriptor=="BEAM"); // second string NOT lowercased in ARM.
    assert(t.next_step==2);t.next_step=0;
    return i!=t.fail_insert_at;
}
bool global_resolve(std::uint32_t slot,void* ctx,
                    OriginalRegistrationMappedGlobal* out) noexcept {
    auto& t=*static_cast<Test*>(ctx);
    assert(t.lookups<27);
    const auto i=unsigned(t.lookups++);
    assert(slot==kOriginalRegistrationSequence0x27fd7c[i].global_slot_arm_va);
    *out={0x9000u+4*i,t.global_slots[i].data(),4,
          t.global_words[i].data(),4};
    return true;
}
OriginalNativeRegistrationServices services(Test& t){
    OriginalNativeRegistrationServices s{};
    s.resolve_string=resolve_strings;
    s.character_table={t.character_map_slot.data(),
        t.character_map_slot.size(),0x8888u,
        t.character_map.data(),t.character_map.size()};
    s.original_0x2839c8=precheck;
    s.original_0x283f5c=matched;
    s.original_0x2837b4=insert_pair;
    s.context=&t;
    return s;
}
}
int main(){
    static_assert(kOriginalRegistrationCharacterGotVA==0x004909f4u);
    Test t{};
    put(t.character_map_slot.data(),0x8888u);
    for(std::size_t i=0;i<256;++i){
        auto value=std::uint8_t(i);
        if(value>='A'&&value<='Z')value=std::uint8_t(value+('a'-'A'));
        t.character_map[i*2+2]=value;
    }
    for(unsigned i=0;i<27;++i){
        put(t.global_slots[i].data(),0x9000u+i*4u);
        put(t.global_words[i].data(),0x7000u+i);
    }
    auto svc=services(t);
    OriginalRegistrationInputs in{0x1000u,global_resolve,nullptr,&t,&svc};
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::CompletedOriginalSequence);
    assert(t.lookups==27&&t.prechecks==27&&t.inserts==27);
    assert(t.matched==13);
    assert(t.next_step==0);

    // Original 0x2839C8 nonzero path must execute 0x283F5C
    // before any original pair insertion.
    t.lookups=t.prechecks=t.inserts=t.matched=t.next_step=0;
    t.refuse_match=true;
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::OriginalRegistrationRefused);
    assert(t.prechecks==2&&t.matched==1&&t.inserts==1);
    t.refuse_match=false;

    t.lookups=t.prechecks=t.inserts=t.matched=t.next_step=0;
    t.fail_insert_at=2;
    assert(original_register_stage_resources_0x27fd7c(in)==
           OriginalRegistrationStatus::OriginalRegistrationRefused);
    assert(t.inserts==3&&t.prechecks==3);
    t.fail_insert_at=-1;

    // Resource strings and game's actual character-conversion mapping
    // must both be present; never quietly use ASCII fallback.
    t.next_step=0;
    t.bad_string=true;
    OriginalRegistrationCall one{kOriginalStartupRegistrationHelperVA,
        kOriginalRegistrationSequence0x27fd7c[0].callsite_arm_va,
        0x1018u,0x7000u,kOriginalRegistrationSequence0x27fd7c[0].descriptor_arm_va,
        false};
    assert(original_execute_resource_registration_0x284178(one,svc)==
           OriginalNativeRegistrationStatus::UnmappedOriginalString);
    t.bad_string=false;
    t.character_map_slot[0]=0;
    assert(original_execute_resource_registration_0x284178(one,svc)==
           OriginalNativeRegistrationStatus::InvalidOriginalCharacterTable);
    put(t.character_map_slot.data(),0x8888u);
    svc.original_0x2837b4=nullptr;
    assert(original_execute_resource_registration_0x284178(one,svc)==
           OriginalNativeRegistrationStatus::MissingOriginalService);
    return 0;
}
