import struct
import unittest
from tools.deadspace_native_leaf_recover import noop_return, unconditional_tail_target


class OriginalJniNoopProof(unittest.TestCase):
    def test_direct_arm_branch_targets_and_reject_calls(self):
        # ARM B (+0) goes to current PC+8. BL differs by the link bit.
        self.assertEqual(unconditional_tail_target(
            struct.pack("<I",0xea000000),False,0x1000),0x1008)
        self.assertEqual(unconditional_tail_target(
            struct.pack("<I",0xeaffffff),False,0x1000),0x1004)
        self.assertIsNone(unconditional_tail_target(
            struct.pack("<I",0xeb000000),False,0x1000))
        self.assertIsNone(unconditional_tail_target(
            struct.pack("<I",0x0a000000),False,0x1000))

    def test_direct_thumb_branch(self):
        self.assertEqual(unconditional_tail_target(
            struct.pack("<H",0xe000),True,0x1000),0x1004)
        self.assertIsNone(unconditional_tail_target(
            struct.pack("<H",0xd000),True,0x1000))

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
