#!/usr/bin/env python3
import struct,sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as apk:data=apk.read("assets/published/texturepacks_ui/ui_frontend.m3g")
off=21
while off<len(data)-4:
    typ=data[off];n=struct.unpack_from("<I",data,off+1)[0];off+=5
    obj=data[off:off+n];off+=n
    if typ!=10:continue
    p=8+4*struct.unpack_from("<I",obj,4)[0];count=struct.unpack_from("<I",obj,p)[0];p+=4
    for j in range(count):
        key,size=struct.unpack_from("<II",obj,p);p+=8;part=obj[p:p+size];p+=size
        if key!=500:continue
        print("SBIN length",len(part),"header",part[:8].hex())
        pos=8
        chunks={}
        while pos<len(part):
            if pos+12>len(part):
                print("SBIN_TRAIL",pos,len(part)-pos);break
            sig=part[pos:pos+4].decode("ascii","replace")
            length,digest=struct.unpack_from("<II",part,pos+4)
            print("SBIN_CHUNK",sig,"at",pos,"length",length,"digest",hex(digest),
                 "body_first",part[pos+12:pos+12+min(length,32)].hex())
            if length>len(part)-pos-12:break
            chunks[sig]=part[pos+12:pos+12+length]
            pos+=12+length
        for key in ("STRU","FIEL","OHDR","CHDR"):
            if key in chunks:
                b=chunks[key]
                print("SBIN_TABLE",key,"size",len(b),
                      "first_uint16",[struct.unpack_from("<H",b,i)[0] for i in range(0,min(len(b),64)-1,2)])
        if "CHDR" in chunks and "CDAT" in chunks:
            c=chunks["CHDR"];d=chunks["CDAT"]
            for k in range(min(14,len(c)//8)):
                start,n=struct.unpack_from("<II",c,k*8)
                print("SBIN_NAME",k,start,n,repr(d[start:start+n]) if start+n<=len(d) else "<out-of-bounds>")
