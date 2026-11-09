# CyberpunkVR Port 0.2.3 RC1 candidate

This is our customized 0.2.2-based fork with selected 0.2.3 backports, not a clean upstream 0.2.3 rebuild. The locally tested runtime checkpoint is `631a180b65da8c026ce3cf7afe03dfbd48adf484`, DLL SHA-256 `bfc0340e5bed7130adf7be15f5023b435e6d00697b16edf09e49849046f4ad4b`. The release switcher pins this RC1 archive and official upstream 0.2.3 separately.

## Changes to highlight

- Selectable **NVIDIA OFA + V557 reconstruction** reduces the bike-windscreen ghosting observed with the other OFA reconstruction. User validation covers PSVR2/SteamVR and Quest 3/VDXR; a Quest 3/Steam Link run was also recorded. The image algorithm is not restricted to SteamVR.
- Keep our SteamVR framegen package as one restart-required option, including queue isolation and timing. Steam Link can use it; VDXR automatically bypasses it. Adaptive real-frame pacing remains separate.
- Prepare both eyes before prior-display waits, remove unused real-output copies, and reuse pinned native captures instead of a duplicate history ring. The last change alone saves about 222 MiB of explicitly accounted allocations at 3144x3000 per eye. Full-resolution replay pixels remain identical; GPU ownership/backend-switch tests pass.
- Retain our fork's haptics, controller controls, driving grip toggle, aiming and selected body/roomscale fixes. Review their individual source commits when preparing upstream PRs; do not replace them with a stock upstream tree.
- The switcher adds **Check installation and recent errors (read-only)** with loader identities and bounded Cheeky/VR/mod log excerpts. See CHEEKY-DIAGNOSTICS.md and LOADER-IDENTITY.md.

## Validation limits

The three November 9 natural gameplay runs are not a matched-scene benchmark. Fresh accepted real+generated submissions averaged 70.86/s PSVR2, 89.41/s Quest VDXR and 82.49/s Quest Steam Link. SteamVR compositor estimates were 85.15 and 88.00 respectively; VDXR has no equivalent telemetry. Submitted frames do not prove displayed/unique-content FPS, and streaming latency is not measured. PSVR2 rendered about 7% more pixels. No claim of universal 90 FPS or perfect interpolation.

The V557 option remains experimental; the other generators remain available. Longer native capture leases occasionally caused brief slot misses on Quest, without sustained generation failure in these runs. Existing DLSS/RR/NR combinations retain their own limitations; this release candidate is not a new stereo-RR implementation.

## Install and defaults

Download **CyberpunkVRPort-Switcher.zip**, extract it outside the game and run **Switch-CyberpunkVRPort.bat**. Choose **4** for this RC1, **1** for official upstream 0.2.3, **5** to restore your previous port, or **11** for read-only Cheeky/loader diagnostics. Download/check options only stage files; they do not install.

RED4ext 1.31.0 is downloaded automatically. No Nexus downloads are required for the port itself. Shared frameworks and unrelated mods are retained. The port is replaced cleanly with rollback backups; personal fit/input preferences migrate. Graphics settings and Cheeky/ReShade loaders remain untouched.

Generation starts OFF, with NVIDIA OFA + V557 / Medium / 50% preselected. Adaptive pacing and the SteamVR package start ON; VDXR bypasses the latter. Diagnostic recording starts OFF. HAGS helped on the tested machine, but the benefit is system-dependent. See [setup instructions](PSVR2-023-RC1.md).

The V557 subset is LGPL-3.0, with source, provenance, licenses and rebuild instructions supplied. No external OFXR installation is required for this built-in backend.

Advanced PSVR2 haptics remain optional external setup:
https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2
Standard OpenXR haptics do not require the bridge or Steam DSX.
