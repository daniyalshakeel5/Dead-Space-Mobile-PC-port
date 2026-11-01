#!/usr/bin/env python3
"""Metadata-only Android lifecycle reconstruction from temporary JADX output.

Do not publish proprietary decompiled Java source. This reports an ordered
sequence of public Android/Java runtime calls, native method DECLARATIONS,
and concrete callbacks relevant to reconstructing a C++ native bootstrap.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import re

CALLBACKS = {
    "onCreate", "onStart", "onResume", "onPause", "onStop", "onDestroy",
    "onSurfaceCreated", "onSurfaceChanged", "onDrawFrame",
    "surfaceCreated", "surfaceChanged", "surfaceDestroyed",
    "onTouchEvent", "onKeyDown", "onKeyUp", "onGenericMotionEvent",
    "onBackPressed", "run", "onActivityResult", "onRequestPermissionsResult",
}
SAFE_CALLS = {
    "loadLibrary", "setContentView", "setRequestedOrientation",
    "onCreate", "onResume", "onPause", "onDestroy", "surfaceCreated",
    "surfaceChanged", "surfaceDestroyed", "onSurfaceCreated", "onSurfaceChanged",
    "onDrawFrame", "setRenderer", "setEGLContextClientVersion",
    "queueEvent", "requestRender", "setRenderMode", "finish",
    "getWindow", "getWindowManager", "setFlags", "addFlags", "clearFlags",
    "setOnTouchListener", "setOnKeyListener", "setFocusable", "setFocusableInTouchMode",
    "registerListener", "unregisterListener", "getAssets", "getResources",
    "setOnSystemUiVisibilityChangeListener", "getApplicationContext",
    "setOnClickListener", "setOnGenericMotionListener",
}
PUBLIC_CLASS_MARKERS = (
    "android.app", "android.opengl", "android.view",
    "android.content", "android.os", "java.lang",
)
METHOD = re.compile(
    r'(?m)^[ \t]*(?:(?:@\w+(?:\([^)]*\))?)\s*)*'
    r'(?:(?:public|protected|private|static|synchronized|final|native|'
    r'override|abstract)\s+)*'
    r'[\w<>,\[\].?@ ]+\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{'
)
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
NATIVE_DECL = re.compile(
    r'\bnative\s+[\w\[\]<>,.?]+\s+([A-Za-z_]\w*)\s*\([^;]{0,800}\)\s*;'
)
CLASS = re.compile(r'\b(?:class|interface)\s+([A-Za-z_]\w*)')
LOAD_LIBRARY = re.compile(r'(?:System|Runtime\.getRuntime\(\))\.loadLibrary\s*\(\s*"([^"]{1,100})"\s*\)')
PACKAGE = re.compile(r'(?m)^\s*package\s+([\w.]+)\s*;')


def get_body(text: str, opening: int, cap: int = 100000) -> str:
    depth = 1
    limit = min(len(text), opening + cap)
    for i in range(opening, limit):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[opening:i]
    return ""


def examine(source: str) -> dict | None:
    if not (" native " in source or "loadLibrary" in source or
            any(x + "(" in source for x in CALLBACKS)):
        return None
    package = PACKAGE.search(source)
    klass = CLASS.search(source)
    # A class name is ABI metadata, not decompiled method code.
    identifier = (package.group(1) + "." if package else "") + (klass.group(1) if klass else "unknown")
    methods = []
    for m in METHOD.finditer(source):
        name = m.group(1)
        if name not in CALLBACKS:
            continue
        body = get_body(source, m.end())
        if not body:
            continue
        ordered = [x for x in CALL.findall(body) if x in SAFE_CALLS]
        methods.append({
            "callback": name,
            "public_runtime_call_sequence": ordered[:80],
            "body_present": True,
            "private_call_count": sum(x not in SAFE_CALLS for x in CALL.findall(body)),
        })
        if len(methods) >= 30:
            break
    natives = sorted(set(NATIVE_DECL.findall(source)))[:100]
    libraries = sorted(set(LOAD_LIBRARY.findall(source)))[:20]
    if not (methods or natives or libraries):
        return None
    return {
        "class": identifier,
        "native_declarations": natives,
        "library_names": libraries,
        "callbacks": methods
    }


def survey(root: Path) -> dict:
    relevant = []
    total_classes = 0
    truncated = 0
    for path in sorted(root.rglob("*.java")):
        try:
            if path.stat().st_size > 3 * 1024 * 1024:
                truncated += 1
                continue
            src = path.read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeError):
            continue
        total_classes += 1
        v = examine(src)
        if v:
            relevant.append(v)
    # JNI and library-loading classes are much more relevant to portable
    # bootstrap recovery than arbitrary background Runnable implementations.
    relevant.sort(key=lambda x: (
        -int(bool(x["native_declarations"] or x["library_names"])),
        -sum(method["callback"] != "run" for method in x["callbacks"]),
        x["class"]))
    selected = relevant[:45]
    return {
        "schema": 1,
        "kind": "original-android-bootstrap-call-sequences",
        "java_sources_scanned": total_classes,
        "relevant_classes": selected,
        "total_relevant_classes": len(relevant),
        "skipped_or_remaining": truncated + max(0, len(relevant) - len(selected)),
        "limitations": (
            "Ordered public method calls and native declarations only. "
            "JADX is not byte-for-byte Java source; branch predicates, loops, "
            "gameplay effects, indirect calls and native function semantics "
            "require separate ARM verification. No decompiled source published."
        ),
    }


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--source-dir", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    if not args.source_dir.is_dir():
        p.error("JADX source directory missing")
    data = survey(args.source_dir)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Recovered public Android callback order and JNI declarations; no source published.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
