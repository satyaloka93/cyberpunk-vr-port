---
type: Implementation
title: Optional native or Toolkit haptics after RC1
description: Preserve OpenXR for all HMDs and make PSVR2 Toolkit exclusive output opt-in, with restart-safe routing and bound-output diagnostics.
tags: [psvr2, haptics, openxr, bindings]
timestamp: 2026-10-21
---

# Decision

**Follow-up supersedes routing defaults below:** user requested the earlier
generic haptics on Quest and native fallback on PSVR2 without a bridge. Auto2
is now the source default; saved0 stays native,1 prefers Toolkit but falls back
if no compatible listener is detected at startup. See [restored generic
feedback](generic-openxr-haptics.md) for current implementation and limits.
The remaining text records the original selector deployment and binding repair.

User accepts upstream0.1.8 asymmetric projection; do not restore PR96's old
FOV UI. User confirms Quest3 native haptics work in the published fork. Keep
that route and make Toolkit opt-in rather than mandatory for PSVR2.

# Implementation

Current local tree: build/upstream-0.1.8, existing psvr2-tweaks-0.1.7 branch.
New xr_psvr2_haptic_backend key:0/default OpenXR,1 Toolkit only on PSVR2.
HapticRouting.hpp separates atomic requested versus session-latched active
route. Invalid values use OpenXR; saved Toolkit never redirects another HMD.
F10 Controls shows selector, active output and restart requirement. Native
shots are suppressed only in Toolkit mode. That same route gates every bridge
publisher/heartbeat; native mode does not create the IPC mapping. Toolkit
melee pulses now respect port vibration enable/strength. Other headsets retain
upstream events. Native apply logs bound sources once plus changed results;
test buttons enqueue left/right belt-touch events on the usual XR thread.

Restart required because the existing external bridge independently writes
EDS trigger/audio effects. Port cannot safely live-switch the whole actuator
owner by simply stopping its heartbeat. Stop bridge for native mode; start
bridge for Toolkit mode. No bridge process/driver/config edits are performed.

# Evidence and limits

SteamVR settings currently select Workshop3788385174 for Cyberpunk's PSVR2
Sense application binding. Cached JSON is at Steam/steamapps/workshop/content/
250820/3788385174/14358285451816558998_legacy.bin (text JSON despite extension).
Its gameplay haptics array is empty. This supports missing custom output
bindings as an explanation of pre-integration native silence, not absence of
PSVR2 capability in upstream. The binding was repaired in the follow-up below.
Physical acceptance of both routes remains outstanding.

Published RC1 remains forced-Toolkit on PSVR2. The selector is now deployed
locally but not published, and PR96 remains unchanged. See docs/
PSVR2-HAPTICS-OUTPUT.md for the next candidate's procedure and narrow PR scope.

# Validation

MSVC Release native build passes, including a final incremental rebuild after
moving route initialization outside the optional gameplay-action block.
Built DLL SHA256: ec84609b63b66813f86c2ec4e061829529b667fdccef45a2e3fabfa9948ffb05.
Predeployment DLL was RC1 f93440a74da3890d3354d758787b3a4c75653c26fb26ff8fc9ff553ce401a3e1.
GCC and MSVC CTest both pass:24 input-policy checks plus17 routing checks
(default/native, explicit Toolkit, restart-only switching, another HMD with
saved Toolkit preference, invalid values). Ordinary-melee Lua regression
passes25 scenarios at20/30/60/90/120Hz. Diff whitespace and strict OKF validation
pass. These checks do not establish physical PSVR2 haptic output.

# Local deployment

October21 10:31KST: user requested deployment after finding the menu absent.
Game closed. Installed DLL/PDB, verified installed DLL equals ec84609b above.
Appended only xr_psvr2_haptic_backend=1 and a comment to vrport.ini, preserving
all original bytes and retaining the user's working Toolkit route. Source
default remains OpenXR. Transaction backed up all three files before writing:
build/haptic-selector-018-deployment-20261021-103134-895 (relative to parent repo).
Helper: build/deploy-haptic-selector-018.ps1. Protected game graphics settings,
launcher, loaders, engine INI, melee script, EDS/audio mods and SteamVR settings
hashes unchanged. No bridge process/driver/binding mutation. F10 > Controls >
PSVR2 haptics output (restart) is the test entry point; selector is PSVR2-only.
Dynamic handoff is not implemented. Output bindings were still missing at this
first deployment; the repair below supersedes that limitation.

# Confirmed missing binding and repair

User's restarted OpenXR run logged output=OpenXR requested=0, bound outputs
result=0 count=0, and successful xrApply results on both hands. This confirms
the configured output was active but unbound; it is not a bridge-selection
failure. Saved pre-fix log is in the driving-toggle deployment backup below.

At11:29KST, with game and SteamVR closed, scripts/Repair-PSVR2-HapticBinding.ps1
created a personal copy of the exact selected Workshop3788385174 binding and
added /actions/gameplay/out/haptic to both /user/hand/{left,right}/output/haptic.
Selected URI: file:///C:/Users/gthom/AppData/Local/CyberpunkVRPort/bindings/
cyberpunk-psvr2-native-haptics.json (one uninterrupted URI).
Only the Cyberpunk CurrentURL_openxr value changed in SteamVR settings. All
input mappings and the vrcompositor system/Create binding remain identical.
Workshop cache is unmodified. Original settings/source binding backed up at
C:/Users/gthom/AppData/Local/CyberpunkVRPort/binding-backups/
native-haptics-20261021-112932-498.

Helper defaults to preview, refuses live game/SteamVR writes, validates game
identity, preserves unrelated settings/inputs, backs up before writes and
rolls back on write/readback failure. Fixture tests passed preview immutability,
two-output repair, semantic preservation, idempotency and wrong-game refusal.
No bridge process was running at the final check. Keep current
xr_psvr2_haptic_backend=0 for native testing. Physical acceptance remains pending.
The subsequent [grip-toggle restoration](vehicle-grip-toggle.md) deployed DLL
1635a033... without changing haptics code or the user's OpenXR selection.
