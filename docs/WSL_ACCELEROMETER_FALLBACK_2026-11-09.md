# WSL / sensorless Linux startup — safe accelerometer fallback

**2026-11-09.** Owner's issue: Dead Space Mobile crashes on the WSL machine
after accelerometer input was switched from an emulated input to the real Linux
IIO accelerometer.

## Evidence

The native host's `SystemAndroidDelegate.GetAccelerometerCount` reports
`ds_accel_init() ? "1" : "0"`. Without any readable IIO sensor under
`/sys/bus/iio/devices`, `ds_accel_init()` originally returned 0, so the
original game was told no accelerometer was available. The same probe also
controls whether `NativeOnAcceleration` is driven.

This is a **credible cause** of the WSL startup failure, not yet verified by
replaying the owner's exact failure on WSL hardware.

## Packaged fix for the uploaded native controller build

`source/packaging/linux-touchscreen/Prepare-Accelerometer.sh` is sourced by
the Linux launcher after establishing a writable `DS_DATA` folder and
**before JNI starts**. It:
1. Accepts an explicit `DS_ACCEL_DEVICE` only if the three raw axes and
   corresponding positive IIO scales are readable and valid.
2. Otherwise scans usable Linux IIO devices, leaving native device selection
   (including display/lid preference and coordinate matrices) unchanged.
3. If no usable sensor exists (including WSL), creates four small numeric
   files under `$DS_DATA/.deadspace-dummy-accelerometer`:

   ```
   in_accel_x_raw = 0
   in_accel_y_raw = 0
   in_accel_z_raw = 9807
   in_accel_scale = 0.001
   ```

4. Exports `DS_ACCEL_DEVICE` pointing to that directory. The **existing**
   native IIO reader sees a real, readable device, reports count=1, and sends
   neutral (0,0,+9.807 m/s²) sensor samples. It does not synthesize an action,
   weapon tilt, shake, or Zero-G jump.

A stale explicit sensor path from another machine triggers a new scan and only
uses the dummy when no real sensor can be read.

The owner's **existing, patched i686 executable and controller remapper
remain byte-for-byte unchanged**. This is a launcher-level compatibility
fix, not a rebuilt game or a change to native combat/hit detection.

The future-source equivalent is in
`source/experimental/lifted_milestone6/tools/linux_accel.h`. It reports one
logical neutral accelerometer and supplies neutral samples if hardware
discovery fails even when run without the package launcher.

## Testing and use

```bash
bash source/tests/test-accelerometer-fallback.sh
DS_ACCEL_DIAG=1 bash ./Play-Dead-Space-Touch.sh
# Optional diagnostic force-fallback:
DS_ACCEL_FORCE_DUMMY=1 bash ./Play-Dead-Space-Touch.sh
```

Six asset-free regression checks cover sensorless WSL-like environment, valid
real sensor override, stale override plus a real sensor, forced dummy, invalid
sensor data, and repeated launch. Shell syntax and ZIP integrity also pass.

**Still required:** launch the actual game on WSL without an IIO device,
confirm first boot does not crash, then verify the real-sensor machine still
receives actual movements and the physical Zero-G jump retains D-pad Up.
