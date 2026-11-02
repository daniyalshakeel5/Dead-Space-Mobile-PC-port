#!/usr/bin/env bash
# Fix OpenGL missing in DeadSpaceMobile-Xperia-Linux bundled ARMHF sysroot.
# Extract this small patch beside Run Dead Space.sh, then Run as Program.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "$0")" && pwd)"
LIB="$ROOT/sysroot/usr/lib/arm-linux-gnueabihf"
FIX="$ROOT/graphics-patch"
fail(){ printf '\nGraphics patch failed: %s\n' "$*" >&2; exit 1; }
[[ -x "$ROOT/bin/qemu-arm-static" && -x "$ROOT/bin/deadspace-armhf" ]] ||
  fail "Extract this patch into your existing DeadSpaceMobile-Xperia-Linux folder, alongside Run Dead Space.sh."
[[ -d "$FIX/lib" && -f "$FIX/lib/libGL.so.1" ]] ||
  fail "Patch incomplete: graphics-patch/lib/libGL.so.1 missing."
mkdir -p "$LIB" "$ROOT/logs"
for name in libGL.so.1 libGLX.so.0 libGLX_mesa.so.0; do
  [[ -f "$FIX/lib/$name" ]] || fail "Missing bundled ARMHF $name"
  cp -f -- "$FIX/lib/$name" "$LIB/$name"
done
if [[ -f "$FIX/lib/libGLdispatch.so.0" ]]; then
  cp -f -- "$FIX/lib/libGLdispatch.so.0" "$LIB/libGLdispatch.so.0"
fi
if [[ -f "$FIX/bin/graphics-probe-armhf" ]]; then
  cp -f -- "$FIX/bin/graphics-probe-armhf" "$ROOT/bin/graphics-probe-armhf"
  chmod +x "$ROOT/bin/graphics-probe-armhf"
fi
printf 'OpenGL repair installed for the ARMHF guest.\n'
printf 'Verified ARM libraries now in %s\n' "$LIB"
if [[ -x "$ROOT/bin/graphics-probe-armhf" ]]; then
  printf 'Checking ARMHF SDL2 window + OpenGL under QEMU...\n'
  export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
  log="$ROOT/logs/graphics-probe-$(date +%Y%m%d-%H%M%S).log"
  if "$ROOT/bin/qemu-arm-static" -L "$ROOT/sysroot" \
        "$ROOT/bin/graphics-probe-armhf" >"$log" 2>&1; then
    cat "$log"
    printf 'Graphics probe succeeded. Start Run Dead Space.sh again.\n'
  else
    cat "$log"
    printf '\nOpenGL library was installed but the graphical probe failed.\n'
    printf 'Send the newest graphics-probe log for the next runtime diagnosis.\n'
  fi
else
  printf 'Start Run Dead Space.sh again and send its newest log if needed.\n'
fi
