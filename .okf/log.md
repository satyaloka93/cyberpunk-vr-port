# Update Log

## 2026-10-27

* User accepted aiming correction. Deployed Overture assisted case extraction and preserved body-grab grip after ejection, with actual Lua extraction tests at30/60/90Hz; runtime visual verification pending. See [migration evidence](game-250-migration.md).

* Live aiming split: empty hand muzzle snapshots; restore native visual-recoil timing, direct Game publisher and accepted/rejected diagnostics. Correct Rosco source track 8 and prevent same-grip reload reseating. Build and focused tests pass; runtime confirmation pending. See [migration evidence](game-250-migration.md).

## 2026-10-26

* **Script compilation repaired and deployed**: First runtime loaded all five native plugins but failed redscript. Migrated weapon equip/selection signatures, right-hand weapon state and optics helper; patched local LUT reference comparison and restricted MoreOccluders to exposed ordinary meshes. Whole installed-mod snapshot compiles with game SCC0.5.32. Seven scripts backed up/deployed (receipt20261026-150755-390); native DLL/settings untouched. Gameplay validation and separate KeypadCursor hook remain pending. See [migration status](game-250-migration.md).

* **Deployed for first2.50 test**: Fourth integrated MSVC build9829e6a6 passes; nine compatibility-policy cases pass. Final camera/marker/HUD/optics/partition migrations and composition fallback offset corrected. Enabled only the exact2.50 SDK/engine profile and installed DLL/PDB with verified backup20261026-142514-163. Settings/scripts/shaders/layers unchanged; no game launched, no runtime acceptance or publication claimed. See [migration status](game-250-migration.md).

* **Native migration implemented, not deployed**: Updated camera/DLSS, render graph/submission, resource pools, HUD/grading, framegen input, movement and aiming layouts using paired2.31/2.50 native evidence. Three integrated MSVC builds pass; latest checkpoint daed3208. Added native operand and generated-thunk checks, including fifth-argument forwarding and live-register preservation fixes. Remaining camera/marker/optics work keeps the compatibility guard closed. Native stereo explicitly deferred. See [migration status](game-250-migration.md).

* **Reference recovered**: Steam QR download retrieved the exact recorded 2.31 EXE and matching address JSON into the research folder. Stable identities map 124 of 133 registered hooks. Native comparison proves camera descriptor, DLSS, Present and calling-contract changes; six scanned hook patterns fail on 2.50. Preserved reproducible mapping/body evidence; no game files changed or new VR DLL deployed during retrieval. See [migration status](game-250-migration.md).

* **Repair in progress**: Installed seven official2.50 prerequisite packages with verified backups; preserved settings and installed VR DLL. Current full-memory dump and REA identify ArchiveXL callback handler+0x20 versus game's+0x40 as the startup crash. Updated official SDK and built a fail-closed compatibility guard, not deployed. Audited133 engine hooks; private-address/layout migration remains incomplete. See [migration status](game-250-migration.md).

* **Diagnosed only**: Five2.50 startups fail during ArchiveXL1.27.1 initialization; CET1.37.1 explicitly rejects2.50. Windows and Cheeky VEH agree on a non-executable heap target. VR DLL never loads; its old SDK and two mismatched native guards establish separate migration work. Saved logs/REA evidence; no installed changes. See [startup analysis](game-250-startup.md).

## 2026-10-21

* **Deployed**: Verified cf6d6986 haptics DLL/PDB installed with backup, local routing set Auto2; all other tuning, scripts and bindings preserved. Four Windows haptics/input suites,21 driving tests and25 Lua scenarios pass. Physical acceptance pending. See [generic feedback](generic-openxr-haptics.md).

* **Restored**: Found prior generic source in fsr3-worktree (vehicle commit7297823). Restored Quest swing output, weapon-strength recoil, weaker support-hand pulse and audio-derived driving rumble. Added startup bridge detection and PSVR2 native fallback; no live handoff or publication. See [generic feedback](generic-openxr-haptics.md).

* **Repaired and deployed**: Runtime confirmed zero native haptic outputs. Created/selected a personal binding adding both outputs while preserving inputs and all other settings. Restored exact tested7bc109a driving toggle, passed21 driving tests, deployed verified DLL/PDB. See [haptic repair](haptic-routing.md) and [grip toggle](vehicle-grip-toggle.md).

