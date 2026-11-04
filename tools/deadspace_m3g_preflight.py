#!/usr/bin/env python3
"""Conservative JSR-184 M3G *header-section* verifier.

Read ONLY the bounded, mandatory first M3G section from an authorized APK.
Never copy or output game data, path names, original asset bytes or hashes.
Does not attempt to interpret geometry, animation or other scene objects.
"""
from __future__ import annotations
from collections import Counter
import argparse
import json
from pathlib import Path
import stat
import struct
from zipfile import ZipFile
import zlib

MAGIC = bytes.fromhex("ab4a5352313834bb0d0a1a0a")
IM_MAGIC = bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
MAX_HEADER_SECTION = 1024 * 1024
MAX_M3G_FILES = 10000


def verify_prefix(prefix: bytes, section: bytes, declared_file_size: int) -> tuple[str, dict]:
    """Validate one header section only, not a complete M3G file."""
    if prefix != MAGIC:
        return "non_jsr184_magic", {}
    if len(section) < 13:
        return "truncated_section", {}
    if section[0] != 0:
        return "compressed_or_reserved_header", {}
    total, uncompressed = struct.unpack_from("<II", section, 1)
    if total < 13 or total > MAX_HEADER_SECTION or len(section) != total:
        return "bad_section_size", {}
    body = section[9:-4]
    if uncompressed != len(body):
        return "bad_uncompressed_length", {}
    expected_checksum = struct.unpack_from("<I", section, total - 4)[0]
    if zlib.adler32(section[:-4]) != expected_checksum:
        return "checksum_mismatch", {}
    if len(body) < 5 or body[0] != 0:
        return "missing_header_object", {}
    size = struct.unpack_from("<I", body, 1)[0]
    if size != len(body) - 5 or size < 12:
        return "malformed_header_object_length", {}
    obj = body[5:]
    # VersionNumber[2], hasExternalReferences (boolean), TotalFileSize[4],
    # ApproximateContentSize[4], NUL-terminated authoring field.
    if obj[2] not in (0, 1):
        return "bad_external_reference_flag", {}
    if not obj[11:].endswith(b"\x00"):
        return "unterminated_authoring_field", {}
    claimed_size = struct.unpack_from("<I", obj, 3)[0]
    if claimed_size != declared_file_size:
        return "total_file_size_mismatch", {}
    version = f"{obj[0]}.{obj[1]}"
    if version != "1.0":
        return "unsupported_jsr184_version", {"version": "other"}
    return "valid_jsr184_header", {
        "version": version, "external_references": bool(obj[2]),
        "declared_total_matches": True,
    }


