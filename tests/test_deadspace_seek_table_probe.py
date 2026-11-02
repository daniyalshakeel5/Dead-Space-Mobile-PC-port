import struct
import unittest
from tools.deadspace_seek_table_probe import (
    decode_origin_table, DISPATCH, TABLE_DISPLACEMENT, LITERAL
)


class SeekTableRecoveryTests(unittest.TestCase):
    def test_pc_relative_elf_data_lookup(self):
        raw = bytearray(1024)
        code = [(0x273000, 0x273300, 0)]
        # The table is in another ELF virtual segment (not adjacent memory).
        data = [(0x48a000, 0x48a100, 768)]
        desired = 0x48a020
        displacement = desired - (DISPATCH + 12 + TABLE_DISPLACEMENT)
        struct.pack_into("<i", raw, LITERAL - 0x273000, displacement)
        struct.pack_into("<II", raw, 768 + 0x20, 1, 2)
        self.assertEqual(decode_origin_table(raw, code, data),
                         (desired, 1, 2))

    def test_unmapped_table_rejected(self):
        raw = bytearray(1024)
        code = [(0x273000, 0x273300, 0)]
        data = [(0x48a000, 0x48a100, 768)]
        struct.pack_into("<i", raw, LITERAL - 0x273000, 0)
        with self.assertRaises(ValueError):
            decode_origin_table(raw, code, data)


if __name__ == "__main__":
    unittest.main()
