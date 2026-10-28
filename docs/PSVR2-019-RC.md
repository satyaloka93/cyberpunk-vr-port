**Requires Cyberpunk 2077 2.5.**

# PSVR2 on CyberpunkVR Port 0.1.9 — release-candidate setup

Release: **0.1.9-psvr2.1-rc1**. Download the installer ZIP from
[this fork release](https://github.com/satyaloka93/cyberpunk-vr-port/releases/tag/0.1.9-psvr2.1-rc1).
Official upstream and our PSVR2 fork are different selections. The switcher
bundled with this release automatically downloads the checksum-pinned 0.1.9
PSVR2 candidate, not the older 0.1.7 fork.

## Install or upgrade cleanly

1. Close Cyberpunk, REDlauncher and mod managers. Extract the entire switcher
   ZIP outside the game directory. Run **Install-PSVR2-Fork.bat** to download
   and install this fork, with a preview/confirmation before changing files.
   For upstream/fork choices or restore, run `Switch-CyberpunkVRPort.bat`.
2. Select **Preview / stage PSVR2 fork** and review the selected archive name.
   Leave the optional local ZIP path empty to download this release automatically.
   **Install upstream 0.1.9** installs the official build, not our PSVR2 changes.
3. Keep installed dependencies where compatible. Missing GitHub dependencies
   download automatically with checksums; Nexus dependencies require your own
   signed-in manual download. Follow the displayed links. HUDitor is not a
   dependency; the port's native HUD replaces it.
4. Select **Install PSVR2 fork**, review the file plan and confirm. All affected
   originals are backed up before changes. Old port-owned files are removed
   before the selected release is installed; installed hashes and absence of
   retired files are verified before success. Failures trigger rollback.
5. Start SteamVR, then optionally the Toolkit bridge for advanced PSVR2 effects,
   then launch Cyberpunk. F10 or L3+R3 opens the port menu.

The cleanup includes old CyberpunkVR native DLLs (including renamed DLL backups
inside their plugin directories), port scripts, shader files and obsolete
port archives. It is not a wipe of `red4ext`, `r6`, `bin` or all mods.
Saves, game `UserSettings.json`, unrelated mods, replacement DLSS DLLs,
Cheeky/ReShade loaders and their settings are preserved. Personal fit/input
preferences and calibration migrate; old experimental render,
framegen and debug settings do not. Separate bike offsets are retained where
supported by the selected build, not added as a feature to stock 0.1.9.
The first-launch graphics overwrite is disabled and built-in framegen starts OFF.
The package retains upstream 0.1.9 engine configuration and native SBS stereo.
Stable empty-overlay pacing defaults ON; its F10 toggle is session-only.
Turning it OFF restores upstream empty-overlay skipping. ON retains an additional
GPU submission/fence wait, which improved this SteamVR setup; benefit varies.

Do not delete `dxgi.dll` merely because the port does not ship one: Cheeky or
ReShade may own it. A conflicting incoming loader blocks installation for review.
Explicit `-DisableDxgiProxy` is the only switcher option that disables dxgi;
`-Yes` does not. The switcher does not install Cheeky, select an OpenXR runtime,
register layers, replace Toolkit drivers or start/stop a bridge.

**Restore previous port** restores the latest completed switch. Every upgrade
gets a separate backup; older backups remain available. Do not remove journals
to bypass an interrupted transaction. Framework minimum versions and the redscript compiler hash are checked for game 2.5;
other mods use presence checks. These checks cannot guarantee runtime compatibility.

## Advanced haptics and adaptive triggers

Download the full package from **[our PSVR2Toolkit fork, v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)**.
It contains separate `driver/` and `bridge/` directories. **Steam DSX is not
required; do not run DSX or EDS's UDPClient alongside this bridge.**

The Cyberpunk **017** bridge name denotes its isolated protocol. Our modified
0.1.9 retains that protocol; do not replace it with the old legacy bridge.
Auto haptics (default) selects the compatible bridge if running on PSVR2 at game
startup; otherwise it uses our OpenXR gun, melee and vehicle effects. Quest uses
OpenXR. F10 exposes the route and gain controls. Start the bridge before the game
for adaptive triggers; restart the game to change routing. This is not a live
handoff. Standard OpenXR vibration does not need Toolkit or the extra EDS mods.

1. If new to Toolkit, complete the
   [base Toolkit installation](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation)
   first. Keep its support DLLs and original Sony-driver backup.
2. Keep an existing compatible Toolkit driver. Only if choosing our tested
   driver baseline, close SteamVR and run `driver\INSTALL_RAW_TRIGGER_DRIVER.cmd`.
   It shows hashes, requests confirmation and creates a recoverable backup.
   **v0.3.2 ships the same driver as v0.3.1/v0.2.0: those users need no driver update.**
3. Install [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156)
   and [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518)
   plus their prerequisites. Run once to create EDS settings, then close the game.
4. Run `bridge\setup_cyberpunk017.cmd`. It adds the audio-menu companion, sets
   `UDPautostart=false`, and backs up/disables EDS's separate native launcher
   DLL. **Keep the DualSense Support CET mod and weapon profiles.**
5. Each session: SteamVR and both controllers first, then
   `bridge\run_cyberpunk017.cmd`, then Cyberpunk. Use only one bridge.
   For nonstandard locations, pass `-GameRoot "D:\path\to\Cyberpunk 2077"`.
   If a launcher reports an empty bridge path, supply
   `-BridgeRoot "C:\path\to\extracted\bridge"` explicitly.
6. In the game, open **Mods > DualSense settings > PSVR2 Haptics**.
   Audio haptics defaults OFF on a fresh setup; enable and adjust strength
   there, not in F10. Existing choices persist.

Why Toolkit/a different driver? The stock Sony driver lacks the Toolkit
controller API used for adaptive triggers and PCM grip vibration. Our driver
adds raw-trigger transport and clears stale grip-PCM attenuation. Current
bridge effects use standard Toolkit trigger modes, so raw-trigger support alone
does not justify replacing an otherwise compatible working driver. Installing
the audio-menu update does not require a new driver.

Audio captures the default Windows output, including ambience/music and other
apps; it is not game-event-only. Turning audio OFF also removes audio-derived
road rumble, but retains adaptive triggers, semantic weapon recoil and VR
melee swing pulses. Start with audio OFF to assess those effects clearly.

## Controls

- Triangle touch + right stick: D-pad; right-stick turning is suppressed while held.
- Create tap: Start/Pause. Hold at least 500 ms: Back/Select.
- Triangle touch + R3: alternate tap/hold menu gesture.
- L3+R3: port overlay; F10 also works.

If Create takes SteamVR screenshots instead, review the SteamVR system binding
and Cyberpunk application binding separately. In the application bind physical
left Create to **SystemButton (Sense Create)**; do not assign it to Screenshot.
The switcher does not silently rewrite your system bindings.

## Performance starting point

Keep **F10 > STEREO > PERFORMANCE > Stable pacing with no overlay graphics**
checked. It already defaults ON at each launch; the checkbox is session-only.
This preserves a small overlay-queue submission/fence wait even with no visible
overlay, which improved SteamVR frame delivery in our tests. The tradeoff is
extra synchronization/work; results may differ on other runtimes. Unchecking
restores upstream empty-overlay behavior. This is **not** framegen's separate
adaptive pacing checkbox and does not generate frames.

Try Windows **Hardware-accelerated GPU scheduling (HAGS)** ON and restart Windows.
Find it under Settings > System > Display > Graphics (default/advanced graphics
settings, depending on Windows version). It appears only on supported hardware
and drivers; see [Microsoft's graphics settings guide](https://devblogs.microsoft.com/directx/navigating-the-redesigned-graphics-settings-page/).
The installer does not change HAGS or your registry.

Earlier 0.1.8 testing improved after enabling HAGS and restarting. That was not
a controlled HAGS-only benchmark; no universal FPS benefit is promised for 0.1.9.

Begin without NR or framegen, then enable one extra at a time. Keep built-in
framegen OFF when using external OFXR; do not stack both generators. External
OFXR felt better in the maintainer's tests, not a universal performance claim.
Cheeky compatibility fixes belong to its separate build, not this port installer.

## Candidate scope and known limitations

Includes upstream 0.1.9 native SBS rendering, restored PSVR2 controls, Auto
OpenXR/bridge haptics, grip latching, separate bike/car cameras, default-on pacing,
revolver/weapon selection and stereo aiming-dot corrections. No old FOV override
has been transplanted into this renderer. Some splash cropping remains.

The tested DLL is packaged unchanged, SHA256:
471d1e72e37d870d96b9fecdfc7381f05303ff3ece233e504514cf5830d500e1
The release tag records the patched source. This remains a release candidate.


## Optional More Occluders compatibility patch

More Occluders remains optional. Run **Switch-CyberpunkVRPort.bat**, then choose
**8 — Patch / install optional More Occluders for game 2.5** with the game closed.
An installed copy is used automatically. If absent, option 8 prints the author's
[Nexus download page](https://www.nexusmods.com/cyberpunk2077/mods/33136?tab=files)
and accepts your manually downloaded **Scripted v0.3 ZIP**. No third-party script
is bundled or downloaded automatically.

Only the recognized original or our known patched script is accepted (BOM/line
endings normalized for identification). Unknown versions or custom edits are
refused without modifying the game. The patch keeps ordinary-mesh optimization
and skips the incompatible instanced-mesh branch. This matches the maintainer's
working October 26 game-2.5 migration; remaining performance benefit is unmeasured.

Review the prompt and type SWITCH. Existing files are backed up under
`%LOCALAPPDATA%/CyberpunkVRPort/switcher-backups/optional-occluders-*`.
To undo this standalone patch, close the game and copy `MoreOccluders.reds.original`
back to `r6/scripts/MoreOccluders.reds`; for a newly installed copy, remove that
script. This separate repair does not replace the previous-port restore history.

For scripted use: `-Action PatchOccluders`, optionally with
`-MoreOccludersArchive "C:\path\download.zip"`; `-WhatIf` previews only.
During a normal port install, `-PatchMoreOccluders` opts in for an installed
copy, or `-MoreOccludersArchive` supplies a downloaded one. The patch then joins
that installation's ordinary backup/rollback/restore transaction.
Normal installation never requires this mod or patches it without opting in.
