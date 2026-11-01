"""Pure synthetic ARM PLT fixtures; never require original APK contents."""
import struct
import unittest
from tools.deadspace_plt_import import arm_immediate,resolve_arm_plt_import

def elf_fixture():
    raw=bytearray(1024)
    raw[:6]=b"\x7fELF\x01\x01"
    struct.pack_into("<I",raw,32,0x200)
    struct.pack_into("<HH",raw,46,40,5)
    def section(index,kind,flags,va,offset,length,link=0,step=0):
        struct.pack_into("<IIIIIIIIII",raw,0x200+index*40,
                         0,kind,flags,va,offset,length,link,0,4,step)
    section(1,1,4,0x1000,0x100,12)
    section(2,9,0,0,0x120,8,3,8)
    section(3,11,0,0,0x140,32,4,16)
    section(4,3,0,0,0x180,22)
    # add ip,pc,#0 ; add ip,ip,#0 ; ldr pc,[ip,#0]!
    struct.pack_into("<III",raw,0x100,0xe28fc000,0xe28cc000,0xe5bcf000)
    struct.pack_into("<II",raw,0x120,0x1008,(1<<8)|22)
    struct.pack_into("<IIIBBH",raw,0x150,1,0,0,0,0,0)
    symbol=b"\x00__aeabi_memclr4\x00"
    raw[0x180:0x180+len(symbol)]=symbol
    return raw

class ArmPltImportTests(unittest.TestCase):
    def test_arm_immediate_rotation(self):
        self.assertEqual(arm_immediate(4|(6<<8)),0x00400000)
        self.assertEqual(arm_immediate(0x23),0x23)
    def test_symbol_resolution(self):
        resolved=resolve_arm_plt_import(bytes(elf_fixture()),0x1000)
        self.assertEqual(resolved["import_name"],"__aeabi_memclr4")
        self.assertEqual(resolved["got_va"],"0x00001008")
    def test_mismatched_relocation_fails_closed(self):
        data=elf_fixture()
        struct.pack_into("<I",data,0x120,0x3000)
        with self.assertRaises(ValueError):
            resolve_arm_plt_import(bytes(data),0x1000)
    def test_mismatched_plt_code_fails_closed(self):
        data=elf_fixture()
        struct.pack_into("<I",data,0x100,0xe1a00000)
        with self.assertRaises(ValueError):
            resolve_arm_plt_import(bytes(data),0x1000)

if __name__=="__main__":unittest.main()
