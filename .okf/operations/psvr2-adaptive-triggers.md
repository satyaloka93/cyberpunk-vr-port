---
type: Integration Runbook
title: PSVR2 Sense adaptive triggers and grip haptics
description: Optional integration of Enhanced DualSense Support gameplay profiles with the PSVR2Toolkit bridge, including VR melee motion haptics and the single-owner rule for the controller-effect path.
resource: https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0
tags: [psvr2, sense, adaptive-triggers, haptics, melee, cyberpunk, dependencies]
timestamp: 2026-08-11T15:40:00+09:00
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
- Do not drive controller haptics from the VR plugin either. See
  [Nothing else may drive the actuators](#nothing-else-may-drive-the-actuators).

## UDP autostart resets itself

This is the single most consequential setting, and it re-enables silently. The mod's default
for `UDPautostart` is `true`, so **any use of its "reset to defaults" restores the conflict**
even if the option was previously turned off — which is exactly how the weapon-preset guidance
above came to be measured under a conflict.

The setting is therefore locked in three places, all in the mod's own files:

| File | Change |
|---|---|
| `utils/ManageSettings.lua` | default `UDPautostart = false`, so a reset cannot re-enable it |
| `utils/SetupNativeSettings.lua` | the switch refuses to latch and snaps back via `NS.setOption` |
| `config/settings.json` | current value `false` |

NativeSettings has no disabled control state, so snap-back is the closest available equivalent
to greying the option out. These edits live in a third-party mod and will not survive updating
Enhanced DualSense Support from Nexus — reapply them after any update. The project deploy
script only refreshes `CyberpunkVRPort_*` CET mods, so it will not overwrite them.

## Nothing else may drive the actuators

The boundary covers more than the UDP port. Adding an OpenXR
`XR_ACTION_TYPE_VIBRATION_OUTPUT` action to the VR plugin and calling
`xrApplyHapticFeedback` cost most of the game's gun haptics: recoil and automatic-fire
rhythms went quiet, machine guns lost feedback entirely, and a tech sniper shot arrived in the
wrong hand. The pulses themselves were valid — the runtime accepted every one — but SteamVR
haptic output and Toolkit CAPI compete for the same Sense actuators.

The plugin therefore ships with that action disabled. Motion-driven haptics must go through
the bridge's own effect engine so they mix with its audio and semantic layers rather than
fighting them.

# Native launcher boundary

Enhanced DualSense Support's RED4ext process-launcher DLL is unnecessary. The installed historical DLL advertised runtime revision `3.0.80.33925` while the tested game runtime was `3.0.80.51928`. Patching only that advertised revision caused a freeze/exit. If RED4ext rejects the DLL, disable or remove it; never revision-patch it.

# Sense weapon-category overrides — leave at default

**Use Enhanced DualSense Support's stock per-weapon effects.** Every entry under
`weaponsSettings` in `config/settings.json` should read `Normal`, which is the mod's default.
With the UDP conflict below resolved, the stock effects are consistent and the full haptic
range works.

This reverses earlier guidance in this document. A previous revision recommended a
category-override profile (Very Soft handguns, Soft revolvers, Choppy automatics, Medium
rifles, Hard heavy weapons) on the grounds that Sense interpreted the stock effects poorly.
That testing was confounded: the mod's own UDP client was autostarting alongside the
PSVR2Toolkit bridge, so two clients were driving the controller-effect path at once. The
symptom — inconsistent, partly missing trigger effects — was the conflict, not the presets.

Retested with only the bridge running, the category overrides **degrade** the result: they
break haptics and produce inconsistent trigger effects, while the defaults behave correctly.
Enhanced DualSense Support's own **Not Recommended** label on those overrides is accurate.

Back up `config/settings.json` before any tuning.

# Trigger and haptic fidelity

DualSense and Sense reuse custom effect IDs `0x22`, `0x23`, and `0x27`, but their parameter layouts differ. The first direct mapping sent a normal DSX Bow strength pair of `4/4`; Sense interpreted it as excessive resistance. The corrected bridge converts start, end, and strength into a gradual official Sense curve and briefly releases resistance when a shot is detected. Galloping and Machine modes use safe official trigger vibration, while detailed timing is sent through grip haptics.

The bridge emits signed 8-bit grip-haptic PCM at 3000 Hz from two sources. Gameplay-profile transitions provide weapon recoil, automatic-fire timing, charge timing, and shotgun detection. An optional WASAPI loopback layer, active only for `Cyberpunk2077.exe`, derives lower-frequency energy and transients from the Windows default audio output for explosions, impacts, vehicles, ambience, and other audible events.

Enhanced DualSense Support does not expose Cyberpunk's original DualSense audio waveform, so exact console haptic reproduction remains outside this integration's data boundary. Audio-derived coverage is broader but depends on Cyberpunk using the Windows output endpoint captured when the bridge starts.

# Melee motion haptics

Physical melee swings and impacts produce per-hand haptics, confirmed on katana and machete.
This is a bridge capability: it needs a bridge build carrying the VR motion watcher, not just
the plugin.

The audio layer cannot supply this. The weapon whoosh replayed by `VRMeleeWhoosh` falls
outside the 28-320 Hz tactile band and is absent even at `--audio-haptics-gain 2.5`, and being
stereo-derived it buzzes both grips rather than the weapon hand.

## Path

| Stage | Component |
|---|---|
| Detect swing and impact | CET weapon mod — speed thresholds, direction-flip re-arm, once per swing episode |
| Publish hand / amplitude / duration | `SetVRHapticPulse` native into slots `[157..160]` |
| Consume and mix | bridge `VRMotionHaptics` watcher -> `HapticsEngine::Pulse` |

Routing through `HapticsEngine` is what makes this safe: the pulse mixes with the audio and
semantic layers instead of competing for the actuators, which is the failure described in
[Nothing else may drive the actuators](#nothing-else-may-drive-the-actuators).

Slot `[157]` is written last, after the payload, so a changed sequence means the record is
complete. The watcher adopts the current sequence when it attaches, so joining mid-session
does not replay a stale event, and it validates amplitude and duration before emitting —
the mapping outlives the game process and a stale record must not reach the actuators at full
strength.

## Tuning

| Flag | Effect |
|---|---|
| `--vr-motion-gain 0..3` | scales motion pulses only, default `1.0` |
| `--no-vr-motion-haptics` | disables the watcher |

Swing amplitude scales 0.45-0.85 with swing speed; impact is fixed at 1.0 so contact reads as
clearly stronger than the whoosh. The carrier is chosen by duration — pulses of 60 ms or more
use 70 Hz for contact, shorter ones 150 Hz for the whoosh — which keeps Sense actuator tuning
in the bridge rather than requiring the plugin to know about it.

The watcher runs on its own 4 ms thread. The bridge's main loop can idle up to 250 ms on
`SO_RCVTIMEO`, which is far too slow for a swing to feel attached to the motion. It waits for
Cyberpunk and retries attaching once a second, so starting the bridge first still works.

# Deployment and recovery

The release's installer must run with SteamVR closed. It backs up `driver_playstation_vr2.dll` before installing the matching raw-trigger Toolkit driver. PlayStation VR2 App updates may restore Sony's driver; rerun the matching installer after such an update.

Detailed user instructions and troubleshooting live in the repository's [PSVR2 adaptive-trigger setup](https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/docs/PSVR2-ADAPTIVE-TRIGGERS.md).

# Citations

[1] [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit)
[2] [Cyberpunk DSX Bridge v0.2.0](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0)
[3] [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156)
[4] [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518)
