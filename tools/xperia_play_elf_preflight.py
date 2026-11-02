#!/usr/bin/env python3
"""Offline bounded Xperia Play ARM ELF preflight. Never executes game code."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

SHA1="0ed42b611415015807f759ec9b5457857143ce39"
EXPORTS=(
 "JNI_OnLoad",
 "Java_com_ea_blast_MainActivity_NativeOnCreate",
 "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
 "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
 "Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent",
 "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown",
 "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp",
 "Java_com_ea_blast_AccelerometerAndroidDelegate_NativeOnAcceleration",
)
RELOCS={2:"R_ARM_ABS32",21:"R_ARM_GLOB_DAT",22:"R_ARM_JUMP_SLOT",23:"R_ARM_RELATIVE"}
class ElfError(ValueError):pass

def parse(raw):
    if not 52<=len(raw)<=64*1024*1024 or raw[:7]!=b"\x7fELF\x01\x01\x01":
        raise ElfError("Not a little-endian ELF32 library")
    u16=lambda n:struct.unpack_from("<H",raw,n)[0]
    u32=lambda n:struct.unpack_from("<I",raw,n)[0]
    if (u16(16),u16(18))!=(3,40):
        raise ElfError("Need ARM ET_DYN; not an x86 or different-version library")
    phoff,phents,phnum=u32(28),u16(42),u16(44)
    if phents!=32 or not 0<phnum<=128 or phoff+phents*phnum>len(raw):
        raise ElfError("Invalid ARM program header table")
    spans=[];dyn=[]
    for i in range(phnum):
        kind,off,addr,_,filesz,memsz,flags,_=struct.unpack_from("<8I",raw,phoff+32*i)
        if off+filesz>len(raw):
            raise ElfError("Truncated ELF segment")
        if kind==1:
            if filesz>memsz or addr+memsz>2**32:
                raise ElfError("Invalid loadable segment")
            spans.append((addr,addr+filesz,off,memsz,flags))
        if kind==2:
            if filesz>64*1024 or filesz%8:
                raise ElfError("Invalid dynamic table")
            for p in range(off,off+filesz,8):
                key,val=struct.unpack_from("<II",raw,p)
                if key==0:break
                dyn.append((key,val))
    if not spans or not dyn:raise ElfError("Missing PT_LOAD or PT_DYNAMIC")
    sorted_spans=sorted(spans)
    for a,b in zip(sorted_spans,sorted_spans[1:]):
        if a[0]+a[3]>b[0]:raise ElfError("Overlapping guest segments")
    def mapped(addr,size):
        if addr+size>2**32:raise ElfError("Overflow in guest address range")
        for start,end,off,_,_ in spans:
            if start<=addr and addr+size<=end:
                return off+addr-start
        raise ElfError("ELF virtual address outside file-backed PT_LOAD")
    tags={}
    for key,val in dyn:tags.setdefault(key,[]).append(val)
    def get(key,default=None):
        val=tags.get(key,[])
        if not val:
            if default is None:raise ElfError("Missing required ELF dynamic field")
            return default
        if len(set(val))!=1:raise ElfError("Inconsistent ELF dynamic entries")
        return val[0]
    if get(11)!=16:raise ElfError("Unexpected ARM Elf32_Sym width")
    hashaddr=mapped(get(4),8)
    buckets,symbol_count=struct.unpack_from("<II",raw,hashaddr)
    if symbol_count<=0 or symbol_count>200000 or buckets>200000:
        raise ElfError("Excessive symbol count")
    syms=mapped(get(6),symbol_count*16)
    strsz=get(10)
    if not 0<strsz<=32*1024*1024:raise ElfError("Invalid ELF string table size")
    strs=mapped(get(5),strsz)
    def string(offset):
        if not 0<=offset<strsz:raise ElfError("Dynamic string index out of bounds")
        at=strs+offset
        end=raw.find(b"\0",at,min(strs+strsz,at+8192))
        if end<0:raise ElfError("Unterminated ELF string")
        return raw[at:end].decode("utf-8","replace")
    def symbol(i):
        if i>=symbol_count:raise ElfError("Bad relocation symbol reference")
        at=syms+i*16
        return string(struct.unpack_from("<I",raw,at)[0]),u32(at+4),u16(at+14)
    exports={}
    for i in range(symbol_count):
        name,addr,section=symbol(i)
        if section and name in EXPORTS:exports[name]=f"0x{addr:08x}"
    spec=[(get(17),get(18),get(19))]
    if get(2,0):
        if get(20)!=17:raise ElfError("Unexpected PLT relocation encoding")
        spec.append((get(23),get(2),8))
    counts=Counter();imports=set();unknown=Counter();targets=set()
    for addr,size,entry in spec:
        if entry!=8 or size%8 or size>800000:raise ElfError("Bad ARM REL table")
        at=mapped(addr,size)
        for i in range(size//8):
            target,info=struct.unpack_from("<II",raw,at+8*i)
            mapped(target,4)
            if target in targets:raise ElfError("Duplicate relocation target")
            targets.add(target)
            typ,sym=info&255,info>>8
            counts[RELOCS.get(typ,f"unknown_{typ}")]+=1
            if typ not in RELOCS:unknown[typ]+=1
            if sym:
                name,_,section=symbol(sym)
                if not section and name:imports.add(name)
    needed=[string(offset) for offset in tags.get(1,[])]
    return {
        "format":"ELF32 ARM ET_DYN","elf_entry":f"0x{u32(24):08x}",
        "needed":needed,"jnis":exports,
        "missing_jni":sorted(set(EXPORTS)-set(exports)),
        "relocations":dict(sorted(counts.items())),
        "unsupported_relocations":dict(sorted(unknown.items())),
        "relocation_total":len(targets),
        "unresolved_import_count":len(imports),
        "unresolved_imports":sorted(imports),
        "loadable_segments":len(spans),
        "init_array_size":get(27,0),
        "has_text_relocations":26 in tags
    }

def inspect(path):
    if path.stat().st_size>64*1024*1024:raise ElfError("Oversized ELF")
    raw=path.read_bytes()
    result=parse(raw)
    result["sha1"]=hashlib.sha1(raw).hexdigest()
    result["sha256"]=hashlib.sha256(raw).hexdigest()
    result["exact_vita_xperia_1_1_33"]=result["sha1"]==SHA1
    return result

def main():
    arg=argparse.ArgumentParser()
    arg.add_argument("--so",type=Path,required=True)
    arg.add_argument("--json",type=Path)
    a=arg.parse_args()
    try:report=inspect(a.so)
    except (OSError,ValueError,struct.error) as err:
        print("XPERIA_PRECHECK_FAILED:",err)
        return 2
    if a.json:a.json.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print(json.dumps({k:v for k,v in report.items()
                      if k!="unresolved_imports"},indent=2,sort_keys=True))
    return 0 if (report["exact_vita_xperia_1_1_33"]
                 and not report["missing_jni"]
                 and not report["unsupported_relocations"]) else 2

if __name__=="__main__":
    raise SystemExit(main())
