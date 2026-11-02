import hashlib
import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile
from tools.xperia_reanimated_hd_install import (
    HdAssetError,HdSpec,inspect,install
)

class ReanimatedHdTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.apk=self.root/"reanimated.apk"
        self.files={
            "assets/texture_hd/corridor/wall.png":b"synthetic hd one",
            "assets/texture_hd/menu_title.png":b"synthetic hd two",
            # Original game's other edition must never be installed:
            "assets/game/assets/published/models/main_menu.m3g":b"wrong-version original model",
            "lib/arm64-v8a/libdsr.so":b"Android compatibility layer",
        }
        self.make_apk()
    def make_apk(self):
        with ZipFile(self.apk,"w") as a:
            for name,blob in self.files.items():
                a.writestr(name,blob)
        self.spec=HdSpec(self.apk.stat().st_size,
            (hashlib.sha256(self.apk.read_bytes()).hexdigest(),),
            min_textures=2,max_textures=2)
    def test_extract_only_hd_files_atomically(self):
        result=inspect(self.apk,self.spec)
        self.assertEqual(result["hd_file_count"],2)
        self.assertFalse(result["mounted_in_game"])
        destination=self.root/"enhancements"/"reanimated"
        installed=install(self.apk,destination,self.spec)
        self.assertEqual(installed,result)
        self.assertEqual((destination/"texture_hd"/"menu_title.png").read_bytes(),
                         b"synthetic hd two")
        self.assertFalse((destination/"assets").exists())
        self.assertFalse((destination/"lib").exists())
        self.assertTrue((destination/"hd-layer.json").exists())
        with self.assertRaisesRegex(HdAssetError,"exists"):
            install(self.apk,destination,self.spec)
    def test_wrong_apk_does_not_install(self):
        spec=HdSpec(self.spec.size,("a"*64,),2,2)
        with self.assertRaisesRegex(HdAssetError,"SHA256"):
            install(self.apk,self.root/"bad",spec)
        self.assertFalse((self.root/"bad").exists())
    def test_zip_traversal_rejected(self):
        self.files["assets/texture_hd/../escape.png"]=b"escape"
        self.make_apk()
        with self.assertRaisesRegex(HdAssetError,"traversal"):
            inspect(self.apk,HdSpec(self.spec.size,self.spec.accepted_hashes,2,3))
    def test_case_collisions_rejected(self):
        self.files["assets/texture_hd/MENU_TITLE.png"]=b"case duplicate"
        self.make_apk()
        with self.assertRaisesRegex(HdAssetError,"Duplicate"):
            inspect(self.apk,HdSpec(self.spec.size,self.spec.accepted_hashes,2,3))
    def test_reject_wrong_count(self):
        with self.assertRaisesRegex(HdAssetError,"count"):
            inspect(self.apk,HdSpec(self.spec.size,self.spec.accepted_hashes,5,10))

if __name__=="__main__":
    unittest.main()
