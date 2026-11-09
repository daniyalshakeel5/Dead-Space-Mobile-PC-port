#!/usr/bin/env bash
# Dead Space Mobile, genuine-APK native Linux i686 touchscreen launcher.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GAME="$ROOT/game"
SO="$GAME/lib/armeabi/libDeadSpace.so"
ASSETS="$GAME/assets"
DATA="${DS_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/dead-space-mobile}"
LOGS="${DS_LOG_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/dead-space-mobile/logs}"
EXPECTED_SO_SHA1="64a3323cd3cc0c483a645b62af8508624163ba9b"
mkdir -p "$LOGS"
warn() { printf '%s\n' "$*" >&2; }
die() { warn "ERROR: $*"; exit 1; }

if [[ "$(uname -m)" != i?86 && "$(uname -m)" != "x86_64" ]]; then
    die "This native i686 executable needs an x86/x86-64 Linux device. This package is not an ARM build."
fi
[[ -f "$ROOT/dspc_sdl" ]] || die "Missing dspc_sdl executable beside launcher."
chmod u+x "$ROOT/dspc_sdl" || true
if ldd "$ROOT/dspc_sdl" 2>&1 | grep 'not found' >/dev/null; then
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
if [[ -n "$APK" ]]; then
    APK="${APK/#\~/$HOME}"
    [[ -s "$APK" ]] || die "APK not found: $APK"
    command -v unzip >/dev/null 2>&1 || die "unzip required to read the APK."
fi

if [[ ! -s "$SO" || ! -d "$ASSETS/published" ]]; then
    if [[ -z "$APK" ]] && command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
        APK="$(zenity --file-selection --title='Select Dead Space Mobile Fixes or Reanimated v1.3 APK' --file-filter='Android packages | *.apk' 2>/dev/null || true)"
    fi
    if [[ -z "$APK" ]]; then
        warn "Select your own original Fixes APK or DeadSpaceReanimated-1.3.apk (no game files are bundled)."
        read -r -p "Full path to APK: " APK
    fi
    APK="${APK/#\~/$HOME}"
    [[ -s "$APK" ]] || die "APK not found: $APK"
    command -v unzip >/dev/null 2>&1 || die "unzip required for first launch."
    # Fully consume archive listing so grep cannot make unzip hit SIGPIPE
    # under set -o pipefail when the library appears early in a large APK.
    mkdir -p "$GAME"
    if unzip -Z1 "$APK" | grep -Fx 'lib/armeabi/libDeadSpace.so' >/dev/null; then
        # Original 2011/Fixes donor (confirmed same 32-bit game library).
        unzip -qn "$APK" 'lib/armeabi/libDeadSpace.so' 'assets/*' -d "$GAME" ||
            die "Could not extract original game assets."
        warn "[DONOR] Original Dead Space Mobile/Fixes APK"
    elif unzip -Z1 "$APK" | grep -Fx 'assets/game/libDeadSpace32.so' >/dev/null; then
        # Ginestro's Reanimated v1.3 embeds the *identical* 2011 ARM ELF and
        # all 1,729 original published files under assets/game/. Its ARM64
        # Android runtime and separate HD KTX pack are not Linux-x86 code.
        warn "[DONOR] Dead Space Reanimated APK (native x86 runtime recreates fixes)"
        stage="$(mktemp -d "$GAME/.reanimated.XXXXXXXX")"
        trap 'rm -rf -- "$stage"' EXIT
        unzip -qn "$APK" 'assets/game/assets/*' -d "$stage" ||
            die "Could not unpack Reanimated's original game data."
        mkdir -p "$GAME/assets" "$GAME/lib/armeabi"
        cp -an "$stage/assets/game/assets/." "$GAME/assets/" ||
            die "Could not install Reanimated's game asset tree."
        unzip -p "$APK" 'assets/game/libDeadSpace32.so' > "$SO" ||
            die "Could not extract Reanimated's original 32-bit game library."
        rm -rf -- "$stage"
        trap - EXIT
    else
        die "Unsupported APK: supply original Fixes or DeadSpaceReanimated-1.3.apk. Xperia Play uses a different library."
    fi
