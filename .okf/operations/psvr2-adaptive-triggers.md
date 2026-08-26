---
type: Integration Runbook
title: PSVR2 Sense adaptive triggers and grip haptics
description: Optional integration of Enhanced DualSense Support gameplay profiles with the PSVR2Toolkit bridge, including VR melee motion haptics and the single-owner rule for the controller-effect path.
resource: https://github.com/satyaloka93/PSVR2Toolkit/tree/cyberpunk-dsx-bridge-slot-guard
tags: [psvr2, sense, adaptive-triggers, haptics, melee, cyberpunk, dependencies]
timestamp: 2026-08-26T09:10:00+09:00
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
- Do not drive **Sense** through the VR plugin's generic OpenXR output. That backend rejects PSVR2 automatically; see [Nothing else may drive the actuators](#nothing-else-may-drive-the-actuators) and [Generic OpenXR controller haptics](generic-openxr-haptics.md).

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

The upstream-0.1.3 PSVR2 plugin creates a generic vibration action for compatibility with Quest/Touch and other headsets but rejects all requests when the detected system is PSVR2. Sense motion haptics therefore still go only through the bridge's own effect engine so they mix with its audio, weapon, and vehicle layers rather than fighting them.

# The bridge runs THREE haptic layers, and they compete

Undocumented until 2026-08-26, which is why a report of "haptics reverted to ambient stereo rather
than the DualSense mod" took a source dive to explain. The bridge mixes three independent sources
onto the same Sense actuators, and each has its own switch and gain:

| Layer | Source | Default | Switch |
|---|---|---|---|
| DSX trigger effects | Enhanced DualSense Support, via `DualSenseXConfig.txt` | always on | — |
| Full-game audio haptics | stereo game audio, loopback-derived | **gain 1.35** | `--no-game-audio-haptics`, `--audio-haptics-gain 0..3` |
| VR motion haptics | the plugin's shared slots `[157..160]` | gain 1.0 | `--no-vr-motion-haptics`, `--vr-motion-gain 0..3` |

The startup banner names all three, and that banner is the fastest way to see what is actually
running:

```
Cyberpunk grip PCM haptics enabled.
Full-game audio haptics enabled at gain 1.35.
VR motion haptics watcher enabled at gain 1 (waits for Cyberpunk).
```

**The audio layer is broadband and continuous; the DSX layer is sparse and event-shaped.** At gain
1.35 the audio layer can mask the trigger effects, which reads exactly as "everything feels like
ambient rumble and the weapon character is gone". `--no-game-audio-haptics` is the clean A/B: with
it off, anything still felt is the DSX and motion layers alone.

`1.35` is the **compiled-in default**, identical on `cyberpunk-dsx-bridge` and
`cyberpunk-dsx-bridge-slot-guard`, so it does not drift between builds and is not a setting that can
be silently lost. If the balance changed, the variable is the launch arguments, not the binary.

# Launching it: run_bridge.cmd -> run_bridge.ps1

`run_bridge.cmd` only echoes the two warnings and calls `run_bridge.ps1`; **every knob lives in the
PowerShell script's param block**, and it never passes `--no-game-audio-haptics`, so the audio layer
is always on at whatever gain is given:

| Parameter | Default | Effect |
|---|---|---|
| `-AudioHapticsGain` | **1.35** | full-game audio layer, 0..3. `0` silences it -- the clean A/B |
| `-VRMotionGain` | 1.0 | melee swing/impact pulses; `0` passes `--no-vr-motion-haptics` |
| `-Port` | 6969 | DSX UDP port |
| `-GameRoot` | auto-detected | Cyberpunk install |

To isolate the DualSense-mod layer from the ambient audio layer:

```
powershell -NoProfile -ExecutionPolicy Bypass -File run_bridge.ps1 -AudioHapticsGain 0
```

Anything still felt is then the DSX trigger effects and the motion layer alone.

## The launcher also enforces policy, which is easy to miss

`Sync-DualSenseSettings` runs before the bridge starts and does two things beyond launching:

* it **patches `UDPautostart` to false automatically** if it finds it on -- a fourth lock beyond the
  three listed above;
* it **warns on any weapon category that is not Default**, printing "Stock effects are recommended
  on PS VR2 Sense; overrides give inconsistent triggers." It deliberately **warns rather than
  rewrites** ("category overrides are the user's call"), so an override does persist -- but the
  advice on screen is the 2026-08-11 reversal, and that reversal was measured under a UDP conflict
  which this same script now prevents. Treat the warning as historical, not as a diagnosis.

