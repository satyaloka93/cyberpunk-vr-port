---
type: Integration Runbook
title: PSVR2 Sense adaptive triggers and grip haptics
description: Optional integration of Enhanced DualSense Support gameplay profiles with the PSVR2Toolkit bridge.
resource: https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0
tags: [psvr2, sense, adaptive-triggers, haptics, cyberpunk, dependencies]
timestamp: 2026-08-10T07:52:00+09:00
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

# Sense weapon-category overrides

Enhanced DualSense Support marks category overrides **Not Recommended** because, on a DualSense controller, they replace more detailed per-weapon effects. PSVR2 Sense interprets those effects differently, so the direct category presets produced the more usable result in testing. Treat the following mapping as a comfortable starting point, not a required profile:

| Category | Starting preset |
|---|---|
| Handguns and light/fast melee | Very Soft |
| Revolvers and light blades | Soft |
| Automatic weapons and chainswords | Choppy |
| Rifles and medium-weight melee | Medium |
| Precision rifles, sniper rifles, shotguns, launchers, and heavy melee | Hard |

Very Hard, Hardest, and Rigid are avoided by default because they can fatigue the trigger finger. The mapping lives under `weaponsSettings` in `config/settings.json`; back up that file before tuning it. These overrides change the R2 effect while leaving L2 and the gameplay-state producer intact.

# Trigger and haptic fidelity

DualSense and Sense reuse custom effect IDs `0x22`, `0x23`, and `0x27`, but their parameter layouts differ. The first direct mapping sent a normal DSX Bow strength pair of `4/4`; Sense interpreted it as excessive resistance. The corrected bridge converts start, end, and strength into a gradual official Sense curve and briefly releases resistance when a shot is detected. Galloping and Machine modes use safe official trigger vibration, while detailed timing is sent through grip haptics.

The bridge emits signed 8-bit grip-haptic PCM at 3000 Hz from two sources. Gameplay-profile transitions provide weapon recoil, automatic-fire timing, charge timing, and shotgun detection. An optional WASAPI loopback layer, active only for `Cyberpunk2077.exe`, derives lower-frequency energy and transients from the Windows default audio output for explosions, impacts, vehicles, ambience, and other audible events.

Enhanced DualSense Support does not expose Cyberpunk's original DualSense audio waveform, so exact console haptic reproduction remains outside this integration's data boundary. Audio-derived coverage is broader but depends on Cyberpunk using the Windows output endpoint captured when the bridge starts.

# Deployment and recovery

The release's installer must run with SteamVR closed. It backs up `driver_playstation_vr2.dll` before installing the matching raw-trigger Toolkit driver. PlayStation VR2 App updates may restore Sony's driver; rerun the matching installer after such an update.

Detailed user instructions and troubleshooting live in the repository's [PSVR2 adaptive-trigger setup](https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/docs/PSVR2-ADAPTIVE-TRIGGERS.md).

# Citations

[1] [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit)
[2] [Cyberpunk DSX Bridge v0.2.0](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0)
[3] [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156)
[4] [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518)
