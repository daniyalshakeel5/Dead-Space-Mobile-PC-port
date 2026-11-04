#!/usr/bin/env python3
"""Bounded checksum research for the one original scene texture failing JSR-184.
Only structural sizes and integer checksums are printed, no game payloads.
"""
import struct
import sys
import zipfile
import zlib

name="assets/published/textures_uncompressed/texture_env_generic_uv_particle.m3g"
with zipfile.ZipFile(sys.argv[1]) as archive:
    meta=archive.getinfo(name)
    if meta.file_size>8*1024*1024:
        raise SystemExit("texture size budget exceeded")
    data=archive.read(name)
assert data[:12]==bytes([0xab,74,83,82,49,56,52,0xbb,13,10,26,10])
pos=12
for section_index in range(16):
    if pos==len(data):
        print("TEXTURE_CHECKSUM complete sections",section_index)
        break
    if pos+13>len(data):
        raise SystemExit("truncated section header")
    compression,total,plain=struct.unpack_from("<BII",data,pos)
    if total<13 or total>len(data)-pos or plain>8*1024*1024:
        raise SystemExit(f"section {section_index} invalid length")
    raw=data[pos:pos+total-4]
    expected=struct.unpack_from("<I",data,pos+total-4)[0]
    actual_adler=zlib.adler32(raw)&0xffffffff
    actual_crc=zlib.crc32(raw)&0xffffffff
    body=data[pos+9:pos+total-4]
    print("TEXTURE_CHECKSUM section",section_index,
        "offset",pos,"compression",compression,"total",total,
        "declared_uncompressed",plain,"stored",len(body),
        "expected",hex(expected),
        "adler32",hex(actual_adler),"crc32",hex(actual_crc),
        "match_adler",expected==actual_adler,
        "match_crc",expected==actual_crc,
        "body_adler",hex(zlib.adler32(body)&0xffffffff),
        "body_crc",hex(zlib.crc32(body)&0xffffffff))
    pos+=total
else:
    raise SystemExit("too many texture sections")
assert pos==len(data)
