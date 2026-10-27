---
type: Migration Status
title: Cyberpunk 2.50 native migration deployed for first runtime test
description: Prerequisites repaired and reviewed native migration built and deployed locally; exact 2.50 profile enabled, with headset and game-runtime acceptance still pending.
tags: [compatibility, crash, game-250, red4ext, migration]
timestamp: 2026-10-26
---

# Current status

**First test follow-up:** RED4ext loaded all five native plugins, including the
migrated VR DLL. Script compilation then failed on seven2.50 API changes.
Five port scripts and two local third-party scripts were corrected and
deployed with backups. The complete staged installed-mod set now compiles
with the game's actual SCC0.5.32, against vanilla2.50, without errors.
See repository document `docs/GAME-250-SCRIPT-COMPATIBILITY.md`.
The next real game launch compiled all scripts successfully. KeypadCursor failed its
native hook install in the first run; this separate UI item remains pending.

## Post-script crash, October 26

PID32492 failed at15:12:48. The REDEngine exception dump and Cheeky VEH
agree: `StageReplay::EngineFrame` called `ReadSafe`/memcpy with a noncanonical
source address `0x657873796870A909`, size4. The old2.31 renderer global
RVA3427C00 was still used by the every-Present summary, even when replay and
census were off. The game recorded this as an unhandled access violation;
do not assume the local SEH wrapper makes speculative invalid reads harmless.

Fixed the reader to use shared `RENDERER_GLOBAL_RVA` (2.50:3BBCAE8) and frame
offset4CC8, matching the audited Framegen reader. Inactive replay/census now
skips the diagnostic clock read entirely; VRCAM partitions still apply.
Full process dump, exact crashed DLL/PDB, original exception registers and
symbolized stacks are under `build/diagnostics/game250/crash-32492/`.
The full dump independently verifies old global contents6578737968705C65
(text bytes), new renderer pointer000001C382048A30 and frame counter1.
Windows Release build and diff checks passed. DLL/PDB deployed with verified
hashes; backup receipt `build/diagnostics/game250/deployment-backups/20261026-152307-804/receipt.json`.
Installed DLL SHA256 `e4da1e2918126eeaf0c59b8e82d7ac3c146d5592190c75ae38baab1489547126`.
Runtime acceptance of the fix remains pending.

The following records the earlier native deployment checkpoint:

User authorized proceeding after the [startup diagnosis](game-250-startup.md).
Seven official prerequisite packages are installed and hash-verified. The
native SDK and private-engine migration compile. **A local test candidate is
deployed; runtime compatibility is not yet established.** Installed DLL is
`9829e6a684dcda6ca70ba1c1daf7af6a6bdf31c85a83ef9090bc7f68e1a564c5`.
The prior cf6d6986 DLL and PDB were backed up and verified before replacement.
Parent receipt: `build/diagnostics/game250/deployment-backups/20261026-142514-163/receipt.json`.
Deployment changed only the DLL and PDB. The exact2.50 SDK/private-engine
profile is now enabled; older/unknown builds still refuse before registration.
No game was launched. No graphics, haptics, pacing, runtime layers, saves or
controller bindings were changed. Existing dirty source work is preserved on
the same psvr2-tweaks-0.1.7 branch/worktree.

## Implementation checkpoint, October 26

The earlier audit below is historical; production addresses and layouts have
now been changed in this worktree using the exact paired executables. This is
more than an SDK rebuild. Implemented areas include camera descriptors and
DLSS, renderer submission and frame graphs, native resource-pool layout,
view reuse, HUD and grading blocks, framegen inputs, aiming, roomscale,
animation, swimming and surveillance. Native constants changed in size as
well as location: camera848->864, frame480->528, HUD512->432, resource slot
stride5E00->70C0. Private leaf ABI fixes preserve live registers, and the
binding-update observer now forwards the engine's fifth argument.

Four integrated MSVC Release builds passed. Latest completed checkpoint:
SHA256 `9829e6a684dcda6ca70ba1c1daf7af6a6bdf31c85a83ef9090bc7f68e1a564c5`,
9,839,616 bytes, UTC2026-10-26T05:24:20.9356362Z. Receipt/log:
`build/diagnostics/game250/native-migration-build.{json,log}` in the parent
repository. These are build checks, **not evidence that VR runs on2.50**.
The installed hash matches that build. Nine compatibility-policy cases pass.

Evidence reports under parent `build/diagnostics/game250/reference-231/`:
`abi-audit/`, `aob-migration/`, `camera-migration/`, `dispatch-migration/`,
`framegen-input-migration/`, `gameplay-migration/`,
`grading-constants-migration/`, `input-hooks-migration/`,
`render-parity-migration/`, `resource-migration/`, `roomscale-migration/`,
`view-reuse-migration/`, `weapon-aim-migration/`, `remaining-aob-migration/`,
`partition-migration/`, `physical-optics-migration/` and `marker-migration/`.
The latter also contains `hud-panel-contracts.json` and `verify_hud_panel.py`.
Each records its bounded
native contracts and limitations; generated-thunk fixtures exercise callback
clobbering and stack alignment where hooks intercept live instructions.

Final active-hook review and implementation completed: world-marker text's
new third argument and interior store, physical optics field shifts, native
HUD texture/repaint helpers, camera AOB register preservation, LoD sites and
the VRCAM worker partition override. Composition fallback's stale holder+258
was corrected to288 after the final independent review. A uniquely matching
signature is not itself a compatible callback: generated-code fixtures
exercise flags/register preservation and policy branches. Remaining camera
fixtures passed38 cases, alternate LoD24, partition36; HUD has45 exact native
instruction contracts, eight guards and eleven identity mappings.

No concrete unsafe normal-path site remained in the scoped final reviews.
Shader visual behavior and flat ink routing require runtime checks; unchanged
exact shaders can match existing replacements, while changed hashes retain
native shaders. Roomscale's backend+8 owner
initializer is not independently established; the existing identity check
is retained, so an unmatched owner cannot receive movement writes. Optional
old single-pass experiments are not certified by this migration.

Installed settings were read, not changed: nativeFG0, partition override1,
stage replay0, PSVR2 haptic backend2 (Auto). No CVR_* environment overrides
were found in process/user/machine scopes; native single-pass probe shader
assets are absent from the installed plugin. The old node-name table is used
only for labels; its larger node certificate table belongs to guarded,
zero-default owned-graph experiments, not normal rendering.

Next acceptance: start the headset runtime and game, check dependency/native
registration logs, load a save, inspect both-eye world/HUD and controller
input. Keep nativeFG off for this first startup/render test. Only actual
runtime evidence can establish the migration works. No release/PR is made.

The user explicitly chose to restore the existing stereo port before trying
the game's new native stereo path. See `docs/GAME-250-NATIVE-STEREO.md` for
that deferred research; no native-stereo mode is enabled here.

# Immediate crash identified

The first pass found no current dump in Windows WER archives or REDEngine's
queue. Follow-up found full-memory dumps in `F:/crashdumps`:
Cyberpunk2077.exe.2460.dmp (October26 10:39) and .29380.dmp (10:38).
The 2460 exception context matches the latest log's RIP and startup thread.

