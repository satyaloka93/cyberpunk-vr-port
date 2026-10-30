# CyberpunkVR Port 0.1.9 PSVR2 RC2 — memory headroom, haptics and Quest controls

For Cyberpunk 2077 **2.5**, based on upstream **0.1.9 native SBS stereo**.
Supports PSVR2 and other compatible OpenXR headsets, including Quest 3.

## DLSS / RT memory work

- Reclaims the original full-width DLSS Super Resolution context once stereo is using its two eye contexts and the original context is idle. Cleanup waits for tracked GPU work to finish; it does not force a GPU stall or free active eye/Ray Reconstruction contexts.
- In the tested configuration, roughly **825 MiB of identified original-context allocations were reclaimed**. This provides VRAM headroom for demanding RT/RR setups. The amount depends on resolution, DLSS settings and driver; **it is not a guaranteed FPS improvement or 825 MiB reduction in RT-specific allocations**.
- Right-eye DLSS staging textures now remain alive until their GPU submissions complete. Replaced textures are released through fence-backed ownership instead of the old fixed retirement list that could retain them indefinitely.
- Preserves RT/RR settings and visual quality. No reduced-quality graphics preset, lower-resolution reflection hack, or shared stereo histories are applied. Larger differences in total VRAM between our runs were not controlled comparisons and are not claimed as savings.

## Fixes and controls

- **Native in-game DLSS-G crash:** fixes an output-copy array overflow when both scaling output and HUD-less color are supplied. The failing case passed subsequent gameplay tests. This is a compatibility fix, not a claim that native DLSS-G performs better than external OFXR.
- **Crash on normal exit:** HUD and subtitle caches no longer keep widgets alive until after the game's UI services shut down. Cached references and deferred repaints are weak, and caches are cleared during plugin unload. Normal exit was validated on Quest 3/VDXR after repeated matching exit crashes.
- **Quest / Touch D-pad:** touch the **left thumb rest** and move the **right stick**. The original 0.5 direction threshold is restored; turning is suppressed for the whole hold. PSVR2 keeps Triangle-touch shifting. L3 remains the fallback. This requires the runtime to expose Oculus Touch thumb-rest input; it is not a promise of support for every Pico/controller profile.
- **F10 audio-haptics controls:** OpenXR driving-audio enable, strength, channel selection, and optional per-hand wheel/handlebar grip gating. Toolkit users get audio enable/strength controls through the installed PSVR2 Haptics companion. Toolkit audio channel selection/grip gating are not exposed by that bridge.
- PSVR2 **OpenXR / Prefer Toolkit / Auto** output selection remains available in F10 and requires a game restart. Quest automatically uses OpenXR; the PSVR2-only route selector is intentionally hidden there.
- Carries forward driving grip toggle, separate bike/car camera offsets, firing-hand dual-wield feedback, both-eye aiming dot, weapon/reload fixes, and the stable empty-overlay pacing toggle (ON by default).
- Removes temporary F10 GPU-capture controls and the added descriptor/allocation tracing from this release. Ordinary troubleshooting/error logs remain; no diagnostic marker files are packaged.

## Install or update

Download **CyberpunkVRPort-Switcher.zip**, extract it outside the game directory,
then run **Install-PSVR2-Fork.bat** or **Switch-CyberpunkVRPort.bat → option 4**.
The included switcher pins **this RC2 archive and SHA-256**. Existing downloaded
switchers do not update themselves: replace them with this release's switcher ZIP.
Option 2 still installs official upstream 0.1.9. VRCAM comparison options remain.

The switcher backs up and replaces port-owned files, preserves unrelated mods,
saves, personal calibration/input preferences, DLSS replacements and Cheeky/ReShade
loaders/settings. It downloads public dependencies and provides links for required
Nexus downloads. **More Occluders is optional**, not a required dependency; its
recognized game-2.5 compatibility patch remains available as an explicit option.
HUDitor is not required. Graphics presets are not forced; port framegen starts OFF.

## Optional advanced PSVR2 haptics

Use **[PSVR2Toolkit fork v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)**
and follow **PSVR2-SETUP.md**. Standard OpenXR vibration needs neither the bridge
nor Steam DSX. Advanced triggers/audio require the compatible driver, bridge and
companion setup. **Steam DSX is not required** and should not run alongside this bridge.

The compatible driver preserves raw trigger travel for the bridge while adaptive
trigger effects are active. The fork does not silently replace a driver: its
installer identifies the proposed driver and creates a backup. Toolkit v0.3.2
bundles the same driver as v0.3.1/v0.2.0, so existing users of those packages do not
need another driver update. Start the bridge before the game for Auto detection.

HAGS helped on the maintainer's system, but is not a universal performance promise.
Use one framegen route at a time; external OFXR is not bundled or installed here.
RT filtering remains a significant GPU cost; this release does not eliminate it.
