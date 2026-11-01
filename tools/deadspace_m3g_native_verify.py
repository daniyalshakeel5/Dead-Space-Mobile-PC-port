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
import struct
import zlib
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


def validate_rgba_png(path: Path, width: int, height: int) -> bool:
    """Validate transient PNG chunk CRCs and exact RGBA scanline count."""
    content = path.read_bytes()
    if len(content) > 64 * 1024 * 1024 or not content.startswith(
            b"\x89PNG\r\n\x1a\n"):
        return False
    pos = 8
    chunks = []
    header_ok = False
    ended = False
    while pos + 12 <= len(content):
        length = struct.unpack_from(">I", content, pos)[0]
        if length > len(content) - pos - 12:
            return False
        kind = content[pos + 4:pos + 8]
        payload = content[pos + 8:pos + 8 + length]
        expected_crc = struct.unpack_from(">I", content, pos + 8 + length)[0]
        if zlib.crc32(kind + payload) != expected_crc:
            return False
        pos += length + 12
        if kind == b"IHDR":
            if header_ok or len(payload) != 13:
                return False
            w, h, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload)
            header_ok = ((w, h, depth, color, compression, filtering, interlace)
                         == (width, height, 8, 6, 0, 0, 0))
        elif kind == b"IDAT":
            chunks.append(payload)
        elif kind == b"IEND":
            ended = True
            break
    if not (header_ok and ended and pos == len(content) and chunks):
        return False
    inflated_bytes = height * (width * 4 + 1)
    if inflated_bytes > 64 * 1024 * 1024:
        return False
    decoder = zlib.decompressobj()
    inflated = decoder.decompress(b"".join(chunks), inflated_bytes + 1)
    if not decoder.eof or len(inflated) != inflated_bytes:
        return False
    return all(inflated[y * (width * 4 + 1)] == 0 for y in range(height))


