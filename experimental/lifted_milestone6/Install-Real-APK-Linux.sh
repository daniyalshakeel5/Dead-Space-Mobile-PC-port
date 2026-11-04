#!/usr/bin/env bash
# Fresh native Linux/i686 install built directly from the user's real APK.
# No synthetic model/content, emulated Android, or ARM interpreter.
set -euo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
apk="${1:-}"
dest="${2:-$HOME/Games/DeadSpaceMobilePC}"
usage() {
cat <<'HELP'
Dead Space Mobile native PC build (Linux, i686)
Usage: bash Install-Real-APK-Linux.sh <original-apk-path> [new-install-dir]
Prerequisites on Debian/Ubuntu:
  sudo dpkg --add-architecture i386
  sudo apt update
  sudo apt install python3-pyelftools unzip file gcc-multilib libc6-dev-i386 \
      libsdl2-dev:i386 libgl1-mesa-dri:i386
Runs the original executable's reconstructed native code; assets are read from
your APK. Existing installations/saves are never overwritten.
HELP
}
if [[ -z "$apk" || "$apk" == "--help" || "$apk" == "-h" ]]; then usage; exit 0; fi
[[ -f "$apk" && -r "$apk" ]] ||
  { printf "Cannot read original APK: %s\n" "$apk" >&2; exit 2; }
apk="$(cd -- "$(dirname -- "$apk")" && pwd -P)/$(basename -- "$apk")"
for cmd in python3 unzip gcc file; do
  command -v "$cmd" >/dev/null ||
    { echo "Missing dependency: $cmd" >&2; exit 3; }
done
python3 -c 'import elftools' >/dev/null 2>&1 ||
  { echo 'Install python3-pyelftools to translate your original ARM library' >&2; exit 3; }
[[ -f /usr/include/SDL2/SDL.h ]] ||
  { echo 'Install 32-bit SDL2 headers: libsdl2-dev:i386' >&2; exit 3; }
probe="$(mktemp /tmp/dspc-i686-XXXXXXXX)"
trap 'rm -f "$probe"' RETURN
if ! printf 'int main(void){return 0;}\n' |
    gcc -m32 -x c - -o "$probe" >/dev/null 2>&1; then
  echo 'Missing GCC i686 toolchain: install gcc-multilib libc6-dev-i386' >&2
  exit 3
fi
rm -f "$probe"
for asset in lib/armeabi/libDeadSpace.so assets/EAMCore.ini; do
  unzip -Z1 "$apk" | grep -Fxq "$asset" ||
    { echo "Original APK is missing: $asset" >&2; exit 4; }
done
[[ -d "$here/tools" ]] ||
  { echo "Missing native translation toolkit in $here/tools" >&2; exit 4; }
[[ ! -e "$dest" ]] ||
  { echo "Already exists: $dest (choose a fresh destination; no files deleted)" >&2; exit 5; }
mkdir -p -- "$(dirname -- "$dest")"
stage="$(mktemp -d "$(dirname -- "$dest")/.dspc-install-XXXXXXXX")"
cleanup() { rm -rf -- "$stage"; }
trap cleanup EXIT
echo "[1/5] Extracting the original APK to a local, private installation..."
mkdir -p "$stage/apk" "$stage/tools" "$stage/userdata"
unzip -qq "$apk" -d "$stage/apk"
cp -a "$here/tools/." "$stage/tools/"
cd -- "$stage/tools"
echo "[2/5] Generating genuine native i686 code from original ARM binary..."
export DS_SO="$stage/apk/lib/armeabi/libDeadSpace.so"
python3 gen_all.py
echo "[3/5] Integrating verified native BufferedInputStream and M3G loader..."
python3 native_buffered_input.py --source out/fn_058.c --instrument
python3 m3g_loader_serialization.py --source out/fn_055.c --instrument
echo "[4/5] Building SDL2/OpenGL PC executable..."
cd -- out
export DS_BUILD_JOBS="${DS_BUILD_JOBS:-1}"
SDL_CFLAGS="-I/usr/include/SDL2" SDL_LIBS="-lSDL2" ./build.sh
[[ -x dspc_sdl ]] || { echo 'Native game binary missing' >&2; exit 6; }
file ./dspc_sdl
cat > "$stage/Run-DeadSpace-Mobile.sh" <<'LAUNCH'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
mkdir -p "$root/userdata"
export DS_ASSETS="$root/apk/assets"
export DS_DATA="$root/userdata"
export DS_WINDOW=1
export DS_STEPS="create,surface,frame"
export DS_FRAMES=0
export DS_NATIVE_BUFFERED_IO=1
export DS_M3G_SERIALIZE=1
export DS_QUIET_JAVA=1
export DS_WIDTH="${DS_WIDTH:-1280}"
export DS_HEIGHT="${DS_HEIGHT:-720}"
exec "$root/tools/out/dspc_sdl" "$root/apk/lib/armeabi/libDeadSpace.so"
LAUNCH
chmod +x "$stage/Run-DeadSpace-Mobile.sh"
echo "[5/5] Completing a fresh installation..."
mv -- "$stage" "$dest"
trap - EXIT
echo "Installed native PC game to: $dest"
echo "Launch using: $dest/Run-DeadSpace-Mobile.sh"
