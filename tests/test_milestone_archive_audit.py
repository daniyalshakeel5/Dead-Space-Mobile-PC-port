"""Synthetic, no-game-data tests for secure milestone ZIP metadata intake."""
import io
from pathlib import Path
import tempfile
import unittest
from zipfile import ZIP_DEFLATED, ZipFile

from tools.milestone_archive_audit import audit, unsafe_entry


class ArchiveAuditTests(unittest.TestCase):
    def make_zip(self, folder, entries, compression=ZIP_DEFLATED):
        path = Path(folder) / "checkpoint.zip"
        with ZipFile(path, "w", compression=compression) as archive:
            for name, content in entries:
                archive.writestr(name, content)
        return path

    def test_safe_archive_only_counts_and_no_original_filenames(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_zip(folder, [
                ("milestone4/src/main.cpp", "// private work"),
                ("milestone4/include/control.hpp", "#pragma once"),
                ("milestone4/README.md", "research note"),
            ])
            report = audit(archive)
            self.assertTrue(report["valid_zip"])
            self.assertTrue(report["safe_to_extract_after_independent_license_review"])
            self.assertEqual(report["code_file_count"], 3)
            self.assertEqual(report["restricted_binary_file_count"], 0)
            self.assertEqual(report["counts_by_extension"][".cpp"], 1)
            self.assertEqual(report["entry_count"], 3)
            self.assertEqual(report["source_bytes_published"], 0)
            self.assertNotIn("milestone4/src/main.cpp", str(report))
            self.assertNotIn("// private work", str(report))

    def test_path_guard(self):
        for candidate in ("../unsafe.py", "/root/unsafe.py",
                          "C:/tmp/readme.md", "ok\\bad.cpp",
                          "dir//file.cpp", "folder/./file.cpp",
                          "folder/../file.cpp", "folder/"):
            if candidate == "folder/":
                self.assertFalse(unsafe_entry(candidate))
            else:
                self.assertTrue(unsafe_entry(candidate), candidate)

    def test_invalid_paths_are_reported_not_extracted(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_zip(folder, [
                ("src/safe.cpp", "// okay"),
                ("../../private.hpp", "#secret"),
            ])
            report = audit(archive)
            self.assertIn("unsafe_entry_path", report["critical_findings"])
            self.assertFalse(report["safe_to_extract_after_independent_license_review"])

    def test_colliding_casefold_names(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_zip(folder, [
                ("Engine/Input.cpp", "a"),
                ("engine/input.cpp", "b"),
            ])
            report = audit(archive)
            self.assertIn("duplicate_or_case_colliding_path", report["critical_findings"])

    def test_game_binary_is_not_approved(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_zip(folder, [
                ("src/source.py", "print('test')"),
                ("data/DeadSpace.so", b"\x7fELF"),
            ])
            report = audit(archive)
            self.assertEqual(report["restricted_binary_file_count"], 1)
            self.assertIn("restricted_binary_extensions_present", report["warnings"])
            self.assertFalse(report["safe_to_extract_after_independent_license_review"])

    def test_suspicious_ratio_flag(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = self.make_zip(folder, [
                ("payload.txt", b"A" * (2 * 1024 * 1024)),
            ])
            report = audit(archive)
            self.assertIn("suspicious_compression_ratio", report["warnings"])

    def test_non_zip(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder) / "checkpoint.zip"
            archive.write_bytes(b"not a zip")
            report = audit(archive)
            self.assertFalse(report["valid_zip"])
            self.assertIn("invalid_or_unreadable_zip", report["critical_findings"])


if __name__ == "__main__":
    unittest.main()
