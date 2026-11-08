#!/usr/bin/env python3
"""Verified Linux m3g::Loader::load vector/asset-path diagnostic, no game-data edits.

Works only on the owner's generated f_00365a2c function. Logs the actual
output vector BEFORE and AFTER native f_0032b260 and f_003659bc in both
original branch paths. All original calls and control flow remain unchanged.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import sys

FUNC = "f_00365a2c"
MARK = "DS_M3G_LOADER_VECTOR_AUDIT_00365A2C"
CALL = "{ u64 rv = f_0032b260(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
FINISH = "{ u64 rv = f_003659bc(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }"
FN = re.compile(r"(?m)^u64 (f_[0-9a-fA-F]{8})\(")

def region(source: str) -> tuple[int,int]:
    matches = list(FN.finditer(source))
    chosen = [m for m in matches if m.group(1)==FUNC]
    if len(chosen)!=1:
        raise ValueError(f"Expected one {FUNC}, found {len(chosen)}")
    start=chosen[0].start()
    stop=next((m.start() for m in matches if m.start()>start),len(source))
    return start,stop

def trace(stage:str, branch:str) -> str:
    return """
  /* DS_M3G_LOADER_VECTOR_AUDIT_00365A2C */
  fprintf(stderr,
    "[M3G_LOADER_%s] branch=%s out=%%08x name_ref=%%08x "
    "begin=%%08x end=%%08x cap=%%08x result=%%08x\\n",
    (unsigned)r5, (unsigned)r7,
    (unsigned)LD32(r5), (unsigned)LD32(r5+4u),
    (unsigned)LD32(r5+8u), (unsigned)r0);
  fflush(stderr);
""" % (stage,branch)

# Use controlled string split to avoid matching the same original site twice.
def safe_patch(source: str) -> str:
    lo,hi=region(source)
    f=source[lo:hi]
    if MARK in f:
        if f.count(MARK)!=4: raise ValueError("Audit marker count changed")
        return source
    if "m3g6Loader4load" not in f[:240]:
        raise ValueError("Function is not m3g::Loader::load")
    if f.count(CALL)!=2 or f.count(FINISH)!=2:
        raise ValueError("Unexpected ARM calls in Loader::load, aborting")
    if f.count("L_365a80:;")!=1:
        raise ValueError("Native Loader branch absent")
    parts=f.split(CALL)
    f=parts[0]+CALL+trace("AFTER_DECODE","A")+parts[1]+CALL+trace("AFTER_DECODE","B")+parts[2]
    parts=f.split(FINISH)
    if len(parts)!=3: raise ValueError("Unexpected cleanup call count")
    f=parts[0]+FINISH+trace("AFTER_CLEANUP","A")+parts[1]+FINISH+trace("AFTER_CLEANUP","B")+parts[2]
    full=source[:lo]+f+source[hi:]
    if "#include <stdio.h>\n" not in full:
        full="#include <stdio.h>\n"+full
    return full

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",required=True,type=Path)
    ap.add_argument("--instrument",action="store_true")
    opts=ap.parse_args()
    try:
        old=opts.source.read_text(encoding="utf8")
        patched=safe_patch(old)
        if not opts.instrument:
            print("[M3G_LOADER] Verified, read-only. Use --instrument to write.")
            return 0
        if old==patched:
            print("[M3G_LOADER] Audit already installed.")
            return 0
        backup=opts.source.with_suffix(opts.source.suffix+".pre-loader-audit.bak")
        if not backup.exists(): backup.write_text(old,encoding="utf8")
        opts.source.write_text(patched,encoding="utf8")
        print("[M3G_LOADER] Instrumented both native branches, source backup:",backup)
        return 0
    except (OSError,ValueError) as exc:
        print("[M3G_LOADER ERROR]",exc,file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
