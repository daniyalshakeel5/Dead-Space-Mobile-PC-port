#include "ds_port/original_resource_token_lookup.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
using namespace ds_port::original_runtime;
namespace {
void word(std::uint8_t* p,unsigned x) {
    for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(x>>(i*8));
}
}
int main(){
    using S=OriginalTokenLookupStatus;
    OriginalNativeResourceTokenIndex index{};
    assert(index.insert_stored("alpha"));
    assert(index.insert_stored("alph"));
    assert(index.insert_stored("alpha"));
    assert(index.count()==2);
    assert(index.contains("alpha")&&index.contains("alph"));
    assert(!index.contains("ALPHA")&&!index.contains("alp"));
    // Game character table is bytewise and not necessarily ASCII.
    std::array<std::uint8_t,4> got{};
    std::array<std::uint8_t,kOriginalRegistrationCharMapMinBytes> values{};
    word(got.data(),0x7799);
    for(unsigned n=0;n<256;++n) {
        unsigned mapped=n;
        if(n>='A'&&n<='Z')mapped=n+('a'-'A');
        values[n*2u+2u]=static_cast<std::uint8_t>(mapped);
    }
    const OriginalRegistrationCharacterTable table{
        got.data(),got.size(),0x7799,values.data(),values.size()
    };
    const std::uint8_t upper[]={'A','L','P','H','A',0};
    const std::uint8_t prefix[]={'A','L','P','H',0};
    const std::uint8_t absent[]={'A','L','P',0};
    auto a=original_token_lookup_0x285630(&index,table,upper,sizeof(upper));
    assert(a.status==S::Found&&a.name_bytes==5);
    a=original_token_lookup_0x285630(&index,table,prefix,sizeof(prefix));
    assert(a.status==S::Found&&a.name_bytes==4);
    a=original_token_lookup_0x285630(&index,table,absent,sizeof(absent));
    assert(a.status==S::NotFound&&a.name_bytes==3);
    // Original LDRB + short lookup permits non-ASCII bytes.
    values[0xc1*2u+2u]=0xe1;
    const char mapped_high[]={char(0xe1),'q'};
    assert(index.insert_stored(std::string_view(mapped_high,2)));
    const std::uint8_t high[]={0xc1,'Q',0};
    a=original_token_lookup_0x285630(&index,table,high,sizeof(high));
    assert(a.status==S::Found&&a.name_bytes==2);
    const std::uint8_t empty[]={0};
    a=original_token_lookup_0x285630(&index,table,empty,sizeof(empty));
    assert(a.status==S::NotFound&&a.name_bytes==0);
    a=original_token_lookup_0x285630(&index,table,upper,5);
    assert(a.status==S::UnterminatedOriginalName);
    a=original_token_lookup_0x285630(&index,table,nullptr,6);
    assert(a.status==S::InvalidOriginalName);
    auto bad=table;bad.original_table_arm_va=0x1234;
    a=original_token_lookup_0x285630(&index,bad,upper,sizeof(upper));
    assert(a.status==S::InvalidOriginalCharacterTable);
    a=original_token_lookup_0x285630(nullptr,table,upper,sizeof(upper));
    assert(a.status==S::MissingOriginalIndex);
    assert(index.count()==3);
    return 0;
}
