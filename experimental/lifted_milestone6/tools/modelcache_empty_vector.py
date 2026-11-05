#!/usr/bin/env python3
"""Use original ModelCache failure/cleanup branch for an absent node vector.

Verified in user's private C: f_000629b4 initializes sp+52 to NULL,
f_00061414 may leave it NULL, and the subsequent load crashes. The original
function already handles a NULL first node via L_62b44. Only the empty vector
case is routed through that existing path; normal game code is unchanged.
"""
import argparse
from pathlib import Path
import re
import sys

MARKER="DS_MODELCACHE_EMPTY_RECOVERY_000629B4"
SITE="""  {
    u32 ea = sp + 52u;
    r3 = LD32(ea);
  }
  {
    u32 ea = r3 + 0u;
    r7 = LD32(ea);
  }"""
GUARD=r'''
  /* DS_MODELCACHE_EMPTY_RECOVERY_000629B4: original failure and cleanup */
  if (r3 == 0u) {
    fprintf(stderr,
      "[MODEL_CACHE_EMPTY_NODES] name_ref=%08x mode=%08x "
      "begin=%08x end=%08x cap=%08x -> native failure L_62b44\n",
      (unsigned)r10, (unsigned)r8, (unsigned)LD32(sp+52u),
      (unsigned)LD32(sp+56u), (unsigned)LD32(sp+60u));
    fflush(stderr);
    goto L_62b44;
  }
'''
FUNCTION=re.compile(r"(?m)^u64 f_[0-9a-fA-F]{8}\(")
def patch(text):
    marks=list(FUNCTION.finditer(text))
    matches=[m.start() for m in marks if text.startswith("u64 f_000629b4(",m.start())]
    if len(matches)!=1: raise ValueError("missing/duplicate original ModelCache function")
    lo=matches[0]
    hi=next((m.start() for m in marks if m.start()>lo),len(text))
    f=text[lo:hi]
    if MARKER in f:
        if f.count(MARKER)!=1: raise ValueError("guard duplicated")
        return text
    checks={
        "unique null vector dereference":f.count(SITE)==1,
        "original vector initialization":"u32 ea = sp + 52u;\n    ST32(ea, r3);" in f,
        "vector construction helpers":"f_0032b260(" in f and "f_00061414(" in f,
        "native null-node failure branch":"if (fZ) goto L_62b44;" in f and "L_62b44:;" in f,
        "native result/cleanup":"u32 ea = r4 + 0u;\n    ST32(ea, r3);" in f and
             "f_00373bd0(" in f and "f_001a3588(" in f,
    }
    for desc,valid in checks.items():
        if not valid: raise ValueError("Unknown lifted code; missing "+desc)
    f=f.replace(SITE,SITE.replace("  {\n    u32 ea = r3 + 0u;",GUARD+"  {\n    u32 ea = r3 + 0u;",1),1)
    result=text[:lo]+f+text[hi:]
    if "#include <stdio.h>\n" not in result: result="#include <stdio.h>\n"+result
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",type=Path,required=True)
    ap.add_argument("--instrument",action="store_true")
    a=ap.parse_args()
    try:
        original=a.source.read_text(encoding="utf8")
        updated=patch(original)
        if original==updated:
            print("[MODEL_CACHE] Already patched.")
            return 0
        if not a.instrument:
            print("[MODEL_CACHE] Original function verified, no file written.")
            return 0
        backup=a.source.with_suffix(a.source.suffix+".pre-modelcache.bak")
        if not backup.exists(): backup.write_text(original,encoding="utf8")
        a.source.write_text(updated,encoding="utf8")
        print("[MODEL_CACHE] Original cleanup branch enabled for missing node vector.")
        print("[MODEL_CACHE] Private source backup:",backup)
        return 0
    except (OSError,ValueError) as exc:
        print("[MODEL_CACHE ERROR]",exc,file=sys.stderr)
        return 2

if __name__=="__main__": sys.exit(main())
