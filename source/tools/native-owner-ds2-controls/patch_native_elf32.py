#!/usr/bin/env python3
"""Apply the owner's native controller ELF32 corrections to the EXACT supplied executable.

This script does not generate game code, unpack assets, or execute the game.
The original SHA256 and each instruction sequence must match, otherwise it fails.
"""
import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_ORIGINAL = "fc1257844846043f6df78d01193d30b75a76e6022d025cd90cf17c7f4312bfa9"
EXPECTED_PATCHED = "3e1a8e8088c07cf646587f1b7a34a8ab398aa1fefc748c9c89ff44e291c95e80"
LOAD_BIAS = 0x08048000

def patch(original, destination):
    src = Path(original)
    dst = Path(destination)
    original_bytes = src.read_bytes()
    actual = hashlib.sha256(original_bytes).hexdigest()
    if actual != EXPECTED_ORIGINAL:
        raise ValueError("Refusing to patch unrecognized ELF SHA256 " + actual)
    if original_bytes[:5] != b"\\x7fELF\\x01" or original_bytes[18:20] != b"\\x03\\x00":
        raise ValueError("Expected 32-bit little-endian x86 ELF")
    data = bytearray(original_bytes)

    def replace(addr, old, new, description):
        offset = addr - LOAD_BIAS
        if len(old) != len(new) or data[offset:offset+len(old)] != old:
            raise ValueError(f"{description}: instructions differ at 0x{addr:08x}")
        data[offset:offset+len(old)] = new
        print(f"0x{addr:08x}: {description}: {old.hex()} -> {new.hex()}")

    def near_jcc(addr, cc, target):
        return b"\\x0f" + bytes([cc]) + struct.pack("<i", target - (addr+6))

    def near_jmp_nop(addr, target):
        return b"\\xe9" + struct.pack("<i", target - (addr+5)) + b"\\x90"

    # Preserve existing L2 equipment safety: only unarmed input bypasses
    # unsafe aim/fire setup, but rejoins the original native action path.
    replace(0x0883ce0c, bytes.fromhex("0f84dc000000"),
            near_jcc(0x0883ce0c,0x84,0x0883d3cd), "unarmed action routing")
    # Remove only the unarmed action-ID whitelist.
    replace(0x0883d035, bytes.fromhex("75c2"), bytes.fromhex("ebc2"),
            "allow native actions before Plasma Cutter")
    # Preserve original stomp ActionEvent without requiring weapon equipment.
    replace(0x0883d062, bytes.fromhex("0f85c0010000"),
            near_jmp_nop(0x0883d062,0x0883d228), "unarmed stomp")

    patched = bytes(data)
    digest = hashlib.sha256(patched).hexdigest()
    if digest != EXPECTED_PATCHED:
        raise ValueError("Patched executable failed full-file hash verification: " + digest)
    if dst.resolve() == src.resolve():
        raise ValueError("Choose a different output path to preserve the original executable")
    dst.write_bytes(patched)
    dst.chmod(src.stat().st_mode)
    print("Saved", dst, "SHA256", digest)

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("original_dspc_sdl")
    ap.add_argument("patched_dspc_sdl")
    args = ap.parse_args()
    patch(args.original_dspc_sdl, args.patched_dspc_sdl)
