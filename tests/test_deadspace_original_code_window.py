"""Synthetic tests for bounded ELF32 ARM memory mapping; no real game bytes."""
import struct
import unittest
from tools.deadspace_original_code_window import (
    elf32_load_ranges, bounded_virtual_read, proven_frame_dispatch_prefix)


class ArmWindowTests(unittest.TestCase):
    def make_elf(self):
        data=bytearray(512)
        data[:6]=b"\x7fELF\x01\x01"
        struct.pack_into("<I",data,28,52)  # e_phoff
        struct.pack_into("<HH",data,42,32,1)
        struct.pack_into("<IIIIIIII",data,52,
                         1,128,0x1000,0,16,16,5,4096)
        data[128:144]=bytes(range(16))
        return bytes(data)

    def test_exact_original_arm_frame_dispatch_proof(self):
        specs={
            16:("ldr","r3, [r4]"),20:("cmp","r3, #0"),
            24:("beq","#0x27ef98"),28:("ldr","r3, [r4]"),
            32:("cmp","r3, #1"),36:("beq","#0x27efac"),
            40:("add","sp, sp, #8"),44:("pop","{r4, pc}"),
            48:("add","r0, r4, #8"),52:("bl","#0x27f870"),
            56:("mov","r3, #1"),60:("str","r3, [r4]"),
            64:("b","#0x27ef90"),68:("add","r0, r4, #8"),
            72:("bl","#0x27fc68"),76:("cmp","r0, #0"),
            80:("bne","#0x27efc8"),84:("add","sp, sp, #8"),
            88:("pop","{r4, lr}"),92:("b","#0x27eecc"),
        }
        code=[{"offset":at,"op":op,"args":args}
              for at,(op,args) in specs.items()]
        self.assertTrue(all(proven_frame_dispatch_prefix(code).values()))
        altered=[dict(entry) for entry in code]
        next(x for x in altered if x["offset"]==60)["args"]="r3, [r4, #4]"
        result=proven_frame_dispatch_prefix(altered)
        self.assertFalse(result["state_zero_call_then_store_one"])
        self.assertTrue(result["state_one_tick_result_branch"])
        altered=[dict(entry) for entry in code]
        next(x for x in altered if x["offset"]==80)["args"]="#0x27efbc"
        result=proven_frame_dispatch_prefix(altered)
        self.assertTrue(result["state_zero_call_then_store_one"])
        self.assertFalse(result["state_one_tick_result_branch"])

    def test_file_backed_segment_and_exact_offsets(self):
        buf=self.make_elf()
        spans=elf32_load_ranges(buf)
        self.assertEqual(spans,[(0x1000,0x1010,128)])
        self.assertEqual(bounded_virtual_read(buf,spans,0x1004,4),
                         bytes([4,5,6,7]))
        self.assertEqual(bounded_virtual_read(buf,spans,0x100f,1),
                         bytes([15]))

    def test_rejects_bad_address_and_truncated_code(self):
        buf=self.make_elf()
        spans=elf32_load_ranges(buf)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x100f,2)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x1010,1)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x1000,257)

    def test_reject_elf_overflow(self):
        bad=bytearray(self.make_elf())
        struct.pack_into("<I",bad,52+16,400)
        with self.assertRaises(ValueError):
            elf32_load_ranges(bytes(bad))


if __name__=="__main__":
    unittest.main()
