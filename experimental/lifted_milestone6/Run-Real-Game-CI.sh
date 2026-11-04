#!/usr/bin/env bash
# Real Dead Space Mobile Linux i686 runtime; NO synthetic test fixtures.
set -euo pipefail
: "${DS_PRIVATE_GAME_APK:?Authentic original game APK required; no synthetic fallback}"
: "${GITHUB_WORKSPACE:?GitHub source checkout required}"
: "${RUNNER_TEMP:?Ephemeral build directory required}"
[[ -s "$DS_PRIVATE_GAME_APK" ]] || { echo 'REAL APK INPUT MISSING'; exit 40; }
root="$RUNNER_TEMP/dspc-live"
assets="$root/apk/assets"
mkdir -p "$root/apk" "$root/data" "$root/logs"
unzip -qq "$DS_PRIVATE_GAME_APK" -d "$root/apk"
[[ -s "$root/apk/lib/armeabi/libDeadSpace.so" ]] || {
  echo 'REAL FAIL: APK does not contain lib/armeabi/libDeadSpace.so'; exit 41;
}
if [[ -n "${DS_REAL_DATA_ZIP:-}" ]]; then
  [[ -s "$DS_REAL_DATA_ZIP" ]] || { echo 'REAL FAIL: external game data path missing'; exit 42; }
  unzip -qq "$DS_REAL_DATA_ZIP" -d "$root/data"
  mapfile -d '' -t roots < <(find "$root/data" -type d -name published -print0)
  if (( "${#roots[@]}" > 1 )); then
    echo 'REAL FAIL: ambiguous multiple published asset roots'; exit 42;
  fi
  if (( "${#roots[@]}" == 1 )); then
    mkdir -p "$assets/published"
    cp -a -n "${roots[0]}/." "$assets/published/"
  fi
fi
[[ -d "$assets/published" && -s "$assets/EAMCore.ini" ]] || {
  echo 'REAL FAIL: original resources or EAMCore.ini missing'; exit 43;
}
echo "REAL APK ASSETS: $(find "$assets/published" -type f | wc -l) published resource files"; find "$assets/published" -mindepth 1 -maxdepth 1 -printf 'asset-subtree: %f\n' | head -n 30
echo "REAL ELF SHA-256: $(sha256sum "$root/apk/lib/armeabi/libDeadSpace.so" | cut -d' ' -f1)"
src="$GITHUB_WORKSPACE/experimental/lifted_milestone6/tools"
cp -a "$src" "$root/tools"
cd "$root/tools"
export DS_SO="$root/apk/lib/armeabi/libDeadSpace.so"
python3 gen_all.py
# Observe the exact original M3G vector before the proven null dereference.
# This is derived from the live APK; it never suppresses a loader error or
# substitutes an Object3D for a missing node.
python3 modelcache_node_probe.py --source out/fn_003.c --instrument
python3 object3d_duplicate_probe.py --source out/fn_060.c --instrument
cd out
chmod +x build.sh
# Use three independent GCC workers on GitHub's four-core Linux runner;
# leave end-user WSL/local builds at the conservative one-worker default.
if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then export DS_BUILD_JOBS="${DS_BUILD_JOBS:-3}"; fi
SDL_CFLAGS="-I/usr/include/SDL2" SDL_LIBS="-lSDL2" ./build.sh > "$root/logs/native-build.log" 2>&1 || {
  echo 'REAL FAIL: i686 compile/link failed'
  tail -n 80 "$root/logs/native-build.log"; exit 44;
}
[[ -s ./dspc_sdl ]] || { echo 'REAL FAIL: no native SDL game executable'; exit 45; }
file ./dspc_sdl
# Exercise the exact native Android/JNI stream implementation with a disposable
# 8-byte input; then separately launch the actual copyrighted APK and assets.
# Test data is only a JNI unit fixture, never a substitute for game content.
if ! DS_JNI_STREAM_SELFTEST=1 ./dspc_sdl; then
  echo 'REAL FAIL: native JNI stream ownership/EOF regression'; exit 44
