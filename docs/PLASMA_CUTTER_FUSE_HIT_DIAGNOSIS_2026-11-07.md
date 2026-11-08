# Dead Space Mobile Linux — plasma cutter fires, fuse does not react

## User hardware evidence, 2026-11-07 18:30:58 PKT

From user-supplied `20261107-183058-*.log` on Linux (Mesa Intel UHD Graphics 600, 1366x768 panel):

- Exit code 0, JNI initialized, game ran frames.
- `[AIM_SURFACE]`: initial drawable 1366x731 (window decoration); later `[AIM_VIEWPORT]` reports GL viewport 1366x768 and game camera aspect 1.77864587. A difference at initial setup is **not** proof of the aim failure after window resize.
- Reanimated-style touch thresholds: pixel 10→15 and double tap 30→46 at 731 drawable height. These are gesture-recognition thresholds; not bullet collision geometry.
- Ten calls to **actual native** `WeaponPlasmaCutter::onFire` at RVA 0x001f6344 during user's fuse test.
- Four pairs of `GameObjectShootSwitchCovered::onHit` and `GameObjectShootSwitch::onHit`, all `event_type=0`, **preceded** the first logged `onFire`. Therefore they do NOT establish that any fired shot collided with the fuse.
- No `[AIM_RAYCAST]`, `[AIM_RAY_HIT]`, `[AIM_IMPACT]`, or `[AIM_POST_HIT]` during the shots. This by itself does **not** prove no physics hit: the original plasma-cutter onFire implementation invokes another lower-level ray function directly.

## Tutorial sequence correction (user-confirmed)

The user clarified that the **first four tutorial fuses are cut with the
plasma saw**, while **subsequent fuses require the plasma cutter**.
Earlier troubleshooting incorrectly treated all fuses as plasma-cutter
targets and thus missed the distinction between melee and firearm hits.

The log shows exactly four pairs of `CoveredShootSwitch::onHit` /
`ShootSwitch::onHit`, for **four distinct** original game-object addresses:

- `a20d9e20`
- `a20e7aa8`
- `a20e53a0`
- `a20d5c28`

All appear **before** the first `WeaponPlasmaCutter::onFire` entry.
They are consistent with the plasma-saw fuse stage, though the logs
alone do not identify the attacker class for each callback.

Ten plasma-cutter `onFire` entries follow. **No further fuse onHit
callbacks follow them in the uploaded trace.** This narrows the softlock
to the **plasma-cutter-required fuse stage**, not broken collision
registration for all switches. It still does not establish whether those
ten shots actually struck ordinary wall geometry.

Do not change the original fuse eligibility rules, enlarge projectile
hitboxes, simulate fuse destruction, or conflate tap tolerance with
physics collision. Compare the actual plasma-cutter physics-ray return
values and valid post-hit dispatch with those earlier successful
switch interactions. Attribute earlier callbacks to the plasma saw
only as contextually consistent, pending direct attacker/source logging.

## Original game code — reliable diagnosis

Original `WeaponPlasmaCutter::onFire` decompiled from the genuine ELF:
`f_001f6344` calls `f_0023e12c` **five** times to perform lower-level physics collision checks and tests the callback result at `sp+28`. This count was confirmed by the exact generated original function in [failed diagnostic run #42377956531](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/42377956531); the tracer failed closed because it had assumed only two calls. Corrected now to cover all five.

Original `GameObjectShootSwitch::onHit` RVA `0x0013264c` distinguishes hit-event types and source-object categories, with an early branch for event type 5 and other conditions, rather than simply opening for every callback. `GameObjectShootSwitchCovered::onHit` RVA `0x00133278` also has eligibility checks. The previous `event_type=0` messages are not enough to prove correct gunfire collision or a game-state transition.

## New read-only diagnostic

`source/experimental/lifted_milestone6/tools/shot_hit_trace.py` now instruments the actual `f_0023e12c` ray-entry routine and, specifically within `WeaponPlasmaCutter::onFire`, all **five** original calls to it, before and after invocation.

With `DS_AIM_DIAG=1`, check:

- `[AIM_FIRE]` — weapon firing path invoked.
- `[AIM_SHOT_RAY]` — originating world, from/to world-space coordinates, callback pointer.
- `[AIM_PHYSICS_WORLD_RAY]` — physics ray path reached.
- `[AIM_SHOT_RESULT]` — callback hit pointer after physics ray completed.
- `[AIM_IMPACT]`, `[AIM_POST_HIT]` — weapon impact processing reached.
- `[AIM_FUSE_HIT]` — fuse callback and event type; distinguish before/after shots.

These probes do **not** modify hitboxes, collision masks, aim coordinates, projectile behavior, or game state.

## Acceptance criteria before claiming a fix

1. Reproduce on user's real touchscreen device at the fuse with the new diagnostic binary. Fire at ordinary wall geometry and the red fuse.
2. If every ray callback is null: inspect original physics world's collision registration/masks for loaded scene and ray direction/length. Do not auto-open the door.
3. If the ray collides with world geometry but misses the fuse: inspect transformed camera/gun orientation and the fuse collider/mask.
4. If fuse `onHit` runs *after* firing but rejects event: diagnose eligibility and projectile object/type, using the original `onHit` code.
5. Test original 480p viewport mode and 1366x768 mode only if ray endpoint diagnostics actually implicate projection/aim offsets.

Never mark fuse hit/progression verified based on frame-loop completion, animation, or `onFire` alone.
