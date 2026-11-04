import hashlib
import unittest
from tools.deadspace_vita_abi_compare import (
    VITA_LIBRARY_NAME, VITA_REQUIRED_JNI, summarize)

class VitaResearchComparisonTests(unittest.TestCase):
    def test_reports_nonidentical_build_without_rejecting_symbols(self):
        sample=b"fixture only, not proprietary binary"
        result=summarize("lib/armeabi-v7a/libGame.so",sample,
                         {VITA_REQUIRED_JNI[0],VITA_REQUIRED_JNI[3]})
        self.assertFalse(result["identical_vita_game_binary"])
        self.assertFalse(result["library_sha1_matches_vita"])
        self.assertEqual(result["required_vita_jni_count"],len(VITA_REQUIRED_JNI))
        self.assertEqual(len(result["matching_jni_entrypoints"]),2)
        self.assertEqual(len(result["missing_jni_entrypoints"]),
                         len(VITA_REQUIRED_JNI)-2)

    def test_name_match_is_not_binary_equivalence(self):
        r=summarize(VITA_LIBRARY_NAME,b"unrelated",set(VITA_REQUIRED_JNI))
        self.assertTrue(r["library_member_matches_vita"])
        self.assertFalse(r["library_sha1_matches_vita"])
        self.assertFalse(r["identical_vita_game_binary"])
        self.assertEqual(len(r["missing_jni_entrypoints"]),0)

if __name__=="__main__":
    unittest.main()
