"""Synthetic fixture regression tests for Object3D crash investigation."""
import importlib.util
from pathlib import Path
import tempfile
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
        self.assertIn("FATAL:OBJECT3D_DUPLICATE_NULL",patched)
        self.assertIn('trap_unsupported(0x00369060u',patched)
        self.assertIn("call_ind(LD32(r3 + 24u)",patched)
        self.assertNotIn("goto L_36906c;",patched)
        self.assertEqual(patched.count(mod.MARKER),1)
        self.assertEqual(patched,mod.patch(patched))

    def test_fail_closed_for_changed_native_site(self):
        with self.assertRaisesRegex(ValueError,"dereference differs"):
            mod.patch(SOURCE.replace("r3 = LD32(ea);","r7 = LD32(ea);"))

    def test_fail_closed_for_missing_virtual_method(self):
        with self.assertRaisesRegex(ValueError,"virtual duplicate"):
            mod.patch(SOURCE.replace("LD32(r3 + 24u)","LD32(r3 + 20u)"))

    def test_local_report_contains_function_and_direct_callers(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td)
            (d/"fn_060.c").write_text(SOURCE,encoding="utf8")
            (d/"fn_059.c").write_text(CALLER,encoding="utf8")
            report=mod.context_report(SOURCE,d/"fn_060.c",d)
            self.assertIn("L_369060",report)
            self.assertIn("CALLER fn_059.c",report)
            self.assertIn("f_00362000",report)
            self.assertNotIn("CALLER fn_060.c",report)

if __name__=="__main__": unittest.main()
