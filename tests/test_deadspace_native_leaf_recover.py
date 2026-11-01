import struct
import unittest
from tools.deadspace_native_leaf_recover import noop_return


class OriginalJniNoopProof(unittest.TestCase):
    def test_arm_bx_lr_only(self):
        self.assertTrue(noop_return(struct.pack("<I",0xe12fff1e),False))
        self.assertTrue(noop_return(struct.pack("<I",0xe1a0f00e),False))
        self.assertFalse(noop_return(struct.pack("<II",0xe3a00001,0xe12fff1e),False))
        self.assertFalse(noop_return(struct.pack("<I",0x112fff1e),False))

    def test_thumb_bx_lr_only(self):
        self.assertTrue(noop_return(struct.pack("<H",0x4770),True))
        self.assertTrue(noop_return(struct.pack("<HH",0x4770,0xbf00),True))
        self.assertFalse(noop_return(struct.pack("<HH",0x2001,0x4770),True))
        self.assertFalse(noop_return(struct.pack("<H",0x4778),True))

if __name__=="__main__":
    unittest.main()
