# Lobby startup audit — 2026-11-17

Compared with pristine upstream0.2.45 commit
`6f33af62c50a85f88eceaa63f776539d8fc6f395` (latest release verified on GitHub).
The lobby controller, intro, startup skip hook, loading renderer, MenuMode hook,
EngineStereo LoadingShown/OnePicture/LoadingSideBySide functions and their
30-present loading timeout match upstream. Startup feature toggles remain our
selective launcher adaptation. CreationTarget differs only in newer mirror
customization helpers; lobby items.lua differs by a comment. Neither difference
changes startup composition or load-screen lifetime. No broad lobby import needed.

Installed35 Lua/sound lobby files,2 engine-stereo redscripts and the port archive
match maintained source. No active duplicate CET Lobby init exists. Port script
log reports VRLobby ready; the latest inspected boot reports main menu reached
and boot notice ending. Installed xr_lobby=0, so this is not a lobby-enabled test.
The old local-build.json is stale metadata; deployed DLL was identified by its
actual SHA256, not that file.

## Recovered regression guard

The2026-11-02 blank/flashing lobby was documented in the0.2.0 worktree's
`docs/LOBBY-COMPOSITION-COMPATIBILITY.md`: world and menu were ready, curtain open,
but composition rejected the grading signature and left the flat backdrop.
Only the first8 floats (32 bytes) were consistently repeated. Remaining48 bytes
were zero or stale words. The corrected build was user-confirmed at the time.

Current upstream compares72 bytes, ignoring only the final8 padding bytes. That
fix remains intact as the primary match, but it still rejects our recorded older
variant. Restored the validated32-byte fallback in
`include/Lobby/CompositionGrading.hpp`, requiring finite, non-denormal, nonzero
leading grading rows. The production caller retains all upstream weight,
whole-pixel dimension, saturation and aberration guards. No signature bypass,
loading-timeout change, SDK change, or framegen modification.

`tools/lobby_composition_tests/fixture.hpp` contains exact20-word first/second
values from root artifact
`build/upstream-0.2.0/build-isolation-no-fg-logging/run-grading-words/cyberpunkvrport.log`,
lines546 onward. Tests show upstream's72-byte comparison rejects that fixture;
the fallback accepts it. Additional checks retain full72/80-byte compatibility,
ignore only unstable tail data, and reject malformed weights/dimensions/leading
grading/saturation/aberration.42 checks pass Release+ASan/UBSan; Windows DLL builds.
This is a measured historical regression correction, not proof the user's current
lobby-disabled run hit it. Candidate staged while user tests; no live deployment
or settings changes. Artifacts: root `build/lobby-audit-20261117`.
