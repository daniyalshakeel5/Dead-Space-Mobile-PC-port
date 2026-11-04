"""Small, dependency-free test fixtures. No game binaries needed."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

from tools.deadspace_audit import analyze_library, scan_apk, scan_repo


class TestPortAudit(unittest.TestCase):
    def test_repository_inventory(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "readme.txt").write_text("test", encoding="utf-8")
            (root / "sample.so").write_bytes(b"test")
            report = scan_repo(root)
            self.assertEqual(report["files_by_suffix"][".txt"], 1)
            self.assertEqual(report["native_file_paths"], ["sample.so"])

    def test_apk_inventory_with_fake_arm_header(self):
        with tempfile.TemporaryDirectory() as d:
            apk = Path(d) / "sample.apk"
            header = bytearray(52)
            header[:4] = b"\x7fELF"
            header[4] = 1  # ELF32
            header[5] = 1  # little-endian
            header[16:18] = (3).to_bytes(2, "little")  # ET_DYN
            header[18:20] = (40).to_bytes(2, "little")  # EM_ARM
            with ZipFile(apk, "w") as archive:
                archive.writestr("lib/armeabi-v7a/libTest.so", header)
                archive.writestr("AndroidManifest.xml", b"fixture")
                archive.writestr("classes.dex", b"test dex")
                archive.writestr("assets/data/sector.dat", b"game fixture")
            report = scan_apk(apk)
            self.assertTrue(report["has_android_manifest"])
            self.assertEqual(report["dex_count"], 1)
            self.assertEqual(report["entry_groups"]["assets"]["files"], 1)
            self.assertEqual(report["entry_groups"]["lib"]["files"], 1)
            self.assertEqual(report["archive_entries"], 4)
            self.assertGreater(report["archive_uncompressed_bytes"], 52)
            self.assertEqual(report["native_libraries"][0]["machine"], "ARM32")
            self.assertEqual(report["native_libraries"][0]["elf_class"], "ELF32")
            self.assertEqual(report["native_libraries"][0]["elf_type"], "ET_DYN")

    def test_native_import_evidence_is_platform_only(self):
        with tempfile.TemporaryDirectory() as d:
            elf = Path(d) / "test.so"
            data = bytearray(52)
            data[:4] = bytes.fromhex("7f454c46")
            data[4], data[5] = 1, 1
            data[16:18] = (3).to_bytes(2, "little")
            data[18:20] = (40).to_bytes(2, "little")
            elf.write_bytes(data)
            # readelf fixture rows: public imports plus one private symbol.
            symbols = """
   1: 00000000     0 FUNC GLOBAL DEFAULT UND AAssetManager_open
   2: 00000000     0 FUNC GLOBAL DEFAULT UND glCompressedTexImage2D
   3: 00000000     0 FUNC GLOBAL DEFAULT UND eglCreateContext
   4: 00000000     0 FUNC GLOBAL DEFAULT UND inflate
   5: 00000000     0 FUNC GLOBAL DEFAULT UND MyPrivateNativeSymbol
   6: 00000100     0 FUNC GLOBAL DEFAULT  11 JNI_OnLoad
"""
            dynamic = " 0x00000001 (NEEDED) Shared library: [libGLESv2.so]"
            def fake_readelf(*args):
                return dynamic if "-d" in args else symbols
            with patch("tools.deadspace_audit.command_output", side_effect=fake_readelf):
                result = analyze_library(elf, "lib/armeabi-v7a/fixture.so")
            self.assertEqual(result["machine"], "ARM32")
            self.assertEqual(result["elf_type"], "ET_DYN")
            self.assertIn("JNI_OnLoad", result["bootstrap_exports"])
            self.assertEqual(result["needed_libraries"], ["libGLESv2.so"])
            self.assertEqual(result["platform_api_import_evidence"]["android_asset_manager"],
                             ["AAssetManager_open"])
            self.assertEqual(result["platform_api_import_evidence"]["gles_texture"],
                             ["glCompressedTexImage2D"])
            self.assertEqual(result["platform_api_import_evidence"]["egl_context"],
                             ["eglCreateContext"])
            self.assertEqual(result["platform_api_import_evidence"]["decompression"],
                             ["inflate"])
            self.assertNotIn("MyPrivateNativeSymbol", str(result))
            self.assertEqual(result["undefined_symbol_count"], 5)
            self.assertTrue(result["symbol_scan_available"])

    def test_truncated_or_invalid_elf_header_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bad.so"
            path.write_bytes(bytes.fromhex("7f454c460101") + b"0" * 25)
            self.assertEqual(analyze_library(path, "lib/test.so")["error"],
                             "Truncated ELF header")
            path.write_bytes(bytes.fromhex("7f454c460901") + b"0" * 52)
            self.assertEqual(analyze_library(path, "lib/test.so")["error"],
                             "Invalid ELF class or endianness")


if __name__ == "__main__":
    unittest.main()
