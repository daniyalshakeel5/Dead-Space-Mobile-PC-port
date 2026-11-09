"""Synthetic fixture regression tests for Object3D crash investigation."""
import importlib.util
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

path = Path(__file__).resolve().parents[1] / "experimental/lifted_milestone6/tools/object3d_duplicate_probe.py"
spec = importlib.util.spec_from_file_location("object3d_duplicate_probe", path)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

SOURCE = '''#include "protos.h"
u64 f_00368efc(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { /* _ZN3m3g8Object3D9duplicateEPNS0_16DuplicateContextE */
  u32 r4=0,r5=0,r6=0,r7=0,r8=0,r9=0,r10=0,r11=0,r12=0;
  if (r0) goto L_369060;
  r0 = r3;
L_369060:;
  {
    u32 ea = r0 + 0u;
    r3 = LD32(ea);
  }
  { u64 rv = call_ind(LD32(r3 + 24u), r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  return PACK(r0,r1);
}
u64 f_00369000(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) { return 0; }
'''
CALLER = '''u64 f_00362000(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) {
  { u64 rv = f_00368efc(r0,r1,r2,r3,sp); r0=(u32)rv; }
  return r0;
}
'''

class Object3DProbeTests(unittest.TestCase):
    def test_exact_patch_preserves_normal_virtual_call(self):
        patched=mod.patch(SOURCE)
        self.assertIn("if (r0 == 0u)",patched)
        self.assertIn("REAL_OBJECT3D_NULL_DUPLICATE",patched)
        self.assertIn("__builtin_return_address(0)",patched)
        self.assertNotIn("trap_unsupported",patched)
        self.assertIn("call_ind(LD32(r3 + 24u)",patched)
        self.assertNotIn("goto L_36906c;",patched)
        self.assertEqual(patched.count(mod.MARK),1)
        self.assertEqual(patched,mod.patch(patched))

    def test_fail_closed_for_changed_native_site(self):
        with self.assertRaisesRegex(ValueError,"crash site differs"):
            mod.patch(SOURCE.replace("r3 = LD32(ea);","r7 = LD32(ea);"))

    def test_fail_closed_for_missing_or_duplicate_function(self):
        with self.assertRaisesRegex(ValueError,"single real duplicate"):
            mod.patch(SOURCE.replace("f_00368efc", "f_00368ef0"))
        with self.assertRaisesRegex(ValueError,"single real duplicate"):
            mod.patch(SOURCE + SOURCE)

    def test_cli_read_only_and_instrument_backup(self):
        with tempfile.TemporaryDirectory() as td:
            target=Path(td)/"fn_060.c"
            target.write_text(SOURCE)
            subprocess.run([sys.executable,str(path),"--source",str(target)],check=True,capture_output=True)
            self.assertEqual(target.read_text(),SOURCE)
            subprocess.run([sys.executable,str(path),"--source",str(target),"--instrument"],check=True,capture_output=True)
            self.assertEqual(target.with_suffix(".c.pre-dup-source.bak").read_text(),SOURCE)
            self.assertEqual(target.read_text(),mod.patch(SOURCE))

if __name__=="__main__": unittest.main()
