#!/usr/bin/env python3
"""Build a native Win32 PE host from the actual lifted project's C glue.

CI mode uses only synthetic tables: *never* an EA ELF or translated game C.
Private build mode is handled by build_win32_lifted.sh on the owner's WSL.
"""
from __future__ import annotations
import os
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
TOOLS=ROOT/"experimental"/"lifted_milestone6"/"tools"
WIN=ROOT/"experimental"/"windows_native_host"
SOURCES=("host.c","rt.h","jni_vm.c","threads.c","libc_files.c",
         "gl_shim.c","front_sdl.c","windows_lifted_compat.h",
         "win32_import_bridge.h","win32_missing_imports.c","win32_missing_gles.c")
PROTOS= r'''#pragma once
#include "rt.h"
typedef u64 (*fn_t)(u32,u32,u32,u32,u32);
typedef struct {u32 addr; fn_t fn;} FnEnt;
typedef struct {const char* name; fn_t fn;} ImpEnt;
extern const FnEnt FNTAB[];
extern const u32 FNTAB_N;
extern const ImpEnt IMPTAB[];
extern const u32 IMPTAB_N;
extern const char* const JNI_NAMES[];
extern const u32 JNI_N;
u64 unlifted_called(u32 addr,const char* name);
u64 unimplemented_import(const char* name);
'''
TABLES = r'''#include "protos.h"
const FnEnt FNTAB[1] = {{0,0}}; const u32 FNTAB_N=0;
const ImpEnt IMPTAB[1] = {{0,0}}; const u32 IMPTAB_N=0;
const char* const JNI_NAMES[1] = {0}; const u32 JNI_N=0;
'''

# Exact missing imports from the owner's 2026-11-04 private APK link log.
# Synthetic import table forces a real PE32 linker resolution of all 28 symbols.
REQUIRED_IMPORTS=['__gnu_Unwind_Find_exidx','__cxa_type_match','__cxa_begin_cleanup','vsscanf','readdir_r','pthread_attr_setstack','sscanf','setsockopt','recvfrom','recv','sendto','send','socket','getpeername','getsockname','connect','bind','shutdown','getaddrinfo','freeaddrinfo','glPointSizePointerOES','glGetTexParameterxv','glGetTexEnvxv','glGetMaterialxv','glGetLightxv','glGetFixedv','glGetClipPlanex','glGetClipPlanef']
def require_imports():
    decl="".join(f"u64 imp_{name}(u32,u32,u32,u32,u32);\n" for name in REQUIRED_IMPORTS)
    tbl='const ImpEnt IMPTAB[] = {' + ','.join(f'{{"{n}",imp_{n}}}' for n in REQUIRED_IMPORTS) + '};\n'
    tbl+=f'const u32 IMPTAB_N={len(REQUIRED_IMPORTS)};'
    return decl,tbl

def run(args,cwd):
    print("RUN", " ".join(map(str,args)),flush=True)
    p=subprocess.run(args,cwd=cwd,capture_output=True,text=True)
    if p.returncode:
        print("STDOUT",p.stdout[-3000:])
        print("STDERR",p.stderr[-15000:])
        raise SystemExit(p.returncode)
    if p.stdout.strip():print(p.stdout[-3000:])
    if p.stderr.strip():print("WARN",p.stderr[-4000:])

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--sdl-root",type=Path,default=None)
    args=ap.parse_args()
    cc=os.environ.get("WINCC","i686-w64-mingw32-gcc-posix")
    with tempfile.TemporaryDirectory(prefix="dspc_win_host_") as t:
        root=Path(t)
        for name in SOURCES:shutil.copyfile(TOOLS/name,root/name)
        for name in ("win32_substrate.h","win32_substrate.c"):
            shutil.copyfile(WIN/name,root/name)
        decl, symtab = require_imports()
        (root/"protos.h").write_text(PROTOS + decl)
        (root/"tables.c").write_text(TABLES.replace(
            "const ImpEnt IMPTAB[1] = {{0,0}}; const u32 IMPTAB_N=0;",symtab))
        run([sys.executable,str(TOOLS/"gen_gl.py"),str(root/"gl_gen.c")],root)
        flags=[cc,"-O1","-std=gnu11","-w","-I.", "-fno-strict-aliasing"]
        run(flags+["-fsyntax-only","host.c"],root)
        run(flags+["-fsyntax-only","win32_substrate.c"],root)
        for name in ("win32_missing_imports","win32_missing_gles"):
            run(flags+["-include","win32_import_bridge.h","-c",
                 name+".c","-o",name+".o"],root)
        extras=["win32_missing_imports.o","win32_missing_gles.o"]
        run(flags+["-o","dspc_windows_host_test.exe","host.c",
                   "win32_substrate.c","tables.c"]+extras+
            ["-lm","-lwinpthread","-lws2_32","-lopengl32"],root)
        if args.sdl_root:
            sdk=args.sdl_root/"i686-w64-mingw32"
            inc=sdk/"include"/"SDL2"
            lib=sdk/"lib"
            if not (inc/"SDL.h").exists() or not lib.exists():
                raise SystemExit(f"SDL2 MinGW 32-bit SDK missing: {sdk}")
            sdlflags=flags+["-DDS_SDL","-DSDL_MAIN_HANDLED",
                            "-I"+str(inc)]
            run(sdlflags+["-fsyntax-only","host.c"],root)
            run(sdlflags+["-o","dspc_windows_sdl_host_test.exe",
                "host.c","win32_substrate.c","tables.c"]+extras+
                ["-L"+str(lib),"-lSDL2","-lm","-lwinpthread","-lws2_32","-lopengl32"],root)
            check_pe(root/"dspc_windows_sdl_host_test.exe")
        check_pe(root/"dspc_windows_host_test.exe")
    print("PASS: actual native i686 Windows host and 28 owner-reported imports link into PE32"
          " with original engine excluded",flush=True)

def check_pe(path: Path):
    import struct
    exe=path.read_bytes()
    assert exe[:2]==b"MZ"
    off=struct.unpack_from("<I",exe,0x3c)[0]
    assert exe[off:off+4]==b"PE\x00\x00"
    assert struct.unpack_from("<H",exe,off+4)[0]==0x14c
    print(f"PASS: verified PE32 native x86 {path.name} ({len(exe)} bytes)")
if __name__=="__main__":
    main()
