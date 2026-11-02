#include "guest_elf32.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>

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
    segments.clear();pending_imports.clear();
    remaining_relocations=0;relative_relocations=0;
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
            // Actual guest address space and relocation base must agree.
            // Previously B+A pointers were rebased, but code/segments
            // remained at unrebased addresses: a dereference would fault.
            const auto mapped=std::uint64_t(guest_base)+addr;
            if(mapped+memsz>0x100000000ull)
                throw std::runtime_error("Guest PT_LOAD exceeds ARM32 address space");
            seg.start=checked32(mapped);seg.contents.resize(memsz);
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
        // Resolve names directly from the mapped ELF dynamic table,
        // without a host symbol resolver or Android OS process.
        struct Sym {std::string name;std::uint32_t value=0;bool defined=false;bool weak=false;bool absolute=false;};
        bool symbols_ready=false;
        std::uint32_t symbols_count=0;
        std::uint32_t sym_addr=0,str_addr=0,str_size=0;
        const auto symbol=[&](std::uint32_t idx)->Sym {
            if(!symbols_ready) {
                const auto* hash=read(checked32(std::uint64_t(guest_base)+get(4)),8);
                if(!hash)throw std::runtime_error("Missing mapped SysV hash table");
                symbols_count=rd32(hash+4);
                if(!symbols_count||symbols_count>200000)
                    throw std::runtime_error("Symbol count outside bounds");
                sym_addr=get(6);str_addr=get(5);str_size=get(10);
                if(get(11)!=16||!str_size||str_size>32u*1024u*1024u)
                    throw std::runtime_error("Invalid ELF dynamic symbol layout");
                symbols_ready=true;
            }
            if(idx>=symbols_count)
                throw std::runtime_error("Relocation references an invalid dynsym index");
            const auto* p=read(checked32(std::uint64_t(guest_base)+sym_addr+
                std::uint64_t(idx)*16u),16);
            if(!p)throw std::runtime_error("Unmapped ELF dynsym entry");
            const auto offset=rd32(p);
            if(offset>=str_size)
                throw std::runtime_error("Symbol name outside ELF dynamic strings");
            const auto* strings=read(
                checked32(std::uint64_t(guest_base)+str_addr),str_size);
            if(!strings)throw std::runtime_error("Unmapped ELF dynamic string table");
            const auto end=std::find(strings+offset,strings+str_size,0);
            if(end==strings+str_size)
                throw std::runtime_error("Unterminated ELF dynamic symbol");
            if(end-(strings+offset)>8192)
                throw std::runtime_error("Excessively long dynamic symbol");
            Sym sym;
            sym.name.assign(reinterpret_cast<const char*>(strings+offset),
                            reinterpret_cast<const char*>(end));
            sym.value=rd32(p+4);
            const auto section=rd16(p+14);
            sym.defined=section!=0;
            sym.absolute=section==0xfff1;
            sym.weak=(p[12]>>4)==2;
            return sym;
        };
        std::unordered_set<std::uint32_t> used_targets;
        std::size_t processed=0;
        for(const auto& table:tables){
            if(table.entry!=8||table.size%8||table.size/8>100000)
                throw std::runtime_error("Bad ARM32 relocation table");
            if(table.size==0)continue;
            // Only the original PT_LOAD file-backed payload can contain
            // relocation table records; target slots can be zero-filled BSS.
            const auto* rels=read(checked32(std::uint64_t(guest_base)+table.address),table.size);
            if(!rels)throw std::runtime_error("Unmapped REL table");
            for(std::uint32_t off=0;off<table.size;off+=8){
                auto target=rd32(rels+off);
                const auto info=rd32(rels+off+4);
                const auto kind=info&255u;
                if(!used_targets.insert(target).second)
                    throw std::runtime_error("Duplicate ARM ELF relocation target");
                if(!writeable(checked32(std::uint64_t(guest_base)+target),4))
                    throw std::runtime_error("Relocation target unmapped");
                if(kind==23u){
                    if((info>>8u)!=0u)throw std::runtime_error("RELATIVE with symbol index");
                    auto* at=writeable(checked32(std::uint64_t(guest_base)+target),4);
                    // ARM32 REL addend lives in the relocated target itself.
                    const auto addend=rd32(at);
                    const auto rebased=checked32(std::uint64_t(guest_base)+addend);
                    at[0]=std::uint8_t(rebased);
                    at[1]=std::uint8_t(rebased>>8u);
                    at[2]=std::uint8_t(rebased>>16u);
                    at[3]=std::uint8_t(rebased>>24u);
                    ++relative_relocations;
                }else if(kind==2u||kind==21u||kind==22u){
                    const auto idx=info>>8;
                    if(!idx)throw std::runtime_error("ARM import relocation without symbol");
                    const auto sym=symbol(idx);
                    const auto at=checked32(std::uint64_t(guest_base)+target);
                    const auto addend=read_u32(at); // ARM REL implicit addend
                    if(sym.defined||sym.weak){
                        // Internal or optional undefined-weak symbols bind
                        // immediately. External functions remain unbound.
                        const std::uint32_t value=sym.defined
                            ? checked32(std::uint64_t(sym.absolute?0:guest_base)+sym.value)
                            : 0u;
                        const auto result=kind==2u
                            ? checked32(std::uint64_t(value)+addend)
                            : value;
                        auto* slot=writeable(at,4);
                        for(unsigned n=0;n<4;++n)slot[n]=std::uint8_t(result>>(8*n));
                    }else{
                        if(sym.name.empty())throw std::runtime_error("Unnamed mandatory import");
                        pending_imports.push_back({at,kind,addend,sym.name});
                        ++remaining_relocations;
                    }
                }else throw std::runtime_error("Unsupported ARM relocation type");
                ++processed;
            }
        }
        error.clear();return true;
    }catch(const std::exception& e) {
        error=e.what();segments.clear();pending_imports.clear();
        remaining_relocations=0;relative_relocations=0;
        return false;
    }
}
bool GuestElf32::bind_imports(
    const std::unordered_map<std::string,std::uint32_t>& symbols,
    std::string& error) noexcept {
    // Validate EVERYTHING before changing any guest relocation slot.
    // A missing required host service must leave the module unbound.
    try {
        for(const auto& rel:pending_imports) {
            const auto it=symbols.find(rel.name);
            if(it==symbols.end()||it->second==0)
                throw std::runtime_error("Unbound original Android import: "+rel.name);
            if(rel.kind==2u && std::uint64_t(it->second)+rel.addend>UINT32_MAX)
                throw std::runtime_error("Guest ARM32 symbol/addend overflow");
            if(!writeable(rel.target,4))
                throw std::runtime_error("Original import slot became unmapped");
        }
        for(const auto& rel:pending_imports){
            const auto addr=symbols.at(rel.name);
            const auto value=rel.kind==2u?addr+rel.addend:addr;
            auto* bytes=writeable(rel.target,4);
            for(unsigned i=0;i<4;++i)bytes[i]=std::uint8_t(value>>(i*8u));
        }
        pending_imports.clear();remaining_relocations=0;
        error.clear();return true;
    }catch(const std::exception& e){
        error=e.what();return false;
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
