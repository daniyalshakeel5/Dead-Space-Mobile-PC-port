"""Only synthetic fixture data. Never uses original Dead Space game resources."""
import json
from pathlib import Path
import tempfile
import unittest
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo

from tools.deadspace_asset_recon import investigate, sniff, safe_name, group_for


class AssetReconTests(unittest.TestCase):
    def test_common_signatures(self):
        cases = {
            b"\x89PNG\r\n\x1a\nrest": "png",
            b"DDS something": "dds",
            b"\xabKTX 11\xbb\r\n\x1a\n": "ktx1",
            b"\xabKTX 20\xbb\r\n\x1a\n": "ktx2",
            b"PVR\x03rest": "pvr3",
            b"PKM 20texture": "pkm",
            b"\x13\xab\xa1\x5c": "astc",
            b"OggS\x00": "ogg",
            b"RIFF1234WAVEaudio": "wav",
            b"RIFF1234WEBPimage": "webp",
            b"PK\x03\x04": "zip",
            b"\x7fELF": "elf",
            b"dex\n035\x00": "dex",
            b"UnityFS\x00resource": "unityfs",
            b"\x00\x01\x02\x03": "unrecognized",
            bytes.fromhex("ab4a5352313834bb0d0a1a0a") + b"xx": "m3g_jsr184",
            b"\x78\x9c\x03\x00": "possible-zlib",
        }
        for data, expected in cases.items():
            with self.subTest(expected=expected):
                self.assertEqual(sniff(data), expected)

    def test_only_known_group_labels_and_paths(self):
        self.assertEqual(group_for("assets/game.data"), "assets")
        self.assertEqual(group_for("res/raw/a.ogg"), "res")
        self.assertEqual(group_for("PRIVATE_SECRET/file.bin"), "other")
        for path in ("../thing", "/etc/passwd", "C:/bad", "sub\\bad", "x/./y"):
            self.assertFalse(safe_name(path))
        self.assertTrue(safe_name("assets/game/sub.dat"))

    def test_fixture_recon_has_no_source_paths_or_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            apk = Path(folder) / "fixture.apk"
            big_unknown = b"\x01\x04\x08\xff" + bytes(range(256)) * 5000
            with ZipFile(apk, "w") as z:
                z.writestr("AndroidManifest.xml", "fixture only", ZIP_STORED)
                z.writestr("classes.dex", b"dex\n035\x00", ZIP_STORED)
                z.writestr("assets/TOP_SECRET_texture.png",
                           b"\x89PNG\r\n\x1a\n" + b"\0" * 32, ZIP_DEFLATED)
                z.writestr("assets/sub/HiddenWorld.raw", big_unknown, ZIP_STORED)
                z.writestr("res/raw/SENSITIVE_OST.ogg", b"OggSdummy", ZIP_STORED)
                z.writestr("lib/armeabi-v7a/libDeadSpace.so",
                           b"\x7fELF" + b"\0" * 12, ZIP_STORED)
            result = investigate(apk)
            self.assertTrue(result["valid_zip"])
            self.assertEqual(result["archive_entry_count"], 6)
            self.assertEqual(result["inspected_prefixes"], 4)
            self.assertEqual(result["group_stats"]["assets"]["count"], 2)
            self.assertEqual(result["magic_stats"]["png"]["count"], 1)
            self.assertEqual(result["magic_stats"]["elf"]["count"], 1)
            self.assertEqual(result["magic_stats"]["ogg"]["count"], 1)
            self.assertEqual(result["large_unrecognized_files"], 1)
            self.assertEqual(result["large_unrecognized_declared_bytes"],
                             len(big_unknown))
            public = json.dumps(result)
            self.assertNotIn("TOP_SECRET", public)
            self.assertNotIn("HiddenWorld", public)
            self.assertNotIn("SENSITIVE_OST", public)
            self.assertNotIn("libDeadSpace.so", public)
            self.assertFalse(result["original_filenames_exported"])
            self.assertFalse(result["original_data_exported"])

    def test_path_traversal_is_not_probed(self):
        with tempfile.TemporaryDirectory() as folder:
            apk = Path(folder) / "fixture.apk"
            with ZipFile(apk, "w") as z:
                z.writestr("assets/../evil.dat", b"\x7fELFhacked")
                z.writestr("assets/safe.png", b"\x89PNG\r\n\x1a\n")
            result = investigate(apk)
            self.assertTrue(result["valid_zip"])
            self.assertEqual(result["skipped_counts"]["unsafe_filename"], 1)
            self.assertEqual(result["inspected_prefixes"], 1)

    def test_symlink_not_read(self):
        with tempfile.TemporaryDirectory() as folder:
            apk = Path(folder) / "fixture.apk"
            link = ZipInfo("assets/link.dat")
            link.create_system = 3
            link.external_attr = (0o120777 << 16)
            with ZipFile(apk, "w") as z:
                z.writestr(link, "untrusted target")
                z.writestr("assets/actual.png", b"\x89PNG\r\n\x1a\n")
            result = investigate(apk)
            self.assertEqual(result["skipped_counts"]["symlink"], 1)
            self.assertEqual(result["inspected_prefixes"], 1)

    def test_real_format_probe_is_aggregate_only(self):
        with tempfile.TemporaryDirectory() as folder:
            apk = Path(folder) / "fixture.apk"
            with ZipFile(apk, "w") as z:
                z.writestr("assets/PRIVATE_SPRITE.sph", b"\x00" * 8)
                z.writestr("assets/PRIVATE_SPRITE.sps", b"\x78\x9c" + b"data")
                z.writestr("assets/UNMATCHED.sph", b"\x00" * 8)
                z.writestr("assets/PRIVATE_MODEL.m3g",
                           bytes.fromhex("ab4a5352313834bb0d0a1a0a") + b"\x00")
            report = investigate(apk)
            self.assertEqual(report["sprite_stem_stats"]["paired_count"], 1)
            self.assertEqual(report["sprite_stem_stats"]["sph_count"], 2)
            self.assertEqual(report["extension_magic_stats"][".m3g"]["m3g_jsr184"]["count"], 1)
            self.assertEqual(report["extension_magic_stats"][".sps"]["possible-zlib"]["count"], 1)
            public_text = json.dumps(report)
            self.assertNotIn("PRIVATE_SPRITE", public_text)
            self.assertNotIn("PRIVATE_MODEL", public_text)

    def test_invalid_zip_is_reported_without_exception_details(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "SECRET_NAME.apk"
            path.write_bytes(b"garbage")
            result = investigate(path)
            self.assertFalse(result["valid_zip"])
            self.assertIn("invalid_or_unreadable_zip", result["warnings"])
            self.assertNotIn("SECRET_NAME", json.dumps(result))


if __name__ == "__main__":
    unittest.main()
