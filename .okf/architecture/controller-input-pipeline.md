---
type: Architecture
title: OpenXR controller to XInput pipeline
description: How motion-controller actions become Cyberpunk 2077 gamepad input and how to diagnose PSVR2 failures.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Hooks/XInput.cpp
tags: [openxr, xinput, controllers, psvr2, input]
timestamp: 2026-08-23T11:40:00+09:00
---

# Pipeline

1. `OpenXRManager::Init` creates one gameplay action set and suggests bindings for Oculus Touch, Valve Index, Vive, WMR, and KHR simple-controller profiles.
2. The XR frame loop calls `xrSyncActions`, queries per-hand poses, sticks, triggers, grips, clicks, face buttons, and menu, and publishes a `VRControllerState` snapshot. The combined menu action is published as a raw held/not-held state.
3. `HookedXInputGetCapabilities` advertises a virtual gamepad on XInput user 0.
4. `HookedXInputGetState` merges the snapshot into that virtual gamepad. It owns the UEVR-style `SystemButton` timer so a release before 500 ms emits one XInput Start event and a hold of at least 500 ms emits one XInput Back event without also emitting Start on release.
5. Cyberpunk consumes its normal gamepad bindings; CET/native modules read additional grip and gesture values from shared slots.

For [PSVR2 through SteamVR](../hardware/psvr2-steamvr.md), the active compatibility profile is expected to be Oculus Touch. Capacitive actions also provide [Triangle-touch D-pad shifting](../fixes/psvr2-triangle-dpad.md).

# Synthesised buttons collide with the game's own chords

Step 4 above emits real XInput buttons, and the game's bindings do not know they were synthesised.
`XInput.cpp` emits `LEFT_THUMB` for **sprint** and `RIGHT_THUMB` for the **crouch** gesture and for
the scanner **tag** — and it emits them **in the same frame** when the gestures overlap. The game
declares a chord `IK_PAD_LR_THUMB` (`buttonGroup`, `timeWindow="0.1f"`) meaning both stick clicks
together, so any same-frame pair satisfies it outright and the time window offers no protection.
Stock, that chord opened **photo mode**, which sliding therefore triggered by itself.

**Before adding or reusing a synthesised button, check the merged cache for chords that contain it.**
The declarations live in `r6\cache\inputUserMappings.xml`; a `buttonGroup` is a chord definition and
grepping for its id finds both the definition and every mapping that binds it.

## input_loader replaces on a name collision

