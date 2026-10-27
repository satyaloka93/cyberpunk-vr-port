---
type: Runtime Integration
title: Recovered generic OpenXR feedback and PSVR2 fallback
description: Restore older-fork Quest recoil, swings and driving audio with startup-only Toolkit listener detection on PSVR2.
tags: [quest, psvr2, openxr, haptics, vehicle]
timestamp: 2026-10-21
---

# Evidence and provenance

Quest user reported ordinary-melee swings absent after 0.1.8 integration.
SetVRHapticPulse in Registration.cpp was Toolkit-gated. Upstream 0.1.8 native
events had no vehicle producer. Initial root/0.1.7 inspection was incomplete:
the earlier generic implementation survives in build/fsr3-worktree, including
its .okf/operations/generic-openxr-haptics.md. Vehicle source is committed as
7297823 (Add Quest vehicle audio haptics), not an invented replacement.

Recovered OpenXRVehicleAudioHaptics.cpp retains the 28–320 Hz filters, RMS,
fast-minus-slow transient, attack/decay coefficients and driving-only gate.
It now reads atomic gains, skips filtering on foot/muted, and publishes through
the 0.1.8 haptics arbiter rather than a competing second OpenXR writer.
Default endpoint loopback includes non-game audio. Gear bumps are audio-derived,
not identified transmission events. It does not reproduce Toolkit PCM fidelity.

# Integration

Generic recoil restores .45+.25*sqrt(kick), 35+8*sqrt(kick) milliseconds capped65,
and 0.55 support share. Native gain defaults1.25, vehicle gain1, both live in
F10 and persisted under the original xr_openxr_* keys. Upstream's master switch
and scale still apply. Upstream shot profile rows/exports retained for ABI but
are no longer used by Shot(); interaction envelopes remain active.

Ordinary-melee keeps the already adapted 0.1.8 physical swing detector and its
45/90ms requests; only the missing generic output route is restored. These
requests are NOT proof of a confirmed collision. The old pre-0.1.7 blade-hit
150ms signal is not blindly transplanted into 0.1.8's changed collision system.

Per-hand raw requests keep strongest/longest, expire after100ms. Blocks/parries
outrank generic pulses; vehicle rumble never interrupts an event envelope.
Vehicle expires100ms after last sample, uses45ms output lifetime, and stops
being scheduled immediately on vehicle exit. Focus loss, failed action sync,
master mute, and shutdown discard queued state. All xrApply calls remain on
the action-synchronized XR thread.

# Routing

See [haptic routing](haptic-routing.md). New/default selector2 is Auto. Existing
0 still means explicit native;1 now prefers Toolkit with native fallback.
Quest always native. PSVR2 selects Toolkit only if allowed by preference AND a
same-session psvr2_toolkit_dsx_bridge_017.exe owns a loopback UDP listener.
Detection is initialization-only and not a physical-output readiness handshake.
No bridge/driver/registry edits are needed. Starting/stopping a bridge mid-game
does not hand ownership over; restart required. Do not run bridge in forced
native mode. Renamed executables are intentionally not detected.

# Validation

GCC routing/input tests and Windows production Haptics.cpp playback tests cover
Quest swings, PSVR2 no-bridge fallback, Toolkit exclusivity, recoil support ratio,
event priority over vehicle, freshness, mute, vehicle exit, focus clear, invalid
inputs. Windows socket-only child fixture tests absent/present/exited compatible
listeners without loading Toolkit or accessing hardware. Ordinary-melee tests
cover25 scenarios at20/30/60/90/120Hz. Native release build passes.
Headset feel remains unverified until user tests the deployed build.

# Scope

Existing psvr2-tweaks-0.1.7 branch / build/upstream-0.1.8 only. Keep driving grip
toggle, adaptive pacing and asymmetric projections. No PR/release publication.

# Local deployment

October21 12:02KST, game closed. Installed DLL/PDB with verified SHA256
cf6d6986f255a9d108673132e375559d14c4807b48aca8a6c312a9f94c5c3f28.
Backup (parent repository): build/restored-haptics-018-deployment-20261021-120212-575.
Only INI edit was xr_psvr2_haptic_backend=0 ->2 (Auto), per user's fallback request.
Gains take source defaults until next menu save. Existing Lua behavior unchanged;
source Lua change is comments only and was not deployed. Protected hashes confirm
graphics, pacing/other INI values, launcher, loaders, EDS/audio settings, melee
scripts, SteamVR settings and personal bindings preserved. No running bridge at
predeployment check; none was launched/stopped. Pre-restoration log archived.

Final checks: four Windows suites pass (24 input,31 policy,15 production playback
checks plus absent/present/exited listener fixture);21 driving tests;25 Lua motion
scenarios; GCC input/routing suites; strict OKF validation and diff whitespace.
Ask user to test Quest katana swings, gun recoil and bike rumble, then PSVR2 native
with bridge closed. For advanced effects start bridge before next game launch.
