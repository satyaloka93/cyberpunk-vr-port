---
type: Architecture
title: Mounted vehicle interaction pipeline
description: How vehicle classification, seated VRIK, manual steering, gun controls, and car/bike offsets cooperate in the upstream 0.1.3 PSVR2 port.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Anim/WheelGrab.cpp
tags: [vehicles, vrik, steering, psvr2, input]
timestamp: 2026-08-23T11:40:00+09:00
---

# State and classification

`Hooked_LocateCamera` resolves the player's `mountedVehicle` property. A non-null handle sets `g_isInVehicle`; walking the mounted object's RTTI parent chain identifies `vehicleBikeBaseObject` and publishes `g_isOnBike`. Driving is narrower than mounted/passenger state and is published separately as `g_isDriving`.

The camera path selects independent live offsets:

- Cars and other non-bike mounts use `xr_veh_head_offset_x/y/z`.
- Motorcycles use `xr_bike_head_offset_x/y/z`.
- All seated offsets are ignored immediately on foot.

The local test profile starts motorcycle Z at `+0.050 m`; packaged defaults remain zero until a value is validated broadly.

# Seated skeleton ownership

While mounted, VRIK is **arms-only**. Cyberpunk's authored vehicle animation owns the torso, hips, spine, legs, and seated camera relationship. Letting the full-body solver place the body under the HMD fought that authored pose and caused incorrect vehicle-body placement.

Do not apply a fixed 90° or 180° skeleton rotation. Heading diagnostics compare untouched animated-body forward against both the game camera and final render view four times after entry. A value near `1` is aligned, `0` is sideways, and `-1` is backwards. Tests have shown both transient entry disagreement and later alignment, so correction must be based on settled samples rather than one transition frame.

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
7. Watch gun and hand rendering in both eyes. Intermittent flashing remains under investigation; do not attribute it to VRCAM, pacing, or the skeleton without a matched reproduction. The observed run had no GPU fault or stale-VRCAM event and the symptom varied between unchanged launches.

See the [controller input pipeline](controller-input-pipeline.md) and [PSVR2 SteamVR profile](../hardware/psvr2-steamvr.md).
