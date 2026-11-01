"""Synthetic tests for bounded ELF32 ARM memory mapping; no real game bytes."""
import struct
import unittest
from tools.deadspace_original_code_window import elf32_load_ranges, bounded_virtual_read


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
