# PSVR2 0.2.3 RC1 setup

This is our customized 0.2.2-based fork with selected 0.2.3 backports. It also supports Quest and other OpenXR controllers.

1. Extract **CyberpunkVRPort-Switcher.zip** outside the game.
2. Close Cyberpunk and mod managers. Run **Switch-CyberpunkVRPort.bat**, choose **4** and confirm the install. RED4ext 1.31.0 is downloaded automatically. Existing saves, unrelated mods, graphics settings and Cheeky loaders are preserved; the previous port is backed up.
3. Start your OpenXR runtime, then the game. **F10** opens the port settings. Frame generation starts off; NVIDIA OFA + V557, Medium / 50%, is preselected. Enable it when ready. Adaptive pacing and the single SteamVR optimization package start on; VDXR bypasses the SteamVR package. Changing the package requires a game restart.
4. Use **Auto** or **OpenXR** for normal controller vibration. F10 contains audio/road haptics controls, driving grip toggle, and separate bike/car camera settings.

HAGS (Windows Hardware-accelerated GPU scheduling) helped on the tested machine and is needed for NVIDIA DLSS-G. This is a machine-dependent observation, not a promised FPS gain. Reboot after changing HAGS. Do not run external OFXR and built-in frame generation simultaneously.

## Optional PSVR2 adaptive triggers / advanced haptics

Use the separate [PSVR2Toolkit Cyberpunk bridge release](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2). **Steam DSX is not required.** Standard OpenXR vibration needs none of these extras.

The bridge uses the Cyberpunk **0.1.7 protocol** even with this RC1. Follow that release's setup instructions: install the optional [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156) and [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518), including their required frameworks. Run the Toolkit package's `setup_cyberpunk017.cmd`; it keeps the CET profiles, sets UDP autostart off and disables the incompatible native DualSense launcher. Start SteamVR and controllers, run `run_cyberpunk017.cmd` (or your existing BAT wrapper), then launch the game. Select Auto or Toolkit in F10.

The compatible Toolkit driver adds the bridge's raw trigger/audio transport and stale-audio cleanup. An unmodified original Toolkit driver does not necessarily implement that interface. **Keep an already compatible driver**: v0.3.2 uses the same compatible driver as v0.3.1/v0.2.0. The port switcher does not replace drivers, register layers or launch the bridge.

A separate local Toolkit polling hotfix improved very short gear-shift trigger delivery. It is not included in the linked v0.3.2 release or this port ZIP; do not assume every short shift pulse will match the local test machine. Its source patch is tracked under `tools/psvr2_bridge` for a future Toolkit update.

## Controls and troubleshooting

PSVR2 retains Triangle D-pad shifting and Create/menu support. Quest retains left thumbrest D-pad shifting and **left thumbrest + right stick click** for inventory. Existing personal input choices migrate.

For Cheeky loader conflicts, run switcher **11: Check installation and recent errors**. It identifies known DLLs and shows bounded recent log excerpts, including recovery messages. It changes nothing and uploads nothing. [Diagnostic details](CHEEKY-DIAGNOSTICS.md).

To return to the exact previous port, close the game and use switcher **5**. To install stock upstream instead, choose **1**. Do not delete arbitrary dxgi.dll/version.dll files: they may be your Cheeky/ReShade loaders.
