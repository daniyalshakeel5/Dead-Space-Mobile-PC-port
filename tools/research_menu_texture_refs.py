#!/usr/bin/env python3
"""Inspect original M3G user-parameter texture labels (no game pixels exported)."""
import pathlib,struct,sys
def objects(path):
    data=pathlib.Path(path).read_bytes()
    assert data[:12]==b'\\xabIM-M3G\\xbb\\r\\n\\x1a\\n'
    pos=21;end=len(data)-4;result=[]
    while pos<end:
        typ=data[pos];n=struct.unpack_from('<I',data,pos+1)[0];pos+=5
        assert pos+n<=end
        result.append((typ,data[pos:pos+n]))
        pos+=n
    assert pos==end
    return result
def meta(payload):
    pos=0
    if len(payload)<12:return None
    user,tracks=struct.unpack_from('<II',payload,pos);pos+=8
    if tracks>256 or tracks*4>len(payload)-pos:return None
    pos+=tracks*4
    if len(payload)-pos<4:return None
    n=struct.unpack_from('<I',payload,pos)[0];pos+=4
    if n>64:return None
    result=[]
    for i in range(n):
        if len(payload)-pos<8:return None
        key,length=struct.unpack_from('<II',payload,pos);pos+=8
        if length>len(payload)-pos:return None
        raw=payload[pos:pos+length];pos+=length
        trimmed=raw.rstrip(b'\\x00')
        if len(trimmed)<=100 and all(32<=b<=126 for b in trimmed):
            label=trimmed.decode('ascii')
        else:label='<binary>'
        result.append((key,length,label))
    return user,result
for path in sys.argv[1:]:
    items=objects(path)
    print('ORIGINAL_M3G_OBJECT_LABELS',pathlib.Path(path).name,'count',len(items))
    for i,(typ,payload) in enumerate(items,1):
        if typ not in (3,10,14,17,19,21):continue
        extracted=meta(payload)
        if extracted is None:continue
        userid,params=extracted
        if params or userid:
            print('M3G_NAMED',i,'type',typ,'user',userid,'params',params[:4])
    print('M3G_LABEL_CHECK_DONE')
