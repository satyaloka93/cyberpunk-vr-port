# CyberpunkVR Port Wabbajack proof of concept

This directory contains a reproducible Wabbajack authoring definition for the PS VR2 release. It does **not** redistribute third-party mods. Every payload is pinned to either an immutable GitHub release/commit or its Nexus Mods `modID` and `fileID`.

## Current status

Tested locally with Wabbajack **4.2.1.4**, Mod Organizer 2 **2.5.2**, and Cyberpunk 2077 **2.31**:

1. `Initialize-AuthorWorkspace.ps1` created a portable MO2 authoring workspace.
2. The complete list compiled with all **15 archives**: 11 immutable direct sources and four Nexus sources.
3. A clean installation started with an empty downloads directory and automatically downloaded and validated all 15 archives, including the four Nexus files.
4. Wabbajack installed 1,873 directives with zero missing archives; `verify-modlist-install` reported zero errors.
5. The installed `ModOrganizer.exe` opened successfully, selected the `PSVR2` profile, recognized the Steam Cyberpunk installation, and loaded the current Cyberpunk Basic Games plugin.
6. The installed stereo DLL retained SHA-256 `56ad555cfb5fc19f486ac34c54f7c8bbaa3fdaba9357368485b2fda0a1d6c0be`.
7. HUDitor's bundled `persistency.json` was stripped, and the compiled artifact contains none of the four user-owned calibration/layout files.
8. MO2 writes runtime output to a sibling `CyberpunkVRPort-PSVR2-UserData\overwrite` directory. All four probe files survived a Wabbajack reinstall byte-for-byte while the existing game-owned files remained unchanged.
9. No game files were changed and Cyberpunk was not launched during this proof.

The current CLI artifact is a **metadata-light validation build**, not a published end-user release:

```text
build\wabbajack-author\full-output\PSVR2.wabbajack
SHA-256: 1deca586d2196bfe5b26eced6db01d4ccc9a31d4f60b154006a2612a7a5278d4
```

Use Wabbajack's GUI compiler with the generated settings for a release artifact carrying the public name, version, image, website, and README metadata.

Wabbajack also logged a missing indexed-game-file entry for Cyberpunk build `3.0.5294808`. It continued compiling and detected the installed Steam game correctly, but this confirms Wabbajack's documented experimental Cyberpunk support and prevents treating the proof as release-ready without an end-to-end game launch test.

## Architecture

The list uses portable MO2 rather than copying files directly into the Cyberpunk folder. Current MO2 Cyberpunk support can force-load:

```text
bin/x64/version.dll   # Cyber Engine Tweaks
bin/x64/winmm.dll     # RED4ext
```

This makes legacy Root Builder deployment unnecessary for Cyberpunk 2.12 and newer. MO2 2.5.2 bundles an older Basic Games package, so the initializer replaces the **complete** package from pinned commit `3bd9da97c159a1bc05fd21e81199e997ed16e0f6`. Updating only `game_cyberpunk2077.py` is invalid because its support classes must match.

The profile contains:

- RED4ext 1.30.0
- Cyber Engine Tweaks 1.37.1
- redscript 0.5.31
- ArchiveXL 1.27.1
- TweakXL 1.11.4
- Codeware 1.20.3
- Input Loader 0.2.3
- Equipment-EX 1.2.9
- Visual Holsters 1.2
- Visible Bullets 2.31
- Nova Optics 1.3.1
- HUDitor 1.1.0
- CyberpunkVR Port 0.1.1-psvr2.2

The optional PSVR2Toolkit adaptive-trigger driver is deliberately outside this list because it is a driver-level installation with separate recovery and compatibility requirements.

## Build the author workspace

Requirements:

- Windows 11
- Cyberpunk 2077 2.31 installed through Steam for the first supported profile
- 7-Zip
- Current Wabbajack
- The four Nexus archives listed below in a local download directory
- A Wabbajack Nexus OAuth login for full compilation and clean-install testing

From PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\wabbajack\Initialize-AuthorWorkspace.ps1 -Force
```

The default workspace is:

```text
build\wabbajack-author\CyberpunkVRPort-PSVR2
```

For a legal, incomplete compilation smoke test that excludes all Nexus-only payloads:

```powershell
.\wabbajack\Initialize-AuthorWorkspace.ps1 `
  -Workspace "$PWD\build\wabbajack-author\CyberpunkVRPort-CorePOC" `
  -ExcludeManualMods -Force
