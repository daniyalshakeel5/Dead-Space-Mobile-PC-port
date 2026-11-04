"""Recover full 0x27FD7C ordered startup registration call specifications.

Runs only on the user's confirmed APK in ephemeral GitHub CI. Reports
addresses, never original resource strings or proprietary binary data.
"""
import argparse
import json
import re
import struct
from pathlib import Path
from zipfile import ZipFile
from tools.deadspace_original_code_window import (
    bounded_virtual_read, elf32_executable_sections)

ROOT=0x0027FD7C
HELPER=0x00284178
LDR_RE=re.compile(r"^(r[23467]), \[pc, #(?P<off>0x[0-9a-f]+|[0-9]+)\]$")

def derive_calls(instructions, read_u32, *, expected_calls=None):
    reg={}
    base=None
    slot=None
    slot_loaded=False
    saw_context=False
    saw_word=False
    rows=[]
    # PC is instruction address + 8 in original ARM state.
    for ins in instructions:
        address=ROOT+ins["offset"]
        op=ins["op"]
        args=ins["args"]
        m=LDR_RE.fullmatch(args) if op=="ldr" else None
        if m:
            reg[m.group(1)]=read_u32(address+8+int(m.group("off"),0))
        elif op=="add" and args in ("r4, pc, r4","r6, pc, r6",
                                     "r7, pc, r7","r2, pc, r2"):
            dest=args[:2]
            if dest not in reg:
                raise ValueError(f"Missing original PC-relative {dest} at {address:x}")
            reg[dest]=(reg[dest]+address+8)&0xffffffff
            if dest=="r4":base=reg[dest]
        elif op=="ldr" and args=="r3, [r4, r3]":
            if base is None or "r3" not in reg:
                raise ValueError("Unresolved original BSS-relative resource slot")
            slot=(base+reg["r3"])&0xffffffff
            slot_loaded=True
        elif op=="ldr" and args=="r1, [r3]":
            if not slot_loaded:
                raise ValueError("Expected original double-dereference before resource call")
            saw_word=True
        elif op=="add" and args=="r5, r0, #0x18":
            saw_context=True
        elif op=="mov" and args in ("r2, r6","r2, r7"):
            src=args[-2:]
            if src not in reg:
                raise ValueError("Unresolved original descriptor register")
            reg["r2"]=reg[src]
        elif op in ("bl","b") and args==f"#0x{HELPER:x}":
            if not (slot_loaded and saw_word and saw_context and "r2" in reg):
                raise ValueError(f"Incomplete original registration call at 0x{address:08x}")
            rows.append({
                "callsite_va":f"0x{address:08x}",
                "source_global_slot_va":f"0x{slot:08x}",
                "descriptor_va":f"0x{reg['r2']:08x}",
                "tail_call":op=="b",
            })
            slot_loaded=False
            saw_word=False
            slot=None
        elif op=="bl":
            raise ValueError(f"Unexpected branch target in registration function {address:x}")
    if expected_calls is not None and len(rows)!=expected_calls:
        raise ValueError(f"Original registration count is {len(rows)}, expected {expected_calls}")
    if not rows or not rows[-1]["tail_call"] or any(r["tail_call"] for r in rows[:-1]):
        raise ValueError("Original registration tail-branch not found")
    if len({row["source_global_slot_va"] for row in rows})!=len(rows):
        raise ValueError("Original registration input globals unexpectedly alias")
    return rows

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--instruction-report",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    report=json.loads(a.instruction_report.read_text())
    ins=report["windows"]["post_setup_direct_callee_0x27fd7c"]
    if not ins["complete_arm_decode"] or len(ins["instructions"])!=176:
        raise ValueError("Unverified full 0x27FD7C original ARM trace")
    with ZipFile(a.apk) as zipfile:
        items=[e for e in zipfile.infolist() if e.filename.startswith("lib/")
               and e.filename.endswith(".so") and 1024<e.file_size<=64*1024*1024]
        if len(items)!=1:
            raise ValueError("Original APK must contain precisely one bounded ARM ELF")
        data=zipfile.read(items[0])
    sections=elf32_executable_sections(data)
    def read_u32(address):
        return struct.unpack("<I",bounded_virtual_read(data,sections,address,4))[0]
    rows=derive_calls(ins["instructions"],read_u32,expected_calls=27)
    result={
        "function":"0x0027fd7c",
        "helper":"0x00284178",
        "original_context_addend":24,
        "verified":True,
        "calls":rows,
    }
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
    print("ORIGINAL_STARTUP_REGISTRATION_27",json.dumps(result,separators=(",",":")))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
