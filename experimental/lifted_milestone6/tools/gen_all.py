#!/usr/bin/env python3
"""Whole-program generation: all lifted functions split into chunks + prototypes + dispatch tables."""
import os, sys, shutil
from lift import *

SO = os.environ.get("DS_SO", "../apk/lib/armeabi/libDeadSpace.so")
OUT = sys.argv[1] if len(sys.argv) > 1 else "out"
CHUNK = int(os.environ.get("CHUNK_KB", "900")) * 1024

def main():
    img = Image(SO)
    ok, bad = lift_all(img)
    shutil.rmtree(OUT, ignore_errors=True); os.makedirs(OUT)
    all_funcs = {a: n for a, (s, n) in img.funcs.items()}
    all_funcs.update({a: n for a, (s, n) in img.thumb.items()})
    from overrides import OVERRIDES
    all_funcs.update(img.override)
    unlifted = sorted(a for a in all_funcs if a not in ok and a not in img.override)
    DATA = {"__sF", "__stack_chk_guard", "_ctype_", "_tolower_tab_", "_toupper_tab_", "__page_size", "__dso_handle", "tzname"}
    dynsym = img.elf.get_section_by_name(".dynsym")
    imps = sorted({sym.name for sym in dynsym.iter_symbols()
                   if sym["st_shndx"] == "SHN_UNDEF" and sym.name and sym.name not in DATA and not sym.name.startswith(("_ZTV", "_ZTI", "_ZTS"))}
                  | set(img.plt.values()))

    with open(f"{OUT}/protos.h", "w") as f:
        f.write('#pragma once\n#include "rt.h"\n')
        f.write("typedef u64 (*fn_t)(u32,u32,u32,u32,u32);\n")
        for a in sorted(all_funcs): f.write(f"u64 f_{a:08x}(u32,u32,u32,u32,u32);\n")
        for n in imps: f.write(f"u64 imp_{n}(u32,u32,u32,u32,u32);\n")
        f.write("typedef struct { u32 addr; fn_t fn; } FnEnt;\ntypedef struct { const char* name; fn_t fn; } ImpEnt;\n")
        f.write("extern const FnEnt FNTAB[]; extern const u32 FNTAB_N;\nextern const ImpEnt IMPTAB[]; extern const u32 IMPTAB_N;\n")
        f.write("extern const char* const JNI_NAMES[]; extern const u32 JNI_N;\n")
        f.write("u64 unlifted_called(u32 addr, const char* name);\nu64 unimplemented_import(const char* name);\n")

    # lifted functions -> chunks
    idx, cur, size = 0, [], 0
    def flush():
        nonlocal idx, cur, size
        if not cur: return
        with open(f"{OUT}/fn_{idx:03d}.c", "w") as f:
            f.write('#include "protos.h"\n' + "\n".join(cur) + "\n")
        idx += 1; cur, size = [], 0
    for a in sorted(ok):
        code = ok[a][0]
        cur.append(code); size += len(code)
        if size >= CHUNK: flush()
    flush()

    with open(f"{OUT}/overrides.c", "w") as f:
        f.write('#include "protos.h"\n')
        for a, n in sorted(img.override.items()):
            f.write(f"u64 f_{a:08x}(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){{ /* {n} */ {OVERRIDES[n]} }}\n")

    with open(f"{OUT}/unlifted.c", "w") as f:
        f.write('#include "protos.h"\n')
        for a in unlifted:
            n = all_funcs[a].replace('"', "")[:80]
            f.write(f'u64 f_{a:08x}(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){{ return unlifted_called(0x{a:x}u, "{n}"); }}\n')

    with open(f"{OUT}/tables.c", "w") as f:
        f.write('#include "protos.h"\n')
        for n in imps:
            f.write(f'__attribute__((weak)) u64 imp_{n}(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){{ return unimplemented_import("{n}"); }}\n')
        f.write("const FnEnt FNTAB[] = {\n")
        for a in sorted(all_funcs): f.write(f"  {{0x{a:x}u, f_{a:08x}}},\n")
        f.write(f"}};\nconst u32 FNTAB_N = {len(all_funcs)};\nconst ImpEnt IMPTAB[] = {{\n")
        for n in imps: f.write(f'  {{"{n}", imp_{n}}},\n')
        f.write(f"}};\nconst u32 IMPTAB_N = {len(imps)};\n")
        import sys as _s; _s.path.insert(0, ".."); _s.path.insert(0, ".")
        from jninames import JNI_NAMES
        f.write("const char* const JNI_NAMES[] = {" + ",".join('"%s"' % n for n in JNI_NAMES) + "};\n")
        f.write(f"const u32 JNI_N = {len(JNI_NAMES)};\n")
    import gen_gl; gen_gl.main(f"{OUT}/gl_gen.c")
    for fn in ("rt.h", "host.c", "jni_vm.c", "threads.c", "libc_files.c", "gl_shim.c", "front_sdl.c", "windows_lifted_compat.h", "build.sh"): shutil.copy(fn, OUT)
    # Preserve the exact guest translation. Add a fail-closed two-point probe
    # only to the confirmed TexturePack::load NULL-vector site. Do not mask
    # the exception or invent empty textures. DS_TEXTUREPACK_DIAG=0 disables.
    if os.environ.get("DS_TEXTUREPACK_DIAG", "1").lower() not in ("0", "no", "false", "off"):
        from texturepack_fault_guard import patch as patch_texturepack
        from pathlib import Path as _Path
        candidates = [p for p in _Path(OUT).glob("fn_*.c")
                      if "u64 f_0035b2e0(" in p.read_text(encoding="utf-8")]
        if len(candidates) != 1:
            raise RuntimeError(
                f"TexturePack probe requires one f_0035b2e0 translation; "
                f"found {len(candidates)}. Check the original game library.")
        p = candidates[0]
        original = p.read_text(encoding="utf-8")
        p.write_text(patch_texturepack(original), encoding="utf-8")
        print(f"TexturePack null-vector diagnostics injected into {p}")
    tot = sum(os.path.getsize(f"{OUT}/{x}") for x in os.listdir(OUT))
    print(f"{len(ok)} lifted, {len(unlifted)} unlifted stubs, {len(imps)} imports, {idx} chunks, {tot/1e6:.1f} MB of C")

if __name__ == "__main__":
    main()
