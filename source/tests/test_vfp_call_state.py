"""Guest VFP argument, callee-save, FPSCR and thread-isolation regression.

Uses tiny synthetic ARM functions, not proprietary game code or assets.
Run with Python 3 + pyelftools, a C compiler and pthreads on Linux.
"""
import pathlib
import subprocess
import sys
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / 'experimental/lifted_milestone6/tools'
sys.path.insert(0, str(TOOLS))
from lift import FuncLifter


class SyntheticImage:
    plt = {}
    override = {}
    names_at = {}
    # Caller: push {lr}; vmov s0,r0; bl leaf; pop {pc}.
    # Leaf reads the incoming s0 directly, as the game's btRayAabb2 clone does.
    words = {
        0x1000: [0xe52de004, 0xee000a10, 0xeb00000c, 0xe49df004],
        0x1040: [0xee100a10, 0xe12fff1e],
        # Preserve d8 while changing s16: vpush, vmov, vpop, return.
        0x1080: [0xed2d8b02, 0xee081a10, 0xecbd8b02, 0xe12fff1e],
        0x10c0: [0xeef10a10, 0xe12fff1e],  # vmrs r0,FPSCR; bx lr
    }
    funcs = {a: (len(ws) * 4, 'synthetic') for a, ws in words.items()}

    def w(self, address):
        for start, words in self.words.items():
            if start <= address < start + 4 * len(words):
                return words[(address - start) // 4]
        raise KeyError(address)


class VfpStateTest(unittest.TestCase):
    def test_cross_translation_unit_calls_and_threads(self):
        with tempfile.TemporaryDirectory() as directory:
            out = pathlib.Path(directory)
            declarations = '#include "rt.h"\nu64 f_00001040(u32,u32,u32,u32,u32);\n'
            files = []
            for address in SyntheticImage.funcs:
                path = out / f'{address:x}.c'
                path.write_text(declarations + FuncLifter(SyntheticImage(), address).lift())
                files.append(str(path))
            main = out / 'main.c'
            main.write_text(r'''
#define _GNU_SOURCE
#include "rt.h"
#include <pthread.h>
#include <sys/mman.h>
#include <assert.h>
u32 g_base;
__thread u32 ds_arm_vfp[32];
__thread u32 ds_arm_fpscr;
u64 f_00001000(u32,u32,u32,u32,u32);
u64 f_00001040(u32,u32,u32,u32,u32);
u64 f_00001080(u32,u32,u32,u32,u32);
u64 f_000010c0(u32,u32,u32,u32,u32);
void trap_unsupported(u32 a,const char* s){abort();}
u64 call_ind(u32 a,u32 b,u32 c,u32 d,u32 e,u32 f){abort();}
static pthread_barrier_t barrier;
static void* worker(void* arg) {
    u32 value=(u32)(uintptr_t)arg;
    ds_arm_vfp[0]=value;
    ds_arm_fpscr=value;
    pthread_barrier_wait(&barrier);
    assert((u32)f_00001040(0,0,0,0,0)==value);
    assert((u32)f_000010c0(0,0,0,0,0)==value);
    return NULL;
}
int main(void) {
    void* stack=mmap(NULL,65536,PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(stack!=MAP_FAILED && (uintptr_t)stack < 0xffff0000u);
    u32 sp=(u32)(uintptr_t)stack+65000;
    u32 values[]={0,0x42480000,0x3f800000,0xc2480000,0x7f800000};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i)
        assert((u32)f_00001000(values[i],0,0,0,sp)==values[i]);
    ds_arm_vfp[16]=0x12345678;ds_arm_vfp[17]=0x87654321;
    f_00001080(0,0xffffffff,0,0,sp);
    assert(ds_arm_vfp[16]==0x12345678 && ds_arm_vfp[17]==0x87654321);
    pthread_t a,b;pthread_barrier_init(&barrier,NULL,2);
    pthread_create(&a,NULL,worker,(void*)0x10000000);
    pthread_create(&b,NULL,worker,(void*)0x60000000);
    pthread_join(a,NULL);pthread_join(b,NULL);
    munmap(stack,65536);
    puts("PASS VFP arguments, callee-saved registers, FPSCR and thread isolation");
}
''')
            binary = out / 'vfp-test'
            subprocess.run(['gcc', '-O1', '-ffp-contract=off', '-I'+str(TOOLS),
                            *files, str(main), '-lm', '-pthread', '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
