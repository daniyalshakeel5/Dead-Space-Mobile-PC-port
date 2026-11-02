#!/usr/bin/env python3
import struct,sys,zipfile,collections
with zipfile.ZipFile(sys.argv[1]) as apk:b=apk.read("assets/published/texturepacks_ui/ui_frontend.m3g")
at=21
while at<len(b)-4:
    typ=b[at];n=struct.unpack_from("<I",b,at+1)[0];at+=5
    part=b[at:at+n];at+=n
    if typ!=10:continue
    p=8+4*struct.unpack_from("<I",part,4)[0]
    count=struct.unpack_from("<I",part,p)[0];p+=4
    for _ in range(count):
        k,n=struct.unpack_from("<II",part,p);p+=8
        x=part[p:p+n];p+=n
        if k!=500:continue
        chunks={};pos=8
        for _ in range(8):
            if pos+12>len(x):break
            typ=x[pos:pos+4];sz=struct.unpack_from("<I",x,pos+4)[0]
            if sz>len(x)-pos-12:break
            chunks[typ.decode("ascii")]=x[pos+12:pos+12+sz];pos+=12+sz
        ohdr=chunks["OHDR"];data=chunks["DATA"];strings=chunks["STRS"]
        print("SBIN_END",pos,"TAIL",x[pos:].hex())
        cnt=struct.unpack_from("<I",strings,0)[0]
        ix=4;names=[]
        for _ in range(cnt):
            if ix+4>len(strings):break
            size=struct.unpack_from("<I",strings,ix)[0];ix+=4
            if size>len(strings)-ix:break
            names.append(strings[ix:ix+size].decode("utf8","replace"));ix+=size
        print("SBIN_STRINGS",cnt,len(names),"first20",names[:20])
        print("SBIN_FIELDS",[(struct.unpack_from("<4H",chunks["FIEL"],i), names[struct.unpack_from("<H",chunks["FIEL"],i)[0]-1] if 0<struct.unpack_from("<H",chunks["FIEL"],i)[0]<=len(names) else "?") for i in range(0,len(chunks["FIEL"]),8)])
        pointers=[v[0] for v in struct.iter_unpack("<I",ohdr)]
        print("SBIN_OHDR",len(pointers),"first40",pointers[:40],"last20",pointers[-20:])
        print("SBIN_DATA_first128",data[:128].hex(" "))
        for method in ("direct","div8","mul8"):
            found=[]
            for i,ptr in enumerate(pointers):
                off=ptr if method=="direct" else (ptr//8 if method=="div8" else ptr*8)
                if off<0 or off+16>len(data):continue
                name,tex,scale,w,h,src,dst=struct.unpack_from("<HBBHHII",data,off)
                if name>0 and name<=len(names) and w>0 and h>0 and w<=2048 and h<=2048 and src<len(pointers) and dst<len(pointers):
                    found.append((i,off,names[name-1],tex,scale,w,h,src,dst))
            print("SBIN_IMAGE_CANDIDATES",method,len(found),"first15",found[:15])

        for object_id in range(2,12):
            rawptr=pointers[object_id]
            off=rawptr//8
            print("SBIN_SPRITE_OBJECT",object_id,"offset",off,"remainder",rawptr%8,
                "raw",data[off:off+28].hex(" "))
            if off+20<=len(data):
                print("SBIN_SPRITE_FIELDS",object_id,
                      "at0",struct.unpack_from("<HBBHHII",data,off),
                      "at4",struct.unpack_from("<HBBHHII",data,off+4))
