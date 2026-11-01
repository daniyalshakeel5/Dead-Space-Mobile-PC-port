#!/usr/bin/env python3
"""Run new native IronMonkey OBJ geometry decoder against original owned APK.

All extracted models and exported OBJ files are ephemeral runner-local; only
aggregate counts appear in reports. Never ship or upload original geometry.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
from zipfile import ZipFile

IM = bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
MAX_BYTES = 64*1024*1024
MAX_OBJ = 128*1024*1024


def verify_obj(path: Path, expected_v: int, expected_t: int) -> bool:
    if not path.exists() or path.stat().st_size > MAX_OBJ:
        return False
    verts=faces=0
    with path.open("r",encoding="utf-8") as src:
        for line in src:
            if line.startswith("v "):
                verts+=1
            elif line.startswith("f "):
                components=line.split()
                if len(components)!=4:
                    return False
                try:
                    indexes=[int(x) for x in components[1:]]
                except ValueError:
                    return False
                if any(i<=0 or i>expected_v for i in indexes):
                    return False
                faces+=1
    return verts==expected_v and faces==expected_t


def analyze(apk: Path, exe: Path) -> dict:
    statuses=Counter()
    totals=Counter()
    obj_checks=Counter()
    with ZipFile(apk) as archive, tempfile.TemporaryDirectory() as directory:
        local=Path(directory)/"input.m3g"
        output=Path(directory)/"model.obj"
        for info in archive.infolist():
            if not info.filename.startswith("assets/") or not info.filename.lower().endswith(".m3g"):
                continue
            if not (30 <= info.file_size <= MAX_BYTES) or info.flag_bits & 1:
                statuses["unsafe_size_or_flags"]+=1
                continue
            with archive.open(info) as src:
                signature=src.read(12)
            if signature != IM:
                statuses["other_m3g_magic"]+=1
                continue
            try:
                with archive.open(info) as src, local.open("wb") as dst:
                    shutil.copyfileobj(src,dst,64*1024)
                if local.stat().st_size != info.file_size:
                    statuses["input_length_mismatch"]+=1
                    continue
                res=subprocess.run([str(exe),str(local)],
                                   capture_output=True,text=True,check=False,timeout=60)
                if res.returncode!=0:
                    statuses["unsupported_geometry"]+=1
                    continue
                meta=json.loads(res.stdout)
                if not meta.get("valid"):
                    statuses["invalid_response"]+=1
                    continue
                statuses["native_mesh_decoded"]+=1
                for k in ("mesh_objects","submeshes","vertices","triangles"):
                    val=meta.get(k)
                    if type(val) is not int or val < 0:
                        raise ValueError("invalid numeric geometry metadata")
                    totals[k]+=val
                if (obj_checks["attempted"]<3 and info.file_size<8*1024*1024):
                    obj_checks["attempted"]+=1
                    converted=subprocess.run(
                        [str(exe),str(local),str(output)],
                        capture_output=True,text=True,check=False,timeout=60)
                    if converted.returncode==0 and verify_obj(
                            output,meta["vertices"],meta["triangles"]):
                        obj_checks["validated_obj"]+=1
                    else:
                        obj_checks["invalid_obj"]+=1
            except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired,
                    json.JSONDecodeError):
                statuses["read_or_native_error"]+=1
            finally:
                local.unlink(missing_ok=True)
                output.unlink(missing_ok=True)
    return {
        "schema":1,"kind":"public-native-ironmonkey-geometry-evidence",
        "native_reader_outcomes":dict(sorted(statuses.items())),
        "decoded_geometry_counts":dict(sorted(totals.items())),
        "temporary_obj_smoke_test":dict(sorted(obj_checks.items())),
        "limitations":"Geometry-only OBJ triangle lists and positions. No visual accuracy, normals, UVs, materials, rigging or native gameplay verified.",
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apk",required=True,type=Path)
    p.add_argument("--native-mesh",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    output=analyze(a.apk,a.native_mesh.resolve())
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(output,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("Native IronMonkey geometry verification complete; no models/OBJ uploaded.")


if __name__=="__main__":
    main()
