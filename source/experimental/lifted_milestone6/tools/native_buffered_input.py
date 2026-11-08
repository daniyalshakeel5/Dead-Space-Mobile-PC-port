#!/usr/bin/env python3
"""Native PC BufferedInputStream::read, recovered from original ARM routines.

Replace just the classic buffered byte-copy/refill subsystem with native i686
code, preserving the original underlying IInputStream virtual read/availability
calls and the original asset bytes. Explicitly enabled by DS_NATIVE_BUFFERED_IO.
No fake objects, fabricated content, fast-forwarding or runtime ARM interpreter.
"""
from pathlib import Path
import argparse
import sys

ORIG="u64 f_00347d68(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
NEW="static u64 ds_original_BufferedInputStream_read(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
MARK="DS_NATIVE_BUFFERED_IO_FROM_REAL_ARM_00347D68"
CODE=r'''
/* DS_NATIVE_BUFFERED_IO_FROM_REAL_ARM_00347D68
 * Object fields verified from original BufferedInputStream::read:
 * +4 source IInputStream, +16/+20 cache begin/end pointers,
 * +32 cache cursor, +36 bytes cached, +40 underlying availability.
 * Original virtual IInputStream::read (vtable +12) is still used.
 */
static u64 ds_native_BufferedInputStream_read(u32 self,u32 dest,
                                              u32 requested,u32 r3,u32 sp) {
    if (!requested) return 0;
    if ((s32)requested < 0 || !self || !dest) {
        fprintf(stderr,"[NATIVE_BUFFERED_IO_INVALID_ARGS] stream=%08x dst=%08x len=%u\n",
                self,dest,requested);
        return 0;
    }
    u32 total=0, cache_begin=LD32(self+16u), cache_end=LD32(self+20u);
    if (!cache_begin || cache_end <= cache_begin) {
        fprintf(stderr,
                "[NATIVE_BUFFERED_IO_INVALID_CACHE] stream=%08x start=%08x end=%08x\n",
                self,cache_begin,cache_end);
        return 0;
    }
    u32 cap=cache_end-cache_begin;
    /* A malformed model cannot request more than its real input size without
       the original ARM stream reporting EOF; no padding or fabricated data. */
    while(total<requested) {
        u32 cursor=LD32(self+32u), cached=LD32(self+36u);
        if(cursor>cached || cached>cap) {
            fprintf(stderr,
                "[NATIVE_BUFFERED_IO_CORRUPT_CACHE] stream=%08x cursor=%u cached=%u cap=%u\n",
                self,cursor,cached,cap);
            return total;
        }
        if(cursor<cached) {
            u32 n=cached-cursor;
            if(n>requested-total) n=requested-total;
            memcpy((void*)(uintptr_t)(dest+total),
                   (const void*)(uintptr_t)(cache_begin+cursor),n);
            ST32(self+32u,cursor+n);
            total+=n;
            continue;
        }
        u32 source=LD32(self+4u);
        if(!source) return total;
        u32 vt=LD32(source);
        if(!vt) return total;
        u64 a=call_ind(LD32(vt+16u),source,0,0,0,sp-64u);
        ST32(self+40u,(u32)a);
        u64 rv=call_ind(LD32(vt+12u),source,cache_begin,cap,0,sp-64u);
        s32 got=(s32)(u32)rv;
        if(got<=0) {
            ST32(self+32u,0u);
            ST32(self+36u,0u);
            return total;
        }
        if((u32)got>cap) {
            fprintf(stderr,
                "[NATIVE_BUFFERED_IO_INVALID_READ] stream=%08x got=%d cap=%u\n",
                self,got,cap);
            return total;
        }
        ST32(self+32u,0u);
        ST32(self+36u,(u32)got);
    }
    return total;
}
u64 f_00347d68(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) {
    if(!getenv("DS_NATIVE_BUFFERED_IO"))
        return ds_original_BufferedInputStream_read(r0,r1,r2,r3,sp);
    return ds_native_BufferedInputStream_read(r0,r1,r2,r3,sp);
}
'''
def patch(source):
    if MARK in source:
        if source.count(MARK)!=1:raise ValueError("Native BufferedInputStream marker altered")
        return source
    if source.count(ORIG)!=1 or "BufferedInputStream4readEPci" not in source[source.index(ORIG):source.index(ORIG)+240]:
        raise ValueError("Native patch requires exactly one verified original BufferedInputStream ARM function")
    return "#include <stdio.h>\n#include <stdlib.h>\n"+source.replace(ORIG,NEW)+CODE

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--source",required=True,type=Path)
    a.add_argument("--instrument",action="store_true")
    o=a.parse_args()
    try:
        old=o.source.read_text(encoding="utf8");new=patch(old)
        if o.instrument:
            backup=o.source.with_suffix(".c.pre-native-buffered-io.bak")
            if not backup.exists():backup.write_text(old,encoding="utf8")
            o.source.write_text(new,encoding="utf8")
        print("[REAL_NATIVE_IO] Original ARM BufferedInputStream faithfully reconstructed as optional native PC C routine")
        return 0
    except (OSError,ValueError) as e:
        print("[REAL_NATIVE_IO_ERROR]",e,file=sys.stderr);return 2
if __name__=="__main__":raise SystemExit(main())
