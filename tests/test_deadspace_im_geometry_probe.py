"""Synthetic IM-M3G reference graph, no original game files."""
from pathlib import Path
import struct
import tempfile
import unittest
from zipfile import ZipFile
import zlib
from tools.deadspace_im_geometry_probe import MAGIC, analyze, profile, parse_objects


def u32(i): return struct.pack("<I",i)
def chunk(t,body): return bytes((t,))+u32(len(body))+body


def file():
    va = bytes(12) + bytes((2,3,0)) + struct.pack("<H",2) + bytes(12)
    vb = bytes(12)+bytes((255,255,255,255))+u32(2)+bytes(20)+u32(0)+u32(0)+u32(0)
    assert len(vb)==48
    mesh = bytes(22)+u32(3)+u32(1)+u32(4)
    objects = chunk(0,bytes(12))+chunk(20,va)+chunk(21,vb)+chunk(100,bytes(8))+chunk(14,mesh)
    section = b"\x00"+u32(13+len(objects))+u32(len(objects))+objects
    return MAGIC+section+u32(zlib.adler32(section))


class IMGeometryTests(unittest.TestCase):
    def test_synthetic_references(self):
        d=file()
        self.assertEqual(len(parse_objects(d)),5)
        res=profile(d)
        c=res["counters"]
        self.assertEqual(c["va_plausible_metadata"],1)
        self.assertEqual(c["va_exact_payload_size"],1)
        self.assertEqual(c["vb_position_ref_to_vertexarray"],1)
        self.assertEqual(c["vb_uv_count_exact_layout"],1)
        self.assertEqual(c["mesh_ref_to_vertexbuffer"],1)
        self.assertEqual(c["mesh_index_ref_to_im_indexbuffer"],1)

    def test_bounded_truncation(self):
        self.assertIsNone(parse_objects(file()[:-1]))
        self.assertIsNone(parse_objects(b"garbage"))
        self.assertEqual(profile(b"junk")["status"],"not_framed")

    def test_aggregate_privacy(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive=Path(tmp)/"test.apk"
            with ZipFile(archive,"w") as z:
                z.writestr("assets/SECRET_MODEL_FILENAME.m3g", file())
            result=analyze(archive)
            self.assertEqual(result["candidate_frame_status"]["framed"],1)
            self.assertNotIn("SECRET",str(result))


if __name__=="__main__":
    unittest.main()
