#!/usr/bin/env python3
"""Observe the exact real ARM DataInputStream::read 12-byte buffer and return.
The original translated method always executes unchanged, including its vtable.
No runtime simulation, overrides, fallback objects or alternative file parser.
"""
import argparse
from pathlib import Path
import sys

ORIGINAL="u64 f_00324884(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
RENAMED="static u64 ds_original_DataInputStream_read(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
MARK="DS_NATIVE_M3G_DATAINPUT_READ_BUFFER"
WRAPPER=r'''
/* DS_NATIVE_M3G_DATAINPUT_READ_BUFFER: wrapper observes, never substitutes. */
u64 f_00324884(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    u64 result=ds_original_DataInputStream_read(r0,r1,r2,r3,sp);
    if (getenv("DS_M3G_TRACE") && r2==12u) {
        u32 ds_substream = LD32(r0+4u);
        u32 ds_vtable = ds_substream ? LD32(ds_substream) : 0u;
        u32 ds_method = ds_vtable ? LD32(ds_vtable+12u) : 0u;
        fprintf(stderr,
                "[REAL_M3G_NATIVE_READ] tid=%lu input=%08x source=%08x "
                "vtbl=%08x virtual_read=%08x arm_offset=%08x dest=%08x "
                "requested=12 returned=%08x bytes=",
                (unsigned long)pthread_self(),r0,ds_substream,
                ds_vtable,ds_method,ds_method-g_base,r1,(unsigned)result);
        if (result==12) {
            for (u32 k=0;k<12;k++) fprintf(stderr,"%02x ",(unsigned)LD8(r1+k));
        }
        fprintf(stderr,"\n");
    }
    return result;
}
'''

def patch(source):
    if MARK in source:
        if source.count(MARK)!=1:raise ValueError("DataInputStream existing wrapper altered")
        return source
    if source.count(ORIGINAL)!=1:raise ValueError("Not precisely one real f_00324884 entry")
    if "DataInputStream4read" not in source[source.index(ORIGINAL):source.index(ORIGINAL)+220]:
        raise ValueError("Unexpected original native DataInputStream body")
    return "#include <stdio.h>\n#include <stdlib.h>\n#include <pthread.h>\n"+source.replace(ORIGINAL,RENAMED)+WRAPPER

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--source",required=True,type=Path)
    p.add_argument("--instrument",action="store_true")
    a=p.parse_args()
    try:
        old=a.source.read_text(encoding="utf8"); new=patch(old)
        if a.instrument:
            backup=a.source.with_suffix(".c.pre-m3g-datainput-audit.bak")
            if not backup.exists():backup.write_text(old,encoding="utf8")
            a.source.write_text(new,encoding="utf8")
        print("[REAL_M3G] Original native DataInputStream read result+12-byte buffer instrumented")
        return 0
    except (OSError,ValueError) as e:
        print("[REAL_M3G_NATIVE_READ_ERROR]",e,file=sys.stderr);return 2
if __name__=="__main__":raise SystemExit(main())
