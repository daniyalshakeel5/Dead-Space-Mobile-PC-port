"""Synthetic IM geometry numeric and strip-range regression fixtures."""
from pathlib import Path
import struct
import tempfile
import unittest
from zipfile import ZipFile
import zlib
from tools.deadspace_im_numeric_probe import inspect_file, analyze
from tools.deadspace_im_geometry_probe import MAGIC

def u32(n): return struct.pack("<I",n)
def chunk(kind,payload): return bytes([kind])+u32(len(payload))+payload


def fixture(out_of_bounds=False):
    positions=bytes(12)+bytes([2,3,0])+struct.pack("<H",3)+bytes(18)
    vb=bytes(12)+bytes([255,255,255,255])+u32(2)+struct.pack("<ffff",0,0,0,1)+u32(0)+u32(0)+u32(0)
    assert len(vb)==48
    strip=bytes(12)+bytes([0x81])+u32(3)+bytes([0,1,3 if out_of_bounds else 2])+u32(1)+u32(3)
    ib=bytes(8)+u32(0)+u32(4)+u32(5)
    mesh=bytes(22)+u32(3)+u32(1)+u32(6)
    obj=(chunk(0,bytes(12))+chunk(20,positions)+chunk(21,vb)+chunk(11,strip)+
         chunk(3,bytes(12))+chunk(100,ib)+chunk(14,mesh))
    sec=b"\x00"+u32(13+len(obj))+u32(len(obj))+obj
    return MAGIC+sec+u32(zlib.adler32(sec))


class IMNumericTests(unittest.TestCase):
    def test_references_and_finite_bias(self):
        ok,c=inspect_file(fixture())
        self.assertTrue(ok)
        for metric in ("mesh_ref_to_vertexbuffer",):
            self.assertNotIn(metric,c) # numeric probe uses scoped counters
        self.assertEqual(c["mesh_position_int16x3_exact"],1)
        self.assertEqual(c["mesh_bias_and_scale_finite"],1)
        self.assertEqual(c["mesh_verified_index_buffers"],1)
        self.assertEqual(c["mesh_all_strips_three_indices"],1)
        self.assertEqual(c["mesh_indices_within_position_vertex_count"],1)

    def test_invalid_vertex_index(self):
        ok,c=inspect_file(fixture(out_of_bounds=True))
        self.assertTrue(ok)
        self.assertEqual(c["mesh_indices_outside_position_vertex_count"],1)

    def test_aggregate_never_shows_names(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"fixture.apk"
            with ZipFile(path,"w") as z:
                z.writestr("assets/PRIVATE_REAL_NAME.m3g",fixture())
            report=analyze(path)
            self.assertEqual(report["numeric_geometry_checks"]["mesh_verified_index_buffers"],1)
            self.assertNotIn("PRIVATE_REAL_NAME",str(report))


if __name__=="__main__":
    unittest.main()
