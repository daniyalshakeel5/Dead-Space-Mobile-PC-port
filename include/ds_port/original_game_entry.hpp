#pragma once
#include <cstdint>
#include "ds_port/original_jni_exports.hpp"

// This original Java JNI export is an actual ARM-verified zero-return stub,
// NOT the game's main loop. The original wrapper calls a two-instruction
// MOV r0,#0 / BX LR native function and returns the result unchanged.
extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t
Java_com_ea_DeadSpace_DeadSpace_runEntryPoint(void* env, void* object) noexcept;