WinDbg/CDB stack:
RED4ext plugin loader -> ArchiveXL Main -> ArchiveXL+0xDA70 callback
registration -> game RTTI AddRegisterCallback -> callback copy -> execute
0x00000241AB300140, a non-executable heap pointer.

This is a concrete ABI mismatch:

* ArchiveXL's callback constructor at DLL RVA0xD900 writes its handler at +0x20.
* Registration uses RTTI vtable slot0xC0 (AddRegisterCallback), not a proven
  shifted slot. That slot remains0xC0 in the updated SDK.
* Game copy reads the callback handler from +0x40, then calls handler+0x08.
* Official SDK2.50 changes DefaultFixedCallbackBufferSize from32 to64, moving
  the handler accordingly. The old32-byte buffer makes the game read beyond
  the callback's old representation and call unrelated heap data.

Static REA evidence, bound to old ArchiveXL SHA256cee7aaccfeaa19ae2f865bb80e08afdd63fa1f14968c44c0a0538bf0b8d931b2:

* Registration: ev_e321b0f0b3d2a826cc5b895dc7bdb0accd0e4e1072c7e0afa48a2960680224c3.
* Callback constructor: ev_846989b0241ea97c98ce1902469e8ddeb578e4bc840ee4c2f9691e5375fe5592.

Parent-repository evidence: build/diagnostics/game250/startup2460-cdb.log,
startup2460-callsite.log and archivexl-callback-rea.json. Old DLL is retained
in the dependency installation backup. No live-process control was needed.

# Dependency installation

Official GitHub asset digests verified before staging, each changed file
backed up and checked before writes. Installed RED4ext1.31.0, CET1.38.1,
ArchiveXL1.28.0, Codeware1.21.1, TweakXL1.12.0, redscript0.5.32 and
Input Loader0.2.5. Post-install preview: zero changes,79 kept/identical files.

Parent-repository backup receipts:

* build/diagnostics/game250/dependencies/backups/20261026-110134-500-1e158642/receipt.json — first six packages,27 changed/new files.
* build/diagnostics/game250/dependencies/backups/20261026-110811-005-626f4c06/receipt.json — Input Loader DLL/readme only.

Installer and rollback are in dependencies/Install-Dependencies250.ps1. Roll
back newer receipt first if reverting all updates. Existing settings/CET mods
and Cheeky loaders are preserved. Installed version.dll was already identical
to the CET package. Input Loader INI/XML/cache files were rehashed unchanged;
its runtime may rebuild caches on a later game launch.

Input Loader source compatibility was checked beyond its version number:
v0.2.4 added2.50 input fingerprints and new SDK types/RED4ext1.31 minimum;
v0.2.5 adds corrected Vortex packaging. No claim that all remaining script mods
have compiled successfully on2.50: no new game compilation has run.

# Initial SDK and startup guard checkpoint

Official SDK commit5246e659d783e5834cc08a90e56c8932c9ed3449 replacesd02a1f32;
primary .gitmodules SDK URL now wopss/RED4ext.SDK. Prior SDK state saved under
build/diagnostics/game250/sdk-before. No prior semantic SDK edits discarded.

New Core/GameCompatibility checks numeric file-version words before native
registration, first-launch writes, engine hooks or worker threads. Query no
longer advertises runtime-independent compatibility and reports version0.1.8.
At this initial checkpoint SDK target was2.50 while the private-engine profile
remained2.31, deliberately refusing both builds. The final reviewed migration
now selects Game250 for both, as recorded above. There is no force-load option.
Unload is a no-op after refusal.
**This guard is not a replacement for migrating the engine hooks.**

Native MSVC Release build passes; eight portable compatibility cases pass.
Initial SDK-only guarded candidate SHA256eab526c272472f32388c64a071e671ae69e96dfaf61a222ba925b26d9c6feed0.
It exists only in build/pacing-018-msvc, not the installed game or a release.
Logs: build/diagnostics/game250/sdk-build.log and sdk-guard-build.log.

# Private engine migration evidence and blocker

Audit identifies133 real registered hooks (65 unconditional,68 conditional),
not136 textual matches including comments. Relative to2.50 unwind records,
116 old locations fall inside functions,11 lack a covering unwind range,6
coincide with entries. Neither entry coincidence nor missing unwind metadata
establishes function identity. There are also direct hooks, return addresses,
data globals and manual view/component layouts outside this registry.

The initial local search did not find a genuine old EXE/address JSON. Sixteen
retained 2.31 dumps did not contain the RED4ext Addresses object or a complete
old code image. That bounded negative result is preserved in parent-repository
build/diagnostics/game250/hook-audit/REPORT.md; it is **no longer a missing-reference
blocker**, because the following Steam retrieval succeeded.

A masked signature produces one **candidate** for the old motion getter:
old0xD455B0 -> new0xE3C3A0,93-byte unwind extent, file offset0xE3B9A0.
The existing26-byte prefix matches with its RIP displacement masked. The
Apply hook's shorter prologue has169 matches and is not an identity anchor.
At that initial audit checkpoint no production RVA was changed. The later
paired-image implementation is summarized above. Raw-function REA output is saved with provenance
in motion-candidate-rea.json; generic raw-image decompiler argument labels do
not model Windows x64 correctly, so they must not be used as an ABI signature.

# Exact old reference recovered

After the user authorized download and completed Steam QR sign-in, official
DepotDownloader 3.4.0 retrieved only `bin/x64/Cyberpunk2077.exe` and
`bin/x64/cyberpunk2077_addresses.json` from app 1091500, depot 1091501,
manifest 3389237510439361019. Download completed successfully on October 26.
The installed game was not downgraded or modified by this retrieval.

Research root in the parent repository:
`build/diagnostics/game250/reference-231/`. Files are under `content/bin/x64/`.
`Get-Reference231.ps1`, `files.txt`, `download-provenance.json`,
`download-result.json` and `verification.json` retain the reproducible procedure
and exact artifact identities. The downloader was fetched from its official
GitHub release; that release exposes no publisher SHA-256, so its recorded
local hash is provenance, not an independently verified publisher digest.

* Old EXE SHA-256: `a7de82945c03e041fc7339fcf9066224d98db2f5d80fea50f7947bb350a60991`.
  **Exact match** to the fork's recorded `RoomscaleSites.hpp` baseline.
  Product version 2.31; numeric file version 3.0.80.51928.
* Old address JSON SHA-256: `58da9e852fa519f851eb42fd36fd27c292c5c8585b1ec66cf34d4a2544a5689f`.
  Its 987,479 records have linker timestamp `0x68af45ea`, matching the EXE.
* Current EXE SHA-256: `d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4`.
  Numeric file version 3.0.88.46235.
* Current address JSON SHA-256: `eab304f38074145ab5ef53e2aa741576ba00674138761fafff23252005948fd4`.

# Native comparison results

`map_registered_hooks.py` validates both executable hashes and each address
table's linker timestamp. It matches **both** primary and secondary identity
hashes, not the collision-prone primary hash alone. Results:

* 124 of 133 registered hooks have a unique corresponding shipped identity.
* Seven old identities are absent; one folded identity splits into two new
  functions; one hook is an instruction inside a function and needs a precise
  instruction mapping. These nine have separate caller/control-flow analysis
  under `reference-231/unresolved/`; candidate matches are not ABI certification.
