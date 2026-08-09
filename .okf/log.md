# Update Log

## 2026-08-08
* **Release preparation**: Created the `satyaloka93/cyberpunk-vr-port` GitHub fork, added bundled `PSVR2-CONTROLS.txt`, PS VR2 release notes, and package-script support for shipping both documents in `0.1.1-psvr2.1`.
* **Update**: Implemented and deployed UEVR-style dual-role SystemButton timing in the XInput poll: quick release emits Start, a ≥500 ms hold emits Back once, and Triangle-touch + R3 feeds the same fallback state machine while bare R3 remains crouch. Deployed DLL SHA-256: `72bb2a6459010bbff66473979403dd2a3e4b0a3d22c09ad0a7c0a232c82cde7b`; prior DLL backed up as `build/backups/CyberpunkVR_Stereo.pre-uevr-systembutton-timing-20260809-1100.dll`; VRIK calibration remained unchanged.
* **Creation**: Added the [runtime dependency compatibility gate](/operations/runtime-dependency-compatibility.md) after CET rejected Cyberpunk 2.31, leaving VRCAM disabled and output flat.
* **Update**: Updated official CET to 1.37.1, moved the VRCAM reload hotkey out of CET's `onInit`, and added one-time merged-XInput diagnostics.
* **Update**: Confirmed SteamVR's Oculus-to-Sense remapper omits Menu/System despite generated binding JSON; added Triangle-touch + R3 as a direct Start/Pause fallback while preserving bare R3 crouch.
* **Finding**: SteamVR recorded repeated Sense `Lost`/`Boot`, nine-second stuck frames, and up to 1813 dropped controller-tracker frames, confirming a separate controller tracking/Bluetooth stability problem.
* **Finding**: A manual SteamVR binding of left Create to the app's global SystemButton bypassed the broken auto-remap and opened Cyberpunk Start/Pause; SteamVR's actual system dashboard remains reserved.
* **Finding**: PSVR2 grip-pose calibration required about +26° pitch versus generic wrist defaults to level both avatar hands; persisted measured values are R `(27.4,-90,0)` and L `(-154.3,-90,0)`.
* **Finding**: Full UEVR source audit confirmed there is one OpenXR `SystemButton`, not separate Start/Menu actions; UEVR binds both menu/system candidate paths to it, maps short release to XInput Start and >500 ms hold to XInput Back, while L3+R3 opens UEVR's own framework.
* **Creation**: Added [PSVR2 Triangle-touch D-pad shifting](/fixes/psvr2-triangle-dpad.md), including SteamVR binding-editor boundaries and regression checks.
* **Update**: Recorded the first PSVR2 retest and the missing early `XInputGetCapabilities` hook in the [controller input pipeline](/architecture/controller-input-pipeline.md).
* **Creation**: Added the [PSVR2 SteamVR/OpenXR compatibility profile](/hardware/psvr2-steamvr.md) from measured runtime and installed-mod evidence.
* **Creation**: Documented the [VRCAM resolution catalogue invariant](/architecture/vrcam-resolution-catalog.md).
* **Creation**: Documented the [OpenXR controller to XInput pipeline](/architecture/controller-input-pipeline.md) and packet-number fix.
* **Creation**: Scaffolded the CyberpunkVR Port Knowledge bundle with `okf_init.py`.
