"""Tests for disassembly categories using invented ARM instructions only."""
import unittest
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_GRP_CALL, CS_GRP_JUMP

class ArmCallClassification(unittest.TestCase):
    def test_arm_control_flow(self):
        # Little-endian ARM encodings: BL +0, BLS +0, BLT +0, BX LR.
        machine = bytes.fromhex("000000eb 0000009a 000000ba 1eff2fe1")
        decoder = Cs(CS_ARCH_ARM, CS_MODE_ARM)
        decoder.detail = True
        inst = list(decoder.disasm(machine, 0x1000))
        self.assertEqual(len(inst), 4)
        self.assertTrue(inst[0].group(CS_GRP_CALL))
        self.assertFalse(inst[0].group(CS_GRP_JUMP))
        for i in inst[1:]:
            self.assertFalse(i.group(CS_GRP_CALL),
                             f"{i.mnemonic} misidentified as a call")
            self.assertTrue(i.group(CS_GRP_JUMP),
                            f"{i.mnemonic} not identified as a jump")

if __name__ == "__main__":
    unittest.main()
