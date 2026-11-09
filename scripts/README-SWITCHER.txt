CyberpunkVR Port - game 2.5 / PSVR2 0.2.3 RC1 or official upstream 0.2.3

Extract the whole ZIP outside the game. Run Switch-CyberpunkVRPort.bat.
1 Install official upstream 0.2.3
2 Download/check official upstream without installing
3 Download/check our PSVR2 0.2.3 RC1 without installing
4 Install our PSVR2 0.2.3 RC1 (also supports Quest/OpenXR)
5 Restore the previous installed port
11 Check installation and recent errors (read-only), including Cheeky

Install-PSVR2-Fork.bat opens the RC1 install directly.
Install-Upstream-023.bat opens the official install directly.
Download/check stages a preview; it does not change your game.
Install verifies downloads, backs up the old port, removes it cleanly and installs
new files. Personal fit/input settings migrate; render/FG/debug tuning resets.
Saves, unrelated mods, DLSS replacements and game graphics settings are preserved.
Close the game and mod managers first. Keep the backup journals for option 5.

Both modern targets require RED4ext 1.31.0, downloaded automatically. No Nexus
account is needed for the port. Optional advanced Toolkit haptics have extra setup:
docs/PSVR2-023-RC1.md. OpenXR vibration works without Toolkit or Steam DSX.

RC1 is our 0.2.2-based fork with selected 0.2.3 backports, not all stock features.
Generation starts OFF. NVIDIA OFA + V557, Medium / 50%, is preselected in F10.
The SteamVR optimization package is ON (restart required to change); VDXR bypasses
it automatically. Adaptive real-frame pacing is ON. Diagnostic recording is OFF.

Existing Cheeky/ReShade loaders and settings are preserved. Option 11 identifies
known loaders and extracts recent errors without changing files or uploading logs.
See docs/CHEEKY-DIAGNOSTICS.md. Unknown hashes are not automatically a fault.

Options 9/10 retain the game-2.5 VRCAM 0.1.8 comparison, with its older dependencies.
Option 8 can patch a recognized optional More Occluders script; modern ports do
not require it. Use option 4 to return to RC1; option 5 restores your exact backup.
