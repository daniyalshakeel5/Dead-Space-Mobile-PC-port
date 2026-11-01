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

# Find potential *trailing* child tables: group payloads contain variable
# length object names so reference offsets need not be 4-byte aligned.
# Check a count followed by exactly count valid node/mesh IDs at end;
# print only (group object ID, start offset, count, target types).
def trailing_tables(typ):
    results=[]
    for obj_id,(t,blob) in enumerate(obj,1):
        if t!=typ:continue
        matches=[]
        for offset in range(0,len(blob)-3):
            count=struct.unpack_from('<I',blob,offset)[0]
            if count==0 or count>128 or offset+4+4*count!=len(blob):
                continue
            ids=struct.unpack_from('<'+'I'*count,blob,offset+4)
            if all(0<id_<=len(obj) and obj[id_-1][0] in (5,9,14) for id_ in ids):
                matches.append((offset,count,tuple(obj[id_-1][0] for id_ in ids)))
        results.append((obj_id,len(blob),matches))
    return results
for typ in (9,5):
    found=trailing_tables(typ)
    print('trailing_reference_table_type',typ,'records',len(found))
    print('  matched',sum(bool(x[2]) for x in found))
    for item in found[:15]:print('  tail_record',item)
# Sliding-window reference frequencies for each named node; count and
# type pattern only. This avoids assuming payload byte alignment.
for typ in (9,5):
    nodes=[(i+1,b) for i,(t,b) in enumerate(obj) if t==typ]
    counts=collections.Counter()
    for ident,b in nodes:
        for off in range(0,len(b)-3):
            id_=struct.unpack_from('<I',b,off)[0]
            if id_ and id_<=len(obj) and obj[id_-1][0] in (5,9,14):
                counts[(off,obj[id_-1][0])]+=1
    print('unaligned_candidate_freq_type',typ,counts.most_common(25))
