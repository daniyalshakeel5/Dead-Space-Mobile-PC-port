#!/usr/bin/env python3
"""Bounded ARM32 ELF control-flow inspection for original Dead Space Mobile.

Run only with a user-owned, extracted libEAMGameDeadSpace.so.
Prints instruction *metadata*, not game code bytes or embedded assets.
This is research, not an ARM runtime or a native engine implementation.
Requires: pip install pyelftools capstone
"""
import argparse
import json
from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM


def inspect(path, va, size, thumb=False):
    if not 0 < size <= 0x10000:
        raise ValueError("size must be 1..65536")
    if va < 0:
        raise ValueError("negative virtual address")
    with Path(path).open("rb") as stream:
        elf = ELFFile(stream)
        if elf.elfclass != 32 or elf["e_machine"] != "EM_ARM":
            raise ValueError("expected original 32-bit ARM ELF")
        segment = next((s for s in elf.iter_segments()
                        if s["p_type"] == "PT_LOAD"
                        and int(s["p_vaddr"]) <= va
                        and va + size <= int(s["p_vaddr"]) + int(s["p_filesz"])), None)
        if segment is None:
            raise ValueError("requested range is not fully file-backed in one PT_LOAD segment")
        offset = int(segment["p_offset"]) + va - int(segment["p_vaddr"])
        stream.seek(offset)
        code = stream.read(size)
        if len(code) != size:
            raise ValueError("truncated ELF range")
    engine = Cs(CS_ARCH_ARM, CS_MODE_THUMB if thumb else CS_MODE_ARM)
    engine.detail = True
    instructions = list(engine.disasm(code, va))
    if not instructions or sum(i.size for i in instructions) != len(code):
        raise ValueError("range contains an undecodable instruction")
    calls, branches, virtual_sites = [], [], []
    for ins in instructions:
        name = ins.mnemonic.lower()
        entry = {"at": f"0x{ins.address:08x}", "instruction": name,
                 "operands": ins.op_str}
        if name.startswith("bl"):
            for operand in ins.operands:
                if operand.type == ARM_OP_IMM:
                    entry["target"] = f"0x{operand.imm & 0xffffffff:08x}"
                    break
            calls.append(entry)
        elif name in {"b", "bx", "beq", "bne", "bhi", "bls", "bge", "blt",
                      "bgt", "ble", "bcc", "bcs", "bpl", "bmi", "cbz", "cbnz"}:
            branches.append(entry)
        if name in {"ldr", "add"} and ("#0x50" in ins.op_str or "#80" in ins.op_str):
            virtual_sites.append(entry)
    return {"start_va": f"0x{va:08x}", "size": size, "thumb": thumb,
            "decoded_count": len(instructions), "calls": calls,
            "branches": branches, "possible_vtable_0x50_sites": virtual_sites,
            "warning": "Call targets and offsets are disassembly hints, not verified C++ behavior."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--va", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--size", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--thumb", action="store_true")
    parser.add_argument("--output", type=Path, help="write new JSON report (never overwrite)")
    args = parser.parse_args()
    result = inspect(args.elf, args.va, args.size, args.thumb)
    report = json.dumps(result, indent=2) + "\n"
    if args.output:
        with args.output.open("x", encoding="utf-8") as out:
            out.write(report)
    else:
        print(report, end="")


if __name__ == "__main__":
    main()
