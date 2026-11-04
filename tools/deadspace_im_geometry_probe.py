#!/usr/bin/env python3
"""Validate IronMonkey mesh/vertex structural references on authorized APK.

Grounded in actual IM-M3G chunk framing plus published format research.
No geometry, names, coordinates or source assets leave the ephemeral runner.
Results are structural evidence; no mesh renderer or animated rig is claimed.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
from zipfile import ZipFile

MAGIC = bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
MAX_SIZE = 64 * 1024 * 1024
MAX_OBJECTS = 20000


def le32(data, off):
    if off < 0 or off + 4 > len(data):
        raise ValueError("truncated u32")
    return struct.unpack_from("<I", data, off)[0]


def parse_objects(data: bytes) -> list[tuple[int, memoryview]] | None:
    if not data.startswith(MAGIC) or len(data) < 30 or data[12] != 0:
        return None
    if le32(data, 13) != len(data) - 12:
        return None
    end = len(data)-4
    off = 21
    objects: list[tuple[int, memoryview]] = []
    src = memoryview(data)
    while off < end and len(objects) < MAX_OBJECTS:
        if end-off < 5:
            return None
        typ = data[off]
        ln = le32(data, off+1)
        if ln > end-off-5:
            return None
        objects.append((typ, src[off+5:off+5+ln]))
        off += 5+ln
    return objects if objects and off == end else None


def profile(data: bytes) -> dict:
    objects = parse_objects(data)
    if objects is None:
        return {"status": "not_framed", "counters": {}}
    hist = Counter()
    for typ, chunk in objects:
        if typ == 20:  # VertexArray; 12-byte Object3D superclass candidate
            if len(chunk) < 17:
                hist["vertexarray_short"] += 1
                continue
            comp_size, channels, encoding = chunk[12], chunk[13], chunk[14]
            vertices = struct.unpack_from("<H",chunk,15)[0]
            hist["va_comp_size_" + str(comp_size)] += 1
            hist["va_channels_" + str(channels)] += 1
            hist["va_encoding_" + str(encoding)] += 1
            if comp_size in (1,2,4) and 1 <= channels <= 4 and vertices > 0:
                hist["va_plausible_metadata"] += 1
                expected = comp_size*channels*vertices
                if len(chunk)-17 == expected:
                    hist["va_exact_payload_size"] += 1
                elif len(chunk)-17 < expected:
                    hist["va_shorter_than_raw"] += 1
                else:
                    hist["va_larger_than_raw"] += 1
            else:
                hist["va_invalid_metadata"] += 1
        elif typ == 21:  # VertexBuffer: 12-byte Object3D, RGBA default
            if len(chunk) < 48:
                hist["vertexbuffer_short"] += 1
                continue
            ref = le32(chunk,16)
            count = le32(chunk,44)
            if ref > 0 and ref <= len(objects) and objects[ref-1][0] == 20:
                hist["vb_position_ref_to_vertexarray"] += 1
            else:
                hist["vb_position_ref_other"] += 1
            if count <= (len(chunk)-48)//20 and len(chunk)==48+count*20:
                hist["vb_uv_count_exact_layout"] += 1
            else:
                hist["vb_uv_count_unmatched_layout"] += 1
        elif typ == 14:  # Mesh, 22-byte superclass in public reverse engineering
            if len(chunk) < 30:
                hist["mesh_short"] += 1
                continue
            ref = le32(chunk,22)
            count = le32(chunk,26)
            if ref > 0 and ref <= len(objects) and objects[ref-1][0] == 21:
                hist["mesh_ref_to_vertexbuffer"] += 1
            else:
                hist["mesh_vb_ref_other"] += 1
            if count > (len(chunk)-30)//4:
                hist["mesh_index_table_invalid"] += 1
            else:
                hist["mesh_index_table_fits"] += 1
                if len(chunk)==30+4*count:
                    hist["mesh_index_table_exact_size"] += 1
                for n in range(count):
                    ix=le32(chunk,30+4*n)
                    if ix>0 and ix<=len(objects) and objects[ix-1][0] == 100:
                        hist["mesh_index_ref_to_im_indexbuffer"] += 1
                    else:
                        hist["mesh_index_ref_other"] += 1
        elif typ == 100:
            hist["im_indexbuffer_objects"] += 1
    return {"status": "framed", "counters": hist}


def analyze(apk: Path) -> dict:
    statuses=Counter()
    totals=Counter()
    with ZipFile(apk) as z:
        for info in z.infolist():
            if not info.filename.startswith("assets/") or not info.filename.lower().endswith(".m3g"):
                continue
            if info.file_size < 30 or info.file_size > MAX_SIZE or info.flag_bits & 1:
                statuses["skipped_size_or_flags"] += 1
                continue
            with z.open(info) as stream:
                data=stream.read(MAX_SIZE+1)
            if len(data)!=info.file_size:
                statuses["zip_size_mismatch"] += 1
                continue
            result=profile(data)
            statuses[result["status"]] += 1
            totals.update(result["counters"])
    return {
        "schema":1,"kind":"public-im-mesh-reference-evidence",
        "candidate_frame_status":dict(sorted(statuses.items())),
        "mesh_vertex_reference_counters":dict(sorted(totals.items())),
        "limitations":"Structural object and reference indexes only. No scene geometry, matrices, textures, animation or native game behavior verified.",
    }


def main() -> int:
    a=argparse.ArgumentParser()
    a.add_argument("--apk",type=Path,required=True)
    a.add_argument("--output",type=Path,required=True)
    arg=a.parse_args()
    result=analyze(arg.apk)
    arg.output.parent.mkdir(parents=True,exist_ok=True)
    arg.output.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("IronMonkey geometry structural reference census complete (counts only).")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
