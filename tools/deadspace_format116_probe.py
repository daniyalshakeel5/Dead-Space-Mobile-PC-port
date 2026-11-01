#!/usr/bin/env python3
"""Check proprietary Image2D code 116 against ETC block-layout hypotheses.

Reads authorized user-identified upstream APK exclusively in GitHub runner RAM.
Exports ONLY aggregate counters; NEVER game bytes, names, symbols or hashes.
No texture decode or visual/algorithm correctness is claimed by this script.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
from zipfile import ZipFile
import zlib

MAGIC = bytes.fromhex("ab4a5352313834bb0d0a1a0a")
MAX_FILE = 64 * 1024 * 1024
MAX_ENTRIES = 500


def u32(buf: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(buf):
        raise ValueError("truncated u32")
    return struct.unpack_from("<I", buf, off)[0]


def sections(data: bytes) -> tuple[bytes, bytes] | None:
    if not data.startswith(MAGIC):
        return None
    off = 12
    result = []
    for _ in range(2):
        if off + 13 > len(data) or data[off] != 0:
            return None
        total = u32(data, off + 1)
        size = u32(data, off + 5)
        if total < 13 or total > MAX_FILE or total > len(data) - off:
            return None
        if size != total - 13:
            return None
        part = data[off:off + total]
        if zlib.adler32(part[:-4]) != u32(part, total - 4):
            return None
        result.append(part[9:-4])
        off += total
    if off != len(data):
        return None
    return result[0], result[1]


def image_payload(data: bytes) -> tuple[bytes, int] | None:
    pair = sections(data)
    if pair is None:
        return None
    first, second = pair
    if len(first) < 5 or first[0] != 0 or u32(first, 1) != len(first) - 5:
        return None
    if len(second) < 15 or second[0] != 0:
        return None
    padding = u32(second, 1)
    if padding > len(second) - 10:
        return None
    image_at = 5 + padding
    if second[image_at] != 10:
        return None
    length = u32(second, image_at + 1)
    if length != len(second) - image_at - 5:
        return None
    payload = second[image_at + 5:]
    absolute = 12 + len(first) + 13 + 9 + image_at + 5
    return payload, absolute


def read_116(data: bytes) -> tuple[str, dict]:
    match = image_payload(data)
    if match is None:
        return "no_standard_two_section_image", {}
    blob, absolute = match
    try:
        off = 0
        user_id, animation_count = u32(blob, 0), u32(blob, 4)
        if animation_count > (len(blob) - 8) // 4:
            return "invalid_superclass", {}
        off = 8 + animation_count * 4
        params = u32(blob, off)
        off += 4
        if params > (len(blob) - off) // 8:
            return "invalid_superclass", {}
        for _ in range(params):
            parameter_id = u32(blob, off)
            length = u32(blob, off + 4)
            off += 8
            if length > len(blob) - off:
                return "invalid_superclass", {}
            off += length
        if len(blob) - off < 10:
            return "invalid_metadata", {}
        fmt = blob[off]
        fmt_abs = absolute + off
        flag = blob[off+1]
        width = u32(blob, off + 2)
        height = u32(blob, off + 6)
        off += 10
        if fmt != 116:
            return "different_format", {}
        result = {"flag": flag, "w": width, "h": height, "format_offset":fmt_abs}
        if not (0 < width <= 8192 and 0 < height <= 8192):
            return "dimensions_invalid", result
        if flag > 1:
            return "non_boolean_mutable", result
        if flag == 1:
            return ("mutable_no_data" if off == len(blob) else "mutable_trailing"), result
        palette_len = u32(blob, off); off += 4
        if palette_len > len(blob) - off:
            return "palette_length_invalid", result
        off += palette_len
        pixel_len = u32(blob, off); off += 4
        if pixel_len > len(blob) - off:
            return "pixel_length_invalid", result
        result.update({
            "palette_len": palette_len, "pixel_len": pixel_len,
            "pixel_abs": absolute + off, "remaining":len(blob)-off,
            "trailing":len(blob)-off-pixel_len,
        })
        expected_etc1 = ((width + 3)//4) * ((height + 3)//4) * 8
        expected_etc2_rgba = expected_etc1 * 2
        mip_etc1 = 0
        mw, mh = width, height
        while True:
            mip_etc1 += ((mw + 3)//4) * ((mh + 3)//4) * 8
            if mw == 1 and mh == 1:
                break
            mw, mh = max(1, mw//2), max(1, mh//2)
        result["size_class"] = ("etc1_exact" if pixel_len == expected_etc1
                                else "etc1_full_mip_chain" if pixel_len == mip_etc1
                                else "etc_rgba8_exact" if pixel_len == expected_etc2_rgba
                                else "etc_rgba8_full_mip_chain" if pixel_len == 2*mip_etc1
                                else "raw_rgb_exact" if pixel_len == width*height*3
                                else "raw_rgba_exact" if pixel_len == width*height*4
                                else "other")
        result["etc1_ratio_pct"] = round(100 * pixel_len / expected_etc1)
        result["mip_ratio_pct"] = round(100 * pixel_len / mip_etc1)
        result["matches_noesis_offset_0x74"] = (absolute + off == 0x74)
        return ("structural_payload_exact" if result["trailing"] == 0
                else "payload_trailing_bytes"), result
    except (IndexError, ValueError, struct.error):
        return "metadata_bounds_error", {}


def analyze(apk: Path) -> dict:
    status = Counter()
    flags, sizes, palettes, offsets, dims = (Counter() for _ in range(5))
    ratios: Counter = Counter()
    mip_ratios: Counter = Counter()
    candidate_count = 0
    with ZipFile(apk) as archive:
        for info in archive.infolist():
            if not info.filename.startswith("assets/") or not info.filename.lower().endswith(".m3g"):
                continue
            candidate_count += 1
            if candidate_count > MAX_ENTRIES:
                status["file_limit_reached"] += 1
                break
            if info.file_size > MAX_FILE or info.file_size < 25 or info.flag_bits & 1:
                status["file_size_or_flag_skipped"] += 1
                continue
            with archive.open(info) as stream:
                data = stream.read(MAX_FILE + 1)
            if len(data) != info.file_size:
                status["zip_size_mismatch"] += 1
                continue
            key, fields = read_116(data)
            status[key] += 1
            if fields:
                flags[str(fields.get("flag", "not_read"))] += 1
                dims["valid_power_of_two" if fields.get("w",0) > 0 and
                     fields["w"] & (fields["w"]-1) == 0 and
                     fields.get("h",0)>0 and fields["h"] & (fields["h"]-1) == 0
                     else "other"] += 1
                if "size_class" in fields:
                    sizes[fields["size_class"]] += 1
                # Ratios only; never output individual original pixel lengths.
                ratios[str(fields["etc1_ratio_pct"])] += 1
                mip_ratios[str(fields["mip_ratio_pct"])] += 1
                    palettes["empty" if fields["palette_len"] == 0 else "nonempty"] += 1
                    offsets["exact_noesis_fixed_0x74" if fields[
                        "matches_noesis_offset_0x74"] else "variable"] += 1
    return {
        "schema": 1, "kind": "public-format116-structure-evidence",
        "m3g_candidates": candidate_count,
        "parsing_status": dict(sorted(status.items())),
        "mutability_field_counts": dict(sorted(flags.items())),
        "power_of_two_dimensions": dict(sorted(dims.items())),
        "compressed_block_size_hypotheses": dict(sorted(sizes.items())),
        "etc1_base_length_ratio_pct": dict(sorted(ratios.items())),
        "etc1_full_mip_length_ratio_pct": dict(sorted(mip_ratios.items())),
        "palette_presence": dict(sorted(palettes.items())),
        "pixel_start_offset_class": dict(sorted(offsets.items())),
        "limitations": "Size matches are structural evidence only, NOT confirmation of an ETC1 decoder or the original ARM loader.",
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    result=analyze(a.apk)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("Aggregate proprietary Image2D layout census recorded.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
