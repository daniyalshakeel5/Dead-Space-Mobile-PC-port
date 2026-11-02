#!/usr/bin/env bash
# User-local original Xperia game; all QEMU and ARMHF dependencies are bundled.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
GAME="$ROOT/game-data"
LOGDIR="$ROOT/logs"
mkdir -p "$LOGDIR"
fail() { echo "Dead Space launch error: $*" >&2; exit 1; }
pick_file() {
  if command -v zenity >/dev/null; then
    zenity --file-selection --title="$1" 2>/dev/null || :
  elif command -v kdialog >/dev/null; then
    kdialog --getopenfilename "$HOME" "$2" "$1" 2>/dev/null || :
  else
    echo "No graphical chooser installed. Place your deadspace.zip beside this launcher." >&2
  fi
}
verify_hash() {
  local got
  got="$(sha256sum "$1" | cut -d' ' -f1)"
  [[ "$got" == "$2" ]]
}
install_original() {
  [[ -f "$GAME/lib/armeabi/libEAMGameDeadSpace.so" ]] && return
  local original="$ROOT/deadspace.zip" temp
  if [[ ! -f "$original" ]]; then
    original="$(pick_file "Choose original Xperia Play deadspace.zip" "*.zip")"
  fi
  [[ -f "$original" ]] || fail "Please provide the original Xperia Play deadspace.zip"
  echo "Validating original Xperia game ZIP..."
  [[ "$(stat -c%s "$original")" == "145903794" ]] || fail "Wrong ZIP size for the verified Xperia 1.1.33 donor"
  verify_hash "$original" "04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31" ||
    fail "Wrong ZIP checksum. No unverified game package will be extracted."
  temp="$(mktemp -d "$ROOT/.extract.XXXXXXXX")"
  "$ROOT/bin/busybox" unzip -q "$original" -d "$temp" ||
    fail "Original game ZIP could not be extracted"
  [[ -f "$temp/deadspace/lib/armeabi/libEAMGameDeadSpace.so" ]] ||
    fail "Original Xperia ARM library not found"
  [[ "$(sha1sum "$temp/deadspace/lib/armeabi/libEAMGameDeadSpace.so" | cut -d' ' -f1)" == "0ed42b611415015807f759ec9b5457857143ce39" ]] ||
    fail "Wrong Xperia ARM executable version"
  [[ ! -e "$GAME" ]] || fail "Incomplete game-data directory already exists; won't overwrite it."
  mv "$temp/deadspace" "$GAME"
  rm -rf "$temp"
}
install_hd() {
  local source="$ROOT/DeadSpaceReanimated-1.3.apk" out="$GAME/enhancements/reanimated" temp hash
  [[ -d "$out/texture_hd" ]] && return
  if [[ ! -f "$source" ]]; then
    source="$(pick_file "Optional: Reanimated APK for future HD textures (Cancel to skip)" "*.apk")"
  fi
  [[ -f "$source" ]] || return 0
  hash="$(sha256sum "$source" | cut -d' ' -f1)"
  case "$hash" in
    dbd94ee54fc3fefdddb101fac4eba1e2058a5db7cfcf30ac41ad96e88b285a3a|042c0d60c53c72d3c71137710834f2081684625d66d0c3d214d9e75d468830a3) ;;
    *) echo "Unrecognized Reanimated APK; optional HD import skipped." >&2; return ;;
  esac
  temp="$(mktemp -d "$ROOT/.hd.XXXXXXXX")"
  if "$ROOT/bin/busybox" unzip -q "$source" 'assets/texture_hd/*' -d "$temp" &&
     [[ -d "$temp/assets/texture_hd" ]]; then
    mkdir -p "$out"
    mv "$temp/assets/texture_hd" "$out/"
    echo "HD KTX sidecar imported (not yet applied to the renderer)."
  else
    echo "Optional HD import failed; running original graphics." >&2
  fi
  rm -rf "$temp"
}
[[ "$(uname -m)" == "x86_64" ]] || fail "This QEMU Linux package targets x86-64, not $(uname -m)"
[[ -x "$ROOT/bin/qemu-arm-static" && -x "$ROOT/bin/deadspace-armhf" ]] ||
  fail "Bundled QEMU and/or licensed ARM loader missing."
[[ -e "$ROOT/sysroot/lib/ld-linux-armhf.so.3" ]] ||
  fail "Bundled ARMHF glibc loader missing."
install_original
install_hd
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
export LOADER_TRACE=1
export DEADSPACE_DONOR_PROFILE=vita-rip-atc
# Keep SDL video/audio enabled for a REAL window; do not set offscreen.
LOG="$LOGDIR/game-$(date +%Y%m%d-%H%M%S).log"
echo "Starting original Xperia game under bundled Linux qemu-arm."
echo "Original files stay on your device. Diagnostics: $LOG"
if "$ROOT/bin/qemu-arm-static" -L "$ROOT/sysroot" "$ROOT/bin/deadspace-armhf" "$GAME" 2>&1 | tee "$LOG"; then
  exit 0
else
  fail "Game exited unsuccessfully. Share the latest log in $LOGDIR."
fi
