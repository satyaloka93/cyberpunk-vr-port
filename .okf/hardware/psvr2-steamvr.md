---
type: Hardware Compatibility Profile
title: PlayStation VR2 through SteamVR/OpenXR
description: Measured PSVR2 runtime geometry and controller-profile behavior for the CyberpunkVR port.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/tree/psvr2-tweaks
tags: [psvr2, steamvr, openxr, controllers, resolution]
timestamp: 2026-08-09T13:00:00+09:00
---

# Runtime identity

On the tested PSVR2 PC-adapter setup, SteamVR reports:

```text
runtimeName = SteamVR/OpenXR
systemName = SteamVR/OpenXR : playstation_vr2
recommended per-eye image = 3400 x 3468 at the tested 100% scale
maximum swapchain image = 8192 x 8192
```

The recommendation can change with SteamVR resolution scaling; `3400x3468` is measured evidence, not a universal hardcoded target.

# Render shape

The measured recommendation has aspect `3400/3468 = 0.98039`. The runtime lens FOV observed by this port was approximately `104.95° x 106.08°`; the tangent-space aspect is about `0.97974`, confirming that the recommended shape is coherent.

The 2026-08-08 stereo retest reported runtime IPD `0.0590 m`; `PatchCamera` diagnostics consistently recovered `horiz=0.0587–0.0590 m` after removing head motion, with `xr_stereo_scale`, `xr_world_scale`, and `xr_ipd_scale` all at `1.0`. Thus the implemented geometric eye separation matched the runtime, although perceived strength/convergence still requires a qualitative headset test.

The shipped archive has no exact PSVR2-shaped VRCAM asset. The PSVR2 launcher ladder therefore uses existing square assets. Square is 2.0% wider than the measured ideal and is the closest currently authored shape. The previously selected Pimax Dream Air `3072x2536` shape is 23.6% wider than PSVR2's measured runtime shape.

See [VRCAM resolution catalogue](/architecture/vrcam-resolution-catalog.md) before adding an exact `3400x3468` option.

# Sense controllers

SteamVR currently exposes the PSVR2 Sense controllers to this OpenXR application through:

```text
/interaction_profiles/oculus/touch_controller
```

The SteamVR-generated CyberpunkVR binding maps:

| Sense control | OpenXR compatibility path | XInput/game label |
|---|---|---|
| Left Square | left `x/click` | X |
| Left Triangle click | left `y/click` | Y |
| Left Triangle capacitive touch | left `y/touch` | [D-pad shift modifier](/fixes/psvr2-triangle-dpad.md) |
| Right Cross | right `a/click` | A |
| Right Circle | right `b/click` | B |
| Sticks | `thumbstick` | left/right sticks |
| Triggers | `trigger/value` | LT/RT |
| Grips | `squeeze/value` | mod grip channels |
| Left Create | manual SteamVR assignment to app `SystemButton` | Tap: XInput Start/system-pause; hold ≥500 ms: XInput Back/in-game menu |
| Right Options / actual system function | reserved by SteamVR | Not an in-game action |
| Triangle capacitive touch + R3 | `y/touch` + `thumbstick/click` | Same tap/hold state machine as fallback |

Do not assume `/interaction_profiles/sony/playstation_vr2_controller` is active. Local UEVR experiments found SteamVR returning `XR_ERROR_PATH_UNSUPPORTED` for that profile while presenting the device as Oculus Touch.

# Diagnostics

The modified plugin logs three progressively stronger facts:

1. `OpenXRManager[PSVR2]: headset detected` from the system name.
2. `OpenXRManager[Input]: active profile ...` after `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`.
3. `OpenXRManager[PSVR2]: Sense controller actions confirmed` after the first non-zero action state.

A successful `xrSuggestInteractionProfileBindings` alone does not prove physical input is reaching the application. In the 2026-08-08 `vrserver.txt`, SteamVR enumerated `menu`, `system`, and `thumbrest` from the Oculus binding but omitted all three while auto-remapping to `playstation_vr2_sense`. However, the user successfully bypassed auto-remapping in SteamVR's binding editor by assigning physical left Create directly to the port's global `SystemButton` application action. The port now interprets that action like UEVR: release before 500 ms emits XInput Start; a hold of at least 500 ms emits XInput Back once and suppresses Start on release. Those are Cyberpunk application menus; the action does not expose SteamVR's reserved dashboard.

# VRIK grip-pose correction

The Oculus-emulated Sense grip pose tilted both avatar hands downward with the generic defaults. A successful user calibration added about `+26°` wrist pitch on both sides while retaining `-90°` yaw: right `(pitch=27.4, yaw=-90, roll=0)`, left `(pitch=-154.3, yaw=-90, roll=0)` where left `-154.3°` is the mirrored equivalent relative to its `-180°` default. It is persisted in `red4ext/plugins/CyberpunkVR_Stereo/vrik_calibration.ini`; treat it as measured PSVR2 guidance, not yet a universal default.

# Controller contact loss

The same `vrserver.txt` independently confirmed a driver/tracking failure, not a mod input failure: repeated `[CONT_L/R] Lost`, `The frame is stuck` for up to nine seconds, controller tracker drops reaching 1813 frames, and later controller `Boot` recovery events. Check charge, room lighting/occlusion, Bluetooth signal/interference, firmware, and USB/Bluetooth power management before debugging game mappings.

# Related code

- [`src/vr/overlay/launcher_dialog.cpp`](https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/overlay/launcher_dialog.cpp)
- [`src/vr/openxr/openxr_manager.cpp`](https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/openxr/openxr_manager.cpp)
- [Controller input pipeline](/architecture/controller-input-pipeline.md)
- [PSVR2 Triangle-touch D-pad shifting](/fixes/psvr2-triangle-dpad.md)
- [Optional adaptive triggers and grip haptics](/operations/psvr2-adaptive-triggers.md)
