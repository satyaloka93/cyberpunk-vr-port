CyberpunkVRPort 0.1.8 PSVR2 release-candidate installer
====================================================

Extract the ENTIRE CyberpunkVRPort-Switcher.zip outside the game folder.
Close Cyberpunk, REDlauncher and mod managers.

Easiest route: double-click Install-PSVR2-Fork.bat.
It downloads our 0.1.8-psvr2.1-rc1 game-root ZIP, verifies its pinned SHA-256,
stages dependencies and shows a plan. Type SWITCH only after reviewing it.
No GitHub account or manual port-ZIP download is needed.

For upstream/fork choices, preview-only, dependency status, Nexus links or
restore, run Switch-CyberpunkVRPort.bat. Option 4 installs our fork; option 2
installs OFFICIAL upstream without our PSVR2 bridge integration.

Missing GitHub dependencies download automatically with checksums. Nexus
downloads require your own signed-in manual download. Save ZIPs in Downloads
and rerun; follow the displayed instructions for More Occluders Scripted v0.3.
HUDitor is NOT required and is disabled into backup.

A clean install verifies backups of all affected originals, removes old
port-owned files, writes the staged release, then verifies hashes and retired
file removal. Failure rolls back. Unrelated mods, saves, game UserSettings,
DLSS replacements, dxgi/version loaders and Cheeky/ReShade settings are kept.
Personal fit/input/FOV and calibration migrate; old render/FG/debug tuning
does not. First-launch graphics overwrite is disabled; built-in FG starts OFF.

Do not delete dxgi.dll blindly. Only explicit -DisableDxgiProxy disables that
loader with a backup; -Yes alone does not. A different incoming loader is
blocked for review. -KeepReShade is a legacy no-op compatibility option.
Preservation is not a guarantee of third-party compatibility.

Upgrades get independent backups; Restore unwinds newest first. Interrupted
Prepared transactions require recovery first. Never delete recovery journals.
The switcher does not wipe shared script caches, install Toolkit drivers,
register OpenXR layers, change HAGS, or launch/stop a bridge.

Read docs\PSVR2-018-RC.md for controls, performance and haptics setup:
https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2
Steam DSX is NOT required. The compatible Cyberpunk 017 protocol bridge is
also used by this modified 0.1.8. Without it this fork has no PSVR2 native
OpenXR vibration fallback. Audio controls are in Mods > DualSense settings >
PSVR2 Haptics, not F10. Existing compatible drivers can be retained.

Stable pacing with no overlay graphics defaults ON at every launch. This is
NOT framegen's adaptive pacing switch. HAGS ON plus a Windows restart helped
our tested setup; the guide gives measurements and limitations. Startup/exit
crashes remain under investigation: this is a release candidate.

For a manually downloaded port ZIP:
  Switch-CyberpunkVRPort.bat -Action Prepare -Edition Fork -ForkArchive "C:\path\CyberpunkVRPort-0.1.8-psvr2.1-rc1.zip"
The pinned filename uses its expected checksum. For a renamed ZIP pass its
trusted -ForkSha256 explicitly. Without a pin, a computed hash alone is not
independent authentication.

Current requirements: Cyberpunk 2.31, RED4ext, CET, redscript, ArchiveXL,
TweakXL, Codeware >=1.20, Input Loader, Equipment-EX, Visual Holsters, Visible
Bullets, Nova Optics and More Occluders Scripted v0.3. Existing dependency
presence (plus Codeware's version gate) is not a runtime compatibility test.

Legacy 0.1.7 instructions and the optional AutoFireGrip helper are retained for
reference. Do not run the helper automatically or replace EDS profiles blindly.
