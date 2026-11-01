"""Original ARM JNI constant recovery: accept only proven return sequences."""
import struct
import unittest
from tools.deadspace_native_enum_recover import constant_return, rotate_right, literal_return


class NativeEnumTests(unittest.TestCase):
    def test_arm_mov_imm_and_bx_lr(self):
        self.assertEqual(constant_return(struct.pack("<II",0xe3a00007,0xe12fff1e),False),7)
        self.assertEqual(constant_return(struct.pack("<II",0xe3a00101,0xe12fff1e),False),0x40000000)
        self.assertEqual(constant_return(struct.pack("<II",0xe3a00000,0xe1a0f00e),False),0)

    def test_thumb_movs_and_return(self):
        self.assertEqual(constant_return(struct.pack("<HH",0x2004,0x4770),True),4)
        self.assertIsNone(constant_return(struct.pack("<HH",0x2101,0x4770),True))
        self.assertIsNone(constant_return(struct.pack("<HH",0x2002,0xd000),True))

    def test_exact_movw_return_encoding(self):
        # Thumb-2 MOVW r0, #1000 (0x3e8) and BX LR.
        self.assertEqual(constant_return(struct.pack("<HHH",
            0xf240,0x30e8,0x4770),True),1000)
        # ARM MOVW r0,#1000 and BX LR.
        self.assertEqual(constant_return(struct.pack("<II",
            0xe30003e8,0xe12fff1e),False),1000)
        self.assertIsNone(constant_return(struct.pack("<HHH",
            0xf240,0x31e8,0x4770),True)) # wrong destination r1

    def test_thumb_pc_literal_return(self):
        # LDR r0,[PC,#4]; BX LR; 4 alignment/padding bytes; literal at +8.
        code = struct.pack("<HHI",0x4801,0x4770,0) + struct.pack("<I",12)
        self.assertEqual(literal_return(code[:4],True,0x1000,code,0x1000),12)
        self.assertIsNone(literal_return(
            struct.pack("<HH",0x4901,0x4770),True,0x1000,code,0x1000))
        self.assertIsNone(literal_return(code[:4],True,0x1000,code[:8],0x1000))

    def test_arm_pc_literal_return(self):
        code = struct.pack("<III",0xe59f0000,0xe12fff1e,1000)
        self.assertEqual(literal_return(code[:8],False,0x4000,code,0x4000),1000)
        self.assertIsNone(literal_return(
            struct.pack("<II",0xe59f1000,0xe12fff1e),False,0x4000,code,0x4000))
        self.assertIsNone(literal_return(
            struct.pack("<II",0xe59f0000,0xe12fff1f),False,0x4000,code,0x4000))
        self.assertIsNone(literal_return(code[:8],False,0x4000,code[:8],0x4000))

    def test_reject_branch_and_prologue(self):
        self.assertIsNone(constant_return(struct.pack("<II",0xe3a00001,0xeaffffff),False))
        self.assertIsNone(constant_return(struct.pack("<II",0xe1a00001,0xe12fff1e),False))
        self.assertIsNone(constant_return(b"",False))
        self.assertIsNone(constant_return(b"",True))


if __name__=="__main__":
    unittest.main()
