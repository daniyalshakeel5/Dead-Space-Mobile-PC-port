#include "ds_port/original_secondary_pipeline.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
struct State {int allocations=0;int steps=0;std::uint32_t token=0x5000;};
void put(std::uint8_t* p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
bool alloc(const OriginalSecondaryAllocation& a,void* ctx,std::uint32_t* out) noexcept {
    auto& s=*static_cast<State*>(ctx);
    ++s.allocations;
    assert(a.descriptor_arm_address==0x0042ffb0u);
    assert(a.requested_bytes==0x24u&&a.stack0==4&&a.stack1==0);
    *out=s.token;return true;
}
bool step(std::uint32_t target,std::uint32_t address,std::uint32_t a1,
          std::uint32_t a2,void* ctx,std::uint32_t* out) noexcept {
    auto& s=*static_cast<State*>(ctx);
    const std::uint32_t targets[]={0x2828acu,0x27fb28u,0x27fa74u};
    assert(s.steps<3&&target==targets[s.steps]);
    assert(address==(s.steps==0?0x5000u:0x5014u));
    assert(a1==(s.steps==0?0u:s.steps==1?1u:2u));
    assert(a2==(s.steps==2?3u:0u));
    ++s.steps;*out=0x6000u;return true;
}
}
int main() {
    std::array<std::uint8_t,4> owner{};
    std::array<std::uint8_t,16> table{};
    std::array<std::uint8_t,0x24> object{};
    put(owner.data(),0x1000);put(table.data()+12,0x2000);
    State s;
    OriginalSecondaryPipeline inputs{0x3000,owner.data(),owner.size(),
        table.data(),table.size(),0x5000,object.data(),object.size(),
        0x7000,1,2,3,alloc,step,&s};
    auto r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::Completed);
    assert(r.postprocess_status==OriginalPostprocessStatus::Completed);
    assert(r.original_result_arm_address==0x5000&&s.allocations==1&&s.steps==3);
    assert(object[0]==0&&object[1]==0x70&&object[0x20]==0&&object[0x21]==0x60);
    s={};s.token=0x9999;object.fill(0);
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::PostprocessRefused);
    assert(r.postprocess_status==OriginalPostprocessStatus::InvalidMemory);
    assert(r.original_result_arm_address==0&&s.steps==0);
    s={};s.token=0;object.fill(0);
    r=original_run_secondary_pipeline(inputs);
    assert(r.allocation_status==OriginalSecondaryStatus::Completed);
    assert(r.original_result_arm_address==0&&s.allocations==1&&s.steps==0);
    for(auto byte:object)assert(byte==0);
    return 0;
}
