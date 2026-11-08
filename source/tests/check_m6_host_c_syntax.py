#!/usr/bin/env python3
"""Compile milestone 6 host and texture instrumentation without original EA files.

Generates only stub declarations for the translated game; no game data, binary
image, ARM instruction translation, or proprietary generated fn_*.c is needed.
This is a syntax-only smoke test, not game boot certification.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
TOOLS=ROOT/"experimental"/"lifted_milestone6"/"tools"
CFILES=("host.c","rt.h","jni_vm.c","threads.c","libc_files.c","gl_shim.c","front_sdl.c","linux_accel.h","audio_sdl.h","texture_hd.h","texture_ktx.h")

PROTOS=r'''
#pragma once
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
u64 unlifted_called(u32 addr, const char* name);
u64 unimplemented_import(const char* name);
'''

def check_run(argv, cwd):
    proc=subprocess.run(argv,cwd=cwd,text=True,capture_output=True)
    if proc.returncode:
        print("CHECK FAILED", " ".join(argv))
        print(proc.stdout[-2000:])
        print(proc.stderr[-10000:])
        raise SystemExit(proc.returncode)
    print("PASS", " ".join(argv))

def main():
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        for path in CFILES:
            shutil.copyfile(TOOLS/path,root/path)
        (root/"protos.h").write_text(PROTOS)
        check_run([sys.executable,str(TOOLS/"gen_gl.py"),"gl_gen.c"],root)
        cc=os.environ.get("CC","cc")
        # Includes the full JNI VM, so all controller and AssetManager changes
        # are syntax checked against the same host C translation unit.
        check_run([cc,"-m32","-std=gnu11","-w","-fsyntax-only",
                   "-DCOMPARE_MODE","host.c"],root)
        # This additionally includes full resource IO, OpenGL, threading.
        check_run([cc,"-m32","-std=gnu11","-w","-fsyntax-only",
                   "host.c"],root)
        # Exercise the ACTUAL JNI VM, not a simplified copy. All original
        # game code/ELF calls are in unused sections discarded at link time.
        shutil.copyfile(ROOT/"tests"/"test_m6_jni_threads_main.c",
                        root/"test_m6_jni_threads_main.c")
        check_run([cc,"-m32","-std=gnu11","-O1","-w","-pthread",
                   "-ffunction-sections","-fdata-sections","-no-pie",
                   "test_m6_jni_threads_main.c","-Wl,--gc-sections",
                   "-lm","-ldl","-o","test_m6_jni_threads"],root)
        check_run([str(root/"test_m6_jni_threads")],root)
    print("Milestone 6 host C syntax and JNI worker tests PASS (no original game files)")

if __name__=="__main__":
    main()
