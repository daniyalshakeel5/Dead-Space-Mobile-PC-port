#!/usr/bin/env bash
# Diagnose ORIGINAL Linux i686 m3g Loader and TexturePack data flow.
# Logs real vector before/after Loader::load subcalls; never creates dummy
# Object3D or bypasses original asset handling. Keeps original executable.
set -euo pipefail
root="${1:?Existing Milestone5 project required}"
guard="${2:?TexturePack guard Python required}"
root="$(cd "$root" && pwd)"
out="$root/tools/out"
binary="$out/dspc_sdl"
so="$root/apk/lib/armeabi/libDeadSpace.so"
loader_probe="$(dirname "$guard")/loader_vector_probe.py"
[[ -s "$guard" && -s "$loader_probe" && -s "$out/protos.h" && -s "$so" && -s "$binary" ]] || {
    echo "[DS M3G ERROR] Missing original native game, TexturePack guard, or loader probe" >&2
    exit 11
}
exec > >(tee "$out/texturepack-build.log") 2>&1
echo "[DS M3G] Native Linux executable: $binary"
echo "[DS M3G] Owner APK, original assets and saves remain unchanged."
mapfile -t tex_candidates < <(grep -l '^u64 f_0035b2e0(' "$out"/fn_*.c || true)
mapfile -t loader_candidates < <(grep -l '^u64 f_00365a2c(' "$out"/fn_*.c || true)
[[ "${#tex_candidates[@]}" -eq 1 && "${#loader_candidates[@]}" -eq 1 ]] || {
    echo "[DS M3G ERROR] Expected exactly one TexturePack and one m3g::Loader source" >&2
    exit 12
}
texsrc="${tex_candidates[0]}"
loadsrc="${loader_candidates[0]}"
[[ "$texsrc" != "$loadsrc" ]] || { echo '[DS M3G ERROR] Both functions in one file, needs different relink strategy' >&2; exit 13; }
texobj="${texsrc%.c}.o"
loadobj="${loadsrc%.c}.o"
backup="$binary.pre-loader-trace.bak"
[[ -s "$backup" ]] || cp -p -- "$binary" "$backup"
echo "[DS M3G] Original executable backed up: $backup"
# Validate BOTH ARM sites before any source instrumentation.
python3 "$guard" --source "$texsrc"
python3 "$loader_probe" --source "$loadsrc"
python3 "$guard" --source "$texsrc" --instrument
python3 "$loader_probe" --source "$loadsrc" --instrument
t1="$texobj.loader-new"
t2="$loadobj.loader-new"
target="$binary.loader-new"
cleanup() { rm -f -- "$t1" "$t2" "$target"; }
trap cleanup EXIT
flags=(-m32 -O1 -g3 -fno-omit-frame-pointer -msse2 -mfpmath=sse -ffp-contract=off -w)
gcc "${flags[@]}" -c "$texsrc" -o "$t1"
gcc "${flags[@]}" -c "$loadsrc" -o "$t2"
mapfile -t objs < <(printf '%s\n' "$out"/fn_*.o)
(( "${#objs[@]}" >= 20 )) || { echo '[DS M3G ERROR] Original translated C object files missing' >&2; exit 14; }
for n in unlifted overrides tables host_sdl; do
    [[ -s "$out/$n.o" ]] || { echo "[DS M3G ERROR] Missing $n.o" >&2; exit 15; }
done
found_tex=0
found_loader=0
for i in "${!objs[@]}"; do
    if [[ "${objs[i]}" == "$texobj" ]]; then objs[i]="$t1"; found_tex=1; fi
    if [[ "${objs[i]}" == "$loadobj" ]]; then objs[i]="$t2"; found_loader=1; fi
done
(( found_tex && found_loader )) || { echo '[DS M3G ERROR] Chunk replacement failed' >&2; exit 16; }
gcc -m32 -no-pie -o "$target" "${objs[@]}" \
    "$out/unlifted.o" "$out/overrides.o" "$out/tables.o" "$out/host_sdl.o" \
    -lm -lpthread -ldl -lSDL2
[[ -s "$target" ]]
# Install the working native binary only AFTER both C chunks compiled+linked.
mv -f -- "$target" "$binary"
mv -f -- "$t1" "$texobj"
mv -f -- "$t2" "$loadobj"
echo '[DS M3G] Success: native SDL Linux i686 Loader+TexturePack diagnostics linked.'
echo "[DS M3G] Previous executable preserved: $backup"
echo '[DS M3G] Next log includes M3G_LOADER_AFTER_DECODE, AFTER_CLEANUP, TEXPACK_SOURCE_VECTOR.'
