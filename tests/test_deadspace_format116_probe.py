"""Synthetic only: verify bounded Image2D-116 length/flag/ETC heuristics."""
import struct
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile
import zlib
from tools.deadspace_format116_probe import MAGIC, read_116, analyze


def u32(n):
    return struct.pack("<I", n)


def section(body):
    header = b"\x00" + u32(len(body) + 13) + u32(len(body)) + body
    return header + u32(zlib.adler32(header))


def m3g_texture(width=4, height=4, flag=0, fmt=116, data_size=8,
                trailing=False):
    img = u32(0) + u32(0) + u32(0)
    img += bytes((fmt, flag)) + u32(width) + u32(height)
    if flag == 0:
        img += u32(0) + u32(data_size) + bytes((0x15,)) * data_size
    if trailing:
        img += b"x"
    objects = b"\x00" + u32(0) + b"\x0a" + u32(len(img)) + img
    second = section(objects)
    header_obj = b"\x00" + u32(12) + b"\x01\x00\x00" + u32(0) + u32(0) + b"\x00"
    first = section(header_obj)
    total = len(MAGIC) + len(first) + len(second)
    header_obj = b"\x00" + u32(12) + b"\x01\x00\x00" + u32(total) + u32(total) + b"\x00"
    return MAGIC + section(header_obj) + second


class Format116Tests(unittest.TestCase):
    def test_exact_etc1_size_is_only_hypothesis(self):
        result, meta = read_116(m3g_texture())
        self.assertEqual(result, "structural_payload_exact")
        self.assertEqual(meta["size_class"], "etc1_exact")
        self.assertEqual((meta["w"], meta["h"]), (4, 4))
        self.assertEqual(meta["flag"], 0)
        self.assertFalse(meta["matches_noesis_offset_0x74"])

    def test_other_sizes_fail_etc_heuristic_not_container(self):
        result, meta = read_116(m3g_texture(data_size=5))
        self.assertEqual(result, "structural_payload_exact")
        self.assertEqual(meta["size_class"], "other")
        result, meta = read_116(m3g_texture(trailing=True))
        self.assertEqual(result, "payload_trailing_bytes")
        self.assertEqual(meta["trailing"], 1)

    def test_non_boolean_or_mutable_remains_explicit(self):
        status, meta = read_116(m3g_texture(flag=7))
        self.assertEqual(status, "non_boolean_mutable")
        self.assertEqual(meta["flag"], 7)
        status, meta = read_116(m3g_texture(flag=1))
        self.assertEqual(status, "mutable_no_data")

    def test_corrupt_checksum_rejected(self):
        b = bytearray(m3g_texture())
        b[-1] ^= 1
        self.assertEqual(read_116(bytes(b))[0], "no_standard_two_section_image")

    def test_aggregate_report_contains_no_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            apk = Path(tmp) / "fixture.apk"
            with ZipFile(apk, "w") as z:
                z.writestr("assets/DO_NOT_PUBLISH_TEXTURE.m3g", m3g_texture())
                z.writestr("assets/ANOTHER_PRIVATE_TEXTURE.m3g",
                           m3g_texture(flag=1))
            result = analyze(apk)
            self.assertEqual(result["compressed_block_size_hypotheses"]["etc1_exact"], 1)
            self.assertNotIn("DO_NOT_PUBLISH", str(result))
            self.assertEqual(result["parsing_status"]["mutable_no_data"], 1)


if __name__ == "__main__":
    unittest.main()
