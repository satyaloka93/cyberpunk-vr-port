---
type: Fix
title: VR HUD layout and scanner-details placement
description: Keep standard-widget placement in HUDitor while moving the complete scanner and quickhack details controller into the VR-visible area once at initialization.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/mods/redscript/CyberpunkVRPort_HUD/vrport_scanner_ui.reds
tags: [hud, psvr2, vr, scanner, quickhack, huditor, redscript]
timestamp: 2026-08-23T15:20:00+09:00
---

# Failure

The upstream 0.1.3 HUDitor preset places and scales the standard HUD widgets, but HUDitor does not expose the complete dynamic scanner/quickhack details panel. Its `scannerDetailsGameController` root remains at the flat-screen right edge, leaving some of its elements outside the comfortable PSVR2 view.

The attribution-only commit `94d3ae0` documented the controller identification but did not restore the actual wrapper from `psvr2-tweaks`. Consequently, it could not change scanner placement.

# Current ownership

HUDitor owns the 26 standard widgets in its `persistency.json`. The removed CET `CyberpunkVRPort_HUD` root walker and the old F10 HUD-region controls stay removed: they scaled or repeatedly rewrote broad HUD roots and could fight HUDitor.

The scanner details panel is the narrow exception because HUDitor does not own it. A redscript wrapper runs once when `scannerDetailsGameController` initializes, obtains the complete controller root, and applies the PSVR2-tweaks margin:

```reds
root.SetMargin(new inkMargin(-500.0, 0.0, 0.0, 0.0));
```

Moving the controller root keeps its heading, description, quickhack details, and related children together. The wrapper retains no widget reference and performs no periodic polling.

Controller and widget identification is credited to [nben/Cyberpunk-UI-mods-for-VR](https://github.com/nben/Cyberpunk-UI-mods-for-VR). That mapping is why the port targets `scannerDetailsGameController` rather than one child text widget.

# Deployment

The source is [vrport_scanner_ui.reds](../../mods/redscript/CyberpunkVRPort_HUD/vrport_scanner_ui.reds), packaged and installed at:

```text
r6\scripts\CyberpunkVRPort_HUD\vrport_scanner_ui.reds
```

Redscript compiles it on the next game launch. A compilation failure disables all redscript mods, so verify `r6/logs/redscript_rCURRENT.log` reports `Compilation complete`.

# Regression checks

1. Open scanner mode and select an NPC, device, or vehicle that displays the details panel.
2. Verify the complete right-side information panel is shifted toward the center and all child elements remain aligned.
3. Open the quickhack chooser and verify the list and details remain readable.
4. Confirm HUDitor's minimap, tracker, health, stamina, weapon, notification, phone, and vehicle placements are unchanged.
5. Confirm no `CyberpunkVRPort_HUD` CET poller or `hud_layout.ini` is required.
6. Check `redscript_rCURRENT.log` for a successful compile after deployment.
