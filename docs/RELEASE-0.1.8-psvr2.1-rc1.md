# CyberpunkVR Port 0.1.8-psvr2.1-rc1 — PSVR2 release candidate

Based on [dariulone's upstream 0.1.8](https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.1.8),
with our tested PSVR2 controls/haptics integration and stable-pacing toggle.
This is a prerelease, not an upstream release or a stability guarantee.

## Download and install

Download **CyberpunkVRPort-Switcher.zip**, extract the entire ZIP outside the
game folder, and run **Install-PSVR2-Fork.bat** with the game closed.
It automatically downloads **this 0.1.8 PSVR2 fork**, verifies its SHA-256,
stages dependencies, and asks for confirmation before a clean replacement.
No GitHub account is required. Nexus dependencies still require your own
signed-in manual download; the installer supplies links and instructions.

For preview/upstream/restore, use **Switch-CyberpunkVRPort.bat** instead.
Option 4 installs this fork; option 2 installs official upstream without our
isolated PSVR2 bridge integration. Do not use an older switcher ZIP that still
selects the 0.1.7 fork. The game-root ZIP is also supplied for manual users.

Old port files are backed up before removal, new files are hash-verified, and
failure rolls back. Saves, unrelated mods, game settings, DLSS replacements,
Cheeky/ReShade loaders and their settings are preserved. HUDitor is no longer
a dependency. First-launch graphics overwrite is disabled; built-in FG starts OFF.

## Changes and defaults

- Upstream 0.1.8 rendering and asymmetric per-eye projection retained.
- PSVR2 Triangle touch + right stick emulates D-pad, suppressing look while held.
- Create tap gives Start/Pause; hold at least 500 ms gives Back/Select.
  Triangle touch + R3 is the alternate gesture; L3+R3/F10 opens the overlay.
- PSVR2 VR haptic events use the isolated Toolkit bridge channel, avoiding
  competing OpenXR vibration writers. EDS/Toolkit owns weapon recoil and
  adaptive triggers. Other headsets retain upstream OpenXR haptics.
- **Stable pacing with no overlay graphics defaults ON at every launch**:
  F10 > STEREO > PERFORMANCE. Keeps the small submission/fence wait that improved
  SteamVR headset delivery when overlays disappear (including vehicle use).
  Extra synchronization is the tradeoff; uncheck for upstream behavior.
  This is separate from native framegen's adaptive pacing checkbox.
- FOV controls remain available with the 0.1.8 asymmetric projection path.
- Clean installer, per-upgrade backups, audited rollback, updated dependencies
  and explicit loader preservation. No system-wide layer/driver changes.
- Retains the tested loader-throttling INI section, without historical
  quality-reducing or async-compute overrides. No isolated speedup claim for it.

## Advanced haptics — separate Toolkit package

Use **[our PSVR2Toolkit v0.3.2 full package](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)**.
**Steam DSX is not required.** Do not run DSX, EDS UDPClient or a second bridge.
The package has separate driver/ and bridge/ directories. Its Cyberpunk 017
protocol bridge is compatible with this modified 0.1.8.

The stock Sony driver lacks Toolkit's controller API. New users must complete
the [base Toolkit installation](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation)
first. Keep a compatible existing Toolkit driver; v0.3.2 uses the same driver
as v0.3.1/v0.2.0, so those users need no driver update. Our driver adds raw-trigger
transport and fixes stale grip-PCM attenuation; raw-trigger support alone is not
a reason to replace a working compatible driver. Replacement is explicit and backed up.

Install Enhanced DualSense Support and Native Settings UI, then run Toolkit's
bridge/setup_cyberpunk017.cmd with the game closed. Keep the EDS CET mod; setup
disables only its separate native launcher DLL and UDP autostart with backups.
Each session: SteamVR/controllers, then bridge/run_cyberpunk017.cmd, then game.
Without the bridge this PSVR2 fork has no native OpenXR vibration fallback.

Audio controls: **Mods > DualSense settings > PSVR2 Haptics**, not F10.
Audio defaults OFF; turning it off preserves triggers/recoil/melee pulses but
removes audio-derived road rumble. Audio includes ambience/music/default Windows
output, not just weapon events. Full steps are in **PSVR2-SETUP.md** below.

## Performance notes

Try **HAGS ON and restart Windows**; keep the pacing toggle ON. Our tested
PSVR2/SteamVR session after HAGS/restart recorded **84.75 average / 89 median
fpsVR FPS**, including a settled near-90 section at 3144 x 3000 output with
DLSS 1048 x 1000 input and without active NR/framegen. Cheeky processing was
disabled in the later measured section. This is not a controlled HAGS-only
benchmark or a universal FPS guarantee; hardware, scene and settings matter.

In that same run, pacing ON/OFF/ON produced about 90/53/90 XR submissions per
second in selected settled windows. Submissions are not necessarily unique
displayed frames. The observed benefit supports default-on pacing on the
tested SteamVR setup, even with HAGS enabled.

Keep built-in framegen OFF when using external OFXR; do not stack both.
Cheeky fixes remain a separate addon release and are not bundled here.

## Validation / known limitations

The native DLL is the exact tested/deployed binary, SHA-256
`f93440a74da3890d3354d758787b3a4c75653c26fb26ff8fc9ff553ce401a3e1`.
Its embedded upstream identity remains 0.1.8/9aa2601; this release tag contains
the patched source and packaging recipe. Native build validation, input/melee
regressions and real-ZIP installer/upgrade/restore tests are run before publishing.
Bounded CPU/input logging remains; optional GPU capture is idle until armed.

Startup/exit crashes remain unresolved and their investigation is deferred.
Unclean shutdown has not been established as their cause. This candidate does
not claim to include every experimental feature from older forks.
