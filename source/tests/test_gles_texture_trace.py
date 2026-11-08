#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest

SCRIPT=Path(__file__).resolve().parents[1]/"tools"/"compare_gles_texture_trace.py"
spec=importlib.util.spec_from_file_location("gles_trace",SCRIPT)
app=importlib.util.module_from_spec(spec)
spec.loader.exec_module(app)

class GLLogTests(unittest.TestCase):
    def test_original_logger_decimal_enums(self):
        # Format fields use %u / %d in milestone6 gen_gl.py; never 100/116.
        txt="\n".join([
            "glClear(16384)",
            "glTexImage2D(3553, 0, 6408, 64, 64, 0, 6408, 5121, 0x12345678)",
            "glTexImage2D(3553, 0, 6407, 256, 128, 0, 6407, 5121, 0x10101010)",
            "glTexImage2D(3553, 1, 6407, 128, 64, 0, 6407, 5121, 0x10101010)",
            "glTexImage2D(3553, 0, 0x1908, 32, 16, 0, 0x1908, 0x1401, 0x1)",
        ])
        x=app.parse_log(txt)
        self.assertEqual(x["total_gl_uploads"],4)
        self.assertEqual(x["invalid_upload_records"],0)
        self.assertEqual(x["top_dimensions"]["256x128"],1)
        self.assertEqual(x["mip_level_counts"]["glTexImage2D:level=1"],1)
        self.assertTrue(any("GL_RGBA" in t for t in x["upload_api_and_formats"]))
        self.assertTrue(any("GL_RGB" in t for t in x["upload_api_and_formats"]))
        self.assertTrue(any("not original .m3g" in x for x in x["scope_notes"]))

    def test_unsupported_format_not_mislabeled_etc(self):
        x=app.parse_log("glTexImage2D(3553,0,116,8,8,0,116,5121,0x1)")
        self.assertEqual(x["total_gl_uploads"],1)
        self.assertTrue(any("0x74" in t for t in x["upload_api_and_formats"]))
        self.assertFalse(any("GL_ETC1" in t for t in x["upload_api_and_formats"]))

    def test_malformed_is_bounded(self):
        x=app.parse_log("glTexImage2D(???)\nglTexImage2D(3553,0,6408,50000,8,0,6408,5121,0x0)")
        self.assertEqual(x["total_gl_uploads"],0)
        self.assertEqual(x["invalid_upload_records"],2)
        self.assertEqual(len(x["warnings"]),2)

if __name__=="__main__":
    unittest.main()