def analyze(apk: Path, native_probe: Path,
            texture_exporter: Path | None = None) -> dict:
    outcomes: Counter = Counter()
    diagnostic_outcomes: Counter = Counter()
    diagnostic_types: Counter = Counter()
    image2d_format_counts: Counter = Counter()
    image2d_failure_stages: Counter = Counter()
    image_decoding_counts: Counter = Counter()
    png_smoke_counts: Counter = Counter()
    decoded_rgba_bytes = 0
    image2d_objects = 0
    image2d_standard_payloads = 0
    image2d_unrecognized_payloads = 0
    image2d_max_width = 0
    image2d_max_height = 0
    image2d_validated_pixel_bytes = 0
    diagnostic_nonstandard_headers = 0
    diagnostic_reserved_objects = 0
    diagnostic_sections = 0
    diagnostic_objects = 0
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
                process = subprocess.run([str(native_probe), str(stage)],
                                         capture_output=True, text=True,
                                         check=False, timeout=30)
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
                    if reason == "unexpected second M3G header object":
                        exploratory = subprocess.run(
                            [str(native_probe), str(stage), "--diagnostic"],
                            capture_output=True, text=True, check=False, timeout=30)
                        if exploratory.returncode != 0:
                            diagnostic_outcomes["process_error"] += 1
                        else:
                            extra = json.loads(exploratory.stdout)
                            if not extra.get("diagnostic"):
                                diagnostic_outcomes["unexpected_mode"] += 1
                            elif extra.get("valid") is True:
                                diagnostic_outcomes["structural_diagnostic_pass"] += 1
                                diagnostic_nonstandard_headers += int(
                                    extra.get("nonstandard_header_objects", 0))
                                diagnostic_reserved_objects += int(
                                    extra.get("reserved_type_objects", 0))
                                diagnostic_sections += int(extra.get("sections", 0))
                                diagnostic_objects += int(extra.get("objects", 0))
                                image2d_objects += int(extra.get("image2d_objects", 0))
                                image2d_standard_payloads += int(
                                    extra.get("image2d_standard_payloads", 0))
                                image2d_unrecognized_payloads += int(
                                    extra.get("image2d_unrecognized_payloads", 0))
                                image2d_max_width = max(
                                    image2d_max_width, int(extra.get("image2d_max_width", 0)))
                                image2d_max_height = max(
                                    image2d_max_height, int(extra.get("image2d_max_height", 0)))
                                image2d_validated_pixel_bytes += int(
                                    extra.get("image2d_validated_pixel_bytes", 0))
                                for key, val in (extra.get("image2d_formats") or {}).items():
                                    if key in {"96", "97", "98", "99", "100"}:
                                        image2d_format_counts[key] += int(val)
                                for key, val in (extra.get("image2d_failure_stages") or {}).items():
                                    if key in {"0", "1", "2", "3", "4", "5", "6", "7"}:
                                        image2d_failure_stages[key] += int(val)
                                if int(extra.get("image2d_standard_payloads", 0)) > 0:
                                    decoded = subprocess.run(
                                        [str(native_probe), str(stage), "--decode-image"],
                                        capture_output=True, text=True, check=False, timeout=30)
                                    if decoded.returncode != 0:
                                        image_decoding_counts["native_error"] += 1
                                    else:
                                        payload = json.loads(decoded.stdout)
                                        if payload.get("rgba_decoded") is True:
                                            image_decoding_counts["rgba_decoded"] += 1
                                            decoded_rgba_bytes += (
                                                int(payload["rgba_width"]) *
                                                int(payload["rgba_height"]) * 4)
                                            if (texture_exporter is not None and
                                                    png_smoke_counts["tested"] < 3):
                                                png_smoke_counts["tested"] += 1
                                                target = Path(directory) / "temporary-owned-image.png"
                                                try:
                                                    made = subprocess.run(
                                                        [str(texture_exporter), str(stage),
                                                         str(target)],
                                                        capture_output=True, text=True,
                                                        check=False, timeout=30)
                                                    if made.returncode == 0 and validate_rgba_png(
                                                            target, int(payload["rgba_width"]),
                                                            int(payload["rgba_height"])):
                                                        png_smoke_counts["verified_png"] += 1
                                                    else:
                                                        png_smoke_counts["invalid_png"] += 1
                                                finally:
                                                    target.unlink(missing_ok=True)
                                        else:
                                            image_decoding_counts["unsupported_image"] += 1
                                for key, value in (extra.get("type_counts") or {}).items():
                                    if key.isdecimal() and 0 <= int(key) <= 255:
                                        diagnostic_types[key] += int(value)
                            else:
                                reason2 = extra.get("reason", "")
                                diagnostic_outcomes[
                                    reason2 if reason2 in VALID_REASONS
                                    else "unknown_diagnostic_error"] += 1
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
        "diagnostic_outcomes": dict(sorted(diagnostic_outcomes.items())),
        "diagnostic_nonstandard_header_objects": diagnostic_nonstandard_headers,
        "diagnostic_reserved_type_objects": diagnostic_reserved_objects,
        "diagnostic_total_sections": diagnostic_sections,
        "diagnostic_total_objects": diagnostic_objects,
        "diagnostic_object_type_counts": dict(
            sorted(diagnostic_types.items(), key=lambda p: int(p[0]))),
        "image2d_objects": image2d_objects,
        "image2d_jsr184_payloads": image2d_standard_payloads,
        "image2d_unrecognized_payloads": image2d_unrecognized_payloads,
        "image2d_max_width": image2d_max_width,
        "image2d_max_height": image2d_max_height,
        "image2d_validated_pixel_bytes": image2d_validated_pixel_bytes,
        "image2d_jsr184_format_counts": dict(sorted(image2d_format_counts.items())),
        "image2d_failure_stage_counts": dict(sorted(image2d_failure_stages.items())),
        "image_rgba_decode_outcomes": dict(sorted(image_decoding_counts.items())),
        "image_rgba_decoded_bytes": decoded_rgba_bytes,
        "temporary_png_smoke_test": dict(sorted(png_smoke_counts.items())),
        "limitations": "Strict JSR-184 results separate from diagnostic opaque object framing. Diagnostic passes are NOT validated model decoders or gameplay.",
    }


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--apk", type=Path, required=True)
    p.add_argument("--native-probe", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--texture-exporter", type=Path)
    args = p.parse_args()
    report = analyze(args.apk, args.native_probe.resolve(),
                     args.texture_exporter.resolve() if args.texture_exporter else None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print("Native scene-container verification completed; only aggregate counts saved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
