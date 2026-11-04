#!/usr/bin/env bash
# Build and launch Dead Space Mobile native i686 Linux port under WSL2/WSLg.
# Usage: bash scripts/build-run-wsl.sh '/mnt/c/path/to/Deas.Space.Mobile.Fixes.apk'
set -euo pipefail
if (( $# != 1 )); then
  printf 'Usage: bash %s /mnt/c/path/to/Deas.Space.Mobile.Fixes.apk\n' "$0" >&2
  exit 2
fi
apk="$(realpath "$1")"
[[ -f "$apk" ]] || { echo "APK not found: $apk" >&2; exit 2; }
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tools="$root/experimental/lifted_milestone6/tools"
[[ -f "$tools/gen_all.py" ]] || { echo 'Run from the real-baseline branch checkout.' >&2; exit 2; }
[[ "$(uname -m)" == x86_64 ]] || { echo 'Requires x86-64 WSL.' >&2; exit 2; }
[[ -n "${DISPLAY:-}" ]] || { echo 'DISPLAY unset; enable WSLg to show the game window.' >&2; exit 2; }
if ! command -v sudo >/dev/null || ! command -v apt-get >/dev/null; then
  echo 'Requires Ubuntu/Debian WSL.' >&2; exit 2
fi
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  gcc-multilib libc6-dev-i386 python3 python3-venv unzip util-linux binutils \
  libsdl2-2.0-0:i386 libgl1:i386 libgl1-mesa-dri:i386 ca-certificates
build="$HOME/DeadSpaceMobilePC-Linux"
mkdir -p "$build/apk" "$build/logs" "$build/sdl-dev"
if [[ ! -f "$build/apk/.extracted" ]] || ! cmp -s <(sha256sum "$apk" | cut -d' ' -f1) "$build/apk/.extracted"; then
  rm -rf "$build/apk"
  mkdir -p "$build/apk"
  unzip -q "$apk" -d "$build/apk"
  sha256sum "$apk" | cut -d' ' -f1 > "$build/apk/.extracted"
fi
so="$build/apk/lib/armeabi/libDeadSpace.so"
[[ -s "$so" && -s "$build/apk/assets/EAMCore.ini" ]] || {
  echo 'Incompatible APK: expected lib/armeabi/libDeadSpace.so and assets/EAMCore.ini' >&2; exit 2;
}
mkdir -p "$build/apk/assets/var" "$build/apk/assets/published/var"
python3 -m venv "$build/venv"
"$build/venv/bin/pip" install -q pyelftools
# Extract SDL2 i386 headers to avoid multiarch development-package conflicts.
if [[ ! -d "$build/sdl-prefix/usr/include/SDL2" ]]; then
  (cd "$build/sdl-dev" && apt-get download libsdl2-dev:i386 && \
    dpkg-deb -x libsdl2-dev_*_i386.deb "$build/sdl-prefix")
fi
export DS_SO="$so" DS_TEXTUREPACK_DIAG=0
cd "$tools"
"$build/venv/bin/python" gen_all.py > "$build/logs/generation.log" 2>&1
cd out
export DS_BUILD_JOBS="${DS_BUILD_JOBS:-2}"
export SDL_CFLAGS="-I$build/sdl-prefix/usr/include/SDL2 -I$build/sdl-prefix/usr/include/i386-linux-gnu"
export SDL_LIBS=/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0
bash build.sh > "$build/logs/build.log" 2>&1 || {
  tail -n 80 "$build/logs/build.log" >&2; exit 1;
}
cat > "$build/play.sh" <<PLAY
#!/usr/bin/env bash
set -euo pipefail
cd "$tools/out"
mkdir -p "$build/logs"
export DS_DATA="$build/apk/assets"
export DS_ASSETS="$build/apk/assets"
export DS_WINDOW=1 DS_NOVSYNC=1 DS_STEPS=create,surface,frame DS_FRAMES=0
export DS_WATCHDOG=45 DS_QUIET_JAVA=1
# Match the reference agent single-CPU environment for first boot.
cpu=\$(python3 -c "import os; print(min(os.sched_getaffinity(0)))")
set +e
taskset -c "\$cpu" ./dspc_sdl "$so" 2>&1 | tee "$build/logs/game.log"
rc=\${PIPESTATUS[0]}
set -e
if (( rc != 0 )); then
  echo "Native game crashed. Mapping logged PCs to C functions:" >&2
  grep -m 1 "backtrace (use nm" "$build/logs/game.log" | grep -oE "0x[0-9a-fA-F]+" |
    xargs -r addr2line -a -f -C -e "$tools/out/dspc_sdl" > "$build/logs/backtrace.txt" || true
  cat "$build/logs/backtrace.txt" >&2
fi
exit "\$rc"
PLAY
chmod +x "$build/play.sh"
echo "Linux game built. Launching now. Next time: $build/play.sh"
exec "$build/play.sh"
