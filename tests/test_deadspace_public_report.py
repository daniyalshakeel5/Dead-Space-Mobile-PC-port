"""Redaction checks for public APK evidence reports, using synthetic content only."""
import json
import unittest

from tools.deadspace_public_report import publish


class PublicReportTests(unittest.TestCase):
    def test_strips_private_apk_paths_hashes_and_symbols(self):
        audit = {
            "apk": {
                "apk_sha256": "SECRETAPKIDENTITY",
                "apk_size_bytes": 326000000,
                "archive_entries": 3,
                "dex_count": 1,
                "native_libraries": [{
                    "apk_path": "lib/arm/SECRET_GAME_ENGINE.so",
                    "sha256": "SECRETLIBIDENTITY",
                    "machine": "ARM32",
                    "elf_type": "ET_DYN",
                    "elf_class": "ELF32",
                    "elf_endianness": "little",
                    "size_bytes": 500,
                    "jni_export_hints": ["Java_private_SECRET_NAME"],
                    "bootstrap_exports": ["JNI_OnLoad"],
                    "platform_api_import_evidence": {
                        "android_asset_manager": ["AAssetManager_open", "SECRET_PRIVATE_SYMBOL"],
                        "gles_texture": ["glCompressedTexImage2D"],
                    },
                }],
            },
        }
        recon = {
            "apk_sha256": "SECRETAPKIDENTITY",
            "valid_zip": True,
            "archive_entry_count": 3,
            "group_stats": {"assets": {"count": 2, "declared_bytes": 1000},
                            "SECRET_GROUP": {"count": 1}},
            "magic_stats": {"unrecognized": {"count": 1, "declared_bytes": 100},
                            "SECRET_RESOURCE_MAGIC": {"count": 1}},
            "extension_stats": {
                ".dat": {"count": 4, "declared_bytes": 1000},
                ".png": {"count": 1, "declared_bytes": 10},
                ".SECRET_PRIVATE_FULL_FILENAME": {"count": 2, "declared_bytes": 100},
            },
        }
        report = publish(audit, recon)
        serialized = json.dumps(report)
        for secret in ("SECRETAPKIDENTITY", "SECRETLIBIDENTITY",
                       "SECRET_GAME_ENGINE", "SECRET_NAME", "SECRET_PRIVATE_SYMBOL",
                       "SECRET_GROUP", "SECRET_RESOURCE_MAGIC"):
            self.assertNotIn(secret, serialized)
        lib = report["native_libraries"][0]
        self.assertEqual(lib["machine"], "ARM32")
        self.assertTrue(lib["jni_exports_present"])
        self.assertEqual(
            lib["recognized_public_api_imports"]["android_asset_manager"],
            ["AAssetManager_open"])
        self.assertEqual(report["resources"]["magic_stats"]["unrecognized"]["count"], 1)
        self.assertEqual(report["resources"]["extension_stats"][".dat"]["count"], 4)
        self.assertNotIn(".png", report["resources"]["extension_stats"])
        self.assertNotIn(".SECRET_PRIVATE_FULL_FILENAME", report["resources"]["extension_stats"])

    def test_repository_only_report_contains_no_apk(self):
        report = publish({"apk": None, "repo": {"native_file_paths": ["secret"]}})
        self.assertFalse(report["real_apk_analyzed"])
        self.assertNotIn("secret", json.dumps(report))
        self.assertNotIn("native_libraries", report)


if __name__ == "__main__":
    unittest.main()
