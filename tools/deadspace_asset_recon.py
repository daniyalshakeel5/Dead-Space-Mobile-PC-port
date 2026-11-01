#!/usr/bin/env python3
"""Read-only, redacted APK content-format reconnaissance.

This is *not* a game asset decoder or engine. It classifies a limited prefix
of archive members in memory and emits only aggregate counts and size totals.
It never writes original file names, data, code, textures, or audio to reports.
Runs on GitHub Actions or locally with the user's own APK.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import stat
from zipfile import BadZipFile, ZipFile

MAX_MEMBERS = 100_000
MAX_PROBES = 8_000
MAX_PREFIX = 64
MAX_PROBE_FILE_BYTES = 1024 * 1024 * 1024
MAX_RATIO = 250
MAX_TOTAL_DECLARED = 32 * 1024 * 1024 * 1024

CATEGORIES = {"assets", "res", "lib", "META-INF"}
# Defined by the JSR-184 M3G file-format specification. Extension alone is
# NOT proof: the mobile game can use its own unrelated .m3g serialization.
JSR184_MAGIC = bytes.fromhex("ab4a5352313834bb0d0a1a0a")
SIGNATURES = (
    ("m3g_jsr184", JSR184_MAGIC),
    ("png", b"\x89PNG\r\n\x1a\n"),
    ("jpeg", b"\xff\xd8\xff"),
    ("dds", b"DDS "),
    ("ktx2", b"\xabKTX 20\xbb\r\n\x1a\n"),
    ("ktx1", b"\xabKTX 11\xbb\r\n\x1a\n"),
    ("pvr3", b"PVR\x03"),
    ("pkm", b"PKM 10"),
    ("pkm", b"PKM 20"),
    ("astc", b"\x13\xab\xa1\x5c"),
    ("ogg", b"OggS"),
    ("flac", b"fLaC"),
    ("id3", b"ID3"),
    ("riff", b"RIFF"),
    ("gif", b"GIF87a"),
    ("gif", b"GIF89a"),
    ("bmp", b"BM"),
    ("zip", b"PK\x03\x04"),
    ("gzip", b"\x1f\x8b\x08"),
    ("sqlite", b"SQLite format 3\x00"),
    ("elf", b"\x7fELF"),
    ("dex", b"dex\n"),
    ("unityfs", b"UnityFS\x00"),
    ("glb", b"glTF"),
    ("bink", b"BIK"),
)

def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(part)
    return digest.hexdigest()


def sniff(prefix: bytes) -> str:
    for kind, marker in SIGNATURES:
        if prefix.startswith(marker):
            if kind == "riff" and len(prefix) >= 12:
                if prefix[8:12] == b"WAVE":
                    return "wav"
                if prefix[8:12] == b"WEBP":
                    return "webp"
                return "riff-other"
            return kind
    if prefix.startswith(b"<?xml") or prefix.lstrip().startswith(b"<resources"):
        return "xml-text"
    if prefix.startswith(b"{") or prefix.startswith(b"["):
        return "possible-json-text"
    if len(prefix) >= 2:
        cmf, flg = prefix[:2]
        # Valid zlib header only, not proof that a complete stream decodes.
        if (cmf & 0x0F) == 8 and (cmf >> 4) <= 7 and (256 * cmf + flg) % 31 == 0:
            return "possible-zlib"
    return "unrecognized"


def safe_name(name: str) -> bool:
    if not name or name.startswith("/") or "\\" in name or ":" in name or "\x00" in name:
        return False
    parts = name.rstrip("/").split("/")
    return bool(parts) and all(p not in ("", ".", "..") for p in parts)


def group_for(path: str) -> str:
    first = path.split("/", 1)[0]
    return first if first in CATEGORIES else "other"


def stat_record(mapping: dict, key: str, size: int) -> None:
    record = mapping.setdefault(key, {"count": 0, "declared_bytes": 0})
    record["count"] += 1
    record["declared_bytes"] += size


def investigate(apk: Path) -> dict:
    report = {
        "schema": 1,
        "kind": "redacted-apk-resource-format-recon",
        "apk_bytes": apk.stat().st_size,
        "apk_sha256": file_sha256(apk),
        "original_filenames_exported": False,
        "original_data_exported": False,
        "valid_zip": False,
        "warnings": [],
    }
    try:
        with ZipFile(apk) as archive:
            entries = [info for info in archive.infolist() if not info.is_dir()]
            report["archive_entry_count"] = len(entries)
            if len(entries) > MAX_MEMBERS:
                report["warnings"].append("member_count_limit_exceeded")
                return report
            if sum(i.file_size for i in entries) > MAX_TOTAL_DECLARED:
                report["warnings"].append("total_declared_size_limit_exceeded")
                return report

            by_group: dict = {}
            by_extension: dict = {}
            skipped = Counter()
            candidates = []
            for info in entries:
                group = group_for(info.filename)
                stat_record(by_group, group, info.file_size)
                ext = Path(info.filename).suffix.lower()[:32] or "[none]"
                stat_record(by_extension, ext, info.file_size)
                if not safe_name(info.filename):
                    skipped["unsafe_filename"] += 1
                    continue
                unix_mode = info.external_attr >> 16
                if stat.S_IFMT(unix_mode) == stat.S_IFLNK:
                    skipped["symlink"] += 1
                    continue
                if info.flag_bits & 1:
                    skipped["encrypted"] += 1
                    continue
                if info.file_size > MAX_PROBE_FILE_BYTES:
                    skipped["oversized"] += 1
                    continue
                if info.file_size > 1024 * 1024 and (
                    info.file_size / max(info.compress_size, 1)) > MAX_RATIO:
                    skipped["suspicious_ratio"] += 1
                    continue
                if group not in ("assets", "res", "lib"):
                    continue
                candidates.append(info)

            # Analyze large files first: often packed containers hold most data.
            # Filename is used for stable local tie-breaking but is NEVER exported.
            candidates.sort(key=lambda x: (-x.file_size, x.filename))
            inspected = candidates[:MAX_PROBES]
            if len(candidates) > MAX_PROBES:
                skipped["probe_cap"] += len(candidates) - MAX_PROBES

            by_magic: dict = {}
            by_extension_magic: dict = {}
            # Pair count, not the underlying names; a useful clue as to
            # whether .sph and .sps form two components of each sprite.
            asset_sph = {entry.filename[:-4] for entry in entries
                         if safe_name(entry.filename) and
                         entry.filename.startswith("assets/") and
                         entry.filename.lower().endswith(".sph")}
            asset_sps = {entry.filename[:-4] for entry in entries
                         if safe_name(entry.filename) and
                         entry.filename.startswith("assets/") and
                         entry.filename.lower().endswith(".sps")}
            unknown_large_bytes = 0
            unknown_large_count = 0
            read_errors = 0
            for info in inspected:
                try:
                    with archive.open(info) as stream:
                        prefix = stream.read(MAX_PREFIX)
                except (BadZipFile, RuntimeError, EOFError, OSError, ValueError):
                    read_errors += 1
                    continue
                kind = sniff(prefix)
                stat_record(by_magic, kind, info.file_size)
                # No paths or source bytes; just extension x validated magic.
                ext = Path(info.filename).suffix.lower()
                if ext in {".m3g", ".sps", ".sph", ".sb", ".bin"}:
                    entry_group = by_extension_magic.setdefault(ext, {})
                    stat_record(entry_group, kind, info.file_size)
                if kind == "unrecognized" and info.file_size >= 1024 * 1024:
                    unknown_large_count += 1
                    unknown_large_bytes += info.file_size

            report.update({
                "valid_zip": True,
                "group_stats": dict(sorted(by_group.items())),
                "extension_stats": dict(sorted(by_extension.items(),
                    key=lambda pair: (-pair[1]["declared_bytes"], pair[0]))[:80]),
                "magic_stats": dict(sorted(by_magic.items())),
                "extension_magic_stats": {ext: dict(sorted(stats.items()))
                                          for ext, stats in
                                          sorted(by_extension_magic.items())},
                "sprite_stem_stats": {
                    "sph_count": len(asset_sph),
                    "sps_count": len(asset_sps),
                    "paired_count": len(asset_sph & asset_sps),
                },
                "candidate_files": len(candidates),
                "inspected_prefixes": len(inspected) - read_errors,
                "prefix_read_errors": read_errors,
                "skipped_counts": dict(sorted(skipped.items())),
                "large_unrecognized_files": unknown_large_count,
                "large_unrecognized_declared_bytes": unknown_large_bytes,
                "interpretation": (
                    "Signatures identify candidate file/container families, not "
                    "validated game asset formats. Unknown payloads must be studied "
                    "against verified original loader routines."
                ),
            })
    except (BadZipFile, OSError, ValueError) as exc:
        # Do not include untrusted file paths or exception messages in a public report.
        report["warnings"].append("invalid_or_unreadable_zip")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path("report/deadspace-resource-recon.json"))
    args = parser.parse_args()
    result = investigate(args.apk)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print("Wrote redacted resource format summary (no original files or names).")
    return 0 if result["valid_zip"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
