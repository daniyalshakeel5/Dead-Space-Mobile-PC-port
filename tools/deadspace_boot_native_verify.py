#!/usr/bin/env python3
"""Verify native texture decoder against original splash/menu files.

On a temporary GitHub runner ONLY. Emits metadata, never images,
pixels, game file contents or ELF bytes. Extracted files auto-delete.
"""
import argparse
import json
import struct
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
def scan_im_objects(blob:bytes)->dict:
    # Original IM-M3G framing already independently reconstructed
    # in src/im_mesh.cpp. Inspect metadata only; no game data output.
    magic=bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
    if len(blob)<30 or blob[:12]!=magic or blob[12]!=0 or (
        struct.unpack_from("<I",blob,13)[0]!=len(blob)-12):
        return {"recognized":False}
    counts={}
    sizes={}
    records=[]
    at=21
    end=len(blob)-4
    while at<end and len(records)<20000:
        if end-at<5:return {"recognized":True,"framing_ok":False}
        typ=blob[at]
        n=struct.unpack_from("<I",blob,at+1)[0]
        if n>end-at-5:return {"recognized":True,"framing_ok":False}
        start=at+5
        counts[str(typ)]=counts.get(str(typ),0)+1
        sizes[str(typ)]=sizes.get(str(typ),0)+n
        if typ==10 or (len(records)<6 and len(blob)<300000):
            # Numeric preview of object metadata only.
            fields=[struct.unpack_from("<I",blob,start+4*i)[0]
                for i in range(min(12,n//4))]
            records.append({"index":len(records)+1,"type":typ,
                "length":n,"fields32":fields[:8]})
        else:records.append({"index":len(records)+1,"type":typ,"length":n})
        at=start+n
    return {"recognized":True,"framing_ok":at==end,
        "object_count":len(records),
        "types":counts,"type_bytes":sizes,
        "selected_fields":[item for item in records
            if item["type"]==10][:6] or records[:6]}

def inspect(apk:Path,probe:Path,im_probe:Path|None=None)->dict:
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
            # A published independent texture-format script identifies
            # an image header around 0x60. Probe numeric dimensions
            # and lengths only; never log original game bytes.
            header={}
            if info.file_size>=0x74:
                with target.open("rb") as original:
                    original.seek(0x60)
                    chunk=original.read(20)
                u,w,h,zero,n=struct.unpack("<IIIII",chunk)
                plausible=(0<w<=8192 and 0<h<=8192 and
                           n>0 and n<=info.file_size-0x74)
                header={
                    "field0":u,"width":w,"height":h,
                    "field3":zero,"encoded_bytes":n,
                    "plausible":plausible,
                    "etc1_expected_bytes":((w+3)//4)*((h+3)//4)*8 if plausible else 0,
                    "rgba_expected_bytes":w*h*4 if plausible else 0,
                    "trailing_bytes":info.file_size-0x74-n if plausible else None
                }
            # This game also uses the original IronMonkey
            # IM-M3G framing for proprietary splash/UI packs.
            # Report type counts and numeric object metadata only.
            im=scan_im_objects(target.read_bytes())
            # Verify that the existing native mesh decoder can
            # load the REAL main-menu scene. It prints counts only.
            if path=="assets/published/models/main_menu.m3g" and im_probe:
                checked=subprocess.run([str(im_probe),str(target)],
                    capture_output=True,text=True,timeout=60,check=False)
                im["native_mesh_supported"]=checked.returncode==0
                if checked.returncode==0:
                    im["native_mesh_report"]=json.loads(checked.stdout)
            results.append({
                "im_m3g":im,
                "candidate_offset_0x60":header,
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
    parser.add_argument("--im-probe",type=Path)
    args=parser.parse_args()
    print("ORIGINAL_BOOT_NATIVE_DECODER_AUDIT",json.dumps(
        inspect(args.apk,args.probe,args.im_probe),separators=(",",":")))
if __name__=="__main__":main()
