"""Small, dependency-free test fixtures. No game binaries needed."""
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile

from tools.deadspace_audit import scan_apk, scan_repo


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
            header = bytearray(32)
            header[:4] = b"\x7fELF"
            header[4] = 1  # ELF32
            header[5] = 1  # little-endian
            header[18:20] = (40).to_bytes(2, "little")  # EM_ARM
            with ZipFile(apk, "w") as archive:
                archive.writestr("lib/armeabi-v7a/libTest.so", header)
                archive.writestr("AndroidManifest.xml", b"fixture")
            report = scan_apk(apk)
            self.assertTrue(report["has_android_manifest"])
            self.assertEqual(report["native_libraries"][0]["machine"], "ARM32")
            self.assertEqual(report["native_libraries"][0]["elf_class"], "ELF32")


if __name__ == "__main__":
    unittest.main()
