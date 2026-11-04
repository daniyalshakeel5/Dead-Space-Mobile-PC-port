"""Synthetic topology object references (no original game data)."""
from pathlib import Path
import struct
import tempfile
import unittest
from zipfile import ZipFile
import zlib
from tools.deadspace_im_topology_probe import topology, analyze
from tools.deadspace_im_geometry_probe import MAGIC


def u32(n): return struct.pack("<I",n)
def obj(t,p): return bytes([t])+u32(len(p))+p


def fixture():
    strip = bytes(12)+bytes([0x81])+u32(3)+bytes([0,1,2])+u32(1)+u32(3)
    idx = bytes(8)+u32(0)+u32(2)+u32(3)
    seq = obj(0,bytes(12))+obj(11,strip)+obj(3,bytes(12))+obj(100,idx)
    header=b"\x00"+u32(len(seq)+13)+u32(len(seq))+seq
    return MAGIC+header+u32(zlib.adler32(header))


class IMTopologyTests(unittest.TestCase):
    def test_valid_object_refs_and_encoding(self):
        ok,c=topology(fixture())
        self.assertTrue(ok)
        self.assertEqual(c["strip_encoding_129"],1)
        self.assertEqual(c["strip_array_exact_size"],1)
        self.assertEqual(c["strip_lengths_sum_matches_indices"],1)
        self.assertEqual(c["im_to_strip_reference_valid"],1)
        self.assertEqual(c["im_to_appearance_valid"],1)
        self.assertEqual(c["im_indexbuffer_exact_size"],1)

    def test_reject_invalid(self):
        self.assertFalse(topology(b"garbage")[0])
        self.assertFalse(topology(fixture()[:-4])[0])

    def test_no_file_names_in_report(self):
        with tempfile.TemporaryDirectory() as t:
            path=Path(t)/"synthetic.apk"
            with ZipFile(path,"w") as z:
                z.writestr("assets/PRIVATE_TOPOLOGY.m3g", fixture())
            result=analyze(path)
            self.assertNotIn("PRIVATE_TOPOLOGY",str(result))
            self.assertEqual(result["reference_and_index_layout_counts"][
                "im_to_strip_reference_valid"],1)


if __name__=="__main__":
    unittest.main()
