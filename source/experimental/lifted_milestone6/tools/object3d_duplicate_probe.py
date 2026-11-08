#!/usr/bin/env python3
"""Inspect the real translated Object3D::duplicate NULL invocation.

The crash in authentic Deas.Space.Mobile.Fixes.apk after successful M3G load
resolves to f_00368efc, L_369060, LD32(r0). Instrument EXACT translated ARM
function to report object pointer and true native caller; retain original
calls, branches and fault behavior. No fake objects, no bypass.
"""
from pathlib import Path
import argparse
import re
import sys

MARK="DS_REAL_OBJECT3D_DUPLICATE_SOURCE_00368EFC"
FUNCTION="f_00368efc"
EXPECTED=r'''L_369060:;
  {
    u32 ea = r0 + 0u;
    r3 = LD32(ea);
  }'''
TRACE=r'''
  /* DS_REAL_OBJECT3D_DUPLICATE_SOURCE_00368EFC */
  if (r0 == 0u) {
    fprintf(stderr,
      "[REAL_OBJECT3D_NULL_DUPLICATE] this=0x%08x context=0x%08x "
      "native_return_pc=%p guest=f_00368efc arm=L_369060\n",
      (unsigned)r0, (unsigned)r1, __builtin_return_address(0));
    fflush(stderr);
  }
'''
FN=re.compile(r"(?m)^u64 (f_[0-9a-fA-F]{8})\(")
def patch(source:str)->str:
    matches=list(FN.finditer(source))
    chosen=[m for m in matches if m.group(1)==FUNCTION]
    if len(chosen)!=1: raise ValueError("Expected single real duplicate native function")
    lo=chosen[0].start()
    hi=next((m.start() for m in matches if m.start()>lo),len(source))
    fn=source[lo:hi]
    if MARK in fn:
        if fn.count(MARK)!=1: raise ValueError("Duplicate trace marker count changed")
        return source
    if "Object3D9duplicate" not in fn[:250] or fn.count(EXPECTED)!=1:
        raise ValueError("Translated original Object3D crash site differs; refuse mutation")
    fn=fn.replace(EXPECTED,TRACE+EXPECTED,1)
    source=source[:lo]+fn+source[hi:]
    if "#include <stdio.h>\n" not in source:
        source="#include <stdio.h>\n"+source
    return source
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",required=True,type=Path)
    ap.add_argument("--instrument",action="store_true")
    args=ap.parse_args()
    try:
        before=args.source.read_text(encoding="utf8")
        after=patch(before)
        if not args.instrument:
            print("[REAL_OBJECT3D] Original ABI and null site verified (read-only)")
            return 0
        if after!=before:
            backup=args.source.with_suffix(args.source.suffix+".pre-dup-source.bak")
            if not backup.exists(): backup.write_text(before,encoding="utf8")
            args.source.write_text(after,encoding="utf8")
        print("[REAL_OBJECT3D] Native source calling site logged; original game behavior retained")
        return 0
    except (OSError,ValueError) as exc:
        print("[REAL_OBJECT3D ERROR]",exc,file=sys.stderr)
        return 2
if __name__=="__main__":
    raise SystemExit(main())
