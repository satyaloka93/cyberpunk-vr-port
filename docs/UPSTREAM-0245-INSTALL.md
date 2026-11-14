# Upstream 0.2.45 testing with the switcher

Run `Switch-CyberpunkVRPort.bat` from the extracted installer folder:

- **1 — Install official upstream 0.2.45** (confirmation required).
- **2 — Download/check upstream 0.2.45**, without installing.
- **4 — Install published PSVR2 0.2.3 RC1**.
- **5 — Restore previous port**, including locally patched DLLs and settings.
- **11 — Read-only installation and recent-log diagnostics**.

`Install-Upstream-0245.bat` opens the official install directly. The older
`Install-Upstream-023.bat` remains a compatibility launcher for the same current
upstream selection; it does not pin the old release.

Requires Cyberpunk 2077 **2.5**, an OpenXR runtime, and **RED4ext 1.31.0 or newer**.
The switcher verifies/downloads RED4ext automatically; this edition needs no
Nexus sign-in. Requirements were checked against the tagged README and the
actual release ZIP's INSTALL.txt. Existing frameworks and unrelated mods remain
installed. Cheeky, Toolkit and PureDark's optional AFW plugin are not dependencies.

The install backs up the current port, removes its old files, and installs the
unmodified official payload. Personal fit/input preferences and calibration
migrate; old render/framegen/debug tuning does not. Built-in framegen starts OFF,
first-launch graphics preset installation is disabled, and your game settings,
saves, DLSS replacements and Cheeky/ReShade loaders/settings are preserved.
No Toolkit driver or OpenXR layer changes are made.

**For your exact current fork, including local scanner fixes, return using option 5.**
Option 4 downloads the published RC1 and does not contain later local test fixes.

Upstream now opens the scanner with the **physical left hand at the left ear +
left-stick click (L3)**; click again to close. This differs from our fork's grip
gesture. Version 0.2.45 fixes HUD blinking with framegen off on non-SteamVR
runtimes. Its documented default framegen is NVIDIA OFA + FidelityFX, but the
switcher still leaves generation off until you enable it for the test.

Config: `red4ext/plugins/cyberpunkvrport/config/vrport.ini`.
Log: `red4ext/plugins/cyberpunkvrport/log/cyberpunkvrport.log`.
The modern config location is used even though parts of INSTALL.txt retain
legacy `bin/x64/vrport.ini` wording.

[Official release](https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.2.45).
ZIP SHA-256: `64b23fb8f211cb5c3df896676a7d9c8ebf6e70d3affaa4f280ffd39d9cfab6cf`.
Optional advanced haptics: [PSVR2Toolkit bridge](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2).
