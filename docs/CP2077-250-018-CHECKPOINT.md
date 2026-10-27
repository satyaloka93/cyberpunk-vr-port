# Cyberpunk 2.50 checkpoint on port 0.1.8

Branch: `cp2077-2.5-base-0.1.8`.
Base: fork checkpoint `08f0af3`, following 0.1.8 release commit `0922782` and
upstream 0.1.8 source `9aa2601`. This is a development checkpoint, not a release.

## Preserved work

- Exact-game compatibility guard, 2.50 SDK pin and engine hook/layout migration.
- Script API updates, stereo lighting/fog/reflection corrections and VR overlays.
- Optional OpenXR versus exclusive Toolkit haptics, per-hand dual firing feedback,
  driving grip toggle and separate vehicle camera controls.
- Restored native visual-recoil timing, per-weapon muzzle publication and validation.
- Reload rig remapping, Overture round visibility, crane control and assisted ejection.
- Existing 0.1.8 pacing and PSVR2 base changes remain in branch history.

## Validation and limits

Latest native build passed MSVC Release; deployed DLL SHA256:
`eef7696ca6b97285ba50d47e572d5c77ee0d23f5bba5ac3bca57a76d2c103004`.
User confirmed aiming much improved/apparently fixed and haptics working.
Later reload-only deployment at 2026-10-27 20:30 KST is included, but its visual
result is still pending. Crash-free behavior and complete weapon coverage are not
established. See `.okf/game-250-migration.md` for evidence and remaining issues.

The SDK submodule points to public wopss/RED4ext.SDK commit
`5246e659d783e5834cc08a90e56c8932c9ed3449`. Local Version.hpp may appear dirty
because of CRLF conversion; it has no content diff and requires no private patch.
Local game dependency installations, user settings, dumps, logs and Toolkit bridge
binaries are separate from this source checkpoint.

## Next: upstream 0.1.9

Keep this branch as the rollback/comparison point. Inspect upstream 0.1.9 changes
in an isolated checkout before deciding which compatibility fixes remain needed.
Do not overwrite this working tree or deploy 0.1.9 during source comparison.
Compare game version/SDK support, shot evaluation timing, native dual-hand routing,
reload rig mapping, stereo fixes, pacing and controller bindings. Reuse regression
checks in `tools/game250_regression_tests`; confirm surviving engine hooks against
the target executable. Preserve installed binaries/settings before a later test
switch. A successful build alone does not establish runtime compatibility.
