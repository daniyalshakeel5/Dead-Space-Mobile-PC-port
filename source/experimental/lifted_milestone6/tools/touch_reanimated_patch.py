#!/usr/bin/env python3
"""Version-locked native touch-threshold patch for the original 2011 engine.

The GINESTR0 Reanimated v1.3 APK embeds the *identical* ARM ELF (SHA1 below).
Reanimated's ARM64 Android runtime changes the 10-pixel tap tolerance,
30-pixel double-tap distance, and HUD interaction handling. This independent
x86 implementation scales the equivalent, verified original game sites.
It does NOT bundle or translate the closed Reanimated runtime.

The patch is intentionally fail-closed, exact-function scoped, and idempotent.
It runs on translated output generated from the authentic user's ELF only.
"""
from __future__ import annotations
import hashlib
from pathlib import Path

EXPECTED_SHA1 = "64a3323cd3cc0c483a645b62af8508624163ba9b"
# (native function RVA, original assembly-derived expression, replacement)
SITES = (
    (
        "0007aefc",  # InputForwarderTaps::getRecentReleasePointer(int,int)
        """  {
    u32 ea = r0 + 468u;
    r2 = LD32(ea);
  }""",
        """  {
    u32 ea = r0 + 468u;
    r2 = ds_reanimated_tap_px(LD32(ea), 30u);
  }""",
    ),
    (
        "0007b810",  # InputForwarderTaps::onEvent
        """  {
    u32 ea = r0 + 472u;
    r5 = LD32(ea);
  }""",
        """  {
    u32 ea = r0 + 472u;
    r5 = ds_reanimated_tap_px(LD32(ea), 10u);
  }""",
    ),
    (
        "0006a908",  # Hud::doSpecialAction_InteractionWithObjects
        "  V[23] = f2ur((float)((u32)V[23]));",
        "  V[23] = f2ur((float)((u32)V[23]) * ds_reanimated_touch_scale());",
    ),
)

def patch(out: Path, so: Path) -> list[str]:
    with so.open("rb") as f:
        actual = hashlib.file_digest(f, "sha1").hexdigest()
    if actual != EXPECTED_SHA1:
        raise ValueError(
            "Reanimated touch correction requires original ARM32 library "
            f"SHA1 {EXPECTED_SHA1}; got {actual}"
        )
    changes: dict[Path, str] = {}
    descriptions: list[str] = []
    candidates = sorted(out.glob("fn_*.c"))
    if not candidates:
        raise ValueError("No generated native function units exist")
    for function, original, replacement in SITES:
        header = f"u64 f_{function}("
        matches = []
        for path in candidates:
            source = changes.get(path)
            if source is None:
                source = path.read_text("utf-8")
            start = source.find(header)
            if start < 0:
                continue
            if source.find(header, start + len(header)) >= 0:
                raise ValueError(f"Duplicate original function in {path}")
            end = source.find("\nu64 f_", start + len(header))
            if end == -1:
                end = len(source)
            body = source[start:end]
            matches.append((path, source, start, end, body))
        if len(matches) != 1:
            raise ValueError(
                f"Expected exactly one real 2011 function {header}; found {len(matches)}"
            )
        path, source, start, end, body = matches[0]
        if body.count(replacement) == 1 and original not in body:
            descriptions.append(f"{function}: already patched, {path.name}")
            continue
        if body.count(original) != 1:
            raise ValueError(
                f"Ambiguous original touch pattern in function {function}: "
                f"expected once, found {body.count(original)}. DO NOT guess."
            )
        changes[path] = source[:start] + body.replace(original, replacement, 1) + source[end:]
        descriptions.append(f"{function}: patched original ARM call site in {path.name}")
    for path, content in changes.items():
        path.write_text(content, encoding="utf-8")
    return descriptions

if __name__ == "__main__":
    import os
    out = Path(os.environ.get("DS_PATCH_OUT", "out"))
    game = Path(os.environ.get("DS_SO", "../apk/lib/armeabi/libDeadSpace.so"))
    for message in patch(out, game):
        print("REANIMATED_TOUCH_PATCH", message)
