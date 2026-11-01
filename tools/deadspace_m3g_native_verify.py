#!/usr/bin/env python3
"""Run the independent native JSR-184 container reader against an authorized APK.

The original M3G bytes are transient runner-local files, not committed,
logged, or uploaded. Only aggregate status counters are exported.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
from zipfile import ZipFile

MAGIC = bytes.fromhex("ab4a5352313834bb0d0a1a0a")
MAX_FILE = 128 * 1024 * 1024
MAX_CANDIDATES = 500
VALID_REASONS = {
    "not a JSR-184 M3G file",
    "truncated M3G identifier or header section",
    "unsafe section memory limit",
    "truncated section header/checksum",
    "unsupported M3G section compression scheme",
    "M3G section exceeds file bounds",
    "M3G section exceeds memory limit",
    "M3G section Adler32 mismatch",
    "uncompressed section length mismatch",
    "invalid M3G zlib data or output size",
    "first M3G section must not be compressed",
    "truncated M3G object header",
    "M3G object payload exceeds section",
    "reserved M3G object type",
    "header section must contain one Header object",
    "invalid M3G header payload size",
    "unsupported JSR-184 version",
    "invalid external references flag",
    "M3G declared file size mismatch",
    "unterminated authoring field",
    "unexpected second M3G header object",
    "external reference section contains scene object",
    "unexpected external reference object",
    "M3G header section not exactly one object",
    "first non-header M3G section is empty",
    "missing non-header M3G section",
}


def analyze(apk: Path, native_probe: Path) -> dict:
    outcomes: Counter = Counter()
    successful_sections = 0
    successful_objects = 0
    compressed_sections = 0
    candidates = 0
    with ZipFile(apk) as archive, tempfile.TemporaryDirectory() as directory:
        stage = Path(directory) / "temp.m3g"
        for info in archive.infolist():
            if not info.filename.lower().endswith(".m3g") or info.is_dir():
                continue
            candidates += 1
            if candidates > MAX_CANDIDATES:
                outcomes["candidate_limit_reached"] += 1
                break
            parts = info.filename.split("/")
            if (not info.filename.startswith("assets/") or ".." in parts or
                    "\\" in info.filename or ":" in info.filename or
                    stat.S_IFMT(info.external_attr >> 16) == stat.S_IFLNK or
                    info.flag_bits & 1):
                outcomes["unsafe_file"] += 1
                continue
            if info.file_size > MAX_FILE or info.file_size < 25:
                outcomes["size_limit_or_tiny"] += 1
                continue
            try:
                with archive.open(info) as src:
                    if src.read(12) != MAGIC:
                        outcomes["other_m3g_signature"] += 1
                        continue
                with archive.open(info) as src, stage.open("wb") as dst:
                    shutil.copyfileobj(src, dst, 64 * 1024)
                if stage.stat().st_size != info.file_size:
                    outcomes["zip_size_mismatch"] += 1
                    continue
                try:
                    process = subprocess.run([str(native_probe), str(stage)],
                                             capture_output=True, text=True,
                                             check=False, timeout=30)
                finally:
                    stage.unlink(missing_ok=True)
                if process.returncode != 0:
                    outcomes["native_process_error"] += 1
                    continue
                result = json.loads(process.stdout)
                if result.get("valid") is True:
                    outcomes["valid_full_jsr184_container"] += 1
                    successful_sections += int(result["sections"])
                    successful_objects += int(result["objects"])
                    compressed_sections += int(result["compressed_sections"])
                else:
                    reason = result.get("reason", "")
                    outcomes[reason if reason in VALID_REASONS else "unknown_native_error"] += 1
            except (OSError, RuntimeError, ValueError, EOFError,
                    subprocess.TimeoutExpired, json.JSONDecodeError):
                outcomes["read_or_probe_error"] += 1
            finally:
                stage.unlink(missing_ok=True)
    return {
        "schema": 1, "kind": "public-native-jsr184-container-evidence",
        "candidate_m3g_files": candidates,
        "outcomes": dict(sorted(outcomes.items())),
        "valid_containers_total_sections": successful_sections,
        "valid_containers_total_objects": successful_objects,
        "valid_containers_compressed_sections": compressed_sections,
        "limitations": "Container checks only; no original geometry/texture/animation rendering verified.",
    }


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--apk", type=Path, required=True)
    p.add_argument("--native-probe", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    report = analyze(args.apk, args.native_probe.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print("Native scene-container verification completed; only aggregate counts saved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
