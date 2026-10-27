---
type: Behavior
title: Restored driving grip toggle on 0.1.8
description: Recover the exact tested 0.1.7 per-hand click-to-latch wheel and handlebar behavior missed in the 0.1.8 release integration.
tags: [vehicle, grip, driving, psvr2, regression]
timestamp: 2026-10-21
---

# Provenance and behavior

The0.1.8 RC integration omitted our tested commit
7bc109a1f205a0e15e3120587ef7e2be10d7c949, leaving upstream squeeze-to-hold.
Recovered its WheelGrab.cpp ownership changes and F10 help, not the old
renderer or old steering algorithm. Retained0.1.8's tracking API and steering.

With xr_wheel_grab=1, click a grip near the animated wheel/handlebar hand to
latch that hand. Release the physical grip without losing the latch. Click
again to release even away from the wheel. Each hand is independent; right
hand cannot latch with a weapon equipped. Leaving driver seat, disabling the
feature or losing tracking clears ownership. A camera-pair miss alone retains
a tracked latch; the maintenance path never creates a new grab.

# Validation and deployment

Restored the original production ownership test while preserving0.1.8's
five-argument GetGestureHandFrame stub. All21 Windows driving CTests pass,
including camera loss, toggle release, steering handoff, freshness/prediction
and gamepad range. MSVC native Release build passes. Automated checks do not
replace user acceptance on bike and car.

Deployed DLL/PDB at2026-10-21 11:29KST with game closed.
Installed DLL SHA256:1635a033ba9f83dabb89956ebf3b42caa76e2f8a4bc7a893d6b9f29c4586e936.
Backup: parent repo build/driving-toggle-018-deployment-20261021-112959-735.
Previous ec84609b DLL/PDB and pre-fix haptics log retained. Helper:
build/deploy-driving-toggle-018.ps1. Runtime/game graphics settings, EDS/audio
mods, loader, engine INI and newly repaired SteamVR binding hashes preserved.
xr_wheel_grab=1 and xr_psvr2_haptic_backend=0 remain selected. No bridge process
running at final check. Source remains on the existing branch, uncommitted;
no release or PR push in this follow-up.

## Separate seated camera offsets restored, 2026-10-26

Recovered the local `build/upstream-0.1.7` implementation: runtime
`vehicleBikeBaseObject` ancestry selects a separate motorcycle X/Y/Z trio;
other mounted vehicles use the existing car trio. F10 driving panel shows both
sets and the currently active type. Existing passenger-window-combat suppression
is retained; head and hand targets consume the same chosen offset.

Added `xr_bike_head_offset_x/y/z` throughout defaults, INI parsing, live state,
UI editing and serialization. Existing `xr_veh_head_offset_x/y/z` remain car
settings. Unlike the old local-only migration that reset cars to zero, a legacy
shared-only profile initializes bike values from the shared values and preserves
car calibration. Explicit bike keys thereafter remain independent. No archived
calibration values are silently substituted. DLL build/deployment follows.

Release build and restored anchor/vehicle-selection regression checks passed.
Deployed DLL/PDB, SHA256
`90f8ce9b7ce70e48bab5827c42762b5d091b71de8a3c9cd4208e6e0c0ad6669a`;
backup receipt `build/diagnostics/game250/deployment-backups/20261026-174342-708/receipt.json`
in parent tree. Current2.50 and lighting fixes remain. In-headset bike/car
selection and adjustment acceptance pending.
