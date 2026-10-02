CyberpunkVRPort 0.1.7 switcher
================================

Extract this entire ZIP outside the Cyberpunk 2077 game folder. Double-click
Switch-CyberpunkVRPort.bat or run scripts\Switch-CyberpunkVRPort.ps1 in Windows
PowerShell 5.1. The game must be closed to stage, install, or restore.

The menu can stage/install official upstream 0.1.7 or our PSVR2 fork. The
switcher ZIP pins the fork game-root ZIP's expected SHA-256 and planned GitHub
release URL. It downloads the fork automatically only after that GitHub asset
has been published; before then, use the local game-root ZIP with -ForkArchive.
For example:

  .\Switch-CyberpunkVRPort.bat -Action Prepare -Edition Fork -ForkArchive "C:\path\CyberpunkVRPort-0.1.7-psvr2.1.zip"

The packaged release-selection.json checks a matching local filename too.
For a differently named local ZIP, also pass -ForkSha256 with its trusted hash.
Without a matching package hash or that parameter, only a computed hash is
displayed; it is not independently authenticated.

Dependencies from GitHub are downloaded automatically and SHA-256 checked.
Nexus-account downloads are opened/identified for manual download, never
scraped. Save the specified ZIPs in Downloads; supply More Occluders (Scripted
v0.3) with -MoreOccludersArchive. Existing unrelated mods, saves, game graphics
settings, and personal port fit/input settings are preserved. An install backs
up every overwritten port file and can be restored via the switcher. Review
the printed plan before confirming.

If PSVR2/Sense hardware is detected, or the fork is selected, the switcher
prints an optional PSVR2Toolkit setup link. The toolkit, driver, and bridge
are separate; this switcher does not install or register any of them. See
docs\PSVR2-TWEAKS-017.md for the 0.1.7 integration requirements.

The optional scripts\Install-PSVR2AutoFireGrip.ps1 and patches\dualsense\
PSVR2AutoFireGrip.lua are included for the separate Enhanced DualSense Support
rapid-fire grip enhancement. They are not run by the switcher. Read the guide,
check compatibility with your CET mod, and close the game before using them.
