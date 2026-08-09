---
type: Runbook
title: Runtime dependency compatibility gate
description: Diagnose flat fallback, missing VRCAM/VRIK, and absent CET behavior before changing native rendering or OpenXR code.
resource: https://github.com/maximegmd/CyberEngineTweaks/releases/tag/v1.37.1
tags: [cyber-engine-tweaks, cet, cyberpunk-2.31, vrcam, vrik, troubleshooting]
timestamp: 2026-08-08T03:55:00Z
---

# Dependency gate

The native RED4ext plugins can load successfully while Cyber Engine Tweaks rejects the current game. This creates a misleading partial system:

- OpenXR starts, tracks the HMD, and samples Sense actions.
- Native camera hooks see main/VRCAM-named camera calls.
- CET `registerForEvent` callbacks never run.
- The selected VRCAM component remains disabled, so no fresh right-eye texture exists and submission falls back to the main image in both eyes.
- CET VRIK, weapon, holster, HUD, and other Lua bridges do not initialize.

Therefore, “the game is in the headset” is not evidence that the stereo or gameplay-mod stack initialized.

# 2026-08-08 failure signature

The installed game was Cyberpunk `2.31`, while `bin/x64/plugins/cyber_engine_tweaks/cyber_engine_tweaks.log` reported:

```text
Unsupported game version! Only 2.21 is supported.
```

At the same time, `cyberpunkvrport.log` showed no `[stable] committed vrcam snapshot` line and continually submitted mono frames. This directly explained the flat world and missing CET-driven behavior.

CET `1.37.0` added game 2.31 support. The installation was updated to official CET `1.37.1`; the release ZIP SHA-256 was verified as:

```text
1855017796a27f518199f5b7d7210ef1db7a5c5f0af468c68e04e6e666ad248c
```

# Validation after an update

1. Confirm CET's current log no longer says `Unsupported game version`.
2. Confirm the CET console/log prints `[Stereo.VRCAM] enabled vrcam_<W>x<H>`.
3. Confirm `CyberpunkVRPort_Stereo/bridge/vrcam_active.txt` names the active virtual camera.
4. Confirm `cyberpunkvrport.log` prints `[stable] committed vrcam snapshot`.
5. Confirm the headset receives distinct eye images.
6. Confirm VRIK/weapon Lua modules initialize before diagnosing missing hands or weapon behavior.
7. On CET 1.37, register convenience hotkeys at mod-load scope, not inside `onInit`; `registerHotkey` is nil inside that callback and can abort the callback after partial initialization.

# Rollback

Before updating CET, archive only the package-owned files while preserving user mods/config separately. The 2026-08-08 local rollback archive is:

```text
build/backups/CET-pre-1.37.1-20260808-1252.tar.gz
```

Do not restore an old CET build onto Cyberpunk 2.31 as a functional configuration; use the backup only to recover files/settings.
