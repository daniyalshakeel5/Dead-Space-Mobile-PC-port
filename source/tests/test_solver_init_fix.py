import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'experimental/lifted_milestone6/tools'
spec=importlib.util.spec_from_file_location('solver_init_fix',TOOLS/'solver_init_fix.py')
fix=importlib.util.module_from_spec(spec);spec.loader.exec_module(fix)

class SolverFixTest(unittest.TestCase):
    def test_scope_and_poisoned_row(self):
        # Synthetic caller using the exact generated call convention. The
        # repair must clear all 136 bytes and preserve adjacent allocations.
        body=fix.FUNCTION+'u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) { '+fix.CALL+' return r0; }'
        other='\nu64 unrelated(u32 r0,u32 r1,u32 r2,u32 r3,u32 sp) { '+fix.CALL+' return r0; }'
        patched=fix.patch_text(body+other)
        self.assertEqual(patched.count('SOLVER_ROW_ZERO_FIX'),1)
        self.assertIn(other,patched)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp); c=p/'test.c'; binary=p/'test'
            c.write_text('''#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uintptr_t u32; typedef uint64_t u64;
static u64 imp_memset(u32 p,u32 v,u32 n,u32 r3,u32 sp){memset((void*)p,v,n);return p;}
static u64 f_0028b4e4(u32 p,u32 v,u32 n,u32 r3,u32 sp){return (uintptr_t)memchr((void*)p,v,n);}
'''+patched+'''
int main(void){unsigned char data[152];memset(data,0xa5,sizeof data);
f_00262fe8((uintptr_t)(data+8),0,136,0,0);
for(int i=0;i<152;i++)assert(data[i]==(i>=8&&i<144?0:0xa5));
memset(data,0xa5,sizeof data);unrelated((uintptr_t)data,0,sizeof data,0,0);
for(int i=0;i<152;i++)assert(data[i]==0xa5);return 0;}
''')
            subprocess.run(['cc','-O1',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    def test_rejects_changed_or_repeated_site(self):
        for text in ['',fix.FUNCTION+'...',fix.FUNCTION+fix.CALL+fix.CALL]:
            with self.assertRaises(ValueError):fix.patch_text(text)
        once=fix.patch_text(fix.FUNCTION+fix.CALL)
        with self.assertRaises(ValueError):fix.patch_text(once)

if __name__=='__main__':unittest.main()
