---
type: Fix
title: VR HUD layout and one-shot widget adjustments
description: Combine HUDitor standard-widget alignment with F10 regions and one-shot dynamic-controller adjustments.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/mods/cet/CyberpunkVRPort_HUD/init.lua
tags: [hud, psvr2, vr, scanner, quickhack, popups, cet, redscript]
timestamp: 2026-08-10T09:08:00+09:00
---

# Failure

Cyberpunk does not place every HUD element under one stable controller or named region. Standard roots such as minimap, health, ammo, and quest widgets can be adjusted through the shared HUD tree, but scanner details, phone messages, notifications, and tutorials may initialize under separate controllers.

The original center-only scaling reduced oversized generic overlays but did not reliably move the complete quickhack information panel. A later D-pad panning experiment moved broad HUD roots without consistently moving the intended content and was removed.

# Current layout system

The F10 HUD controls expose X, Y, and Size for ten named regions, including `Center overlays` and `Right center`, plus seven legacy single-axis offsets. These 37 values persist in:

```text
plugins/cyber_engine_tweaks/mods/CyberpunkVRPort_HUD/hud_layout.ini
```

A Size value of `1.0` applies the established half-scale VR layout; `2.0` restores the original widget scale. Missing keys use safe defaults, so older layout files remain valid.

The CET layer resolves the HUD root, records original widget metrics, and reapplies only regions whose values changed. Standard minimap, health, stamina, ammo, quest, radio, and corner layouts keep their independent controls.

# HUDitor for standard widgets

The tested user installation also requires [HUDitor](https://www.nexusmods.com/cyberpunk2077/mods/3315) to align and resize supported standard HUD widgets. The persisted user state includes adjusted minimap, tracker, stamina, input hints, wanted level, weapon roster, crouch/D-pad elements, quest/item notifications, phone elements, and vehicle widgets.

HUDitor and CyberpunkVR Port have separate ownership. HUDitor provides the base position/scale of supported widgets; F10 provides VR grouping and final headset-relative adjustment; one-shot wrappers cover dynamic controllers HUDitor does not expose. Apply HUDitor first and keep F10 offsets conservative to avoid compounded transforms. Follow the [HUDitor VR layout workflow](../operations/huditor-vr-layout.md), including its mandatory F7 hotkey conflict resolution and persistence rules.

# Controller-specific adjustments

Some widgets are safer to adjust once when their own controller initializes. Small redscript wrappers handle these cases without retaining widget references or polling asynchronously:

| Controller | Purpose |
|---|---|
| `scannerDetailsGameController` | Moves the complete scanner/quickhack details panel toward the center |
| `PhoneDialerLogicController` | Places the contacts/dialer UI in the readable area |
| `PhoneMessagePopupGameController` | Places incoming message popups |
| `GenericNotificationController` | Places generic notifications |
| `TutorialPopupGameController` | Places tutorial cards |

Controller and widget identification for the quickhack/scanner work was informed by and is explicitly credited to [nben/Cyberpunk-UI-mods-for-VR](https://github.com/nben/Cyberpunk-UI-mods-for-VR). Its controller/widget mapping led this port to target `scannerDetailsGameController` for the complete visible details panel rather than moving only a child text widget. The implementation uses local one-shot wrappers in [scanner UI](../../mods/redscript/CyberpunkVRPort_HUD/vrport_scanner_ui.reds) and [additional UI controllers](../../mods/redscript/CyberpunkVRPort_HUD/vrport_ui_extras.reds).

# D-pad behavior

Triangle-touch or L3 plus the right stick still emits XInput D-pad directions and suppresses turning for the duration of the modifier hold. It no longer pans HUD regions. HUD placement is changed only through F10 controls and persisted layout values.

# Validation status

- The complete quickhack details panel was confirmed correctly placed.
- The expanded phone/notification/tutorial build was reported to look good in initial testing.
- Standard HUD offsets remain intentionally conservative.
- Continued checks are required for contacts, unread messages, notifications, tutorials, health, stamina, minimap, ammo, and auxiliary widgets across different gameplay states.

# Regression checks

1. Open scanner mode and the quickhack chooser; verify both the list and complete information panel are readable.
2. Open contacts and unread messages and trigger notification/tutorial popups.
3. Verify health, stamina, minimap, ammo, quest, radio, and corner widgets retain their own F10 placement.
4. Change `Center overlays` X/Y/Size, close F10, and confirm the values persist after restart.
5. Use shifted D-pad directions and confirm the game receives them without turning or moving the HUD.
6. Preserve the user's existing `hud_layout.ini` and HUDitor `persistency.json` during deployment or packaging.
7. Confirm HUDitor uses a rebound editor key because F7 belongs to VR recenter.
