#!/usr/bin/env python3
"""Verify native texture decoder against original splash/menu files.

On a temporary GitHub runner ONLY. Emits metadata, never images,
pixels, game file contents or ELF bytes. Extracted files auto-delete.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from zipfile import ZipFile

ASSETS=(
    "assets/published/texturepacks_ui/splash_ea.m3g",
    "assets/published/texturepacks_ui/splash_deadspace.m3g",
    "assets/published/texturepacks_ui/ui_frontend.m3g",
    "assets/published/models/main_menu.m3g",
    "assets/published/models/texture_env_main_menu_01.m3g",
    "assets/published/models/texture_env_main_menu_objects.m3g",
    "assets/published/models/texture_env_main_menu_objects_02.m3g",
    "assets/published/models/texture_env_main_menu_starfield.m3g",
    "assets/published/textures/texture_env_main_menu_01.m3g",
    "assets/published/textures/texture_env_main_menu_objects.m3g",
    "assets/published/textures/texture_env_main_menu_objects_02.m3g",
    "assets/published/textures/texture_env_main_menu_starfield.m3g",
)
def inspect(apk:Path,probe:Path)->dict:
    results=[]
    with ZipFile(apk) as archive, tempfile.TemporaryDirectory(prefix="ds-boot-") as root:
        for number,path in enumerate(ASSETS):
            try:info=archive.getinfo(path)
            except KeyError:
                results.append({"path":path,"present":False})
                continue
            if info.file_size>64*1024*1024 or info.file_size<24:
                results.append({"path":path,"present":True,"status":"unsafe_size"})
                continue
            # The resource bytes only exist in a temporary runner file.
            target=Path(root)/(str(number)+".m3g")
            with archive.open(info) as src,target.open("wb") as dest:
                while part:=src.read(65536):dest.write(part)
            if target.stat().st_size!=info.file_size:
                raise RuntimeError("original resource size mismatch")
            call=subprocess.run([str(probe),str(target),"--decode-image"],
                capture_output=True,text=True,timeout=60,check=True)
            decoded=json.loads(call.stdout)
            results.append({
                "path":path,"present":True,"size":info.file_size,
                "valid_diagnostic_container":decoded["valid"],
                "type_counts":decoded["type_counts"],
                "image2d_objects":decoded["image2d_objects"],
                "image2d_formats":decoded["image2d_formats"],
                "RGBA_decoded":decoded.get("rgba_decoded",False),
                "RGBA_dimensions":[decoded.get("rgba_width",0),decoded.get("rgba_height",0)],
                "ETC1_decoded":decoded.get("etc1_decoded",False),
                "ETC1_dimensions":[decoded.get("etc1_width",0),decoded.get("etc1_height",0)],
                "error":decoded.get("reason",""),
            })
    return {"assets":results}
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--apk",type=Path,required=True)
    parser.add_argument("--probe",type=Path,required=True)
    args=parser.parse_args()
    print("ORIGINAL_BOOT_NATIVE_DECODER_AUDIT",json.dumps(
        inspect(args.apk,args.probe),separators=(",",":")))
if __name__=="__main__":main()
