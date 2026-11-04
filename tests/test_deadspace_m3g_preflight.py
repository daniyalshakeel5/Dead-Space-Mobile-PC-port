"""Synthetic JSR-184 headers only; never ship game resources."""
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from zipfile import ZipFile

from tools.deadspace_m3g_preflight import MAGIC, analyze_archive, verify_prefix


def fixture_m3g(corrupt_checksum=False, bad_total=False):
    # One header + one minimal fake scene section; this tests ONLY the header.
    object_data = bytes((1, 0, 0)) + b"\x00" * 8 + b"\x00"
    header_object = b"\x00" + struct.pack("<I", len(object_data)) + object_data
    first_section_len = 9 + len(header_object) + 4
    next_section = b"\x00" + struct.pack("<II", 13, 0)
    next_section += struct.pack("<I", zlib.adler32(next_section))
    size = len(MAGIC) + first_section_len + len(next_section)
    obj = bytes((1, 0, 0)) + struct.pack("<I", size + int(bad_total)) + struct.pack("<I", size) + b"\x00"
    header_object = b"\x00" + struct.pack("<I", len(obj)) + obj
    section = b"\x00" + struct.pack("<II", 9 + len(header_object) + 4, len(header_object))
    section += header_object
    checksum = zlib.adler32(section)
    section += struct.pack("<I", checksum ^ int(corrupt_checksum))
    return MAGIC + section + next_section, section


class M3GPreflightTests(unittest.TestCase):
    def test_valid_first_section(self):
        data, section = fixture_m3g()
        state, info = verify_prefix(data[:12], section, len(data))
        self.assertEqual(state, "valid_jsr184_header")
        self.assertEqual(info["version"], "1.0")
        self.assertFalse(info["external_references"])

    def test_rejects_corruption_and_length_changes(self):
        data, section = fixture_m3g(corrupt_checksum=True)
        self.assertEqual(verify_prefix(data[:12], section, len(data))[0],
                         "checksum_mismatch")
        data, section = fixture_m3g(bad_total=True)
        self.assertEqual(verify_prefix(data[:12], section, len(data))[0],
                         "total_file_size_mismatch")
        data, section = fixture_m3g()
        self.assertEqual(verify_prefix(data[:12], section[:7], len(data))[0],
                         "truncated_section")

    def test_counts_without_exposing_file_bytes_or_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "test.apk"
            data, _ = fixture_m3g()
            with ZipFile(path, "w") as archive:
                archive.writestr("assets/PRIVATE_SCENE.m3g", data)
                archive.writestr("assets/OTHER_PRIVATE.m3g", b"other data" + b"x" * 32)
                archive.writestr("assets/FOO.sps", b"more unknown")
            report = analyze_archive(path)
            self.assertEqual(report["candidate_m3g_files"], 2)
            self.assertEqual(report["header_result_counts"]["valid_jsr184_header"], 1)
            self.assertEqual(report["header_result_counts"]["non_jsr184_magic"], 1)
            self.assertNotIn("PRIVATE", str(report))
            self.assertNotIn("SCENE", str(report))


if __name__ == "__main__":
    unittest.main()
