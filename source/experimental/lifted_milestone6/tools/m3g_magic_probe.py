#!/usr/bin/env python3
"""Trace genuine M3G Loader::load 12-byte signature, expected magic, decode exit.

This is instrumentation of the actual ARM32 translation from the user's APK.
Preserves every original native call and branch; invents no game data.
"""
import argparse
from pathlib import Path
import re
import sys

FUNC = "f_0032aed4"
MARK = "DS_REAL_M3G_MAGIC_AUDIT_0032AED4"
CALL = "{ u64 rv = f_00324884(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
EXIT_BAD = "L_32b228:;"
EXIT_EMPTY = "L_32b240:;"
FN = re.compile(r"(?m)^u64 (f_[0-9a-fA-F]{8})\(")

def patch(source):
    matches=list(FN.finditer(source))
    f=[m for m in matches if m.group(1)==FUNC]
    if len(f)!=1: raise ValueError("Expected precisely one authentic M3G stream parser")
    lo=f[0].start()
    hi=next((m.start() for m in matches if m.start()>lo),len(source))
    body=source[lo:hi]
    if MARK in body:
        if body.count(MARK)!=3: raise ValueError("Previous trace markers changed")
        return source
    if "Loader4load" not in body[:320] or body.count(CALL)!=1 or body.count(EXIT_BAD)!=1 or body.count(EXIT_EMPTY)!=1:
        raise ValueError("Original M3G stream parser instructions differ; refusing patch")
    hdr=r'''
  /* DS_REAL_M3G_MAGIC_AUDIT_0032AED4 */
  {
    char ds_bytes[3][40] = {{0}};
    for (unsigned ds_i=0; ds_i<12; ds_i++) {
      snprintf(ds_bytes[0]+ds_i*3, 4, "%02x ", (unsigned)LD8(r8+ds_i));
      snprintf(ds_bytes[1]+ds_i*3, 4, "%02x ", (unsigned)LD8(r6+ds_i));
      snprintf(ds_bytes[2]+ds_i*3, 4, "%02x ", (unsigned)LD8(r6+12u+ds_i));
    }
    fprintf(stderr,
      "[REAL_M3G_HEADER] tid=%lu read_status=%08x bytes=[%s] "
      "expected_A=[%s] expected_B=[%s]\n",
      (unsigned long)pthread_self(), (unsigned)r0,
      ds_bytes[0],ds_bytes[1],ds_bytes[2]);
  }
'''
    bad=r'''
  /* DS_REAL_M3G_MAGIC_AUDIT_0032AED4 */
  fprintf(stderr, "[REAL_M3G_REJECTED_HEADER] tid=%lu out=%08x\n",
          (unsigned long)pthread_self(),(unsigned)r10);
'''
    empty=r'''
  /* DS_REAL_M3G_MAGIC_AUDIT_0032AED4 */
  fprintf(stderr, "[REAL_M3G_NO_OBJECTS] tid=%lu out_begin=%08x out_end=%08x\n",
          (unsigned long)pthread_self(),(unsigned)LD32(r10),
          (unsigned)LD32(r10+4u));
'''
    body=body.replace(CALL,CALL+"\n"+hdr,1)
    body=body.replace(EXIT_BAD,EXIT_BAD+"\n"+bad,1)
    body=body.replace(EXIT_EMPTY,EXIT_EMPTY+"\n"+empty,1)
    source=source[:lo]+body+source[hi:]
    if "#include <stdio.h>\n" not in source:
        source="#include <stdio.h>\n#include <pthread.h>\n"+source
    return source

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",type=Path,required=True)
    ap.add_argument("--instrument",action="store_true")
    a=ap.parse_args()
    try:
        before=a.source.read_text(encoding="utf8")
        after=patch(before)
        if not a.instrument:
            print("[REAL_M3G] Validated original M3G parser; read-only")
            return 0
        if after!=before:
            backup=a.source.with_suffix(a.source.suffix+".pre-m3g-read-probe.bak")
            if not backup.exists(): backup.write_text(before,encoding="utf8")
            a.source.write_text(after,encoding="utf8")
        print("[REAL_M3G] Original M3G loader magic and rejection sites instrumented; no bypass")
        return 0
    except (OSError,ValueError) as ex:
        print("[REAL_M3G ERROR]",ex,file=sys.stderr)
        return 2

if __name__=="__main__": raise SystemExit(main())
