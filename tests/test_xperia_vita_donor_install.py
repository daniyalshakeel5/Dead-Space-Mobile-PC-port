import hashlib
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from zipfile import ZipFile
from tools.xperia_vita_donor_install import Donor, DonorError, verify, install

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
