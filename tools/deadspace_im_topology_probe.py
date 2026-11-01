#!/usr/bin/env python3
"""IronMonkey IM-M3G index topology framing/refs, metadata only.

Independent parser cross-checking public community format research.
No triangles, model names, geometry, or copyrighted assets are uploaded.
"""
from __future__ import annotations
from collections import Counter
import argparse
import json
from pathlib import Path
from zipfile import ZipFile
if __package__:
    from .deadspace_im_geometry_probe import parse_objects, le32, MAX_SIZE
else:
    from deadspace_im_geometry_probe import parse_objects, le32, MAX_SIZE


def topology(data: bytes) -> tuple[bool, Counter]:
    objects = parse_objects(data)
    if objects is None:
        return False, Counter()
    out = Counter()
    for typ, chunk in objects:
        if typ == 100:
            if len(chunk) < 20:
                out["im_index_short"] += 1
                continue
            flag = le32(chunk, 8)
            out["im_submesh_flag_" + str(flag) if flag in (0,1) else "im_submesh_flag_other"] += 1
            p = 12
            if flag == 1:
                if len(chunk)-p < 8:
                    out["im_named_submesh_short"] += 1
                    continue
                p += 4 # additional slot
                length = le32(chunk, p)
                p += 4
                if length > len(chunk)-p:
                    out["im_name_bounds_error"] += 1
                    continue
                p += length
            elif flag != 0:
                out["im_unrecognized_flag"] += 1
                continue
            if len(chunk)-p < 8:
                out["im_indices_short"] += 1
                continue
            strips, appearance = le32(chunk, p), le32(chunk, p+4)
            p += 8
            if p == len(chunk):
                out["im_indexbuffer_exact_size"] += 1
            else:
                out["im_indexbuffer_trailing"] += 1
            if strips and strips <= len(objects) and objects[strips-1][0] == 11:
                out["im_to_strip_reference_valid"] += 1
            else:
                out["im_to_strip_reference_other"] += 1
            if appearance and appearance <= len(objects) and objects[appearance-1][0] == 3:
                out["im_to_appearance_valid"] += 1
            elif appearance == 0:
                out["im_to_appearance_null"] += 1
            else:
                out["im_to_appearance_other"] += 1
        elif typ == 11:
            if len(chunk) < 21:
                out["triangle_strip_short"] += 1
                continue
            encoding = chunk[12]
            out["strip_encoding_" + str(encoding)] += 1
            count = le32(chunk, 13)
            item_bytes = {0x81:1, 0x82:2, 0x83:4}.get(encoding)
            if not item_bytes:
                out["strip_unsupported_encoding"] += 1
                continue
            p = 17
            if count > (len(chunk)-p-4)//item_bytes:
                out["strip_index_array_out_of_bounds"] += 1
                continue
            p += count*item_bytes
            count_strips = le32(chunk, p)
            p += 4
            if count_strips > (len(chunk)-p)//4:
                out["strip_lengths_out_of_bounds"] += 1
                continue
            total_indices = 0
            for i in range(count_strips):
                total_indices += le32(chunk,p+i*4)
            p += count_strips*4
            if p == len(chunk):
                out["strip_array_exact_size"] += 1
                if total_indices == count:
                    out["strip_lengths_sum_matches_indices"] += 1
                else:
                    out["strip_lengths_sum_differs"] += 1
            else:
                out["strip_array_trailing_bytes"] += 1
    return True, out


def analyze(apk: Path) -> dict:
    status, counts = Counter(), Counter()
    with ZipFile(apk) as z:
        for info in z.infolist():
            if not info.filename.startswith("assets/") or not info.filename.lower().endswith(".m3g"):
                continue
            if info.file_size < 30 or info.file_size > MAX_SIZE or info.flag_bits & 1:
                status["size_or_flags_skipped"] += 1
                continue
            with z.open(info) as f:
                data = f.read(MAX_SIZE+1)
            if len(data) != info.file_size:
                status["zip_size_mismatch"] += 1
                continue
            ok, evidence = topology(data)
            status["im_framed" if ok else "not_im_framed"] += 1
            counts.update(evidence)
    return {
        "schema":1,
        "kind":"public-im-triangle-topology-evidence",
        "files":dict(sorted(status.items())),
        "reference_and_index_layout_counts":dict(sorted(counts.items())),
        "limitations":"Counts-only topology metadata, not reconstructed triangles, animations, game scene or executable.",
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    report=analyze(a.apk)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("IronMonkey index topology evidence collected (aggregate only).")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