fi
test_frames="${DS_REAL_TEST_FRAMES:-900}"
[[ "$test_frames" =~ ^[0-9]+$ && "$test_frames" -ge 60 && "$test_frames" -le 900 ]] || {
  echo 'REAL FAIL: DS_REAL_TEST_FRAMES must be 60..900'; exit 45;
}
if (( test_frames > 650 )); then shot_at=650; else shot_at=$((test_frames-10)); fi
runtime_budget=360
if (( test_frames > 300 )); then runtime_budget=900; fi
echo "REAL NATIVE TEST: frame target=$test_frames screenshot_at=$shot_at"
set +e
timeout --kill-after=20s "${runtime_budget}s" xvfb-run -a env \
  LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe SDL_AUDIODRIVER=dummy \
  DS_WINDOW=1 DS_NOVSYNC=1 DS_ASSET_TRACE=1 DS_WATCHDOG=45 \
  DS_DATA="$assets" DS_ASSETS="$assets" \
  DS_STEPS=create,surface,frame DS_FRAMES="$test_frames" \
  DS_CLICKS="60:640:360,180:640:360,300:640:360,510:640:360" \
  DS_SHOT="$shot_at:$root/logs/native-frame.ppm" \
  ./dspc_sdl "$root/apk/lib/armeabi/libDeadSpace.so" \
  > "$root/logs/native-run.log" 2>&1
code=$?
set -e
echo "REAL GAME EXIT: $code"
if (( code != 0 )); then
  # Symbolize the ACTUAL crash address from this run's debug-enabled i686
  # binary; no test fixture, guessed symbol, or separate download required.
  echo '===== REAL NATIVE CRASH: symbolized execution addresses ====='
  mapfile -t fault_pcs < <(sed -n 's/.*signal 11 at pc=\(0x[0-9a-fA-F]*\).*/\1/p' "$root/logs/native-run.log" | sort -u)
  mapfile -t backtrace_pcs < <(grep -m 1 'backtrace (use nm' "$root/logs/native-run.log" | grep -oE '0x[0-9a-fA-F]+' || true)
  if (( ${#fault_pcs[@]} == 0 )); then
    echo 'No SIGSEGV PC found; inspect the last runtime lines below.'
  else
    for pc in "${fault_pcs[@]}"; do
      echo "Actual fault PC: $pc"
      addr2line -e ./dspc_sdl -a -f -C -i "$pc" || true
      # Use addr2line's actual DWARF function name and line. On Ubuntu,
      # default mawk lacks strtonum(), so do not depend on an awk fallback.
    done
    for pc in "${backtrace_pcs[@]}"; do
      addr2line -e ./dspc_sdl -a -f -C "$pc" || true
    done
  fi
  # Resolve true native caller addresses emitted by Object3D::duplicate.
  # These come from the genuine i686 game executable, not guessed ARM labels.
  mapfile -t dup_callers < <(grep -oE 'native_return_pc=0x[0-9a-fA-F]+' "$root/logs/native-run.log" | sed 's/native_return_pc=//' | sort -u || true)
  for caller in "${dup_callers[@]}"; do
    echo "Actual Object3D duplicate caller: $caller"
    addr2line -e ./dspc_sdl -a -f -C -i "$caller" || true
  done
  echo '===== ORIGINAL MODEL LOADER / JAVA STREAM FAILURE EVENTS ====='
  grep -E 'REAL_MODEL_|REAL_OBJECT3D_|JAVA_STREAM_PARTIAL|JAVA_STREAM_SKIP_CLAMP|JNI_STREAM_SELFTEST' "$root/logs/native-run.log" | tail -n 80 || true
  echo '===== LAST REAL RESOURCE/M3G/OBJECT LOADING EVENTS ====='
  grep -E 'M3G_LOADER|TEXPACK_|FATAL:|GUEST_RESOURCE_(MISS|OK)|AssetManager.openFd|AssetManager.open\('     "$root/logs/native-run.log" | tail -n 65 || true
  echo '===== END ACTUAL RUNTIME SYMBOLS ====='
  echo 'REAL FAIL: original game native executable crashed, trapped or timed out'
  tail -n 100 "$root/logs/native-run.log"; exit 46;
fi
if ! grep -q '^ran frames' "$root/logs/native-run.log"; then
  echo "REAL FAIL: game did not complete $test_frames native frames"
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
echo "REAL PASS: original APK, native i686 engine, $test_frames SDL frames, nonblank frame"
echo 'This is a real runtime smoke test, NOT proof of fully playable gameplay.'
