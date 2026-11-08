"""Test IIO discovery, physical units, mount matrix, enable/rate and read errors."""
import os
import pathlib
import subprocess
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / 'experimental/lifted_milestone6/tools'


class LinuxAccelTest(unittest.TestCase):
    def test_iio_backend(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for i, label in enumerate(('accel-base', 'accel-display')):
                device = root / f'iio:device{i}'
                device.mkdir()
                for name, value in {
                    'label': label, 'scale': '0.5',
                    'in_accel_x_raw': '2', 'in_accel_y_raw': '-4', 'in_accel_z_raw': '6',
                    'in_accel_mount_matrix': '-1,0,0;0,-1,0;0,0,-1',
                }.items():
                    (device / name).write_text(value + '\n')
            code = root / 'test.c'
            code.write_text(r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <assert.h>
#include <unistd.h>
#define DS_SDL 1
#include "linux_accel.h"
int main(void) {
    float xyz[3]={99,99,99},m[9];
    assert(ds_accel_init());
    assert(strstr(ds_accel_path,"iio:device1"));
    assert(!ds_accel_poll(100,xyz) && xyz[0]==99);
    ds_accel_set_enabled(1);ds_accel_set_delay(1);
    assert(ds_accel_poll(100,xyz));
    assert(xyz[0]==-1 && xyz[1]==2 && xyz[2]==-3);
    assert(!ds_accel_poll(119,xyz));
    assert(ds_accel_poll(120,xyz));
    ds_accel_set_delay(3);
    assert(!ds_accel_poll(319,xyz));
    assert(ds_accel_poll(320,xyz));
    ds_accel_set_enabled(0);assert(!ds_accel_poll(1000,xyz));
    assert(!ds_accel_parse_matrix("1,0,0;1,0,0;0,0,1",m));
    assert(!ds_accel_parse_matrix("nan,0,0;0,1,0;0,0,1",m));
    assert(ds_accel_parse_matrix("0,-1,0;1,0,0;0,0,1",m));
    char path[4096];snprintf(path,sizeof path,"%s/in_accel_y_raw",ds_accel_path);
    assert(!unlink(path));ds_accel_set_enabled(1);
    assert(!ds_accel_poll(1500,xyz));
    puts("PASS IIO selection, scale, mounting, enable/rate and read failure");
}
''')
            binary = root / 'test'
            subprocess.run(['gcc', '-O1', '-I'+str(TOOLS), '-DDS_IIO_ROOT="'+str(root)+'"',
                            str(code), '-lm', '-o', str(binary)], check=True)
            environment = {k: v for k, v in os.environ.items() if not k.startswith('DS_ACCEL_')}
            subprocess.run([str(binary)], env=environment, check=True)


if __name__ == '__main__':
    unittest.main()
