# Selective upstream 0.1.6 imports

This branch does not merge or cherry-pick upstream 0.1.6 wholesale. Its native stereo, camera,
input, weapon, and monolithic archive changes overlap PSVR2, DLSSNR, foveation, and validated
save-load recovery work.

Source revision:

```text
0.1.6  b4a74461b8f7964ad5231113075da44890117523
```

## UI-only archive

`mods/archive/cyberpunkvrport_ui_016.archive` is a reviewed split of the eight UI resources added
by upstream's `mods/archive/cyberpunkvrport.archive`:

```text
base/gameplay/gui/widgets/braindance/braindance.inkwidget
base/gameplay/gui/widgets/interactions/dialog.inkwidget
base/gameplay/gui/widgets/notifications/tutorial.inkwidget
base/gameplay/gui/widgets/phone/new_phone.inkwidget
base/gameplay/gui/widgets/subtitles/subtitles.inkwidget
base/gameplay/gui/widgets/tutorial/tutorial_braindance.inkwidget
base/gameplay/gui/widgets/vehicle_control/vehicles_manager.inkwidget
base/gameplay/gui/widgets/vehicle_control/vehicles_radio.inkwidget
```

Hashes:

```text
upstream 0.1.6 monolithic archive:
056f3baf68166f8e9e6f8b7a2bad54ffeff8664b19478237807c49850f4ccf3d

PSVR2 baseline monolithic archive (retained unchanged):
d0cedecb17c4408d74df201b316714daa989210eba7038d4198e02e3b9e9a6c6

UI-only archive:
69e7cec136016d294105ca75cad4a8275a12c453bcbc42e6839194bfd84a565a
```

The UI-only archive has exactly eight entries and no depot-path overlap with the baseline archive.
A pack/extract round trip reproduced all eight source resources byte-for-byte.

The full upstream archive is intentionally excluded. Compared with the baseline, it also adds 14
laser/targeting resources and two player-replacer entities, and removes
`base/gameplay/focus_mode.envparam`. Those changes are not UI-only and depend on broader 0.1.6 work.

## Script-only imports

- `CyberpunkVRPort_LootUi/vrport_loot_ui.reds` is the exact upstream file.
- `CyberpunkVRPort_ScannerHud/vrport_scanner_hud.reds` imports only upstream's braindance-state
  scaling addition. Outside a braindance its multiplier is `1.0`.

Generic notifications handled by `GenericNotificationController` are not included in the eight
upstream UI resources and remain a separate issue.