* **Deployed selector**: DLL/PDB plus explicit Toolkit preference installed with three-file backup and checksum verification; other settings and SteamVR bindings unchanged. See [haptics routing](haptic-routing.md).

* **Local implementation**: Optional PSVR2 output selection, native/default for all headsets, restart-latched exclusive Toolkit route, output tests/logging. Found selected Workshop binding has no haptic outputs. Keep 0.1.8 asymmetric projections; no SteamVR mutation or PR/release publication. See [haptics routing](haptic-routing.md).

* **Published and downloaded**: RC1 is public as a prerelease; GitHub asset digests match. Downloaded the public installer and exercised its pinned fork download/staging on a disposable game. Correct tested DLL, no fixture changes. Source/tag on the existing PSVR2 branch; prior dirty worktree preserved. See [release candidate](release-018.md).

* **Release preparation**: Collected the tested modified 0.1.8 source, same-branch lineage, clean installer, pinned fork download and PSVR2/HAGS instructions. See [release candidate](release-018.md).

- 2026-10-26: Documented and deployed cascade cache freshness repair for right-eye shadow investigation; native flag audit and portable policy tests retained. Visual outcome pending.

- 2026-10-26: Positive lighting test reported; log shows old cascade sharing rejected. Restored local0.1.7 independent bike/car camera sliders and persistence into current2.50 build, retaining existing calibration.

## 2026-10-26

Weapon settings repair

Persisted laser aim setting; backed up and removed obsolete three-slot cigarette
weapon override; migrated semi-auto hand gate and shot counter. Native build and
whole staged SCC compilation passed; deployed DLL SHA256
`adaa4cba8609f7812ec9eee8cb511fa8082c70a1f7d11d2553182536c96e1d43`.
Independent dual-trigger firing remains unimplemented; method audit captured.

Dual-wield EDS firing-object/state selection deployed; eleven selector cases
passed. Temporary read-only equip audit added for unresolved shotgun issue.

Right recoil moved to player shot events; right animation feature migrated to
2.50, left native recoil preserved. Exact equip transitions and first cascade
failure reasons now logged; native build and SCC validation passed.

Fog shader migration: seven exact2.50 replacements validated/deployed,26 mono
ray components corrected; native stereo branches unchanged. Native camera/
shader eye-contract research documented; headset native stereo not enabled.

## 2026-10-26

Independent firing feedback and launcher double-cycle.

Recorded per-hand shot-counter/EDS/Toolkit candidate, validation and deployment
in [2.50 migration](game-250-migration.md). Fixed the press/release duplicate
cycle path that matches Testera flashing then skipping after the arm launcher.
Hardware confirmation pending.

- 2026-10-27: Added and deployed accessible chest/waist spare-magazine grab; tests pass, headset confirmation pending. See game-250-migration.md.

- 2026-10-27: Fixed revolver latch/velocity cleanup and deployed bounded reload audit; distortion and aiming investigation remains open.

- 2026-10-27: Added tested wrist-roll cylinder closure and tracked-hand validity checks; laser/fire symptom still under investigation.

- 2026-10-27: Analyzed18:16 crash; fixed native reload-write cleanup on unsupported weapon switches and stable-ID audit. Crash attribution remains unknown.

- 2026-10-27: Deployed dedicated native dual-wield grip vibration, per-hand pending Toolkit pulses and no-duplicate Lua adapter; runtime feel pending.

- 2026-10-27: Built/tested active-rig isolation, stable closed crane writes and surface-range laser; deployment pending game close.

- 2026-10-27: Analyzed 18:43 crash with CDB; documented damaged pointer and dump limits. Corrected reload float-track remapping/capacity and removed cross-pass bone pin fallback. Built/tested/deployed with pending laser-distance and closed-cylinder-pose candidate; working dual haptics preserved. REA whole-game startup timed out.
- 2026-10-27: Built/tested/deployed per-weapon muzzle snapshots and isolated cone caches for 2.50 dual wield; restored absolute cylinder writes through same-component canonical mapping. Runtime impact/reload confirmation pending; haptics unchanged.
