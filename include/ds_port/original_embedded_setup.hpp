#pragma once
#include "ds_port/original_registry_lookup.hpp"
#include <cstddef>
#include <cstdint>

namespace ds_port::original_runtime {
// Original ARM ELF 0x0027FA74 (complete 12-instruction function):
// r0=embedded object, r1=first literal, r2=second literal;
// BL 0x27E930 returns interface; r1=[embedded+4];
// vtable method at interface->vtable+0x84 (r0=interface,
// r1=embedded field, r2=original r1, r3=original r2).
// ARM32 VAs are tokens, never dereferenced as x86-64 pointers.
struct OriginalEmbeddedMethod84Call {
    std::uint32_t original_method_arm_va;
    std::uint32_t original_interface_arm_va;
    std::uint32_t embedded_field_plus_four;
    std::uint32_t caller_arg1;
    std::uint32_t caller_arg2;
};
using OriginalEmbeddedMethod84Dispatch = bool (*)(
    const OriginalEmbeddedMethod84Call&, void* context,
    std::uint32_t* out_original_return) noexcept;

// Native contract for the five virtual-method calls in original 0x27FB28.
// Only arguments actually defined by ARM instructions are marked known.
struct OriginalEmbeddedVirtualCall {
    std::uint32_t slot_offset;
    std::uint32_t original_method_arm_va;
    std::uint32_t original_interface_arm_va;
    std::uint32_t r1, r2, r3;
    std::uint8_t known_registers_mask;
    // For 0x27FAF4's +0x74 call, this maps the ARM stack-saved r3 word.
    std::uint32_t* saved_r3_stack_word;
};
struct OriginalEmbeddedVirtualResult {
    std::uint32_t r0;
    std::uint32_t r3_after;
};
using OriginalEmbeddedVirtualDispatch=bool (*)(
    const OriginalEmbeddedVirtualCall&, void* context,
    OriginalEmbeddedVirtualResult*) noexcept;

struct OriginalEmbeddedService {
    const void* registry_bss_slot;
    std::size_t registry_bss_slot_bytes;
    std::uint32_t registry_arm_va;
    const void* registry_object;
    std::size_t registry_object_bytes;
    std::uint32_t registry_vtable_arm_va;
    const void* registry_vtable;
    std::size_t registry_vtable_bytes;

    std::uint32_t interface_arm_va;
    const void* interface_object;
    std::size_t interface_object_bytes;
    std::uint32_t interface_vtable_arm_va;
    const void* interface_vtable;
    std::size_t interface_vtable_bytes;

    OriginalRegistryQueryDispatch query_registry;
    OriginalEmbeddedMethod84Dispatch call_method84;
    void* callback_context;
    OriginalEmbeddedVirtualDispatch call_full_virtual = nullptr;
};

enum class OriginalEmbeddedSetupStatus : std::uint8_t {
    InvalidMemory,
    MissingOriginalRegistry,
    RegistryLookupRefused,
    UnexpectedInterface,
    InvalidOriginalInterfaceVtable,
    MissingOriginalMethod84,
    OriginalMethod84Refused,
    Completed
};

OriginalEmbeddedSetupStatus original_execute_embedded_setup_0x27fa74(
    const void* embedded_host_bytes, std::size_t embedded_bytes,
    std::uint32_t caller_arg1_arm_va, std::uint32_t caller_arg2_arm_va,
    const OriginalEmbeddedService& services,
    std::uint32_t* original_return_arm_value) noexcept;
} // namespace ds_port::original_runtime
