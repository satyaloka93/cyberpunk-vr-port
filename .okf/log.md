# Update Log

## 2026-08-10
* **Documentation**: Expanded [overlay middle-ground pacing](fixes/overlay-middle-ground-pacing.md) into the detailed Mode 0/1/2 implementation, ownership, crash-evidence, diagnostics, rollback, and regression record. Added the required [HUDitor VR layout workflow](operations/huditor-vr-layout.md), including Input Loader, the F7 conflict, keyboard/mouse editing, independent persistence files, and explicit credit to `nben/Cyberpunk-UI-mods-for-VR` for quickhack/scanner controller identification.
* **Maintenance**: Audited the bundle for current wording and GitHub navigation. Replaced the stale D-pad HUD-panning description with the current F10 layout and one-shot redscript design in [VR HUD layout](fixes/center-overlay-scaling.md), simplified the Sense adaptive-trigger explanation, added the [OKF maintenance and link policy](operations/okf-maintenance.md), and added `scripts/check_okf_links.py` to repair and validate document-relative links.
* **Fix / rejected experiments**: Validated [overlay middle-ground pacing](fixes/overlay-middle-ground-pacing.md): injected DXGI latency 1 held gameplay near 38–45 FPS and injected latency 2 caused `DXGI_ERROR_DEVICE_HUNG`, while the original poster's actual Mode 2—per-resource fences plus a previous-overlay fence and no injected DXGI wait—averaged about 69 FPS, reached a stable ~82 FPS segment and 96% reported GPU utilization, retained zero allocator-slot waits, and completed without a fence/device fault.
* **Finding**: Added the current [ray-tracing stereo limitation](architecture/ray-tracing-stereo-limitations.md) after ray-traced reflections appeared only in the right/VRCAM eye and ray-traced lighting made the left/MAIN eye darker. Matching render masks rule out a generic lighting gate; per-eye RT histories and outputs remain the unresolved boundary.

## 2026-08-09
* **Update**: Expanded the initial three tested weapon overrides into a conservative all-category Sense baseline and made explicit that users should choose categories rather than leave DualSense-oriented defaults, then tune every choice to taste.
* **Finding**: Native Settings category overrides produced better Sense mechanics than complex DualSense defaults; recorded Handguns = Very Soft, Shotgun = Hard, and Submachine Gun = Choppy as the initial PSVR2 compatibility profile despite the mod's DualSense-oriented Not Recommended label.
* **Update**: Added full-game stereo audio-derived Sense PCM for explosions, impacts, vehicles, ambience, and other audible gameplay, plus explicit same-mode shotgun recoil detection and stronger two-hand heavy-weapon impulses.
* **Fix**: Replaced incompatible DualSense raw Bow packing with Sense-tuned gradual weapon curves and shot-event resistance release after a `4/4` handgun profile was observed as stiff and chunky.
* **Fix**: Converted bundle-root absolute concept links to file-relative links so OKF navigation works both for bundle consumers and directly in GitHub's `.okf/` tree.
* **Creation**: Documented the optional [PSVR2 adaptive-trigger and grip-haptic integration](operations/psvr2-adaptive-triggers.md), including its extended dependencies, direct-config ownership rules, profile fidelity, launcher-DLL incompatibility, and driver recovery procedure.
* **Superseded experiment**: Extended shifted-D-pad panning to the separate `RightCenter` quickhack description panel and carried `dpadShiftActive` into the XInput hook to zero final merged right-stick axes during selection. The stick-suppression behavior remains; HUD panning was later removed as ineffective.
* **Superseded experiment**: Added shifted-D-pad horizontal HUD panning in 160-pixel steps while preserving normal D-pad input. This was later replaced by explicit F10 X/Y/Size controls and one-shot controller-specific wrappers.
* **Fix**: Added a live `Center overlays` HUD region after runtime logs showed scanner, quickhack, and popup layers arriving as unclassified centered `HUDMiddleWidget` roots; deployed a test DLL and CET script while preserving VRIK calibration and the user's existing HUD layout.

## 2026-08-08
* **Release preparation**: Created the `satyaloka93/cyberpunk-vr-port` GitHub fork, added bundled `PSVR2-CONTROLS.txt`, PS VR2 release notes, and package-script support for shipping both documents in `0.1.1-psvr2.1`.
* **Update**: Implemented and deployed UEVR-style dual-role SystemButton timing in the XInput poll: quick release emits Start, a ≥500 ms hold emits Back once, and Triangle-touch + R3 feeds the same fallback state machine while bare R3 remains crouch. Deployed DLL SHA-256: `72bb2a6459010bbff66473979403dd2a3e4b0a3d22c09ad0a7c0a232c82cde7b`; the prior DLL was retained locally; VRIK calibration remained unchanged.
* **Creation**: Added the [runtime dependency compatibility gate](operations/runtime-dependency-compatibility.md) after CET rejected Cyberpunk 2.31, leaving VRCAM disabled and output flat.
* **Update**: Updated official CET to 1.37.1, moved the VRCAM reload hotkey out of CET's `onInit`, and added one-time merged-XInput diagnostics.
* **Update**: Confirmed SteamVR's Oculus-to-Sense remapper omits Menu/System despite generated binding JSON; added Triangle-touch + R3 as a direct Start/Pause fallback while preserving bare R3 crouch.
* **Finding**: SteamVR recorded repeated Sense `Lost`/`Boot`, nine-second stuck frames, and up to 1813 dropped controller-tracker frames, confirming a separate controller tracking/Bluetooth stability problem.
* **Finding**: A manual SteamVR binding of left Create to the app's global SystemButton bypassed the broken auto-remap and opened Cyberpunk Start/Pause; SteamVR's actual system dashboard remains reserved.
* **Finding**: PSVR2 grip-pose calibration required about +26° pitch versus generic wrist defaults to level both avatar hands; persisted measured values are R `(27.4,-90,0)` and L `(-154.3,-90,0)`.
* **Finding**: Full UEVR source audit confirmed there is one OpenXR `SystemButton`, not separate Start/Menu actions; UEVR binds both menu/system candidate paths to it, maps short release to XInput Start and >500 ms hold to XInput Back, while L3+R3 opens UEVR's own framework.
* **Creation**: Added [PSVR2 Triangle-touch D-pad shifting](fixes/psvr2-triangle-dpad.md), including SteamVR binding-editor boundaries and regression checks.
* **Update**: Recorded the first PSVR2 retest and the missing early `XInputGetCapabilities` hook in the [controller input pipeline](architecture/controller-input-pipeline.md).
* **Creation**: Added the [PSVR2 SteamVR/OpenXR compatibility profile](hardware/psvr2-steamvr.md) from measured runtime and installed-mod evidence.
* **Creation**: Documented the [VRCAM resolution catalogue invariant](architecture/vrcam-resolution-catalog.md).
* **Creation**: Documented the [OpenXR controller to XInput pipeline](architecture/controller-input-pipeline.md) and packet-number fix.
* **Creation**: Scaffolded the CyberpunkVR Port Knowledge bundle with `okf_init.py`.
