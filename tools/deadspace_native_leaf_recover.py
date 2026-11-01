#!/usr/bin/env python3
"""Proof of exact instruction-level JNI no-op bodies from user's original ELF.

Only tiny ARM or Thumb returns are accepted. This does not classify a longer
routine from its name or approximate call graph.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from zipfile import ZipFile

TARGETS = (
    "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
    "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
    "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged",
    "Java_com_ea_blast_MainActivity_NativeOnPause",
    "Java_com_ea_blast_MainActivity_NativeOnResume",
)


def noop_return(code: bytes, thumb: bool) -> bool:
    if thumb:
        if len(code) < 2 or struct.unpack_from("<H", code)[0] != 0x4770:
            return False
        # Remaining bytes only alignment fillers, not an assumed second branch.
        trailing=code[2:]
        return not trailing or trailing in (b"\x00\xbf", b"\x00\x00")
    if len(code) < 4:
        return False
    ins=struct.unpack_from("<I", code)[0]
    return ins in (0xe12fff1e, 0xe1a0f00e) and (
        code[4:] in (b"", b"\x00\x00\x00\x00"))


def inspect(binary: bytes) -> dict:
    if len(binary) < 52 or binary[:6] != b"\x7fELF\x01\x01":
        raise ValueError("expected ELF32 little-endian original lib")
    header_off=struct.unpack_from("<I",binary,32)[0]
    entsize,count=struct.unpack_from("<HH",binary,46)
    if entsize<40 or count>4096 or header_off+entsize*count>len(binary):
        raise ValueError("invalid section table bounds")
    sec=[]
    for n in range(count):
        vals=struct.unpack_from("<IIIIIIIIII",binary,header_off+n*entsize)
        _,typ,_,addr,off,length,link,_,_,step=vals
        if typ != 8 and (off>len(binary) or length>len(binary)-off):
            raise ValueError("invalid section span")
        sec.append((typ,addr,off,length,link,step))
    results={}
    for typ,_,off,length,link,step in sec:
        if typ!=11 or link>=len(sec) or step<16 or step>128:
            continue
        _,_,names_off,names_len,_,_=sec[link]
        for entry in range(off,off+length,step):
            if entry+16>off+length:break
            name_off,value,size,attrs,_,index=struct.unpack_from("<IIIBBH",binary,entry)
            if (attrs&15)!=2 or not size or index>=len(sec) or name_off>=names_len:
                continue
            start=names_off+name_off
            stop=binary.find(b"\x00",start,names_off+names_len)
            if stop<0 or stop-start>240:continue
            name=binary[start:stop].decode("ascii","ignore")
            if name not in TARGETS: continue
            _,addr,begin,span,_,_=sec[index]
            position=(value&~1)-addr
            if position<0 or size>128 or position+size>span:continue
            body=binary[begin+position:begin+position+size]
            proven=noop_return(body,bool(value&1))
            if name in results:results[name]=results[name] and proven
            else:results[name]=proven
    return {"schema":1,
            "kind":"original-arm-jni-noop-instruction-proof",
            "checked_targets":{name: results.get(name) for name in TARGETS},
            "proven_noop_count":sum(v is True for v in results.values()),
            "limitations":"Only single return-instruction no-ops; longer callbacks are unverified."}


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    with ZipFile(a.apk) as z:
        libs=[i for i in z.infolist() if i.filename.startswith("lib/")
              and i.filename.endswith(".so") and not i.is_dir()
              and i.file_size<=64*1024*1024]
        if len(libs)!=1:raise ValueError("expected a single original ARM library")
        with z.open(libs[0]) as f: binary=f.read(64*1024*1024+1)
        if len(binary)!=libs[0].file_size:raise ValueError("input binary truncated")
    report=inspect(binary)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print("Checked original JNI callback instruction patterns, no binaries published.")
    print(json.dumps(report,sort_keys=True))


if __name__=="__main__":
    raise SystemExit(main())
