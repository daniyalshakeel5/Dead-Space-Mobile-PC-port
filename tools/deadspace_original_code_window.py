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
WINDOWS = {
    "jni_draw_frame_tail": (0x0028f008, 4),
    "jni_draw_frame_target": (0x0027ef68, 52),
    "native_function_start_containing_frame_target": (0x0027ead0, 52),
    "original_game_entry_jni": (0x00045de8, 48),
    "game_entry_direct_callee": (0x0028e974, 8),
    "activity_native_create": (0x0028ee54, 60),
    "renderer_native_surface_changed": (0x0028ecf8, 60),
    "frame_direct_helper_a": (0x000450bc, 12),
    "frame_direct_helper_b": (0x000458b4, 12),
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
    arm=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    thumb=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
    out={}
    for key,(address,nbytes) in WINDOWS.items():
        block=bounded_virtual_read(data,segs,address,nbytes)
        # The selected original library code paths are ARM instructions;
        # if a function uses Thumb, the address must come from an
        # independently verified Thumb symbol. Never infer instruction mode.
        decoded=list(arm.disasm(block,address))
        is_contiguous=(sum(i.size for i in decoded)==nbytes)
        out[key]={
            "address": f"0x{address:08x}",
            "file_backed": True,
            "complete_arm_decode": is_contiguous,
            "instructions":[
                {"offset": i.address-address,
                 "op": i.mnemonic.lower(),
                 "args": i.op_str.lower()}
                for i in decoded[:20]],
        }
    return {
        "schema": 1,
        "kind": "original-ARM-targeted-instruction-evidence",
        "windows": out,
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
