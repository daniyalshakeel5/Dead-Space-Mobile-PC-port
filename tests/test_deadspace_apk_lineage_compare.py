import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile
from tools.deadspace_apk_lineage_compare import metadata, comparison


def fake_elf(machine=40):
    elf = bytearray(64)
    elf[:4] = b"\x7fELF"
    elf[4] = 1
    elf[5] = 1
    elf[18:20] = machine.to_bytes(2, "little")
    return bytes(elf)


class LineageTests(unittest.TestCase):
    def test_asset_change_and_unchanged_native_library_under_renamed_path(self):
        with tempfile.TemporaryDirectory() as folder:
            a = Path(folder) / "old.apk"
            b = Path(folder) / "new.apk"
            with ZipFile(a, "w") as z:
                z.writestr("AndroidManifest.xml", b"fake")
                z.writestr("assets/published/level.bin", b"unchanged")
                z.writestr("assets/published/texture.bin", b"original")
                z.writestr("assets/published/old.bin", b"old")
                z.writestr("lib/armeabi-v7a/liboriginal.so", fake_elf())
            with ZipFile(b, "w") as z:
                z.writestr("AndroidManifest.xml", b"fake")
                z.writestr("assets/published/level.bin", b"unchanged")
                z.writestr("assets/published/texture.bin", b"modded")
                z.writestr("assets/published/new.bin", b"new")
                z.writestr("assets/runtime/liboriginal.so", fake_elf())
                z.writestr("lib/arm64-v8a/libruntime.so", fake_elf(183))
            result = comparison(metadata(a), metadata(b))
            assets = result["asset_comparison"]
            self.assertEqual(assets["shared_path_count"], 2)
            self.assertEqual(assets["matching_size_and_crc32_count"], 1)
            self.assertEqual(assets["changed_size_or_crc32_count"], 1)
            self.assertEqual(assets["original_only_count"], 1)
            self.assertEqual(assets["reference_only_count"], 2)  # new asset plus native ELF moved under assets/
            self.assertEqual(assets["cross_path_matching_size_and_crc32_count"], 0)
            self.assertEqual(assets["original_directory_groups"]["assets/published"], 3)
            self.assertEqual(assets["changed_extensions"], {".bin": 1})
            self.assertEqual(len(result["native_elf_byte_identical_matches"]), 1)
            self.assertEqual(result["reference"]["native_libraries"][-1]["elf_machine"], "AArch64" if result["reference"]["native_libraries"][-1]["path"].startswith("lib/arm64") else "ARM32")
            # Individual copyrighted asset paths are not emitted in summaries.
            self.assertNotIn("assets", result["original"])
            self.assertNotIn("assets", result["reference"])
    def test_renamed_asset_same_crc_and_size_is_only_a_candidate(self):
        with tempfile.TemporaryDirectory() as folder:
            a, b = Path(folder) / "a.apk", Path(folder) / "b.apk"
            with ZipFile(a, "w") as z:
                z.writestr("assets/published/data/game.bin", b"level payload")
            with ZipFile(b, "w") as z:
                z.writestr("assets/game/published/data/game.bin", b"level payload")
            v = comparison(metadata(a), metadata(b))["asset_comparison"]
            self.assertEqual(v["shared_path_count"], 0)
            self.assertEqual(v["original_only_count"], 1)
            self.assertEqual(v["reference_only_count"], 1)
            self.assertEqual(v["cross_path_matching_size_and_crc32_count"], 1)

    def test_reject_duplicate_member_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / "duplicate.apk"
            with ZipFile(p, "w") as z:
                z.writestr("assets/a", b"a")
                import warnings
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore")
                    z.writestr("assets/a", b"b")
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                metadata(p)


if __name__ == "__main__":
    unittest.main()