```

The initializer:

- validates every archive against `manifest.json`;
- downloads GitHub-hosted dependencies using immutable URLs;
- finds locally downloaded Nexus archives by pattern and SHA-256;
- emits Wabbajack `.meta` files;
- extracts portable MO2 and all mod folders;
- installs the pinned current Basic Games package;
- strips protected runtime configuration from every staged mod;
- creates the `PSVR2` profile and compiler settings;
- never writes to the game directory.

## Complete the full compilation

1. Open Wabbajack 4.2.1.4 or newer.
2. Open Settings and sign in to Nexus Mods. Wabbajack's Nexus OAuth is separate from MO2's saved Nexus login.
3. Re-run the full initializer.
4. In **Create a Modlist**, load:

   ```text
   build\wabbajack-author\CyberpunkVRPort-PSVR2\CyberpunkVR-Port-PSVR2.compiler_settings
   ```

5. Compile and confirm all **15 archives** are gathered: 11 direct sources plus four Nexus sources.
6. Install the resulting `.wabbajack` into a new isolated folder and use a shared downloads directory.
7. Run `Test-Definition.ps1` against the workspace, compiled artifact, and installed proof.
8. Open MO2 and confirm the title identifies Cyberpunk 2077 and the `PSVR2` profile before launching anything.
9. Back up the current manually installed game and perform the first real game launch only after the isolated profile passes inspection.

The Nexus records are pinned as follows:

| Mod | Nexus mod | File |
|---|---:|---:|
| Visual Holsters 1.2 | 21936 | 112577 |
| Visible Bullets 2.31 | 22251 | 120998 |
| Nova Optics 1.3.1 | 29190 | 155124 |
| HUDitor 1.1.0 | 3315 | 135149 |

These IDs were cross-checked against Wabbajack's public modlist validation reports and the exact local archive hashes.

## CLI proof commands

After extracting the current Wabbajack release, the metadata-light CLI proof uses:

```powershell
wabbajack-cli.exe compile `
  -i "$PWD\build\wabbajack-author\CyberpunkVRPort-PSVR2" `
  -o "$PWD\build\wabbajack-author\full-output"

wabbajack-cli.exe install `
  -w "$PWD\build\wabbajack-author\full-output\PSVR2.wabbajack" `
  -o "$PWD\build\wabbajack-clean-install-test" `
  -d "$PWD\build\wabbajack-clean-download-test"
```

The GUI compiler should be used for a public build because it applies the name, version, image, website, and README fields from the generated compiler settings.

## Validation

```powershell
.\wabbajack\Test-Definition.ps1 `
  -Workspace "$PWD\build\wabbajack-author\CyberpunkVRPort-PSVR2" `
  -WabbajackFile "$PWD\build\wabbajack-author\full-output\PSVR2.wabbajack" `
  -InstalledProof "$PWD\build\wabbajack-clean-install-test"
```

The definition intentionally limits `NoMatchInclude` to project-generated profile/configuration files and individual MO2 `meta.ini` files. Never add the complete `mods` directory: if Nexus authentication failed, that would risk embedding unmatched third-party files in the `.wabbajack` archive.

## User-owned files and updates

Do not ship or overwrite:

```text
red4ext/plugins/CyberpunkVR_Stereo/vrik_calibration.ini
bin/x64/plugins/cyber_engine_tweaks/mods/CyberpunkVRPort_HUD/hud_layout.ini
bin/x64/plugins/cyber_engine_tweaks/mods/HUDitor/persistency.json
bin/x64/vrport.ini
```

The definition supplies none of these files. HUDitor's upstream archive contains a default `persistency.json`, so the initializer explicitly removes it before compilation.

The managed installation points MO2's overwrite directory outside the folder Wabbajack replaces:

```text
<install parent>\CyberpunkVRPort-PSVR2-UserData\overwrite
```

Do not put this user-data directory inside the Wabbajack installation. A same-version reinstall was tested with all four protected paths in the external overwrite directory; every file survived byte-for-byte.

Migrate an existing manual configuration without modifying its source files:

```powershell
.\wabbajack\Migrate-UserState.ps1 `
  -InstallPath "$PWD\build\wabbajack-clean-install-test" `
  -GamePath "D:\SteamLibrary\steamapps\common\Cyberpunk 2077"
```

The migration is hash-verified and preserves a differing destination by default. Use `-ReplaceExisting` only when the manual-game copy should deliberately replace it; the script backs up the previous destination first.

## Manual post-installation remains

Wabbajack cannot safely decide these user/runtime settings:

- set SteamVR as the active OpenXR runtime;
- start SteamVR before launching through MO2;
- rebind HUDitor away from F7;
- bind left Create to `SystemButton` when SteamVR omits it;
- perform VRIK and HUD calibration;
- install the optional PSVR2Toolkit adaptive-trigger driver separately.

## References

- [Wabbajack supported games](https://wiki.wabbajack.org/user_documentation/Supported%20Games%20and%20Mod%20Managers.html)
- [Wabbajack pre-compilation and source rules](https://wiki.wabbajack.org/modlist_author_documentation/Pre-Compilation.html)
- [Wabbajack metadata files](https://wiki.wabbajack.org/modlist_author_documentation/Meta%20Files.html)
- [Wabbajack compilation settings](https://wiki.wabbajack.org/modlist_author_documentation/Compilation%20Settings.html)
- [MO2 Basic Games Cyberpunk plugin](https://github.com/ModOrganizer2/modorganizer-basic_games/blob/3bd9da97c159a1bc05fd21e81199e997ed16e0f6/games/game_cyberpunk2077.py)
