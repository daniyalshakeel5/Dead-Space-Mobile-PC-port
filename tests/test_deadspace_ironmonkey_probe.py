"""Synthetic-only IronMonkey framing fixtures and privacy checks."""
import struct
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile
import zlib
from tools.deadspace_ironmonkey_probe import MAGIC, probe_file, analyze, parse_chunks


def u32(n):
    return struct.pack("<I", n)


def chunk(kind, payload):
    return bytes((kind,)) + u32(len(payload)) + payload


def resource(corrupt_crc=False):
    obj = chunk(0, bytes(12)) + chunk(20, bytes(7)) + chunk(14, bytes(8))
    start = b"\x00" + u32(len(obj)+13) + u32(len(obj)) + obj
    checksum = zlib.adler32(start) ^ int(corrupt_crc)
    return MAGIC + start + u32(checksum)


class IM3GTests(unittest.TestCase):
    def test_bounded_chunks(self):
        test = chunk(0, b"") + chunk(20, b"abc")
        ok, counts = parse_chunks(test, 0, len(test))
        self.assertTrue(ok)
        self.assertEqual(counts["20"], 1)
        self.assertFalse(parse_chunks(test[:-1], 0, len(test)-1)[0])
        self.assertFalse(parse_chunks(test, 5, 0)[0])

    def test_standard_framing_hypothesis(self):
        kind, fields = probe_file(resource())
        self.assertEqual(kind, "im_m3g")
        self.assertTrue(fields["section_within_file"])
        self.assertTrue(fields["checksum_matches"])
        self.assertEqual(fields["layouts"]["section_payload_minus_checksum"]["14"], 1)
        _, bad = probe_file(resource(corrupt_crc=True))
        self.assertFalse(bad["checksum_matches"])
        self.assertIn("section_payload_minus_checksum", bad["layouts"])

    def test_archive_aggregation_no_names(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/"synthetic.apk"
            with ZipFile(path,"w") as z:
                z.writestr("assets/PRIVATE_MODEL.m3g", resource())
                z.writestr("assets/PRIVATE_TEXTURE.m3g", b"not model" + bytes(40))
            outcome=analyze(path)
            self.assertEqual(outcome["family_classification"]["im_m3g"],1)
            self.assertEqual(outcome["standard_section_object_type_histogram"]["20"],1)
            self.assertNotIn("PRIVATE_MODEL",str(outcome))
            self.assertNotIn("PRIVATE_TEXTURE",str(outcome))


if __name__ == "__main__":
    unittest.main()
