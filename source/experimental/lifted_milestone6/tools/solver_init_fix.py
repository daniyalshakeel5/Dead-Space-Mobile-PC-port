"""Repair the original donor's solver-row initialization call, fail closed.

The verified solveGroupCacheFriendlySetup call at 0x26325c passes a row,
zero, and 136 bytes to EA::StdC::Memchr. It needs memset: searching leaves
stale constraint data. Only this call site is changed, never global Memchr.
"""
from pathlib import Path
import hashlib
import struct

DONOR_SHA1 = '64a3323cd3cc0c483a645b62af8508624163ba9b'
FUNCTION = 'u64 f_00262fe8('
CALL = '{ u64 rv = f_0028b4e4(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); }'
FIX = '{ u64 rv = imp_memset(r0, r1, r2, r3, sp); r0 = (u32)rv; r1 = (u32)(rv>>32); } /* SOLVER_ROW_ZERO_FIX */'

def patch_text(text):
    start = text.find(FUNCTION)
    if start < 0 or text.count(FUNCTION) != 1:
        raise ValueError('Expected exactly one verified solver function')
    end = text.find('\nu64 ', start + len(FUNCTION))
    if end < 0:
        end = len(text)
    body = text[start:end]
    if body.count(CALL) != 1 or 'SOLVER_ROW_ZERO_FIX' in body:
        raise ValueError('Solver initialization call differs from verified donor')
    body = body.replace(CALL, FIX)
    return text[:start] + body + text[end:]

def patch(out: Path, so: Path):
    if hashlib.sha1(so.read_bytes()).hexdigest() != DONOR_SHA1:
        raise ValueError('Solver fix requires the verified original donor')
    from elftools.elf.elffile import ELFFile
    with so.open('rb') as stream:
        elf = ELFFile(stream)
        section = elf.get_section_by_name('.text')
        offset = 0x263250 - section['sh_addr']
        words = struct.unpack_from('<4I', section.data(), offset)
        # mov r0,r6; mov r1,#0; mov r2,#136; bl EA::StdC::Memchr
        expected_bl = 0xeb000000 | (((0x28b4e4 - (0x26325c + 8)) // 4) & 0xffffff)
        if words != (0xe1a00006, 0xe3a01000, 0xe3a02088, expected_bl):
            raise ValueError('Solver ARM instructions differ from verified call site')
    files = [p for p in out.glob('fn_*.c') if FUNCTION in p.read_text()]
    if len(files) != 1:
        raise ValueError('Expected one generated solver function')
    p = files[0]
    p.write_text(patch_text(p.read_text()))
    return 'SOLVER_ROW_ZERO_FIX: 0x26325c Memchr -> memset (136-byte constraint row)'
