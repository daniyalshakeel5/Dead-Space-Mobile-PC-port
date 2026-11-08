#!/usr/bin/env python3
"""Audit the actual translated EASTL vector<char>::DoInsertValues crash.

This instrumented wrapper calls the full original ARM-translated operation,
with unchanged arguments and original return. A log includes vector triplet,
insert iterator and requested count so malformed M3G stream values can be
traced back to their producer. It never fabricates a buffer or game object.
"""
import argparse
from pathlib import Path
import sys

ORIGINAL="u64 f_00325b04(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
RENAMED="static u64 ds_real_EASTL_vector_char_DoInsertValues(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
MARK="DS_REAL_EASTL_CHAR_INSERT_AUDIT_00325B04"
WRAPPER=r'''
/* DS_REAL_EASTL_CHAR_INSERT_AUDIT_00325B04
 * Guest layout: eastl::vector<char> { begin, end, capacity }
 * r1: insertion iterator; r2: number of bytes; r3: pointer to fill value.
 * These are the exact arguments passed by the original game loader.
 */
u64 f_00325b04(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    u32 begin=0, end=0, cap=0;
    int has_vec=(r0!=0);
    if (has_vec) {
        begin=LD32(r0);
        end=LD32(r0+4u);
        cap=LD32(r0+8u);
    }
    int bad=(!has_vec || end<begin || cap<end ||
             r1<begin || r1>end || (r2!=0 && r3==0) ||
             (u64)(end-begin)+(u64)r2 > (u64)0xffffffffu);
    if (getenv("DS_EASTL_CHAR_TRACE")) {
        fprintf(stderr,
          "[REAL_M3G_CHAR_INSERT] tid=%lu vec=%08x begin=%08x end=%08x "
          "cap=%08x iterator=%08x count=%u value=%08x malformed=%d\n",
          (unsigned long)pthread_self(),
          r0,begin,end,cap,r1,r2,r3,bad);
        fflush(stderr);
    }
    u64 rv=ds_real_EASTL_vector_char_DoInsertValues(r0,r1,r2,r3,sp);
    return rv;
}
'''
def patch(source):
    if MARK in source:
        if source.count(MARK)!=1: raise ValueError("Existing original EASTL trace marker count changed")
        return source
    if source.count(ORIGINAL)!=1: raise ValueError("Expected one exact original EASTL vector<char> insert")
    pos=source.index(ORIGINAL)
    if "eastl6vectorIcNS_9allocatorEE14DoInsertValues" not in source[pos:pos+260]:
        raise ValueError("Function symbol differs from the original crash")
    return "#include <stdio.h>\n#include <stdlib.h>\n#include <pthread.h>\n"+source.replace(ORIGINAL,RENAMED,1)+WRAPPER
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",required=True,type=Path)
    parser.add_argument("--instrument",action="store_true")
    a=parser.parse_args()
    try:
        before=a.source.read_text(encoding="utf8")
        after=patch(before)
        if a.instrument:
            backup=a.source.with_suffix(".c.pre-real-eastl-char-trace.bak")
            if not backup.exists():backup.write_text(before,encoding="utf8")
            a.source.write_text(after,encoding="utf8")
        print("[REAL_EASTL] Original vector<char> insertion verified and traced; no semantic changes")
        return 0
    except (OSError,ValueError) as err:
        print("[REAL_EASTL_TRACE_ERROR]",err,file=sys.stderr)
        return 2
if __name__=="__main__":raise SystemExit(main())
