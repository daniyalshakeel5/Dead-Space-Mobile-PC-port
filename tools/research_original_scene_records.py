#!/usr/bin/env python3
"""Ephemeral original menu IM-M3G structural scanner. No game bytes persisted."""
from pathlib import Path
import struct,sys,collections
data=Path(sys.argv[1]).read_bytes()
assert data[:12]==b'\xabIM-M3G\xbb\r\n\x1a\n'
assert struct.unpack_from('<I',data,13)[0]==len(data)-12
end=len(data)-4; pos=21;obj=[]
while pos<end:
    typ=data[pos];size=struct.unpack_from('<I',data,pos+1)[0];pos+=5
    assert pos+size<=end
    obj.append((typ,data[pos:pos+size]));pos+=size
assert pos==end
by_type=collections.Counter(t for t,_ in obj)
print('types',sorted(by_type.items()))
# Examine leading common prefixes of camera/group/mesh/node/appearance,
# all hex displayed ONLY as 32-bit values with no texture/vertex bytes.
for typ in (5,9,14,3,19,1,2):
    records=[(i+1,b) for i,(t,b) in enumerate(obj) if t==typ]
    print('type',typ,'count',len(records),'lengths',sorted(collections.Counter(len(b) for _,b in records).items()))
    for ident,b in records[:min(4,len(records))]:
        print('  id',ident,'first_uint32s',[struct.unpack_from('<I',b,k)[0] for k in range(0,min(len(b)-3,56),4)])
# For group objects, seek repeating contiguous child IDs at end of data
# print compact reference-type sequences, not the original byte values.
for ident,b in [(i+1,b) for i,(t,b) in enumerate(obj) if t==9][:8]:
    cand=[]
    for offset in range(0,len(b)-3):
        val=struct.unpack_from('<I',b,offset)[0]
        if val and val<=len(obj) and obj[val-1][0] in (5,9,14):
            cand.append((offset,obj[val-1][0],val))
    print('group_candidates',ident,cand[:30])
for ident,b in [(i+1,b) for i,(t,b) in enumerate(obj) if t==5][:1]:
    print('camera_payload_length',len(b),'camera_candidate_float_fields',
          [(offset,round(struct.unpack_from('<f',b,offset)[0],5))
           for offset in range(0,len(b)-3,4)
           if abs(struct.unpack_from('<f',b,offset)[0])>1e-7 and abs(struct.unpack_from('<f',b,offset)[0])<1e7])
