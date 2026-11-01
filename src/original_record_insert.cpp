#include "ds_port/original_record_insert.hpp"
#include "ds_port/original_resource_utf16.hpp"
#include <string>
#include <utility>
namespace ds_port::original_runtime {
OriginalRecordInsertResult0x2862e8 original_insert_record_0x2862e8(
    const OriginalRecordInsertInputs0x2862e8& in) noexcept {
    using S=OriginalRecordInsertStatus0x2862e8;
    auto fail=[](S s) noexcept {return OriginalRecordInsertResult0x2862e8{s,0};};
    if(!in.original_receiver_arm_va||!in.original_tag_arm_va||
       (in.tag_literal_bytes&&!in.original_tag_literal_utf16)||
       (in.record_utf8_bytes&&!in.original_record_utf8))
        return fail(S::InvalidOriginalInput);
    if(!in.original_virtual_0x58 && !in.native_receiver_0x278018)
        return fail(S::MissingOriginalReceiver);
    // Original 0x286364 calls 0x285590 to copy the code-unit range
    // resolved from its PC-relative UTF-16 literal.
    OriginalNativeUtf16String wide{};
    if(original_copy_utf16_range_0x285590(
       wide,in.original_tag_literal_utf16,in.tag_literal_bytes)!=
       OriginalNativeUtf16Status::Completed)
        return fail(S::OriginalLiteralBlocked);
    // The wide temporary is mutated in-place by original 0x28637C
    // BL 0x28611C. The source is the passed UTF-8 record range.
    // This code actually replaces native UTF-16 data, rather than
    // pretending that the receiver already accepted the new record.
    OriginalRecordUtf16Buffer record{};
    record.contents=std::move(wide.contents);
    record.original_capacity_code_units=record.contents.size()+1u;
    if(original_replace_record_text_0x28611c(
       record,in.original_record_utf8,in.record_utf8_bytes)!=
       OriginalRecordReplaceStatus::Completed)
        return fail(S::OriginalRecordTextBlocked);
    // Original 0x286394..0x286398 loads [receiver.vtable+0x58].
    // The virtual service must perform the real parsed-record insert.
    if(in.native_receiver_0x278018) {
        if(in.native_receiver_0x278018->original_owner_arm_va !=
           in.original_receiver_arm_va)
            return fail(S::InvalidOriginalInput);
        const auto native=original_execute_receiver_prefix_0x278018(
            *in.native_receiver_0x278018,
            std::u16string_view(record.contents));
        if(native.status!=OriginalReceiverPrefixStatus0x278018::CompletedNoMatch)
            return fail(S::OriginalReceiverRefused);
        // Original ARM 0x2862E8 ignores receiver r0. A receiver
        // returning zero on a resource miss completes its outer call,
        // but it did NOT insert or load a game resource.
        return {S::CompletedOriginalNoMatch,record.contents.size()};
    }
    if(!in.original_virtual_0x58(
        in.original_receiver_arm_va,0x58u,
        std::u16string_view(record.contents),
        in.original_tag_arm_va,in.original_argument_arm_va,in.context))
        return fail(S::OriginalReceiverRefused);
    return {S::CompletedCallSequence,record.contents.size()};
}
} // namespace ds_port::original_runtime
