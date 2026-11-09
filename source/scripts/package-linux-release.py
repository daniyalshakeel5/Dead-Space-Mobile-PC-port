#!/usr/bin/env python3
"""Package a verified native executable using an explicit asset-free file list."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

REPO = Path(__file__).resolve().parents[2]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--binary', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--version', required=True)
    ap.add_argument('--runtime-commit', required=True,
                    help='Source revision of the compiled game runtime')
    args = ap.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.version):
        ap.error('Version must be a plain filename component')
    with args.binary.open('rb') as stream:
        header = stream.read(20)
    if len(header) != 20 or header[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', header, 18)[0] != 3:
        ap.error('Expected a little-endian ELF32 x86 executable')
    revision = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip()
    runtime = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', args.runtime_commit+'^{commit}'], text=True).strip()
    args.output.mkdir(parents=True, exist_ok=True)
    name = 'DeadSpaceMobile-Linux-i686-' + args.version
    final = args.output / name
    archive = args.output / (name + '.zip')
    if final.exists() or archive.exists():
        ap.error('Release already exists; use a new output directory/version')
    files = {
        'Play-Dead-Space-Touch.sh': 'source/packaging/linux-touchscreen/Play-Dead-Space-Touch.sh',
        'START-HERE.md': 'source/packaging/linux-touchscreen/START-HERE.md',
        'README-LINUX-TOUCH.txt': 'source/packaging/linux-touchscreen/README-LINUX-TOUCH.txt',
        'THIRD_PARTY_ASSETSTUDIO_LICENSE.txt': 'source/packaging/linux-touchscreen/THIRD_PARTY_ASSETSTUDIO_LICENSE.txt',
        'Play Dead Space Mobile.sh': 'source/launcher/Play Dead Space Mobile.sh',
        'Play Dead Space Mobile.cmd': 'source/launcher/Play Dead Space Mobile.cmd',
        'DeadSpaceMobile-WSL.ps1': 'source/launcher/DeadSpaceMobile-WSL.ps1',
        'TECHNICAL_BUILD_GUIDE.md': 'docs/TECHNICAL_BUILD_GUIDE.md',
    }
    with tempfile.TemporaryDirectory(prefix='.package-', dir=args.output) as temporary:
        staged = Path(temporary)/name
        staged.mkdir()
        shutil.copyfile(args.binary, staged/'dspc_sdl')
        for target, source in files.items():
            shutil.copyfile(REPO/source, staged/target)
        for path in staged.iterdir():
            path.chmod(0o755 if path.suffix=='.sh' or path.name=='dspc_sdl' else 0o644)
        guide = staged/'TECHNICAL_BUILD_GUIDE.md'
        guide.write_text(guide.read_text().replace('(../source/README.md)',
            '(https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/blob/'+revision+'/source/README.md)'))
        metadata = {'version': args.version, 'package_source_commit': revision,
                    'runtime_source_commit': runtime, 'binary_sha256': digest(args.binary),
                    'architecture': 'ELF32 i686 Linux', 'minimum_glibc': '2.38',
                    'minimum_sdl2': '2.0.18',
                    'donor_elf_sha1': '64a3323cd3cc0c483a645b62af8508624163ba9b',
                    'game_assets_included': False}
        (staged/'BUILD_INFO.txt').write_text(json.dumps(metadata, indent=2)+'\n')
        notes = f'''# Dead Space Mobile Linux {args.version}

Prebuilt native i686 touchscreen release. Read **START-HERE.md** before launching.

- Matching drawable/touch coordinates; 1366x768 default request.
- Original-game audio output and Linux IIO accelerometer bridge.
- Raycast/VFP correction; hit detection and opening skybox user-confirmed.
- Reanimated v1.3 KTX HD textures, extracted from the user's APK.
- Writable XDG saves, legacy migration and corrected asset namespace.
- Guarded correction of the original Bullet solver initialization call.
- Linux launcher plus the repo's Windows launcher wired to this bundled build via WSLg.

User confirmed persistent saves and sharp textures. The known solver correction
is included; confirmation of the previously affected combat scene is pending.
Native Linux was tested on Debian 13. The optional Windows/WSLg launcher has
not been end-to-end tested on Windows hardware. A later motion-gated level has
not been completed as part of validation. This does not claim every additional
Reanimated gameplay/audio feature.

Requires a supported Fixes APK or DeadSpaceReanimated-1.3.apk. Neither game data
nor the HD pack is bundled. Requires x86 Linux, glibc >=2.38, 32-bit SDL2 >=2.0.18
and 32-bit OpenGL drivers. This is not a native Windows or ARM executable.

Package source: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/commit/{revision}
Runtime source: https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/commit/{runtime}
'''
        (staged/'RELEASE-NOTES.md').write_text(notes)
        (staged/'SHA256SUMS.txt').write_text(''.join(digest(p)+'  '+p.name+'\n'
            for p in sorted(staged.iterdir()) if p.is_file()))
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for path in sorted(staged.iterdir()):
                z.write(path, name+'/'+path.name)
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:
                raise RuntimeError('ZIP integrity verification failed')
            assert {Path(n).name for n in z.namelist()} == {p.name for p in staged.iterdir()}
        staged.rename(final)
    (args.output/(archive.name+'.sha256')).write_text(digest(archive)+'  '+archive.name+'\n')
    shutil.copyfile(final/'RELEASE-NOTES.md', args.output/'RELEASE-NOTES.md')
    (args.output/'PUBLISHING.md').write_text(f'''# Ready for a GitHub release

Prepared release: `{args.version}`. No public release has been created.

1. On GitHub, choose **Draft a new release** in Dead-Space-Mobile-PC-port.
2. Create tag `linux-{args.version}` targeting commit `{revision}` on
   `work/deadspace-linux-touchscreen-2026-11-07`.
3. Use `RELEASE-NOTES.md` as the release description.
4. Upload `{archive.name}` and `{archive.name}.sha256` from this folder.
5. Review and publish when ready. The ZIP includes both launchers and all setup instructions.

Do not upload your installed `game/` folder, APKs, HD pack or saves. GitHub's
automatic source-code archives do not contain the playable executable.
''')
    print(json.dumps({'folder':str(final),'zip':str(archive),'sha256':digest(archive)},indent=2))

if __name__ == '__main__':
    main()
