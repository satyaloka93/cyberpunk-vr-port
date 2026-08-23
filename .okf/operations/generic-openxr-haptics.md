---
type: Runtime Integration
title: Generic OpenXR controller haptics
description: Quest/Touch and other non-PSVR2 controllers receive gun, melee, and audio-derived vehicle vibration through the application's OpenXR session, while PSVR2 remains exclusively owned by the Toolkit bridge.
tags: [openxr, quest-3, touch, haptics, recoil, melee, vehicle, audio, input]
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

If several events arrive between XR frames, the queue keeps the strongest amplitude and longest duration per hand. Events are dropped while unfocused rather than replayed late when focus returns. Session shutdown stops the WASAPI worker and both outputs, then clears pending requests.

A separate worker uses Windows WASAPI loopback on the default multimedia render endpoint. It never calls OpenXR. It publishes only a recent two-hand amplitude envelope; the XR frame owner expires that envelope after 100 ms and applies it with 45 ms requests refreshed each frame. An event pulse establishes a per-hand hold-until time, preventing the next low engine update from replacing and truncating a gunshot or melee impact.

# Event sources

- `RecoilOnShot` is the measured, refractory once-per-round signal already used by the visual hand-recoil spring. It avoids false feedback from an empty revolver cylinder, reload trigger suppression, held throttle, or merely crossing an input threshold.
- Gun amplitude and duration follow `CyberpunkVR_WeaponKickDeg`; a two-handed weapon adds a weaker `0.55x` support-hand pulse only while the existing two-hand grip state is active.
- `SetVRHapticPulse` routes physical melee swing episodes and confirmed blade impacts to OpenXR on non-PSVR2 systems while continuing to publish the stable `[157..160]` bridge record for PSVR2.
- While `g_isDriving` is true, the audio worker filters the default output to the PSVR2 bridge's 28–320 Hz low band. Low-band RMS becomes sustained engine/road body; fast-minus-slow full-band energy becomes a stronger shift/impact transient. Both cars and bikes use the same measured audio path.

Gear feedback is audio-derived, as it is in the PSVR2 bridge—not a claim that an internal transmission event was found. Music, voice chat, or another application sent to the same default endpoint can contribute while driving. Quest reduces the source to OpenXR's amplitude/duration model; it cannot reproduce Sense PCM waveform detail.

# Gain and dynamic range

`xr_openxr_haptic_gain` is live and persisted through `F10 → Controls → OpenXR controller haptic gain`. Range is `0..2`; `0` disables all generic output and the default is `1.25`.

`xr_openxr_vehicle_haptic_gain` is an independent `0..2` multiplier, exposed as `OpenXR vehicle audio rumble`, default `1.0`. Zero disables only engine/road/shift audio while gun and melee events remain active.

The global increase is deliberately modest. Quest has simple controller vibration rather than adaptive-trigger resistance or Sense PCM, but blindly forcing every event to amplitude `1` would erase the per-weapon ladder and flatten engine shifts. Gain is applied after source strength is chosen and clamped only at the final OpenXR amplitude.

# Quest D-pad modifier

The Oculus Touch binding already exposes `/user/hand/left/input/thumbrest/touch`. Touching the Quest left thumbrest sets the D-pad shift modifier; the right stick selects direction and is masked from camera turning for the complete hold. PSVR2 uses Triangle touch through SteamVR's Oculus mapping, while left-stick click remains the fallback on every runtime. The first non-PSVR2 thumbrest activation logs `left thumbrest D-pad shift confirmed`.

# Regression checks

1. Quest 3: fire a light pistol and a heavy revolver; right-hand pulses occur once per actual round and remain distinguishable.
2. With two-hand grip active, firing adds a weaker left-hand pulse; releasing the support hand removes it.
3. Swing a melee weapon without contact, then hit a target; the confirmed impact is stronger/longer.
4. Drive a car and a bike: idle/acceleration produce continuous two-hand texture and audible shifts produce stronger bumps. Exiting the vehicle stops it within 100 ms plus the final 45 ms request.
5. Fire while driving: the event pulse remains distinct and is not cut short by the next engine refresh.
6. Touch the Quest left thumbrest and move the right stick in four directions; D-pad actions occur without camera turn.
7. PSVR2: start the Toolkit bridge and confirm `VR motion haptics protocol v1 active`; neither generic-active nor vehicle-audio-capture lines may appear.
8. Set `xr_openxr_vehicle_haptic_gain=0`; vehicle audio stops while gun/melee pulses remain. Set global gain to `0`; all Quest OpenXR vibration stops while controls and visual recoil remain unchanged.
