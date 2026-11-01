#pragma once
#include <cstddef>
#include <cstdint>
namespace ds_port::original_runtime {
// Independently verified from the original user-confirmed ARM APK, not
// copied from the Xperia Play Vita build.
inline constexpr std::uint32_t kOriginalRawPointerDown=0x6000cu;
inline constexpr std::uint32_t kOriginalRawPointerMove=0x4000cu;
inline constexpr std::uint32_t kOriginalRawPointerUp=0x8000cu;
inline constexpr std::uint32_t kOriginalKeyDownEvent=0x40008u;
inline constexpr std::uint32_t kOriginalKeyUpEvent=0x80008u;
inline constexpr std::uint32_t kOriginalInputRecordInitialVtable=0x00489828u;
inline constexpr std::uint32_t kOriginalInputRecordFinalVtable=0x00489840u;
inline constexpr std::uint32_t kOriginalInputRecordSize=0x30u;

struct OriginalInputRecord {
    std::uint32_t arm_va=0;
    void* mapped_bytes=nullptr;
    std::size_t accessible_bytes=0;
};
struct OriginalInputAllocation {
    std::uint32_t original_pool_arm_va;
    std::uint32_t original_allocator_method_arm_va;
    std::uint32_t size=0x30;
    std::uint32_t r2=0;
    std::uint32_t r3=0;
    std::uint32_t stack0=8;
    std::uint32_t stack1=0;
};
using OriginalInputAllocate=bool (*)(const OriginalInputAllocation&,
    void* context,OriginalInputRecord*) noexcept;

// Original 0x2807A0 invokes Bionic __atomic_swap(0, record+4) via
// PLT 0x00035194 -> GOT 0x0049031C (real APK relocation verified).
// Native C++17 performs that exchange directly before queuing input.
struct OriginalInputQueuedEvent {
    std::uint32_t original_queue_owner_arm_va;
    std::uint32_t original_queue_method_arm_va;
    std::uint32_t event_id;
    std::uint32_t original_record_arm_va;
    std::uint32_t r3=0;
    std::uint32_t stack0=0;
};
using OriginalInputQueue=bool (*)(const OriginalInputQueuedEvent&,
    void* context) noexcept;
struct OriginalInputServices {
    std::uint32_t pool_owner_arm_va;
    std::uint32_t allocator_method_arm_va;
    std::uint32_t queue_owner_arm_va;
    std::uint32_t queue_method_arm_va;
    OriginalInputAllocate allocate;
    OriginalInputQueue queue;
    void* context;
};
// Both singleton BSS slots were independently resolved from pointer,
// key-down and key-up ARM handlers in original real APK run 41316094922.
inline constexpr std::uint32_t kOriginalInputAllocatorSingletonSlotVA=0x00490b30u;
inline constexpr std::uint32_t kOriginalInputQueueSingletonSlotVA=0x00490b24u;
struct OriginalInputMappedOwner {
    // Original 32-bit BSS word at one of the slots above.
    const void* singleton_slot_bytes;
    std::size_t singleton_slot_size;
    // Original ARM32 address and its mapped x86-64 object representation.
    std::uint32_t original_owner_arm_va;
    const void* owner_bytes;
    std::size_t owner_size;
    // Original ARM32 vtable token stored in the mapped object's first word.
    std::uint32_t original_vtable_arm_va;
    const void* vtable_bytes;
    std::size_t vtable_size;
};
struct OriginalInputMappedOwners {
    OriginalInputMappedOwner allocator;
    OriginalInputMappedOwner queue;
};
// Original JNI loads [global_slot] -> owner and [owner] -> vtable,
// then branches through vtable +0x0C (allocator) or +0x20 (queue).
// This resolves those four ARM address tokens from *mapped original
// memory*, rather than supplying arbitrary host-side method addresses.
// On failure, leaves output untouched.
bool original_resolve_input_virtual_owners(
    const OriginalInputMappedOwners& mapped,
    OriginalInputAllocate allocate_callback,OriginalInputQueue queue_callback,
    void* context,OriginalInputServices* output) noexcept;

enum class OriginalInputStatus:std::uint8_t {
    NotReady,MissingOriginalService,AllocationRefused,AllocationReturnedNull,
    InvalidRecord,QueueRefused,Completed
};
struct OriginalInputPacket {
    enum class Kind:std::uint8_t {Pointer,KeyDown,KeyUp};
    Kind kind;
    // Pointer: event_id is caller's raw action. Key events use the
    // proven constants (0x40008,0x80008) instead.
    std::uint32_t event_id;
    std::uint32_t module_id;
    std::uint32_t pointer_id;
    float x;
    float y;
    std::uint32_t android_key;
    std::uint32_t alt_pressed;
};
// Original pointer JNI 0x27F9B4, key down 0x284828, key up 0x284770.
// Readiness is the verified original guard 0x27E97C result supplied by
// the host. This executes no ARM code or Android runtime.
OriginalInputStatus original_dispatch_input_event(
    bool original_ready,const OriginalInputPacket& packet,
    const OriginalInputServices& services) noexcept;
} // namespace ds_port::original_runtime
