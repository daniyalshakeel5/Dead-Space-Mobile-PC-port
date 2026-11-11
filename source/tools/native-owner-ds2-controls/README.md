# Owner's native Dead Space 2-style gamepad patch (2026-11-09)

This directory contains the **reproducible patch source**, not the original
game or the user-supplied native executable. The original packaged Linux build
is a different native-controller implementation than the earlier
`source/experimental/lifted_milestone6/tools/front_sdl.c` frontend.

## Exact original binary fingerprint

- Target: Linux i686, ELF32 x86, native controller build from the user-uploaded
  `DeadSpaceMobile-Linux-Touchscreen controller support.zip`
- Original binary SHA256: `fc1257844846043f6df78d01193d30b75a76e6022d025cd90cf17c7f4312bfa9`
- Patched binary SHA256: `3e1a8e8088c07cf646587f1b7a34a8ab398aa1fefc748c9c89ff44e291c95e80`
- No donor APK, game assets, saved games or copyrighted native binary are committed.

## Building the exact patch

```bash
python3 source/tools/native-owner-ds2-controls/patch_native_elf32.py \
  /path/to/unpacked-original/dspc_sdl /path/to/package/dspc_sdl
gcc -m32 -O2 -fPIC -shared -nostdlib \
  -Wl,-soname,libds2controls.so -Wl,-z,relro -Wl,-z,now \
  -o /path/to/package/libds2controls.so \
  source/tools/native-owner-ds2-controls/libds2controls.c
```

The original shell launcher must export
`LD_PRELOAD="$ROOT/libds2controls.so${LD_PRELOAD:+:$LD_PRELOAD}"` before
launching `dspc_sdl`. The test ZIP already includes a patched launcher,
the source code of the shim, a binary and full control notes.

## Native behavior

Physical R1 is native alternate fire while L2 aiming and native Plasma Saw
melee while not aiming. L3 becomes the existing 180 turn; L1 becomes run.
Triangle in aim is native Stasis, R3 becomes locator. R2 retains native fire
when aimed and stomp when not aimed.

**Zero-G Jump update (2026-11-09):** The original controller mapped
Zero-G Jump (native action `0x352fb9f`) to D-pad Down, while D-pad Up
was Locator (native action `0x352fb95`). The SDL shim now maps physical
**D-pad Up to native Zero-G Jump**, preserves **R3 as Locator**, and leaves
physical D-pad Down unassigned. All other control mappings, the game
executable and the existing alternate-fire action are unchanged.
The new remapper passed 14 isolated button-state checks; in-game Zero-G
jump still requires physical gameplay verification.

The three exact-byte patches preserve the existing unarmed **aim safety**:
no-weapon branches still avoid dereferencing an absent equipped weapon in
`setAiming`. Other native actions and the stomp event now continue to dispatch
before collecting the Plasma Cutter.

**Validation limits:** the patcher uses both a complete original-file hash
and exact opcode guards; the ELF was disassembled after patching and the remap
passed isolated input-mapping checks. Original-game runtime/hardware
verification has not yet been performed on this patched build. Do not merge
it as gameplay-verified until the owner tests it.
