#include "ds_port/original_parsed_tree_registration.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstring>
using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,unsigned v) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(i*8));
}
struct Fixture {
    std::array<std::uint8_t,0xc0> parsed{};
    std::array<std::array<std::uint8_t,0x28>,3> nodes{};
    std::array<std::uint8_t,4> got{};
    std::array<std::uint8_t,kOriginalRegistrationCharMapMinBytes> chars{};
    OriginalNativeResourceMap resources{};
    bool deny_last=false;
};
bool node(std::uint32_t va,void* p,OriginalTreeNodeSpan* out) noexcept {
    auto& f=*static_cast<Fixture*>(p);
    if(va>=0x1000u&&va<0x1000u+f.parsed.size()) {
        const auto offset=va-0x1000u;
        *out={f.parsed.data()+offset,f.parsed.size()-offset};
        return true;
    }
    for(unsigned i=0;i<3;++i) {
        if(va==0x3000u+0x100u*i&&!(f.deny_last&&i==2)) {
            *out={f.nodes[i].data(),f.nodes[i].size()};
            return true;
        }
    }
    return false;
}
bool strings(std::uint32_t va,void*,OriginalRegistrationStringBytes* out) noexcept {
    static constexpr std::uint8_t a[]="ALPHA",b[]="BETA",c[]="GAMMA";
    static constexpr std::uint8_t d[]="D1",e[]="D2",f[]="D3";
    const std::uint8_t* ptr=nullptr;std::size_t len=0;
    switch(va) {
      case 0x5000:ptr=a;len=sizeof(a);break;
      case 0x5001:ptr=b;len=sizeof(b);break;
      case 0x5002:ptr=c;len=sizeof(c);break;
      case 0x6000:ptr=d;len=sizeof(d);break;
      case 0x6001:ptr=e;len=sizeof(e);break;
      case 0x6002:ptr=f;len=sizeof(f);break;
      default:return false;
    }
    *out={ptr,len};return true;
}
void setup(Fixture& f) {
    put(f.got.data(),0x7777);
    for(unsigned ch=0;ch<256;++ch) {
        unsigned folded=ch;
        if(ch>='A'&&ch<='Z')folded=ch+('a'-'A');
        f.chars[ch*2+2]=static_cast<std::uint8_t>(folded);
    }
    const unsigned offsets[]={0x28,0x50,0x78};
    for(unsigned i=0;i<3;++i) {
        // BL 0x283408 returns container+4; LDR [r0+8] = +0xC;
        // end marker is +0x8 from container.
        const unsigned sentinel=0x1000u+offsets[i]+8u;
        put(f.parsed.data()+offsets[i]+12,0x3000u+0x100u*i);
        put(f.nodes[i].data()+8,sentinel);
        put(f.nodes[i].data()+0x10,0x5000u+i);
        put(f.nodes[i].data()+0x24,0x6000u+i);
    }
}
}
int main() {
    Fixture fixture{};
    setup(fixture);
    OriginalNativeRegistrationServices registration{};
    registration.resolve_string=strings;
    registration.character_table={fixture.got.data(),fixture.got.size(),
        0x7777u,fixture.chars.data(),fixture.chars.size()};
    registration.context=&fixture;
    registration.native_resource_map=&fixture.resources;
    OriginalParsedTreeRegistrationInputs in{
        0x1000u,fixture.parsed.data(),fixture.parsed.size(),
        0x2018u,node,&fixture,&registration
    };
    auto result=original_register_parsed_trees_0x286918(in);
    assert(result.status==OriginalParsedTreeStatus::Completed);
    assert(result.registered_nodes==3&&result.completed_passes==3);
    assert(fixture.resources.count()==3);
    OriginalNativeResourceEntry e{};
    assert(fixture.resources.find("alpha",&e)&&e.descriptor=="D1");
    assert(fixture.resources.find("beta",&e)&&e.descriptor=="D2");
    assert(fixture.resources.find("gamma",&e)&&e.descriptor=="D3");
    // All native loader registrations must reject an unreadable node.
    fixture.deny_last=true;
    result=original_register_parsed_trees_0x286918(in);
    assert(result.status==OriginalParsedTreeStatus::UnmappedOriginalNode);
    assert(result.registered_nodes==2&&result.completed_passes==2);
    // Original last root word is parsed+0x84: require all four bytes.
    fixture.deny_last=false;
    in.parsed_accessible_bytes=0x84;
    result=original_register_parsed_trees_0x286918(in);
    assert(result.status==OriginalParsedTreeStatus::InvalidOriginalState);
    return 0;
}
