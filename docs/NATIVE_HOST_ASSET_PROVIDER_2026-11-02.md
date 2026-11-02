# Native original-APK file-backed provider — 2026-11-02

This document describes **native Windows/Linux game resource I/O used by
`deadspace_native`**, not an emulator, ARM loader or stand-in resource format.
All game data is read from the user's installed APK assets. None is distributed.

## Grounded ARM32 behavior

Original game lib ELF in the user's confirmed APK, verified through
[remote original-APK evidence run 41426559665](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41426559665):

- Provider virtual read at original `0x2716F8`: libc `read` via original
  `0x35554`. Original provider fd word at +`0x10`; mode at +`0xA4`.
- Provider seek at `0x273034`: libc `lseek` via `0x35350`. The exact
  original ARM dispatch table at **`0x0042F0F4`** contains 1 and 2.
  Verified modes: 0→`SEEK_SET`, 1→`SEEK_CUR`, 2→`SEEK_END`.
  The original SUB/CMP/MOVHI path sends other modes to `SEEK_SET`.
  A failed lseek writes errno to provider+`0xA0` and returns zero.
- File-backed size at `0x2763AC`: `lseek(fd,0,SEEK_CUR)`, followed by
  `lseek(fd,0,SEEK_END)`, then `lseek(fd,prior,SEEK_SET)`.
  The returned value is file length; on either initial seek failure
  the game records errno at provider+`0xA0` and returns 0xFFFFFFFF.
  The last restore result is ignored as in original instructions.
- File-position selector 0 at `0x271794` calls `lseek(fd,0,SEEK_CUR)`.

These native host-backed translations live in
`src/original_embedded_provider_file.cpp`.
`tools/deadspace_seek_table_probe.py` validates the exact ARM branch
instructions and resolves the read-only origin table from the user's ELF.
The probe fails closed if expected instructions/table values change.

## Real game boot integration

`src/original_host_asset_io.cpp` implements a bounded, fail-closed
native read path: `resolve_game_asset_path` validates the installed
root and relative resource paths; the host opens the actual local
file; a temporary original-layout provider buffer is explicitly bound
to the opened native fd; native size, absolute seek and read are invoked
with their verified original semantics. Reads complete in 1 MiB chunks
with short-read handling, and no partial file data survives a failure.
The loader refuses files outside caller-specified limits (maximum 64 MiB).

Both `apps/deadspace_native.cpp` and `src/original_splash_host.cpp`
use this shared loader. Consequently the actual original EA/Dead Space
splashes, 3D main-menu geometry, original frontend layout and texture
packs are now loaded by this provider rather than independent
`std::ifstream` paths.

Synthetic fixture and system-file tests:
- `ds_original_arm_provider_file_read_seek`: original file read,
  SEEK_SET/CUR/END, end-relative negative offsets, file length,
  descriptor binding and failure status
- `ds_original_arm_file_provider_boot_loader`: actual native
  path resolver → open → provider length → seek → read; bounds,
  unsafe paths, empty and missing files

The original APK OpenGL splash/menu workflow passed with the
integrated boot loader in
[41427434538](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41427434538)
and [41427444021](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41427444021).

## Important remaining boundaries

The temporary provider struct is a **host I/O mapping**, not a claim
that original ownership, allocator, vtable dispatch, provider
construction or full resource-registration/parser pipelines have been
recovered. Memory-mode provider codec/seek branches still need real
original implementations. The original active engine and gameplay
remain incomplete; successful resource display does not prove gameplay
boot or original engine stage-one completion.

Further work must wire recovered resource owner/factory to these native
host-backed operations, then prove original game state transitions,
level loading and active-frame simulation from the APK. Do not fake
service readiness or ship original copyrighted game assets.