fi
[[ -s "$SO" && -d "$ASSETS/published" ]] || die "Incomplete original game assets."
ACTUAL="$(sha1sum "$SO" | awk '{print $1}')"
[[ "$ACTUAL" = "$EXPECTED_SO_SHA1" ]] ||
    die "Wrong game library; expected SHA1 $EXPECTED_SO_SHA1, got $ACTUAL."
# Keep saved progress independent of APK extraction and removable installs.
mkdir -p "$DATA/var" || die "Cannot create save directory: $DATA/var"
DATA="$(cd -- "$DATA" && pwd)"
if [[ ! -e "$DATA/.legacy-save-imported" && "$DATA" != "$ASSETS" ]]; then
    if [[ -d "$ASSETS/var" ]]; then
        cp -an "$ASSETS/var/." "$DATA/var/" || die "Could not migrate existing saves."
    fi
    : > "$DATA/.legacy-save-imported"
fi
probe="$(mktemp "$DATA/var/.write-check.XXXXXXXX")" || die "Save directory is not writable: $DATA/var"
printf 'save-write-check\n' > "$probe" || die "Cannot write save directory."
mv -- "$probe" "$probe.renamed" || die "Cannot rename files in save directory."
rm -- "$probe.renamed"
warn "Save files: $DATA/var"

# The original Android game expects accelerometer support even on hosts with
# no IIO device (notably WSL). Initialize a stationary dummy IIO device only
# when no readable real accelerometer exists; preserve real hardware readings.
[[ -r "$ROOT/Prepare-Accelerometer.sh" ]] ||
    die "Missing accelerometer initialization helper: Prepare-Accelerometer.sh"
source "$ROOT/Prepare-Accelerometer.sh" || die "Accelerometer initialization failed"

# A Reanimated APK may be supplied to add HD textures to an existing install.
if [[ -n "$APK" ]]; then
    APK="${APK/#\~/$HOME}"
    if unzip -Z1 "$APK" | grep -F 'assets/texture_hd/' >/dev/null; then
        stage="$(mktemp -d "$GAME/.hd.XXXXXXXX")"
        trap 'rm -rf -- "$stage"' EXIT
        unzip -q "$APK" 'assets/texture_hd/*.ktx' -d "$stage" || die "Could not extract Reanimated HD textures."
        mkdir -p "$GAME/texture_hd"
        cp -a "$stage/assets/texture_hd/." "$GAME/texture_hd/" || die "Could not install HD textures."
        rm -rf -- "$stage"
        trap - EXIT
    fi
fi
export DS_HD_TEXTURES="${DS_HD_TEXTURES:-$GAME/texture_hd}"
if [[ "$DS_HD_TEXTURES" != 0 && -d "$DS_HD_TEXTURES" ]]; then warn "HD texture pack: $DS_HD_TEXTURES"; fi

# Native touchscreen mode. No Xperia/PS4 controller spoofing and no emulator.
export DS_WINDOW=1 DS_TOUCHSCREEN=1
# Recreate Reanimated's resolution-scaled tap and interaction thresholds.
# Set DS_REANIMATED_TOUCH=0 for an exact original-game touch baseline.
export DS_REANIMATED_TOUCH="${DS_REANIMATED_TOUCH:-1}"
export DS_TOUCH_FULLSCREEN="${DS_TOUCH_FULLSCREEN:-1}"
if [[ "$WINDOWED" == "1" ]]; then export DS_TOUCH_FULLSCREEN=0; fi
export DS_MANUFACTURER="${DS_MANUFACTURER:-Generic}"
export DS_MODEL="${DS_MODEL:-LinuxTouch}"
export DS_ASSETS="$ASSETS" DS_DATA="$DATA"
export DS_STEPS="create,surface,frame" DS_FRAMES=0
export DS_WIDTH="${DS_WIDTH:-1366}" DS_HEIGHT="${DS_HEIGHT:-768}"
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
