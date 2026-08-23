---
type: Playbook
title: HUDitor VR layout workflow
description: Install HUDitor, resolve its F7 conflict, align standard HUD widgets, and preserve both HUD layout stores.
resource: https://www.nexusmods.com/cyberpunk2077/mods/3315
tags: [hud, huditor, vr, psvr2, installation, dependencies, persistence]
timestamp: 2026-08-10T09:08:00+09:00
---

# Role in the tested installation

The user uses **HUDitor v1.1.0** to align and resize standard Cyberpunk HUD elements for the headset. Treat HUDitor as a required third-party mod for reproducing this tested HUD layout, but do not bundle it without the author's permission.

HUDitor complements [CyberpunkVR Port's HUD layout system](../fixes/center-overlay-scaling.md):

- HUDitor owns supported standard widget positions and scales.
- F10 owns VR-port grouping, final headset-relative adjustment, and persisted VR layout controls.
- One-shot redscript wrappers own dynamic controllers such as complete scanner/quickhack details, phone/messages, notifications, and tutorials.
- Triangle-touch/L3 shifted D-pad input does not move HUD widgets.

# Dependencies

Install:

1. [HUDitor](https://www.nexusmods.com/cyberpunk2077/mods/3315), tested at v1.1.0.
2. [Input Loader](https://www.nexusmods.com/cyberpunk2077/mods/4575), required by the current HUDitor input mapping.
3. Optionally, [Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885) for the in-game hotkey and press/hold configuration.

CyberpunkVR Port already requires the other facilities HUDitor uses: CET, RED4ext, redscript, ArchiveXL, and Codeware. HUDitor's v1.1.0 changelog requires redscript 0.5.31 or newer.

A correct HUDitor installation places files in `archive/pc/mod`, `bin/x64/plugins/cyber_engine_tweaks/mods/HUDitor`, `r6/input`, and `r6/scripts/HUDitor`.

# Mandatory hotkey change

HUDitor defaults to F7. CyberpunkVR Port also uses F7 for HMD recentering, so the default is unsuitable.

Rebind HUDitor in the game's Mod Settings page. If Mod Settings is not installed, close the game and replace `IK_F7` in `r6/input/HUDitor.xml` with a valid unused keyboard `EInputKey`, then restart so Input Loader rebuilds its merged mapping. Leave the VR port's F7 recenter and F10/Insert settings keys unchanged.

# Editing workflow

1. Enter normal gameplay outside a safe/combat-restricted area.
2. Open HUDitor with its rebound keyboard key.
3. Use Left/Right arrows to select the previous/next widget.
4. Hold left mouse button and move the mouse to reposition it.
5. Use W/A/S/D for one-unit position changes.
6. Use the mouse wheel to resize.
7. Exit and persist with the editor key, Esc, or C.
8. Repeat while inside a vehicle for vehicle-only widgets.
9. Use X only to intentionally reset every HUDitor widget.

HUDitor has no gamepad editor. Use keyboard/mouse with the desktop mirror or remove the headset temporarily.

The installed user state confirms non-default placement for standard elements including minimap, tracker, stamina, input hints, wanted level, weapon roster, crouch/D-pad elements, quest/item notifications, phone widgets, and vehicle widgets. These values are personal and resolution-dependent; document ownership and workflow rather than copying the numbers as global defaults.

# Persistence and update safety

Preserve both independent stores:

```text
bin/x64/plugins/cyber_engine_tweaks/mods/HUDitor/persistency.json
bin/x64/plugins/cyber_engine_tweaks/mods/CyberpunkVRPort_HUD/hud_layout.ini
```

`persistency.json` contains HUDitor translations/scales. `hud_layout.ini` contains the VR port's 37 F10 layout values. Deployment must not replace either user-owned file. The VR port's deployment scripts do not own the third-party HUDitor directory.

Use HUDitor first for base standard-widget placement, then conservative F10 adjustments. Large transforms in both layers can compound and push content outside the visible headset area.

Full user-facing instructions are in repository document `docs/HUDITOR-VR-SETUP.md`.

# Quickhack and scanner credit

HUDitor does not replace the targeted complete scanner/quickhack details adjustment. Controller and widget identification for that work was informed by [nben/Cyberpunk-UI-mods-for-VR](https://github.com/nben/Cyberpunk-UI-mods-for-VR), specifically the mapping that led to targeting `scannerDetailsGameController`. Preserve this credit in public documentation.

# Citations

[1] [HUDitor — Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/3315)
[2] [Input Loader — Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/4575)
[3] [Mod Settings — Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/4885)
[4] [nben/Cyberpunk-UI-mods-for-VR](https://github.com/nben/Cyberpunk-UI-mods-for-VR)
