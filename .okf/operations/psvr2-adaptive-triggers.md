---
type: Integration Runbook
title: PSVR2 Sense adaptive triggers and grip haptics
description: Optional integration of Enhanced DualSense Support gameplay profiles with the PSVR2Toolkit bridge.
resource: https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0
tags: [psvr2, sense, adaptive-triggers, haptics, cyberpunk, dependencies]
timestamp: 2026-08-09T19:05:00+09:00
---

# Dependency boundary

This is an optional extension of the [PSVR2 SteamVR compatibility path](../hardware/psvr2-steamvr.md), not a base requirement of CyberpunkVR Port. It adds Enhanced DualSense Support, Native Settings UI, and the matching PSVR2Toolkit Cyberpunk bridge/driver. CET and RED4ext are already base dependencies.

The integration does not use DSX. Enhanced DualSense Support remains the gameplay-profile producer; the bridge directly monitors its `DualSenseXConfig.txt`, translates effects, publishes `DSXData.json` status, and sends commands through Toolkit CAPI.

# Runtime ownership

Only the Toolkit bridge may own the DSX-compatible UDP port/controller-effect path:

- Disable Enhanced DualSense Support UDP autostart.
- Do not run DSX or bundled `UDPClient.exe` concurrently.
- Restart the bridge rather than using the mod's Restart UDP Client command.
- Start SteamVR before the bridge so Toolkit CAPI can be discovered.

# Native launcher boundary

Enhanced DualSense Support's RED4ext process-launcher DLL is unnecessary. The installed historical DLL advertised runtime revision `3.0.80.33925` while the tested game runtime was `3.0.80.51928`. Patching only that advertised revision caused a freeze/exit. If RED4ext rejects the DLL, disable or remove it; never revision-patch it.

# Trigger and haptic fidelity

DualSense and Sense reuse custom effect IDs `0x22`, `0x23`, and `0x27` but use different parameter layouts. The initial direct mapping packed a normal DSX Bow strength pair of `4/4` into byte `0x1b`, which Sense interpreted as excessive force. The corrected bridge maps each weapon's start/end/strength semantics to a gradual official Sense slope and releases trigger resistance on the detected firing transition. Galloping and Machine use safe official trigger vibration while timing detail is carried in grip PCM.

The bridge emits signed 8-bit PCM at 3000 Hz from two layers. Semantic profile transitions add explicit weapon recoil and automatic/charge timing; shotgun detection includes same-mode Bow breakpoint jumps that do not transition to Resistance. A full-game WASAPI loopback layer, gated on `Cyberpunk2077.exe`, extracts stereo 28–320 Hz energy and full-band transients from the Windows default output for explosions, impacts, vehicles, ambience, and other audible gameplay.

Enhanced DualSense Support does not expose Cyberpunk's original DualSense audio waveform, so exact console haptic reproduction remains outside this integration's data boundary. Audio-derived coverage is broader but depends on Cyberpunk using the Windows output endpoint captured when the bridge starts.

# Deployment and recovery

The release's installer must run with SteamVR closed. It backs up `driver_playstation_vr2.dll` before installing the matching raw-trigger Toolkit driver. PlayStation VR2 App updates may restore Sony's driver; rerun the matching installer after such an update.

Detailed user instructions and troubleshooting live in [`docs/PSVR2-ADAPTIVE-TRIGGERS.md`](https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/docs/PSVR2-ADAPTIVE-TRIGGERS.md).

# Citations

[1] [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit)
[2] [Cyberpunk DSX Bridge v0.2.0](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0)
[3] [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156)
[4] [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518)
