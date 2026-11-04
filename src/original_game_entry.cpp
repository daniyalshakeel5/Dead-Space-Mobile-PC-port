#include "ds_port/original_game_entry.hpp"

extern "C" DS_ORIGINAL_JNI_EXPORT std::int32_t
Java_com_ea_DeadSpace_DeadSpace_runEntryPoint(void*, void*) noexcept {
    // Original ELF vaddr 0x00035de8: JNIEnv* and jobject are unused, r0/r1
    // are set to 0, and native callee at 0x0027e974 executes MOV r0,#0;
    // BX LR. The original wrapper returns that same 0, without side effects
    // outside its own stack. Verified from the actual APK's ARM instructions.
    return 0;
}
