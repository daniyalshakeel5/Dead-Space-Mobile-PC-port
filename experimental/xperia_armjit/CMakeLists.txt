cmake_minimum_required(VERSION 3.20)
project(xperia_arm32_cpu_jit LANGUAGES C CXX ASM)
include(CTest)
include(FetchContent)
set(DYNARMIC_TESTS OFF CACHE BOOL "" FORCE)
set(DYNARMIC_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(DYNARMIC_USE_BUNDLED_EXTERNALS ON CACHE BOOL "" FORCE)
set(DYNARMIC_FRONTENDS "A32" CACHE STRING "" FORCE)
set(DYNARMIC_USE_LLVM OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  dynarmic
  GIT_REPOSITORY https://github.com/lioncash/dynarmic.git
  GIT_TAG a41c380246d3d9f9874f0f792d234dc0cc17c180
  GIT_SHALLOW FALSE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(dynarmic)
if(WIN32)
  # Windows-only original Xperia graphics/audio/controller runtime.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(sdl2
    URL https://www.libsdl.org/release/SDL2-2.30.12.tar.gz)
  FetchContent_MakeAvailable(sdl2)
  find_package(OpenGL REQUIRED)
endif()
add_executable(xperia_arm32_jit_smoke arm32_jit_smoke.cpp)
target_compile_features(xperia_arm32_jit_smoke PRIVATE cxx_std_20)
target_link_libraries(xperia_arm32_jit_smoke PRIVATE dynarmic)
add_test(NAME xperia_arm32_jit_synthetic_a32 COMMAND xperia_arm32_jit_smoke)
# First joined ARM32 path: ELF PT_LOAD + relative relocations +
# dynamic JIT, all from original-free synthetic instructions.
add_library(xperia_guest_elf32 STATIC ../xperia_guest_loader/guest_elf32.cpp)
target_include_directories(xperia_guest_elf32 PUBLIC ../xperia_guest_loader)
add_executable(xperia_guest_elf32_tests
    ../xperia_guest_loader/guest_elf32_tests.cpp)
target_compile_features(xperia_guest_elf32_tests PRIVATE cxx_std_17)
target_link_libraries(xperia_guest_elf32_tests PRIVATE xperia_guest_elf32)
add_test(NAME xperia_original_elf32_symbol_types_and_relocations
    COMMAND xperia_guest_elf32_tests)
add_executable(xperia_elf_jit_synthetic elf_mapped_jit_smoke.cpp)
target_compile_features(xperia_elf_jit_synthetic PRIVATE cxx_std_20)
target_link_libraries(xperia_elf_jit_synthetic PRIVATE
    xperia_guest_elf32 dynarmic)
add_test(NAME xperia_elf_mapped_a32_jit COMMAND xperia_elf_jit_synthetic)

# ARM guest service integration: real A32 BLX -> SVC -> native memcpy
# -> guest BX LR; service code is original-free and fail-closed.
add_library(xperia_arm_guest_libc STATIC ../xperia_guest_loader/guest_libc_bridge.cpp ../xperia_guest_loader/guest_heap.cpp ../xperia_guest_loader/guest_filesystem.cpp ../xperia_guest_loader/guest_jni_vm.cpp ../xperia_guest_loader/guest_bionic_data.cpp)
target_include_directories(xperia_arm_guest_libc PUBLIC ../xperia_guest_loader)
target_compile_features(xperia_arm_guest_libc PUBLIC cxx_std_17)
target_link_libraries(xperia_arm_guest_libc PUBLIC xperia_guest_elf32)
add_executable(xperia_guest_libc_jit_smoke guest_libc_jit_smoke.cpp)
target_compile_features(xperia_guest_libc_jit_smoke PRIVATE cxx_std_20)
target_link_libraries(xperia_guest_libc_jit_smoke PRIVATE
    xperia_arm_guest_libc dynarmic)
add_test(NAME xperia_jit_bionic_libc_a32_svc_roundtrip
    COMMAND xperia_guest_libc_jit_smoke)

# Exercise the same Bionic/ELF/host-filesystem implementation directly
# alongside the translated ARM call-runner on the Windows CI target.
add_executable(xperia_guest_abi_tests
    ../xperia_guest_loader/guest_libc_bridge_tests.cpp)
target_link_libraries(xperia_guest_abi_tests PRIVATE xperia_arm_guest_libc)
add_test(NAME xperia_guest_bionic_cpp_lifetime_and_errno
    COMMAND xperia_guest_abi_tests)

add_executable(xperia_guest_filesystem_tests ../xperia_guest_loader/guest_filesystem_tests.cpp)
target_link_libraries(xperia_guest_filesystem_tests PRIVATE xperia_arm_guest_libc)
add_test(NAME xperia_guest_original_appbundle_asset_io COMMAND xperia_guest_filesystem_tests)

add_executable(xperia_guest_bionic_data_tests ../xperia_guest_loader/guest_bionic_data_tests.cpp)
target_link_libraries(xperia_guest_bionic_data_tests PRIVATE xperia_arm_guest_libc)
add_test(NAME xperia_guest_bionic_data_symbols COMMAND xperia_guest_bionic_data_tests)

add_executable(xperia_guest_jni_vm_tests ../xperia_guest_loader/guest_jni_vm_tests.cpp)
target_link_libraries(xperia_guest_jni_vm_tests PRIVATE xperia_arm_guest_libc)
add_test(NAME xperia_guest_jni_vm_aapcs_tables COMMAND xperia_guest_jni_vm_tests)

# Reusable original ARM lifecycle call runner with explicit exit trampoline,
# a real stack and service-trap dispatcher. No copyrighted ARM code.
add_library(xperia_arm_call_runner STATIC
    guest_arm_call_runner.cpp guest_xperia_input.cpp
    ../../src/dualshock4_input.cpp ../../src/original_ui_interaction.cpp)
target_include_directories(xperia_arm_call_runner PUBLIC ../../include)
if(WIN32)
  target_sources(xperia_arm_call_runner PRIVATE
    guest_sdl_host.cpp guest_texture_decoder.cpp
    third_party/powervr/PVRTDecompress.cpp)
  if(TARGET SDL2::SDL2-static)
    set(XPERIA_SDL2_TARGET SDL2::SDL2-static)
  elseif(TARGET SDL2::SDL2)
    set(XPERIA_SDL2_TARGET SDL2::SDL2)
  else()
    message(FATAL_ERROR "SDL2 target not found in pinned SDL build")
  endif()
  target_link_libraries(xperia_arm_call_runner PUBLIC ${XPERIA_SDL2_TARGET} OpenGL::GL)
endif()
add_executable(xperia_texture_decoder_tests
    guest_texture_decoder_tests.cpp guest_texture_decoder.cpp
    third_party/powervr/PVRTDecompress.cpp)
target_compile_features(xperia_texture_decoder_tests PRIVATE cxx_std_17)
add_test(NAME xperia_ATC_PVRTC_RGBA_and_payload_validation
    COMMAND xperia_texture_decoder_tests)
add_executable(xperia_guest_input_tests guest_xperia_input_tests.cpp)
target_link_libraries(xperia_guest_input_tests PRIVATE xperia_arm_call_runner)
add_test(NAME xperia_ds4_real_arm_jni_input_delivery
    COMMAND xperia_guest_input_tests)
target_compile_features(xperia_arm_call_runner PUBLIC cxx_std_20)
target_link_libraries(xperia_arm_call_runner PUBLIC xperia_arm_guest_libc dynarmic)
add_executable(xperia_arm_call_runner_tests guest_arm_call_runner_tests.cpp)
target_compile_features(xperia_arm_call_runner_tests PRIVATE cxx_std_20)
target_link_libraries(xperia_arm_call_runner_tests PRIVATE xperia_arm_call_runner)
add_test(NAME xperia_arm32_call_runner_and_constructor_order
    COMMAND xperia_arm_call_runner_tests)

# Loads the user's original Xperia Play ELF from THEIR disk at runtime
# and reports the exact missing Android/JNI/GLES import names.
add_executable(xperia_guest_import_report xperia_guest_import_report.cpp)
target_compile_features(xperia_guest_import_report PRIVATE cxx_std_20)
target_link_libraries(xperia_guest_import_report PRIVATE xperia_arm_guest_libc)

# Original user-supplied Xperia ELF startup: fail-closed if Android imports
# are missing; only execute authentic game constructors when fully bound.
add_executable(xperia_guest_startup xperia_guest_startup.cpp)
target_compile_features(xperia_guest_startup PRIVATE cxx_std_20)
target_link_libraries(xperia_guest_startup PRIVATE xperia_arm_call_runner)
