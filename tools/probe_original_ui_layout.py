#!/usr/bin/env python3
"""Inspect only structural metadata from original compiled UI layouts."""
import collections
import re
import struct
import sys
import zipfile
filename="assets/published/layoutdata/layouts.xml.sb"
with zipfile.ZipFile(sys.argv[1]) as apk:
    data=apk.read(filename)
assert len(data)<=16*1024*1024
print("UI_LAYOUT_FILE",filename,"bytes",len(data),"prefix",data[:96].hex(" "))
print("UI_LAYOUT_DENSITY",sum(32<=c<127 for c in data),len(data))
if data[:4]==b"SBIN":
    print("UI_LAYOUT_SBIN_VERSION",struct.unpack_from("<I",data,4)[0])
    pos=8
    chunks={}
    for _ in range(32):
        if pos+12>len(data):break
        name=data[pos:pos+4]
        if not all(65<=n<=90 for n in name):break
        length,digest=struct.unpack_from("<II",data,pos+4)
        if length>len(data)-pos-12:break
        chunk=data[pos+12:pos+12+length]
        chunks[name.decode()]=chunk
        print("UI_LAYOUT_CHUNK",name.decode(),"offset",pos,"length",length,"hash",hex(digest),
              "prefix",chunk[:36].hex(" "))
        pos+=12+length
    print("UI_LAYOUT_CHUNKS_END",pos,"trailing_bytes",len(data)-pos)
    if "STRS" in chunks:
        b=chunks["STRS"]
        if len(b)>=4:
            count=struct.unpack_from("<I",b,0)[0];p=4;strings=[]
            for i in range(min(count,20000)):
                if p+4>len(b):break
                n=struct.unpack_from("<I",b,p)[0];p+=4
                if n>len(b)-p:break
                strings.append(b[p:p+n].decode("utf8","replace"));p+=n
            print("UI_LAYOUT_STRING_COUNT",count,"decoded",len(strings))
            for i,string in enumerate(strings[:220]):
                print("UI_LAYOUT_STRING",i+1,repr(string[:135]))
            print("UI_LAYOUT_NAME_HITS",
                [s for s in strings if any(k in s.lower() for k in
                    ("button","mainmenu","main_menu","profile","resume","newgame","new_game","continue","load","options","setting","quit","play"))][:120])
    for name in ("STRU","FIEL","OHDR","DATA"):
        if name in chunks:
            b=chunks[name]
            print("UI_LAYOUT_RECORD",name,"bytes",len(b),
                "first_u32",[struct.unpack_from("<I",b,i)[0] for i in
                    range(0,min(len(b)-3,64),4)])
else:
    parts=[(m.start(),m.group().decode("ascii","replace"))
           for m in re.finditer(rb"[\x20-\x7e]{5,120}",data)]
    print("UI_LAYOUT_ASCII_FRAGMENTS",parts[:90])
