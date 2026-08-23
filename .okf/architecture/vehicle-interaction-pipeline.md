---
type: Architecture
title: Mounted vehicle interaction pipeline
description: How vehicle classification, seated VRIK, manual steering, gun controls, and car/bike offsets cooperate in the upstream 0.1.3 PSVR2 port.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Anim/WheelGrab.cpp
tags: [vehicles, vrik, steering, psvr2, input]
timestamp: 2026-08-23T17:34:00+09:00
---

# State and classification

`Hooked_LocateCamera` resolves the player's `mountedVehicle` property. A non-null handle sets `g_isInVehicle`; walking the mounted object's RTTI parent chain identifies `vehicleBikeBaseObject` and publishes `g_isOnBike`. Driving is narrower than mounted/passenger state and is published separately as `g_isDriving`.

The camera path selects independent live offsets:

- Cars and other non-bike mounts use `xr_veh_head_offset_x/y/z`.
- Motorcycles use `xr_bike_head_offset_x/y/z`.
- All seated offsets are ignored immediately on foot.

Packaged defaults remain zero. These values currently live in one shared `vrport.ini`; they are not headset-scoped. Switching between Quest/VDXR and PSVR2/SteamVR therefore retains the same car and motorcycle offsets, so preserve the current profile before calibrating a different headset.

# Seated skeleton ownership

While mounted, VRIK is **arms-only**. Cyberpunk's authored vehicle animation owns the torso, hips, spine, legs, and seated camera relationship. Letting the full-body solver place the body under the HMD fought that authored pose and caused incorrect vehicle-body placement.

Do not apply a fixed 90° or 180° skeleton rotation. Heading diagnostics compare untouched animated-body forward against both the game camera and final render view four times after entry. A value near `1` is aligned, `0` is sideways, and `-1` is backwards. Tests have shown both transient entry disagreement and later alignment, so correction must be based on settled samples rather than one transition frame.

A Quest 3/VDXR car test confirmed `mounted=1`, `bike=0`, and the arms-only branch. Final-view heading settled positive (`1.000`, `0.987`, `0.923`, `0.919`), so that run does not show the old full-body ownership regression or a backwards body. A complaint about bad **position** should first record whether the body is left/right, ahead/behind, or high/low and A/B the saved car offsets against zero; heading evidence alone cannot diagnose translation.

# Wheel and handlebar ownership

`WheelGrab` captures the engine's animated hand positions before VRIK writes. Per hand:

1. A fresh grip click must begin within `xr_wheel_radius` of the animated wheel/handlebar hand.
2. That click toggles the arm onto the driving animation; the physical button can be released.
3. The next click releases the arm wherever the controller has moved.
4. A drawn weapon blocks the right-hand wheel grab; the left hand remains available for steering.

The wheel-armed shared mask prevents grab/release clicks from also triggering scanner, holster, smoking, or reload behavior. PSVR2 left-grip scanner synthesis is independently suppressed for every mounted state.

Steering is derived from controller tilt: right-minus-left for two hands, or the remaining controller against the frozen animated wheel centre for one hand. `xr_wheel_steer_max_deg` controls sensitivity—lower requires less movement—and `xr_wheel_steer_dead_deg` rejects centre tremor. When drawing a weapon changes an active grab from two hands to left-only, pickup continuity preserves the previous steering angle rather than immediately pulling to one side.

# Native and gun-mode controls

Vehicle face buttons retain Cyberpunk's native contract:

- Sense Circle / XInput B: `ExitVehicle_Button`.
- Sense Square / XInput X: horn.

With a weapon drawn in the driver seat, the right trigger fires through vehicle-combat RB. Throttle latches at the value present when the weapon appears. R3 toggles that throttle between idle and its remembered nonzero value; if the weapon was drawn at idle, `xr_vehicle_throttle_trim` is retained as the compatibility key for the initial restore level. R3 is consumed before the inverse-camera binding and is withheld from physical-reload publication. Left-stick Y is not consumed.

# Regression checks

1. Enter a motorcycle and car separately; confirm the logged RTTI class and the correct F10 offset trio is live.
2. Confirm mounted logs say `VRIK arms-only` and collect all four settled heading samples.
3. Click each grip once at the wheel/handlebar, release the physical button, steer, then click again to detach.
4. Confirm left grip never opens scanner while mounted.
5. Draw a weapon while steering: the vehicle must not pull immediately, the right arm must release, and the left hand must retain steering.
6. Confirm RT fires, R3 toggles throttle off/on, Circle exits, and Square horns.
7. Watch gun and hand rendering in both eyes. The 17:22 Quest session reproduced hand+weapon disappearance/reappearance after drawing a gun in both `vehicleArmedCarBaseObject` and ordinary `vehicleCarBaseObject`. The body position and general driving were good. Stereo remained healthy (`28798/28800` submissions, two startup misses), VRCAM continued producing, and every sampled render mask retained `GeometrySkinned=MV` and `WeaponPlane=MV`; no GPU/device error appeared. This narrows the trigger to the mounted weapon/arm path rather than whole-view pacing, stale VRCAM, a particular car, or the removed low-spec INI, but it does not yet distinguish animation visibility/state from moved-mesh culling.
8. During active flashing, use `F10 → VRIK → Start VR hand tracking` as a live A/B without leaving the vehicle. If disabling tracking immediately stops the flashing while Cyberpunk's authored one-handed vehicle pose remains visible, investigate the arms-only solve, arm bounds, and per-pass pose ownership. If flashing continues unchanged, investigate the game's mounted weapon/arm visibility and render path instead. Re-enable tracking after observing the result; do not remove steering or arms-only body ownership based on this test.

The preserved session is under `%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260823-173244-quest-vehicle-gun-flash` (`cyberpunkvrport.log` SHA-256 `de695ff4…`).

See the [controller input pipeline](controller-input-pipeline.md) and [PSVR2 SteamVR profile](../hardware/psvr2-steamvr.md).
