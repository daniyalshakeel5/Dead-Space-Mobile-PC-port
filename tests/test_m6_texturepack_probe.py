#!/usr/bin/env python3
"""Pure-synthetic fault guard tests: no game data, generated code, or APK."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

SOURCE=Path(__file__).resolve().parents[1]/"experimental"/"lifted_milestone6"/"tools"/"texturepack_fault_guard.py"
spec=importlib.util.spec_from_file_location("textureprobe",SOURCE)
probe=importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

GENERATED=r'''#include "protos.h"
u64 f_0035b1d4(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
  return 0;
}
u64 f_0035b2e0(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
  u32 r4=0,r5=0,r6=0,r7=0,r8=0,r9=0,r10=0,r11=0,r12=0,lr=0;
  {
    u32 ea = r0;
    r1 = LD32(ea);
  }
  L_35b564:;
  {
    u32 ea = r10 + 0u;
    r3 = LD32(ea);
  }
  {
    u32 xx = sp, yy = 1004u;
    u64 s = (u64)xx + yy + 0; u32 t = (u32)s;
    r2 = t;
  }
  {
    u32 ea = sp + 88u;
    ST32(ea, r2);
  }
  {
    u32 ea = r3 + 0u;
    r1 = LD32(ea);
  }
  { u64 rv = f_00368338(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  return 0;
}
u64 f_0035c844(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
  { u64 rv = f_00365a2c(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  return f_0035b2e0(r0,r1,r2,r3,sp);
}
'''
class GuardTests(unittest.TestCase):
    def test_injects_only_verified_function_and_is_idempotent(self):
        s=probe.patch(GENERATED)
        self.assertIn(probe.MARKER,s)
        self.assertIn(probe.CALLER_MARKER,s)
        self.assertLess(s.index(probe.CALLER_MARKER),
                        s.rindex("return f_0035b2e0"))
        self.assertEqual(s.count(probe.MARKER),1)
        self.assertIn("#include <stdio.h>",s)
        self.assertLess(s.index("if (r3 == 0u)"),s.rindex("r1 = LD32(ea);"))
        self.assertEqual(probe.patch(s),s)
        self.assertIn("return f_0035b2e0",s)

    def test_rejects_unknown_lifted_source(self):
        bad=GENERATED.replace("L_35b564:;","L_35b999:;")
        with self.assertRaisesRegex(ValueError,"differs"):
            probe.patch(bad)
        bad2=GENERATED.replace("r1 = LD32(ea);", "r1 = LD16(ea);")
        with self.assertRaisesRegex(ValueError,"differs"):
            probe.patch(bad2)

    def test_preserves_data_in_read_only_mode(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"fn_059.c"
            p.write_text(GENERATED)
            self.assertNotEqual(probe.patch(p.read_text()),p.read_text())
            self.assertEqual(p.read_text(),GENERATED)
            self.assertFalse(list(Path(d).glob("*.bak")))
if __name__=="__main__":
    unittest.main()
