#!/usr/bin/env python3
"""Map backtraces printed by dspc_native (built with -no-pie) to game function names. usage: bt.py logfile [binary]"""
import re, subprocess, bisect, sys
sys.path.insert(0, '.')
from lift import Image
txt = open(sys.argv[1]).read()
binary = sys.argv[2] if len(sys.argv) > 2 else 'out/dspc_native'
syms = []
for line in subprocess.run(['nm', '-n', binary], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
keys = [s[0] for s in syms]
img = Image('../lib/armeabi/libDeadSpace.so')
def name(a):
    i = bisect.bisect_right(keys, a) - 1
    n = syms[i][1] if i >= 0 else '?'
    g = img.names_at.get(int(n[2:], 16), '') if n.startswith('f_') else ''
    return f"{a:#x}  {n:<14} {g[:95]}"
blocks = re.findall(r'(THREAD \d+ )?backtrace[^:\n]*:([^\n]*)', txt)
for who, addrs in blocks:
    print("---", who.strip() or "crash thread")
    for a in addrs.split():
        try: v = int(a, 16)
        except ValueError: continue
        s = name(v)
        if 'segv_handler' in s or 'usr2_handler' in s or 'data_start' in s: continue
        print("  ", s)
