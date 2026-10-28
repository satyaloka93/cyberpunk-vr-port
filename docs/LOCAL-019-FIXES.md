# Local 0.1.9 corrections for game 2.5

Base: upstream tag 0.1.9 (`e73fab02`), branch `cp2077-2.5-base-0.1.9`.
The game-2.50/port-0.1.8 checkpoint remains on its separate branch/worktree.

- **Pacing:** F10 / Stereo / Performance, "Stable pacing with no overlay graphics".
  Also recorded by the VR Settings UI. Defaults ON each launch; OFF restores upstream
  empty-overlay skipping. ON retains the preceding submission fence and adds a small
  empty GPU command submission. It is separate from framegen adaptive pacing.
- **Shotgun skipped after launcher:** consume the matching SwitchItem release after
  the launcher already cycles on press. Reset pending equipment switch state while
  consuming that gesture; unrelated weapon selections keep normal behavior.
- **Overture/Rosco reload:** source-to-destination float-track mapping with both bounds,
  validated rig signatures, component-local absolute cylinder writes and stable closed
  rest pose; source showTrack8; assisted staggered extraction after tilt; body-zone
  loader pickup without spending an already-held body grip. Keep upstream's shared
  read-only track buffer protection and 51-weapon modules.
- **Startup Load Game cropping:** track LoadGameMenuGameController initialization and
  teardown alongside SingleplayerMenuGameController. While either is alive, capture
  the complete menu window and present it as one panel to both eyes. Prevent treating
  the frontend save list as a duplicated side-by-side loading screen. Mono panels are
  intentional; gameplay remains native stereo.
- **Build:** skip optional tools/rtti_dump when absent from the upstream source tag.

Validation: whole installed script snapshot compiles with game SCC0.5.32; Lua parses;
actual crane-rest, revolver extraction (30/60/90Hz) and native rig writer tests pass.
Runtime behavior of this 0.1.9 combination remains to be tested. No claim yet of a
measured FPS improvement, universal weapon coverage or visual menu confirmation.

Build used MSVC2022 Release with public 2.50 SDK revision
`5246e659d783e5834cc08a90e56c8932c9ed3449` through `CVR_RED4EXT_SDK_DIR`.
Upstream's default submodule pin is older; reproduce with that explicit override.
OpenXR release-1.0.34 and ImGui v1.90.9 sources were reused from the local build
cache; FidelityFX and DLSS SDKs use upstream 0.1.9's pinned revisions.


Deployment: 2026-10-28 08:40:55 KST, game closed. Native full and final incremental
MSVC Release builds passed. DLL SHA256:
`a3ab88a0694827a3dac5239adb6b85f92d4cb0be1bbb465dd8150c21cbefbb65`.
DLL/PDB, three REDscripts and three reload Lua/profile files installed with
hash verification. Backup/receipt: parent repository
`build/diagnostics/game250/fixes019/backup-20261028-084055`.
No runtime visual/performance acceptance claimed yet.


Runtime feedback 2026-10-28 08:49: user confirms revolver and weapon switching
fixed; save-selection view better, but initial SBS/splash cropping remains.
Pacing subjectively improved. fpsVR whole-session histogram mean83.41 FPS,
12.42% reprojected,11.09% samples below60. Earlier unmodified upstream79.15 FPS,
18.27%,20.83%; restored fork89.40 FPS,0.65%,0%. These are unmatched sessions,
not an isolated pacing or native-stereo benchmark. No claim of fork parity.
Latest run archived under parent build/diagnostics/game250/fixes019/test-20261028-084150.


## Follow-up: logging and startup/loading panels (2026-10-28)

- World-marker camera diagnostics now print at most once per eye per process;
  exact floating-point changes previously produced 29,103 lines in the last run.
  Marker rendering is retained. This is not yet a measured FPS improvement.
- Initial stereo activation moves from frontend controller creation to closure of
  the last tracked frontend controller. The initial splash can outlive creation.
  Stereo stays enabled once armed; the working native SBS gameplay path remains.
- Flat menu/loading panels no longer receive the gameplay HUD's asymmetric eye
  translation/squeeze. The native menu flag alone missed the loading-screen phase.
- Captured images carry their own panel flag/aspect to submission, avoiding use of
  a later live UI state to interpret an older captured image.
- The complete panel fits within the configured menu FOV with preserved aspect,
  rather than enlarging its width beyond that limit. A failed full-window blitter
  initialization skips capture instead of copying half the splash.

This addresses startup, frontend save selection, loading and return-to-menu paths,
with actual in-headset confirmation still required. No claim that every splash
variant has been verified; loading layout still relies on the engine's layer state.

Validation/deployment for the panel follow-up: MSVC Release full build and final
incremental build passed. Production-function regression tests
`test_frontend_activation.py` and `test_panel_ink_shift.py` passed. REDscript logic
is unchanged in this follow-up (only its explanatory comment changed).
DLL/PDB deployed with backup and hash verification while the game was closed,
2026-10-28 09:13:14 KST. DLL SHA256:
`d94f3b69b0ad1a7aadd21ac0239b523e5c4367474be7367530411960c72fd09f`.
Backup: parent `build/diagnostics/game250/fixes019/panels-backup-20261028-091314`.
The native SBS setting remains `xr_engine_stereo=1`. In-headset splash validation
is pending. `[menu-capture]` layout transitions are capped at 32 lines per launch.

## Performance follow-up and stereo aiming dot (2026-10-28 09:21)

