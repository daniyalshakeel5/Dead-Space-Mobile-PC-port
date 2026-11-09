"""Synthetic regression test for unmodified ARM loader control flow."""
import importlib.util
from pathlib import Path
import unittest

p=Path(__file__).resolve().parents[1]/"experimental/lifted_milestone6/tools/loader_vector_probe.py"
s=importlib.util.spec_from_file_location("loader_vector_probe",p)
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
SRC='''#include "protos.h"
u64 f_00365a2c(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { /* _ZN3m3g6Loader4loadERKN5eastl12basic_stringIwN2im20StringEASTLAllocatorEEE */
  u32 r4=0,r5=0,r6=0,r7=0,r8=0,r9=0,r10=0,r11=0,r12=0,lr=0;
  u32 fN=0,fZ=0,fC=0,fV=0;
  { u32 t = r1; r7 = t; }
  { u32 t = r0; r5 = t; }
  { u64 rv = f_0032b260(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  { u64 rv = f_003659bc(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
L_365a80:;
  { u64 rv = f_0032b260(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
  { u64 rv = f_003659bc(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }
}
u64 f_00365a30(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){return 0;}
'''
class Tests(unittest.TestCase):
    def test_audit_preserves_native_functions_and_idempotency(self):
        out=m.safe_patch(SRC)
        self.assertEqual(out.count(m.MARK),4)
        self.assertEqual(out.count(m.CALL),2)
        self.assertEqual(out.count(m.FINISH),2)
        self.assertEqual(out.count('L_365a80:;'),1)
        self.assertEqual(out,m.safe_patch(out))
        self.assertIn("M3G_LOADER_AFTER_DECODE",out)
        self.assertIn("M3G_LOADER_AFTER_CLEANUP",out)
    def test_fail_closed_if_original_loader_differs(self):
        with self.assertRaises(ValueError):m.safe_patch(SRC.replace("L_365a80:;","L_365a84:;"))
        with self.assertRaises(ValueError):m.safe_patch(SRC.replace(m.CALL,"",1))
if __name__=="__main__":unittest.main()
