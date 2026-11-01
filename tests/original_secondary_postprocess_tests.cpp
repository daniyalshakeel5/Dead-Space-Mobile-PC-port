#include "ds_port/original_secondary_postprocess.hpp"
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
namespace {
struct Trace { int count=0; int fail_at=0; };
bool call(std::uint32_t target,std::uint32_t arg,std::uint32_t arg1,std::uint32_t arg2,void* c,std::uint32_t* result) noexcept {
    auto& t=*static_cast<Trace*>(c);
    const std::uint32_t targets[]={0x2828acu,0x27fb28u,0x27fa74u};
    assert(t.count<3);assert(target==targets[t.count]);
    assert(arg==(t.count==0?0x1000u:0x1014u));
    assert(arg1==(t.count==0?0u:t.count==1?1u:2u));
    assert(arg2==(t.count==2?3u:0u));
    ++t.count;
    if(t.fail_at==t.count)return false;
    *result=0xaabbccddu;return true;
}
std::uint32_t read32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
        (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
int main(){
    std::array<std::uint8_t,0x24> obj{};
    Trace trace{};
    OriginalPostprocessInputs x{0x1000,obj.data(),obj.size(),
        0x2000,1,2,3,call,&trace};
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::Completed);
    assert(trace.count==3&&read32(obj.data())==0x2000u);
    assert(read32(obj.data()+0x20)==0xaabbccddu);
    trace={};obj.fill(0);trace.fail_at=1;
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::CallRefused);
    assert(read32(obj.data())==0);
    trace={};obj.fill(0);trace.fail_at=2;
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::CallRefused);
    assert(read32(obj.data())==0x2000u&&read32(obj.data()+0x20)==0);
    trace={};obj.fill(0);trace.fail_at=3;
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::CallRefused);
    assert(read32(obj.data()+0x20)==0);
    x.call_original=nullptr;
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::MissingOriginalCallees);
    x.call_original=call;x.accessible_object_bytes=0x23;
    assert(original_postprocess_0x288688_prefix(x)==OriginalPostprocessStatus::InvalidMemory);
    return 0;
}
