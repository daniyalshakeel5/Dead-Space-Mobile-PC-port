#!/usr/bin/env python3
"""Bounded original ARM instruction evidence for native-code reconstruction.

Reads the user's confirmed APK only on ephemeral GitHub runners. Reports short
instruction windows at verified native JNI/engine control-flow addresses.
Never publishes ELF bytes, strings, decompiled C or a complete disassembly.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from zipfile import ZipFile

# Actual addresses independently grounded by previous native Ghidra/ELF runs.
# Original *ELF virtual addresses*. Do not pass Ghidra's re-based values
# (+0x10000 for this import) to an ELF section/segment mapper.
WINDOWS = {
    "jni_draw_frame_branch": (0x0027f008, 4),
    "native_frame_state_dispatch": (0x0027ef68, 144),
    "original_game_entry_jni": (0x00035de8, 48),
    "game_entry_direct_callee": (0x0027e974, 12),
    "activity_native_create": (0x0027ee54, 64),
    "renderer_native_surface_changed": (0x0027ecf8, 64),
    "renderer_native_surface_created": (0x0027ec24, 84),
}


def elf32_load_ranges(raw: bytes) -> list[tuple[int,int,int]]:
    if len(raw)<52 or raw[:6]!=b"\x7fELF\x01\x01":
        raise ValueError("Original native binary is not little-endian ELF32")
    phoff=struct.unpack_from("<I",raw,28)[0]
    phsize,phcount=struct.unpack_from("<HH",raw,42)
    if phsize<32 or phcount>1024 or phoff>len(raw) or phsize*phcount>len(raw)-phoff:
        raise ValueError("Unsafe ELF program header table")
    segments=[]
    for i in range(phcount):
        typ,offset,va,_,filesz,memsz,flags,_=struct.unpack_from(
            "<IIIIIIII",raw,phoff+i*phsize)
        if typ!=1: continue
        if filesz>memsz or offset>len(raw) or filesz>len(raw)-offset:
            raise ValueError("Unsafe PT_LOAD segment")
        if filesz:segments.append((va,va+filesz,offset))
    if not segments:raise ValueError("No ELF PT_LOAD segments")
    return segments


def bounded_virtual_read(raw: bytes, segs: list[tuple[int,int,int]],
                         va: int, count: int) -> bytes:
    if count<1 or count>256: raise ValueError("Unsafe instruction window size")
    for start,end,offset in segs:
        if start<=va and va+count<=end:
            p=offset+(va-start)
            return raw[p:p+count]
    raise ValueError("Instruction address not covered by file-backed PT_LOAD")


def elf32_executable_sections(raw: bytes) -> list[tuple[int,int,int]]:
    """Map SHF_EXECINSTR SHT_PROGBITS sections independently of PT_LOAD."""
    if len(raw)<52:raise ValueError("ELF too short")
    shoff=struct.unpack_from("<I",raw,32)[0]
    shsize,shcount=struct.unpack_from("<HH",raw,46)
    if shsize<40 or shcount>4096 or shoff>len(raw) or shsize*shcount>len(raw)-shoff:
        raise ValueError("Unsafe section header table")
    sections=[]
    for i in range(shcount):
        _,typ,flags,addr,off,size,_,_,_,_=struct.unpack_from(
            "<IIIIIIIIII",raw,shoff+i*shsize)
        if typ!=1 or not(flags & 4):continue
        if off>len(raw) or size>len(raw)-off:
            raise ValueError("Executable ELF section outside file")
        if size:sections.append((addr,addr+size,off))
    if not sections:raise ValueError("No executable sections in ARM ELF")
    return sections


def mapping_comparison(raw: bytes, sections: list[tuple[int,int,int]],
                       segments: list[tuple[int,int,int]],
                       va: int, nbytes: int) -> dict:
    section=bounded_virtual_read(raw,sections,va,nbytes)
    segment_candidates=[]
    for span in segments:
        if span[0]<=va and va+nbytes<=span[1]:
            data=bounded_virtual_read(raw,[span],va,nbytes)
            segment_candidates.append(data==section)
    return {
        "executable_section_present": True,
        "segment_candidate_count": len(segment_candidates),
        "any_segment_matches_section": any(segment_candidates),
        "all_segments_match_section": all(segment_candidates)
    }


def elf32_jni_symbols(raw: bytes, names: set[str]) -> dict[str, tuple[int, int, bool]]:
    """Use original ELF dynamic symbols, not Ghidra's inferred function starts."""
    shoff=struct.unpack_from("<I",raw,32)[0]
    shsize,count=struct.unpack_from("<HH",raw,46)
    if shsize<40 or count>4096 or shoff>len(raw) or shsize*count>len(raw)-shoff:
        raise ValueError("Invalid original section table")
    sections=[]
    for i in range(count):
        fields=struct.unpack_from("<IIIIIIIIII",raw,shoff+i*shsize)
        _,typ,_,addr,offset,length,link,_,_,step=fields
        if typ!=8 and (offset>len(raw) or length>len(raw)-offset):
            raise ValueError("Corrupt section bounds")
        sections.append((typ,addr,offset,length,link,step))
    found={}
    for typ,_,offset,length,link,step in sections:
        if typ!=11 or link>=len(sections) or step<16 or step>128:
            continue
        _,_,name_offset,name_size,_,_=sections[link]
        for at in range(offset,offset+length,step):
            if at+16>offset+length:break
            name_off,value,size,attr,_,sect=struct.unpack_from("<IIIBBH",raw,at)
            if name_off>=name_size or (attr&15)!=2 or not size or sect>=len(sections):
                continue
            begin=name_offset+name_off
            end=raw.find(b"\x00",begin,name_offset+name_size)
            if end<0 or end-begin>240:continue
            name=raw[begin:end].decode("ascii","ignore")
            if name not in names:continue
            sym=(value&~1,size,bool(value&1))
            if name in found and found[name]!=sym:
                raise ValueError("Conflicting ELF symbol values")
            found[name]=sym
    return found


