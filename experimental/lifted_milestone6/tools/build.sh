#!/bin/sh
# build the translated program: every generated chunk -> .o, then link two binaries (real + compare mode)
CF="-m32 -O1 -msse2 -mfpmath=sse -ffp-contract=off -w"
# Preserve source and frame-pointer locations for local SIGSEGV diagnostics.
if [ "${DS_DEBUG:-1}" = "1" ]; then CF="$CF -g3 -fno-omit-frame-pointer"; fi
for f in fn_*.c unlifted.c overrides.c tables.c; do
  o="${f%.c}.o"
  [ "$o" -nt "$f" ] && continue
  gcc $CF -c "$f" -o "$o" || { echo "FAILED $f"; exit 1; }
  echo "built $f"
done
gcc $CF -c host.c -o host.o && gcc $CF -DCOMPARE_MODE -c host.c -o host_cmp.o || exit 1
# optional window build: SDL2 headers/libs for 32-bit, e.g. SDL_CFLAGS="-I/usr/include/SDL2" SDL_LIBS="-lSDL2"
if [ -n "$SDL_CFLAGS" ]; then
  gcc $CF -DDS_SDL $SDL_CFLAGS -c host.c -o host_sdl.o && gcc -m32 -no-pie -o dspc_sdl fn_*.o unlifted.o overrides.o tables.o host_sdl.o -lm -lpthread -ldl $SDL_LIBS || exit 1
fi
gcc -m32 -no-pie -o dspc_native fn_*.o unlifted.o overrides.o tables.o host.o -lm -lpthread || exit 1
gcc -m32 -no-pie -o dspc_native_cmp fn_*.o unlifted.o overrides.o tables.o host_cmp.o -lm -lpthread || exit 1
echo BUILD_DONE
