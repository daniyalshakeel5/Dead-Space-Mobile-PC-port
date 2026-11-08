#!/usr/bin/env python3
"""Differential test: lifted C (32-bit x86) vs the Unicorn emulator, same inputs, compare r0/r1/scratch."""
import sys, os, random, zlib, struct, subprocess, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
SO = "../apk/lib/armeabi/libDeadSpace.so"
sys.argv = [sys.argv[0], SO] + sys.argv[1:]
import loader
from unicorn import *
from unicorn.arm_const import *
from lift import *
from gen import generate

LIB_BASE, SCRATCH, SCR_SZ, STACK_TOP = 0x10000000, 0x60000000, 0x10000, 0x7F000000

def lcg_fill(seed, nbytes):
    out = bytearray()
    for _ in range(nbytes // 4):
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        out += struct.pack("<I", seed)
    return bytes(out)

def lcg_fill_f(seed, nbytes):
    out = bytearray()
    for _ in range(nbytes // 4):
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        out += struct.pack("<f", (((seed >> 8) & 0xFFFFFF) - 0x800000) / 1024.0)
    return bytes(out)

def main(limit=None, nvec=6):
    loader.Emu._unmapped = lambda self, *a: False          # quiet
    emu = loader.Emu(SO); loader.H(emu)
    uc = emu.uc
    # the C harness only maps image/scratch/stack, so make the reference see the same address space
    for base, size in [(0x20000000, 0x10000000), (0x30000000, 0x10000000), (0x40000000, 0x10000000),
                       (0x50000000, 0x100000), (0x58000000, 0x10000)]:
        uc.mem_unmap(base, size)
    uc.mem_unmap(0x70001000, 0x100000 - 0x1000)      # keep only the exit-stub page
    uc.mem_map(SCRATCH, SCR_SZ)
    image = bytes(uc.mem_read(LIB_BASE, emu.lib_end - LIB_BASE))
    open("image.bin", "wb").write(image)
    # writable segments are restored between tests
    wr_ranges = [(s["p_vaddr"], s["p_filesz"]) for s in emu.elf.iter_segments() if s["p_type"] == "PT_LOAD" and s["p_flags"] & 2]
    wr_ranges = [(v, v + max(sz, 0)) for v, sz in [(s["p_vaddr"], s["p_memsz"]) for s in emu.elf.iter_segments() if s["p_type"] == "PT_LOAD" and s["p_flags"] & 2]]

    img = Image(SO)
    ok, bad = lift_all(img)
    leaves = sorted(a for a, (c, f) in ok.items() if not f.calls and not f.indirect)
    if os.environ.get('ONLY_VFP'): leaves = [a for a in leaves if ok[a][1].uses_vfp]
    leaves = [a for a in leaves]
    rng = random.Random(1234)
    if limit: leaves = sorted(rng.sample(leaves, min(limit, len(leaves))))
    from overrides import OVERRIDES, INT_ONLY
    extra = {a: f"u64 f_{a:08x}(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){{ {OVERRIDES[n]} }}" for a, n in img.override.items()}
    if not os.environ.get("NO_OVERRIDES"): leaves = sorted(set(leaves) | set(extra))
    fmode = {a: (n not in INT_ONLY) for a, n in img.override.items()}
    generate(img, ok, leaves, "lifted.c", extra)
    print(f"{len(leaves)} leaf functions selected; compiling 32-bit harness ...", flush=True)
    r = subprocess.run(["gcc", "-m32", "-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-DARM_NAN_FIXUP", "-w", "-o", "harness", "harness.c"], capture_output=True, text=True)
    if r.returncode: print(r.stderr[:3000]); sys.exit(1)

    def arg(fm=False):
        k = rng.random()
        if fm and k >= 0.70:
            return struct.unpack('<I', struct.pack('<f', rng.randrange(-8000000, 8000000) / 1024.0))[0]
        if k < 0.35: return rng.randrange(0, 17)
        if k < 0.70: return SCRATCH + 0x1000 + (rng.randrange(0, 0x7000) & ~3)
        if k < 0.90: return rng.getrandbits(32)
        return (-rng.randrange(1, 17)) & 0xFFFFFFFF

    vectors, expect = [], {}
    n_invalid = 0
    stack_sp = STACK_TOP - 0x100
    for a in leaves:
        for i in range(nvec):
            fm = (1 if fmode[a] else 0) if a in extra else (1 if (ok[a][1].uses_vfp and not os.environ.get('VFP_BITS')) else 0)
            args = [arg(fm) for _ in range(4)]
            seed = rng.randrange(1, 1 << 30)
            # --- reference run
            for (lo, hi) in wr_ranges: uc.mem_write(LIB_BASE + lo, image[lo:hi])
            uc.mem_write(stack_sp - 0x2000, bytes(0x2000))          # fresh (zero) stack below sp, like the forked C side
            filler = lcg_fill_f if fm else lcg_fill
            uc.mem_write(SCRATCH, filler(seed, SCR_SZ))
            uc.mem_write(stack_sp, filler(seed ^ 0x5A5A5A5A, 0x100))
            for i in range(16): uc.reg_write(UC_ARM_REG_D0 + i, 0)
            for rr in range(UC_ARM_REG_R4, UC_ARM_REG_R4 + 9): uc.reg_write(rr, 0)       # r4-r12 start at 0, like the C locals
            try: uc.reg_write(UC_ARM_REG_FPSCR, 0)
            except Exception as ex:
                if not globals().get('_warned'): print('note: cannot reset FPSCR:', ex); globals()['_warned'] = True
            for reg, v in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args): uc.reg_write(reg, v)
            uc.reg_write(UC_ARM_REG_SP, stack_sp); uc.reg_write(UC_ARM_REG_LR, loader.EXIT_STUB)
            try:
                uc.emu_start(LIB_BASE + a, loader.EXIT_STUB, count=300000)
            except UcError:
                n_invalid += 1; continue
            if emu.reg(UC_ARM_REG_PC) != loader.EXIT_STUB: n_invalid += 1; continue    # hit the instruction budget
            res = (emu.reg(UC_ARM_REG_R0), emu.reg(UC_ARM_REG_R1), zlib.crc32(bytes(uc.mem_read(SCRATCH, SCR_SZ))))
            vectors.append((a, args, seed, fm)); expect[(a, tuple(args), seed)] = res
    print(f"{len(vectors)} valid vectors ({n_invalid} discarded: emulator fault/timeout)", flush=True)
    with open("vectors.txt", "w") as f:
        for a, args, seed, fm in vectors:
            f.write(f"{a:x} {args[0]:x} {args[1]:x} {args[2]:x} {args[3]:x} {seed} {fm}\n")
    out = subprocess.run(["./harness", "image.bin", "vectors.txt"], capture_output=True, text=True).stdout.splitlines()

    RET64 = {"__aeabi_ui2d", "__floatsidf", "__extendsfdf2", "__aeabi_ul2d", "__floatdidf", "__aeabi_ddiv", "__muldf3",
             "__aeabi_ldivmod", "__aeabi_uldivmod"}
    r1_ignore = {a for a, n in img.override.items() if n not in RET64}      # 32-bit-returning helpers leave r1 undefined
    mism = collections.defaultdict(list); crashes = collections.Counter(); matched = 0
    # outputs are in vector order, one line per vector
    vi = 0
    for line in out:
        p = line.split()
        a, args, seed, _fm = vectors[vi]; vi += 1
        if p[0] == "R":
            got = (int(p[2], 16), int(p[3], 16), int(p[4], 16))
            exp = expect[(a, tuple(args), seed)]
            if a in r1_ignore: got, exp = (got[0], 0, got[2]), (exp[0], 0, exp[2])
            if got == exp: matched += 1
            else: mism[a].append((args, expect[(a, tuple(args), seed)], got))
        else:
            crashes[a] += 1
    print(f"matched {matched}/{len(vectors)} vectors; functions with mismatches: {len(mism)}; functions with C crashes: {len(crashes)}")
    for a in list(mism)[:30]:
        args, e, g = mism[a][0]
        print(f"  MISMATCH f_{a:08x} {img.names_at.get(a, "?")[:50]} args={[hex(x) for x in args]} emu(r0,r1,crc)={tuple(hex(x) for x in e)} c={tuple(hex(x) for x in g)}")
    for a in list(crashes)[:8]:
        print(f"  CRASH    f_{a:08x} {img.names_at.get(a, "?")[:50]} ({crashes[a]} vectors)")

if __name__ == "__main__":
    lim = int(os.environ.get("LIMIT", "0")) or None
    main(limit=lim)
