#!/usr/bin/env python3
"""Exercise the SDL audio sink without any original game assets."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def flags(variable, query):
    if variable in os.environ:
        return shlex.split(os.environ[variable])
    return shlex.split(subprocess.check_output(['pkg-config', query, 'sdl2'], text=True))

with tempfile.TemporaryDirectory() as temporary:
    binary = str(Path(temporary) / 'test_audio_sdl')
    subprocess.run([os.environ.get('CC', 'cc'), *shlex.split(os.environ.get('CFLAGS', '')),
                    '-O1', '-DDS_SDL', '-pthread',
                    '-I' + str(root / 'experimental/lifted_milestone6/tools'),
                    *flags('SDL_CFLAGS', '--cflags'), str(root / 'tests/test_audio_sdl.c'),
                    *flags('SDL_LIBS', '--libs'), '-o', binary], check=True)
    subprocess.run([binary], check=True, timeout=10)
