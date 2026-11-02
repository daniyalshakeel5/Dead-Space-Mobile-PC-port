#include "guest_elf32.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace xperia {
namespace {
std::uint16_t rd16(const std::uint8_t* p){
    return std::uint16_t(p[0]) | std::uint16_t(p[1])<<8u;
}
std::uint32_t rd32(const std::uint8_t* p){
    return std::uint32_t(rd16(p)) | std::uint32_t(rd16(p+2))<<16u;
}
std::uint32_t checked32(std::uint64_t n){
    if(n>UINT32_MAX)throw std::runtime_error("ARM32 address overflow");
    return static_cast<std::uint32_t>(n);
}
}
bool GuestElf32::open(const std::vector<std::uint8_t>& bytes,
                      std::string& error) noexcept {
    segments.clear();remaining_relocations=0;relative_relocations=0;
    try {
        if(bytes.size()<52||bytes.size()>64u*1024u*1024u ||
           std::memcmp(bytes.data(),"\177ELF",4)!=0 ||
           bytes[4]!=1||bytes[5]!=1||bytes[6]!=1 ||
           rd16(bytes.data()+16)!=3||rd16(bytes.data()+18)!=40)
            throw std::runtime_error("Expected bounded little-endian ARM ELF32 ET_DYN");
        const std::uint32_t phoff=rd32(bytes.data()+28);
        const std::uint16_t phentsize=rd16(bytes.data()+42),
                            phcount=rd16(bytes.data()+44);
        if(phentsize!=32||phcount==0||phcount>128||
           std::uint64_t(phoff)+std::uint64_t(phcount)*32u>bytes.size())
            throw std::runtime_error("Invalid program headers");
        std::uint32_t dynamic_offset=0,dynamic_size=0;
        std::uint64_t mem_total=0;
        for(unsigned i=0;i<phcount;++i){
            auto* p=bytes.data()+phoff+32u*i;
            const auto type=rd32(p),offset=rd32(p+4),addr=rd32(p+8),
                       filesz=rd32(p+16),memsz=rd32(p+20);
            if(std::uint64_t(offset)+filesz>bytes.size())
                throw std::runtime_error("ELF segment out of file bounds");
            if(type==2){
                if(filesz%8 || filesz>65536)
                    throw std::runtime_error("Malformed PT_DYNAMIC");
                dynamic_offset=offset;dynamic_size=filesz;
            }
            if(type!=1)continue;
            if(memsz<filesz||memsz==0||std::uint64_t(addr)+memsz>0x100000000ull)
                throw std::runtime_error("Invalid PT_LOAD memory span");
            mem_total+=memsz;
            if(mem_total>128ull*1024*1024)
                throw std::runtime_error("Guest memory cap exceeded");
            GuestSegment seg;
            seg.start=addr;seg.contents.resize(memsz);
            std::copy_n(bytes.data()+offset,filesz,seg.contents.data());
            segments.push_back(std::move(seg));
        }
        if(segments.empty()||dynamic_size==0)
            throw std::runtime_error("No loadable segments or dynamic table");
        std::sort(segments.begin(),segments.end(),
                  [](const GuestSegment& a,const GuestSegment& b){
                      return a.start<b.start;});
        for(std::size_t i=1;i<segments.size();++i)
            if(std::uint64_t(segments[i-1].start)+segments[i-1].contents.size()>
               segments[i].start)
                throw std::runtime_error("Overlapping PT_LOAD guest maps");
        std::vector<std::pair<std::uint32_t,std::uint32_t>> tags;
        for(std::uint32_t off=dynamic_offset;off<dynamic_offset+dynamic_size;off+=8){
            const auto tag=rd32(bytes.data()+off),val=rd32(bytes.data()+off+4);
            if(tag==0)break;
            tags.emplace_back(tag,val);
        }
        const auto get=[&](std::uint32_t tag,std::uint32_t def=UINT32_MAX){
            for(const auto& item:tags)if(item.first==tag)return item.second;
            if(def==UINT32_MAX)throw std::runtime_error("Missing DT_REL metadata");
            return def;
        };
        struct RelTable {std::uint32_t address,size,entry;};
        std::vector<RelTable> tables={{get(17),get(18),get(19)}};
        const auto plt_size=get(2,0);
        if(plt_size){
            if(get(20)!=17)throw std::runtime_error("PLT relocations must use ELF REL");
            tables.push_back({get(23),plt_size,8});
        }
        std::size_t processed=0;
        for(const auto& table:tables){
            if(table.entry!=8||table.size%8||table.size/8>100000)
                throw std::runtime_error("Bad ARM32 relocation table");
            if(table.size==0)continue;
            // Only the original PT_LOAD file-backed payload can contain
            // relocation table records; target slots can be zero-filled BSS.
            const auto* rels=read(table.address,table.size);
            if(!rels)throw std::runtime_error("Unmapped REL table");
            for(std::uint32_t off=0;off<table.size;off+=8){
                auto target=rd32(rels+off);
                const auto info=rd32(rels+off+4);
                const auto kind=info&255u;
                if(!writeable(target,4))
                    throw std::runtime_error("Relocation target unmapped");
                if(kind==23u){
                    if((info>>8u)!=0u)throw std::runtime_error("RELATIVE with symbol index");
                    auto* at=writeable(target,4);
                    // ARM32 REL addend lives in the relocated target itself.
                    const auto addend=rd32(at);
                    const auto rebased=checked32(std::uint64_t(guest_base)+addend);
                    at[0]=std::uint8_t(rebased);
                    at[1]=std::uint8_t(rebased>>8u);
                    at[2]=std::uint8_t(rebased>>16u);
                    at[3]=std::uint8_t(rebased>>24u);
                    ++relative_relocations;
                }else if(kind==2u||kind==21u||kind==22u){
                    ++remaining_relocations; // Needs original host thunk/import binding.
                }else throw std::runtime_error("Unsupported ARM relocation type");
                ++processed;
            }
        }
        error.clear();return true;
    }catch(const std::exception& e) {
        error=e.what();segments.clear();
        remaining_relocations=0;relative_relocations=0;
        return false;
    }
}
const std::uint8_t* GuestElf32::read(std::uint32_t vaddr,std::size_t size) const noexcept {
    for(const auto& seg:segments){
        if(vaddr<seg.start)continue;
        const std::uint64_t offset=std::uint64_t(vaddr)-seg.start;
        if(offset<=seg.contents.size() && size<=seg.contents.size()-offset)
            return seg.contents.data()+offset;
    }
    return nullptr;
}
std::uint8_t* GuestElf32::writeable(std::uint32_t vaddr,std::size_t size) noexcept {
    for(auto& seg:segments){
        if(vaddr<seg.start)continue;
        const std::uint64_t offset=std::uint64_t(vaddr)-seg.start;
        if(offset<=seg.contents.size() && size<=seg.contents.size()-offset)
            return seg.contents.data()+offset;
    }
    return nullptr;
}
std::uint32_t GuestElf32::read_u32(std::uint32_t vaddr) const {
    const auto* p=read(vaddr,4);
    if(!p)throw std::out_of_range("Unmapped guest ELF32 word");
    return rd32(p);
}
} // namespace xperia
