#!/usr/bin/env python3
"""Fail-closed diagnosis of the observed m3g::Object3D::duplicate NULL deref.

User's Windows x86 crash: f_00368efc, L_369060, fn_060.c:47397.
Do NOT fabricate objects, return a fake pointer, or skip native branches.
Only insert a logging/trap probe before the exact crashing load, leaving
all non-NULL Object3D duplication behavior byte-for-byte equivalent.

Also write a bounded owner-local report containing the entire lifted
duplicate function and exact call sites within the generated private C.
The report never travels to GitHub Actions or the repository.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

FUNC = "f_00368efc"
MARKER = "DS_OBJECT3D_DUPLICATE_NULL_00369060"
LABEL = "L_369060:;"
SITE = """L_369060:;
  {
    u32 ea = r0 + 0u;
    r3 = LD32(ea);
  }"""
GUARD = r'''
  /* DS_OBJECT3D_DUPLICATE_NULL_00369060
   * Crash reproduces before the virtual duplicate method at +24.
   * Never suppress this failure by creating a stand-in Object3D.
   */
  if (r0 == 0u) {
    fprintf(stderr,
       "\n[FATAL:OBJECT3D_DUPLICATE_NULL] "
       "arm=0x00369060 fn=f_00368efc this_candidate=%08x "
       "r1=%08x r2=%08x r3=%08x sp=%08x "
       "r4=%08x r5=%08x r6=%08x r7=%08x r8=%08x "
       "r9=%08x r10=%08x r11=%08x r12=%08x\n"
       "The original guest attempted LD32(NULL) before virtual +24. "
       "Check model construction and translated caller; no fake objects "
       "were allocated.\n",
       (unsigned)r0, (unsigned)r1, (unsigned)r2,
       (unsigned)r3, (unsigned)sp, (unsigned)r4,
       (unsigned)r5, (unsigned)r6, (unsigned)r7,
       (unsigned)r8, (unsigned)r9, (unsigned)r10,
       (unsigned)r11, (unsigned)r12);
    fflush(stderr);
    trap_unsupported(0x00369060u,
       "m3g::Object3D::duplicate source is NULL; model construction failed");
  }
'''
DEF = re.compile(r"(?m)^u64 (f_[0-9a-fA-F]{8})\(")


def function_region(source: str, function: str) -> tuple[int, int]:
    matches = list(DEF.finditer(source))
    found = [m for m in matches if m.group(1) == function]
    if len(found) != 1:
        raise ValueError(f"Expected exactly one translated {function}; found {len(found)}")
    start = found[0].start()
    stop = next((m.start() for m in matches if m.start() > start), len(source))
    return start, stop


def patch(source: str) -> str:
    lo, hi = function_region(source, FUNC)
    body = source[lo:hi]
    if MARKER in body:
        if body.count(MARKER) != 1:
            raise ValueError("Duplicate Object3D diagnostic marker")
        return source
    if body.count(SITE) != 1:
        raise ValueError("Object3D native null dereference differs from verified C")
    if body.count("call_ind(LD32(r3 + 24u)") != 1:
        raise ValueError("Original Object3D virtual duplicate method missing")
    if "m3g" not in body[:200] or "duplicate" not in body[:200]:
        raise ValueError("Unexpected Object3D symbol name, refusing to patch")
    body = body.replace(SITE, SITE.replace(
        "  {\n    u32 ea = r0 + 0u;", GUARD + "  {\n    u32 ea = r0 + 0u;", 1), 1)
    result = source[:lo] + body + source[hi:]
    if "#include <stdio.h>\n" not in result:
        result = "#include <stdio.h>\n" + result
    return result


def context_report(source: str, source_path: Path, scan_dir: Path | None) -> str:
    lo, hi = function_region(source, FUNC)
    body = source[lo:hi]
    chunks: list[str] = [
        "Dead Space Mobile native Object3D duplication fault context",
        "ARM 0x00369060: m3g::Object3D::duplicate -> LD32(r0+0), r0 was zero",
        "The ModelCache empty-vector fallback ran earlier in the same process.",
        "This report contains translated source excerpts, not game assets.",
        "",
        f"===== COMPLETE {FUNC} from {source_path.name} =====",
    ]
    starting_line = source[:lo].count("\n") + 1
    chunks.extend(f"{starting_line+n:6}: {line}" for n,line in enumerate(body.splitlines()))
    chunks += ["", "===== DIRECT CALLERS IN GENERATED C (maximum 40 snippets) ====="]
    if scan_dir and scan_dir.is_dir():
        count = 0
        for path in sorted(scan_dir.glob("fn_*.c")):
            content = path.read_text(encoding="utf-8", errors="replace").splitlines()
            header = ""
            for i, line in enumerate(content):
                m = DEF.match(line)
                if m:
                    header = m.group(1)
                if FUNC + "(" not in line or m and m.group(1) == FUNC:
                    continue
                chunks.append(f"\nCALLER {path.name}:{i+1} {header}")
                start,end=max(0,i-18),min(len(content),i+15)
                chunks.extend(f"{j+1:6}: {content[j]}" for j in range(start,end))
                count += 1
                if count >= 40:
                    break
            if count >= 40:
                chunks.append("CALLER LIMIT 40 REACHED; additional sites omitted")
                break
        chunks.append(f"\nDirect call sites found: {count} (limit 40)")
    else:
        chunks.append("Generated C directory not available for caller scan")
    return "\n".join(chunks) + "\n"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", required=True, type=Path)
    p.add_argument("--report", type=Path)
    p.add_argument("--instrument", action="store_true")
    a = p.parse_args()
    try:
        original = a.source.read_text(encoding="utf-8")
        patched = patch(original)
        if a.report:
            a.report.parent.mkdir(parents=True, exist_ok=True)
            a.report.write_text(
                context_report(original, a.source, a.source.parent),
                encoding="utf-8")
            print("[OBJECT3D] Owner-local duplicate + call-site report:", a.report)
        if not a.instrument:
            print("[OBJECT3D] Read-only, original guest code unchanged.")
            return 0
        if patched == original:
            print("[OBJECT3D] Crash probe already installed; normal code intact.")
            return 0
        backup = a.source.with_suffix(a.source.suffix + ".pre-object3d-probe.bak")
        if not backup.exists():
            backup.write_text(original, encoding="utf-8")
        a.source.write_text(patched, encoding="utf-8")
        print("[OBJECT3D] Guard installed at the exact null dereference.")
        print("[OBJECT3D] Only NULL logs + traps; non-NULL behavior unchanged.")
        return 0
    except (OSError, ValueError) as e:
        print("[OBJECT3D ERROR]", e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
