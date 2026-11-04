#include "ds_port/original_secondary_allocator.hpp"
#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void store(std::uint8_t* p,std::uint32_t v){
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));
}
struct State{int alloc=0;int post=0;bool fail=false;std::uint32_t result=0x7000;};
bool allocate(const OriginalSecondaryAllocation& a,void* ctx,std::uint32_t* r) noexcept{
    auto& s=*static_cast<State*>(ctx);++s.alloc;
    assert(a.method_arm_address==0x1234&&a.owner_arm_address==0x2000);
    assert(a.requested_bytes==36&&a.descriptor_arm_address==0x3456);
    assert(a.flags==1&&a.stack0==4&&a.stack1==0);
    if(s.fail)return false;
    *r=s.result;return true;
}
bool post(std::uint32_t v,void* ctx) noexcept{
    auto& s=*static_cast<State*>(ctx);++s.post;assert(v==0x7000);return !s.fail;
}
}
int main(){
    std::array<std::uint8_t,4> owner{};
    std::array<std::uint8_t,16> vtable{};
    store(owner.data(),0x4000);store(vtable.data()+12,0x1234);
    State s;std::uint32_t out=0xdead;
    auto run=[&](OriginalSecondaryDispatch a,OriginalSecondaryPostprocess p){
        return original_allocate_0x2849e0(0x2000,owner.data(),4,
            vtable.data(),16,0x3456,a,p,&s,&out);
    };
    assert(run(allocate,post)==OriginalSecondaryStatus::Completed);
    assert(out==0x7000&&s.alloc==1&&s.post==1);
    out=0xdead;s.result=0;
    assert(run(allocate,nullptr)==OriginalSecondaryStatus::Completed);
    assert(out==0&&s.post==1);
    out=0xdead;s.result=0x7000;
    assert(run(allocate,nullptr)==OriginalSecondaryStatus::MissingPostprocess);
    assert(out==0xdead);
    s.fail=true;
    assert(run(allocate,post)==OriginalSecondaryStatus::AllocatorRefused);
    s.fail=false;
    assert(run(nullptr,post)==OriginalSecondaryStatus::MissingAllocator);
    store(vtable.data()+12,0);
    assert(run(allocate,post)==OriginalSecondaryStatus::InvalidVtable);
    return 0;
}
