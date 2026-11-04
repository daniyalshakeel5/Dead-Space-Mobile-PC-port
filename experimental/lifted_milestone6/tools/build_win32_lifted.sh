#!/usr/bin/env bash
# Compile the owner's AOT generated C into native x86 Windows SDL/OpenGL locally.
# Never upload, archive or commit derived copyrighted fn_*.c or original APK.
set -euo pipefail

PROJECT=${1:?Project root required}
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$(cd "$PROJECT" && pwd)"
GEN="$PROJECT/tools/out"
WORK="$PROJECT/windows-build"
OBJ="$WORK/obj"
SDK_VERSION=2.32.10
SDK_ARCHIVE="$WORK/SDL2-devel-${SDK_VERSION}-mingw.tar.gz"
SDKROOT="$WORK/SDL2-${SDK_VERSION}/i686-w64-mingw32"
CC=i686-w64-mingw32-gcc-posix

mkdir -p "$WORK" "$OBJ"
exec > >(tee "$WORK/compile-windows.log") 2>&1
printf '[DS WINDOWS] Private translated functions: %s\n' "$GEN"
printf '[DS WINDOWS] Output: %s\n' "$WORK"

if ! command -v "$CC" >/dev/null 2>&1; then
    echo '[ERROR] Missing Windows cross compiler; use the supplied one-click launcher.'
    exit 2
fi
for f in protos.h tables.c overrides.c unlifted.c rt.h; do
    [[ -s "$GEN/$f" ]] || { echo "[ERROR] Original generated game source missing: $GEN/$f"; exit 3; }
done
[[ -s "$PROJECT/apk/lib/armeabi/libDeadSpace.so" ]] ||
    { echo '[ERROR] Original libDeadSpace.so is absent.'; exit 4; }
[[ -d "$PROJECT/apk/assets" ]] ||
    { echo '[ERROR] Matching original apk/assets directory is absent.'; exit 5; }
shopt -s nullglob
chunks=("$GEN"/fn_*.c)
(( ${#chunks[@]} >= 20 )) ||
    { echo '[ERROR] Private generated translated C is missing or incomplete.'; exit 6; }
for f in "${chunks[@]}"; do
    (( $(stat -c %s "$f") >= 1024 )) ||
        { echo "[ERROR] Incomplete translated C: $f"; exit 7; }
done
if [[ ! -f "$SDKROOT/include/SDL2/SDL.h" || ! -f "$SDKROOT/bin/SDL2.dll" ]]; then
    echo '[DS WINDOWS] Downloading official Windows SDL2 x86 SDK...'
    if [[ ! -f "$SDK_ARCHIVE" ]]; then
        curl --fail --location --retry 3 --silent --show-error \
          "https://www.libsdl.org/release/SDL2-devel-${SDK_VERSION}-mingw.tar.gz" \
          -o "$SDK_ARCHIVE.partial"
        mv "$SDK_ARCHIVE.partial" "$SDK_ARCHIVE"
    fi
    tar -xzf "$SDK_ARCHIVE" -C "$WORK"
fi
if [[ ! -f "$SDKROOT/include/SDL2/SDL.h" ||
      ! -f "$SDKROOT/lib/libSDL2.dll.a" ||
      ! -f "$SDKROOT/bin/SDL2.dll" ]]; then
    echo '[ERROR] Official SDL2 x86 MinGW SDK files could not be verified.'
    exit 8
fi
python3 "$SRC/gen_gl.py" "$OBJ/gl_gen.c"

# Same lifted softfp model as the known Linux native build.
CFLAGS=(
    -O1 -g -std=gnu11 -w -msse2 -mfpmath=sse -ffp-contract=off
    -fno-strict-aliasing -fno-omit-frame-pointer
    "-I$GEN" "-I$SRC" "-I$OBJ" "-I$SDKROOT/include/SDL2"
    -DDS_SDL -DSDL_MAIN_HANDLED
)
count=0
for f in "${chunks[@]}" "$GEN"/overrides.c "$GEN"/unlifted.c "$GEN"/tables.c; do
    stem=$(basename "${f%.c}")
    printf '[DS WINDOWS] Compiling %d/%d: %s\n' \
        "$((count+1))" "$((${#chunks[@]}+3))" "$stem"
    if [[ -f "$OBJ/$stem.o" && "$OBJ/$stem.o" -nt "$f" &&
          "$OBJ/$stem.o" -nt "$GEN/protos.h" && "$OBJ/$stem.o" -nt "$GEN/rt.h" ]]; then
        echo "[DS WINDOWS] Reusing compiled $stem.o (source unchanged)"
    else
        "$CC" "${CFLAGS[@]}" -c "$f" -o "$OBJ/$stem.o"
    fi
    count=$((count+1))
done
"$CC" "${CFLAGS[@]}" -c "$SRC/host.c" -o "$OBJ/host_win.o"
"$CC" "${CFLAGS[@]}" -c "$SRC/win32_substrate.c" -o "$OBJ/win32_substrate.o"
for name in win32_missing_imports win32_missing_gles; do
    "$CC" "${CFLAGS[@]}" -include "$SRC/win32_import_bridge.h" \
        -c "$SRC/$name.c" -o "$OBJ/$name.o"
done

mapfile -t objects < <(find "$OBJ" -maxdepth 1 -type f -name 'fn_*.o' | sort)
objects+=( "$OBJ/overrides.o" "$OBJ/unlifted.o" "$OBJ/tables.o"
           "$OBJ/host_win.o" "$OBJ/win32_substrate.o"
           "$OBJ/win32_missing_imports.o" "$OBJ/win32_missing_gles.o" )
echo "[DS WINDOWS] Linking ${#objects[@]} native Windows x86 objects..."
"$CC" -static-libgcc -Wl,--large-address-aware -Wl,--stack,16777216 \
   -o "$WORK/DeadSpaceMobile-Win32.exe" \
   "${objects[@]}" "-L$SDKROOT/lib" -lSDL2 -lwinpthread -lws2_32 -lopengl32 -lm

cp -f "$SDKROOT/bin/SDL2.dll" "$WORK/SDL2.dll"
WINPTHREAD=$("$CC" -print-file-name=libwinpthread-1.dll)
if [[ ! -f "$WINPTHREAD" ]]; then
    WINPTHREAD=$(find /usr -name 'libwinpthread-1.dll' -type f -print -quit 2>/dev/null || true)
fi
[[ -n "$WINPTHREAD" && -f "$WINPTHREAD" ]] ||
    { echo '[ERROR] Required winpthread DLL is missing.'; exit 9; }
cp -f "$WINPTHREAD" "$WORK/libwinpthread-1.dll"
file "$WORK/DeadSpaceMobile-Win32.exe" || true
echo '[DS WINDOWS] SUCCESS: owner-only native Windows x86 executable built.'
echo "[DS WINDOWS] Location: $WORK/DeadSpaceMobile-Win32.exe"
echo '[DS WINDOWS] No original game files were uploaded.'
