---
type: Release
title: CyberpunkVR Port 0.1.8 PSVR2 release candidate
description: Source and packaging provenance for 0.1.8-psvr2.1-rc1 and its checksum-pinned clean installer.
tags: [psvr2, release, installer, pacing, haptics]
timestamp: 2026-10-21
---

# Source and branch policy

Release tag: 0.1.8-psvr2.1-rc1 on satyaloka93/cyberpunk-vr-port.
Continue the existing psvr2-tweaks-0.1.7 branch; do not create another feature
branch. The release tree intentionally uses upstream0.1.8 (9aa2601) plus the
locally tested PSVR2 integration, not the retired0.1.7 rendering code or old
NPC grabbing experiments. The merge records the prior22478c5 release-lineage
parent and upstream9aa2601 parent with this reviewed0.1.8 tree. The older dirty
worktree is preserved in place, not overwritten or silently reset to the release.

Source carries Triangle-touch D-pad, Create tap/hold and alternate R3 gesture,
focus cancellation, isolated017 Toolkit haptic channel, and native-PSVR2 shot
suppression to avoid competing EDS recoil. Other headsets keep OpenXR haptics.
The bridge is required for PSVR2 vibration in this build. FOV remains on the
0.1.8 asymmetric projection route. See docs/PSVR2-PR96-018.md for integration.

# Artifact provenance

Retain the exact tested native DLL, SHA256
f93440a74da3890d3354d758787b3a4c75653c26fb26ff8fc9ff553ce401a3e1.
It matches both the existing MSVC Release build and installed game. Incremental
MSVC validation passes without changing that hash. Embedded upstream identity
remains0.1.8/9aa2601; do not mistake that for an unpatched official DLL.
Only packaged personal defaults/docs/loader-policy INI change in this release
preparation, not native gameplay code. Source shader/archive/mod packaging uses
upstream's enumerated build_dist.ps1; no account-gated mods or personal game
files are copied into the release. Runtime CPU/input audit logging is retained;
the bounded GPU capture runs only when explicitly armed.

scripts/build_psvr2_release.ps1 takes an explicitly verified DLL hash, packages
safe defaults (first_launch0/show_launcher1/FG0), emits the game ZIP, then pins
that exact ZIP's URL/SHA256 into wabbajack/release-selection.json and a separate
CyberpunkVRPort-Switcher.zip. Install-PSVR2-Fork.bat selects Fork directly;
the menu offers official upstream separately. Both require a preview/confirm.
BuildDir may be relative to this source tree; SourceRevision avoids invoking
Windows Git against a WSL-only worktree path during packaging.

Game ZIP SHA256: 493b1d3cc25120e892eccc9111c8f2d84200f97da1d1a30d3c433efdb017df21.
Switcher ZIP SHA256: cc6beedcb62dbce339d823d7e2f34083c57de4061acd9bd129aa73d0b52131a0.
SHA256SUMS.txt and PSVR2-SETUP.md are separate release assets.

# Installation and external components

The clean installer verifies backups of all originals before removing old
port-owned files, copies the staged release, and audits hashes and retirement
before success. Copy/audit failure rolls back; upgrades preserve previous
backup journals and restore newest first. It preserves unrelated mods, saves,
UserSettings, DLSS replacements and Cheeky/ReShade loaders/settings. Explicit
DisableDxgiProxy is needed to disable dxgi; Yes alone never opts into that.
No driver, layer, SteamVR binding or Windows HAGS mutation occurs.

Advanced haptics use external PSVR2Toolkit v0.3.2, protocol017 compatible with
this0.1.8 port. Steam DSX not required. Base Toolkit prerequisites, driver reuse,
the unchanged v0.3.1/v0.3.2 driver, reversible EDS native-launcher disable and
in-game DualSense audio controls are explained in docs/PSVR2-018-RC.md.

# Performance and limits

CyberpunkVR_OverlaySkipEmpty=0 keeps stable pacing ON each launch; session-only
F10 toggle in STEREO/PERFORMANCE. Not framegen's adaptive pacing. October21
post-HAGS/restart PSVR2/SteamVR run: fpsVR84.75average/89median overall, settled
near90 capture calls/s with Cheeky processing disabled and no active NR/FG.
3144x3000 output/1048x1000 DLSS input. Same-run pacing ON/OFF/ON selected windows:
XR submissions89.95/52.83/89.83 per second. These are not necessarily unique
displayed frames; HAGS-only causal effect and cross-runtime benefit unproven.
The source INI retains the tested loader throttling but no historic quality or
async overrides; it is not advertised as a measured independent speedup.

Startup/exit crashes remain unresolved; user deferred that investigation.
Unclean shutdown is not a proven cause. Do not market this prerelease as stable.
Cheeky and external OFXR are not bundled. Keep nativeFG off with externalOFXR.

# Validation

Native incremental MSVC Release succeeds with unchanged tested hash. Pure
PSVR2 input-policy regression:24 checks. Ordinary-melee/cyberware Lua replay
regressions pass at20/30/60/90/120Hz. Windows PowerShell5.1 fixture suite passes
all53 assertions using the packaged installer and final candidate ZIP: install,
upgrade, two restores, loader/settings preservation, checksum/dependency refusal
and injected corruption rollback. Archive integrity and OKF validation pass.
Published automatic download is the final post-publication check on a disposable
game, not the user's active installation. No live haptic-channel test is run while it could write to
the user's controller bridge mapping.

# Citations

- [Upstream0.1.8](https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.1.8)
- [PSVR2Toolkit v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)
- [Fork release](https://github.com/satyaloka93/cyberpunk-vr-port/releases/tag/0.1.8-psvr2.1-rc1)