Latest fpsVR session 09:14:19–09:17:25, PID30176: histogram mean89.12 FPS,
0.73% reprojection (116/15947 frames), five dropped frames; 0.56% samples below60.
Previous modified run83.41 FPS/12.42%; earlier local0.1.8 run89.40 FPS/0.65%.
Short unmatched sessions, including startup/loading; no isolated attribution to
logging and no claim that fpsVR compositor-side GPU timings measure all game GPU
work. Latest native log confirms eye output3144x3000, DLSS input1048x1000 per eye,
SteamVR2.18.4 and the expected mono->SBS-loading->stereo-gameplay transitions.
Native FG is saved OFF. Marker-camera spam is reduced to two samples. Archived
under parent `build/diagnostics/game250/fixes019/test-20261028-091419`.

Aiming-dot corrections:
- Restore the 0.1.8 checkpoint's `xr_weapon_laser_dot` setting through parser,
  live state, UI snapshot/apply and INI writer. F10/VR settings uses the saved
  state, rather than editing only `g_drawBarrelCross` in memory.
- In native SBS, project using one eye's dimensions and draw each dot into its
  own backbuffer half before capture splits it. Preserve each eye's horizontal
  and vertical asymmetric projection and clip to its half. The old VRCAM
  second-eye draw is not reached by native SBS; copying that path alone would
  leave this bug unchanged. Use configured MAIN eye for physical eye sign.
- Keep the existing legacy draw path. No extra texture or GPU pass is added.
- Hide the world dot on startup/loading panels.

MSVC Release build passed. Production SBS draw-block regression checks both MAIN
assignments, viewport/clipping placement and independent X/Y eye coordinates.
Deployed DLL/PDB with verified backup, game closed, 2026-10-28 09:21:32 KST.
SHA256 `4aca7b16ed3a918b79ed0e1d5805558fbf59c89493543051b16d88068d1e26aa`.
Backup: parent `build/diagnostics/game250/fixes019/dot-backup-20261028-092132`.
Added `xr_weapon_laser_dot=1` to installed INI because the missing saved key lost
this user's enabled choice. Existing settings preserved. In-headset alignment
and restart persistence acceptance remain pending user testing.


## PSVR2 controls/haptics/driving integration — 2026-10-28

Recovered the tested 0.1.8 checkpoint's Sense Triangle/Create controls, Auto
bridge/OpenXR routing, generic gun/swing/vehicle envelopes, wheel grip latch and
separate bike/car offsets. Preserve 0.1.9 native stereo, per-hand recoil detection
and menu haptic profiles. Toolkit's single-gun recoil remains EDS-owned; dual
impacts use the previous per-hand bridge pulse policy. Auto detection is at startup,
not live actuator handoff; start bridge before game for advanced effects.

New bridge/OpenXR code is in Runtimes/HapticRouting, LegacyHapticPolicy,
Psvr2HapticChannel, HapticPulseQueue and OpenXRVehicleAudioHaptics. Existing
LiveControls parser/UI/writer persists routing/gains/camera offsets. Camera-miss
wheel maintenance additionally handles release clicks rather than consuming them.

Validation: full and final MSVC Release builds, two Linux policy tests, all four
Windows input/routing/playback/bridge-detection suites, production wheel/offset
checks and Lua parsing passed. Selected SteamVR personal binding already has both
haptic outputs and Sense control actions. No driver, binding or bridge binary edit.
Deployed DLL/PDB + ordinary_melee.lua with backed-up INI, game closed, 09:56:17 KST.
DLL `471d1e72e37d870d96b9fecdfc7381f05303ff3ece233e504514cf5830d500e1`.
Receipt: parent build/diagnostics/game250/psvr2-019/backup-20261028-095617.
Auto=2 selected; missing gains1.25/1 and bike offsets initialized from current
car offsets, preserving the user's camera position. Physical test is pending;
these changes have not been added to the six published upstream PRs.

## Release acceptance — 2026-10-28

User confirmed the latest controls/haptics/driving build is good for release.
Laser-dot correction was previously accepted; loading panels accepted with
remaining splash limitations. Package 0.1.9-psvr2.1-rc1 preserves the exact
09:56 tested DLL. The installer selects upstream or PSVR2 0.1.9, uses game-2.5
dependency requirements for both, preserves personal haptics/camera settings,
and retains clean port replacement, rollback and restore.

## More Occluders installer correction

The maintainer's installed MoreOccluders.reds was locally patched on October 26
for game 2.5: ordinary worldMeshNode optimization retained; incompatible
worldInstancedMeshNode property writes removed. The switcher preserved that file
but required new users to download the original. This masked a fresh-install
compatibility problem. The external mod is no longer a required dependency or
installed by the switcher for either edition. Existing copies are warned about
and preserved. Test-OptionalOccluders.ps1 verifies staging without it, no prompt,
ignored legacy archive argument, preservation and removal from required links.

Option 8 now offers a hash-guarded compatibility patch matching the locally tested
script. Verified exact output, preview, backup, idempotence, unknown refusal, ZIP
input and partial-write rollback. No full third-party source redistributed.

## Installer portability — October29

Steam library discovery used Join-Path on stale library drive letters, causing
a fatal missing-drive error even when another valid library existed. Use lexical
Path.Combine and check candidate executable existence; remove maintainer-specific
D/F candidates. Keep VDF double-backslash unescaping, never strip all separators.
Archive intake now validates file versus directory and ZIP extension, trims pasted
quotes/whitespace, and reports the actual failing archive path when opening fails.
Windows regression covers missing-drive registry/VDF entries, spaced paths,
explicit GameRoot, directory rejection and quoted ZIP extraction.
