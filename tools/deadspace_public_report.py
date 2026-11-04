#!/usr/bin/env python3
"""Export safe aggregate APK research results from private runner outputs.

Never publish raw audit/recon reports in a public GitHub Actions artifact:
they contain user-owned binary hashes, archive paths and JNI export names.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

from tools.deadspace_audit import IMPORT_FAMILIES

MAGIC_CATEGORIES = {
    "png", "jpeg", "dds", "ktx1", "ktx2", "pvr3", "pkm", "astc",
    "ogg", "flac", "id3", "wav", "webp", "riff-other", "gif", "bmp",
    "zip", "gzip", "sqlite", "elf", "dex", "unityfs", "glb", "bink",
    "xml-text", "possible-json-text", "unrecognized",
    "m3g_jsr184", "m3g_im2", "m3g_im3", "m3g_im_dash", "m3g_gzip_wrapper",
    "inner_read_error", "possible-zlib",
}
GROUPS = {"assets", "res", "lib", "META-INF", "other"}
METRIC_KEYS = ("count", "declared_bytes")


def counts_only(record: object) -> dict:
    if not isinstance(record, dict):
        return {}
    return {key: int(record[key]) for key in METRIC_KEYS
            if type(record.get(key)) is int and record[key] >= 0}


def publish(audit: dict, recon: dict | None = None) -> dict:
    source = audit.get("apk") or {}
    report = {
        "schema": 1,
        "kind": "public-aggregate-apk-evidence",
        "real_apk_analyzed": bool(source),
        "limitations": (
            "Metadata only; no original game loader or engine verified. "
            "A positive imported API name does not identify its caller."
        ),
    }
    if source:
        report["apk_size_bytes"] = source.get("apk_size_bytes")
        report["archive_entries"] = source.get("archive_entries")
        report["dex_count"] = source.get("dex_count")
        report["native_libraries"] = []
        for lib in source.get("native_libraries", []):
            evidence = lib.get("platform_api_import_evidence") or {}
            vetted = {
                family: sorted(set(evidence.get(family, [])) & set(known))
                for family, known in IMPORT_FAMILIES.items()
                if isinstance(evidence.get(family), list)
            }
            report["native_libraries"].append({
                "machine": lib.get("machine"),
                "elf_class": lib.get("elf_class"),
                "elf_type": lib.get("elf_type"),
                "elf_endianness": lib.get("elf_endianness"),
                "size_bytes": lib.get("size_bytes"),
                "recognized_public_api_imports": vetted,
                "jni_exports_present": bool(lib.get("jni_export_hints")),
                "bootstrap_exports_present": bool(lib.get("bootstrap_exports")),
                "audit_error_present": bool(lib.get("error")),
            })
    if recon is not None:
        report["resources"] = {
            "valid_zip": bool(recon.get("valid_zip")),
            "archive_entry_count": recon.get("archive_entry_count"),
            "inspected_prefixes": recon.get("inspected_prefixes"),
            "large_unrecognized_files": recon.get("large_unrecognized_files"),
            "large_unrecognized_declared_bytes":
                recon.get("large_unrecognized_declared_bytes"),
            "magic_stats": {
                key: counts_only(value)
                for key, value in (recon.get("magic_stats") or {}).items()
                if key in MAGIC_CATEGORIES
            },
            "unknown_m3g_prefix_family_stats": {
                kind: counts_only(v)
                for kind, v in
                (recon.get("unknown_m3g_prefix_family_stats") or {}).items()
                if kind in {"extended_m3g_like", "possible_ironmonkey_wrapper",
                            "standard_gzip", "other_unknown_prefix"}
            },
            "m3g_inner_magic_stats": {
                kind: counts_only(v)
                for kind, v in (recon.get("m3g_inner_magic_stats") or {}).items()
                if kind in MAGIC_CATEGORIES
            },
            "extension_magic_stats": {
                ext: {kind: counts_only(values)
                      for kind, values in evidence.items()
                      if kind in MAGIC_CATEGORIES}
                for ext, evidence in (recon.get("extension_magic_stats") or {}).items()
                if ext in {".m3g", ".sps", ".sph", ".sb", ".bin"}
                and isinstance(evidence, dict)
            },
            "sprite_stem_stats": {
                key: val for key, val in (recon.get("sprite_stem_stats") or {}).items()
                if key in {"sph_count", "sps_count", "paired_count"}
                and type(val) is int and val >= 0
            },
            # Aggregate extension labels only. Never publish complete paths or
            # unique filenames. Allow ordinary short suffixes and [none].
            "extension_stats": {
                key: counts_only(value)
                for key, value in (recon.get("extension_stats") or {}).items()
                if (key == "[none]" or
                    (isinstance(key, str) and
                     re.fullmatch(r"\.[a-z0-9]{1,12}", key)))
                and isinstance(value, dict) and value.get("count", 0) >= 2
            },
            "group_stats": {
                key: counts_only(value)
                for key, value in (recon.get("group_stats") or {}).items()
                if key in GROUPS
            },
        }
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", required=True, type=Path)
    parser.add_argument("--recon", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    recon = json.loads(args.recon.read_text(encoding="utf-8")) if args.recon else None
    output = publish(audit, recon)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print("Published aggregate-only evidence report; never original APK data.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
