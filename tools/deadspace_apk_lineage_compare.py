#!/usr/bin/env python3
"""Compare *metadata* of two owned/public Dead Space APKs, no extracted assets.

The ZIP central-directory comparison is a triage tool. Matching CRC32+size
is NOT proof of identical content. A SHA256 match of original native ELF
bytes is much stronger evidence of binary identity.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

MAX_ENTRIES = 100_000
MAX_NAME = 4096
MAX_NATIVE = 128 * 1024 * 1024
MAX_NATIVE_TOTAL = 512 * 1024 * 1024


def extension(name: str) -> str:
    part = name.rsplit("/", 1)[-1]
    suffix = part.rsplit(".", 1)[-1].lower() if "." in part else ""
    return "." + suffix[:24] if suffix else "[none]"


def metadata(apk: Path) -> dict:
    if not apk.is_file():
        raise ValueError("APK path does not exist")
    with ZipFile(apk) as archive:
        entries = [item for item in archive.infolist() if not item.is_dir()]
        if len(entries) > MAX_ENTRIES:
            raise ValueError("Too many ZIP members")
        if any(len(item.filename) > MAX_NAME for item in entries):
            raise ValueError("Excessive ZIP entry filename length")
        if len({item.filename for item in entries}) != len(entries):
            raise ValueError("Duplicate ZIP entry name")
        assets = {
            item.filename: (item.file_size, item.CRC)
            for item in entries if item.filename.startswith("assets/")
        }
        # Release metadata alone cannot prove where a translated ARM ELF
        # is stored. Also check .so objects outside the standard lib/ tree.
        native = [item for item in entries
                  if item.filename.lower().endswith(".so")]
        libraries = []
        budget = 0
        for item in sorted(native, key=lambda x: x.filename):
            if item.file_size <= 0 or item.file_size > MAX_NATIVE or (
                    budget + item.file_size > MAX_NATIVE_TOTAL):
                libraries.append({
                    "path": item.filename,
                    "size": item.file_size,
                    "skipped": "bounded native ELF inspection",
                })
                continue
            budget += item.file_size
            digest = hashlib.sha256()
            header = b""
            count = 0
            with archive.open(item) as stream:
                while True:
                    chunk = stream.read(1024 * 1024)
                    if not chunk:
                        break
                    if len(header) < 24:
                        header = (header + chunk)[:24]
                    digest.update(chunk)
                    count += len(chunk)
                    if count > item.file_size:
                        raise ValueError("Unexpected native library output size")
            if count != item.file_size:
                raise ValueError("Truncated native library")
            elf = len(header) >= 20 and header[:4] == b"\x7fELF"
            machine = int.from_bytes(header[18:20], "little") if elf and header[5] == 1 else None
            libraries.append({
                "path": item.filename,
                "size": item.file_size,
                "sha256": digest.hexdigest(),
                "elf": elf,
                "elf_class": 64 if elf and header[4] == 2 else 32 if elf and header[4] == 1 else None,
                "elf_machine": {40: "ARM32", 183: "AArch64", 62: "x86_64", 3: "x86"}.get(machine, "unknown"),
            })
        return {
            "archive_files": len(entries),
            "asset_count": len(assets),
            "asset_total_bytes": sum(v[0] for v in assets.values()),
            "assets": assets,  # retained only in local RAM, never serialized
            "native_libraries": libraries,
            "has_android_manifest": any(item.filename == "AndroidManifest.xml" for item in entries),
            "dex_files": sum(item.filename.endswith(".dex") for item in entries),
        }


def asset_directory_groups(assets: dict[str, tuple[int, int]]) -> dict[str, int]:
    """Coarse directory counts; no individual proprietary asset filenames."""
    counts = Counter()
    for path in assets:
        parts = path.split("/")
        counts["/".join(parts[:min(3, len(parts) - 1)])] += 1
    return dict(sorted(counts.items(), key=lambda x: (-x[1], x[0]))[:25])


def comparison(a: dict, b: dict) -> dict:
    a_assets, b_assets = a["assets"], b["assets"]
    common = a_assets.keys() & b_assets.keys()
    same = sum(a_assets[k] == b_assets[k] for k in common)
    changed = [k for k in common if a_assets[k] != b_assets[k]]
    renamed_old = Counter(a_assets[k] for k in a_assets.keys() - b_assets.keys())
    renamed_new = Counter(b_assets[k] for k in b_assets.keys() - a_assets.keys())
    # This identifies candidates that retained ZIP payload length and CRC32
    # but changed asset path. Not cryptographic evidence of asset identity.
    renamed_candidates = sum((renamed_old & renamed_new).values())
    old_libs = {x.get("sha256"): x for x in a["native_libraries"] if x.get("elf") and x.get("sha256")}
    new_libs = {x.get("sha256"): x for x in b["native_libraries"] if x.get("elf") and x.get("sha256")}
    matches = old_libs.keys() & new_libs.keys()
    def expose(summary: dict) -> dict:
        return {k: v for k, v in summary.items() if k != "assets"}
    return {
        "schema": 1,
        "comparison_scope": "ZIP central-directory asset metadata and bounded native ELF SHA256",
        "original": expose(a),
        "reference": expose(b),
        "asset_comparison": {
            "shared_path_count": len(common),
            "matching_size_and_crc32_count": same,
            "changed_size_or_crc32_count": len(changed),
            "original_only_count": len(a_assets.keys() - b_assets.keys()),
            "reference_only_count": len(b_assets.keys() - a_assets.keys()),
            "cross_path_matching_size_and_crc32_count": renamed_candidates,
            "original_directory_groups": asset_directory_groups(a_assets),
            "reference_directory_groups": asset_directory_groups(b_assets),
            "changed_extensions": dict(sorted(Counter(extension(k) for k in changed).items())),
        },
        "native_elf_byte_identical_matches": [
            {
                "original_path": old_libs[d]["path"],
                "reference_path": new_libs[d]["path"],
                "sha256": d,
            } for d in sorted(matches)
        ],
        "interpretation": (
            "CRC32+size equality is only metadata similarity, not byte-level proof. "
            "Different ELF hashes do not prove a different game's behavior. "
            "ARM game code may be hidden under a non-.so archive name. "
            "No code execution, game asset extraction or original APK redistribution."
        ),
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original", type=Path, required=True)
    p.add_argument("--reference", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    result = comparison(metadata(args.original), metadata(args.reference))
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    brief = result["asset_comparison"]
    print("REANIMATED_APK_LINEAGE", json.dumps({
        "original_native_libraries": result["original"]["native_libraries"],
        "reference_native_libraries": result["reference"]["native_libraries"],
        "asset_comparison": brief,
        "identical_native_elf_count": len(result["native_elf_byte_identical_matches"]),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
