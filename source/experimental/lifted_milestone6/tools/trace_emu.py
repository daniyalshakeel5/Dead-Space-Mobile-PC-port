#!/usr/bin/env python3
"""Reference trace: names of every import/JNI stub hit while running constructors + JNI_OnLoad on the emulator."""
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, ".")
SO = sys.argv[1] if len(sys.argv) > 1 else "../apk/lib/armeabi/libDeadSpace.so"
sys.argv = [sys.argv[0], SO]
import loader
emu = loader.Emu(SO); loader.H(emu); emu.trace = []
init = emu.elf.get_section_by_name(".init_array")
for i in range(init["sh_size"] // 4):
    fn = emu.u32(loader.LIB_BASE + init["sh_addr"] + 4 * i)
    if fn in (0, 0xffffffff): continue
    emu.call(fn)
v = emu.call(emu.exports["JNI_OnLoad"], emu.vm, 0)
open("emu_trace.txt", "w").write("\n".join(emu.trace) + "\n")
print(f"emulator: {len(emu.trace)} import calls traced, JNI_OnLoad returned {v:#x}")
