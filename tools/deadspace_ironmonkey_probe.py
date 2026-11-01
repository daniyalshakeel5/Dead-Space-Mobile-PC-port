#!/usr/bin/env python3
"""Aggregate-only IronMonkey IM-M3G chunk-framing reconnaissance.

Tests bounded structure hypotheses, informed by public community research.
Never emits resource bytes, model names, ELF symbols or private file paths.
An object-header match is not a mesh decoder or gameplay verification.
"""
from __future__ import annotations
from collections import Counter
import argparse
import json
from pathlib import Path
import struct
from zipfile import ZipFile
import zlib

MAGIC = bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
MAX_FILE = 64 * 1024 * 1024
MAX_ENTRIES = 500
MAX_OBJECTS = 20000


def little(data: bytes, at: int) -> int:
    if at < 0 or len(data) - at < 4:
        raise ValueError("truncated integer")
    return struct.unpack_from("<I", data, at)[0]


def parse_chunks(data: bytes, start: int, end: int) -> tuple[bool, Counter]:
    if start < 0 or end < start or end > len(data):
        return False, Counter()
    off = start
    count = 0
    kinds = Counter()
    while off < end:
        if end - off < 5 or count >= MAX_OBJECTS:
            return False, Counter()
        kind = data[off]
        length = little(data, off + 1)
        if length > end - off - 5:
            return False, Counter()
        kinds[str(kind)] += 1
        count += 1
        off += 5 + length
    return bool(count) and off == end, kinds


def probe_file(data: bytes) -> tuple[str, dict]:
    if not data.startswith(MAGIC) or len(data) < 30:
        return "not_im_m3g", {}
    section_method = data[12]
    section_size = little(data, 13)
    plain_size = little(data, 17)
    info = {
        "scheme": section_method,
        "section_within_file": 13 <= section_size <= len(data) - 12,
        "length_matches": section_size >= 13 and plain_size == section_size - 13,
        "section_exact_file": section_size == len(data) - 12,
    }
    if info["section_within_file"]:
        end = section_size + 12
        info["checksum_matches"] = (
            zlib.adler32(data[12:end - 4]) == little(data, end - 4))
    else:
        info["checksum_matches"] = False
    # Distinct alternatives are evidence hypotheses, not assumptions.
    candidates = {
        "section_payload_minus_checksum": (21, 12 + section_size - 4),
        "whole_section_including_checksum": (21, 12 + section_size),
        "section_length_as_absolute": (21, section_size),
        "whole_file_minus_four": (21, len(data)-4),
        "whole_file": (21, len(data)),
        "shifted_object_start_25": (25, len(data)-4),
    }
    layouts = {}
    for label, (start,end) in candidates.items():
        ok, types = parse_chunks(data,start,end)
        if ok:
            layouts[label] = types
    info["layouts"] = layouts
    return "im_m3g", info


def analyze(apk: Path) -> dict:
    statuses = Counter()
    method_counts = Counter()
    section_flags = Counter()
    layout_counts = Counter()
    object_counts = Counter()
    object_types = Counter()
    candidates = 0
    with ZipFile(apk) as z:
        for member in z.infolist():
            if not member.filename.startswith("assets/") or not member.filename.lower().endswith(".m3g"):
                continue
            candidates += 1
            if candidates > MAX_ENTRIES:
                statuses["candidate_limit"] += 1
                break
            if member.file_size > MAX_FILE or member.file_size < 30 or member.flag_bits & 1:
                statuses["unsafe_or_oversize"] += 1
                continue
            with z.open(member) as stream:
                data = stream.read(MAX_FILE + 1)
            if len(data) != member.file_size:
                statuses["size_mismatch"] += 1
                continue
            family, info = probe_file(data)
            statuses[family] += 1
            if family != "im_m3g":
                continue
            method_counts[str(info["scheme"]) if info["scheme"] in (0,1)
                          else "other"] += 1
            for label in ("section_within_file", "length_matches",
                          "section_exact_file", "checksum_matches"):
                if info[label]:
                    section_flags[label] += 1
            for layout, kinds in info["layouts"].items():
                layout_counts[layout] += 1
                if layout == "section_payload_minus_checksum":
                    object_counts[str(sum(kinds.values()))] += 1
                    object_types.update(kinds)
    return {
        "schema":1, "kind":"public-ironmonkey-object-frame-evidence",
        "m3g_candidate_files":candidates,
        "family_classification":dict(sorted(statuses.items())),
        "section_method":dict(sorted(method_counts.items())),
        "standard_section_conditions_true":dict(sorted(section_flags.items())),
        "candidate_layout_successes":dict(sorted(layout_counts.items())),
        "standard_section_object_count_histogram":dict(sorted(object_counts.items())),
        "standard_section_object_type_histogram":dict(
            sorted(object_types.items(), key=lambda p:int(p[0]))),
        "limitations": "Counts of framing-compatible chunks only; no verified proprietary mesh semantics, no original bytes/names.",
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    args=p.parse_args()
    report=analyze(args.apk)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",
                           encoding="utf-8")
    print("IronMonkey object-framing census finished (aggregates only).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
