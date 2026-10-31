# CyberpunkVR Port 0.1.9 PSVR2 RC3 — built-in V557 framegen

For Cyberpunk 2077 2.5. Carries forward RC2 haptics, controls, stereo DLSS memory
and compatibility fixes; adds the tested built-in V557 frame-generation route.

- F10 > FRAMEGEN > **OFXR V557 (NVIDIA)** replaces the old NVIDIA OFA + FidelityFX choice.
- Includes the patched V557 backend and private OpenXR loader inside the mod.
  **No external OFXR installation, tray or registry layer is required.**
- Normal loader ordering keeps generated submissions below implicit observer layers.
- Cyberpunk panel compatibility: dialog/loot/HUD panels retain game cadence;
  2X shallow scheduling, shared synthesis queue, Hybrid and bidirectional off.
- F10 flight-recorder toggle reports whether a log is actually being written.
- Tested profile: **Slow / 50%**. The installer selects it and leaves generation OFF.
  Disarm external OFXR before launching with built-in framegen.
- Latest active-generation samples averaged 85.1 estimated delivered FPS versus
  84.7 externally. Scenes and Cheeky changed between runs; occasional dips remain.
  These observations do not establish controlled parity or locked 90 FPS.

## Install

Extract **CyberpunkVRPort-Switcher.zip** outside the game and run
**Install-PSVR2-Fork.bat**, or choose **option 4** in the switcher.
This switcher pins the RC3 archive and SHA-256. Option 2 remains official upstream;
VRCAM comparison and restore options remain available. Older switchers do not
update themselves. Existing Cheeky files, DLSS replacements, unrelated mods,
personal calibration and game graphics settings are preserved.

The switcher backs up the private framegen directory with the rest of the port.
Switching editions or restoring removes files not present in the selected edition.
No game binaries or proprietary NVIDIA runtime DLLs are bundled.
See PSVR2-SETUP.md for controls and optional Toolkit haptics.

Release DLL SHA256: c950c44783be657e361e5a37a380ae9169171379cd2c9d5cc3923ac41a9e5919.
Built-in integration upstream PR: https://github.com/dariulone/cyberpunk-vr-port/pull/144.
