#!/usr/bin/env python3
"""Original APK object formats: bounded text-only animation/UI metadata report."""
import collections,struct,sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as apk:
    def objects(asset):
        buf=apk.read("assets/"+asset)
        if len(buf)>32*1024*1024 or buf[:12]!=bytes([171,73,77,45,77,51,71,187,13,10,26,10]):
            raise RuntimeError("not original menu M3G")
        end=len(buf)-4; p=21;result=[]
        while p<end:
            if end-p<5:raise RuntimeError("truncated object")
            t=buf[p];n=struct.unpack_from("<I",buf,p+1)[0];p+=5
            if n>end-p:raise RuntimeError("invalid size")
            result.append((t,buf[p:p+n]));p+=n
        if p!=end:raise RuntimeError("object stream size")
        return result
    m=objects("published/models/main_menu.m3g")
    ui=objects("published/texturepacks_ui/ui_frontend.m3g")
    print("MENU_ANIMATION object_types",sorted(collections.Counter(t for t,_ in m).items()))
    def header(b):
        if len(b)<12:raise ValueError
        count=struct.unpack_from("<I",b,4)[0];at=8+4*count
        if count>512 or at+4>len(b):raise ValueError
        params=struct.unpack_from("<I",b,at)[0];at+=4
        if params>128:raise ValueError
        for _ in range(params):
            if at+8>len(b):raise ValueError
            _,sz=struct.unpack_from("<II",b,at);at+=8+sz
            if at>len(b):raise ValueError
        return at
    for i,(t,b) in enumerate(m,1):
        if t not in (1,2,19):continue
        try:
            at=header(b);tail=b[at:]
            print("MENU_ANIMATION",i,"type",t,"size",len(b),"body",len(tail),
                  "prefix_words",[struct.unpack_from("<I",tail,j)[0]
                  for j in range(0,min(len(tail)-3,24),4)],
                  "prefix_bytes",list(tail[:8]))
        except ValueError:print("MENU_ANIMATION",i,"INVALID_HEADER")
    print("MENU_UI_TYPES",sorted(collections.Counter(t for t,_ in ui).items()))
    for i,(t,b) in enumerate(ui,1):
        try:
            if t!=10:continue
            at=8+struct.unpack_from("<I",b,4)[0]*4
            if at+4>len(b):continue
            count=struct.unpack_from("<I",b,at)[0];at+=4
            for _ in range(min(count,32)):
                if at+8>len(b):break
                id_,sz=struct.unpack_from("<II",b,at);at+=8
                value=b[at:at+sz];at+=sz
                printable=bytes(c if 32<=c<127 else 32 for c in value)
                segments=[q[:60].decode("ascii","ignore") for q in printable.split(b" ")
                          if len(q)>=10][:10]
                print("MENU_UI_PARAM",i,id_,"bytes",sz,"printable_fragments",segments)
        except (ValueError,IndexError):continue
