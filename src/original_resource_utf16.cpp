#include "ds_port/original_resource_utf16.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace ds_port::original_runtime {
OriginalNativeUtf16Status original_copy_utf16_range_0x285590(
    OriginalNativeUtf16String& out,
    const std::uint8_t* p,std::size_t byte_count,
    std::size_t maximum_code_units) noexcept {
    using S=OriginalNativeUtf16Status;
    // ARM 0x285590+0x08 ASR byte-span by one. Do not accept a
    // truncated odd byte as a valid native resource string.
    if((byte_count & 1u)!=0u || (byte_count && !p))
        return S::InvalidOriginalRange;
    const std::size_t length=byte_count/2u;
    if(length>maximum_code_units)return S::ExceedsLimit;
    try {
        std::u16string copy;
        copy.reserve(length);
        for(std::size_t i=0;i<length;++i) {
            const auto low=static_cast<std::uint16_t>(p[i*2u]);
            const auto high=static_cast<std::uint16_t>(p[i*2u+1u]);
            copy.push_back(static_cast<char16_t>(low|(high<<8u)));
        }
        // ARM 0x285590 +0x58 STRH r3,[data,bytes] with r3=0.
        // std::u16string guarantees this native trailing terminator.
        out.contents.swap(copy);
        return S::Completed;
    } catch(const std::bad_alloc&) {
        return S::AllocationFailed;
    } catch(const std::length_error&) {
        return S::ExceedsLimit;
    } catch(...) {
        return S::AllocationFailed;
    }
}
} // namespace ds_port::original_runtime
