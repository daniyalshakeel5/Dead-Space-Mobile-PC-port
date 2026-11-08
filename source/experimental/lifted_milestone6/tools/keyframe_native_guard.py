#!/usr/bin/env python3
"""Audit authentic M3G KeyframeSequence counts and allocation results.

No branch, decoder, buffer or game asset is changed. This instruments the
actual translated setKeyframeSize(int count,int components) in the owner's APK.
"""
from pathlib import Path
import argparse
import re
import sys

FUNC="f_00365168"
MARK="DS_NATIVE_REAL_KEYFRAME_SEQUENCE_ALLOC_00365168"
FR=re.compile(r"(?m)^u64 (f_[0-9a-f]{8})\(")

def patch(s):
    if MARK in s:
        if s.count(MARK)!=3: raise ValueError("Prior Keyframe audit markers altered")
        return s
    hits=list(FR.finditer(s))
    idx=[i for i,hit in enumerate(hits) if hit.group(1)==FUNC]
    if len(idx)!=1: raise ValueError("Expected exactly one original M3G KeyframeSequence method")
    k=idx[0]; lo=hits[k].start(); hi=hits[k+1].start() if k+1<len(hits) else len(s)
    f=s[lo:hi]
    if "KeyframeSequence15setKeyframeSizeEii" not in f[:220]:
        raise ValueError("Unexpected translated original M3G method")
    saved='''  {
    u32 t = r1;
    r5 = t;
  }'''
    allocated='''  { u64 rv = call_ind(LD32(r12 + 8u), r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }'''
    fcall='''  { u64 rv = f_003650b0(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }'''
    if any(f.count(x)!=1 for x in (saved, allocated, fcall)):
        raise ValueError("Native M3G keyframe allocator sequence changed")
    f=f.replace(saved,saved+r'''
  /* DS_NATIVE_REAL_KEYFRAME_SEQUENCE_ALLOC_00365168 */
  fprintf(stderr,
      "[REAL_M3G_KEYFRAME_REQUEST] tid=%lu this=%08x count=%u components=%u "
      "value_ptr=%08x keyframe_ptr=%08x\n",
      (unsigned long)pthread_self(),(unsigned)r4,
      (unsigned)r5,(unsigned)r2,
      (unsigned)LD32(r4+116u),(unsigned)LD32(r4+88u));
''',1)
    f=f.replace(allocated,allocated+r'''
  /* DS_NATIVE_REAL_KEYFRAME_SEQUENCE_ALLOC_00365168 */
  fprintf(stderr,
      "[REAL_M3G_KEYFRAME_ALLOC] this=%08x result=%08x count=%u alloc_size=%u\n",
      (unsigned)r4,(unsigned)r0,(unsigned)r5,(unsigned)((r5+1u)<<2));
''',1)
    f=f.replace(fcall,fcall+r'''
  /* DS_NATIVE_REAL_KEYFRAME_SEQUENCE_ALLOC_00365168 */
  fprintf(stderr,
      "[REAL_M3G_KEYFRAME_STORAGE] this=%08x keyframe_ptr=%08x "
      "time_index_first=%u last=%u value_ptr=%08x\n",
      (unsigned)r4,(unsigned)LD32(r4+88u),
      (unsigned)LD32(r4+80u),(unsigned)LD32(r4+84u),
      (unsigned)LD32(r4+116u));
''',1)
    return "#include <stdio.h>\n#include <pthread.h>\n"+s[:lo]+f+s[hi:]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source",type=Path,required=True)
    ap.add_argument("--instrument",action="store_true")
    a=ap.parse_args()
    try:
        old=a.source.read_text(encoding="utf8")
        new=patch(old)
        if a.instrument:
            backup=a.source.with_suffix(".c.pre-keyframe-alloc-probe.bak")
            if not backup.exists():backup.write_text(old,encoding="utf8")
            a.source.write_text(new,encoding="utf8")
        print("[REAL_M3G] Original KeyframeSequence native counts and allocations traced")
        return 0
    except (ValueError,OSError) as e:
        print("[REAL_M3G_KEYFRAME_TRACE_ERROR]",e,file=sys.stderr)
        return 2
if __name__=="__main__":raise SystemExit(main())
