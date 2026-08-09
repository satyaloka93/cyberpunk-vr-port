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

Shifted D-pad left/right pans both this region and the separate `RightCenter` quickhack-description root in synchronized `160`-pixel steps. Left moves the composition left to reveal information beyond its right lens edge; right moves it back. Each direction fires once until the stick recenters, and the normal XInput D-pad bit is preserved. The resulting X offsets are persisted and appear in the F10 `Center overlays` and `Right center` sliders. If an older test build moved only the center root, the first flick synchronizes the description panel to that existing offset rather than moving the chooser a second step.

The raw `dpadShiftActive` state is also carried from the OpenXR frame snapshot into the XInput hook. During the full modifier hold, the hook zeros the final merged right-stick X/Y axes—not only the OpenXR sample—so a Steam virtual or physical XInput source cannot move the external scanner target. Head-look remains available for target movement.

Its persisted keys are:

```text
xr_hud_center_overlay
xr_hud_center_overlay_y
xr_hud_center_overlay_scale
```

The default Size is `1.0`, which follows existing HUD semantics and applies a `0.5` widget scale. It can be tuned live under **F10 → HUD → Center overlays**. Existing `hud_layout.ini` files remain valid: absent keys fall back to zero offsets and Size `1.0`.

# Test build

Deployed locally for retest:

- `CyberpunkVR_Stereo.dll` SHA-256: `d32ce2f2d4f6c17a45b6a58cb8e772a3650c88d93c11eaa85233b228772c9b7c`
- CET HUD script SHA-256: `5244fb1bc1160ff42c59e75867d9dad8bbc8abe03245bdf509b6107666e95046`

The deployment preserved both `vrik_calibration.ini` and the existing `hud_layout.ini` byte-for-byte.

# Regression checks

1. Enter scanner mode and confirm the scanner information remains centered and readable.
2. Open the quickhack chooser and confirm all entries fit in view.
3. Trigger interaction and game-information popups and confirm they use the same size control.
4. Verify crosshair/cursor behavior; these generic center roots intentionally share the region until individual controller identities are captured.
5. Use shifted D-pad left/right and verify the chooser and right-side description move together, one pan step per recentered flick; confirm the game still receives D-pad left/right.
6. While the shift modifier is held, move the right stick and confirm the external scanner target does not move; confirm head-look still moves it.
7. Confirm F10 reflects both new X values and setting both back to zero re-centers the composition.
8. Confirm named minimap, health, quest, and corner regions retain their prior settings.
