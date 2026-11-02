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
add_executable(xperia_arm32_jit_smoke arm32_jit_smoke.cpp)
target_compile_features(xperia_arm32_jit_smoke PRIVATE cxx_std_20)
target_link_libraries(xperia_arm32_jit_smoke PRIVATE dynarmic)
add_test(NAME xperia_arm32_jit_synthetic_a32 COMMAND xperia_arm32_jit_smoke)
# First joined ARM32 path: ELF PT_LOAD + relative relocations +
# dynamic JIT, all from original-free synthetic instructions.
add_library(xperia_guest_elf32 STATIC ../xperia_guest_loader/guest_elf32.cpp)
target_include_directories(xperia_guest_elf32 PUBLIC ../xperia_guest_loader)
add_executable(xperia_elf_jit_synthetic elf_mapped_jit_smoke.cpp)
target_compile_features(xperia_elf_jit_synthetic PRIVATE cxx_std_20)
target_link_libraries(xperia_elf_jit_synthetic PRIVATE
    xperia_guest_elf32 dynarmic)
add_test(NAME xperia_elf_mapped_a32_jit COMMAND xperia_elf_jit_synthetic)

# ARM guest service integration: real A32 BLX -> SVC -> native memcpy
# -> guest BX LR; service code is original-free and fail-closed.
add_library(xperia_arm_guest_libc STATIC ../xperia_guest_loader/guest_libc_bridge.cpp ../xperia_guest_loader/guest_heap.cpp)
target_include_directories(xperia_arm_guest_libc PUBLIC ../xperia_guest_loader)
target_link_libraries(xperia_arm_guest_libc PUBLIC xperia_guest_elf32)
add_executable(xperia_guest_libc_jit_smoke guest_libc_jit_smoke.cpp)
target_compile_features(xperia_guest_libc_jit_smoke PRIVATE cxx_std_20)
target_link_libraries(xperia_guest_libc_jit_smoke PRIVATE
    xperia_arm_guest_libc dynarmic)
add_test(NAME xperia_jit_bionic_libc_a32_svc_roundtrip
    COMMAND xperia_guest_libc_jit_smoke)
