# CyberpunkVR Port 0.1.9 PSVR2 RC5

For Cyberpunk 2077 2.5. Builds on our RC4 fork, retaining its V557 backend and live framegen switching; this does not adopt upstream's VR lobby.

## Changes since RC4

- Fix asymmetric-projection OFF in SBS: use each eye's aspect and centered projection, preventing the letterboxed/squashed view. Clear stale projection descriptors and reset DLSS history when camera geometry changes. Normal asymmetric per-eye geometry is unchanged.
- Fix DLAA right-eye output copy-back at native resolution.
- Route dual-wield shots and recoil through the firing hand.
- Improve loading-screen presentation: keep panels mono until dismissed before resuming SBS; correct the cropped Continue screen.
- Include the previously tested scope parallelism, stereo water isolation, expanded hand-rig capacity and dual-wield support-grip fixes.
- Keep PSVR2 controls, Auto/OpenXR/Toolkit haptics, F10 audio settings, Quest thumbrest D-pad shift, driving grip toggle, separate bike/car camera settings, pacing toggle and RT resource improvements.
- Switcher now offers official upstream 0.2.1 first, this RC5 fork, and the retained game-2.5 VRCAM comparison. Download/check choices explicitly do not install. Upstream uses its separately verified RED4ext-only dependency list.

## Install

Extract **CyberpunkVRPort-Switcher.zip** outside the game and run **Install-PSVR2-Fork.bat** for RC5, **Install-Upstream-021.bat** for stock upstream, or **Switch-CyberpunkVRPort.bat** for all choices. Close the game first. The installer backs up replaced port files, migrates supported preferences and preserves saves, unrelated mods and Cheeky/ReShade loaders. Older extracted installers remain pinned to older releases: replace the whole switcher folder.

Advanced PSVR2 haptics/adaptive triggers: [PSVR2Toolkit v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2). Follow **PSVR2-SETUP.md** for driver and bridge setup; no Steam DSX purchase is required. The switcher does not replace drivers or change global OpenXR layers.

Framegen starts OFF; pacing remains ON by default. Disable external OFXR before using built-in framegen. Keep the bundled matching ofxr-v557 directory. HAGS ON improved prior local tests; results vary by hardware/runtime and require a Windows restart after changing HAGS.

## Scope and validation

The vector-capture diagnostics and F10 capture button are excluded. No DLSS5/NR model, vector scaling or color-processing change is included, and this release does not claim to fix DLSS5 reconstruction artifacts. Experimental stereo RR camera changes remain excluded. Standard operational logs remain available.

These gameplay fixes were tested locally before packaging. The release build, symmetric-projection regression checks and switcher fixture tests are verified separately; this candidate still needs testing on other systems.
