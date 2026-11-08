import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[2]

class ReleasePackageTest(unittest.TestCase):
    def test_allowlist_checksums_modes_and_metadata(self):
        with tempfile.TemporaryDirectory(prefix='ds release ') as temporary:
            tmp=Path(temporary);binary=tmp/'dspc_sdl'
            header=bytearray(20);header[:6]=b'\x7fELF\x01\x01';struct.pack_into('<H',header,18,3)
            binary.write_bytes(header)
            (tmp/'private-save.sb').write_text('must not be packaged')
            output=tmp/'release'
            subprocess.run([sys.executable,str(ROOT/'source/scripts/package-linux-release.py'),
                '--binary',str(binary),'--output',str(output),'--version','fixture',
                '--runtime-commit','HEAD'],check=True,capture_output=True)
            archive=output/'DeadSpaceMobile-Linux-i686-fixture.zip'
            with zipfile.ZipFile(archive) as z:
                names={Path(n).name:n for n in z.namelist()}
                self.assertNotIn('private-save.sb',names)
                self.assertIn('START-HERE.md',names)
                self.assertIn('DeadSpaceMobile-WSL.ps1',names)
                self.assertFalse(any('/game/' in n for n in z.namelist()))
                for line in z.read(names['SHA256SUMS.txt']).decode().splitlines():
                    digest,name=line.split('  ',1)
                    self.assertEqual(hashlib.sha256(z.read(names[name])).hexdigest(),digest)
                for name in ['dspc_sdl','Play Dead Space Mobile.sh','Play-Dead-Space-Touch.sh']:
                    self.assertEqual((z.getinfo(names[name]).external_attr>>16)&0o777,0o755)
                self.assertFalse(json.loads(z.read(names['BUILD_INFO.txt']))['game_assets_included'])
            expected=(output/(archive.name+'.sha256')).read_text().split()[0]
            self.assertEqual(hashlib.sha256(archive.read_bytes()).hexdigest(),expected)

    def test_linux_launcher_preserves_arguments_and_exit_status(self):
        with tempfile.TemporaryDirectory(prefix='ds launcher ') as temporary:
            tmp=Path(temporary)
            alias=tmp/'Play Dead Space Mobile.sh'
            shutil.copyfile(ROOT/'source/launcher/Play Dead Space Mobile.sh',alias)
            (tmp/'Play-Dead-Space-Touch.sh').write_text('#!/bin/bash\nprintf "%s\\0" "$@"\nexit 23\n')
            args=['--windowed','/some path/$(not-a-command).apk']
            result=subprocess.run(['bash',str(alias),*args],capture_output=True)
            self.assertEqual(result.returncode,23)
            self.assertEqual(result.stdout,b'\0'.join(x.encode() for x in args)+b'\0')

if __name__=='__main__':unittest.main()
