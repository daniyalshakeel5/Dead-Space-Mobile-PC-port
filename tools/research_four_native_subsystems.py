#!/usr/bin/env python3
"""Four bounded, independent original-APK workstreams. No asset bytes exported."""
import collections
import pathlib
import re
import struct
import sys
import zipfile
import tempfile
import subprocess

apk=pathlib.Path(sys.argv[1])
mode=sys.argv[2]
assert mode in ("transforms","materials","animation","input_audio")
with zipfile.ZipFile(apk) as archive:
    names=archive.namelist()
    def scene(name):
        content=archive.read("assets/"+name)
        assert content[:12]==b"\xabIM-M3G\xbb\r\n\x1a\n"
        pos=21;end=len(content)-4;items=[]
        while pos<end:
            typ=content[pos];size=struct.unpack_from("<I",content,pos+1)[0];pos+=5
            assert pos+size<=end
            items.append((typ,content[pos:pos+size]))
            pos+=size
        assert pos==end
        return items
    items=scene("published/models/main_menu.m3g")
    def ref(id):
        return items[id-1][0] if 0<id<=len(items) else None
    if mode=="transforms":
        print("TRANSFORMS_CAMERA original camera records",sum(t==5 for t,_ in items))
        nodes=[(i+1,p) for i,(t,p) in enumerate(items) if t in (5,9,14)]
        print("TRANSFORMS_CAMERA node types",dict(collections.Counter(items[i-1][0] for i,_ in nodes)))
        # Scan positions of common byte prefixes across all node records,
        # not float guesses. Variable-length object names alter offsets.
        for typ in (5,9,14):
            ls=[len(p) for t,p in items if t==typ]
            print("TRANSFORMS_CAMERA type",typ,"minlen",min(ls),"maxlen",max(ls))
        animated=collections.Counter()
        for node_id,blob in nodes:
            for offset in range(len(blob)-3):
                id_=struct.unpack_from("<I",blob,offset)[0]
                if ref(id_) in (1,2,17):
                    animated[(items[node_id-1][0],offset,ref(id_))]+=1
        print("TRANSFORMS_CAMERA possible animation reference offsets",
              animated.most_common(20))
        print("TRANSFORMS_CAMERA matrix semantics UNVERIFIED")
    elif mode=="materials":
        appearances=[(i+1,p) for i,(t,p) in enumerate(items) if t==3]
        textures=[i+1 for i,(t,_) in enumerate(items) if t==19]
        images=[i+1 for i,(t,_) in enumerate(items) if t==10]
        print("MATERIALS appearances",len(appearances),"texture_objects",len(textures),"image_objects",len(images))
        # Distinguish plausible references from validated mappings.
        # For each source object use *unaligned* little-endian references,
        # then report how often a single offset maps to a target type.
        for typ in (3,19):
            candidates=collections.Counter()
            concrete=collections.defaultdict(list)
            for i,(t,p) in enumerate(items,1):
                if t!=typ:continue
                for k in range(len(p)-3):
                    target=struct.unpack_from("<I",p,k)[0]
                    targettype=ref(target)
                    if targettype in (3,10,19):
                        candidates[(k,targettype)]+=1
                        concrete[(k,targettype)].append((i,target))
            for key,count in candidates.most_common(20):
                print("MATERIALS candidate offset/type",key,"hits",count,
                      "distinct_sources",len(set(x for x,_ in concrete[key])),
                      "distinct_targets",len(set(x for _,x in concrete[key])))
        print("MATERIALS original bindings UNVERIFIED")
    elif mode=="animation":
        print("ANIMATION tracks",sum(t==2 for t,_ in items),"controllers",sum(t==1 for t,_ in items),"transform_data",sum(t==17 for t,_ in items))
        for typ in (1,2,17):
            counts=collections.Counter(len(p) for t,p in items if t==typ)
            print("ANIMATION type",typ,"lengths",sorted(counts.items())[:25])
        for typ in (1,2,17):
            frequency=collections.Counter()
            for t,p in items:
                if t!=typ:continue
                for offset in range(len(p)-3):
                    target=struct.unpack_from("<I",p,offset)[0]
                    targettype=ref(target)
                    if targettype in (1,2,17):
                        frequency[(offset,targettype)]+=1
            print("ANIMATION potential reference offsets type",typ,
                  frequency.most_common(20))
        print("ANIMATION playback equations UNVERIFIED")
    else:
        library_names=[n for n in names if n.startswith("lib/") and n.endswith(".so")]
        print("INPUT_AUDIO native ELF count",len(library_names),"ABI list",sorted(set(n.split("/")[1] for n in library_names)))
        # Only probe presence of known engine entrypoint symbol strings,
        # never dump original ELF or asset content to public logs.
        needles=(b"JNI_OnLoad",b"NativeOnPointerEvent",b"NativeOnKeyDown",
            b"NativeOnKeyUp",b"NativeOnDrawFrame",b"AndroidEAAudioCore_Init",
            b"AndroidEAAudioCore_Release")
        for path in library_names:
            if "armeabi-v7a" not in path and "armeabi/" not in path:continue
            blob=archive.read(path)
            print("INPUT_AUDIO ELF",path,"size",len(blob),
                "symbol-string-presence",{x.decode():x in blob for x in needles})
            # Inspect the original ARM dynamic symbol table; unlike raw
            # string matching this distinguishes exported callable symbols.
            with tempfile.TemporaryDirectory() as temp:
                original=pathlib.Path(temp)/"original.so"
                original.write_bytes(blob)
                result=subprocess.run(["readelf","--dyn-syms","--wide",str(original)],
                    capture_output=True,text=True,timeout=15,check=False)
                if result.returncode:
                    print("INPUT_AUDIO dynsym readelf unavailable",result.returncode)
                else:
                    exports=[]
                    for line in result.stdout.splitlines():
                        parts=line.split()
                        if len(parts)>=8 and parts[3]=="FUNC" and parts[6]!="UND":
                            exports.append(parts[-1])
                    jni=[name for name in exports if
                         any(key in name for key in
                             ("JNI_OnLoad","Java_com_ea","Audio","Input","NativeOn"))]
                    print("INPUT_AUDIO exported callable count",len(exports))
                    print("INPUT_AUDIO relevant native export names",jni[:100])
        print("INPUT_AUDIO native callback behavior UNVERIFIED")
