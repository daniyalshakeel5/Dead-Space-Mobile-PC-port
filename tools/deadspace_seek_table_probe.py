#!/usr/bin/env python3
"""Verify actual ARM origin-to-libc-lseek table in the owned Dead Space APK.

This does not emulate ARM or export proprietary executable bytes.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from zipfile import ZipFile
from tools.deadspace_original_code_window import (
    bounded_virtual_read, elf32_executable_sections, elf32_load_ranges,
)

SEEK_START = 0x273034
DISPATCH = 0x2730f0
LITERAL = 0x273190
TABLE_DISPLACEMENT = 0x154

EXPECTED = {
    36: ("sub", "r2, r2, #1"),
    40: ("cmp", "r2, #1"),
    44: ("movhi", "r2, #0"),
    48: ("bls", "#0x2730f0"),
    52: ("bl", "#0x35350"),
    188: ("ldr", "r3, [pc, #0x98]"),
    192: ("add", "r3, pc, r3"),
    196: ("add", "r2, r3, r2, lsl #2"),
    200: ("ldr", "r2, [r2, #0x154]"),
    204: ("bl", "#0x35350"),
}


def decode_origin_table(raw: bytes, code_spans, data_spans) -> tuple[int, int, int]:
    """Resolve original PC-relative displacement and two read-only table words."""
    disp, = struct.unpack("<i", bounded_virtual_read(raw, code_spans, LITERAL, 4))
    table = (DISPATCH + 4 + 8 + disp + TABLE_DISPLACEMENT) & 0xffffffff
    words = bounded_virtual_read(raw, data_spans, table, 8)
    first, second = struct.unpack("<II", words)
    return table, first, second


def validate_instructions(raw: bytes, code_spans) -> None:
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
    arm = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    body = bounded_virtual_read(raw, code_spans, SEEK_START, 208)
    got = {ins.address - SEEK_START:
           (ins.mnemonic.lower(), ins.op_str.lower())
           for ins in arm.disasm(body, SEEK_START)}
    for off, expected in EXPECTED.items():
        if got.get(off) != expected:
            raise ValueError(f"Original seek instruction mismatch at +0x{off:x}: {got.get(off)}")


def inspect_original_apk(apk: Path) -> dict:
    with ZipFile(apk) as z:
        libs = [i for i in z.infolist() if i.filename.startswith("lib/")
                and i.filename.endswith(".so") and not i.is_dir()
                and 1024 < i.file_size <= 64 * 1024 * 1024]
        if len(libs) != 1:
            raise ValueError("Expected exactly one bounded original ARM ELF")
        raw = z.read(libs[0])
        if len(raw) != libs[0].file_size:
            raise ValueError("Truncated native library")
    code = elf32_executable_sections(raw)
    data = elf32_load_ranges(raw)
    validate_instructions(raw, code)
    table, origin1, origin2 = decode_origin_table(raw, code, data)
    if (origin1, origin2) != (1, 2):
        raise ValueError(f"Unrecognized original seek modes: {(origin1, origin2)}")
    return {"schema": 1, "native_elf_verified": True,
            "original_implementation": "0x00273034",
            "original_dispatch_table": f"0x{table:08x}",
            "origin_zero_libc_whence": 0,
            "origin_one_libc_whence": origin1,
            "origin_two_libc_whence": origin2,
            "origin_other_libc_whence": 0,
            "original_libc_plt": "0x00035350",
            "recovered_behavior": "lseek with SEEK_SET, SEEK_CUR, SEEK_END; unknown origins map to SEEK_SET"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apk", type=Path, required=True)
    args = parser.parse_args()
    print("ORIGINAL_SEEK_ORIGIN_EVIDENCE", json.dumps(
        inspect_original_apk(args.apk), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
