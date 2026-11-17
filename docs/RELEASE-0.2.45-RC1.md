# CyberpunkVR Port 0.2.45 PSVR2 RC1

This is the tested PSVR2/Quest fork with selected upstream changes through 0.2.45. It retains our frame generation and haptics; it is **not a full merge of stock 0.2.45**. Requires Cyberpunk 2077 2.5 and RED4ext 1.31.0.

## Install or switch

Download **CyberpunkVRPort-Switcher.zip**, extract the whole folder outside the game, and run **Switch-CyberpunkVRPort.bat**. Choose **4** for this RC1 or **1** for official upstream 0.2.45. The switcher downloads/verifies RED4ext, backs up the current port, removes stale port files, and preserves saves, unrelated mods, game graphics settings and Cheeky/ReShade loaders. Option **5** restores the exact previous installation. Download/check options only stage files.

More Occluders and HUDitor are not dependencies. CET and other frameworks may still be needed by your other mods. Option **11** reports loader identities and relevant Cheeky errors without changing files or uploading logs.

## Changes since our 0.2.3 RC1

- Bike controls: native handlebar/body lean or head-tilt mode; physical banking, bounded turn assistance, wheelies/stoppies and stabilized wheelie control. Steering sensitivity, grip toggle and reset defaults are in F10. Fixed initial steering neutral, hand reattachment and lean recovery.
- Bike haptics: wheel contact pulses and airborne road-rumble suppression. OpenXR includes these directly; see the Toolkit limitation below.
- Optional PhysX hand contacts and constrained NPC grips. Uses approximate collision geometry; disabled by default. Distant Reach is not included.
- Startup options for lobby, VR interface and hand physics. Fixed the lobby grading signature and a VR menu tooltip crash.
- Lightweight, adjustable green delivery overlay, separate from diagnostic recording. Both start off.
- Scanner grip works in scripted drone scenes. Selected 0.2.45 fixes add crouch pulses, raw-stick snap-turn thresholds, and guarded/retryable native foveated DLSS hook activation.
- Preserves our scope/RR fixes, PSVR2 Triangle/Create controls, Quest thumbrest D-pad shifting and thumbrest + right-stick-click inventory, full haptics routing, and separate bike/car camera adjustments.

## Frame generation and settings

Our V557-derived reconstruction and SteamVR timing/queue work are retained. NVIDIA OFA + V557, Medium / 50%, is preselected; frame generation starts **off**. Adaptive pacing and the SteamVR optimization package default **on**; VDXR bypasses the SteamVR-specific package. Restart the game after changing that package. Do not enable external OFXR and built-in frame generation together.

HAGS improved performance on the tested machine and is needed for DLSS-G; benefit depends on the system and driver. Reboot after changing HAGS. This release does not overwrite game graphics settings or include personal bike calibration. The compact overlay measures port output, not independently verified compositor display FPS.

## PSVR2 advanced haptics

Standard vibration works through OpenXR. Adaptive triggers and advanced Toolkit audio use the optional [PSVR2Toolkit bridge](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2); **Steam DSX is not required**. See **PSVR2-SETUP.md** for mod requirements and setup. The compatible Toolkit driver implements the bridge's trigger/audio transport; an already compatible driver does not need replacing. The switcher does not install drivers or OpenXR layers.

The public v0.3.2 bridge does not contain the local short-shift polling hotfix or new airborne road-rumble mute extension. It can continue its own bike rhythm during a wheelie. Those companion changes are not silently bundled in this port RC; OpenXR contact haptics do not require them.

## Validation and source

Publishes the exact user-tested DLL, SHA-256 `be08495e8382dc010b73193ef23c202a0759664d34d44732366f73515445b9e4`. Release build, driving/input policy checks, native Windows hook tests and lobby fixtures passed. Selected 0.2.45 input/hook changes left 46 framegen source files unchanged. No unrelated image/LUT agent work is included.

This remains a release candidate: hand collisions approximate meshes and bike feel depends on tracking/calibration. Source, licenses and rebuild instructions accompany the release. The existing tested fallback compiler is retained; not all stock 0.2.45 changes are included.
