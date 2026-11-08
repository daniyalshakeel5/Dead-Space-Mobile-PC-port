#!/bin/sh
# Build the original ARM32-translated game as a native 32-bit x86 executable.
# Translation, optimization and linking flags are unchanged. DS_BUILD_JOBS
# parallelizes independent fn_*.c translation units for faster REAL-APK tests.
set -eu
CF="-m32 -O1 -msse2 -mfpmath=sse -ffp-contract=off -w"
if [ "${DS_DEBUG:-1}" = "1" ]; then CF="$CF -g3 -fno-omit-frame-pointer"; fi
DS_CC="${DS_CC:-gcc}"
DS_BUILD_JOBS="${DS_BUILD_JOBS:-1}"
case "$DS_BUILD_JOBS" in
  ''|*[!0-9]*) echo "Invalid DS_BUILD_JOBS"; exit 2;;
esac
[ "$DS_BUILD_JOBS" -gt 0 ] && [ "$DS_BUILD_JOBS" -le 16 ] || {
  echo "DS_BUILD_JOBS must be 1..16"; exit 2;
}
export CF DS_CC
# Atomic object builds ensure partial compilation never creates a usable .o.
printf '%s\n' fn_*.c unlifted.c overrides.c tables.c |
  xargs -n 1 -P "$DS_BUILD_JOBS" sh -c '
    f="$1"
    o="${f%.c}.o"
    if [ "$o" -nt "$f" ]; then exit 0; fi
    temp="${o}.new.$$"
    trap '"'"'rm -f "$temp"'"'"' EXIT HUP INT TERM
    "$DS_CC" $CF -c "$f" -o "$temp" || { echo "FAILED $f"; exit 1; }
    mv -f "$temp" "$o"
    echo "built $f"
  ' compile || exit 1

"$DS_CC" $CF -c host.c -o host.o &&
"$DS_CC" $CF -DCOMPARE_MODE -c host.c -o host_cmp.o || exit 1

if [ -n "${SDL_CFLAGS:-}" ]; then
  "$DS_CC" $CF -DDS_SDL $SDL_CFLAGS -c host.c -o host_sdl.o &&
  "$DS_CC" -m32 -no-pie -o dspc_sdl fn_*.o unlifted.o overrides.o tables.o host_sdl.o \
    -lm -lpthread -ldl ${SDL_LIBS:-} || exit 1
fi
"$DS_CC" -m32 -no-pie -o dspc_native fn_*.o unlifted.o overrides.o tables.o host.o -lm -lpthread || exit 1
"$DS_CC" -m32 -no-pie -o dspc_native_cmp fn_*.o unlifted.o overrides.o tables.o host_cmp.o -lm -lpthread || exit 1
echo BUILD_DONE
