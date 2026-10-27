# Switch to upstream 0.1.9

Run `Switch-CyberpunkVRPort.bat`: choose **1** to stage/preview or **2** to install.
Installation requires game 2.5, a closed game and typed SWITCH confirmation.
The official ZIP is pinned to SHA-256
`adc255885f24ede400c1510a9dd34ae5e32a3a61b3cd55bef3a017f802d48bf0`.

The switcher checks the minimum RED4ext, CET, ArchiveXL, TweakXL and Codeware
versions, retaining newer versions. redscript has no file version: the known
0.5.32 compiler hash is accepted; unknown builds are replaced by the pinned
0.5.32 package. Missing/outdated frameworks download automatically with pinned
checksums. Visual Holsters, Nova Optics and More Occluders retain the Nexus
manual-download flow. Equipment-EX and Input Loader remain required by the
upstream README. Visible Bullets is no longer required; existing copies remain.

Each install creates a fresh backup of the CURRENT port. Choose **5** to restore
that backup. The published fork menu option still points to the older 0.1.8 RC1,
not the local game-2.5 development checkpoint: use Restore for the latter.
Git checkpoints preserve source; these switcher backups preserve installed files.

Personal fit/calibration settings, unrelated mods, DLSS replacements and game
UserSettings.json are preserved. Old port files are removed after backing up;
new port rendering defaults are used, first-launch graphics overwrite is disabled
and built-in framegen starts off. Cheeky/ReShade loaders and settings are retained.
The switcher does not start/stop the Toolkit bridge or change OpenXR layers.

Requirements and changes:
https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.1.9
https://github.com/dariulone/cyberpunk-vr-port/blob/0.1.9/README.md#requirements
