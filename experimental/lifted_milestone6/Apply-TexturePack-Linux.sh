#!/usr/bin/env bash
# Target owner-confirmed f_0035b2e0 TexturePack NULL fault in Linux ELF32.
# No fake textures, skipped engine methods, or original asset modifications.
set -euo pipefail
root="${1:?Existing Milestone5 project required}"
guard="${2:?TexturePack guard Python required}"
root="$(cd "$root" && pwd)"
out="$root/tools/out"
binary="$out/dspc_sdl"
so="$root/apk/lib/armeabi/libDeadSpace.so"
[[ -s "$guard" && -s "$out/protos.h" && -s "$so" && -s "$binary" ]] || {
    echo "[DS TEXPACK ERROR] Missing original guest files, executable or guard" >&2
    exit 11
}
exec > >(tee "$out/texturepack-build.log") 2>&1
echo "[DS TEXPACK] Native Linux executable: $binary"
echo "[DS TEXPACK] Owner APK, assets and saves remain unchanged."
mapfile -t candidates < <(grep -l '^u64 f_0035b2e0(' "$out"/fn_*.c || true)
[[ "${#candidates[@]}" -eq 1 ]] || {
    echo "[DS TEXPACK ERROR] Expected one f_0035b2e0 C source; found ${#candidates[@]}" >&2
    exit 12
}
src="${candidates[0]}"
obj="${src%.c}.o"
backup="$binary.pre-texturepack-diagnostic.bak"
[[ -f "$backup" ]] || cp -p -- "$binary" "$backup"
echo "[DS TEXPACK] Crash site source: $src"
python3 "$guard" --source "$src" --instrument
tempobj="$obj.texturepack-new"
tempbin="$binary.texturepack-new"
cleanup() { rm -f -- "$tempobj" "$tempbin"; }
trap cleanup EXIT
gcc -m32 -O1 -g3 -fno-omit-frame-pointer -msse2 -mfpmath=sse -ffp-contract=off \
    -w -c "$src" -o "$tempobj"
mapfile -t objs < <(printf '%s\n' "$out"/fn_*.o)
(( "${#objs[@]}" >= 20 )) || {
    echo "[DS TEXPACK ERROR] Original compiled translated function objects missing" >&2
    exit 13
}
for n in unlifted overrides tables host_sdl; do
    [[ -s "$out/$n.o" ]] || { echo "[DS TEXPACK ERROR] Missing $n.o" >&2; exit 14; }
done
for i in "${!objs[@]}"; do
    if [[ "${objs[i]}" == "$obj" ]]; then objs[i]="$tempobj"; fi
done
gcc -m32 -no-pie -o "$tempbin" "${objs[@]}" \
    "$out/unlifted.o" "$out/overrides.o" "$out/tables.o" "$out/host_sdl.o" \
    -lm -lpthread -ldl -lSDL2
[[ -s "$tempbin" ]]
mv -f -- "$tempobj" "$obj"
mv -f -- "$tempbin" "$binary"
echo "[DS TEXPACK] Built instrumented Linux i686 SDL executable."
echo "[DS TEXPACK] Previous binary retained at $backup"
echo "[DS TEXPACK] Original source retained at $src.pre-texturepack-probe.bak"
