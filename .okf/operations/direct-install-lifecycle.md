---
type: Playbook
title: Direct installation and clean port replacement
description: Install or replace the VR port in the game folder without deleting shared dependencies or user-adjustable state.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/scripts/Install-CyberpunkVRPort.ps1
tags: [installation, uninstall, upgrade, backup, psvr2]
timestamp: 2026-08-23T11:58:00+09:00
---

# Ownership boundary

The direct installer writes the dependency stack and the CyberpunkVR Port payload into the real game folder. A version-to-version port replacement must remove only the port payload; RED4ext, Cyber Engine Tweaks, redscript, ArchiveXL, TweakXL, Codeware, Input Loader, Equipment-EX, HUDitor, and the other shared dependencies remain installed.

Run `scripts\Uninstall-CyberpunkVRPort.cmd` **before** installing another port version. The same script detects whether it is beside the 0.1.1 PSVR2 package or upstream 0.1.3 and selects that release's complete ownership list. It removes owned CET, redscript, tweak and RED4ext-plugin directories plus loose archives, grip files, the OpenVR shim, generated runtime state, and upstream's game-root support files. Removing the old `red4ext\plugins\CyberpunkVR_Hands` directory is mandatory before upstream 0.1.3 because upstream merged Hands into its single Stereo plugin.

# User state

Before removal, the uninstaller copies existing user-adjustable state under:

```text
%LOCALAPPDATA%\CyberpunkVRPort\uninstall-backups\<timestamp>\
```

Protected paths include VRIK calibration/settings/recenter state, the fork HUD layout, HUDitor `persistency.json`, the active VRCAM selection, recoil diagnostics, `vrport.ini`, the launcher selection and the current log. Runtime state is normally removed after backup to give a replacement build a clean first run; `-KeepRuntimeState` preserves it in place.

The newest `UserSettings.pre-vr-*.json` is not restored automatically during a port-to-port transition because both builds expect VR-tuned settings. Interactive uninstall offers restoration, and `-RestoreGameSettings` requests it explicitly.

The VR-tuned `UserSettings.json` must not impose the developer's locale. The original 0.1.1 PSVR2 capture accidentally carried `ru-ru` in VoiceOver, Subtitles, OnScreen, and Platform. The corrected template defaults to `en-us`, and the direct installer copies all four values and their list indices from the player's active settings into the deployed template before the native first-launch replacement. This preserves non-English installations as well as English ones.

A clean port-to-port uninstall removes `vrport.ini`, so installing the replacement recreates `first_launch=1`. Without another guard, the next native startup treats that as a genuinely fresh VR install and overwrites the complete active graphics profile with the packaged tuning—observed as lower quality and DLSS Balanced. `ApplyFirstLaunchGameSettings` now treats any existing `UserSettings.pre-vr-*.json` as durable evidence that VR first-launch setup already happened: it preserves the active `UserSettings.json`, consumes the recreated flag by writing `first_launch=0`, and logs the decision. The one-shot packaged profile remains available only on the first VR installation.

These ownership rules complement the external-state policy in [Wabbajack installation automation](wabbajack-automation.md) and the two independent layouts described by [HUDitor VR layout workflow](huditor-vr-layout.md).

# Upstream 0.1.3 adapted installer

The extracted upstream folder can carry the same direct installer under `scripts\`, with a local manifest identifying release 0.1.3. The installer recognizes the adjacent `bin`, `r6`, `red4ext`, `archive`, and `engine` roots as the port payload, while the manifest retains the validated dependency stack.

For the unified upstream build, preflight refuses installation while the obsolete separate Hands plugin or fork-only HUD directories remain. It also refuses an active `bin\x64\dxgi.dll` proxy. This turns the two most dangerous mixed-install states into instructions instead of copying over them.

Upstream's packaged HUDitor binding is authoritative: the installer derives `IK_F11` from its `r6\input\HUDitor.xml` and aligns HUDitor's config and merged cache to that key. Earlier fork packages without a binding retain their `IK_Delete` default. Upstream's `INSTALL.txt`, `UNINSTALL.txt`, and `UNINSTALL.bat` are copied to the game root because the generated uninstaller uses game-root-relative paths.

# Legacy in-place HUDitor edits

The 0.1.1 direct installer patched HUDitor-owned binding/default and unlock-gate files in place but did not retain their originals. The uninstaller does not guess or synthesize old third-party file content. It reports that limitation and leaves those small edits in place. The adapted upstream installer then supplies and aligns the intended F11 binding explicitly.

# Validation

For a replacement test:

1. Close Cyberpunk 2077, REDlauncher and Mod Organizer.
2. Run the uninstaller from the old extracted release and confirm that its backup path is reported.
3. Verify both old native plugin directories are absent, especially `CyberpunkVR_Hands`.
4. Keep the required dependency stack installed.
5. Install the replacement build.
6. Start the OpenXR runtime before Cyberpunk and collect a fresh log.
7. On a VR-to-VR transition, confirm the log says the prior backup was found and active game settings were preserved; compare the active settings hash with the pre-launch value.
