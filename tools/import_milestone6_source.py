#!/usr/bin/env python3
"""Import ONLY milestone-6's checked, asset-free host/tooling sources.

This is a GitHub Actions helper. It never copies a native ELF, APK, resource
pack, generated ARM-lifted fn_*.c, or binary. The original RAR remains intact.
"""
from pathlib import Path
import shutil
import sys
import tempfile
from analyze_milestones import extract_rar, snapshot

DEST = Path("experimental/lifted_milestone6")
SAFE = {
    ".vscode/extensions.json", ".vscode/launch.json", ".vscode/settings.json",
    "CMakeLists.txt", "CMakePresets.json", "README.md", "src/main.cpp",
    "tools/bt.py", "tools/build.sh", "tools/dexdump.py", "tools/difftest.py",
    "tools/emu_run.py", "tools/front_sdl.c", "tools/gen.py", "tools/gen_all.py",
    "tools/gen_gl.py", "tools/gl_shim.c", "tools/harness.c", "tools/host.c",
    "tools/jni_vm.c", "tools/jninames.py", "tools/libc_files.c", "tools/lift.py",
    "tools/loader.py", "tools/overrides.py", "tools/rt.h", "tools/threads.c",
    "tools/trace_emu.py", "vcpkg.json",
}

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: import_milestone6_source.py dspc_milestone6.rar")
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        warnings=extract_rar(Path(sys.argv[1]),root)
        sources=snapshot(root)
        if set(sources) != SAFE:
            raise SystemExit("Archive unexpectedly differs from verified 29-file "
                             f"source manifest: missing={sorted(SAFE-set(sources))}"
                             f" extra={sorted(set(sources)-SAFE)}")
        if warnings and any("INCOMPLETE" in x for x in warnings):
            raise SystemExit("Refusing to import incomplete RAR entries")
        for name, source in sorted(sources.items()):
            dest=DEST/name
            dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(source,dest)
            print(f"imported {dest.as_posix()} ({dest.stat().st_size} bytes)")
    print("Imported 29 asset-free milestone 6 files, no proprietary resources.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
