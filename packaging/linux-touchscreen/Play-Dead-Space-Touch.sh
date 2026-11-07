#!/usr/bin/env bash
# Dead Space Mobile, genuine-APK native Linux i686 touchscreen launcher.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GAME="$ROOT/game"
SO="$GAME/lib/armeabi/libDeadSpace.so"
ASSETS="$GAME/assets"
LOGS="$ROOT/logs"
EXPECTED_SO_SHA1="64a3323cd3cc0c483a645b62af8508624163ba9b"
mkdir -p "$LOGS"
warn() { printf '%s\n' "$*" >&2; }
die() { warn "ERROR: $*"; exit 1; }

if [[ "$(uname -m)" != i?86 && "$(uname -m)" != "x86_64" ]]; then
    die "This native i686 executable needs an x86/x86-64 Linux device. This package is not an ARM build."
fi
[[ -f "$ROOT/dspc_sdl" ]] || die "Missing dspc_sdl executable beside launcher."
chmod u+x "$ROOT/dspc_sdl" || true
if ldd "$ROOT/dspc_sdl" 2>&1 | grep -q 'not found'; then
    ldd "$ROOT/dspc_sdl" >&2 || true
    die "Missing 32-bit Linux runtime libraries. Read README-LINUX-TOUCH.txt for Debian instructions."
fi

WINDOWED=0
APK=""
for arg in "$@"; do
    case "$arg" in
        --windowed) WINDOWED=1 ;;
        --help|-h)
            printf 'Usage: %s [--windowed] [path/to/original.apk]\n' "$0"
            exit 0 ;;
        --*) die "Unknown option $arg" ;;
        *) [[ -z "$APK" ]] || die "Supply only one APK path."; APK="$arg" ;;
    esac
done

if [[ ! -s "$SO" || ! -d "$ASSETS/published" ]]; then
    if [[ -z "$APK" ]] && command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
        APK="$(zenity --file-selection --title='Select your original Dead Space Mobile APK' --file-filter='Android packages | *.apk' 2>/dev/null || true)"
    fi
    if [[ -z "$APK" ]]; then
        warn "Select your original Deas.Space.Mobile.Fixes.apk (it will not be bundled or redistributed)."
        read -r -p "Full path to APK: " APK
    fi
    APK="${APK/#\~/$HOME}"
    [[ -s "$APK" ]] || die "APK not found: $APK"
    command -v unzip >/dev/null 2>&1 || die "unzip required for first launch."
    # Fully consume archive listing so grep cannot make unzip hit SIGPIPE
    # under set -o pipefail when the library appears early in a large APK.
    unzip -Z1 "$APK" | grep -Fx "lib/armeabi/libDeadSpace.so" >/dev/null ||
        die "APK has no expected lib/armeabi/libDeadSpace.so; Xperia APK is not interchangeable."
    mkdir -p "$GAME"
    # Copy ONLY the user's own library and assets, leaving any old saves intact.
    unzip -qn "$APK" 'lib/armeabi/libDeadSpace.so' 'assets/*' -d "$GAME" ||
        die "Could not extract original game assets."
fi
[[ -s "$SO" && -d "$ASSETS/published" ]] || die "Incomplete original game assets."
ACTUAL="$(sha1sum "$SO" | awk '{print $1}')"
[[ "$ACTUAL" = "$EXPECTED_SO_SHA1" ]] ||
    die "Wrong game library; expected SHA1 $EXPECTED_SO_SHA1, got $ACTUAL."
mkdir -p "$ASSETS/var"

# Native touchscreen mode. No Xperia/PS4 controller spoofing and no emulator.
export DS_WINDOW=1 DS_TOUCHSCREEN=1
export DS_TOUCH_FULLSCREEN="${DS_TOUCH_FULLSCREEN:-1}"
if [[ "$WINDOWED" == "1" ]]; then export DS_TOUCH_FULLSCREEN=0; fi
export DS_MANUFACTURER="${DS_MANUFACTURER:-Generic}"
export DS_MODEL="${DS_MODEL:-LinuxTouch}"
export DS_ASSETS="$ASSETS" DS_DATA="$ASSETS"
export DS_STEPS="create,surface,frame" DS_FRAMES=0
export DS_WIDTH="${DS_WIDTH:-1280}" DS_HEIGHT="${DS_HEIGHT:-720}"
export DS_RENDER_MIN_FAR="${DS_RENDER_MIN_FAR:-1200}"
export DS_QUIET_JAVA=1
STAMP="$(date '+%Y%m%d-%H%M%S')"
warn "Launching original game using native touchscreen input."
warn "Device: $(uname -m); original APK SHA1: $ACTUAL"
warn "Logs: $LOGS/$STAMP-stdout.log and $LOGS/$STAMP-stderr.log"
set +e
"$ROOT/dspc_sdl" "$SO" >"$LOGS/$STAMP-stdout.log" 2>"$LOGS/$STAMP-stderr.log"
EXIT_CODE=$?
set -e
printf '%s\n' "$EXIT_CODE" >"$LOGS/$STAMP-exit-code.txt"
if [[ "$EXIT_CODE" != 0 ]]; then
    tail -n 55 "$LOGS/$STAMP-stderr.log" >&2 || true
    die "Game exited with code $EXIT_CODE. Send the logs when reporting a problem."
fi
warn "Game exited cleanly."
