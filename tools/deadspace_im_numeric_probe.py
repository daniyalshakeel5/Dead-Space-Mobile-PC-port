#!/usr/bin/env python3
"""Validate numeric IM-M3G mesh data and index bounds (aggregate-only).

No original coordinates, topology, names or assets are emitted or saved.
Actual rendering and mesh orientation remain unverified.
"""
from __future__ import annotations
from collections import Counter
import argparse
import json
import math
from pathlib import Path
import struct
from zipfile import ZipFile
if __package__:
    from .deadspace_im_geometry_probe import parse_objects, le32, MAX_SIZE
else:
    from deadspace_im_geometry_probe import parse_objects, le32, MAX_SIZE


def inspect_file(data: bytes) -> tuple[bool, Counter]:
    objects = parse_objects(data)
    if objects is None:
        return False, Counter()
    stats = Counter()
    for kind, mesh in objects:
        if kind != 14 or len(mesh) < 30:
            continue
        vb_ref = le32(mesh,22)
        if not (vb_ref and vb_ref<=len(objects) and objects[vb_ref-1][0]==21):
            stats["mesh_bad_vertexbuffer_ref"] += 1
            continue
        vb=objects[vb_ref-1][1]
        if len(vb)<48:
            stats["mesh_short_vertexbuffer"] += 1
            continue
        pos_ref=le32(vb,16)
        if not(pos_ref and pos_ref<=len(objects) and objects[pos_ref-1][0]==20):
            stats["mesh_bad_position_ref"] += 1
            continue
        va=objects[pos_ref-1][1]
        if len(va)<17:
            stats["mesh_short_position_array"] += 1
            continue
        component_size, components, encoding=va[12],va[13],va[14]
        vertex_count=struct.unpack_from("<H",va,15)[0]
        if (component_size,components,encoding) == (2,3,0) and (
                len(va)-17 == vertex_count*6 and vertex_count>0):
            stats["mesh_position_int16x3_exact"] += 1
        else:
            stats["mesh_position_other_encoding"] += 1
        floats=struct.unpack_from("<ffff",vb,20)
        if all(math.isfinite(x) for x in floats):
            stats["mesh_bias_and_scale_finite"] += 1
        else:
            stats["mesh_invalid_bias_or_scale"] += 1
        count=le32(mesh,26)
        if count>(len(mesh)-30)//4:
            stats["mesh_invalid_submesh_count"] += 1
            continue
        for n in range(count):
            ib_ref=le32(mesh,30+n*4)
            if not (ib_ref and ib_ref<=len(objects) and objects[ib_ref-1][0]==100):
                stats["mesh_bad_indexbuffer"] += 1
                continue
            ib=objects[ib_ref-1][1]
            if len(ib)<20:
                stats["mesh_short_indexbuffer"] += 1
                continue
            name_flag=le32(ib,8); at=12
            if name_flag==1:
                if len(ib)-at<8:
                    stats["mesh_name_header_short"] += 1
                    continue
                at+=4
                name_length=le32(ib,at); at+=4
                if name_length>len(ib)-at:
                    stats["mesh_name_length_invalid"] += 1
                    continue
                at+=name_length
            elif name_flag!=0:
                stats["mesh_unrecognized_name_flag"] += 1
                continue
            if len(ib)-at<8:
                stats["mesh_indexbuffer_short"] += 1
                continue
            strips_ref=le32(ib,at)
            if not(strips_ref and strips_ref<=len(objects) and objects[strips_ref-1][0]==11):
                stats["mesh_bad_strip_ref"] += 1
                continue
            strip=objects[strips_ref-1][1]
            if len(strip)<21:
                stats["mesh_strip_short"] += 1
                continue
            enc=strip[12]
            item_bytes={0x81:1,0x82:2}.get(enc)
            if not item_bytes:
                stats["mesh_strip_unknown_encoding"] += 1
                continue
            nindices=le32(strip,13)
            if nindices>(len(strip)-21)//item_bytes:
                stats["mesh_strip_indices_short"] += 1
                continue
            at=17+nindices*item_bytes
            nstrips=le32(strip,at); at+=4
            if nstrips>(len(strip)-at)//4:
                stats["mesh_strip_lengths_short"] += 1
                continue
            lengths=[le32(strip,at+i*4) for i in range(nstrips)]
            if sum(lengths)!=nindices or at+nstrips*4 != len(strip):
                stats["mesh_strip_lengths_inconsistent"] += 1
                continue
            if all(x==3 for x in lengths):
                stats["mesh_all_strips_three_indices"] += 1
            elif all(x>=3 for x in lengths):
                stats["mesh_has_longer_triangle_strips"] += 1
            else:
                stats["mesh_contains_short_strips"] += 1
            # Single aggregate test of all decoded indices against vertex count.
            raw=strip[17:17+nindices*item_bytes]
            if item_bytes==1:
                values=raw
            else:
                values=struct.unpack_from("<"+"H"*nindices,raw)
            if vertex_count and all(index<vertex_count for index in values):
                stats["mesh_indices_within_position_vertex_count"] += 1
            else:
                stats["mesh_indices_outside_position_vertex_count"] += 1
            stats["mesh_verified_index_buffers"] += 1
    return True,stats


def analyze(apk: Path) -> dict:
    file_counts,aggregate=Counter(),Counter()
    with ZipFile(apk) as zipf:
        for item in zipf.infolist():
            if not item.filename.startswith("assets/") or not item.filename.lower().endswith(".m3g"):
                continue
            if item.file_size < 30 or item.file_size>MAX_SIZE or item.flag_bits & 1:
                file_counts["invalid_size_or_flags"]+=1
                continue
            with zipf.open(item) as src:
                data=src.read(MAX_SIZE+1)
            if len(data)!=item.file_size:
                file_counts["archive_length_error"]+=1
                continue
            ok,counts=inspect_file(data)
            file_counts["im_framed" if ok else "other_m3g"]+=1
            aggregate.update(counts)
    return {
        "schema":1, "kind":"public-ironmonkey-numeric-geometry-evidence",
        "files":dict(sorted(file_counts.items())),
        "numeric_geometry_checks":dict(sorted(aggregate.items())),
        "limitations":"Bounds/finiteness/index-range tests only. No geometry export, rendering, winding, animation or native loader verified.",
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    report=analyze(a.apk)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("IM-M3G numeric mesh reference validation recorded; aggregate counts only.")


if __name__=="__main__":
    main()
