# CyberpunkVR Port 0.1.9 PSVR2 RC1 — Cyberpunk 2077 2.5

Based on upstream 0.1.9, retaining its native SBS stereo renderer.

- Sense Triangle touch + right stick emulates D-pad; Create tap/hold supplies Start/Back.
- Auto selects our compatible Toolkit bridge on PSVR2 at startup, otherwise restored OpenXR haptics. Quest uses OpenXR. Gun, melee and vehicle vibration; dual-wield feedback follows the firing hand.
- Optional advanced haptics/adaptive triggers through [PSVR2Toolkit v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2). Steam DSX is not required. Standard vibration needs no bridge. See setup guide for compatible driver requirements.
- Click-to-latch driving grip, second click releases; separate saved bike/car camera offsets.
- Stable empty-overlay pacing defaults ON, with F10 session toggle. It retains a GPU submission/fence wait instead of upstream skipping; performance benefit is runtime dependent.
- Reduced repetitive marker logging; corrected skipped shotgun selection and revolver reload alignment/extraction.
- Aiming dot appears in both SBS eyes and its enable setting persists.
- Improved startup/save-selection/loading panels; some splash cropping remains.

## Install

Download **CyberpunkVRPort-Switcher.zip**, extract outside the game folder and run
**Switch-CyberpunkVRPort.bat**. Option 2 installs official upstream 0.1.9; option 4
installs this PSVR2 fork. **Install-PSVR2-Fork.bat** goes directly to fork installation.
Both use verified archives and game-2.5 dependencies. Public dependencies download
automatically; missing Nexus mods require your signed-in manual downloads using
the displayed links. HUDitor is not required.

Old port-owned files are backed up and removed before installation. Restore is
available; game saves, personal fit/calibration, unrelated mods, DLSS replacements
and Cheeky/ReShade loaders/settings are preserved. Graphics presets are not forced;
built-in framegen starts OFF. The installer does not change XR layers or drivers.

Read **PSVR2-SETUP.md** for controls, optional Toolkit setup and driver explanation.
If using the bridge, start it before Cyberpunk; routing changes need a game restart.
HAGS improved performance on the maintainer's system; this is not a universal FPS
claim. Loading panels remain imperfect. No new native framegen compatibility claim.

The tested DLL is packaged unchanged. MSVC build, input/routing/playback/listener
and driving regression checks passed; user accepted controls/haptics/driving in game.

## Installer correction — 2026-10-28

More Occluders is **not required**. The updated switcher no longer requests,
installs or blocks on it for either upstream or PSVR2. Existing copies are
preserved with a compatibility warning. If the scripted variant fails compilation,
disable it through your mod manager or move `r6/scripts/MoreOccluders.reds` outside
the game. The reported game-2.5 script/type errors do not establish CET as the cause
or establish that every variant is broken. The game/DLL ZIP is unchanged.
