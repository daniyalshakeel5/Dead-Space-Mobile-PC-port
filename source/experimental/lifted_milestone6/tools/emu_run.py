#!/usr/bin/env python3
"""Reference run: constructors, JNI_OnLoad, then the Java entry points, on the emulator. Prints import trace + crash info."""
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, ".")
SO = "../apk/lib/armeabi/libDeadSpace.so"
sys.argv = [sys.argv[0], SO]
import loader
from unicorn import *; from unicorn.arm_const import *
emu = loader.Emu(SO); loader.H(emu); emu.trace = []
init = emu.elf.get_section_by_name(".init_array")
for i in range(init["sh_size"] // 4):
    fn = emu.u32(loader.LIB_BASE + init["sh_addr"] + 4 * i)
    if fn not in (0, 0xffffffff): emu.call(fn)
emu.call(emu.exports["JNI_OnLoad"], emu.vm, 0)
base = len(emu.trace)
def export(name): return [a for n, a in emu.exports.items() if n == name][0]
act, rend = 0x20000, 0x20008
def go(name, *args):
    try:
        r = emu.call(export(name), emu.env, *args, max_insns=400_000_000)
        print(f">> {name} -> {r:#x}  pc={emu.reg(UC_ARM_REG_PC):#x}", flush=True)
    except UcError as e:
        regs = {f"r{i}": emu.reg(i) for i in range(0, 13)}
        print("   regs:", " ".join(f"{k}={v:#x}" for k, v in regs.items()), f"sp={emu.reg(UC_ARM_REG_SP):#x}")
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
        pc = emu.reg(UC_ARM_REG_PC)
        code = bytes(emu.uc.mem_read(pc - 40, 48))
        for i in Cs(CS_ARCH_ARM, CS_MODE_ARM).disasm(code, pc - 40): print(f"     {i.address - loader.LIB_BASE:#x}: {i.mnemonic} {i.op_str}")
        print(f"!! {name} faulted: {e} pc={emu.reg(UC_ARM_REG_PC):#x} lr={emu.reg(UC_ARM_REG_LR):#x} (guest vaddr {emu.reg(UC_ARM_REG_PC)-loader.LIB_BASE:#x})", flush=True)
        return False
    return True
ok = go("Java_com_ea_blast_MainActivity_NativeOnCreate", act)
ok = ok and go("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated", rend)
ok = ok and go("Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged", rend, 1280, 720)
ok = ok and go("Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame", rend)
open("emu_run_trace.txt", "w").write("\n".join(emu.trace[base:]) + "\n")
print(len(emu.trace) - base, "import calls after OnLoad")
