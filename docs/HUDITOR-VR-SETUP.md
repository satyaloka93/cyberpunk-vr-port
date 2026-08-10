# HUDitor setup for CyberpunkVR Port

CyberpunkVR Port's tested HUD layout uses [HUDitor](https://www.nexusmods.com/cyberpunk2077/mods/3315) to move and resize supported standard HUD widgets. HUDitor complements the VR port's F10 HUD controls and one-shot scanner/phone/notification fixes; it does not replace them.

## Required mods

Install the normal CyberpunkVR Port requirements first. The current tested HUD workflow additionally requires:

1. [HUDitor v1.1.0](https://www.nexusmods.com/cyberpunk2077/mods/3315).
2. [Input Loader](https://www.nexusmods.com/cyberpunk2077/mods/4575), required by current HUDitor releases for `r6/input/HUDitor.xml` and the editor hotkey.

HUDitor also uses dependencies already required by CyberpunkVR Port: Cyber Engine Tweaks, RED4ext, redscript (current HUDitor requires 0.5.31 or newer), ArchiveXL, and Codeware.

[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885) is optional but recommended because it provides the in-game HUDitor options and hotkey binding UI.

## Install

1. Install or update the normal CyberpunkVR Port dependencies and launch the game once to verify they load.
2. Install Input Loader into the Cyberpunk 2077 game root.
3. Download HUDitor from Nexus Mods and extract it into the same game root. A correct manual installation includes:

   ```text
   archive\pc\mod\HUDitor.archive
   archive\pc\mod\HUDitor.archive.xl
   bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor\
   r6\input\HUDitor.xml
   r6\scripts\HUDitor\
   ```

4. Optionally install Mod Settings to configure HUDitor from the game's Mods/Mod Settings menu.
5. Start the game and check `red4ext\logs\` if redscript or Input Loader reports an error. A redscript compilation failure disables every redscript mod, including HUDitor and the VR port's redscript features.

HUDitor is a third-party download and is not bundled into CyberpunkVR Port.

## Resolve the F7 conflict

HUDitor v1.1.0 defaults to **F7**, but CyberpunkVR Port reserves F7 for HMD recentering. Do not leave both actions on F7.

Preferred method:

1. Open the game's Mod Settings page.
2. Select HUDitor.
3. Bind the HUD editor to an otherwise unused keyboard key and choose press or hold behavior.

Fallback method when Mod Settings is unavailable:

1. Close the game.
2. Open `r6\input\HUDitor.xml`.
3. Replace `IK_F7` in the `HUDitor_Editor_Button` mapping with a valid unused `EInputKey` keyboard identifier.
4. Restart the game so Input Loader rebuilds its merged input configuration.

Keep CyberpunkVR Port's F7 recenter and F10/Insert VR settings bindings unchanged.

## Align standard HUD widgets

HUDitor is keyboard-and-mouse driven; its author does not provide gamepad editing controls. Use the desktop mirror while wearing the headset, or briefly remove the headset while editing.

1. Load normal gameplay and leave safe/combat-restricted areas if the editor says it is unavailable.
2. Press the rebound HUDitor editor key. The game pauses, inactive widgets become translucent, and the selected widget name appears in the upper-left.
3. Select the previous or next widget with the **Left/Right arrow keys**.
4. Move the selected widget by holding **left mouse button** and moving the mouse.
5. Use **W/A/S/D** for one-unit position adjustments.
6. Resize the selected widget with the **mouse wheel**.
7. Exit and save with the HUDitor key, **Esc**, or **C**.
8. Enter a vehicle and repeat the process to align vehicle-only widgets such as speedometer, radio, summon/hotkey, AutoDrive, and CrystalCoat.
9. Press **X only when you intentionally want to reset every HUDitor widget** to its default position and scale.

The tested user layout uses HUDitor for standard widgets including minimap, tracker, stamina, input hints, wanted level, weapon roster, crouch/D-pad elements, notifications, phone elements, and vehicle widgets.

## Use with the VR port's HUD controls

Apply the tools in this order:

1. Use HUDitor to place and scale standard widgets that it supports.
2. Use **F10 → HUD** for VR-port groups, final headset-relative adjustment, and widgets HUDitor does not expose.
3. Use the VR port's one-shot redscript fixes for the complete quickhack/scanner details panel, phone/messages, notifications, and tutorials.

Avoid applying large offsets to the same standard widget in both HUDitor and F10. Prefer HUDitor for the base widget position, then use conservative F10 adjustments. Triangle-touch/L3 D-pad shifting is input only and does not move HUD elements.

## Preserve the layout

HUDitor stores the user's translations and scales in:

```text
bin\x64\plugins\cyber_engine_tweaks\mods\HUDitor\persistency.json
```

CyberpunkVR Port stores its separate 37-value HUD layout in:

```text
bin\x64\plugins\cyber_engine_tweaks\mods\CyberpunkVRPort_HUD\hud_layout.ini
```

Back up and preserve both files when updating mods. Do not replace `persistency.json` with another user's preset unless that is intentional; it contains resolution- and preference-specific positions. CyberpunkVR Port's deployment scripts do not own the HUDitor folder.

## Troubleshooting

- **Editor does not open:** confirm Input Loader loaded `r6/input/HUDitor.xml`, check the rebound key, and try normal gameplay outside a safe zone.
- **Mouse cursor is not visible:** leave the safe zone and reopen the editor. HUDitor can still move the active widget even when its decorative cursor is missing.
- **A widget is absent:** trigger the real widget in gameplay or use its preview. Some phone/dialog/vehicle widgets exist only in their matching state.
- **HUDitor settings disappeared:** restore `persistency.json` from backup and verify CET can write to the HUDitor mod folder.
- **Quickhack details are still too far right:** use the VR port's scanner-details wrapper/F10 controls rather than forcing unrelated HUDitor groups.
- **Everything reset:** X resets all HUDitor widgets. Restore the backed-up `persistency.json`.

## Credits

HUDitor is maintained and distributed through [Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/3315). Controller/widget identification for CyberpunkVR Port's quickhack and scanner-details adjustment was informed by [nben/Cyberpunk-UI-mods-for-VR](https://github.com/nben/Cyberpunk-UI-mods-for-VR); that project deserves explicit credit for mapping the relevant UI controllers and widgets.
