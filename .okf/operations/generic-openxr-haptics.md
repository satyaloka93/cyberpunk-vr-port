---
type: Runtime Integration
title: Generic OpenXR controller haptics
description: Quest/Touch and other non-PSVR2 controllers receive event-driven vibration through the application's OpenXR session, while PSVR2 remains exclusively owned by the Toolkit bridge.
tags: [openxr, quest-3, touch, haptics, recoil, melee, input]
timestamp: 2026-08-23T16:10:00+09:00
---

# Backend boundary

Quest/Touch does not use DSX or PSVR2Toolkit. The port creates one `XR_ACTION_TYPE_VIBRATION_OUTPUT` action with left/right subaction paths and suggests `/user/hand/left/output/haptic` and `/user/hand/right/output/haptic` for Oculus Touch, Valve Index, HTC Vive, Windows MR, and KHR simple-controller profiles.

The actuator rule is automatic and mutually exclusive:

| Detected system | Output owner |
|---|---|
| PSVR2 | [PSVR2Toolkit bridge](psvr2-adaptive-triggers.md); generic OpenXR requests are rejected before queueing |
| Quest/Touch and other non-PSVR2 controllers | The port's attached OpenXR action; DSX and PSVR2Toolkit do not run |

Creating and binding the output action on PSVR2 is inert. `QueueOpenXRHapticPulse` checks `IsRuntimePsvr2()` and cannot enqueue a Sense request, preserving the single-owner rule even if a script calls `SetVRHapticPulse`.

# Thread and lifecycle model

CET melee events and the native weapon-shot hook run outside the XR frame owner. They place abstract `(hand, amplitude, duration)` requests into a mutex-protected two-hand queue. `PumpOpenXRHaptics` consumes it after `xrSyncActions` on the XR frame thread and invokes `xrApplyHapticFeedback` only while the session is focused.

If several events arrive between XR frames, the queue keeps the strongest amplitude and longest duration per hand. Events are dropped while unfocused rather than replayed late when focus returns. Session shutdown stops both outputs and clears pending requests.

# Event sources

- `RecoilOnShot` is the measured, refractory once-per-round signal already used by the visual hand-recoil spring. It avoids false feedback from an empty revolver cylinder, reload trigger suppression, held throttle, or merely crossing an input threshold.
- Gun amplitude and duration follow `CyberpunkVR_WeaponKickDeg`; a two-handed weapon adds a weaker `0.55x` support-hand pulse only while the existing two-hand grip state is active.
- `SetVRHapticPulse` routes physical melee swing episodes and confirmed blade impacts to OpenXR on non-PSVR2 systems while continuing to publish the stable `[157..160]` bridge record for PSVR2.

Road texture, engine vibration, and full-game audio are not synthesized by this first generic backend. Those remain rich PSVR2 bridge features, not claims of the Quest implementation.

# Gain and dynamic range

`xr_openxr_haptic_gain` is live and persisted through `F10 → Controls → OpenXR controller haptic gain`. Range is `0..2`; `0` disables output and the default is `1.25`.

The increase is deliberately modest. Quest has simple controller vibration rather than adaptive-trigger resistance or Sense PCM, but blindly forcing every event to amplitude `1` would erase the per-weapon ladder. Gain is applied after event strength is chosen and clamped only at the final OpenXR amplitude.

# Quest D-pad modifier

The Oculus Touch binding already exposes `/user/hand/left/input/thumbrest/touch`. Touching the Quest left thumbrest sets the D-pad shift modifier; the right stick selects direction and is masked from camera turning for the complete hold. PSVR2 uses Triangle touch through SteamVR's Oculus mapping, while left-stick click remains the fallback on every runtime. The first non-PSVR2 thumbrest activation logs `left thumbrest D-pad shift confirmed`.

# Regression checks

1. Quest 3: fire a light pistol and a heavy revolver; right-hand pulses occur once per actual round and remain distinguishable.
2. With two-hand grip active, firing adds a weaker left-hand pulse; releasing the support hand removes it.
3. Swing a melee weapon without contact, then hit a target; the confirmed impact is stronger/longer.
4. Touch the Quest left thumbrest and move the right stick in four directions; D-pad actions occur without camera turn.
5. PSVR2: start the Toolkit bridge and confirm `VR motion haptics protocol v1 active`; no `OpenXRManager[Haptics]: generic ... active` line may appear.
6. Set `xr_openxr_haptic_gain=0`; Quest receives no OpenXR vibration while controls and visual recoil remain unchanged.
