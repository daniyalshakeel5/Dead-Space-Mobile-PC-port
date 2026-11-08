# Linux collision and accelerometer fixes

## Confirmed collision fault

A captured plasma-cutter shot reached Bullet broadphase and narrowphase code,
but the native build returned a null collision object for all three rays.
Replaying the exact same memory state and ray through the original ARM
`btCollisionWorld::rayTest` returned a hit. The capture occupied 77 memory
pages; it is private diagnostic data and is not included in this repository.

The first divergent call was the internal compiler clone `btRayAabb2` at
original RVA `0x00236d74`. It reads the maximum ray distance from VFP `s0`.
Its caller, the quantized BVH ray traversal at `0x00238f44`, supplies that
value in `s0`. The translator initialized a new zero-filled `V[32]` for
every function, silently discarding this compiler-private float argument.
With a zero maximum distance, valid triangle bounds were rejected.

VFP registers and FPSCR now belong to the guest thread, shared across
translated calls. Existing translated vpush/vpop instructions preserve
callee-saved registers. Thread-local storage separates game/render workers.
This preserves original instruction semantics without modifying collision
masks, ray endpoints, hitboxes, fuse eligibility, or gameplay progression.

The corrected native replay exactly matched the original ARM callback:

- Collision object: `0xa2377820` in that captured process.
- Hit-fraction bits: `0x3ce9636f`.
- The complete first six callback words matched.

The ARM emulator was used only as an offline test oracle; the shipped game
remains an ARM-to-C translation compiled into a native i686 executable.

## Accelerometer bridge

The previous JNI shim reported one accelerometer but discarded all delegate
calls and emitted no acceleration events. The Linux SDL host now discovers
readable IIO accelerometers and prefers display/lid sensors over base sensors.
It applies raw-channel scale/offset and the sensor/udev mounting matrix, then
calls the genuine `NativeOnAcceleration(FFF)V` JNI entry point. The original
game normalizes gravity and forwards events to its input system.

The original delegate's `SetEnabled(Z)V` and `SetUpdateFrequency(I)V`
requests control delivery. The integer frequency request is an Android
SENSOR_DELAY category. Missing/failed sensors produce a diagnostic rather
than fabricated readings. No sensor calibration, sampling-frequency sysfs
setting, or system permission is changed.

Verified on Google Phaser360: display sensor `iio:device1`, kernel scale
`0.000598550`, udev matrix `diag(-1,-1,-1)`. Real readings reached the
original `im::AccelerometerDevice::OnAcceleration` handler, with approximately
one g in the stationary device's principal axis.

## Validation

- Original failing shot: old native replay misses; original ARM replay hits;
  fixed native replay matches the ARM hit object and hit fraction exactly.
- Synthetic cross-translation-unit VFP argument passing, callee-saved VFP
  registers, FPSCR persistence, and independent thread state pass.
- IIO discovery/preference, SI unit conversion, mounting matrix validation,
  enable/rate handling, and failed-read behavior pass with test fixtures.
- Existing host syntax and JNI worker/thread checks pass.
- Native i686 SDL build completes and runs 600 offscreen frames with real
  game assets and live accelerometer delivery, exiting normally.
- User confirmed the live build fixes hit detection and restores the skybox
  in the opening cutscene. The skybox result is a user visual observation.
- Live device movement changes delivered acceleration, and the original game
  input handler consumes it. A later motion-gated level has not been played
  through as part of this verification.

Tests:

```
python3 source/tests/test_vfp_call_state.py
python3 source/tests/test_linux_accel.py
python3 source/tests/check_m6_host_c_syntax.py
```

The VFP test requires pyelftools and gcc; the host checks require an i686
compiler environment. No original assets are needed for these tests.

Sensor conventions: [Android sensor overview](https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview)
and [Linux IIO ABI](https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-iio).
