#include "ds_port/original_record_text.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace ds_port::original_runtime {
OriginalRecordReplaceStatus original_replace_record_text_0x28611c(
    OriginalRecordUtf16Buffer& target,
    const std::uint8_t* source,std::size_t source_bytes,
    std::size_t maximum_code_units) noexcept {
    using S=OriginalRecordReplaceStatus;
    if(!target.original_capacity_code_units ||
       target.contents.size()>=target.original_capacity_code_units)
        return S::InvalidDestination;
    if(source_bytes && !source)return S::InvalidInput;
    if(source_bytes>maximum_code_units*4u)
        return S::ExceedsLimit;
    // Original 0x286130 BL 0x28CC88 with output capacity 0 counts
    // code units, reporting -1 when an invalid sequence is encountered.
    const auto measured=original_utf8_to_utf16_0x28cc88(
        nullptr,0,source,source_bytes,maximum_code_units*4u);
    if(measured.status==OriginalUtf8DecodeStatus::MalformedOriginalEncoding)
        return S::MalformedEncoding;
    if(measured.status!=OriginalUtf8DecodeStatus::Completed)
        return S::InvalidInput;
    const auto count=static_cast<std::size_t>(measured.original_r0);
    if(count>maximum_code_units)return S::ExceedsLimit;
    // ARM 0x286160 compares r6 (decoded size) to current UTF-16
    // length; original 0x28618C..0x2861B4 grows only when required.
    // available = ((capacity_end-begin)/2)-1 (NUL excluded).
    const auto available=target.original_capacity_code_units-1u;
    std::size_t target_capacity=target.original_capacity_code_units;
    if(count>available) {
        // Original 0x286198..0x2861B4:
        // new_capacity=max(count, available<=8?8:2*available)+1.
        std::size_t chosen=available<=8u?8u:available;
        if(available>8u) {
            if(available>maximum_code_units/2u)
                chosen=maximum_code_units;
            else chosen=available*2u;
        }
        chosen=std::max(chosen,count);
        chosen=std::min(chosen,maximum_code_units);
        if(chosen==std::numeric_limits<std::size_t>::max())
            return S::ExceedsLimit;
        target_capacity=chosen+1u;
    }
    // Host conversion is transactional: invalid source or allocation
    // failure leaves the old original buffer state intact. Original
    // 0x28611C replaces UTF-16 text; it does NOT append to the old text.
    try {
        std::u16string fresh;
        fresh.reserve(target_capacity-1u);
        fresh.resize(count);
        const auto decoded=original_utf8_to_utf16_0x28cc88(
            fresh.data(),count+1u,source,source_bytes,
            maximum_code_units*4u);
        if(decoded.status!=OriginalUtf8DecodeStatus::Completed ||
           decoded.original_r0!=measured.original_r0)
            return S::MalformedEncoding;
        target.contents.swap(fresh);
        target.original_capacity_code_units=target_capacity;
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
