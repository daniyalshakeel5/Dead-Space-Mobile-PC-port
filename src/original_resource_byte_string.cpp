#include "ds_port/original_resource_byte_string.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <string>
#include <stdexcept>
#include <utility>

namespace ds_port::original_runtime {
OriginalResourceAppendStatus original_append_bytes_0x285918(
    OriginalResourceByteString& target,
    const std::uint8_t* bytes,std::size_t byte_count,
    std::size_t maximum_content_bytes) noexcept {
    using S=OriginalResourceAppendStatus;
    if(byte_count==0)return S::Completed;  // original CMP r1,r2 / BEQ
    if(!bytes||target.original_capacity_bytes==0||
       target.contents.size()>=std::numeric_limits<std::size_t>::max()-byte_count)
        return S::InvalidRange;
    const auto old_length=target.contents.size();
    const auto required=old_length+byte_count;
    if(required>maximum_content_bytes)return S::ExceedsLimit;
    // Original 0x285940: required=old_end-old_begin+(end-begin).
    // Original 0x285954..0x28596c: available=capacity_end-begin-1.
    auto new_capacity=target.original_capacity_bytes;
    const auto available=new_capacity-1u;
    if(required>available) {
        std::size_t desired=available<=8u?8u:available;
        if(available>8u) {
            if(available>maximum_content_bytes/2u)desired=maximum_content_bytes;
            else desired=available*2u;
        }
        desired=std::max(required,desired);
        // Native maximum limits original allocation expansion safely.
        desired=std::min(desired,maximum_content_bytes);
        if(desired>=std::numeric_limits<std::size_t>::max())
            return S::ExceedsLimit;
        new_capacity=desired+1u;
    }
    // std::string::append() supports source bytes that alias its own
    // allocation; unlike a raw memcpy this is safe with reallocation.
    try {
        target.contents.append(reinterpret_cast<const char*>(bytes),byte_count);
        target.original_capacity_bytes=new_capacity;
        // std::string keeps a trailing NUL independent of embedded zeros.
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