## Where the melee whoosh is expected to come from

The two layers disagree on paper and both notes are worth knowing. `vr_motion_haptics.h` states the
audio layer **cannot** carry a swing: "the weapon whoosh sits outside the 28-320 Hz tactile band and
is inaudible to it even at high gain, and being stereo-derived it buzzes both grips rather than the
weapon hand" -- which is why the motion layer exists at all. `run_bridge.ps1` offers raising
`-AudioHapticsGain` to "test whether quiet sources -- such as the VR melee whoosh replayed on a
physical katana slash -- survive the 28-320 Hz tactile band". So a missing katana swing is a
**motion-layer** question first; the audio layer is not expected to substitute for it.

# An automatic weapon is not a semi-automatic

`firingBreaks` is set when the mod flips a loaded Bow/Weapon profile to `Resistance` or `Machine` --
the shot edge. On that edge the bridge sets the trigger to `SCE_PAD_TRIGGER_EFFECT_MODE_OFF` and
fires a recoil pulse, on the stated reasoning that "the best PSVR2 gun implementations ramp and
plateau while pulling, then drop resistance at the actual shot."

That is right for a semi-auto. On a submachine gun at roughly ten rounds a second the motor is
re-loaded and switched off continuously -- observed as the console alternating `R2 <- Bow` with
`R2 <- Machine -> firing release` -- so no sustained resistance ever establishes and the trigger
reads as flat. **The effect is weapon-class specific**, which is why some weapons keep their
character while automatics lose it.

# L2 and R2 are two CONTROLLERS, not two triggers

`sideValue` 1/2 routes to Left/Right. Enhanced DualSense Support authors LT as aim and RT as fire on
a single pad; on PSVR2 those land on **opposite hands**. A weapon profile whose character lives on
LT therefore puts it in the hand that is not holding the gun. Read the console side labels before
concluding an effect is missing -- `L2 <- Choppy` means the effect is being produced, just not where
it is being looked for.

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

Slot `[157]` is written last, after the payload, so a changed sequence means the record is complete. The guarded watcher additionally requires magic `18512`, protocol version `1`, and a changing OpenXR heartbeat in slots `[168..170]`. It adopts the current sequence only after seeing a live heartbeat, so joining mid-session does not replay a stale event. This matters because the bridge can retain the named mapping after the game exits; magic without freshness would accept metadata left by an earlier process.

# Driving and input isolation

The haptic ABI remains the already-shipped `[157..160]` contract. The upstream 0.1.3 port had independently reused those indices for right B, left Y, R3, and analog R2. That was unsafe even though it compiled: a bridge has no source-level dependency on this repository, so ordinary grep and compiler checks could not see the collision.

The compatible layout is:

| Slots | Owner |
|---|---|
| `[157..160]` | haptic sequence, hand, amplitude, duration — external bridge ABI |
| `[161]` | physical-reload trigger override |
| `[162]` | physical-reload owned hand |
| `[163]` | wheel/handlebar armed mask |
| `[164..167]` | right B, left Y, R3, analog R2 |
| `[168..170]` | haptic magic, version, live heartbeat |

Steering angle, wheel-arm blend, throttle latch, and vehicle classification remain same-DLL state and never enter the haptic record. Enhanced DualSense Support's car/bike trigger profiles and audio-derived vehicle rumble still flow through `HapticsEngine`; they are mixed rather than disabled. If an incompatible DLL is detected, only VR motion pulses pause, while gun, audio, vehicle, and adaptive-trigger processing continues.

There is a normal first-frame race: `OnPresent` creates the named mapping before that same frame reaches `FlushHandsToShared` and writes the protocol marker. A watcher polling every 5 ms can observe the brief zero-filled interval. The guarded bridge allows two seconds for first publication and writes expected pause states to stdout. Writing the transient warning to stderr was a functional bug because Windows PowerShell 5 converted native stderr to `NativeCommandError` under the launcher's `ErrorActionPreference = Stop`, terminating the launcher before the next frame could activate the protocol.

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

Detailed user instructions and troubleshooting live in the repository's [PSVR2 adaptive-trigger setup](https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/docs/PSVR2-ADAPTIVE-TRIGGERS.md).

# Citations

[1] [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit)
[2] [Cyberpunk DSX Bridge guarded branch](https://github.com/satyaloka93/PSVR2Toolkit/tree/cyberpunk-dsx-bridge-slot-guard)
[3] [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156)
[4] [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518)
