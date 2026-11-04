#include "ds_port/original_native_resource_engine.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace ds_port::original_runtime;
namespace {
void put(std::uint8_t* p,std::uint32_t v) {
    for(unsigned k=0;k<4;++k)p[k]=std::uint8_t(v>>(8u*k));
}
int fileno_of(std::FILE* f) {
#ifdef _WIN32
    return ::_fileno(f);
#else
    return ::fileno(f);
#endif
}
}
int main() {
    using S=OriginalOwnerWideLineStatus;
    // No copyrighted original assets: this native test uses a real
    // temporary file with all byte lines, not a memory-only fixture.
    std::FILE* f=std::tmpfile();
    assert(f!=nullptr);
    const int fd=fileno_of(f);
    assert(fd>=0);
    const std::string payload=std::string("HEADER\r\n")+
        std::string(63,'x')+"\r\n"+
        std::string("\xE2\x82\xAC\n",4)+"LAST";
    assert(std::fwrite(payload.data(),1,payload.size(),f)==payload.size());
    assert(std::fflush(f)==0);
    assert(std::fseek(f,0,SEEK_SET)==0);
    std::array<std::uint8_t,4> singleton{};
    put(singleton.data(),0x490000u);
    OriginalNativeResourceEngine engine(0x2000u);
    OriginalOwnerConstructorInputs0x27a768 args{};
    args.mapped_allocator_global_word=singleton.data();
    args.mapped_allocator_global_bytes=singleton.size();
    args.original_pool_arm_va=0x490000u;
    args.original_provider_arm_va=engine.original_owner_va()+0x804u;
    auto result=engine.initialize(args);
    assert(result.status==OriginalOwnerConstructorStatus0x27a768::Completed);
    assert(engine.bind_user_asset_fd(-1)==false);
    assert(engine.bind_user_asset_fd(fd));
    OriginalNativeUtf16String out{};
    auto line=engine.read_bound_asset_line_0x276d18(fd,out);
    assert(line.status==S::LineDecoded&&out.contents==u"HEADER");
    line=engine.read_bound_asset_line_0x276d18(fd,out);
    assert(line.status==S::LineDecoded&&out.contents==std::u16string(63,u'x'));
    // The UTF-8 path uses original 0x26D5FC. The CR was at the
    // end of a provider read and its following LF is consumed.
    line=engine.read_bound_asset_line_0x276d18(fd,out);
    assert(line.status==S::LineDecoded&&out.contents==u"\u20ac");
    line=engine.read_bound_asset_line_0x276d18(fd,out);
    assert(line.status==S::LineDecoded&&out.contents==u"LAST");
    line=engine.read_bound_asset_line_0x276d18(fd,out);
    assert(line.status==S::EndOfStream&&out.contents.empty());
    // Explicit host binding rejects a different host fd even
    // if the original provider was otherwise constructed.
    line=engine.read_bound_asset_line_0x276d18(fd+1,out);
    assert(line.status==S::InvalidInput);
    // Missing original provider pointer cannot be patched over
    // by the native asset adapter.
    OriginalNativeResourceEngine missing(0x3000u);
    args.original_provider_arm_va=0;
    assert(missing.initialize(args).status==
           OriginalOwnerConstructorStatus0x27a768::Completed);
    assert(!missing.bind_user_asset_fd(fd));
    assert(missing.read_bound_asset_line_0x276d18(fd,out).status==
           S::InvalidInput);
    assert(std::fclose(f)==0);
    // Maximum line cap is 0x4000 bytes; failures must roll back
    // the native stream cursor and preserve existing UTF-16 data.
    f=std::tmpfile();
    assert(f!=nullptr);
    const int other=fileno_of(f);
    std::string too_long(0x4001,'Q');
    too_long+='\n';
    assert(std::fwrite(too_long.data(),1,too_long.size(),f)==too_long.size());
    assert(std::fflush(f)==0);
    assert(std::fseek(f,0,SEEK_SET)==0);
    assert(engine.bind_user_asset_fd(other));
    out.contents=u"preserved";
    line=engine.read_bound_asset_line_0x276d18(other,out);
    assert(line.status==S::LineTooLong&&out.contents==u"preserved");
    const auto original=original_embedded_provider_position_file_0x271794(
        // Engine's owned provider is original owner+0x804.
        // This original owner owns mutable mapped storage; selector 2
        // may write original errno to the embedded provider's +0xA0.
        const_cast<std::uint8_t*>(engine.mapped_bytes())+0x804u,
        engine.mapped_size()-0x804u,other,0u);
    assert(original.status==OriginalEmbeddedProviderStatus::Completed);
    assert(original.original_r0==0u);
    assert(std::fclose(f)==0);
    return 0;
}
