import hashlib
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from zipfile import ZipFile
from tools.xperia_vita_donor_install import Donor, DonorError, verify, install
from tools.xperia_reanimated_hd_install import HdSpec

class DonorTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)
        self.zip=self.path/"test.zip"
        self.parts={
          "deadspace/lib/armeabi/libEAMGameDeadSpace.so":b"owned synthetic ARM",
          "deadspace/assets/EAMCore.ini":b"fake config",
          "deadspace/assets/published/data/level_index.sb":b"fake level"
        }
        self.create()
    def create(self):
        with ZipFile(self.zip,"w") as z:
            for name,data in self.parts.items():z.writestr(name,data)
        self.spec=Donor(self.zip.stat().st_size,
            hashlib.sha256(self.zip.read_bytes()).hexdigest(),2,10,
            tuple((name.removeprefix("deadspace/"),hashlib.sha256(data).hexdigest())
                  for name,data in self.parts.items()))
    def test_extract_and_preserve_existing(self):
        self.assertEqual(verify(self.zip,self.spec)["asset_count"],2)
        target=self.path/"installed"
        install(self.zip,target,self.spec)
        self.assertEqual((target/"assets/EAMCore.ini").read_bytes(),b"fake config")
        with self.assertRaisesRegex(DonorError,"exists"):
            install(self.zip,target,self.spec)
        self.assertEqual((target/"assets/EAMCore.ini").read_bytes(),b"fake config")
    def test_two_archive_install_keeps_original_and_hd_separate(self):
        # Test an original-free donor + original-free mock Reanimated APK.
        hd=self.path/"reanimated.apk"
        with ZipFile(hd,"w") as zipfile:
            zipfile.writestr("assets/texture_hd/menus/logo.png",b"synthetic-HD-logo")
            zipfile.writestr("assets/texture_hd/world/wall.png",b"synthetic-HD-wall")
            # Different-edition game binaries must NEVER leak into
            # our installed Xperia guest image.
            zipfile.writestr("assets/game/libDeadSpace32.so",b"wrong-edition-ARM")
            zipfile.writestr("assets/game/assets/published/maps/map.sb",b"wrong-edition-map")
        hd_spec=HdSpec(hd.stat().st_size,
            (hashlib.sha256(hd.read_bytes()).hexdigest(),),
            min_textures=2,max_textures=2)
        installed=self.path/"enhanced"
        info=install(self.zip,installed,self.spec,
                     reanimated_apk=hd,hd_spec=hd_spec)
        self.assertEqual(info["reanimated_hd"]["resource_count"],2)
        self.assertFalse(info["reanimated_hd"]["enabled_in_runtime"])
        self.assertTrue((installed/"lib/armeabi/libEAMGameDeadSpace.so").exists())
        self.assertEqual((installed/"assets/EAMCore.ini").read_bytes(),b"fake config")
        self.assertEqual((installed/"enhancements/reanimated/texture_hd/world/wall.png").read_bytes(),
                         b"synthetic-HD-wall")
        self.assertFalse((installed/"assets/game").exists())
        self.assertFalse((installed/"enhancements/reanimated/texture_hd/game").exists())
        bad_hd_spec=HdSpec(hd_spec.size,("0"*64,),2,2)
        with self.assertRaisesRegex(ValueError,"SHA256"):
            install(self.zip,self.path/"failed",self.spec,
                    reanimated_apk=hd,hd_spec=bad_hd_spec)
        self.assertFalse((self.path/"failed").exists())

    def test_corruption_denied(self):
        bad=replace(self.spec,sha256="0"*64)
        with self.assertRaisesRegex(DonorError,"SHA256"):install(self.zip,self.path/"bad",bad)
        self.assertFalse((self.path/"bad").exists())
    def test_path_traversal_denied(self):
        self.parts["deadspace/assets/../escape"]=b"evil"
        self.create()
        with self.assertRaisesRegex(DonorError,"traversal"):verify(self.zip,self.spec)
    def test_case_collision_denied(self):
        self.parts["deadspace/assets/eamcore.ini"]=b"evil"
        self.create()
        with self.assertRaisesRegex(DonorError,"case-colliding"):verify(self.zip,self.spec)

if __name__=="__main__":unittest.main()
