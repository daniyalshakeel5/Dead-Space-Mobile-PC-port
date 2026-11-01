#pragma once
#include "ds_port/original_record_text.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ds_port::original_runtime {
// The original 0x2862E8 first constructs a temporary wide string at
// 0x285590, calls 0x28611C to replace its text from a byte range,
// then invokes r4's virtual method +0x58 with r1=decoded UTF-16
// begin, r2=original literal tag pointer, r3=original caller argument.
// The receiver method itself remains unported; this native service
// MUST be bound to an independent reconstruction, not a dummy success.
using OriginalRecordReceiverMethod58=bool (*)(
    std::uint32_t receiver_arm_va,std::uint32_t original_method_offset,
    std::u16string_view record_text,
    std::uint32_t original_tag_arm_va,
    std::uint32_t original_argument_arm_va,
    void* context) noexcept;
struct OriginalRecordInsertInputs0x2862e8 {
    std::uint32_t original_receiver_arm_va=0;
    std::uint32_t original_tag_arm_va=0;
    std::uint32_t original_argument_arm_va=0;
    const std::uint8_t* original_tag_literal_utf16=nullptr;
    std::size_t tag_literal_bytes=0;
    const std::uint8_t* original_record_utf8=nullptr;
    std::size_t record_utf8_bytes=0;
    OriginalRecordReceiverMethod58 original_virtual_0x58=nullptr;
    void* context=nullptr;
};
enum class OriginalRecordInsertStatus0x2862e8:std::uint8_t {
    InvalidOriginalInput,MissingOriginalReceiver,OriginalLiteralBlocked,
    OriginalRecordTextBlocked,OriginalReceiverRefused,CompletedCallSequence
};
struct OriginalRecordInsertResult0x2862e8 {
    OriginalRecordInsertStatus0x2862e8 status;
    std::size_t decoded_code_units;
};
OriginalRecordInsertResult0x2862e8 original_insert_record_0x2862e8(
    const OriginalRecordInsertInputs0x2862e8&) noexcept;
} // namespace ds_port::original_runtime
