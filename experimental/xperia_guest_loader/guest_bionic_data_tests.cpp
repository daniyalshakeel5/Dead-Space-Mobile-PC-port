#ifdef NDEBUG
#undef NDEBUG
#endif
#include "guest_bionic_data.hpp"
#include "guest_libc_bridge.hpp"
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
using namespace xperia;
int main(){
    GuestElf32 elf;
    elf.segments.push_back({0x10001000u,std::vector<std::uint8_t>(4096,0)});
    elf.pending_imports.push_back({0x10001100u,21u,0u,"__dso_handle",1u});
    elf.pending_imports.push_back({0x10001104u,21u,0u,"__page_size",1u});
    elf.pending_imports.push_back({0x10001108u,21u,0u,"tzname",1u});
    GuestBionicData data;
    std::string error;
    assert(data.install(elf,error));
    assert(!data.install(elf,error));
    assert(elf.read_u32(data.symbols().at("__page_size"))==4096u);
    const auto ptrs=data.symbols().at("tzname");
    assert(elf.read_u32(ptrs)>=data.base+64);
    assert(elf.read(elf.read_u32(ptrs),1)[0]!=0);
    GuestLibcBridge svc;
    assert(svc.plan_guest_imports(elf,error).empty());
    assert(error.find("Unimplemented guest data")!=std::string::npos);
    auto planned=svc.plan_guest_imports(elf,error,data.symbols());
    assert(!planned.empty() && error.empty());
    assert(elf.bind_imports(planned,error));
    assert(elf.read_u32(0x10001100u)==data.symbols().at("__dso_handle"));
    assert(elf.read_u32(0x10001104u)==data.symbols().at("__page_size"));
    assert(elf.read_u32(0x10001108u)==data.symbols().at("tzname"));
    return 0;
}
