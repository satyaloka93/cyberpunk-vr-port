# CyberpunkVR 0.1.8 VRCAM — game 2.5 comparison build

Version: 0.1.8-psvr2-cp25-vrcam-rc1. This is the patched game-2.5 checkpoint,
NOT the older 0.1.8 release for game 2.31. Requires Cyberpunk 2077 **2.5** and
its current RED4ext/CET/redscript/ArchiveXL/TweakXL/Codeware dependencies.

Uses the older MAIN/VRCAM stereo render path, not 0.1.9 native SBS. Intended for
performance/compatibility comparison; it is not claimed faster on every system.
Preserves the tested pacing, PSVR2 controls, Auto OpenXR/Toolkit haptics, driving
grip toggle, separate bike/car offsets, 2.5 aiming and script compatibility work.
Includes the committed 2.5 fog/reflection shader updates. Remaining visual/reload
issues in this checkpoint are not claimed fixed by packaging.

## Install and return

Download CyberpunkVRPort-Switcher.zip, extract outside the game, close Cyberpunk
and mod managers, then run Switch-CyberpunkVRPort.bat.
- **9:** preview/stage this VRCAM comparison build.
- **10:** install this VRCAM comparison build (or run Install-VRCAM-CP25.bat).
- **4:** return to our current 0.1.9 PSVR2/native-SBS release.
- **2:** install official upstream 0.1.9.
- **5:** restore the state before the latest completed installation.

Installer backs up and removes old port-owned files before replacing them;
saves, game UserSettings.json, personal calibration/input preferences, unrelated
mods, Cheeky/ReShade loaders and DLSS replacements are preserved. Public missing
dependencies download automatically; Nexus downloads need your own account.
More Occluders is optional; option8 can patch a recognized Scripted v0.3 copy.
Built-in framegen starts OFF and first-launch graphics overwrite is disabled.
This does not change OpenXR runtimes/layers, drivers, external framegen or HAGS.

For comparison, use the same save/location, resolution, DLSS mode/preset,
graphics settings and external tools. Port rendering defaults differ across
versions; this is a practical whole-build comparison, not an isolated stereo
algorithm benchmark. Keep Cheeky, NR and framegen off initially. Match FOV/fit
where possible. No performance guarantee is made.

## Optional advanced PSVR2 haptics

Standard vibration uses OpenXR. For adaptive triggers and advanced effects,
see [PSVR2Toolkit v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)
and the [current PSVR2 setup guide](https://github.com/satyaloka93/cyberpunk-vr-port/releases/download/0.1.9-psvr2.1-rc1/PSVR2-SETUP.md).
Start the compatible 017-protocol bridge before the game. Auto chooses it on
PSVR2 if detected; otherwise uses OpenXR. Steam DSX is not required. Existing
compatible Toolkit drivers do not need replacing just to test this port.

## Provenance

Source branch cp2077-2.5-base-0.1.8; game-2.5 migration065e2cdb and reload/aiming
checkpoint38f5160f. Packaging changes do not rebuild the tested native DLL.
DLL SHA256: eef7696ca6b97285ba50d47e572d5c77ee0d23f5bba5ac3bca57a76d2c103004.
No private settings, Toolkit binaries, Nexus mods, NVIDIA DLLs, logs or dumps
are bundled. The compatibility guard remains restricted to the supported game.
