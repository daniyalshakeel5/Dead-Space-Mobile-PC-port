#!/usr/bin/env python3
"""Inspect the lifted host's GL logging backend without accessing game assets.

Recognizes the native gl_gen.py logger's decimal GL enums, reports upload
dimensions and GLES upload formats, and explicitly does *not* infer which
source .m3g file produced each upload. An ETC1 file may appear as plain RGB
because the desktop shim decompresses it before glTexImage2D is logged.

Original asset format bytes 100 and 116 are IronMonkey M3G Image2D values;
they are NOT OpenGL enums, so never interpret them as GL upload formats.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import re

MAX_TRACE_BYTES=48*1024*1024
UPLOAD=re.compile(r"^\s*(glTexImage2D|glCompressedTexImage2D)\(([^)]*)\)")
FORMATS={0x1907:"GL_RGB",0x1908:"GL_RGBA",0x8D64:"GL_ETC1_RGB8_OES",
         0x1906:"GL_ALPHA",0x1909:"GL_LUMINANCE",0x190A:"GL_LUMINANCE_ALPHA"}
PIX_TYPES={0x1401:"GL_UNSIGNED_BYTE",0x8363:"GL_UNSIGNED_SHORT_5_6_5",
           0x8033:"GL_UNSIGNED_SHORT_4_4_4_4"}

def value(text:str)->int:
    text=text.strip()
    if not re.fullmatch(r"(0[xX][0-9a-fA-F]+|[0-9]+)",text):
        raise ValueError(f"Not a GL integer: {text!r}")
    return int(text,16 if text.lower().startswith("0x") else 10)

def parse_log(log:str)->dict:
    formats=Counter()
    formats_levels=Counter()
    dims=Counter()
    warnings=[]
    total=0
    invalid=0
    for no,line in enumerate(log.splitlines(),1):
        m=UPLOAD.match(line)
        if not m: continue
        name,args=m.groups()
        fields=[x.strip() for x in args.split(",")]
        # glTexImage2D(target,level,internalfmt,width,height,border,fmt,type,ptr)
        # glCompressedTexImage2D(target,level,format,width,height,border,size,ptr)
        wanted=9 if name=="glTexImage2D" else 8
        if len(fields)!=wanted:
            invalid+=1
            if len(warnings)<20: warnings.append(f"Line {no}: expected {wanted} arguments for {name}, got {len(fields)}")
            continue
        try:
            level=value(fields[1])
            internal_format=value(fields[2])
            width=value(fields[3]); height=value(fields[4])
            if width>16384 or height>16384:
                raise ValueError("unreasonable upload dimensions")
            fmt=value(fields[6]) if name=="glTexImage2D" else internal_format
            typ=value(fields[7]) if name=="glTexImage2D" else 0
        except ValueError as err:
            invalid+=1
            if len(warnings)<20: warnings.append(f"Line {no}: {err}")
            continue
        family=FORMATS.get(internal_format,hex(internal_format))
        ext=(FORMATS.get(fmt,hex(fmt)) if name=="glTexImage2D" else "COMPRESSED")
        ty=PIX_TYPES.get(typ,hex(typ)) if name=="glTexImage2D" else "COMPRESSED"
        key=f"{name}:{family}:{ext}:{ty}"
        formats[key]+=1
        formats_levels[f"{name}:level={level}"]+=1
        dims[f"{width}x{height}"]+=1
        total+=1
    return {"total_gl_uploads":total, "invalid_upload_records":invalid,
            "upload_api_and_formats":dict(sorted(formats.items())),
            "mip_level_counts":dict(sorted(formats_levels.items())),
            "top_dimensions":dict(dims.most_common(30)),
            "warnings":warnings,
            "scope_notes":[
               "This classifies GPU uploads, not original .m3g files.",
               "The GLES1-to-GL2.1 shim may decode ETC1 to RGB before GL logging.",
               "JSR184 Image2D formats 100 (RGBA) / 116 (proprietary ETC1)"
               " are resource bytes, not OpenGL format enums.",
               "Cannot match a draw/upload to source resource without an"
               " asset-open-to-GL-load correlation ID.",
             ]}

def main()->None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("trace",type=Path)
    ap.add_argument("--output",type=Path,default=None)
    args=ap.parse_args()
    if args.trace.stat().st_size>MAX_TRACE_BYTES:
        ap.error("Trace exceeds 48 MiB; please split into shorter sessions")
    report=parse_log(args.trace.read_text(encoding="utf-8",errors="replace"))
    text=json.dumps(report,indent=2)+"\n"
    if args.output:
        args.output.write_text(text,encoding="utf-8")
    else:
        print(text,end="")

if __name__=="__main__":
    main()