def observe(apk: Path) -> dict:
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
    except ImportError as exc:
        raise RuntimeError("Requires pinned Capstone on GitHub runner") from exc
    with ZipFile(apk) as archive:
        libs=[i for i in archive.infolist()
              if i.filename.startswith("lib/") and i.filename.endswith(".so")
              and 1024<i.file_size<=64*1024*1024 and not i.is_dir()]
        if len(libs)!=1:raise ValueError("Expected exactly one original ARM ELF")
        data=archive.read(libs[0])
        if len(data)!=libs[0].file_size:raise ValueError("Truncated ARM library")
    segs=elf32_load_ranges(data)
    sections=elf32_executable_sections(data)
    arm=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    thumb=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
    out={}
    symbols={
        "on_draw_frame": "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
        "on_surface_changed": "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged",
        "on_surface_created": "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
        "game_entry": "Java_com_ea_DeadSpace_DeadSpace_runEntryPoint",
        "on_create": "Java_com_ea_blast_MainActivity_NativeOnCreate",
    }
    located=elf32_jni_symbols(data,set(symbols.values()))
    symbol_windows={}
    for short,name in symbols.items():
        sym=located.get(name)
        if sym is None:
            symbol_windows[short]={"present":False}
            continue
        addr,size,is_thumb=sym
        count=min(size,64)
        block=bounded_virtual_read(data,sections,addr,count)
        decoder=thumb if is_thumb else arm
        ins=list(decoder.disasm(block,addr))
        symbol_windows[short]={
            "present":True,
            "address":f"0x{addr:08x}",
            "symbol_size":size,
            "thumb":is_thumb,
            "instructions":[{"offset":i.address-addr,"op":i.mnemonic.lower(),
                             "args":i.op_str.lower()} for i in ins[:14]],
        }
    for key,(address,nbytes) in WINDOWS.items():
        evidence=mapping_comparison(data,sections,segs,address,nbytes)
        # The executable section table is the mapping used by the existing
        # symbol-level JNI verifier and independently by Ghidra. A PT_LOAD
        # mismatch must be surfaced, never silently used for translation.
        block=bounded_virtual_read(data,sections,address,nbytes)
        # The selected original library code paths are ARM instructions;
        # if a function uses Thumb, the address must come from an
        # independently verified Thumb symbol. Never infer instruction mode.
        decoded=list(arm.disasm(block,address))
        is_contiguous=(sum(i.size for i in decoded)==nbytes)
        out[key]={
            "address": f"0x{address:08x}",
            "file_backed": True,
            "mapping_evidence": evidence,
            "complete_arm_decode": is_contiguous,
            "instructions":[
                {"offset": i.address-address,
                 "op": i.mnemonic.lower(),
                 "args": i.op_str.lower()}
                for i in decoded[:40]],
        }
    return {
        "schema": 1,
        "kind": "original-ARM-targeted-instruction-evidence",
        "windows": out,
        "original_elf_jni_symbols":symbol_windows,
        "limitations": (
          "Short original instruction windows only, not decompiled code. "
          "Direct JNI -> frame target may land inside a shared runtime routine. "
          "Do not call it a game update loop until side effects and state are "
          "proven. No original binary or gameplay assets published."
        ),
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    report=observe(args.apk)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print("Instruction windows recovered from real ARM library; no original bytes written.")
    print(json.dumps(report,sort_keys=True))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
