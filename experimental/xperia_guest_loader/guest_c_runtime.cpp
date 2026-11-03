#include "guest_c_runtime.hpp"
#include "guest_heap.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace xperia {
namespace {
constexpr std::size_t max_text=1024u*1024u;
std::string readstr(GuestElf32& elf,std::uint32_t addr,
                    std::size_t limit=max_text){
    if(!addr)throw std::runtime_error("ARM C string pointer is null");
    std::string out;
    for(std::size_t i=0;i<limit;++i){
        if(std::uint64_t(addr)+i>UINT32_MAX)
            throw std::runtime_error("ARM libc string address overflow");
        const auto* b=elf.read(addr+static_cast<std::uint32_t>(i),1u);
        if(!b)throw std::runtime_error("ARM libc string pointer unmapped");
        if(!*b)return out;
        out.push_back(static_cast<char>(*b));
    }
    throw std::runtime_error("Unterminated ARM libc string exceeds bounded length");
}
void put32(GuestElf32& elf,std::uint32_t addr,std::uint32_t value){
    if(!addr||(addr&3u))throw std::runtime_error("ARM libc output word invalid");
    auto* dst=elf.writeable(addr,4u);
    if(!dst)throw std::runtime_error("ARM libc output pointer unmapped");
    for(unsigned i=0;i<4;++i)dst[i]=std::uint8_t(value>>(8u*i));
}
std::uint32_t arg(const std::array<std::uint32_t,16>& regs,
                  GuestElf32& guest,unsigned n){
    if(n<4)return regs[n];
    if(n>32 || std::uint64_t(regs[13])+4u*(n-4u)>UINT32_MAX)
        throw std::runtime_error("ARM libc AAPCS argument address overflow");
    return guest.read_u32(regs[13]+4u*(n-4u));
}
struct Varargs {
    const std::array<std::uint32_t,16>& regs;
    GuestElf32& elf;
    unsigned cursor;
    std::uint32_t list=0;
    std::uint32_t read32(){
        if(cursor>=32)throw std::runtime_error("ARM libc varargs length cap");
        if(list){
            if(std::uint64_t(list)+4u*cursor>UINT32_MAX)
                throw std::runtime_error("ARM va_list address overflow");
            return elf.read_u32(list+cursor++*4u);
        }
        return arg(regs,elf,cursor++);
    }
    std::uint64_t read64(){
        if(cursor&1u)++cursor; // AAPCS 8-byte varargs alignment
        const std::uint64_t lo=read32(),hi=read32();
        return lo|(hi<<32u);
    }
};
std::string pad(std::string value,int width,bool left,bool zero){
    if(width<0||width>2048)throw std::runtime_error("ARM printf width exceeds cap");
    if(std::size_t(width)<=value.size())return value;
    const auto n=std::size_t(width)-value.size();
    if(left)return value+std::string(n,' ');
    if(zero){
        const auto prefix=(value.empty()?0u:
                           (value[0]=='-'||value[0]=='+'||value[0]==' ')?1u:
                           (value.size()>2u&&value[0]=='0'&&
                           (value[1]=='x'||value[1]=='X'))?2u:0u);
        return value.substr(0,prefix)+std::string(n,'0')+value.substr(prefix);
    }
    return std::string(n,' ')+value;
}
std::string format(GuestElf32& guest,const std::string& fmt,
                   Varargs& args){
    std::string out;
    for(std::size_t i=0;i<fmt.size();++i){
        if(fmt[i]!='%'){out.push_back(fmt[i]);continue;}
        if(++i>=fmt.size())throw std::runtime_error("Incomplete ARM printf escape");
        if(fmt[i]=='%'){out.push_back('%');continue;}
        bool left=false,zero=false,plus=false,space=false,alternate=false;
        while(i<fmt.size()){
            const auto v=fmt[i];
            if(v=='-')left=true;
            else if(v=='0')zero=true;
            else if(v=='+')plus=true;
            else if(v==' ')space=true;
            else if(v=='#')alternate=true;
            else break;
            ++i;
        }
        int width=0;
        if(i<fmt.size()&&fmt[i]=='*'){
            width=static_cast<std::int32_t>(args.read32());
            if(width<0){left=true;width=-width;}
            ++i;
        }else{
            while(i<fmt.size()&&fmt[i]>='0'&&fmt[i]<='9'){
                width=width*10+fmt[i++]-'0';
                if(width>2048)throw std::runtime_error("ARM printf field width cap");
            }
        }
        int precision=-1;
        if(i<fmt.size()&&fmt[i]=='.'){
            ++i;precision=0;
            if(i<fmt.size()&&fmt[i]=='*'){
                precision=static_cast<std::int32_t>(args.read32());
                ++i;
            }else while(i<fmt.size()&&fmt[i]>='0'&&fmt[i]<='9'){
                precision=precision*10+fmt[i++]-'0';
                if(precision>1024)throw std::runtime_error("ARM printf precision cap");
            }
        }
        if(precision>1024)throw std::runtime_error("ARM printf precision cap");
        bool wide=false;
        if(i<fmt.size()&&fmt[i]=='l'){
            ++i;
            if(i<fmt.size()&&fmt[i]=='l'){wide=true;++i;}
        }else if(i<fmt.size()&&fmt[i]=='h'){
            ++i;if(i<fmt.size()&&fmt[i]=='h')++i;
        }else if(i<fmt.size()&&(fmt[i]=='z'||fmt[i]=='t')){
            ++i; // ARM32 size_t and ptrdiff_t are 32-bit
        }
        if(i>=fmt.size())throw std::runtime_error("Incomplete ARM printf format");
        const auto code=fmt[i];
        if(code=='n')throw std::runtime_error("ARM printf %n cannot write unchecked guest pointer");
        std::string fragment;
        if(code=='s'){
            const auto ptr=args.read32();
            fragment=ptr?readstr(guest,ptr,8192u):"(null)";
            if(precision>=0&&fragment.size()>std::size_t(precision))
                fragment.resize(static_cast<std::size_t>(precision));
        }else if(code=='c'){
            fragment.assign(1,char(args.read32()&255u));
        }else if(code=='d'||code=='i'||code=='u'||
                 code=='x'||code=='X'||code=='o'||code=='p'){
            const auto raw=wide?args.read64():std::uint64_t(args.read32());
            std::ostringstream stream;
            if(code=='x'||code=='X'||code=='p')stream<<std::hex;
            else if(code=='o')stream<<std::oct;
            if(code=='X')stream<<std::uppercase;
            if(code=='p'||(alternate && (code=='x'||code=='X')))
                stream<<(code=='X'?"0X":"0x");
            if(code=='d'||code=='i'){
                const auto value=wide?
                    static_cast<std::int64_t>(raw):
                    std::int64_t(static_cast<std::int32_t>(raw));
                if(value>=0&&plus)stream<<'+';
                else if(value>=0&&space)stream<<' ';
                stream<<value;
            }else if(code=='p'){
                stream<<static_cast<std::uint32_t>(raw);
            }else stream<<(wide?raw:std::uint64_t(std::uint32_t(raw)));
            fragment=stream.str();
            if(precision>=0 && code!='p'){
                std::size_t prefix=0;
                if(!fragment.empty()&&(fragment[0]=='-'||
                    fragment[0]=='+'||fragment[0]==' '))prefix=1;
                if(fragment.size()>prefix && precision>int(fragment.size()-prefix))
                    fragment=fragment.substr(0,prefix)+
                        std::string(std::size_t(precision)-(fragment.size()-prefix),'0')+
                        fragment.substr(prefix);
                zero=false;
            }
        }else if(code=='f'||code=='F'||code=='e'||code=='E'||
                 code=='g'||code=='G'){
            const auto bits=args.read64();
            double value=0;
            std::memcpy(&value,&bits,8u);
            std::ostringstream stream;
            if(plus)stream<<std::showpos;
            if(alternate)stream<<std::showpoint;
            if(code=='F'||code=='E'||code=='G')stream<<std::uppercase;
            if(code=='f'||code=='F')stream<<std::fixed;
            if(code=='e'||code=='E')stream<<std::scientific;
            stream<<std::setprecision(precision<0?6:std::min(precision,64));
            stream<<value;
            fragment=stream.str();
        }else{
            throw std::runtime_error("Unsupported original ARM printf conversion: %"+
                                     std::string(1,code));
        }
        out+=pad(fragment,width,left,zero&&!left);
        if(out.size()>max_text)
            throw std::runtime_error("ARM libc formatted output exceeds 1MiB");
    }
    return out;
}
void guest_write(GuestElf32& elf,std::uint32_t ptr,std::size_t cap,
                 const std::string& text){
    if(cap>max_text+1u)throw std::runtime_error("ARM formatted output capacity cap");
    if(!cap)return;
    const auto count=std::min(cap-1u,text.size());
    auto* dst=elf.writeable(ptr,count+1u);
    if(!dst)throw std::runtime_error("ARM formatted output buffer unmapped");
    if(count)std::memmove(dst,text.data(),count);
    dst[count]=0;
}
std::string from_guest(GuestElf32& guest,std::uint32_t str,
                       std::size_t limit=max_text){
    return readstr(guest,str,limit);
}
} // namespace
bool GuestCRuntime::dispatch(unsigned id,
        std::array<std::uint32_t,16>& regs,GuestElf32& elf,
        GuestHeap* heap,std::string& error) noexcept{
    try{
        if(id<first_svc||id>last_svc)
            throw std::runtime_error("Original ARM C runtime service ID outside range");
        const auto a=regs[0],b=regs[1],n=regs[2];
        std::uint32_t result=0;
        switch(id){
        case 85:case 86:case 87:{ // strcpy / strncpy / strcat
            const auto src=from_guest(elf,b);
            const auto old=id==87?from_guest(elf,a):std::string{};
            if(id==86){
                if(n>max_text)throw std::runtime_error("ARM strncpy count cap");
                auto* dest=elf.writeable(a,n);
                if(n&&!dest)throw std::runtime_error("ARM strncpy target unmapped");
                const auto bytes=std::min<std::size_t>(src.size(),n);
                if(bytes)std::memmove(dest,src.data(),bytes);
                if(n>bytes)std::memset(dest+bytes,0,n-bytes);
            }else{
                const auto merged=id==87?old+src:src;
                if(merged.size()>max_text)
                    throw std::runtime_error("ARM string concatenate limit");
                guest_write(elf,a,merged.size()+1u,merged);
            }
            result=a;break;
        }
        case 88:case 89:{ // strchr / strrchr
            const auto str=from_guest(elf,a);
            const char needle=char(b&255u);
            const auto offset=id==88?str.find(needle):str.rfind(needle);
            result=offset==std::string::npos?0u:
                   a+static_cast<std::uint32_t>(offset);
            if(needle==0)result=a+static_cast<std::uint32_t>(str.size());
            break;
        }
        case 90:{ // strstr
            const auto text=from_guest(elf,a),search=from_guest(elf,b);
            const auto pos=text.find(search);
            result=pos==std::string::npos?0u:
                   a+static_cast<std::uint32_t>(pos);break;
        }
        case 91:{ // strdup
            if(!heap)throw std::runtime_error("Original ARM strdup needs guest heap");
            const auto text=from_guest(elf,a);
            result=heap->allocate(static_cast<std::uint32_t>(text.size()+1u));
            if(!result)throw std::runtime_error("ARM guest strdup heap exhausted");
            guest_write(elf,result,text.size()+1u,text);
            break;
        }
        case 92:case 93:case 94:{ // atoi / strtol / strtoul
            const auto text=from_guest(elf,a,4096u);
            char* finish=nullptr;
            const auto base=id==92?10:int(n);
            if(base!=0&&(base<2||base>36))
                throw std::runtime_error("ARM strtol numeric radix invalid");
            if(id==94){
                const auto value=std::strtoull(text.c_str(),&finish,base);
                result=std::uint32_t(std::min<std::uint64_t>(value,UINT32_MAX));
            }else{
                const auto value=std::strtoll(text.c_str(),&finish,base);
                const auto clamped=std::clamp<std::int64_t>(
                    value,INT32_MIN,INT32_MAX);
                result=std::uint32_t(std::int32_t(clamped));
            }
            if(id!=92 && b)
                put32(elf,b,a+static_cast<std::uint32_t>(finish-text.c_str()));
            break;
        }
        case 95:case 96:case 97:case 98:
        case 99:case 100:{ // sprintf/snprintf/vsnprintf and log variants
            const bool log=id>=99u;
            const bool use_va=id==97||id==98||id==100;
            const auto fmt=log?n:id==95||id==97?n:b;
            const auto format_text=from_guest(elf,fmt,8192u);
            const unsigned first=log?3u:id==95||id==97?3u:2u;
            Varargs args{regs,elf,use_va?0u:first,use_va?regs[3]:0u};
            if(use_va && !regs[3])
                throw std::runtime_error("ARM formatted va_list pointer missing");
            const auto output=format(elf,format_text,args);
            if(log){
                const auto tag=from_guest(elf,b,256u);
                std::clog<<"[original-android/"<<(a&255u)<<"/"<<tag<<"] "<<output<<"\n";
                result=std::uint32_t(output.size());
            }else{
                const auto cap=id==95||id==97?std::size_t(b):output.size()+1u;
                guest_write(elf,a,cap,output);
                result=std::uint32_t(output.size());
            }
            break;
        }
        case 101:{ // getenv: no host environment leaks into guest process
            const auto name=from_guest(elf,a,1024u);
            if(name.empty())throw std::runtime_error("ARM getenv empty key");
            result=0;break; // unset, not fabricated host data
        }
        default:throw std::runtime_error("Unimplemented ARM C runtime service");
        }
        regs[0]=result;
        error.clear();return true;
    }catch(const std::exception& e){
        error=std::string("Original Android ARM32 libc #")+
              std::to_string(id)+": "+e.what();
        return false;
    }
}
} // namespace xperia
