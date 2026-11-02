import unittest
import struct
from tools.xperia_play_elf_preflight import ElfError, parse, SHA1, EXPORTS

def fixture():
    b=bytearray(1024)
    b[0:7]=b"\x7fELF\x01\x01\x01"
    struct.pack_into("<HHIIIIIHHHHHH",b,16,3,40,1,0x1000,52,0,
                     0x05000002,52,32,2,40,0,0)
    struct.pack_into("<8I",b,52,1,0,0x1000,0,1024,1024,5,4096)
    struct.pack_into("<8I",b,84,2,0x100,0x1100,0,80,80,4,4)
    tags=[(4,0x1200),(6,0x1220),(5,0x1240),(10,32),(11,16),
          (17,0x1280),(18,0),(19,8),(2,0),(0,0)]
    for i,(k,v) in enumerate(tags):
        struct.pack_into("<II",b,0x100+8*i,k,v)
    struct.pack_into("<II",b,0x200,1,1)
    b[0x240:0x245]=b"\0hi\0"
    return b

class XperiaTests(unittest.TestCase):
    def test_minimal_elf(self):
        r=parse(bytes(fixture()))
        self.assertEqual(r["format"],"ELF32 ARM ET_DYN")
        self.assertEqual(r["relocation_total"],0)
        self.assertEqual(len(r["missing_jni"]),len(EXPORTS))
    def test_wrong_machine(self):
        b=fixture();struct.pack_into("<H",b,18,62)
        with self.assertRaises(ElfError):parse(bytes(b))
    def test_header_truncation(self):
        b=fixture();struct.pack_into("<I",b,28,1022)
        with self.assertRaises(ElfError):parse(bytes(b))
    def test_rel_size_validation(self):
        b=fixture();struct.pack_into("<I",b,0x100+7*8+4,7)
        with self.assertRaises(ElfError):parse(bytes(b))
    def test_no_data_leak(self):
        self.assertEqual(len(SHA1),40)
        self.assertNotIn("libEAMGameDeadSpace.so",str(parse(bytes(fixture()))))
if __name__=="__main__":unittest.main()
