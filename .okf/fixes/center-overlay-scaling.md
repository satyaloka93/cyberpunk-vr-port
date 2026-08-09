---
type: Fix
title: Dynamic center-overlay scaling
description: Shrink and reposition scanner, quickhack, interaction, and game-information overlays that bypassed the named HUD-region map.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/mods/cet/CyberpunkVRPort_HUD/init.lua
tags: [hud, psvr2, vr, scanner, quickhack, popups, cet]
timestamp: 2026-08-09T15:16:00+09:00
---

# Failure

Named HUD roots such as `TopRightMain`, `BottomRightMain`, and `RightCenter` already receive the port's half-size VR transform. Dynamic scanner, quickhack, interaction, and game-information layers remained oversized and could extend beyond the headset's readable region.

The installed `hud_live_debug.log` showed why: many dynamic layers arrive as generic root children named `HUDMiddleWidget` with `inkEAnchor.Centered`. The region classifier ignored centered generic children, so their original `1.0` scale survived while mapped regions used `appliedRegionScale(1.0) == 0.5`.

# Fix

A new `Center overlays` X/Y/Size region covers:

- centered `HUDMiddleWidget` roots;
- `cursor_device`;
- the generic `TopCenter` and `BottomCenter` roots.

Its persisted keys are:

```text
xr_hud_center_overlay
xr_hud_center_overlay_y
xr_hud_center_overlay_scale
```

The default Size is `1.0`, which follows existing HUD semantics and applies a `0.5` widget scale. It can be tuned live under **F10 → HUD → Center overlays**. Existing `hud_layout.ini` files remain valid: absent keys fall back to zero offsets and Size `1.0`.

# Test build

Deployed locally for retest:

- `CyberpunkVR_Stereo.dll` SHA-256: `78190018f2df5c4bf81e7ed870a1a264d3b1f87f9541f25a251a7bf63814aed5`
- CET HUD script SHA-256: `5244fb1bc1160ff42c59e75867d9dad8bbc8abe03245bdf509b6107666e95046`

The deployment preserved both `vrik_calibration.ini` and the existing `hud_layout.ini` byte-for-byte.

# Regression checks

1. Enter scanner mode and confirm the scanner information remains centered and readable.
2. Open the quickhack chooser and confirm all entries fit in view.
3. Trigger interaction and game-information popups and confirm they use the same size control.
4. Verify crosshair/cursor behavior; these generic center roots intentionally share the region until individual controller identities are captured.
5. Confirm named minimap, health, quest, and corner regions retain their prior settings.
