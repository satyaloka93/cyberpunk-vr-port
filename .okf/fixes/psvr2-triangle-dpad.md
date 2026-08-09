---
type: Fix
title: PSVR2 Triangle-touch D-pad shifting
description: UEVR-compatible left-touch shifting for PSVR2 Sense through SteamVR's Oculus Touch profile.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/openxr/openxr_frameloop.cpp
tags: [psvr2, steamvr, openxr, input, dpad, bindings]
timestamp: 2026-08-08T03:40:00Z
---

# Expected control

Touch and hold **Triangle without clicking it**, then move the right stick to emit D-pad up, down, left, or right. The right-stick axes are zeroed for the whole modifier hold so smooth or snap turn cannot fire while selecting.

Native Oculus Touch controllers use left thumbrest touch for the same behavior. L3 remains a fallback modifier; releasing L3 without selecting a direction emits the normal sprint click.

# Why Triangle touch is the PSVR2 modifier

SteamVR exposes Sense through `/interaction_profiles/oculus/touch_controller`, but does not reliably expose the left thumbrest input expected by UEVR's `LEFT_TOUCH` method. Triangle capacitive touch arrives as the Oculus left secondary-button path:

```text
/user/hand/left/input/y/touch
```

The plugin therefore mirrors that action into the left D-pad-shift modifier only when the runtime system is [PSVR2](../hardware/psvr2-steamvr.md). Triangle click remains XInput Y/weapon switch.

SteamVR's generated Oculus binding describes left Create as Menu and right Options as System, but the tested Oculus-to-`playstation_vr2_sense` auto-remapper omitted both paths. Separate actions did not deliver state. A single UEVR-style global `SystemButton` action made the application action manually assignable in SteamVR; mapping physical left Create to it successfully delivered state. UEVR itself has no separate OpenXR Start/Menu action: its one `/actions/default/in/SystemButton` accepts both wildcard `system/click` and `menu/click` paths, then translates a short release to XInput Start and a hold of at least 500 ms to XInput Back/Select. The Cyberpunk port now implements the same timing in the XInput hook, where the one-shot edge cannot fall between game polls. Triangle capacitive touch plus R3 feeds the same state machine as a binding-independent fallback. In both cases, tap opens Cyberpunk's system/pause menu and hold opens its Back/in-game menu; neither exposes SteamVR's reserved dashboard. Bare R3 remains XInput right-thumb/crouch.

This follows the proven UEVR behavior recorded in `UEVR-AFW-PUBLISH/.okf/fixes/psvr2-triangle-dpad.md`: Triangle touch substitutes for `ThumbrestTouchLeft`, bypassing the Oculus touch-inactivity assumption, and suppresses turning while shifting.

# OpenXR actions and editing

The Oculus Touch suggestion exposes both:

- `D-pad Shift Thumbrest Touch` on left/right `thumbrest/touch`.
- `Secondary Button Touch` on left `y/touch` and right `b/touch`.

SteamVR generates these as editable OpenXR actions. Physical sources can be changed under **SteamVR → Settings → Controllers → Manage Controller Bindings** for the CyberpunkVR OpenXR application. Cyberpunk's own controller menu edits what the resulting XInput D-pad does, not which Sense sensor feeds the OpenXR action.

# Validation

A PSVR2 session should log:

```text
OpenXRManager[Input]: suggest bindings /interaction_profiles/oculus/touch_controller -> 0 (count=22)
OpenXRManager[PSVR2]: Triangle-touch D-pad modifier active.
OpenXRManager[PSVR2]: Triangle-touch + R3 menu fallback active (tap=Start, hold=Back).
```

Regression test:

1. Rest a finger on Triangle without clicking; move the right stick in all four directions.
2. Confirm the game receives D-pad events and does not turn.
3. Release Triangle and confirm right-stick turning returns.
4. Click Triangle normally and confirm weapon switch/Y still works.
5. Tap/release Create in under 500 ms; confirm XInput Start opens the system/pause menu.
6. Hold Create for at least 500 ms; confirm one XInput Back event opens the in-game menu and releasing it does not also emit Start.
7. Repeat both timing tests with Triangle-touch + R3; confirm bare R3/full-down right stick still crouches.
8. Hold L3 plus right stick and confirm fallback shifting still works.
9. Tap/release L3 alone and confirm sprint remains available.
