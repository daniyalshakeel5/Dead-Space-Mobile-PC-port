# generate lifted.c for a chosen set of functions (plus the FN table and prototypes)
import sys
from lift import *

def generate(img, ok, selected, path, extra=None):
    extra = extra or {}
    out = ['#include "rt.h"', "typedef u64 (*fn_t)(u32,u32,u32,u32,u32);"]
    for a in selected: out.append(f"u64 f_{a:08x}(u32,u32,u32,u32,u32);")
    for a in selected: out.append(extra[a] if a in extra else ok[a][0])
    out.append("static const struct { u32 addr; fn_t fn; } FN[] = {")
    out += [f"  {{ 0x{a:x}u, f_{a:08x} }}," for a in selected]
    out.append("};")
    open(path, "w").write("\n".join(out) + "\n")