Measured 2026-08-26, and it is the general rule for this layer, not a fact about one binding. A mod
XML in `r6\input\` whose `mapping name=` matches an existing one **replaces** the game's entry
wholesale — it is not merged into it and the game's buttons are not retained. Verified by shipping
`mods/config/input/CyberpunkVRPort_NoPadPhotoMode.xml` with `TogglePhotoModeButton` carrying only
`IK_N`, then reading the merged cache: exactly one such mapping, one button, and no mapping anywhere
still binding `IK_PAD_LR_THUMB`.

This is worth stating because input_loader's own log shows **only merge operations and never a
removal**, which invites the opposite conclusion. Two consequences:

* A binding can be removed from the pad without touching the port's gesture synthesis — the correct
  fix when suppressing the input itself would cost a real move (here, suppressing the two clicks
  would have cost **slide**, which *is* sprint plus crouch).
* Redeclaring a name means owning **every** button on it. Anything omitted is gone, so the game's
  own bindings must be copied forward deliberately if they are still wanted.

One further property of the same cache, learned the hard way and easy to reach for when a binding
misbehaves: **it is not self-healing.** Deleting the merged XML does not force a clean rebuild — it
silently drops every mod binding instead. Do not treat deletion as a reset. To re-measure a change,
launch once and read the cache; to undo one, remove the mod XML from `r6\input\` and launch again.

# Early XInput hook and falsified capabilities hypothesis

The first PSVR2 retest proved both hands reached the Oculus Touch interaction profile and logged a non-zero Sense action, but Cyberpunk remained in keyboard mode at the first screen. A device-capabilities cache was initially suspected.

The CP2077 2.31 PE import table and the next runtime log falsified that specific hypothesis: the executable imports only `XInputGetState` from `XINPUT9_1_0.dll`, not `XInputGetCapabilities`. The state IAT hook is installed from the RED4ext load callback, after `vrport.ini` is read but before the worker thread's eight-second delay. A capabilities hook remains optional for other builds, but `capabilities=0` is normal on 2.31 and must not mark installation failed.

# Packet-number defect fixed in the PSVR2 pass

The old XInput merge incremented `dwPacketNumber` only when VR buttons or triggers changed. It did not include either stick. When no physical gamepad was connected, the real XInput call returned disconnected and the hook zeroed the state on every poll; the temporary packet could increment for one call and then fall back to zero.

Consumers that use `dwPacketNumber` to avoid reprocessing unchanged input could therefore miss stick-only movement and held controls. The fix compares the complete final merged `XINPUT_GAMEPAD`, maintains a monotonic synthetic packet stream, and protects it with an SRW lock for multi-threaded polling.

# Diagnostic order

Check the pipeline in this order:

1. `xr_input_actions=1` and `xr_xinput_hook=1` in `bin/x64/vrport.ini`.
2. `XInput: IAT hooks state=1` in `cyberpunkvrport.log`. CP2077 2.31 does not import `XInputGetCapabilities`, so `capabilities=0` is expected and non-fatal.
3. `XInput: game began polling virtual VR gamepad state on user 0`.
4. Successful suggested bindings for the expected interaction profile.
5. The active profile logged after the OpenXR interaction-profile-changed event.
6. Non-zero Sense action confirmation.
7. One-time `XInput: merged VR input` lines confirm buttons, triggers, and sticks reached the game's poll.
8. In-game movement/buttons and a monotonic packet number.

A generated SteamVR binding JSON and successful suggested bindings are setup evidence, not proof of active controller samples. In the 2026-08-08 retest, one-time game-poll diagnostics proved right trigger (`RT=20`), Cross/A (`buttons=0x1000`), and stick input reached Cyberpunk. Missing weapon visuals were therefore not evidence of a trigger transport failure.

# Regression checklist

- Left stick moves and strafes without another button changing.
- Right stick turns; R3/full-down still crouches.
- Triggers and all four face buttons map correctly.
- Grips still feed holster/smoking behavior and do not accidentally throw grenades. Right grip is RB only in menus; in gameplay it remains a mod gesture rather than a globally merged gamepad shoulder.
- PSVR2 left grip reaches physical reload immediately. If reload has not claimed the left wrist, a sustained 180 ms hold becomes scanner anywhere; the old ear requirement is disabled for PSVR2.
- While mounted, left-grip scanner output is suppressed so the raw grip remains available to steering. A grip click near the animated wheel/handlebar toggles that hand onto the driving animation; a second click releases it, with no continuous squeeze required.
- Changing an active steering grab from two hands to one—especially when drawing a weapon—uses continuity pickup: the remaining hand inherits the previous steering angle instead of recomputing in a different frame and pulling left or right. `xr_wheel_steer_max_deg` controls sensitivity; lower values require less movement.
- With a gun drawn in the driver seat, RT fires through vehicle-combat RB and throttle latches at its current value. R3 is exclusively consumed as an edge-triggered throttle idle/restore toggle; the click is also suppressed from physical-reload publication and `VehicleInverseCameraToggle_Button`. Left-stick Y is no longer consumed for throttle trim.
- In a vehicle, Sense Circle passes through as native XInput B / `ExitVehicle_Button`, while Sense Square remains X / horn. The port must not mask B or translate a held X into B. See the [mounted vehicle interaction pipeline](vehicle-interaction-pipeline.md).
- PSVR2 Create/SystemButton quick release opens Start/system-pause; a ≥500 ms hold opens Back/in-game menu and release does not also open Start.
- Triangle-touch + R3 follows the same timing; bare R3/full-down still crouches.
- A physical XInput pad can still augment the VR state.
