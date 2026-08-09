---
type: Architecture
title: OpenXR controller to XInput pipeline
description: How motion-controller actions become Cyberpunk 2077 gamepad input and how to diagnose PSVR2 failures.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/core/vr_core.cpp
tags: [openxr, xinput, controllers, psvr2, input]
timestamp: 2026-08-08T03:27:00Z
---

# Pipeline

1. `OpenXRManager::Init` creates one gameplay action set and suggests bindings for Oculus Touch, Valve Index, Vive, WMR, and KHR simple-controller profiles.
2. The XR frame loop calls `xrSyncActions`, queries per-hand poses, sticks, triggers, grips, clicks, face buttons, and menu, and publishes a `VRControllerState` snapshot. The combined menu action is published as a raw held/not-held state.
3. `HookedXInputGetCapabilities` advertises a virtual gamepad on XInput user 0.
4. `HookedXInputGetState` merges the snapshot into that virtual gamepad. It owns the UEVR-style `SystemButton` timer so a release before 500 ms emits one XInput Start event and a hold of at least 500 ms emits one XInput Back event without also emitting Start on release.
5. Cyberpunk consumes its normal gamepad bindings; CET/native modules read additional grip and gesture values from shared slots.

For [PSVR2 through SteamVR](/hardware/psvr2-steamvr.md), the active compatibility profile is expected to be Oculus Touch. Capacitive actions also provide [Triangle-touch D-pad shifting](/fixes/psvr2-triangle-dpad.md).

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
- Grips still feed holster/smoking behavior and do not accidentally throw grenades.
- PSVR2 Create/SystemButton quick release opens Start/system-pause; a ≥500 ms hold opens Back/in-game menu and release does not also open Start.
- Triangle-touch + R3 follows the same timing; bare R3/full-down still crouches.
- A physical XInput pad can still augment the VR state.
