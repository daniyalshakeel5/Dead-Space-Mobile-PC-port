#!/usr/bin/env python3
"""Compare a ZIP milestone against a RAR milestone without storing game files.

This runs in GitHub Actions on temporary checkouts. It never commits extracted
files and produces a bounded text report of file-level changes and high-value
boot/resource symbols. The report contains no bundled game resources.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

MAX_FILES = 10000
MAX_FILE_BYTES = 25_000_000
MAX_PRINT_FILES = 130
MAX_LINE = 190
TEXT_EXT = {".c", ".cpp", ".h", ".hpp", ".py", ".sh", ".md", ".txt",
            ".ini", ".json", ".yml", ".yaml", ".xml", ".cmake", ".cmd",
            ".bat", ".ps1", ".xaml"}
SCAN_RULES = [
    ("texture-crash-function", re.compile(r"f_0035b2e0|L_35b564|TexturePack", re.I)),
    ("source-vector-creator", re.compile(r"f_00365a2c|f_0035c844", re.I)),
    ("startup-background", re.compile(r"f_000adfc4|LayerBackground", re.I)),
    ("resource-file-access", re.compile(r"data[.]zip|EAMCore[.]ini|AssetManager|GetAppDataDirectory|fopen", re.I)),
    ("JNI-boot", re.compile(r"NativeOnCreate|NativeOnDrawFrame|JNI_OnLoad|NativeOnSurfaceCreated")),
    ("load-translator", re.compile(r"gen_all|protos[.]h|call_ind|trap_unsupported", re.I)),
    ("SDL-graphics", re.compile(r"SDL_GL|glCompressedTex|glDrawElements|glTexImage", re.I)),
]
BAD_SUFFIX = {".so", ".apk", ".obb", ".dex", ".bin", ".pvr", ".m3g"}


def extract_zip(path: Path, output: Path) -> None:
    with zipfile.ZipFile(path) as z:
        entries = z.infolist()
        if len(entries) > MAX_FILES:
            raise RuntimeError("ZIP contains unexpectedly many entries")
        for e in entries:
            name = e.filename.replace("\\", "/")
            parts = PurePosixPath(name).parts
            if name.startswith("/") or ".." in parts or not parts:
                raise RuntimeError("Unsafe ZIP path: " + repr(name))
            if e.file_size > MAX_FILE_BYTES:
                raise RuntimeError("ZIP contains oversized entry: " + name)
            target = output.joinpath(*parts)
            if e.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with z.open(e) as src, target.open("wb") as dst:
                    shutil.copyfileobj(src, dst)


def extract_rar(path: Path, output: Path) -> None:
    if not shutil.which("unar"):
        raise RuntimeError("unar command is required for the RAR milestone")
    proc = subprocess.run(
        ["unar", "-quiet", "-o", str(output), str(path.resolve())],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
    if proc.returncode:
        raise RuntimeError("RAR extraction failed: " + proc.stdout[-2500:])


def snapshot(root: Path) -> dict[str, Path]:
    files = sorted(p for p in root.rglob("*") if p.is_file())
    if len(files) > MAX_FILES:
        raise RuntimeError("Archive has unexpectedly many extracted files")
    if any(p.is_symlink() for p in root.rglob("*")):
        raise RuntimeError("Symlink present in archive; refusing to compare")
    relative = [p.relative_to(root).as_posix() for p in files]
    tops = {s.split("/", 1)[0] for s in relative}
    strip = (next(iter(tops)) + "/") if len(tops) == 1 and all("/" in x for x in relative) else ""
    return {r[len(strip):]: p for r, p in zip(relative, files)}


def digest(path: Path) -> tuple[str, int]:
    size = path.stat().st_size
    if size > MAX_FILE_BYTES:
        raise RuntimeError("Unexpectedly large extracted file: " + path.name)
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest(), size


def scan(path: Path) -> tuple[set[str], list[str]]:
    if path.suffix.lower() not in TEXT_EXT:
        return set(), []
    if path.stat().st_size > 2_000_000:
        return set(), []
    s = path.read_text(encoding="utf-8", errors="replace")
    hits, examples = set(), []
    for line in s.splitlines():
        found = [label for label, rule in SCAN_RULES if rule.search(line)]
        if found:
            hits.update(found)
            if len(examples) < 5:
                examples.append(" | ".join(found) + ": " + line.strip()[:MAX_LINE])
    return hits, examples


def compare(old: dict[str, Path], new: dict[str, Path]) -> str:
    orig, now = set(old), set(new)
    common = orig & now
    old_hash = {k: digest(v) for k, v in old.items()}
    new_hash = {k: digest(v) for k, v in new.items()}
    changed = sorted(k for k in common if old_hash[k][0] != new_hash[k][0])
    added = sorted(now-orig)
    removed = sorted(orig-now)
    same = len(common)-len(changed)
    out = [
        "# Dead Space Mobile milestone comparison (GitHub Actions)",
        "",
        "Inputs: milestone 5 ZIP and milestone 6 RAR from main. Extraction is runner-local.",
        "No source or proprietary assets are committed by the analyzer.",
        "",
        f"Milestone 5 files: {len(old)}; milestone 6 files: {len(new)}.",
        f"Changed: {len(changed)}; added: {len(added)}; removed: {len(removed)}; unchanged: {same}.",
        "",
    ]
    for title, names in (("Changed", changed), ("Added", added), ("Removed", removed)):
        out += [f"## {title} ({len(names)})", ""]
        for name in names[:MAX_PRINT_FILES]:
            a = old_hash.get(name, ("", None))[1]
            b = new_hash.get(name, ("", None))[1]
            out.append(f"- {name} : {a} -> {b} bytes")
        if len(names) > MAX_PRINT_FILES:
            out.append(f"... {len(names)-MAX_PRINT_FILES} more.")
        out.append("")
    out += ["## Boot / texture loading changes", ""]
    focus = set()
    for name in changed+added+removed:
        a = scan(old[name]) if name in old else (set(), [])
        b = scan(new[name]) if name in new else (set(), [])
        if a[0] | b[0]:
            focus.add(name)
            out += [f"### {name}", "",
                    f"- signatures before: {', '.join(sorted(a[0])) or '(none)'}",
                    f"- signatures after: {', '.join(sorted(b[0])) or '(none)'}"]
            # Show short, non-source hints (tokens and matching line counts).
            # A full code diff is NOT placed in public workflow logs.
            out += [f"- examples counted: before {len(a[1])}, after {len(b[1])}", ""]
    if not focus:
        out += ["No matching short text files changed.", ""]
    out += ["## Potential archive/package risks", ""]
    for name in added+changed:
        if Path(name).suffix.lower() in BAD_SUFFIX or name.lower().endswith("data.zip"):
            out.append(f"- Check proprietary binary before distribution: {name}")
    out += ["", "## Readiness boundary", "",
            "Milestone 5 WSL SIGSEGV: f_0035b2e0 (TexturePack::load) at source fn_059.c:41995.",
            "A changed host file does not demonstrate a fixed original-game boot.",
            "To verify, reproduce with matching libDeadSpace.so and resource files,",
            "then inspect actual menu rendering, audio and interactions.", ""]
    return "\n".join(out)


def self_test() -> None:
    with tempfile.TemporaryDirectory() as temp:
        d = Path(temp)
        a, b = d/"old", d/"new"
        a.mkdir(); b.mkdir()
        (a/"dspc").mkdir(); (b/"dspc").mkdir()
        (a/"dspc"/"host.c").write_text("TexturePack old")
        (b/"dspc"/"host.c").write_text("TexturePack newer")
        (b/"dspc"/"new.c").write_text("SDL_GL_CreateContext")
        rep=compare(snapshot(a),snapshot(b))
        assert "Changed: 1; added: 1; removed: 0" in rep, rep
        assert "host.c" in rep and "new.c" in rep
    print("Synthetic analyzer tests PASS")


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--old", type=Path)
    ap.add_argument("--new", type=Path)
    ap.add_argument("--report", type=Path, default=Path("milestone6-analysis.md"))
    ap.add_argument("--self-test", action="store_true")
    args=ap.parse_args()
    if args.self_test:
        self_test(); return 0
    if not args.old or not args.new:
        ap.error("--old and --new are required")
    with tempfile.TemporaryDirectory() as t:
        root=Path(t); a=root/"old"; b=root/"new"
        a.mkdir(); b.mkdir()
        extract_zip(args.old, a)
        extract_rar(args.new, b)
        report=compare(snapshot(a),snapshot(b))
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(report, encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
