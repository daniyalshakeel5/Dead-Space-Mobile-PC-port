#!/usr/bin/env python3
"""Privacy-conscious read-only ZIP archive intake for a GitHub Actions runner.

Intended to triage Milestone 4 source checkpoints BEFORE merging them.
No entry content is extracted, executed, or published. Generated JSON contains
aggregate counts/sizes and validation warnings; not source code or path names.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
from zipfile import BadZipFile, ZipFile

MAX_ARCHIVE_BYTES = 256 * 1024 * 1024
MAX_FILES = 100_000
MAX_TOTAL_UNPACKED = 2 * 1024 * 1024 * 1024
MAX_SINGLE_FILE = 512 * 1024 * 1024
MAX_RATIO = 200

SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp",
    ".py", ".java", ".kt", ".cmake", ".md", ".txt",
    ".json", ".yml", ".yaml", ".toml", ".ini",
}
RESTRICTED_SUFFIXES = {
    ".apk", ".xapk", ".obb", ".so", ".dll", ".exe", ".aab",
    ".dex", ".pak", ".pck",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def unsafe_entry(name: str) -> bool:
    """Reject traversal, Windows paths, ambiguous separators, and NULs."""
    if not name or "\x00" in name or "\\" in name or name.startswith("/"):
        return True
    parts = name.rstrip("/").split("/")
    return (
        any(part in ("", ".", "..") for part in parts)
        or any(":" in part for part in parts)
        or PurePosixPath(name).is_absolute()
    )


def audit(path: Path) -> dict:
    report = {
        "schema": 1,
        "archive_sha256": sha256(path),
        "archive_bytes": path.stat().st_size,
        "source_bytes_published": 0,
        "valid_zip": False,
        "archive_input_present": True,
        "critical_findings": [],
        "warnings": [],
        "counts_by_extension": {},
        "code_file_count": 0,
        "restricted_binary_file_count": 0,
    }
    if path.stat().st_size > MAX_ARCHIVE_BYTES:
        report["critical_findings"].append("compressed_archive_exceeds_limit")
        return report

    try:
        with ZipFile(path) as archive:
            infos = archive.infolist()
            report["valid_zip"] = True
            report["entry_count"] = len(infos)
            if len(infos) > MAX_FILES:
                report["critical_findings"].append("too_many_entries")

            total = 0
            counts = Counter()
            seen = set()
            code_count = restricted_count = 0
            for entry in infos:
                if entry.is_dir():
                    continue
                total += entry.file_size
                name = entry.filename
                if unsafe_entry(name):
                    report["critical_findings"].append("unsafe_entry_path")
                normalized = name.casefold()
                if normalized in seen:
                    report["critical_findings"].append("duplicate_or_case_colliding_path")
                seen.add(normalized)

                mode = (entry.external_attr >> 16)
                if stat.S_IFMT(mode) == stat.S_IFLNK:
                    report["critical_findings"].append("symbolic_link")
                if entry.flag_bits & 0x1:
                    report["warnings"].append("encrypted_entry")
                if entry.file_size > MAX_SINGLE_FILE:
                    report["critical_findings"].append("oversized_entry")
                ratio = entry.file_size / max(1, entry.compress_size)
                if ratio > MAX_RATIO and entry.file_size > 1024 * 1024:
                    report["warnings"].append("suspicious_compression_ratio")

                suffix = PurePosixPath(name).suffix.lower() or "[none]"
                counts[suffix] += 1
                if suffix in SOURCE_SUFFIXES or name.endswith("/CMakeLists.txt") or name == "CMakeLists.txt":
                    code_count += 1
                if suffix in RESTRICTED_SUFFIXES:
                    restricted_count += 1
            report["declared_uncompressed_bytes"] = total
            report["counts_by_extension"] = dict(sorted(counts.items()))
            report["code_file_count"] = code_count
            report["restricted_binary_file_count"] = restricted_count
            if total > MAX_TOTAL_UNPACKED:
                report["critical_findings"].append("declared_total_exceeds_limit")
            if restricted_count:
                report["warnings"].append("restricted_binary_extensions_present")
    except (BadZipFile, OSError, ValueError):
        report["critical_findings"].append("invalid_or_unreadable_zip")

    # No archive paths or entry data in a public Actions artifact.
    report["critical_findings"] = sorted(set(report["critical_findings"]))
    report["warnings"] = sorted(set(report["warnings"]))
    report["safe_to_extract_after_independent_license_review"] = (
        report["valid_zip"] and not report["critical_findings"]
        and report["restricted_binary_file_count"] == 0
    )
    report["next_step"] = (
        "Archive contents were NOT imported. Review licensing and provenance; "
        "only explicitly approved source files may be integrated in a separate PR."
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--output", type=Path, default=Path("report/milestone-archive.json"))
    args = parser.parse_args()
    if args.archive:
        report = audit(args.archive)
    else:
        report = {
            "schema": 1,
            "archive_input_present": False,
            "status": "NO_CHECKPOINT_ZIP_SUPPLIED",
            "next_step": "Configure DS_MILESTONE4_URL and manually start GitHub Actions audit.",
        }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Milestone archive metadata report created; no archive content was uploaded.")
    return 0 if not report.get("critical_findings") else 2


if __name__ == "__main__":
    raise SystemExit(main())
