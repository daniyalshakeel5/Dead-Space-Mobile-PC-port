#!/usr/bin/env python3
"""Instrument the EXACT proven milestone-5 TexturePack null vector dereference.

Does not fabricate missing textures, return fake object pointers, alter the
original .so, or silently skip a texture pack. It upgrades a SIGSEGV into an
explicit diagnostic that preserves the original resource/vector values.

Default: read-only validation. --instrument: patch one generated C chunk,
first writing an exact backup, and fail closed if the expected lifted ARM
source has changed. Idempotent across subsequent invocations.

Root cause not yet known: distinguish absent assets, malformed Object3D vector
or C lift errors using the real vector begin/end, not a synthetic substitute.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

MARKER = "DS_TEXTUREPACK_VECTOR_GUARD_0035B2E0"
FUNCTION = "f_0035b2e0"
EXPECTED_LABEL = "L_35b564:;"
# Pin to the two lifted ARM loads visible in the original user's debug build.
LOAD = re.compile(
    r"(L_35b564:;\s*\{\s*u32 ea = r10 \+ 0u;\s*"
    r"r3 = LD32\(ea\);\s*\})",
    flags=re.MULTILINE,
)
# Validate the null read in the same function, not an unrelated substring.
DEREF = re.compile(
    r"\{\s*u32 ea = r3 \+ 0u;\s*r1 = LD32\(ea\);\s*\}",
    flags=re.MULTILINE,
)
FUNC_RE = re.compile(
    r"(?m)^u64 f_[0-9a-f]{8}\(u32 r0, u32 r1, u32 r2,"
    r" u32 r3, u32 sp\) \{"
)
# These are injected into a private, user-generated C file, not game content.
GUARD = r'''
  /* DS_TEXTUREPACK_VECTOR_GUARD_0035B2E0
   * Original ARM vector is expected to have begin/end at r10+0/+4.
   * f_00368338 is not reached: the compiler dereferences begin before
   * performing the call. Never turn this missing resource into a fake texture.
   */
  if (r3 == 0u) {
    const u32 ds_begin = LD32(r10 + 0u);
    const u32 ds_end   = LD32(r10 + 4u);
    const u32 ds_capacity = LD32(r10 + 8u);
    fprintf(stderr,
       "\n[FATAL:TEXPACK_NULL_VECTOR] arm=f_0035b2e0 "
       "label=L_35b564 this=0x%08x vector=0x%08x "
       "begin=0x%08x end=0x%08x capacity=0x%08x "
       "r5=0x%08x r7=0x%08x\n"
       "This is an absent/corrupt intrusive_ptr<Object3D> vector. "
       "Check the matching original asset set and the stream decode "
       "rather than replacing the null pointer with a dummy.\n",
       (unsigned)LD32(sp+32u), (unsigned)r10, (unsigned)ds_begin,
       (unsigned)ds_end, (unsigned)ds_capacity,
       (unsigned)r5, (unsigned)r7);
    fflush(stderr);
    trap_unsupported(0x0035b564u,
       "TexturePack::load vector starts at NULL; asset/object decode invalid");
  }
'''


def patch(source: str) -> str:
    matches = list(FUNC_RE.finditer(source))
    begin = next((x.start() for x in matches if source[x.start():].startswith(
        "u64 f_0035b2e0(")), -1)
    if begin < 0:
        raise ValueError("Missing expected original ARM function f_0035b2e0")
    end = next((x.start() for x in matches if x.start() > begin), len(source))
    body = source[begin:end]
    if MARKER in body:
        return source
    found = list(LOAD.finditer(body))
    if len(found) != 1 or len(DEREF.findall(body)) < 1:
        raise ValueError(
            "The translated TexturePack code differs from the verified "
            "crash report; refusing to modify an unknown code location.")
    m = found[0]
    site = DEREF.search(body, m.end())
    if site is None or site.start() > m.end() + 1100:
        raise ValueError("Crash-site load is not adjacent to L_35b564; "
                         "refusing to instrument an unknown source layout")
    replacement = body[:m.end()] + "\n" + GUARD + body[m.end():]
    if '#include <stdio.h>\n' not in source[:begin]:
        source = '#include <stdio.h>\n' + source
    # Recompute function bounds after potential header insertion.
    matches = list(FUNC_RE.finditer(source))
    begin = next(x.start() for x in matches if source[x.start():].startswith(
        "u64 f_0035b2e0("))
    end = next((x.start() for x in matches if x.start() > begin),len(source))
    return source[:begin] + replacement + source[end:]


def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",type=Path,
       default=Path(__file__).resolve().parent/"out"/"fn_059.c")
    ap.add_argument("--instrument",action="store_true",
       help="Back up and instrument user's generated fn_059.c")
    args=ap.parse_args()
    try:
        original=args.source.read_text(encoding="utf-8")
        patched=patch(original)
    except (OSError, ValueError) as exc:
        print(f"[TEXPACK] {exc}",file=sys.stderr)
        return 2
    if patched==original:
        print("[TEXPACK] Already instrumented; no changes.")
        return 0
    print("[TEXPACK] Verified crash-path source match; "
          "vector begin/end/capacity guard ready.")
    if not args.instrument:
        print("[TEXPACK] Read-only. No generated files changed.")
        return 0
    backup=args.source.with_suffix(args.source.suffix+".pre-texturepack-probe.bak")
    if not backup.exists():
        backup.write_text(original,encoding="utf-8")
    args.source.write_text(patched,encoding="utf-8")
    print("[TEXPACK] Injected an explicit abort before null vector deref.")
    print(f"[TEXPACK] Original preserved: {backup}")
    print("[TEXPACK] Rebuild the changed C chunk before next launch; do NOT "
          "regenerate from the .so before the guarded build.")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
