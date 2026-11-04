#include "ds_port/original_native_resource_map.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>
using namespace ds_port::original_runtime;
int main() {
    OriginalNativeResourceMap registry{};
    OriginalNativeResourceEntry record{};
    assert(registry.count()==0);
    assert(!registry.find("energy",&record));
    assert(!registry.replace("","BEAM",0x4000));
    assert(!registry.replace("energy","BEAM",0));
    assert(registry.replace("energy","BEAM",0x4000u));
    assert(registry.replace("stasis","BLAST",0x4100u));
    assert(registry.count()==2);
    assert(registry.find("energy",&record));
    assert(record.descriptor=="BEAM"&&record.original_descriptor_arm_va==0x4000u);
    // 0x2839C8 + 0x283F5C + 0x2837B4 replaces existing keys.
    assert(registry.replace("energy","NEW",0x5000u));
    assert(registry.count()==2);
    assert(registry.successful_writes()==3);
    assert(registry.replacement_count()==1);
    assert(registry.find("energy",&record));
    assert(record.descriptor=="NEW"&&record.original_descriptor_arm_va==0x5000u);
    // Original 0x2837B4 uses unsigned memcmp, not host locale collation.
    const std::string key_byte(1,static_cast<char>(0xff));
    const std::string key_zero(1,static_cast<char>(0x01));
    OriginalUnsignedByteLess comp{};
    assert(comp(key_zero,key_byte));
    assert(!comp(key_byte,key_zero));
    assert(registry.replace(key_byte,"nonascii",0x6000u));
    assert(registry.find(key_byte,&record));
    assert(record.descriptor=="nonascii");
    assert(!registry.find("ENERGY",&record));
    assert(!registry.find("energy",nullptr));
    return 0;
}
