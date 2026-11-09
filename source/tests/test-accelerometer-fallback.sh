#!/usr/bin/env bash
# Asset-free IIO fallback regression: never start the full Dead Space binary.
set -Eeuo pipefail
repo="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
helper="${1:-$repo/source/packaging/linux-touchscreen/Prepare-Accelerometer.sh}"
T="$(mktemp -d)"; trap 'rm -rf -- "$T"' EXIT
mkdir -p "$T/sys/iio:device3" "$T/real" "$T/bad"
for d in "$T/sys/iio:device3" "$T/real" "$T/bad"; do
    for axis in x y z; do printf '0\n' > "$d/in_accel_${axis}_raw"; done
    printf '0.001\n' > "$d/in_accel_scale"
done
printf 'NaN\n' > "$T/bad/in_accel_x_raw"
run() { env "$@" bash -c 'set -Eeuo pipefail; source "$1"; echo "SELECTED=${DS_ACCEL_DEVICE-unset}"' _ "$helper" 2>&1; }
out="$(run DATA="$T/none" DS_ACCEL_SCAN_ROOT="$T/not-mounted")"
[[ "$out" == *'initialized stationary dummy'* ]]
[[ "$(cat "$T/none/.deadspace-dummy-accelerometer/in_accel_z_raw")" == 9807 ]]
echo '[PASS] no IIO sensor -> stationary dummy'
out="$(run DATA="$T/valid" DS_ACCEL_DEVICE="$T/real" DS_ACCEL_SCAN_ROOT="$T/not-mounted")"
[[ "$out" == *"SELECTED=$T/real"* && "$out" != *'initialized stationary dummy'* ]]
echo '[PASS] real sensor retained'
out="$(run DATA="$T/recover" DS_ACCEL_DEVICE="$T/stale" DS_ACCEL_SCAN_ROOT="$T/sys")"
[[ "$out" == *'Using Linux IIO'* && "$out" == *'SELECTED=unset'* ]]
[[ ! -d "$T/recover/.deadspace-dummy-accelerometer" ]]
echo '[PASS] stale override -> scan for real IIO'
out="$(run DATA="$T/forced" DS_ACCEL_FORCE_DUMMY=1 DS_ACCEL_DEVICE="$T/real" DS_ACCEL_SCAN_ROOT="$T/sys")"
[[ "$out" == *'initialized stationary dummy'* ]]
echo '[PASS] forced dummy'
out="$(run DATA="$T/malformed" DS_ACCEL_DEVICE="$T/bad" DS_ACCEL_SCAN_ROOT="$T/not-mounted")"
[[ "$out" == *'initialized stationary dummy'* ]]
echo '[PASS] malformed sensor values -> dummy'
run DATA="$T/repeat" DS_ACCEL_SCAN_ROOT="$T/not-mounted" >/dev/null
out="$(run DATA="$T/repeat" DS_ACCEL_SCAN_ROOT="$T/not-mounted")"
[[ "$out" == *'initialized stationary dummy'* ]]
echo '[PASS] repeat launch'
