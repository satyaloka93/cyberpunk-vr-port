# CyberpunkVR Port 0.1.9 PSVR2 RC4

For Cyberpunk 2077 2.5. This candidate builds on RC3, not upstream 0.2.0.

## Changes from RC3

- Switch between private OFXR V557 and native frame generators in F10 without restarting the game. Requests take effect at a completed OpenXR frame boundary; native generation waits for V557 to finish disabling.
- Drain GPU work and retire inactive V557 synthesis pools and capture history before native generation starts. Re-enabling V557 reconstructs its history. Switching may briefly hitch; this is not a promise of higher FPS or measured VRAM savings.
- Pair the host with a pinned backend revision supporting the handoff contract. Older backend DLLs are rejected.
- Keep RC3's PSVR2 controls/haptics, Quest thumbrest shifting, pacing toggle and RT resource improvements. **The experimental stereo RR camera changes are excluded; the stereo rendering source is unchanged from RC3.**
- Update the switcher: the fork selection downloads RC4; the official upstream selection installs 0.2.0 with its separate RED4ext-only dependency list and configuration migration. Fork/VRCAM retain their own dependencies.

## Install

Extract **CyberpunkVRPort-Switcher.zip** outside the game and run **Install-PSVR2-Fork.bat**. Close the game first. Review the preview and confirm. Existing settings are migrated selectively and replaced files are backed up. Restore uses the most recent completed switch.

Advanced PSVR2 haptics/adaptive triggers require [our PSVR2Toolkit package](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2); see PSVR2-SETUP.md for driver and bridge instructions. No Steam DSX purchase is required.

Framegen starts OFF. For built-in switching, disarm external OFXR before launching. External OFXR remains a launch-time choice; RC4 does not dynamically remove global OpenXR layers. Keep the complete bundled `ofxr-v557` directory with its matching DLL and loader.

HAGS ON helped prior testing on this setup; results depend on hardware/runtime. Restart Windows after changing HAGS. The pacing fix remains enabled by default.

## Validation and limits

The candidate is built from committed host/backend source. Integration fixtures exercise the real OpenXR loader, backend and D3D12 resources against a fake headset runtime, including completed-frame handoff, inactive history retirement/reconstruction and renderer call chains. These do not establish full game-size synthesis performance or stability on every headset. Please compare live switching and sustained gameplay on your own setup.

Backend source: [`4150114`](https://github.com/satyaloka93/OFXR-Bridge/commit/4150114eb918f8fdc8f087323561c1170560f904). Host handoff: `e2019a0d`.