def analyze_archive(apk: Path) -> dict:
    results = Counter()
    versions = Counter()
    external = 0
    examined = 0
    next_sections = Counter()
    im_sections = Counter()
    try:
        with ZipFile(apk) as archive:
            for info in archive.infolist():
                if not info.filename.lower().endswith(".m3g") or info.is_dir():
                    continue
                examined += 1
                if examined > MAX_M3G_FILES:
                    results["too_many_m3g_files"] += 1
                    break
                components = info.filename.replace("\\", "/").split("/")
                if (".." in components or info.filename.startswith("/") or
                        stat.S_IFMT(info.external_attr >> 16) == stat.S_IFLNK or
                        (info.flag_bits & 1)):
                    results["unsafe_entry_skipped"] += 1
                    continue
                if info.file_size < 25:
                    results["undersized_file"] += 1
                    continue
                try:
                    with archive.open(info) as stream:
                        magic = stream.read(12)
                        if magic == IM_MAGIC:
                            results["ironmonkey_m3g_magic"] += 1
                            section_hdr = stream.read(9)
                            if len(section_hdr) != 9:
                                im_sections["missing_first_section"] += 1
                                continue
                            scheme = section_hdr[0]
                            total, uncompressed = struct.unpack_from(
                                "<II", section_hdr, 1)
                            im_sections["compression_" + str(scheme)
                                        if scheme in (0, 1)
                                        else "unknown_compression"] += 1
                            if total < 13:
                                im_sections["invalid_section_length"] += 1
                                continue
                            if total > MAX_HEADER_SECTION:
                                # Not intrinsically invalid: conservative 1 MiB
                                # bounded-read limit on unknown IM sections.
                                im_sections["section_exceeds_preflight_limit"] += 1
                                continue
                            section = section_hdr + stream.read(total - 9)
                            if len(section) != total:
                                im_sections["truncated_first_section"] += 1
                                continue
                            checksum = struct.unpack_from("<I", section, total - 4)[0]
                            if zlib.adler32(section[:-4]) == checksum:
                                im_sections["valid_adler32"] += 1
                            else:
                                im_sections["checksum_mismatch"] += 1
                            if scheme == 0 and uncompressed == total - 13:
                                im_sections["uncompressed_length_matches"] += 1
                                if total >= 18:
                                    im_sections["first_object_type_" +
                                                str(section[9])] += 1
                            else:
                                im_sections["custom_or_compressed_length"] += 1
                            continue
                        if magic != MAGIC:
                            results["non_jsr184_magic"] += 1
                            continue
                        hdr = stream.read(9)
                        if len(hdr) != 9:
                            results["truncated_section"] += 1
                            continue
                        section_size = struct.unpack_from("<I", hdr, 1)[0]
                        if section_size < 13 or section_size > MAX_HEADER_SECTION:
                            results["bad_section_size"] += 1
                            continue
                        section = hdr + stream.read(section_size - 9)
                        reason, evidence = verify_prefix(magic, section, info.file_size)
                        results[reason] += 1
                        if evidence.get("version"):
                            versions[evidence["version"]] += 1
                        if evidence.get("external_references"):
                            external += 1
                        if reason == "valid_jsr184_header":
                            # Inspect only metadata of the next section, never
                            # export its content or original entry name.
                            next_header = stream.read(9)
                            if len(next_header) < 9:
                                next_sections["missing_second_section"] += 1
                            else:
                                method = next_header[0]
                                second_len, second_plain = struct.unpack_from(
                                    "<II", next_header, 1)
                                next_sections["compression_" + str(method)
                                              if method in (0, 1)
                                              else "reserved_compression"] += 1
                                if (method in (0, 1) and second_len >= 13 and
                                        second_len <= info.file_size -
                                            (12 + section_size)):
                                    if method == 0:
                                        first = stream.read(5)
                                    else:
                                        compressed_prefix = stream.read(
                                            min(65536, second_len - 13))
                                        try:
                                            first = zlib.decompressobj().decompress(
                                                compressed_prefix, 5)
                                        except zlib.error:
                                            first = b""
                                    if len(first) >= 5:
                                        obj_type = first[0]
                                        first_len = struct.unpack_from("<I", first, 1)[0]
                                        next_sections["first_object_type_" +
                                                      str(obj_type)] += 1
                                        if first_len == 0:
                                            next_sections["first_object_zero_length"] += 1
                                        elif first_len <= max(0, second_plain - 5):
                                            next_sections["first_object_length_fits"] += 1
                                        else:
                                            next_sections["first_object_length_exceeds"] += 1
                                    else:
                                        next_sections["first_object_unavailable"] += 1
                                else:
                                    next_sections["second_section_header_invalid"] += 1
                except (OSError, RuntimeError, ValueError, EOFError, zlib.error):
                    results["read_error"] += 1
    except (OSError, ValueError):
        results["apk_error"] += 1
    return {
        "schema": 1, "kind": "public-jsr184-header-evidence",
        "candidate_m3g_files": examined,
        "header_result_counts": dict(sorted(results.items())),
        "validated_versions": dict(sorted(versions.items())),
        "validated_external_reference_headers": external,
        "second_section_probe_counts": dict(sorted(next_sections.items())),
        "ironmonkey_first_section_counts": dict(sorted(im_sections.items())),
        "limitations": "First M3G header section only, not an entire scene or renderer. No game data included.",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = analyze_archive(args.apk)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Wrote aggregate JSR-184 first-section checks (no original file names/data).")
    return 0 if "apk_error" not in report["header_result_counts"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
