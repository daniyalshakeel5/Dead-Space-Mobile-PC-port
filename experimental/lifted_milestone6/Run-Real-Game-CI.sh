#!/usr/bin/env bash
# Real Dead Space Mobile Linux i686 runtime; NO synthetic test fixtures.
set -euo pipefail
: "${DS_PRIVATE_GAME_APK:?An actual original APK is required}"
: "${DS_REAL_DATA_ZIP:?Actual external deadspace.zip is required}"
: "${GITHUB_WORKSPACE:?GitHub source checkout required}"
: "${RUNNER_TEMP:?Ephemeral build directory required}"
[[ -s "$DS_PRIVATE_GAME_APK" && -s "$DS_REAL_DATA_ZIP" ]] || { echo 'REAL INPUTS MISSING'; exit 40; }
root="$RUNNER_TEMP/dspc-live"
assets="$root/apk/assets"
mkdir -p "$root/apk" "$root/data" "$root/logs"
unzip -qq "$DS_PRIVATE_GAME_APK" -d "$root/apk"
[[ -s "$root/apk/lib/armeabi/libDeadSpace.so" ]] || {
  echo 'REAL FAIL: APK does not contain lib/armeabi/libDeadSpace.so'; exit 41;
}
unzip -qq "$DS_REAL_DATA_ZIP" -d "$root/data"
mapfile -d '' -t roots < <(find "$root/data" -type d -name published -print0)
if (( "${#roots[@]}" > 1 )); then
  echo 'REAL FAIL: ambiguous multiple published asset roots'; exit 42;
fi
if (( "${#roots[@]}" == 1 )); then
  mkdir -p "$assets/published"
  cp -a -n "${roots[0]}/." "$assets/published/"
fi
[[ -d "$assets/published" && -s "$assets/EAMCore.ini" ]] || {
  echo 'REAL FAIL: original resources or EAMCore.ini missing'; exit 43;
}
echo "REAL INPUT: $(find "$assets/published" -type f | wc -l) published resource files"
echo "REAL ELF SHA-256: $(sha256sum "$root/apk/lib/armeabi/libDeadSpace.so" | cut -d' ' -f1)"
src="$GITHUB_WORKSPACE/experimental/lifted_milestone6/tools"
cp -a "$src" "$root/tools"
cd "$root/tools"
export DS_SO="$root/apk/lib/armeabi/libDeadSpace.so"
python3 gen_all.py
cd out
chmod +x build.sh
SDL_CFLAGS="-I/usr/include/SDL2" SDL_LIBS="-lSDL2" ./build.sh > "$root/logs/native-build.log" 2>&1 || {
  echo 'REAL FAIL: i686 compile/link failed'
  tail -n 80 "$root/logs/native-build.log"; exit 44;
}
[[ -s ./dspc_sdl ]] || { echo 'REAL FAIL: no native SDL game executable'; exit 45; }
file ./dspc_sdl
set +e
timeout --kill-after=20s 900s xvfb-run -a env \
  LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe SDL_AUDIODRIVER=dummy \
  DS_WINDOW=1 DS_NOVSYNC=1 DS_ASSET_TRACE=1 \
  DS_DATA="$assets" DS_ASSETS="$assets" \
  DS_STEPS=create,surface,frame DS_FRAMES=900 \
  DS_CLICKS="60:640:360,180:640:360,300:640:360,510:640:360" \
  DS_SHOT="650:$root/logs/native-frame.ppm" \
  ./dspc_sdl "$root/apk/lib/armeabi/libDeadSpace.so" \
  > "$root/logs/native-run.log" 2>&1
code=$?
set -e
echo "REAL GAME EXIT: $code"
if (( code != 0 )); then
  echo 'REAL FAIL: original game native executable crashed, trapped or timed out'
  tail -n 100 "$root/logs/native-run.log"; exit 46;
fi
if ! grep -q '^ran frames' "$root/logs/native-run.log"; then
  echo 'REAL FAIL: game did not complete 900 native frames'
  tail -n 100 "$root/logs/native-run.log"; exit 47;
fi
[[ -s "$root/logs/native-frame.ppm" ]] || {
  echo 'REAL FAIL: game did not produce actual SDL framebuffer output'; exit 48;
}
python3 - "$root/logs/native-frame.ppm" <<'PY'
import sys
from PIL import Image
im=Image.open(sys.argv[1]).convert("RGB").resize((80,45))
colors=len(set(im.getdata()))
print("REAL GAME framebuffer sampled distinct colors:", colors)
if colors < 5:
    raise SystemExit("REAL FAIL: game framebuffer is blank/flat")
PY
echo 'REAL PASS: genuine APK+data, compiled native i686 engine, 900 SDL frames, nonblank frame'
echo 'This is a real runtime smoke test, NOT proof of fully playable gameplay.'
