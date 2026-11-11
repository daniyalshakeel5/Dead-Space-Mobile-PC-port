#!/usr/bin/env bash
# Dead Space Mobile Linux: select real IIO accelerometer or provide a
# stationary dummy for sensorless hosts such as WSL. Sourced by the launcher.
# Reuses the original DS_ACCEL_DEVICE -> ds_accel_probe -> JNI input path;
# this does not simulate controller input or shake/jump gestures.

ds_accel_number() {
    local num
    [[ -r "$1" && -f "$1" ]] || return 1
    IFS= read -r num < "$1" || [[ -n "$num" ]] || return 1
    [[ "$num" =~ ^[[:space:]]*[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[[:space:]]*$ ]]
}
ds_accel_positive() {
    ds_accel_number "$1" || return 1
    local num
    IFS= read -r num < "$1" || [[ -n "$num" ]] || return 1
    awk -v v="$num" 'BEGIN { exit !(v + 0 > 0) }'
}
ds_accel_usable() {
    local dir="$1" axis scale
    [[ -d "$dir" ]] || return 1
    for axis in x y z; do
        ds_accel_number "$dir/in_accel_${axis}_raw" || return 1
        if [[ -f "$dir/in_accel_${axis}_scale" ]]; then
            scale="$dir/in_accel_${axis}_scale"
        elif [[ -f "$dir/in_accel_scale" ]]; then
            scale="$dir/in_accel_scale"
        else
            scale="$dir/scale"
        fi
        ds_accel_positive "$scale" || return 1
    done
}

ds_accel_select() {
    local candidate root chosen="" source="" dummy
    : "${DATA:?Accelerometer fallback needs a writable DATA directory}"

    if [[ "${DS_ACCEL_FORCE_DUMMY:-0}" != "1" ]]; then
        if [[ -n "${DS_ACCEL_DEVICE:-}" ]]; then
            if ds_accel_usable "$DS_ACCEL_DEVICE"; then
                chosen="$DS_ACCEL_DEVICE"
                source="user-selected"
            else
                printf '[ACCEL] Specified sensor is unavailable: %s; searching for another device\n' "$DS_ACCEL_DEVICE" >&2
            fi
        fi
        if [[ -z "$chosen" ]]; then
            root="${DS_ACCEL_SCAN_ROOT:-/sys/bus/iio/devices}"
            for candidate in "$root"/iio:device*; do
                if ds_accel_usable "$candidate"; then
                    chosen="$candidate"
                    source="Linux IIO"
                    break
                fi
            done
        fi
    fi

    if [[ -n "$chosen" ]]; then
        if [[ "$source" == user-selected ]]; then
            export DS_ACCEL_DEVICE="$chosen"
        else
            unset DS_ACCEL_DEVICE
        fi
        printf '[ACCEL] Using %s accelerometer (%s)\n' "$source" "$chosen" >&2
        return 0
    fi

    # 0,0,+9.807 m/s^2 at rest: neutral stationary input, no simulated shake.
    # Raw/scaled IIO attributes are understood by the EXISTING native reader.
    dummy="$DATA/.deadspace-dummy-accelerometer"
    (umask 077; mkdir -p -- "$dummy" &&
        printf '0\n' > "$dummy/in_accel_x_raw" &&
        printf '0\n' > "$dummy/in_accel_y_raw" &&
        printf '9807\n' > "$dummy/in_accel_z_raw" &&
        printf '0.001\n' > "$dummy/in_accel_scale") || {
        printf '[ACCEL] Cannot initialize dummy accelerometer in %s\n' "$dummy" >&2
        return 1
    }
    ds_accel_usable "$dummy" || {
        printf '[ACCEL] Dummy device did not pass the native IIO validation\n' >&2
        return 1
    }
    export DS_ACCEL_DEVICE="$dummy"
    printf '[ACCEL] No usable IIO sensor; initialized stationary dummy (%s)\n' "$dummy" >&2
}

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    ds_accel_select
fi
