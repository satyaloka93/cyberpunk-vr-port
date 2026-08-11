---
type: Playbook
title: Wabbajack installation automation
description: Build and validate a portable MO2/Wabbajack profile without redistributing dependencies or overwriting VR calibration.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/wabbajack/Initialize-AuthorWorkspace.ps1
tags: [wabbajack, mo2, installation, dependencies, psvr2, nexus]
timestamp: 2026-08-10T11:15:00+09:00
---

# Current result

A Wabbajack 4.2.1.4 proof of concept now builds a portable Mod Organizer 2 profile from the pinned definition in repository directory `wabbajack/`.

The complete proof:

- compiled all 15 sources: 11 immutable direct downloads and four Nexus file records;
- produced metadata-light artifact SHA-256 `1deca586d2196bfe5b26eced6db01d4ccc9a31d4f60b154006a2612a7a5278d4`;
- started a clean install with an empty download cache and automatically downloaded all 15 archives through Wabbajack, including the four Nexus files;
- installed 1,873 directives with zero missing archives;
- passed Wabbajack's install verifier with zero errors;
- opened Mod Organizer 2 successfully with the `PSVR2` profile and recognized Cyberpunk 2077 through Steam;
- preserved the validated stereo DLL hash `56ad555cfb5fc19f486ac34c54f7c8bbaa3fdaba9357368485b2fda0a1d6c0be`;
- survived a same-version reinstall while preserving four external-overwrite probes byte-for-byte;
- did not modify or launch the installed game.

The CLI artifact proves installation mechanics but remains a validation build. A public artifact requires Wabbajack's GUI compiler metadata and a real MO2-launched VR regression test.

# Deployment architecture

Use portable MO2 2.5.2 with the current Basic Games Cyberpunk plugin pinned at commit `3bd9da97c159a1bc05fd21e81199e997ed16e0f6`.

The current plugin force-loads `version.dll` and `winmm.dll`, allowing CET and RED4ext to run through MO2's virtual filesystem. Root Builder is obsolete for this Cyberpunk 2.31 profile. Replace the complete Basic Games Python package, not only `game_cyberpunk2077.py`; mixing current game code with MO2 2.5.2's older support classes reproduced an initialization error.

# Source policy

Every third-party file must remain sourced from an original download:

- immutable GitHub release/commit archives use `directURL` metadata;
- Visual Holsters, Visible Bullets, Nova Optics, and HUDitor use Nexus `gameName`, `modID`, and `fileID` metadata;
- archive SHA-256 values are pinned in `wabbajack/manifest.json`;
- project-generated MO2 profile/configuration files may use `NoMatchInclude`;
- the complete `mods` directory must never use `NoMatchInclude`.

A broad `mods` inclusion could inline unresolved Nexus payloads when authentication fails, violating Wabbajack's non-redistribution model. The initializer includes only individual generated `meta.ini` files.

# Nexus records

| Mod | Mod ID | File ID |
|---|---:|---:|
| Visual Holsters 1.2 | 21936 | 112577 |
| Visible Bullets 2.31 | 22251 | 120998 |
| Nova Optics 1.3.1 | 29190 | 155124 |
| HUDitor 1.1.0 | 3315 | 135149 |

Without Wabbajack OAuth, the full compilation must fail with unmatched files. It must not fall back to embedding those files.

# Experimental game-index boundary

Wabbajack recognized the Steam installation but requested a missing indexed-game-file document for Cyberpunk build `3.0.5294808`. The compiler logged a 404 and continued because no base-game payload was required. This is consistent with Wabbajack marking Cyberpunk support experimental. Clean download/install and same-version update tests passed, but a public list still requires a real MO2-launched game test.

# User-owned state

The list must not supply or overwrite:

```text
red4ext/plugins/CyberpunkVR_Stereo/vrik_calibration.ini
bin/x64/plugins/cyber_engine_tweaks/mods/CyberpunkVRPort_HUD/hud_layout.ini
bin/x64/plugins/cyber_engine_tweaks/mods/HUDitor/persistency.json
bin/x64/vrport.ini
```

HUDitor's upstream archive supplies `persistency.json`; the initializer strips it before compilation. Artifact validation rejects any protected path.

A first reinstall experiment proved that Wabbajack deletes an ordinary `overwrite` folder inside its managed installation. The accepted profile therefore directs MO2 runtime writes to the sibling path:

```text
<install parent>/CyberpunkVRPort-PSVR2-UserData/overwrite
```

All four protected probe files survived a reinstall there byte-for-byte, and the corresponding files in the real game remained unchanged. Do not move this user-data directory under the Wabbajack install root. `wabbajack/Migrate-UserState.ps1` copies and hash-verifies manual-install calibration into matching external paths without modifying its sources; differing destinations are preserved unless replacement is explicit. This complements the ownership rules in [HUDitor VR layout](huditor-vr-layout.md).

# Remaining validation

1. Back up and clean the existing manual installation before the first MO2 game launch; do not test a virtual profile over duplicate physical framework/plugin files.
2. Copy existing calibration/layout state into the external user-data `overwrite` tree.
3. Start SteamVR, launch unmodified `Cyberpunk2077.exe` through MO2, and validate RED4ext, CET, redscript, VRCAM, OpenXR, controls, and shutdown.
4. Compile the release artifact through Wabbajack's GUI so public name, version, image, website, and README metadata are applied.
5. Test an actual list-version upgrade and deletion/restore workflow in addition to the passed same-version reinstall.

# Citations

[1] [Wabbajack supported games](https://wiki.wabbajack.org/user_documentation/Supported%20Games%20and%20Mod%20Managers.html)
[2] [Wabbajack pre-compilation rules](https://wiki.wabbajack.org/modlist_author_documentation/Pre-Compilation.html)
[3] [Wabbajack metadata files](https://wiki.wabbajack.org/modlist_author_documentation/Meta%20Files.html)
[4] [MO2 Basic Games Cyberpunk plugin](https://github.com/ModOrganizer2/modorganizer-basic_games/blob/3bd9da97c159a1bc05fd21e81199e997ed16e0f6/games/game_cyberpunk2077.py)
