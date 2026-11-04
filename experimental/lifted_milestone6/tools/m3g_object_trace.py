#!/usr/bin/env python3
"""Trace actual M3G loadObject and loadSection acceptance on real game assets.
No object substitution and no skipped original native instruction.
"""
from pathlib import Path
import argparse
import re
import sys

CALL = "{ u64 rv = f_0032a4c4(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
TYPE = """  {
    u32 ea = sp + 135u;
    r3 = LD8(ea);
  }"""
MARK="DS_NATIVE_REAL_M3G_OBJECT_TRACE_0032A4C4"
FN=re.compile(r"(?m)^u64 (f_[0-9a-f]{8})\(")

def locate(source, func):
    m=list(FN.finditer(source))
    hits=[i for i,x in enumerate(m) if x.group(1)==func]
    if len(hits)!=1: raise ValueError("Expected one original translated "+func)
    i=hits[0]
    return m[i].start(),m[i+1].start() if i+1<len(m) else len(source)

def patch(source):
    lo,hi=locate(source,"f_0032a4c4")
    body=source[lo:hi]
    if MARK not in source:
        if "Loader10loadObject" not in body[:250] or body.count(TYPE)!=1:
            raise ValueError("Original loadObject type byte changed")
        body=body.replace(TYPE, TYPE+r'''
  /* DS_NATIVE_REAL_M3G_OBJECT_TRACE_0032A4C4 */
  if (getenv("DS_M3G_TRACE"))
    fprintf(stderr, "[REAL_M3G_OBJECT] stream=%08x type=%u payload_length=%u\n",
            (unsigned)r4, (unsigned)r3, (unsigned)LD32(sp+124u));
''',1)
        source=source[:lo]+body+source[hi:]
    else:
        if source.count(MARK)!=1: raise ValueError("Unexpected prior trace count")
    lo,hi=locate(source,"f_0032ad98")
    part=source[lo:hi]
    if "DS_NATIVE_REAL_M3G_SECTION_RETURN_0032AD98" not in source:
        if "Loader11loadSection" not in part[:260] or part.count(CALL)!=1:
            raise ValueError("Original loadSection object call changed")
        part=part.replace(CALL,CALL+r'''
  /* DS_NATIVE_REAL_M3G_SECTION_RETURN_0032AD98 */
  if (getenv("DS_M3G_TRACE"))
    fprintf(stderr, "[REAL_M3G_OBJECT_RETURN] result=%08x stream=%08x\n",
            (unsigned)r0, (unsigned)r4);
''',1)
        source=source[:lo]+part+source[hi:]
    else:
        if source.count("DS_NATIVE_REAL_M3G_SECTION_RETURN_0032AD98")!=1:
            raise ValueError("Unexpected section marker count")
    if "#include <stdio.h>\n" not in source:
        source="#include <stdio.h>\n#include <stdlib.h>\n"+source
    return source

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--source",type=Path,required=True)
    a.add_argument("--instrument",action="store_true")
    opt=a.parse_args()
    try:
        old=opt.source.read_text(encoding="utf8")
        new=patch(old)
        if opt.instrument:
            bak=opt.source.with_suffix(".c.pre-real-object-audit.bak")
            if not bak.exists():bak.write_text(old,encoding="utf8")
            opt.source.write_text(new,encoding="utf8")
            print("[REAL_M3G] Object records and native loadSection returns traced")
        else: print("[REAL_M3G] Both original ARM object stages verified; read-only")
        return 0
    except (OSError,ValueError) as e:
        print("[REAL_M3G OBJECT ERROR]",e,file=sys.stderr);return 2
if __name__=="__main__":raise SystemExit(main())
