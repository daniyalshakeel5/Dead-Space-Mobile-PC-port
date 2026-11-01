"""Resolve an ARM32 PLT entry to its ELF dynamic relocation symbol.

The original APK is processed only in GitHub Actions. Export a single imported
symbol name and virtual GOT address, never original ELF bytes or assets.
"""
import argparse
import json
import struct
from pathlib import Path
from zipfile import ZipFile

from tools.deadspace_original_code_window import (
    bounded_virtual_read, elf32_executable_sections, elf32_load_ranges)

def arm_immediate(word: int) -> int:
    imm = word & 255
    rotate = 2 * ((word >> 8) & 15)
    if not rotate:
        return imm
    return ((imm >> rotate) | (imm << (32-rotate))) & 0xffffffff

def sections_elf32(raw: bytes) -> list[dict]:
    if raw[:6] != b"\x7fELF\x01\x01" or len(raw) < 52:
        raise ValueError("Expected little-endian ELF32")
    offset=struct.unpack_from("<I",raw,32)[0]
    size,count=struct.unpack_from("<HH",raw,46)
    if size < 40 or count > 4096 or offset > len(raw) or size*count > len(raw)-offset:
        raise ValueError("Invalid ELF section header bounds")
    result=[]
    for i in range(count):
        name,kind,flags,va,start,length,link,info,align,step=struct.unpack_from(
            "<IIIIIIIIII",raw,offset+i*size)
        if kind != 8 and (start > len(raw) or length > len(raw)-start):
            raise ValueError("Out of bounds original ELF section")
        result.append(dict(type=kind,va=va,start=start,size=length,link=link,step=step))
    return result

def resolve_arm_plt_import(raw: bytes, plt_va: int) -> dict:
    """Require exact ADD ip,pc / ADD ip,ip / LDR pc,[ip,#n]! ARM sequence."""
    code=bounded_virtual_read(raw,elf32_executable_sections(raw),plt_va,12)
    first,second,last=struct.unpack("<III",code)
    for word, rn in ((first,15),(second,12)):
        if ((word>>25)&7)!=1 or ((word>>21)&15)!=4 or (
            (word>>16)&15)!=rn or ((word>>12)&15)!=12:
            raise ValueError("Original ARM PLT add-register proof failed")
    if ((last>>26)&3)!=1 or ((last>>25)&1)!=0 or (
        (last>>20)&1)!=1 or ((last>>16)&15)!=12 or (
        (last>>12)&15)!=15 or ((last>>23)&1)!=1:
        raise ValueError("Original PLT indirect LDR proof failed")
    got=(plt_va+8+arm_immediate(first)+arm_immediate(second)+(last&0xfff))&0xffffffff
    sections=sections_elf32(raw)
    found=[]
    for s in sections:
        if s["type"]!=9 or s["step"]<8 or s["link"]>=len(sections):
            continue
        symtab=sections[s["link"]]
        if symtab["type"] not in (2,11) or symtab["step"]<16 or symtab["link"]>=len(sections):
            continue
        strings=sections[symtab["link"]]
        if strings["type"]!=3:continue
        for at in range(s["start"],s["start"]+s["size"],s["step"]):
            if at+8>s["start"]+s["size"]:break
            target,info=struct.unpack_from("<II",raw,at)
            if target!=got or (info&255)!=22:continue # R_ARM_JUMP_SLOT
            index=info>>8
            symbol_at=symtab["start"]+index*symtab["step"]
            if symbol_at+16>symtab["start"]+symtab["size"]:
                raise ValueError("Invalid original ARM relocation symbol index")
            name_offset=struct.unpack_from("<I",raw,symbol_at)[0]
            if name_offset>=strings["size"]:
                raise ValueError("Invalid original ELF string offset")
            start=strings["start"]+name_offset
            end=raw.find(b"\0",start,strings["start"]+strings["size"])
            if end<0:raise ValueError("Unterminated original ELF import name")
            found.append(raw[start:end].decode("utf-8","replace"))
    if len(found)!=1:
        raise ValueError("Original PLT target has no unique R_ARM_JUMP_SLOT relocation")
    return {
        "plt_va":f"0x{plt_va:08x}",
        "got_va":f"0x{got:08x}",
        "import_name":found[0],
        "relocation":"R_ARM_JUMP_SLOT",
        "verified":True,
    }

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    with ZipFile(args.apk) as archive:
        libs=[n for n in archive.infolist() if n.filename.startswith("lib/")
              and n.filename.endswith(".so") and 1024<n.file_size<=64*1024*1024]
        if len(libs)!=1:raise ValueError("Expected exactly one original ARM game ELF")
        raw=archive.read(libs[0])
    evidence=resolve_arm_plt_import(raw,0x00035194)
    # Additional original 0x27E560 imported helpers. Do not infer from
    # address spacing; validate exact R_ARM_JUMP_SLOT relocation for each.
    singleton_helpers={
        "0x0003580c":resolve_arm_plt_import(raw,0x0003580c),
        "0x00035a28":resolve_arm_plt_import(raw,0x00035a28),
    }
    evidence["startup_singleton_imports"]=singleton_helpers
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(evidence,indent=2,sort_keys=True)+"\\n")
    print("ORIGINAL_ARM_INPUT_CONSTRUCTOR_IMPORT",json.dumps(evidence,sort_keys=True))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
