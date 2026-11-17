# Startup feature switches

The desktop startup window now offers upstream's three feature switches, alongside
this fork's existing headset, runtime, resolution and debug controls.

| Switch | On | Off | INI key |
| --- | --- | --- | --- |
| VR Lobby | Use the virtual lobby before entering a save. | Use the normal flat main-menu presentation. | `xr_lobby` |
| VR Interface | Allow the in-world inventory, settings, vendor and stash menus. | Keep the game's normal inventory/vendor/stash menus. F10 remains available. | `xr_vr_interface` |
| Hand Physics | Enable the physics-hands master switch, including contact and NPC grabbing. | Disable physics-hands interaction. | `xr_hand_physics` |

Choices load from `red4ext/plugins/cyberpunkvrport/config/vrport.ini` and save when
**Start Game** is pressed. Closing/cancelling the window does not save edits.
Existing preferences are preserved. Missing VR Interface values default on; the
experimental physics master keeps the fork's default off for fresh configurations.
Collision sub-options in F10 still apply when Hand Physics is on.

VR Interface governs the world menu and its vendor/storage routing, not F10, the
normal HUD or the separate weapon wheel. Lobby is independent of VR Interface.

Implementation selectively follows upstream commit
`6f33af62c50a85f88eceaa63f776539d8fc6f395` (0.2.45), retaining our launcher resolution
lists and runtime selection. No frame-generation, pacing or render changes.

Validation: Windows DLL build; launcher window/input test covering saved-state
reload, switch clicks, Start persistence, Cancel, bounds, existing resolution
lists and game-input restoration; `tools/launcher_tests/interface_gate.lua`
checks normal vendor/stash fallbacks with the interface disabled.
