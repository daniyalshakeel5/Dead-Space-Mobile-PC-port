#include "ds_port/original_resource_handle.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v){
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(i*8));
}
struct State {int calls=0;bool reject=false;};
bool invoke(const OriginalResourceHandleVirtualCall& call,void* ctx,
            std::uint32_t* result) noexcept {
    auto& s=*static_cast<State*>(ctx);
    ++s.calls;
    assert(call.original_provider_va==0x2000u);
    assert(call.original_method_va==0x3100u);
    assert(call.r1==1&&call.r2==6&&call.r3==1&&call.stack0==0);
    if(s.reject)return false;
    *result=0x8765u;
    return true;
}
}
int main(){
    std::array<std::uint8_t,12> handle{};
    std::array<std::uint8_t,4> provider{};
    std::array<std::uint8_t,0x54> table{};
    State state{};
    put(provider.data(),0x3000u);
    put(table.data()+0x50,0x3100u);
    OriginalResourceHandleInput in{handle.data(),handle.size(),
        0x2000u,provider.data(),provider.size(),
        0x3000u,table.data(),table.size()};
    put(handle.data()+4,1);
    put(handle.data()+8,0x2000);
    auto r=original_resource_ready_0x28a5f4(in,invoke,&state);
    assert(r.status==OriginalResourceHandleStatus::Completed&&r.original_r0==1);
    assert(state.calls==0);
    put(handle.data()+8,0);
    r=original_resource_ready_0x28a5f4(in,invoke,&state);
    assert(r.status==OriginalResourceHandleStatus::Completed&&r.original_r0==0);
    put(handle.data()+4,0);put(handle.data()+8,0x2000);
    r=original_resource_ready_0x28a5f4(in,invoke,&state);
    assert(r.status==OriginalResourceHandleStatus::Completed&&r.original_r0==0x8765);
    assert(state.calls==1);
    state.reject=true;
    assert(original_resource_ready_0x28a5f4(in,invoke,&state).status==
           OriginalResourceHandleStatus::OriginalVirtualRefused);
    state.reject=false;
    put(table.data()+0x50,0);
    assert(original_resource_ready_0x28a5f4(in,invoke,&state).status==
           OriginalResourceHandleStatus::MissingOriginalVirtualMethod);
    put(table.data()+0x50,0x3100);
    in.provider_vtable_bytes=0x50;
    assert(original_resource_ready_0x28a5f4(in,invoke,&state).status==
           OriginalResourceHandleStatus::UnmappedOriginalProvider);
    in.provider_vtable_bytes=0x54;in.handle_bytes=8;
    assert(original_resource_ready_0x28a5f4(in,invoke,&state).status==
           OriginalResourceHandleStatus::InvalidOriginalMemory);
    return 0;
}
