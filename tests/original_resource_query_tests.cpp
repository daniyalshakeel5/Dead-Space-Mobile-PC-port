#include "ds_port/original_resource_query.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
using namespace ds_port::original_runtime;
struct F {std::array<unsigned char,32> ctx{};std::array<unsigned char,16> h{};int calls=0;};
static void put(unsigned char* p,unsigned x){for(int i=0;i<4;++i)p[i]=static_cast<unsigned char>(x>>(i*8));}
static bool resolve(unsigned va,void*,OriginalResourceQueryString* n) noexcept {
    static const unsigned char name[]={'a',0};
    if(va!=0x4000)return false;
    *n={name,sizeof(name)};return true;
}
static bool create(unsigned owner,std::string_view n,void* p,OriginalResourceHandleInput* h) noexcept {
    auto& f=*static_cast<F*>(p);++f.calls;
    if(owner!=0x2000||n!="a")return false;
    *h={f.h.data(),f.h.size(),0,nullptr,0,0,nullptr,0};return true;
}
static bool failure(unsigned,std::string_view,const OriginalResourceHandleInput&,void* p) noexcept {
    ++static_cast<F*>(p)->calls;return true;
}
static bool scratch(unsigned owner,void* p) noexcept {
    ++static_cast<F*>(p)->calls;return owner==0x2000;
}
static bool binding(unsigned va,unsigned index,void* p,unsigned* out) noexcept {
    ++static_cast<F*>(p)->calls;*out=index+1;return va==0x1018&&index<2;
}
static bool loader(unsigned va,unsigned first,unsigned second,
                   const OriginalResourceHandleInput&,void* p) noexcept {
    ++static_cast<F*>(p)->calls;return va==0x1018&&first==1&&second==2;
}
static bool clean(void* p) noexcept {++static_cast<F*>(p)->calls;return true;}
static bool mode1(unsigned owner,std::string_view name,void* p,
                  unsigned* out) noexcept {
    auto& f=*static_cast<F*>(p);++f.calls;
    if(owner!=0x2000||name!="a")return false;
    *out=0x5000u;return true;
}
int main(){
    F f{};put(f.ctx.data()+4,0x2000);put(f.h.data()+4,1);put(f.h.data()+8,1);
    OriginalResourceQueryInputs in{0x1000,f.ctx.data(),f.ctx.size(),
       resolve,create,nullptr,failure,scratch,binding,loader,clean,clean,&f};
    auto r=original_execute_resource_query_0x2803e4(in,0x4000);
    assert(r.status==OriginalResourceQueryStatus::Completed&&r.original_r0==1);
    assert(f.calls==7);
    f.calls=0;put(f.h.data()+8,0);
    r=original_execute_resource_query_0x2803e4(in,0x4000);
    assert(r.status==OriginalResourceQueryStatus::Completed&&r.original_r0==0);
    assert(f.calls==2);
    f.calls=0;in.original_0x28a4f8_failure_path=nullptr;
    r=original_execute_resource_query_0x2803e4(in,0x4000);
    assert(r.status==OriginalResourceQueryStatus::OriginalFailurePathBlocked);
    assert(f.calls==1);
    f.calls=0;put(f.h.data()+8,1);in.original_0x286918=nullptr;
    r=original_execute_resource_query_0x2803e4(in,0x4000);
    assert(r.status==OriginalResourceQueryStatus::MissingOriginalService);
    assert(f.calls==1);
    // End-to-end native resource lookup through reconstructed 0x28B3A4
    // constructor: the only unresolved factory dependency is 0x28A718.
    f.calls=0;
    in.original_0x286918=loader;
    OriginalMode1FactoryInputs native{
        f.h.data(),f.h.size(),mode1,&f};
    in.original_0x28b3a4=nullptr;
    in.native_factory_0x28b3a4=&native;
    r=original_execute_resource_query_0x2803e4(in,0x4000);
    assert(r.status==OriginalResourceQueryStatus::Completed);
    assert(r.original_r0==1u);
    assert(f.calls==7);
    // Prove actual ARM vtable/mode/resource/owner stores, not a mock handle.
    auto read=[](const unsigned char* p)->unsigned {
        return unsigned(p[0])|(unsigned(p[1])<<8)|
               (unsigned(p[2])<<16)|(unsigned(p[3])<<24);
    };
    assert(read(f.h.data())==kOriginalMode1HandleVtable);
    assert(read(f.h.data()+4)==1u);
    assert(read(f.h.data()+8)==0x5000u);
    assert(read(f.h.data()+12)==0x2000u);
}