* The bounded body audit of 57 uniquely mapped unconditional hooks found 19
  equivalent after verified relocation normalization, 36 changed or with
  unresolved references, and two without an entry unwind range. It includes
  chained unwind ranges, but does not certify transitive callees or callers.
* Six of 19 recognized literal/mask AOB patterns fail on 2.50: DlssRes,
  FinalCamera, PatchCamera, PhotoModeCapture, ProjectionCommit and SettingsRes.
  Thirteen unique matches in both files do not prove their callbacks remain
  compatible. Scanner coverage is limited to the recognized declaration form
  and executable `.text` section.

This is a real layout and calling-contract migration, not an address-only update:

| Engine data | 2.31 | 2.50 |
|---|---:|---:|
| View camera descriptor base | 0x70 | 0xE0 |
| Camera descriptor stride | 0x3A0 | 0x3E0 |
| Input FOV | 0x1E40 | 0x2C70 |
| Plane override pointers | 0x1688 | 0x2470 |
| FOV scale / addition | 0x17F4 / 0x1D40 | 0x25D4 / 0x2B70 |
| Present output pointer | 0xF88 | 0xF98 |

Camera descriptors still number four. Their new tail contains flags,
parameters and child pointers; it is not padding. DLSS fields, renderer virtual
slots and feature indices also changed. The detailed instruction evidence and
limits are in `reference-231/abi-audit/REPORT.md`. The submission candidate now
consumes a 64-bit third argument where the old function consumed 32 bits; gather
uses a fourth argument absent from the old hook signature. The old FFX loader
path is gone: 2.50 statically imports the FidelityFX loader API instead.

Non-registry calls, globals, vtables and instruction sites are independently
inventoried under `reference-231/direct-addresses/`. That inventory is lexical;
unclassified numeric literals must not be patched as if every one were an RVA.
No production addresses or private layout constants were changed by these audits.

# Remaining implementation and activation boundary

Use the verified reference pair to migrate individual native layouts, hook
signatures, trampolines and direct calls. Resolve remaining globals and validate
new descriptor modes rather than applying a uniform offset. Keep the startup
guard closed until these obligations are completed. Then build, deploy and
verify plugin loading, script compilation and VR gameplay. **No new game test
is ready yet.** Dependency updates, downloaded references and a successful SDK
build do not establish restored VR.

# Citations

