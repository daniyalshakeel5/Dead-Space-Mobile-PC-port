#!/usr/bin/env python3
"""Trace actual ModelCache::loadNodeUncached model vector in real native run.

Instrument exactly the original ARM-translated call to im::m3g::Loader::load,
its EASTL intrusive_ptr vector assignment, and the proven 0x000629b4
null pointer dereference. No fake model, no bypass, no semantic change.
"""
from __future__ import annotations
import argparse
import pathlib
import re
import sys

FUNC = "f_000629b4"
MARK = "DS_REAL_MODELCACHE_VECTOR_000629B4"
CALL = "{ u64 rv = f_0032b260(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
COPY = "{ u64 rv = f_00061414(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
CRASH = (
    "{\n    u32 ea = sp + 52u;\n    r3 = LD32(ea);\n  }\n"
    "  {\n    u32 ea = r3 + 0u;\n    r7 = LD32(ea);\n  }"
)
FN = re.compile(r"(?m)^u64 f_[0-9a-f]{8}\(")
def patch(source: str) -> str:
    markers = list(FN.finditer(source))
    target = [m for m in markers if source[m.start():].startswith("u64 "+FUNC+"(")]
    if len(target) != 1:
        raise ValueError(f"Missing original {FUNC} or ambiguous source")
    lo=target[0].start()
    hi=next((m.start() for m in markers if m.start()>lo),len(source))
    part=source[lo:hi]
    if MARK in part:
        if part.count(MARK)!=2:
            raise ValueError("Unexpected count of existing model vector trace markers")
        return source
    if "ModelCache16loadNodeUncached" not in part[:250]:
        raise ValueError("Wrong model loading function; refusing to alter game")
    if part.count(CALL) != 1 or part.count(COPY) < 1 or part.count(CRASH) != 1:
        raise ValueError("Verified model load, vector assignment or exact crash instruction differs")
    src = r'''
  /* DS_REAL_MODELCACHE_VECTOR_000629B4 */
  fprintf(stderr,
    "[REAL_MODEL_M3G_DECODE] native_out=%08x begin=%08x end=%08x cap=%08x\n",
    (unsigned)(sp+36u),
    (unsigned)LD32(sp+36u), (unsigned)LD32(sp+40u),
    (unsigned)LD32(sp+44u));
  fflush(stderr);
'''
    dst = r'''
  /* DS_REAL_MODELCACHE_VECTOR_000629B4 */
  fprintf(stderr,
    "[REAL_MODEL_NODE_COPY] native_out_begin=%08x native_out_end=%08x "
    "copy_begin=%08x copy_end=%08x copy_cap=%08x\n",
    (unsigned)LD32(sp+36u), (unsigned)LD32(sp+40u),
    (unsigned)LD32(sp+52u), (unsigned)LD32(sp+56u),
    (unsigned)LD32(sp+60u));
  fflush(stderr);
'''
    part=part.replace(CALL,CALL+"\n"+src,1)
    part=part.replace(COPY,COPY+"\n"+dst,1)
    if part.index(CALL)>part.index(COPY) or part.index(COPY)>part.index(CRASH):
        raise ValueError("Original model trace sites in unexpected sequence")
    full=source[:lo]+part+source[hi:]
    if "#include <stdio.h>\n" not in full:
        full="#include <stdio.h>\n"+full
    return full

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",type=pathlib.Path,required=True)
    ap.add_argument("--instrument",action="store_true")
    a=ap.parse_args()
    try:
        before=a.source.read_text(encoding="utf-8")
        after=patch(before)
        if not a.instrument:
            print("[REAL_MODEL] Original game loader code verified, no changes")
            return 0
        if before!=after:
            bak=a.source.with_suffix(a.source.suffix+".pre-original-modeltrace.bak")
            if not bak.exists():bak.write_text(before,encoding="utf-8")
            a.source.write_text(after,encoding="utf-8")
        print("[REAL_MODEL] Installed fail-closed native model loader trace, no bypass")
        return 0
    except (OSError,ValueError) as exc:
        print("[REAL_MODEL ERROR]",exc,file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
