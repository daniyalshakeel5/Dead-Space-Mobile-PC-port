#!/usr/bin/env python3
"""Recover only instruction-PROVEN ARM/Thumb constant JNI enum returns.

These are tiny game runtime ABI functions, not assumptions from naming.
A return value is accepted only if the entire recognized function consists
of a constant load into r0 and an unconditional return. Any other instruction
is REJECTED, not guessed. No game bytes/disassembly are exported.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
from zipfile import ZipFile


def rotate_right(value: int, amount: int) -> int:
    amount &= 31
    return ((value >> amount) | ((value << (32-amount)) & 0xffffffff)) & 0xffffffff if amount else value


def constant_return(code: bytes, thumb: bool) -> int | None:
    if thumb:
        if len(code) < 4:
            return None
        pos = 0
        while pos + 2 <= len(code) and code[pos:pos+2] in (b"\x00\xbf", b"\xc0\x46"):
            pos += 2  # known unconditional Thumb NOPs only
        if len(code) < pos + 4:
            return None
        first, last = struct.unpack_from("<HH", code, pos)
        if first & 0xff00 != 0x2000 or last != 0x4770:
            return None
        # 16-bit Thumb MOVS r0, #imm8; BX LR.
        if any(b not in (0x00,0xbf,0xc0,0x46) for b in code[pos+4:]):
            # Symbol body can contain alignment NOPs; never accept other ops.
            if code[pos+4:].strip(b"\x00"):
                return None
        return first & 0xff
    if len(code) < 8:
        return None
    words = list(struct.unpack_from("<II", code, 0))
    first, second = words
    if first >> 28 != 0xe or second >> 28 != 0xe:
        return None
    if (first & 0x0FE0F000) != 0x03A00000:
        return None
    if (second & 0x0fffffff) not in (0x012fff1e, 0x01a0f00e):
        return None
    if len(code) > 8 and code[8:].strip(b"\x00"):
        return None
    imm = first & 0xff
    rotation = ((first >> 8) & 0xf) * 2
    return rotate_right(imm, rotation)


def native_enum_constants(binary: bytes) -> dict:
    if len(binary) < 52 or binary[:4] != b"\x7fELF" or binary[4:6] != b"\x01\x01":
        raise ValueError("Expected 32-bit little-endian ELF")
    shoff = struct.unpack_from("<I", binary, 32)[0]
    shentsize, shnum = struct.unpack_from("<HH", binary, 46)
    if shentsize < 40 or shnum > 4096 or shoff > len(binary) or shnum*shentsize > len(binary)-shoff:
        raise ValueError("Unsafe ELF section table")
    sections=[]
    for i in range(shnum):
        off=shoff + i*shentsize
        name, typ, flags, addr, file_off, size, link, info, align, entsize = struct.unpack_from(
            "<IIIIIIIIII", binary, off)
        if file_off > len(binary) or size > len(binary)-file_off:
            raise ValueError("ELF section outside file")
        sections.append({"type":typ, "addr":addr, "offset":file_off,
                         "size":size,"link":link,"entsize":entsize})
    exports=[]
    for section in sections:
        if section["type"] != 11 or section["link"] >= len(sections):
            continue
        strings=sections[section["link"]]
        sym_size=section["entsize"]
        if sym_size < 16 or sym_size > 256:
            continue
        for off in range(section["offset"], section["offset"]+section["size"], sym_size):
            if off+16>len(binary):break
            name_off,value,size,attr,other,shndx = struct.unpack_from("<IIIBBH",binary,off)
            if shndx == 0 or (attr&0xf)!=2 or not size or shndx>=len(sections):
                continue
            if name_off>=strings["size"]:continue
            start=strings["offset"]+name_off
            end=binary.find(b"\x00",start,strings["offset"]+strings["size"])
            if end<0 or end-start>240:continue
            name=binary[start:end].decode("ascii",errors="ignore")
            if not (name.startswith("Java_com_ea_blast_") and "_NativeGet" in name):
                continue
            dest=sections[shndx]
            actual_addr=value&~1
            if not dest["addr"]<=actual_addr<dest["addr"]+dest["size"]:
                exports.append((name, None))
                continue
            rel=actual_addr-dest["addr"]
            if size>128 or rel+size>dest["size"]:
                exports.append((name, None))
                continue
            code=binary[dest["offset"]+rel:dest["offset"]+rel+size]
            exports.append((name,constant_return(code,bool(value&1))))
    # Do not silently overwrite if symbol duplicates disagree.
    results={}
    for name,value in exports:
        if name in results and results[name]!=value:
            results[name]=None
        else:
            results[name]=value
    proven={key:value for key,value in results.items() if value is not None}
    return {
        "schema":1,
        "kind":"recovered-original-arm-jni-enum-constants",
        "examined_export_functions":len(results),
        "proven_constant_functions":len(proven),
        "unverified_functions":len(results)-len(proven),
        "constants":dict(sorted(proven.items())),
        "limitations":"Only unconditional two-instruction ARM/Thumb integer returns proven. No original game logic or ABI beyond these tiny functions assumed."
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    args=p.parse_args()
    with ZipFile(args.apk) as archive:
        libs=[i for i in archive.infolist()
              if i.filename.startswith("lib/") and i.filename.endswith(".so")
              and not i.is_dir() and 0 < i.file_size <= 64*1024*1024]
        if len(libs)!=1:
            raise ValueError("Expected one bounded ARM shared library")
        with archive.open(libs[0]) as stream:
            binary=stream.read(64*1024*1024+1)
        if len(binary)!=libs[0].file_size:
            raise ValueError("Truncated native library")
    report=native_enum_constants(binary)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("Original JNI constant functions inspected; all ambiguous code rejected.")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