* [Official SDK2.50 support](https://github.com/wopss/RED4ext.SDK/pull/227).
* [Official generated types](https://github.com/wopss/RED4ext.SDK/commit/5246e659d783e5834cc08a90e56c8932c9ed3449).
* [Input Loader0.2.5](https://github.com/jackhumbert/cyberpunk2077-input-loader/releases/tag/v0.2.5).
* [DepotDownloader parameters](https://github.com/SteamRE/DepotDownloader#parameters).
* [Official DepotDownloader 3.4.0 release](https://github.com/SteamRE/DepotDownloader/releases/tag/DepotDownloader_3.4.0).
* [Steam depot and historical manifest](https://steamdb.info/depot/1091501/apps/).
* Other official release URLs and all payload hashes are in parent-repository build/diagnostics/game250/dependencies/README.md and manifest.json.

## Telemetry screen input repair, October 26

Next launch passed the crash site, but neither telemetry choice worked.
Read-only live debugger showed VR overlay capture=false, visible=false, menuMode=1.
Input Loader0.2.5 logs proved recursive loading of `r6/input/.backup-20260814/`,
including complete old cached contexts/mappings and HUDitor.xml. The resulting
cache omitted both `system_notification_confirm_hold` and `close_popup_hold`
from UINotifications, although vanilla2.50 defines them. The screen only
handles these hold actions, so Enter/Escape and pad confirmation could not work.
Moved all five backup files outside the game input scan to
`build/diagnostics/game250/input-backup-quarantine-20261026`, verifying hashes.
Only ScannerHud and DualSenseSupport XML remain active. Input Loader regenerates
its cache at startup; restart required. No telemetry preference was selected.
The FPS overlay remains disabled in vrport.ini. Runtime input acceptance pending.
Never store XML backups under r6/input, including hidden subdirectories.

## Aiming / dual-wield crash repair, October 26

The next run reached gameplay; user reported slightly different reflections per
eye, then a crash while attempting dual wield with the haptics driver active.
Crash154533/PID20036 reads noncanonical RCX3F8000003F800000 in native
RVA32D02C. Call chain: RTT prepare48FF5B -> resource token32CF98 ->32CFC4.
RTT component000001E09D463530 has1.0/1.0 at+280/+284, where its environment
Ref token lives (+278; SDK entRenderToTextureCameraComponent). The port's
SyncAdsWeaponZoomOneCamera wrote the gameCameraComponent weapon zoom fields
to BOTH MAIN and RTT, mistakenly treating the classes as interchangeable.
An empty resource token passes the old float plausibility gate. This is camera
memory corruption; the stack gives no evidence of a haptics-driver failure.

Added reflected gameCameraComponent IsA guard before reads/writes/restoration,
and SDK offsetof for the two legitimate fields. RTT no longer receives these
writes. MAIN weapon-zoom adjustment remains. Second-eye ADS magnification and
reported reflection mismatch still require visual verification; no claim that
this crash fix solves reflection parity. Native2.50 RTT preparation reads the
environment token exactly at the corrupted address, independently of haptics.

Windows Release build and diff checks pass. DLL/PDB deployed with hashes verified;
DLL3826a71ed1a28397d1ab4b873ada09758b937ada1ee32f98e075dad91360c908.
Backup receipt `build/diagnostics/game250/deployment-backups/20261026-154942-230/receipt.json`.
Crash dump, matching prior DLL/PDB and VR log saved in
`build/diagnostics/game250/crash-20036/`; symbolized evidence in
`build/diagnostics/game250/dual-wield-crash.txt` and `dual-wield-camera.txt`.
Runtime acceptance pending. Input backup repair succeeded sufficiently to reach
normal gameplay; telemetry screen is no longer blocking the reported run.

## Follow-up lifetime crash, October 26

Crash155237/PID35032 is in the newly added GetType call itself: cached camera
0000017901034A80 has a null vtable. This was introduced by the previous repair;
its raw-cache type call was unsafe during loading. The earlier resource-token
corruption is not the faulting path in this dump. Preserved dump and exact DLL/PDB
in `build/diagnostics/game250/crash-35032/`, symbolized exception in
`build/diagnostics/game250/crash-155237.txt`.

Moved ADS sync out of OnLocateCameraCallback (which read both cached pointers)
to OnPatchCameraCallback, using the current engine-provided ownerState only when
classified MAIN. It no longer dereferences either cached camera for zoom sync.
Retains the class gate and SDK offsets, checks the vtable before calling GetType,
and serializes capture/restore state across camera-update threads. RTT is never
passed to this write path. Saved numeric identity is used only to compare with
the current live camera; restoration never dereferences a saved pointer.
Runtime validation remains pending.

Release build and diff checks passed. Corrected DLL/PDB deployed with hash verification;
DLL `f93dd13e73f943c1ac9923eeb4540c9dce4f00d00e1baf7b9fb9e5c3b541b3e9`.
Backup receipt `build/diagnostics/game250/deployment-backups/20261026-155548-144/receipt.json`.

## Gameplay acceptance and remaining parity/haptics, October 26

User confirms no crashes after active-camera lifetime fix. Reflection/lighting
mismatch with RT off; enabling RT resolves visible mismatch at higher cost.
The port is already rendering two eye views. Native2.50 single-pass stereo is a
separate project and is not a prerequisite to repairing this parity regression.
Static native evidence: SSR node77A100 calls CbUpload41A780 at77A9DD with
ECX290hex=656bytes; planar node77D450 does the same at77DDBD. Current
Detour_CbUpload merge gate and kEngineBlockBytes accept384bytes only. Thus the
existing cross-eye SSR merge cannot execute for these uploads. The run creates
exposure/shadow stores but logs no SSR stores. This establishes a migration gap,
not proof that every observed lighting difference has the same cause. Need2.50
constant layout plus matching shader variants; do not blindly extend old shader
contracts or rename replacement hashes. Evidence: `build/diagnostics/game250/reflection-upload-evidence.txt`.

Bridge PID28436 runs the017EXE from Downloads/PSVR2Toolkit-DSX-Bridge. Native
route selects Toolkit exclusive, bridgeListenerDetected1; DSXData reports
connected. DualSense Support.log repeats GetState.lua:16 GetInt invalid field
from init.lua836 each update. Vanilla2.50 PSM renamed Weapon to RightWeapon and
LeftWeapon. Patched local GetState compatibility adapter to resolve RightWeapon
for legacy Weapon requests (older Weapon fallback), guard missing player/board/
field, retain other state types. Lupa tests passed new/old field selection,
precedence, enum conversion, vehicle unaffected and missing-field handling.
Deployed only GetState.lua with backup suffix before-game250-20261026-161844.bak;
no driver/bridge/settings replacement. Existing right-hand semantics retained;
independent dual-wield trigger effects are not claimed. Physical acceptance on
next game launch remains pending. Staged patch is in
`build/diagnostics/game250/haptics/GetState.lua`.

## Non-RT reflection correction, October 26 16:48

User reports crash-free play but unequal reflections/lighting between eyes;
ray tracing conceals the difference. Offline extraction of the installed2.50
`engine/staticshader_final.cache` found629 compute shaders.22 exact shader
keys still contain symmetric tangent reconstruction in the non-native-stereo
branch. Existing2.31 replacements cannot match those new keys.

`tools/asym_tests/asym_ssr_250_shaders.py` generates22 replacements against the
shipped originals, adjusting44 reconstruction sites and19 shared-screen fades.
Tangents now live in b6 row33.zw. The new material-dependent alternate projection
(rows16..19 versus0..3) is retained through the engine's own selector. The native
inverse-projection branch and656-byte constant block are preserved. All22
containers passed DXIL assembler/validator and checksum checks; numerical
projection round-trip checks passed, and obsolete384-byte inputs are rejected.
Generated assets are in `src/Shaders/asym_ssr/` alongside the older exact-key assets.

The independent native audit found engine SSR/SSPR blocks grew384→656bytes;
new CPU append contract is704bytes, rows41..43. However, the old wide-history
merge recipes also assume obsolete control flow and pixel coordinates.
`kWideMergeShaderContractReady=false` explicitly disables SSR/SSPR merge store
allocation until matching shaders exist. Other AO/shadow/exposure channels
are unchanged. This deployment restores projection correction and shared
edge fade, **not** the old cross-eye history merge or a full native stereo path.

Release build succeeded (existing signedness warnings). Installed DLL SHA256:
`95dfb0ea4796c8613812f977c2a06c3ab839b78214e21731bf319e92244c91e1`.
DLL/PDB and22 shader files deployed together with SHA256 verification and rollback
receipt at `build/diagnostics/game250/deployment-backups/20261026-164809-901/receipt.json`.
No graphics settings, haptics settings, loaders, layers or scripts changed in
this deployment. Bounded `[ssr-patch]` log entries report pipeline creation/failure
(up to8 each) and first successful bind; no continuous capture enabled.

**Runtime acceptance pending:** restart game, ray tracing off, compare the same
reflective surface between eyes. Confirm `[ssr-patch]` successful creation/bind
in VR log before attributing visual changes to this fix. Residual lighting may
require separate AO/shadow/fog shader migration; do not claim all lighting fixed.
Research evidence: `build/diagnostics/game250/reflection-layout/REPORT.md`,
`reflection-shaders/patched/migration-250.json`, and extracted original DXIL/IR
under `shaders/compute` and `shaders/ir` in the same diagnostic directory.

## Right-eye shadow sharing investigation, October 26

Latest test log confirms the SSR deployment created5 corrected pipelines,
failed0, and bound them. User clarifies the left eye's lighting/shadows were
correct while the right eye was wrong. The port still uses its MAIN/VRCAM
stereo route; native Odyssey stereo was not enabled. Default MAIN is the right
headset eye (`CyberpunkVR_MainIsRightEye=1`).

Independent native audits found no new incorrect feature/category or cascade
field offset. Bit33 is unchanged; all category descriptors retain verified
identities. Native graph hashing includes both computed feature words, so
changed flags naturally change the graph key. Repeated LUT-source borrow
refusals mean that view builds with its own source; they do not establish a
shadow cause. Evidence: `reference-231/flag-parity-audit/REPORT.md` under the
parent game250 diagnostics directory.

A real stale-data path exists: cascade record and render-camera caches previously
remained valid indefinitely. Native cascade4EDFE0 can skip its camera upload
at4EE04E when index>=activecount, or via later gates, while the port had already
snapshotted the record. Record and camera could therefore come from different
generations. Sampling also lacked a generation/owner identity. This proves a
possible incorrect lend into MAIN, not that it caused the last visible mismatch.

The repair introduces shared cascade freshness policy: source sampling starts a
new generation; renderer/scene owner and current or immediately following engine
frame must match; records need a matching native camera upload ticket; sampling
requires a complete active cascade bundle; camera/record lending requires the
sampling decision. Native calls run outside the cache lock. Keep the destination's
new864-byte camera tail when lending the existing848-byte prefix. Engine frame
advances between the two eyes, so strict frame equality would wrongly reject
normal pairs. Diagnostic camera differences now inspect known matrix fields,
not arbitrary integers interpreted as floats. Bounded `[cascfresh]` log lines
report acceptance/rejection and identities without enabling full capture.

Portable C++ tests exercise cold startup, skipped native uploads, prior-frame
rejection, changed scene/renderer, stale worker tickets, generation resets,
frame wrap, same-frame dispatches and incomplete bundles. Build/deployment
and headset outcome are recorded below once completed.

Final cascade candidate Windows Release build succeeded. Deployed DLL/PDB with
verified hashes; installed DLL `548d9220224d9fd2c9f524b98e8387144f1fac6181a99c3ca08716e5ad570222`.
Rollback receipt: `build/diagnostics/game250/deployment-backups/20261026-172546-322/receipt.json`.
Source freshness is checked before deployment. Tests also cover latching a failed
first sampling decision; later completion cannot switch half a MAIN turn to shared
data. Each sampling slot carries its own generation and failed payload copies
invalidate prior validity before retry. Settings and shader assets unchanged
from the preceding SSR deployment. **Headset visual acceptance remains pending.**

## Shadow test acceptance follow-up

User reports lighting appears fixed. Preserved run log in parent diagnostics
`build/diagnostics/game250/cascade-test-review/lighting-test.log`:5 SSR pipelines
created,0failed,correctedpipelinebound;107 bounded cascFresh samples all report
rejection, with no cascrender first-success line. Internal accept_checks count
context eligibility, **not successful shadow copying**. Observed improvement
is consistent with rejecting old cross-eye sharing and retaining native shadow
data, not proof that repaired sharing runs successfully. No native stereo mode
was enabled. Treat visual report as positive acceptance for this scene, not
comprehensive lighting verification.

## 2026-10-26 weapon settings and pairing repair

Laser aim checkbox now persists as `xr_weapon_laser_dot` through the normal
F10 snapshot, INI write/read and live application. Previously it only edited
`overlay::g_drawBarrelCross` in memory.

The cigarette `EquipmentArea.Weapon.yaml` three-slot override is obsolete.
2.50 uses Right0..2, Left3..5 and Melee6. Its secondary-item lookup uses
`ArraySize(items)/2`; a three-slot override pairs ordinary primary slots0/1,
bypassing the inventory-placement perk invariant. Package override now contains
only explanatory comments; installed override is backed up outside game scanning
and removed. No save inventory or perk is edited.

The semi-auto wrapper now respects `ShouldShoot` and reads
`GetLastChargePressCount()` (Right/LeftLastChargePressCount), replacing the
obsolete shared key. Whole staged mod set compiled successfully with installed
SCC against 2.50. This preserves native alternating dual-wield firing; independent
VR firing is NOT implemented. Read-only method audit is under
`build/diagnostics/game250/dualwield-audit/`: listener wakeups, full-auto, charge
release and native forced-fire paths must be addressed together.

### Dual-wield haptics follow-up

EDS still read only RightWeapon. Its local adapter now selects the firing
weapon and corresponding state together for the existing R2 output channel;
left shots no longer necessarily appear as right Ready. This is not independent
haptic channels and does not increase gain. Eleven mocked selection cases pass;
physical feedback needs a run. Deployed with EDS backup under diagnostics/game250/
haptics/backup-20261026-182714. See tools/compatibility/dualsense250.

User also reports a double-barrel shotgun failing to equip. Cause unconfirmed:
latest logs show no matching rejection. Temporary read-only Weapon250Audit
records requests and changed slots to distinguish persisted layout from request
routing. No save inventory/perks modified; no shotgun fix claimed.

### 18:29 run review: unresolved presentation issues

Evidence archived in diagnostics/game250/run-1830-review. Actual weapon arrays
have six slots, not seven: enum MeleeSlot6 is a special melee/fists selection.
Testera became active according to the weapon module at18:31:36–37 and18:32:46;
2Hz hand snapshots missed it. Do not diagnose inventory refusal from these
snapshots. EquipItem observer bound a bool overload, so its slot label is invalid.

2.50 ReadyEvents writes RightWeaponHandlingData/LeftWeaponHandlingData, whereas
NoAnims still writes the old shared feature. Native VR recoil is a single spring
signaled from the ray override, not per-hand native shots. Animation compatibility
is incomplete; EDS haptic selection did not fix it. No animation change deployed.

40 sampled cascade attempts all rejected; no successful raster-camera sharing
record. Remaining eye mismatch unlocalized without scene images.

### Shot-event recoil candidate (18:44)

Right-hand `ShootEvents.OnEnter` now calls registered native VRRightWeaponShot
after the game executes its shot, guarded to player/right weapon logic. Both
physical-ray and projectile-provider recoil notifications removed; single spring
is now sourced only from real right-weapon events. Removed obsolete40ms ray
deduplication. Right ReadyEvents override writes RightWeaponHandlingData. Left
native recoil is preserved until a true left VR spring exists. This is not
independent dual-trigger firing or full dual-hand aiming support.

New event-level REDscript trace records resolved equipment action and actual
equip/unequip ItemID and side in cyberpunkvrport.log. Old2Hz CET audit removed
into deployment backup; no inventory writes. Cascade logger preserves first
occurrence per failure reason, including previously silent source/read/setup
failures. No shadow acceptance policy changed.

MSVC Release, whole staged SCC compilation and diff whitespace passed. Deployed
DLL SHA256061e0be58b6f103948b028a5177fd92581533c43565498653dbabb897c67afd6
with three matching scripts using Deploy-WeaponEvents250.ps1. Runtime validation
pending; Testera and remaining eye mismatch are not claimed fixed.

### Fog identified: seven shader replacements migrated

User localized remaining mismatch to fog. Four installed legacy asym/ shaders
are absent from2.50 compute shader cache, so they cannot match new pipelines.
New ordinary-camera fog branches still construct unshifted rays. Native-stereo
branches selected by b0row32.x instead use b6 matrices; those remain unchanged.

Seven exact hashes/26 components migrated with tools/asym_tests/
asym_fog_250_shaders.py. Each container assembled and validated; independent CFG
reachability check in native-stereo-fog/ray-branch-validation.json confirms
all edits exclusively mode0 and correct currentb1 projection-minus-jitter.
Source assets retained under src/Shaders/asym_fog. Added bounded fog pipeline
creation/failure and first-bind logs. Native Release build and diff check pass.
Deployed DLL e5552346cdb1e65a6854b9f08432612d66b701d54ef00ec8d804f2099456437a
with seven matching shader files via Deploy-Fog250.ps1. Runtime binding and
visual benefit remain unverified. No native-stereo switch enabled.

Native work also establishes camera input bit, child update entry, eye tag at
CameraShaderConsts+350, and separate fog stereo branch. Full render-resource/
fence/presentation contract remains unresolved. See updated
`docs/GAME-250-NATIVE-STEREO.md`. REA full-program Ghidra
startup timed out; bounded PE/Capstone evidence and shader DXIL used instead.

### 2026-10-26 independent dual-wield haptics candidate

Earlier EDS selector still routed both weapons through R2. New native per-hand
shot counters feed separate local instances of each installed EDS profile, routing
only the firing effect to that controller. Toolkit bridge now recognizes
`VRIndependentHands=true` and skips inferred grip/support pulses; exact shot
pulses own grip impact. Active Toolkit route only; single weapons/vehicles keep
existing behavior. Lua routing/adapter/profile checks and native, bridge and
REDscript builds pass. Deployment requires the game to close; physical test pending.
See `tools/compatibility/dualsense250/README.md` and diagnostic deploy script.

Latest shotgun evidence: Testera's equip handler executes, CET recognizes it as
active, then katana is active within the same second (19:18:06). Not an inventory
rejection. Cause remains unresolved. Timestamped input/equip/shotgun-ready tracing
is staged; no claim that the shotgun is fixed.

Follow-up: user confirmed a flash then automatic next-weapon draw. Found a concrete
press/release double-cycle path leaving VrLauncherReady: custom press-driven cycle
followed by native SwitchItem release-driven cycle. Added consume-release flag
and pending-switch reset only for that launcher gesture. Full scripts compile.
Deployed 19:33:38 with DLL `fa36a8695991`, EDS `48238b35948e`, bridge
`7d214ed4a467`; bridge restarted preserving arguments. Hardware feel and shotgun
retention remain pending user test. Bridge function regression tests confirm
legacy support pulses remain, independent mode suppresses inferred pulses, and
rhythm goes only to the selected physical side.

### 2026-10-26 left-shot follow-up: route was forced OpenXR

User reported no left feedback after candidate. Latest log says
`output=OpenXR requested=0 bridgeListenerDetected=1`; installed INI had
`xr_psvr2_haptic_backend=0`. The Toolkit-only adapter therefore never ran,
explaining absent `[dual250]` shots. Restored backend=2 (automatic Toolkit on
PSVR2 with detected bridge) while game closed; existing bridge verified running.
This configuration prerequisite was missed in the initial deployment.

EDS also threw on an empty DSXData.json read (19:37:46). Replacement
CheckDataFromDSX.lua closes handles before protected JSON decoding, tolerates two
missed polls using last valid status, and expires stale status on the third.
Tests cover truncation, malformed JSON, expiry and recovery. Deployed reader and
route-change logging at 19:45, backup `haptics/routing-backup-20261026-194507`.
Left physical feedback is still awaiting runtime confirmation; do not call it
hardware-validated solely because routing and adapter tests pass.

### 2026-10-26 correction: support both routes, preserve user choice

User clarified backend=0 was intentional comparison, not an accidental setting.
Restored OpenXR selection. Fixed the actual native right-only path: per-hand
ShootEvents now passes its weapon's RecoilKickMax and dual status to native
ShotForHand. RecoilOnShot no longer emits a second haptic pulse. Dual weapons get
only their own vibration; single-gun two-handed support remains. Toolkit skips
native pulses and uses the EDS adapter; adapter trigger routing now works in
either mode, explicit CET grip pulses only in Toolkit. Native tests, Lua adapter
tests, full REDscript compile and native build pass. Deployed while game closed;
physical confirmation pending. See Deploy-BothRoutes250.ps1 and native-shot test.

### 2026-10-27 physical reload / two-handed grip regression

User confirmed overlays and haptics work, but manual reload and two-handed grip
mostly fail. Reload log's earlier detailed capture has CName.new(nil) failures at
reload.lua:345: cfg.slide defaults to an empty table, yet slotModel(sc.slot) is
unconditional. Missing optional slide slot now returns nil so remaining
magazine/cylinder handling runs. Reload detection still used removed Weapon
blackboard field; now RightWeapon with legacy fallback and enum-name comparison.

Another concrete ownership bug: cfgFor returning nil skipped cleanup after a
supported weapon's reload preview/slide. Native TwoHandGrip explicitly rejects
g_VRReloadFingerActive[0], so the old reload could lock out support grip on a
rifle. Unsupported profiles now follow existing hand-release cleanup, including
pressS.on-only ownership. Actual Lua module regression test seeds active left
preview/slide ownership and verifies it is released on unsupported weapon switch.
New bounded error logging reports up to eight distinct reload errors regardless
of DEBUG. No grip geometry or authored calibration was changed.

Deployed only reload init.lua and reload/reload.lua at 11:46:03, backups under
build/diagnostics/game250/reload/backup-20261027-114603. Tests pass; headset
confirmation pending. Earlier DEBUG track dumps also contained implausible float
values; their native rig layout remains unverified and was not guessed/modified.
Need specific failing weapon and interaction if problems remain.

## 2026-10-27 accessible spare-magazine pickup

User completed IK calibration but could not reach replacement magazines reliably;
the belt pouch gate still required reaching the avatar's hip. Added an alternate
chest-to-waist pickup zone for the free hand when the magazine state is `gone`.
The player still squeezes grip. Zone uses valid tracked HMD-local positions rotated
by HMD orientation into the recenter frame, independent of calibrated avatar hips:
18–110 cm below the headset and within 42 cm horizontally. Existing pouch pickup
and belt-less fallback remain. Actual slide grabs/releases and in-range slide
previews keep priority; a distant slide no longer vetoes body-zone pickup merely
because it is closer than the low pouch. No changes to seated-mag extraction,
ammunition accounting, or weapon coverage.

Production Lua helper/gate checks cover both hands, looking down, invalid tracking,
zone bounds, slide priority, low pouch and legacy fallback. Reload cleanup/state
regression checks also pass. Deployed with hash verification and backup
`build/diagnostics/game250/reload/backup-20261027-122951`. In-headset confirmation
is pending; this is not evidence that magazine carrier rendering is fixed.

## 2026-10-27 revolver follow-up

User reports improved spare pickup, intermittent cylinder/loader distortion, no
physical cylinder closing, and inaccurate red dot on an unidentified revolver.
The available weapon log ends before this run's weapon selections; do not label
the last revolver Quasar using older entries. Requested model and laser-vs-optic
clarification. Overture is the only full revolver reload profile. It implements
button close and a 3 m/s lateral gun-hand flick after 0.30 s, not support-hand
push-to-close. No aiming offsets or closing thresholds changed.

Confirmed cleanup defect: magHome reset hatch progress but retained manual open
latch and stale hand velocity sample across weapon switches. Reset latch, button
history, timer, previous position and peak together. Actual-module Lua regression
passes; existing pickup and reload cleanup tests pass. Added capped 64-line
ReloadAudit for weapon/profile changes and open-cylinder state, bone quaternion
summary, peak lateral speed and threshold (maximum one state line/sec). This is
not global DEBUG. Deployed with hash verification; backup
`build/diagnostics/game250/reload/backup-20261027-124243`. Intermittent distortion
and sight-vs-hit alignment remain unconfirmed/unfixed pending weapon evidence.

### Wrist-roll closing follow-up

User tried sideways flick without closure and confirmed aiming marker is laser.
Existing detector measures translation only. Added tracked wrist rotation about
the bore as an alternative: at least 20 degrees accumulated over at least two
consecutive samples above 250 degrees/sec, after existing 0.30 s opening grace.
Rejects invalid quaternions, intervals over 100 ms, slow motion and non-bore
rotation. Translation remains at the original 3 m/s threshold. Raw hand helper
now checks native Vector4.w validity rather than treating invalid zero pose as
a live hand. Moved bounded weapon audit from yieldAll to the actual frame path.

Tests cover real Lua rotational closure, no-translation gesture, pitch/yaw
rejection, quaternion sign equivalence, tracking failures, pauses and single
sample rejection; existing regressions pass. Deployed backup
`build/diagnostics/game250/reload/backup-20261027-124559`. Headset confirmation
pending. Open cylinder blocks trigger while marker remains visible; asked user
whether ammo/muzzle flash occurred to distinguish blocked shots from bad aim.
Overlay barrel dot uses a point at configured distance (default 20 m), not a
confirmed collision point. WeaponAim hook boot reports OK, which does not prove
per-shot redirection. No firing-direction or laser-rendering change made.

### 18:16 crash and revolver-to-launcher cleanup

Latest weapon log identifies `Items.Preset_Overture_Dodger` at18:13:18, followed
by `Items.AdvancedProjectileLauncherElectricEpicPlus` at18:13:29. User reported
cylinder misalignment, ineffective laser aiming and crash. Do not infer smart
weapon type from user's tentative description. Crash dump
`Cyberpunk2077-20261027-181648-7804-1360` was opened with installed cdb and matching
game image. Exception at game RVA0x273e2b dereferences r10=0x78e193f0, loaded
from [r9+0x38], r9=0x21278e10008, in an allocation/free-list-like routine.
Unwound exception stack contains game frames and kernel32, no direct VR frame.
Nearest exported AK symbol names are not accurate function identities. Original
corrupting writer is unknown; cannot call this a reload-caused or fixed crash.
Debugger output: `build/diagnostics/game250/reload/crash-181648.txt`.

Confirmed independent cleanup defect: no-profile branch cleared native rig writes
only for a held slide/finger pose, so cylinder/hammer ownership with magazine in
place could survive switch to launcher. Clear native writes before resetting
all part state whenever oursActive or wasActive. magHome now also resets hammer,
spin and ownAnim ownership. Regression invokes actual module with open cylinder,
cocked hammer and seated magazine, switches to unsupported weapon, verifies clear.
Audit logging compared transient CET object wrappers and exhausted64-line cap
on Nue before revolver. Now keyed by entity hash plus profile; repeated-wrapper
regression passes. All pickup/flick/cleanup tests pass. Deployed backup
`build/diagnostics/game250/reload/backup-20261027-182026`. Headset validation pending;
crash root cause, cylinder base alignment and laser aiming remain unresolved.

## 2026-10-27 dedicated dual-wield grip vibration

User clarified weakness is controller vibration and requested a separate dual
mode. Actual per-hand VRWeaponShot events now directly own dual Toolkit grip
impacts, independent of CET adaptive-trigger profile timing/settings. Existing
OpenXR per-hand route remains. VRWeaponShotCounts.w=1 advertises native ownership;
CET adapter preserves adaptive triggers and emits grip pulses only for older
DLLs without that flag. Right weapon kick=0 from camera-recoil suppression now
falls back to the weapon module's cached original kick.

Confirmed handoff loss: Toolkit v1 publishes one shared mailbox, and previous
Lua dual loop wrote left then right without a bridge polling opportunity. Added
per-hand pending impacts to HapticChannel. Heartbeat drains oldest pending event
at >=8ms intervals (bridge polls4ms). Same-hand pending impacts coalesce by maximum
strength/duration; events older than100ms expire. This prevents immediate dual-loop
overwrite, but v1 has no acknowledgement: a bridge stall exceeding the publication
interval can still lose an event. Not an unbounded per-shot queue. Toolkit dual
impact policy .85/75ms, or1/125ms for kick>=3.5; no global gain increase. Existing
backend choice and bridge executable retained.

Tests: actual native shot function route branch (both sides, disabled/session
gates, cached right kick, single support), actual Lua adapter native-ownership
no-duplicate behavior, existing independent profiles/OpenXR route, and C++ queue
order, spacing, coalescing, stale/invalid requests all pass. Windows Release built.
DLL SHA256 b6ee399f1ae239b3e64e6297969952204748723370eefa704145d047a8944a25
deployed with matching adapter and hashes. DLL backup
`build/diagnostics/game250/deployment-backups/20261027-183038-324`; adapter backup
`build/diagnostics/game250/haptics/dual-grip-backup-20261027-183038`. Headset
strength/latency confirmation pending. No claim that revolver aiming, cylinder
alignment or prior crash is fixed by this change.

## 2026-10-27 Overture alignment and laser surface correction (built, deployment pending)

Changes prepared after user requested completion of aiming/cylinder work:
- Native SetVRAimHit publishes a mutex-protected hit snapshot; overlay derives
  range from a fresh (<100ms), forward-facing, sufficiently collinear surface
  sample. It previously ignored this result and always used20m. Both eyes use
  the same measured range; missing/stale/off-axis samples retain configured
  range fallback. The existing CET query is Static collision group only; this
  does not promise a dynamic-target impact prediction or fix bullet redirection.
- Reload registration now supplies only cfg.rigs instead of every known gun.
  VRRigReset clears signatures and pose validity; native identification validates
  signatures each pass instead of trusting reused track pointers with equal
  bone counts. Does not distinguish two simultaneous instances of the same rig.
- Manual cylinder samples a stable closed local transform for150ms before open.
  Absolute rest*rotation writes prevent taking the base from a mid-reload
  animation. No arbitrary asset quaternion or ABI offset guessed. Reject
  missing/nonfinite/moving transforms; reset per weapon; release on closed.

Tests pass: actual Lua crane sampling/writing, opening with altered animation
base, return-to-rest, invalid/moving sample rejection; C++ hit-distance freshness,
close range, backwards/off-axis rejection; prior reload/ownership regressions.
Windows Release builds successfully. Deployment script refused because game
was running; no game files changed. Awaiting user close via async question.
In-headset cylinder and actual bullet-hit alignment remain acceptance checks.

### 18:43 crash: damaged vtable pointer; mapped reload tracks

User confirms dual-wield controller vibration now works. Preserve b6ee haptics.
The aiming/cylinder candidate had NOT been deployed during this run (closed-game
check stopped it); the crash used b6ee399f native DLL and prior reload Lua.

Dump: ReportQueue/Cyberpunk2077-20261027-184319-31400-32684/Cyberpunk2077.dmp.
Exact game SHA256 d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4.
CDB image-backed unwind: game RVA 0x9364C1, `call qword ptr [rax+0x10]`;
object 0x19fac1dde70 holds 0x00000000f3271980 where restoring upper bits to
0x00007ff7 produces a mapped game vtable. Nearby objects retain full pointers.
This is evidence consistent with an earlier 32-bit zero overwrite, not proof of
which writer caused it. Previous 18:16 dump also had a damaged low-only pointer,
but at a different allocator-like failure site. No mod frame on failing stack.
Minidump omits the native DLL's global-data pages: exact deployed PDB resolves
rig globals, but their values are unavailable. No fresh full dump in F:/crashdumps.
Do not identify misleading nearest-export AK names as actual functions.

Evidence: build/diagnostics/game250/reload/crash-184319{,-memory,-rigstate}.txt;
preserved logs in run-184319/. Latest logged Overture_Dodger at 18:41:32,
then launcher, Testera, Katana and Nue; crash is not evidence it occurred while
Overture was still equipped. REA Ghidra analysis requested against exact game;
REA timed out during whole-executable Ghidra startup after 330 seconds (no completed decompilation). CDB disassembly supplied the verified layout evidence below.

Independent concrete native bug: WeaponRig wrote float tracks at `tv[rigIndex]`,
bounded by the SOURCE rig's count. Actual 2.50 pose-apply RVA 0x1B3D13..0x1B3D39
loads a separate source/destination track remap at a2[6]+0x60, per-LOD counts
at +0x70, and copies floats into the descriptor's track buffer at mapped dst.
Track capacity is poseDesc+0x20 (existing native pose descriptor contract).
Corrected writer/readback to use that same remap and bound both index spaces;
missing mappings never write, nonfinite values rejected. This fixes a demonstrated
incorrect write path, but attribution of the two crashes remains unproven.
Removed bone-slot pin fallback: an absent live bone mapping must not reuse another
LOD/component's destination. Rest translation rejects NaN as well as excessive size.

Added production helper RigTrackMapping.hpp and executable regression covering
non-identity mapping, preserved guard values, invalid indices, zero capacity,
missing mapping and nonfinite writes. Reload cleanup/body pickup/wrist flick and
closed-cylinder helper regressions pass. Release build passed.

Deployed at 18:54:34: native SHA256
3c60a02c6b233cb597caaaf7cd7594baf068e75613f878de41a7626929222807,
matching PDB and reload Lua. Includes previously pending surface-distance laser,
active-weapon signatures, reset validity and stable closed-cylinder pose changes.
Native backup deployment-backups/20261027-185434-513; Lua backup
reload/backup-20261027-185434. Haptics adapter/bridge unchanged. Runtime validation
of crash recurrence, laser impact alignment and cylinder placement still pending.

### 19:00 aiming and revolver follow-up (candidate awaiting deployment)

User reports bullets high, especially one of two simultaneously equipped guns,
and no visible revolver reload. 18:57--18:59 ReloadAudit shows Overture_Dodger
closed baseline captured, open=true/progress=1, wrist-roll closure and mag=gone;
input/latch works. The reported bone angle is PRE-WRITE readback, so zero does
not establish that the output quaternion remained zero.

Compared original 0.1.8 WeaponAim.cpp and retained 2.31/2.50 native-contract audit.
Origin/forward/evaluator migration has exact matched callsites; the legacy aiming
logic still publishes only one active weapon's muzzle. Hitscan first-ray cache
was process-global and keyed only by muzzle-frame sequence. Projectile cone
cache was likewise shared by weapons/providers. These are incompatible with
native 2.50 dual wield and can mix another gun's aim/spread within a frame.

Candidate changes: SetVRHandMuzzle accepts a typed weapon handle and full Transform
from CET for each occupied ranged slot. Bounded 150ms mutex-protected snapshots
are keyed by actual weapon identity, not model name or most-recent shot hand.
Hitscan resolves EffectSharedData.attackData -> SDK AttackData.weapon (+0x68);
projectile providers compare their already-established attack participant handles
to the two weapon identities. Unknown dual identities retain native aim rather
than borrowing the other hand. Existing single-weapon fallback remains. Left gun
does not inherit the right sight's positional lift. Cone caches are thread-local
and keyed additionally by effect context/provider; hitscan retains the sequence
sampled with its origin. Bounded [shot-aim] messages identify resolved hand or
fallback. Actual effect-identity matching and impact alignment require runtime
confirmation; no claim all gun types are verified.

Revolver: absolute writes no longer require capturing a relative translation base.
For omitted later-LOD bones, absolute writes consult THIS component's canonical
LOD0 mapping, bounded by rig and pose capacity, instead of a global cached pin.
An actual extracted production-writer regression verifies opening with a later
pass, separate components with different destinations, and no foreign-slot reuse.
Added applied/destination diagnostics to bounded crane audit. Track bounds fix
and confirmed haptics unchanged.

Tests: real writer regression, hand identity/direction/staleness/invalid-quaternion
fixture, crane helper, reload cleanup and Weapon Lua syntax pass; MSVC Release
build passes. Deploy-HandAim250.ps1 attempted, refused because game had restarted.
NO files changed by that attempt. Waiting for user to close game; previous 18:54
build is still installed until an explicit successful deployment receipt exists.

Deployment follow-up: process check subsequently confirmed game closed. At 19:15:45,
Deploy-HandAim250.ps1 succeeded with verified native DLL/PDB, Weapon/init.lua and
Reload scripts. DLL SHA256 1b29a322670926399aadc7a5001fe5cab60fd6990547f62c05031d7ed119da7d.
Backups: deployment-backups/20261027-191545-096, reload/backup-20261027-191545,
reload/weapon-backup-20261027-191544.lua. The preceding waiting status is superseded.
Additional actual-effect-lookup fixture passes inline/indirect Variant payloads,
left/right identities, absent/foreign rejection using mocked SDK structs; this
checks implementation behavior, not the live game's Variant payload contents.
Final incremental MSVC build passed after ownership gating and pointer checks.


## 2026-10-27 — live hand/weapon aiming split and Rosco reload follow-up

User reports left Rosco straight, right Nue high, single Rosco high; Pulsar straight,
Testera straight two-handed and high one-handed. Live noninvasive CDB reads show
both per-hand muzzle snapshots empty, while legacy muzzle and provider overrides
advance. Prior candidate therefore has no runtime confirmation of per-hand aim.
Weapon CET now calls registered Game.SetVRHandMuzzle directly instead of silently
skipping publication behind a bare-global type guard. Native bounded diagnostics
report actual snapshot acceptance, including distance rejection; precise reason
for previous empty snapshots remains unproven.

Visual recoil moved from the pre-shot ShootEvents transition back to the original
native shot/provider evaluation points. Hand/class/support dependence supports an
early-recoil hypothesis but does not prove it. Haptic firing-event routing stays
unchanged. Live Rosco selects Overture; source track 8 reads 1, source 9 is absent.
Corrected profile showTrack 9 (old destination index) to source 8. Ejection now
consumes the held grip to prevent same-squeeze pickup/reseat; fresh grip can obtain
a speedloader near the body. Added bounded pickup/seat audit. Cylinder remains
attached; body pickup does not create another cylinder.

MSVC Release build and hand-muzzle, actual effect lookup, reload cleanup, crane
rest and actual rig writer tests pass. Runtime aim/reload outcome still pending.
Live CDB artifacts: build/diagnostics/game250/reload/live-rosco*.txt; predeployment
logs preserved in reload/run-before-recoil-timing.

Deployment succeeded with game closed at 19:35:07; DLL SHA256
eef7696ca6b97285ba50d47e572d5c77ee0d23f5bba5ac3bca57a76d2c103004.
DLL/PDB, Weapon/init.lua, Reload scripts and Overture profile hashes verified.
Backup: deployment-backups/20261027-193507-333 and reload/backup-20261027-193507.


## 2026-10-27 — aiming accepted; revolver extraction follow-up

User confirms aiming much better/apparently fixed after restoring recoil timing.
Reload log shows successful item pickup 19:45:33 and seat 19:45:34, but long periods
remain mag=in with crane open. Loader pickup requires all six cases free. Existing
pure-gravity extraction approaches zero acceleration at the friction threshold;
completion also unconditionally consumed the free-hand grip. Overture now opts
into assisted extraction (0.35 m/s minimum once deliberate tilt releases each
staggered case). Exit direction is latched when it starts rather than flipping
halfway through travel as the hand levels. Level gun still retains rounds.
A grip at the body when extraction finishes is preserved; grip at the cylinder
still requires release to avoid immediate reseating. Other weapon profiles keep
their existing extraction settings. Actual revolver Lua test covers level hold,
live-round accounting and complete extraction after tilt/level at 30/60/90 Hz;
reload ownership/cleanup regression passes. Runtime visual result pending.

Reload-only deployment succeeded with game closed at20:30:09; three script hashes verified. Backup reload/revolver-backup-20261027-203009 includes pretest log. Native DLL and haptics unchanged.
