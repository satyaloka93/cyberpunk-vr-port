CyberpunkVR Port - game 2.5 / PSVR2 0.2.45 RC1 or official upstream 0.2.45

Extract the whole ZIP outside the game. Run Switch-CyberpunkVRPort.bat.
1 Install official upstream 0.2.45
2 Download/check official upstream without installing
3 Download/check our PSVR2 0.2.45 RC1 without installing
4 Install our PSVR2 0.2.45 RC1 (also supports Quest/OpenXR)
5 Restore the previous installed port
11 Check installation and recent errors (read-only), including Cheeky

Install-PSVR2-Fork.bat opens the RC1 install directly.
Install-Upstream-0245.bat opens the official install directly.
Download/check stages a preview; it does not change your game.
Install verifies downloads, backs up the old port, removes it cleanly and installs
new files. Personal fit/input settings migrate; render/FG/debug tuning resets.
Saves, unrelated mods, DLSS replacements and game graphics settings are preserved.
Close the game and mod managers first. Keep the backup journals for option 5.

Both modern targets require RED4ext 1.31.0, downloaded automatically. No Nexus
account is needed for the port. Optional advanced Toolkit haptics have extra setup:
docs/PSVR2-0245-RC1.md. OpenXR vibration works without Toolkit or Steam DSX.

RC1 is our 0.2.2-based fork with selected 0.2.45 backports, not all stock features.
Generation starts OFF. NVIDIA OFA + V557, Medium / 50%, is preselected in F10.
The SteamVR optimization package is ON (restart required to change); VDXR bypasses
it automatically. Adaptive real-frame pacing is ON. Diagnostic recording is OFF.

Existing Cheeky/ReShade loaders and settings are preserved. Option 11 identifies
known loaders, checks 64-bit OpenXR layer registrations/versions/environment overrides,
and extracts recent errors without changing files or uploading logs. It flags possible
stale/duplicate Cheeky layers and gives matching-package repair guidance; no automatic repair.
Option 11 now shows a concise summary; known harmless hooks and recovered searches
are grouped, and stale/missing legacy logs are counted. -DetailedDiagnostics prints
full detail; -ReportPath exports summary plus full evidence.
See docs/CHEEKY-DIAGNOSTICS.md. Unknown hashes are not automatically a fault.

Options 9/10 retain the game-2.5 VRCAM 0.1.8 comparison, with its older dependencies.
Option 8 can patch a recognized optional More Occluders script; modern ports do
not require it. Use option 4 to return to RC1; option 5 restores your exact backup.

Upstream 0.2.45 scanner: left hand at left ear + left stick click (L3), click again to close.
Our fork uses its own controls. See docs/UPSTREAM-0245-INSTALL.md.
Option 5 restores your exact pre-test build, including local scanner fixes; option 4 downloads published RC1.
