#!/usr/bin/env python3
import struct,sys,zipfile,re
with zipfile.ZipFile(sys.argv[1]) as z:
    raw=z.read("assets/published/texturepacks_ui/ui_frontend.m3g")
p=21;end=len(raw)-4
while p<end:
    t=raw[p];n=struct.unpack_from("<I",raw,p+1)[0];p+=5
    b=raw[p:p+n];p+=n
    if t!=10:continue
    at=8+struct.unpack_from("<I",b,4)[0]*4
    count=struct.unpack_from("<I",b,at)[0];at+=4
    for _ in range(min(count,32)):
        key,size=struct.unpack_from("<II",b,at);at+=8
        value=b[at:at+size];at+=size
        if key!=500:continue
        print("UI_LAYOUT_BYTES",len(value),"first64",value[:64].hex(" "))
        for term in (b"content_scale",b"source_width",b"source_rect",b"framework/endscrn_bg.tif",b"framework_mainmenu/achmnt_ring_outer.tif"):
            start=value.find(term)
            if start>=0:print("UI_LAYOUT_STRING",term.decode(),"offset",start,
                "before",value[max(0,start-26):start].hex(" "),
                "after",value[start+len(term):start+len(term)+45].hex(" "))
        print("UI_LAYOUT_CHAR_DENSITY",sum(32<=v<127 for v in value),len(value))
        strings=[(m.start(),m.group().decode("ascii","ignore")) for m in re.finditer(rb"[\x20-\x7e]{5,100}",value)]
        print("UI_LAYOUT_FIRST_STRINGS",strings[:50])
        print("UI_LAYOUT_LAST_STRINGS",strings[-12:])
