#!/usr/bin/env python3
"""Use the original model cache caller's existing NULL-result path.

The authentic engine's f_0006223c calls Object3D::duplicate on a null source
when a resource model lacks the requested subnode. The very next instructions
already handle a null *return value*. In opt-in repair mode, treat a null
source as that existing null return rather than dereference address zero.
No fabricated game object, model replacement or substitute asset.
"""
from pathlib import Path
import argparse
import re
import sys

NAME="f_0006223c"
MARK="DS_REAL_MODELCACHE_MISSING_DUPLICATE_SOURCE_0006223C"
CALL="{ u64 rv = f_00368efc(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
FN=re.compile(r"(?m)^u64 (f_[0-9a-f]{8})\(")
PROBE=r'''
  /* DS_REAL_MODELCACHE_MISSING_DUPLICATE_SOURCE_0006223C */
  if (r0 == 0u && getenv("DS_MODELCACHE_NULL_SOURCE_RETURN")) {
    fprintf(stderr,
      "[REAL_MODELCACHE_MISSING_SUBNODE] container=%08x nodevector=%08x "
      "selected=00000000: taking pre-existing original NULL result branch\n",
      (unsigned)r6, (unsigned)LD32(r6));
    r1=0;
  } else {
    { u64 rv = f_00368efc(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  }
'''
def patch(source):
    fs=list(FN.finditer(source))
    hits=[i for i,m in enumerate(fs) if m.group(1)==NAME]
    if len(hits)!=1: raise ValueError("Expected exactly one original model duplication caller")
    i=hits[0];lo=fs[i].start();hi=fs[i+1].start() if i+1<len(fs) else len(source)
    body=source[lo:hi]
    if MARK in body:
        if body.count(MARK)!=1:raise ValueError("Duplicate model caller guard marker")
        return source
    if body.count(CALL)!=1 or 'L_62338:;' not in body:
        raise ValueError("Original model duplication return failure branch changed; refusing modification")
    source=source[:lo]+body.replace(CALL,PROBE,1)+source[hi:]
    return "#include <stdio.h>\n#include <stdlib.h>\n"+source
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source",type=Path,required=True)
    ap.add_argument("--instrument",action="store_true")
    args=ap.parse_args()
    try:
        before=args.source.read_text(encoding="utf8")
        after=patch(before)
        if args.instrument:
            backup=args.source.with_suffix(".c.pre-real-null-source-recovery.bak")
            if not backup.exists():backup.write_text(before,encoding="utf8")
            args.source.write_text(after,encoding="utf8")
        print("[REAL MODELCACHE] Original native absent-node branch enabled only when opted in")
        return 0
    except (OSError,ValueError) as exc:
        print("[REAL MODELCACHE ERROR]",exc,file=sys.stderr);return 2
if __name__=="__main__": raise SystemExit(main())
