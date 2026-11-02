#include "guest_bionic_data.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include <stdexcept>

namespace xperia {
namespace {
void put32(std::uint8_t* out,std::uint32_t val){
    for(unsigned i=0;i<4;++i)out[i]=std::uint8_t(val>>(8u*i));
}
std::string zone_name(std::tm* time){
    if(!time)return "UTC";
    char buffer[48]={};
    if(std::strftime(buffer,sizeof(buffer),"%Z",time)==0 || !buffer[0])
        return "UTC";
    return buffer;
}
}
bool GuestBionicData::install(GuestElf32& guest,std::string& error) {
    if(!symbols_.empty()){error="Guest Bionic data already initialized";return false;}
    if(guest.read(base,1)||guest.read(base+4095u,1)){
        error="Guest Bionic data overlaps ELF memory";return false;
    }
    try{
        guest.segments.push_back({base,std::vector<std::uint8_t>(4096,0)});
        auto* memory=guest.writeable(base,4096);
        if(!memory)throw std::runtime_error("Guest Bionic data unmapped");
        // Android/Linux ARM32 memory pages are 4096 bytes for this build.
        put32(memory+4,4096u);
        std::time_t now=std::time(nullptr);
        std::tm standard{};
        std::tm daylight{};
#ifdef _WIN32
        if(localtime_s(&standard,&now)!=0)
            throw std::runtime_error("Windows localtime failed");
#else
        if(!localtime_r(&now,&standard))
            throw std::runtime_error("Localtime failed");
#endif
        daylight=standard;
        // Store stable guest addresses. The current local timezone is
        // valid; no fabricated pointers into the host's process heap.
        const auto local=zone_name(&standard);
        const auto local2=zone_name(&daylight);
        if(local.size()>63 || local2.size()>63)
            throw std::runtime_error("Time zone abbreviation too large");
        std::memcpy(memory+64,local.c_str(),local.size()+1);
        std::memcpy(memory+128,local2.c_str(),local2.size()+1);
        put32(memory+8,base+64u);
        put32(memory+12,base+128u);
        symbols_.emplace("__dso_handle",base);
        symbols_.emplace("__page_size",base+4u);
        symbols_.emplace("tzname",base+8u);
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
} // namespace xperia
