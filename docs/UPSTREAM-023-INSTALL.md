# Upstream 0.2.3 switcher

Extract the whole switcher ZIP outside the game, close Cyberpunk and mod managers,
then run **Switch-CyberpunkVRPort.bat**:

- **2** downloads/checks upstream 0.2.3 without changing game files.
- **1** installs upstream 0.2.3 after confirmation.
- **4** installs our PSVR2 0.2.3 RC1 fork.
- **5** restores the previous port from its verified backup.
- **9/10** retain the game-2.5 VRCAM comparison.

Game 2.5 is required. Official 0.2.3 needs only **RED4ext 1.31.0**; the switcher
checks/downloads that public dependency and verifies SHA-256. No Nexus download
is required for this edition. RC1 uses the same RED4ext requirement; the VRCAM comparison retains its older dependencies.
Source: https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.2.3

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

Upstream uses its own controls and haptics. Optional advanced PSVR2 setup remains external:
https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2
The switcher does not install drivers, start/stop bridges or change OpenXR layers.

The 0.2.3 release notes and shipped INSTALL.txt explicitly remove Visual Holsters,
Equipment-EX, Nova Optics and More Occluders as requirements; the tagged README
still lists them. This installer follows the release notes and shipped instructions.
Valentino Holsters (Nexus 18100) is optional cosmetic equipment; PureDark's AFW
plugin is optional. Neither is downloaded or required. Upstream labels AFW/OSR
unplayable experimental performance tests; normal engine stereo remains default.

The archive SHA-256 is
`2269fa6d5f21b34ab6bb72f86a7e3a4c710d5a5c370e992eb1032a862ba4cddc`.
The shipped `dxc/dxcompiler.dll` and `dxc/dxil.dll` are required by the new
single-pass stereo shader compilation and checked during staging. Existing
DLSS DLLs are preserved; the official upstream plugin and SDK files are installed
unmodified. No local diagnostics or SDK rebuild is substituted.

To return to the exact locally patched fork you had before installing, choose
**5 Restore previous port**. **4 Install PSVR2 0.2.3 RC1** downloads the tested fork candidate.

Current config: `red4ext/plugins/cyberpunkvrport/config/vrport.ini`.
Current logs: `red4ext/plugins/cyberpunkvrport/log/`.
Use the switcher's restore option for an exact rollback, rather than upstream's
uninstaller. Keep the switcher backup directory.
