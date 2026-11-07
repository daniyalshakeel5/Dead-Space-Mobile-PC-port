"""Copyright-free regression tests for the verified ModelCache missing-node recovery."""
import importlib.util
from pathlib import Path
import unittest

path=Path(__file__).resolve().parents[1]/"experimental/lifted_milestone6/tools/modelcache_empty_vector.py"
spec=importlib.util.spec_from_file_location("modelcache_empty_vector",path)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

SOURCE="""#include "protos.h"
u64 f_000629b4(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) {
  {
    u32 ea = sp + 52u;
    ST32(ea, r3);
  }
  f_0032b260(r0,r1,r2,r3,sp);
  f_00061414(r0,r1,r2,r3,sp);
  {
    u32 ea = sp + 52u;
    r3 = LD32(ea);
  }
  {
    u32 ea = r3 + 0u;
    r7 = LD32(ea);
  }
  if (fZ) goto L_62b44;
L_62b44:;
  {
    u32 ea = r4 + 0u;
    ST32(ea, r3);
  }
  f_00373bd0(r0,r1,r2,r3,sp);
  f_001a3588(r0,r1,r2,r3,sp);
}
u64 f_000629b8(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp){return 0;}
"""

class ModelCacheRecoveryTests(unittest.TestCase):
    def test_recovers_by_native_failure_label_without_substituting_data(self):
        patched=module.patch(SOURCE)
        self.assertIn("if (r3 == 0u)",patched)
        self.assertIn("goto L_62b44;",patched)
        self.assertIn("r7 = LD32(ea);",patched)
        self.assertEqual(patched.count(module.MARKER),1)
        self.assertIn("u64 f_000629b8(",patched)
        self.assertNotIn("malloc(",patched)
        self.assertEqual(patched,module.patch(patched))

    def test_rejects_changed_native_cleanup_branch(self):
        with self.assertRaisesRegex(ValueError,"native null-node failure branch"):
            module.patch(SOURCE.replace("L_62b44:;","L_62b45:;"))

    def test_rejects_unknown_original_pointer_load(self):
        with self.assertRaisesRegex(ValueError,"unique null vector dereference"):
            module.patch(SOURCE.replace("r7 = LD32(ea);","r5 = LD32(ea);"))

if __name__=="__main__": unittest.main()
