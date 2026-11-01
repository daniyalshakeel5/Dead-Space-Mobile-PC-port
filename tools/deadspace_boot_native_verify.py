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

def menu_vertex_binding_evidence(blob:bytes)->dict:
    """Aggregate original VertexBuffer/UV/Texture/Appearance relationships.
    All records are numeric metadata only; no game asset bytes released.
    """
    magic=bytes.fromhex("ab494d2d4d3347bb0d0a1a0a")
    if len(blob)<30 or blob[:12]!=magic:return {"recognized":False}
    objects=[]
    at=21
    while at<len(blob)-4 and len(objects)<20000:
        if len(blob)-4-at<5:return {"framing_ok":False}
        typ=blob[at];n=struct.unpack_from("<I",blob,at+1)[0]
        if n>len(blob)-4-at-5:return {"framing_ok":False}
        objects.append((typ,blob[at+5:at+5+n]))
        at+=5+n
    if at!=len(blob)-4:return {"framing_ok":False}
    lengths={}
    bindings=[]
    arrays={}
    for ix,(typ,data) in enumerate(objects,1):
        if typ==20 and len(data)>=17:
            arrays[ix]={"component_size":data[12],
                        "dimensions":data[13],
                        "encoding":data[14],
                        "vertex_count":struct.unpack_from("<H",data,15)[0],
                        "data_size":len(data)}
        if typ==21:
            lengths[len(data)]=lengths.get(len(data),0)+1
            if len(data)>=48:
                texnum=struct.unpack_from("<I",data,44)[0]
                refs=[]
                if texnum<=8 and len(data)>=48+20*texnum:
                    for slot in range(texnum):
                        array_id=struct.unpack_from("<I",data,48+slot*20)[0]
                        refs.append({
                            "array_valid":array_id in arrays or
                                (1<=array_id<=len(objects) and
                                 objects[array_id-1][0]==20),
                            "layout":arrays.get(array_id,{}),
                            "array_id":array_id,
                        })
                bindings.append({
                    "vertexbuffer_id":ix,
                    "source_bytes":len(data),
                    "texcoord_sets":texnum,
                    "position_array":struct.unpack_from("<I",data,16)[0],
                    "normal_array":struct.unpack_from("<I",data,36)[0],
                    "color_array":struct.unpack_from("<I",data,40)[0],
                    "uv_refs":refs,
                })
    texcoord_counts={}
    uv_formats={}
    uv_valid=0
    for v in bindings:
        n=v["texcoord_sets"]
        texcoord_counts[str(n)]=texcoord_counts.get(str(n),0)+1
        for a in v["uv_refs"]:
            if a["array_valid"]:
                uv_valid+=1
                f=a["layout"]
                key=str((f.get("component_size"),f.get("dimensions"),
                         f.get("encoding")))
                uv_formats[key]=uv_formats.get(key,0)+1
    scene_types={}
    for typ,data in objects:
        if typ in (3,5,9,17,19):
            scene_types[str(typ)]=scene_types.get(str(typ),0)+1
    return {"framing_ok":True,"vertexbuffer_lengths":lengths,
        "texture_coordinate_sets":texcoord_counts,
        "valid_coordinate_array_refs":uv_valid,
        "texture_coordinate_formats":uv_formats,
        "scene_object_types":scene_types,
        "first_five_vertex_bindings":bindings[:5]}

def inspect(apk:Path,probe:Path,im_probe:Path|None=None,ui_probe:Path|None=None,menu_probe:Path|None=None)->dict:
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
                im["vertex_bindings"]=menu_vertex_binding_evidence(
                    target.read_bytes())
                if menu_probe:
                    geometry=subprocess.run([str(menu_probe),str(target)],
                        capture_output=True,text=True,timeout=60,check=False)
                    im["native_menu_uv_supported"]=geometry.returncode==0
                    if geometry.returncode==0:
                        im["native_menu_uv_report"]=json.loads(geometry.stdout)
                im["native_mesh_supported"]=checked.returncode==0
                if checked.returncode==0:
                    im["native_mesh_report"]=json.loads(checked.stdout)
            ui_decoded={}
            if (path.startswith("assets/published/texturepacks_ui/") and ui_probe):
                checked=subprocess.run([str(ui_probe),str(target)],
                    capture_output=True,text=True,timeout=60,check=False)
                ui_decoded={"verified":checked.returncode==0}
                if checked.returncode==0:
                    ui_decoded["original_images"]=json.loads(
                        checked.stdout).get("images",[])
            results.append({
                "native_im_ui_decoder":ui_decoded,
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
    parser.add_argument("--ui-probe",type=Path)
    parser.add_argument("--menu-probe",type=Path)
    args=parser.parse_args()
    print("ORIGINAL_BOOT_NATIVE_DECODER_AUDIT",json.dumps(
        inspect(args.apk,args.probe,args.im_probe,args.ui_probe,args.menu_probe),separators=(",",":")))
if __name__=="__main__":main()
