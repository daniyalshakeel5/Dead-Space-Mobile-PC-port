"""Observe original model parser and buffered seeks; never alter decoding."""
from pathlib import Path

WRAP = r'''
#include "protos.h"
#include <stdio.h>
static void ds_stream_state(const char* event,u32 b,u32 arg,u32 mode) {
    /* Layout verified from the original BufferedInputStream ARM routines. */
    u32 base=LD32(b+16), cursor=LD32(b+32), count=LD32(b+36), origin=LD32(b+40);
    char bytes[49]={0};
    if (base && cursor<=count) {
        u32 n=count-cursor; if(n>16)n=16;
        for(u32 i=0;i<n;i++) snprintf(bytes+i*3,4,"%02x ",LD8(base+cursor+i));
    }
    fprintf(stderr,"[STREAM_AUDIT] %s self=%08x arg=%d mode=%u origin=%u cursor=%u count=%u position=%u bytes=%s\n",
            event,b,(s32)arg,mode,origin,cursor,count,origin+cursor,bytes);
}
'''
def instrument(directory):
    targets = {
        '0032a4c4': ('object', 'u32 b=LD32(r1+4);', 'b'),
        '00347c28': ('seek', 'u32 b=r0;', 'b'),
    }
    for address,(event,setup,b) in targets.items():
        signature=f'u64 f_{address}(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {{'
        files=[p for p in Path(directory).glob('fn_*.c') if signature in p.read_text()]
        if len(files)!=1: raise RuntimeError(f'Expected original function {address}')
        p=files[0];s=p.read_text()
        original=f'ds_original_{address}'
        s=s.replace(signature,signature.replace(f'f_{address}',original),1)
        if '#define DS_STREAM_POSITION_AUDIT' not in s:
            s='#define DS_STREAM_POSITION_AUDIT\n'+WRAP+s
        s+=f'''\nu64 f_{address}(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) {{
    {setup}
    ds_stream_state("{event}-before",b,r1,r2);
    u64 result={original}(r0,r1,r2,r3,sp);
    ds_stream_state("{event}-after",b,(u32)result,r2);
    return result;
}}\n'''
        p.write_text(s)
