# Upstream 0.2.0 switcher

Extract the whole switcher ZIP outside the game, close Cyberpunk and mod managers,
then run **Switch-CyberpunkVRPort.bat**:

- **1** previews/downloads upstream 0.2.0 without changing game files.
- **2** installs upstream 0.2.0 after confirmation.
- **4** still installs the published PSVR2 0.1.9 RC2 fork.
- **5** restores the previous port from its verified backup.
- **9/10** retain the game-2.5 VRCAM comparison.

Game 2.5 is required. Official 0.2.0 needs only **RED4ext 1.31.0**; the switcher
checks/downloads that public dependency and verifies SHA-256. No Nexus download
is required for this edition. Fork/VRCAM retain their separate dependency list.
Source: https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.2.0

The new plugin is `red4ext/plugins/cyberpunkvrport/cyberpunkvrport.dll`.
It contains its own Lua host, redscript compiler, archive and framework support.
Existing CET, redscript, ArchiveXL, TweakXL, Codeware, Input Loader and unrelated
mods remain installed: those mods may still require their frameworks.

The switcher backs up old port files before removing old native plugins,
port-owned CET/redscript/tweak/archive/input files and `vrcam_cpu_tweaks.ini`.
It also recognizes the new directory on later installs or a switch back, avoiding
two active port copies. Save games, game UserSettings, DLL replacements,
Cheeky/ReShade loaders/settings and unrelated mods are preserved. More Occluders
remains an optional separate mod, not a dependency or an automatic patch.

Personal VR preferences are filtered into `config/vrport.ini`; calibration,
launcher settings, matching grip poses and port Lua INI/JSON user data migrate
between old and new layouts. Old Lua code, logs and VRCAM render tuning are not
restored into the new host. Originals remain in the transaction backup.
First-launch graphics overwrite is disabled and built-in framegen starts OFF.
The new host seeds Lua hotkeys from the retained CET bindings on first launch.

Upstream 0.2.0 is not feature-identical to our fork: it lacks our Quest thumb-rest
modifier and full F10 audio/OpenXR/Toolkit selector. It routes PSVR2 haptics to
Toolkit; other headsets use OpenXR. Advanced PSVR2 setup remains external:
https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2
The switcher does not install drivers, start/stop bridges or change OpenXR layers.

Current config: `red4ext/plugins/cyberpunkvrport/config/vrport.ini`.
Current logs: `red4ext/plugins/cyberpunkvrport/log/`.
Use the switcher's restore option for an exact rollback, rather than upstream's
uninstaller. Keep the switcher backup directory.
