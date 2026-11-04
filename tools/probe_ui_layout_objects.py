#!/usr/bin/env python3
"""Recover typed SBIN entity/vertex object representation, no asset data output."""
import struct,sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as a:b=a.read("assets/published/layoutdata/layouts.xml.sb")
pos=8;blocks={}
while pos+12<=len(b):
    t=b[pos:pos+4]
    if not all(65<=x<=90 for x in t):break
    n=struct.unpack_from("<I",b,pos+4)[0]
    if n>len(b)-pos-12:break
    blocks[t.decode()]=b[pos+12:pos+12+n];pos+=12+n
def u32(data,off):return struct.unpack_from("<I",data,off)[0]
strings=blocks["STRS"];names=[];p=4
for _ in range(u32(strings,0)):
    n=u32(strings,p);p+=4
    names.append(strings[p:p+n].decode("ascii","replace"));p+=n
def lookup(i):return names[i-1] if 1<=i<=len(names) else "?"
f=blocks["FIEL"];struc=blocks["STRU"]
for i in range(0,len(struc),6):
    name,start,count=struct.unpack_from("<HHH",struc,i)
    print("LAYOUT_STRUCTURE",lookup(name),"field_start",start,"count",count)
    for j in range(start,start+count):
        at=j*8
        if at+8>len(f):break
        n,t,offset,aux=struct.unpack_from("<HHHH",f,at)
        print("LAYOUT_FIELD",j,lookup(n),"type",t,"offset",offset,"aux",aux)
hdr=blocks["OHDR"];data=blocks["DATA"]
print("LAYOUT_TOTAL_REFERENCES",len(hdr)//4)
for i in list(range(0,30)) + list(range(100,110)):
    if i>=len(hdr)//4:continue
    enc=u32(hdr,i*4);off=enc//8
    chunk=data[off:off+48]
    print("LAYOUT_OBJECT",i,"encoded",enc,"tag",enc%8,"offset",off,
        "bytes",len(chunk),"hex",chunk.hex(" "),
        "u16",list(struct.unpack("<"+"H"*(len(chunk)//2),chunk[:len(chunk)//2*2]))[:16])
for name in ("UI_MAIN","BTN_PLAY","BTN_OPTIONS","BTN_EXTRAS","UI_PROFILE","BTN_NEWGAME","BTN_CONTINUE"):
    print("LAYOUT_NAMED",name,"string_id",names.index(name)+1 if name in names else None)
