---
type: Installer
title: CyberpunkVRPort 0.1.7 release switcher
description: Staged, checksum-checked upstream/fork switching with backups and optional PSVR2Toolkit guidance.
tags: [release, installer, dependencies, psvr2]
timestamp: 2026-10-01T21:00:00+09:00
---

# Distribution

`scripts/build_dist.ps1 -Zip` creates a separate
`CyberpunkVRPort-Switcher.zip` beside the game-root ZIP. Do not put switcher
scripts into the game folder. The switcher ZIP contains its `.bat` entrypoint,
PowerShell core, pinned `wabbajack/manifest.json`, a user guide and the
[PSVR2 integration instructions](psvr2-input-bridge.md). It also carries the
optional rapid-fire grip helper/installer, but never runs that third-party CET
patch automatically. The Toolkit bridge and driver remain external.

# Selection and dependencies

Official upstream 0.1.7 has a pinned GitHub URL and SHA-256. Packaging also
generates `wabbajack/release-selection.json` with the fork ZIP's SHA-256 and
planned GitHub release URL. Automatic download works once that asset is
published; before then, `-Edition Fork -ForkArchive` selects the local ZIP.
Matching local filenames use the generated checksum; for a renamed archive,
`-ForkSha256` can independently validate it. Without either pin, local
selection shows only a computed hash. Safe ZIP extraction
rejects traversal, duplicate paths, executable replacement and unexpected
roots.

The script checks installed mod presence, reuses it, downloads missing GitHub
dependencies with pinned hashes, and presents Nexus links/manual ZIP handoff
for account-gated mods. It stages first, then prints a plan. The transaction
backs up overwritten files, verifies copies and rolls back on failure;
`Restore` reinstates the previous port. Game saves, unrelated mods and game
UserSettings are not overwritten. Migrated `vrport.ini` preferences include
`xr_force_fov` while experimental render and FG settings reset.

# PSVR2 handoff

Detection of a PnP name containing PlayStation VR2/PSVR2/Sense, or explicit
selection of the fork, prints the optional
[PSVR2Toolkit v0.3.1 release](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.1) link. The
switcher does not install Toolkit, drivers or system-wide OpenXR layers.
Use the Toolkit Cyberpunk 0.1.7 bridge rather than the legacy shared-slot
bridge; see [integration](psvr2-input-bridge.md).

# Validation and limits

Windows PowerShell suite: 30 assertions pass against a temporary fake game,
including actual fork-ZIP install/restore, FOV migration, missing-Nexus stop,
rollback and fork-checksum rejection before game-file changes. Both ZIPs built
locally, passed archive integrity checks, and the packaged switcher successfully
staged the final fork ZIP against a temporary game fixture without changing its
game files. This is not a full
real-game install/restore test; headset verification, source commit and
published checksums remain release gates. The PSVR2 PnP hint is advisory, not proof that
the correct Toolkit driver or bridge is installed.
