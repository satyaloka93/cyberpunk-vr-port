---
type: Experimental Rendering Integration
title: DLSS 5 Neural Rendering in stereo VR
description: Measured behavior, safe operating boundary, eliminated stereo hypotheses, and unresolved load-transition lifecycle of the closed RenoDX DLSS5 addon hosted without ReShade.
tags: [dlss, neural-rendering, renodx, ngx, streamline, stereo, vrcam, performance, experimental]
timestamp: 2026-09-02T15:05:24+09:00
---

# Scope and components

Cyberpunk's ordinary DLSS Super Resolution and experimental DLSS 5 Neural Rendering are separate:

| Component | Role |
|---|---|
| `nvngx_dlss.dll` | Ordinary game-integrated DLSS Super Resolution |
| `nvngx_dlssnr.dll` | Experimental NGX feature 18 / Neural Rendering runtime |
| `renodx-dlss5.addon64` | Closed community addon that captures the game's DLSS contract and drives feature 18 |
| `CyberpunkVR_Stereo.dll` addon host | Partial ReShade API-18 host used because ReShade 6.8's OpenXR-layer path crashes this game |

The required addon is the user-selected 2026-08-28 RTX 4000-patched build, file version
`0.2026.0827.2036`, SHA-256
`87aef9ddd937c7241e6bf8d8efea0045d63559135e254c60dab316db3d3a4aee`. Public RenoDX contains
its general framework and CP2077 work, but not this addon's source. Do not replace the binary from
public version labels alone.

The host is not general ReShade support. It implements registration, config, the measured ImGui
1.92.5 slots, `init_device`, and `present`; it does not load `.fx` effects or provide arbitrary
resource/command-list APIs. Arbitrary `*.addon64` loading is not release-safe and must become an
explicit allowlist if the host survives experimentation.

# ReShade ABI findings

The API-18 config signatures include the addon module argument:

```text
ReShadeGetConfigValue(module, runtime, section, key, value, value_size)
ReShadeSetConfigValue(module, runtime, section, key, value)
```

Omitting `module` shifted all arguments and produced nonsense such as
`RenoDX.DLSS5=NRPreset`. The exact ReShade ImGui 1.92.5 slots used by this addon are:

```text
80  Separator       103 TextUnformatted   104 TextV
111 Button          115 Checkbox          129 Combo
144 SliderFloat
```

Unimplemented widget stubs must return zero. Returning true means "changed" and caused per-frame
setting commits and feature-rebuild thrashing. Correct config reads now expose `NRPreset`,
`NRStyle`, intensity, local tone/structure, skin structure, automatic mask, depth convention,
motion-vector scaling, transfer strength, UI correction, `NeuralUplift`, and
`NREnableUpscaling`.

A second ordering defect made `overlay_widgets=1` misleading: the host built the ImGui table from
its compiled default before reading the INI, so the addon retained recording stubs for the whole
launch even though F10 reported real widgets enabled. The host now loads config before constructing
the table. F10 directly embeds the addon's live page; Natural/Cinematic, preset, masks, and structure
controls apply in the current scene. The publish-facing layout keeps status and NR coverage visible
while collapsing image tuning, native color and diagnostics. Slider forwarding renders each addon
label above a full-width hidden-ID slider because ImGui's default trailing labels are difficult to
read at the far edge of the wide VR panel. Raw config fields remain only as a fallback.

# Model controls

The binary exposes:

```text
NR Preset: Default / Preset #1 / Preset #2 / Preset #3
NR Style:  Default / Natural / Cinematic
NR Intensity
Local Tone Strength
Local Structure Strength
Skin Structure Strength
Automatic Mask
```

Preset labels must not be presented as proven mappings to NVIDIA's named model generations; the
closed binary and available contract do not establish that mapping.

# Native versus addon upscaling

The addon's `Enable Upscaling` is not Cyberpunk's ordinary DLSS SR toggle.

* **Native NR** runs after ordinary DLSS SR and refines the finished target: for example,
  `3072x3072 -> 3072x3072`.
* **Addon upscaling** feeds feature 18 at the game's low render resolution: for example,
  `1024x1024 -> 3072x3072`.

Binary strings and shader inspection establish that DLSSNR 310.8 writes only the active neural
network region in the larger output resource and the addon samples that populated region with a
bilinear `SampleNeural` path. The measured low-resolution mode therefore did not provide neural
Super Resolution: Ultra Performance (`1024²`) produced dirty shimmer at roughly 55–61 FPS, while
Performance (`1536²`) was slightly cleaner but only about 30 FPS. Presets cannot restore guide
information that never entered the model. Keep `NREnableUpscaling=0`.

# Stereo isolation evidence

Read-only allocation attribution found separate runtime resource groups and feature-creation scopes:

```text
nvngx_dlssnr.dll:       MAIN 6, VRCAM 6, pointer overlap 0
renodx-dlss5.addon64:   MAIN 4, VRCAM 8, pointer overlap 0
```

Feature 18 was created once under MAIN and once under VRCAM after setup/input-resolution changes.
This strongly deprioritizes the original single-feature/shared-history hypothesis. Feature handles
remain opaque, and the addon appears to reuse one global four-texture inline/codec set sequentially,
so synchronization is not proven.

The addon installs its own `slSetTag` and `slSetTagForFrame` detours after startup and displaced the
port's first diagnostic hook. Any future guide-pointer capture must chain existing hooks without
recursion or overwriting another detour.

## Codec asymmetry

A controlled run with the port's native post-process exactly neutral established that
`NRTransferStrength` and `NRColorStrength` visibly change MAIN/the left eye but not VRCAM/the right
eye. These codec controls produce a larger visual change than the opaque model preset/style controls.
Feature 18 still created successfully in both view scopes, and preset edits caused successful feature
recreation, so this is evidence of unequal closed-addon codec state/dispatch rather than evidence that
feature 18 is globally absent.

The addon binary contains readable shader source for its `UpgradeToneMap`, OKLab hue transfer,
`PaperWhiteScale`, `TransferStrength`, and `ColorStrength`. Reusing that codec exactly requires its
original/proxy/neural inputs; a final-image grade does not have those inputs and must not be described
as an exact port. The stereo-safe boundary is therefore to leave one-eye-only codec strengths neutral
and perform final color grading after NR with identical MAIN/VRCAM constants.

# The eye asymmetry: what the visible stage is bound to

A live A/B — the addon's own `Enable DLSS Neural Rendering` checkbox, which is a real forwarded
widget and applies in the current scene — established a sharper statement than "codec asymmetry":
**the right eye looks identical with NR on and off**, and the effect is only visible at all once
HDR Transfer Strength is non-zero.

The addon's own log carries the discriminator, and it was present from the first hosted session:

```text
feature 18 created ..........  2 (or more)   one per view, per configuration change
signed DLSSNR runtime init ..  1
UpgradeToneMap codec ........  1
created inline NR resources .  1
```

**The model is built per view. The visible codec and its four-texture inline set are built once.**
That is the asymmetry, stated by the binary itself.

## Three eliminated hypotheses

| Hypothesis | Test | Result |
|---|---|---|
| The codec dispatches per view but grades unequally | Neutral-native A/B with the port's post-process exactly neutral | Rejected: one codec is ever created |
| Per-eye binding follows allocation scope | `[DLSSNR-DIAG][alloc]` view attribution across sessions | Rejected: MAIN 0 / VRCAM 40 in one session, MAIN 7 / VRCAM 18 in another. Allocation-time scope is timing-dependent and says nothing about consumption |
| The codec is per-swapchain state, and the host only reports one swapchain | `present_per_view=1`: VRCAM announced as a distinct `api::swapchain` object with its own `get_native()`, `present` dispatched twice per frame | Rejected: `per-view=1` armed, no faults, codec still 1, inline resources still 1 |

The third was worth testing because RenoDX keys state per swapchain and per device
(`src/utils/swapchain.hpp`, `device.hpp`, `state.hpp`) and the host reports exactly one of each.
It does not key the codec that way.

## Closed by static analysis: the resource set is a single-slot cache

Disassembly of `renodx-dlss5.addon64` (SHA-256 `87aef9dd…`, 254 KB `.text`, 70,250 instructions)
settles it. Cross-referencing the addon's own log strings located the two functions, and their
entry guards are unambiguous.

**The inline NR resource set** — `+0x0D510`, which logs `created inline NR resources`:

```asm
xor  edi, edi
cmp  qword ptr [rip+0x4BA99], rdi   ; 0x180059000  cached pointer == 0 ?
je   rebuild
cmp  dword ptr [rip+0x4BABC], r9d   ; 0x18005902C  five scalar key fields
jne  rebuild
cmp  dword ptr [rip+0x4BAB7], r12d  ; 0x180059030
jne  rebuild
cmp  dword ptr [rip+0x4BAB3], edx   ; 0x180059034
jne  rebuild
cmp  dword ptr [rip+0x4BAAF], ebx   ; 0x180059038
jne  rebuild
cmp  dword ptr [rip+0x4BAAE], r13d  ; 0x180059040
jne  rebuild
                                     ; fall through = reuse the existing set
```

One record at `0x180059000..0x18005904F`, written back with three `movaps` stores at `+0x0D9E7`,
`+0x0DA09`, `+0x0DA14`. **Every reference is a fixed RIP-relative address — there is no base+index
form anywhere**, so there is no array and no per-view capacity.

**The codec** — `+0x0C8BA`, which logs `created Control-equivalent … UpgradeToneMap codec` — is the
same shape one level simpler: two qwords at `0x18005A6D0` / `0x18005A6D8`, `cmp …, 0` then
`mov rcx, [that]`. A null-latch, single-slot, also at fixed addresses.

### Why this closes the per-eye question

Two views can never hold sets simultaneously. If their keys match they **share** one set; if the
keys differ the set is **rebuilt**, and alternating views rebuild every frame. That is not a
hypothesis — it is what the compare-and-branch does.

It also retro-explains the rebuild thrashing seen while driving settings from F10 (11 creations
against 2 evaluation log lines, flip-flopping `native`/`upscaling`): the same cache being
invalidated and rebuilt, not a separate defect.

In practice the set is created **once** per session, so both views present a matching key — the
addon writes into one shared resource and one eye's composite carries it.

**No input the host can vary produces two coexisting sets.** Varying the key produces per-frame
rebuilding, which is strictly worse. Goal "NR on both eyes" is closed against this addon build, not
merely unpromising. Matched eyes remain available through the port's own native post-process, which
grades MAIN and VRCAM with identical constants.

## Current public addon 4.55 does not fix stereo ownership

RHI's public `renodx-dlss5` 4.55 (`RenoDX DLSS5 Generic v4.1.5`, build 2026-08-30,
SHA-256 `9150097c…`) was tested separately against the preserved RTX 40-patched runtime. Its newer
feature-handle registry, lazy adoption and lifecycle code do **not** produce per-view inline state in
Cyberpunk VR:

```text
renodx-dlss5.addon64 committed allocations: MAIN 4, VRCAM 0
nvngx_dlssnr.dll committed allocations:      MAIN 6, VRCAM 12
UpgradeToneMap codec creations:               1
inline NR resource-set creations:             1
```

Feature 18 created under both MAIN and VRCAM contexts without a logged NGX failure, but the user
saw the live NR change only in the **left eye—VRCAM**. This port defaults
`CyberpunkVR_MainIsRightEye=1`, so MAIN is the right eye and desktop mirror; neither changed. That
mapping corrects earlier informal MAIN/left assumptions.

The test closes “the newer addon automatically fixes it,” but initially left two mechanisms to
distinguish: the later VRCAM evaluation could overwrite one shared inline set, or MAIN's modified
result could be bypassed/overwritten by later MAIN composition.

A subsequent exact-prologue, MinHook-based census around the outer
`sl.interposer.dll!slEvaluateFeature` call narrowed that boundary without touching the signed NR
runtime. Before VRCAM started, MAIN reached 1,304 evaluations. Afterward a 64-call contiguous sample
alternated `V M V M …` with zero violations and every result was success. In the engine's
MAIN-then-VRCAM render sequence, VRCAM is the later call in each pair. Sampled steady CPU recording
cost also differed materially:

```text
MAIN   median 157 us   range 111..558 us
VRCAM  median 551 us   range 245..777 us
```

A final read-only writeback census resolved the remaining ambiguity. Before VRCAM activation, 9 of
10 sampled MAIN calls recorded the addon's complete eight-command envelope: copy game output into
the singleton set, four root-table binds, two `160x160x1` codec dispatches, then copy singleton
output back. After VRCAM was adopted:

```text
sampled MAIN calls:   36   with addon commands: 0
sampled VRCAM calls:  37   with addon commands: 26
```

Active VRCAM samples used the same singleton descriptors and exact eight-command envelope. The same
command-list objects later carried VRCAM work but no MAIN work, ruling out missing command-list
instrumentation. Therefore MAIN is **not** processed and later erased by MAIN composition: after the
second DLSS feature is adopted, the addon records no NR codec/writeback commands for MAIN at all.
The closed addon transfers exclusive active ownership to VRCAM. A port-side final-composition fix
cannot recover a MAIN neural result that was never produced.

Evidence: `20260831-143600-renodx455-vrcam-left-only`,
`20260831-151000-sl-evaluate-main-vrcam-order`, and
`20260831-154500-dlss-writeback-vrcam-takeover`.

### Method, for reuse

The pipeline is scripted and repeatable against any closed addon: locate log strings by RVA,
linear-sweep `.text` with capstone **restarting after each undecodable byte** (a single pass stops
at the first one and finds nothing), match RIP-relative operand targets to those RVAs, walk back to
each function prologue, then look for globals that are compared at entry and stored before return.


# Feature-18 diagnostics: the old trampoline is disabled, but a tail-jump prehook is proven

The port's old `HookedNrEvaluate`/create/release forwarding functions remain disabled and are never
registered. They call MinHook's trampoline; that `CALL` pushes a return address inside the port over
the addon's caller address, and signed runtime 310.8 refuses the evaluation with `0xBAD00002`.
`g_nrDiagState` now starts at `0` only to arm the separate tail-jump implementation described below;
changing hook state must never register the old forwarding functions.

The broader conclusion previously written here—"any direct hook is permanently impossible"—was
wrong. UEVR subsequently proved a narrower, return-address-preserving construction in both The
Outer Worlds 2 and GTA San Andreas DE: enter a hand-built thunk by `JMP`, save the argument
registers, call only a prehook, restore the arguments, then **`JMP` rather than `CALL`** to the
trampoline. The signed runtime still sees the addon's original return address and recorded zero
refusals. This makes a read/modify **pre-evaluate** census feasible in Cyberpunk.

It does not make independent feature-18 calls feasible. The thunk gives up the return path, cannot
observe the result directly, and does not change the ordinary-call refusals recorded below. Do not
simply change `g_nrDiagState` from `-4`: the existing `HookedNrEvaluate`/create/release functions
still call their trampolines and will still trigger `0xBAD00002`. A replacement must be an
exact-prologue, collision-refusing tail-jump prehook and must re-arm after device reset.

## Proxy-resource proposal: diagnostic gate before allocation or substitution

UEVR also measured the addon's NGX parameter vtable: slot 1 is the resource setter and slot 6 is the
float setter. Wrapping slot 1 can record—and technically substitute—the Color, Output, Depth and
motion-vector resources passed immediately before feature-18 evaluation. Mistyping slot 1 corrupts
resource pointers and hangs the title screen, so no other slot may be wrapped without independent
ABI proof.

That fact alone does **not** establish that a stable proxy can repair Cyberpunk stereo:

* Static analysis of the old addon's singleton cache found five **scalar** key fields, not resource
  pointers. MAIN and VRCAM already match that key and share the singleton; pointer identity is not
  the measured reason ownership transfers.
* With public addon 4.55, the decisive writeback census found zero addon commands on MAIN after
  VRCAM takeover. Slot-1 calls happen inside the inner feature-18 path, after the addon has selected
  whether to process the outer DLSS feature. Substitution there cannot affect an earlier ownership
  decision if MAIN never reaches that path.
* A proxy would need stable Color, Output and guide resources, explicit state transitions, and
  delayed copyback. Copying one eye's result at the next evaluation necessarily gives the two eyes
  different temporal ages until measured otherwise.

The behavior-neutral tail-jump/slot-1 census answered the gate. The first build (`252dfab9…`) failed
closed before installation because MinHook rejected its non-executable private thunk (`status=7`),
so that run supplies no inner-entry evidence. The corrected build (`16f6ad5e…`) made the thunk RX
before `MH_CreateHook`, writable only while the hook was disabled to patch its trampoline target,
and RX again before enabling. It installed successfully, produced no `0xBAD00002`, forwarded every
slot-1 resource unchanged, and shut down cleanly.

First VRCAM inner entry occurred at outer sequence `1283`. MAIN then made exactly `120` transitional
inner calls (`480` slot-1 calls: Color, Output, MVec and Depth), after which its counters remained
fixed at `1400` inner / `5596` slot calls while VRCAM advanced through at least `2200` inner / `8796`
slot calls. Outer Streamline MAIN continued independently through side counts `1800`, `2400` and
`3000`. MAIN and VRCAM also used distinct feature handles and parameter objects.

Therefore MAIN briefly overlaps VRCAM during adoption but does **not** reach signed feature 18 or
slot 1 in steady state. The sustained skip lies between outer `slEvaluateFeature` and inner feature
18. Reject the inner slot-1 proxy as a stereo fix: substitution or copyback cannot affect later MAIN
frames for which the setter and evaluation never run. Evidence:
`20260902-105625-dlssnr-inner-tail-takeover`.

Static analysis of exact addon 4.55 located the next boundary. Both addon evaluate wrappers call the
original DLSS evaluation, then call `addon+0x303F0`; only a true return reaches the inline NR routine
at `addon+0x30A50`. The predicate looks up the NGX handle in one of the addon's two registries and
returns true only when the recorded feature ID is DLSS (`1`) or Ray Reconstruction (`13`). This
made handle eligibility/registry state the next measured boundary.

The exact-build census then proved that boundary remains healthy. MAIN's ordinary DLSS handle stayed
feature `1`, present in the independent ledger, unreleased, and `eligible=1` through at least 3,000
outer MAIN evaluations. VRCAM was independently feature `1` and eligible. Both releases occurred
only during normal shutdown. Nevertheless MAIN's signed inner count stayed fixed while VRCAM
advanced, so MAIN enters `+0x30A50` and is suppressed later. Evidence:
`20260902-134722-renodx455-eligible-before-inner-skip`.

Helper `+0x4EF0` was the next measured gate: it snapshots tracked host compute/graphics binding
state into six validity fields at offsets `+0`, `+1`, `+0x10`, `+0x20`, `+0x30` and `+0x40`.
Transparent census `f35a833a…` found `mask=0x3F`, complete, for both views through outer counts above
2,400 while MAIN inner calls remained flat. Binding-state incompleteness is rejected.

The actual suppression is addon 4.55's output/frame duplicate guard inside executor `+0x8910`.
Ordinary NGX `Output` is the key in the map rooted at `+0x18D3E8`; node `+0x18` stores the last addon
frame. At `+0x8CA2..+0x8CB3`, equality with current frame `+0x18A3E8` returns before feature 18.
After success, `+0xAF70..+0xAF91` writes the current frame to that Output entry. Cyberpunk's
sequential eyes reuse the pooled ordinary DLSS Output: the final MAIN source before takeover and
first VRCAM source were both `000002CF485BF940`. The addon therefore mistakes the other eye for a
duplicate in the same frame. Evidence: `20260902-135619-renodx455-output-frame-dedup`.

The bounded behavior-changing experiment implements a conditional bypass of only the equal branch
at `+0x8CAD`, restricted to exact addon 4.55, a known MAIN/VRCAM outer scope, and post-VRCAM
takeover. A nearby thunk consumes the original comparison flags, preserves every volatile GPR/XMM
register around its scope callback, and jumps either to normal fallthrough `+0x8CB3` or the original
skip target `+0x9471`. Unknown/non-VR calls preserve the original branch; no map, resource,
parameter, frame counter or result is rewritten. Exact branch bytes `0F 84 BE 07 00 00` are required
before patching. Deployed experimental DLL:
`d51d7cdc3dfcfb8e27dc6bea9ac8418313c816e1de44ffbe8501a60df9aceb15`.

The first run succeeded visually and diagnostically. The user saw NR in both eyes; every successive
600-inner-call summary advanced exactly `300 MAIN + 300 VRCAM`, slot 1 remained exactly four sets per
inner call, and all sampled post-takeover writeback sequences were active (`21/21` MAIN, `23/23`
VRCAM). No bad outer result, `0xBAD...`, device loss, DRED fault or fresh crash report appeared.
Evidence: `20260902-141107-renodx455-stereo-dedup-success`.

The cost is real full-resolution binocular NR. At `2560x2560` per eye, game cadence settled around
`27.7..33.7 FPS`; SteamVR stepped from 90 Hz to 45 Hz and then 30 Hz. OpenXR remained healthy at
`7800` cycles / `7798` submits / `2` misses, and median sampled outer calls remained sub-millisecond,
so this is model GPU pixel cost rather than submission failure. Strength sliders do not reduce that
cost, and addon upscaling remains unsafe/visually poor. Lowering output resolution is the safest
performance lever. This run ended its XR session without a WER record but did not log addon
`destroy_device` or plugin unload, so lifecycle/save-transition safety remains unproven.

A clean performance build now retains only the required Streamline outer-context hook, exact addon
identity/branch validation, and conditional branch thunk. It does not install the six RenoDX
eligibility/create/release/state forensic hooks or signed feature-18 tail/slot recorder; outer timing,
burst and writeback tracing are disabled. The completed writeback census's command-list slots
16/17/31 and device SRV/UAV slots 18/19 are left native, and no descriptor ownership map is built.
Deployed clean DLL: `7bba31c4fddf55102a7ac3a4b7833dd659adac6c5ff73a6dfa6ab82273db188b`.

The clean run was also changed from `2560x2560` to `2048x2048`, so it is not an instrumentation-only
A/B. Below-60 FPS samples had median `47.05` (mostly `46..49`) versus `33.5` in the preceding 2560²
instrumented run. The 36% per-eye pixel reduction plausibly explains most of the roughly 40% median
uplift; a resolution-matched run is required to isolate diagnostic overhead. OpenXR remained healthy
through `34800/34798/2` cycles/submits/misses. Ordinary active-NR teardown completed fully:
`destroy_device`, addon NR release/re-arm, successful NGX unhooks, OpenXR shutdown, plugin unload and
addon unregister, with no fresh crash report. Evidence: `20260902-143911-renodx455-clean-stereo-2048`.
This proves ordinary process teardown only, not save/menu or same-size swapchain transitions.

Experimental foveation is implemented separately from rollback checkpoint `847b2f6`. It uses the
proven signed-runtime tail-jump and measured slot-1 resource Set ABI, but identifies eyes from the
attributed outer scope rather than parity. The first build retained a full-height 65% horizontal
slab open toward the nasal/binocular side: MAIN/right kept the left 65%, while VRCAM/left kept the
right 65%. Only the untreated temporal band was copied from current Color to persistent Output
before each feature-18 evaluation; whole-resource copies remained prohibited. All four
Color/Depth/MVec/Output subrects were rewritten only after the complete contract and exact full
Output rect validated. Menu calls, unknown views, pre-takeover calls and missing/mismatched
resources fail closed.

That 65% slab passed its first structural and visual acceptance at actual `2560x2560`: both eyes
sustained `1664x2560` active regions, every sampled subrect readback was exact, and MAIN/VRCAM
advanced 1:1 through 7,800 feature-18 evaluations. The user could not see the temporal transition.
Below-60 gameplay samples had median `43.8 FPS` (range `37.3..50.1`) versus `33.5 FPS` for the prior
full-region binocular run at the same resolution, approximately 31% observed uplift. Scene matching
was imperfect, so this is strong directional evidence rather than a controlled GPU-time benchmark.
OpenXR remained healthy at `16200/16198/2` cycles/submits/misses and no fresh WER/REDEngine report
appeared in that first run. It logged XR session end but not full plugin/addon teardown. Evidence:
`20260902-151427-dlssnr-foveation65-2560-success`.

A later 65% run produced a real `DXGI_ERROR_DEVICE_HUNG` after 296 seconds. It is not a new
foveation-specific signature: the engine breakpoint/stack prefix and DRED endpoint exactly match
pre-foveation reports `20260830-192912` and `20260830-194318`. Gameplay had sustained exact slabs
through 4,207 sampled applies per eye before engine menu mode became active. Foveation then stopped
issuing copies/subrect rewrites as designed, while feature-18 calls continued with menu parameter
samples reporting null resources. GPU progress stopped at
`HologramDepth_and_Distortion/FinalFlushBarriers` and `DecoupledParticleLighting`; overlay fence
timeouts and stale-VRCAM mono fallback followed the stall. This strengthens the existing active-NR
menu/lifecycle warning but neither proves nor exonerates an earlier foveation command. Evidence:
`20260902-182546-foveation65-matched-menu-gpu-hang`.

Four live presets are exposed in F10 and persisted in `bin/x64/nr-foveated.ini`:

```text
35% Center Box   = 35% width x 35% height = about 12.25% model pixels
50% Center Box   = 50% width x 50% height = 25% model pixels
65% Stereo Slab  = 65% width x full height (validated balanced default)
80% Stereo Slab  = 80% width x full height (quality)
```

The centre boxes use mirrored 4% horizontal offsets (VRCAM/left toward the right, MAIN/right toward
the left) and copy four non-overlapping peripheral bands before NR. This is why UEVR's foveation is
square: applying the fraction to both axes multiplies the saving, while a full-height Cyberpunk slab
spends more model pixels to eliminate top/bottom boundaries and retain only one temporal boundary.
UEVR proves evaluation subrects can change without recreating feature 18; an earlier claim that the
closed addon necessarily caches region-sized state was unsupported. Cyberpunk therefore applies the
selection live, but never directly from the UI thread: the request is atomically latched only at a
validated gameplay MAIN boundary, and the following VRCAM evaluation reads the same active preset.
Requests made during menu/loading mode remain pending until gameplay resumes. No addon setting,
feature handle or allocation is recreated.

There is still no post-NR ring shader or proven ControlMask. UEVR's uncommitted ring is armed in a
prehook and slot 0 fires on NR's own first `SetDescriptorHeaps`, before inference completes; Close is
already proven too late. Copying that insertion search would risk stale/frozen eyes and pipeline-state
clobber. The validated slab therefore retains its single temporal hard edge, while centre-box modes
have four boundaries and require separate visual/stability acceptance. An outer-edge-only
suppression-mask feather remains a later gate.

# Driving DLSSNR without the addon: closed

The addon's single-slot cache means per-eye Neural Rendering can only come from owning the NGX
calls ourselves. That was attempted and the runtime refuses it.

With `nvngx_dlssnr.dll` loaded by the port itself (`LoadLibrary` succeeds, exports resolve) and a
parameter block obtained from the game's existing NGX context — no second `NVSDK_NGX_D3D12_Init`,
which is the one call that could disturb the game's own DLSS:

```text
core    GetScratchBufferSize(18) -> 0 bytes  result=0xBAD0000C   FAIL_UnableToInitializeFeature
snippet GetScratchBufferSize(18) -> 0 bytes  result=0xBAD00002   FAIL_FeatureNotSupported
core    CreateFeature(id=18)     -> handle=0 result=0xBAD0000C
snippet CreateFeature(id=18)     -> handle=0 result=0xBAD00002
```

`0xBAD00002` from the snippet is **the same code the feature-18 census produced**, and for the same
reason: the signed 310.8 runtime will not serve a caller it does not accept. Reaching it by an
ordinary call rather than a trampoline changes nothing. `GetScratchBufferSize` is a pure query that
allocates nothing and reads almost no parameters, so its refusal rules out any explanation based on
creation parameters — which were, by then, taken verbatim from the runtime's own string table:
`DLSSNR.Width` / `DLSSNR.Height` prefixed, `CreationNodeMask` / `VisibilityNodeMask` /
`PerfQualityValue` **bare**.

**Feature 18 is only reachable from inside the flow the addon detours.** This closes both
"neural rendering on both eyes" and "run the model before the upscale": neither is blocked by
resolution, parameters or plumbing, but by the runtime declining an independent instance.

An earlier `0xBAD0000C` reading — "the app's NGX context has no feature-18 registration" — was
incomplete: with the addon disabled nothing loads `nvngx_dlssnr.dll` at all, so the core was
dispatching to a snippet that did not exist. Loading it ourselves is what turned the question into
the answer above.

# Correction: the port's Streamline tag capture had never worked

Recorded here previously as "the addon displaced the port's first diagnostic hook". That is wrong.
The hook **never installed**, with the addon present and with it absent:

`InstallE9HookAt` writes an `E9 rel32` jump, which reaches +/-2GB. `sl.interposer.dll` and
`CyberpunkVR_Stereo.dll` land tens of gigabytes apart, so the patch could never be written — and
the install line was behind `g_verboseLog`, so the failure was silent for the life of the code.
`NgxAcquireDepth()` and `NgxAcquireMotionVectors()` returned null throughout.

Re-implemented with MinHook, which allocates its trampoline near the target and is already used
elsewhere in the same file. Safe against `sl.interposer`, which has no caller validation; **not**
safe against `nvngx_dlssnr`, which is why the census stays disabled.

What the working capture then established:

| | |
|---|---|
| Live tag entry point | `slSetTag` — **not** `slSetTagForFrame`. Both are hooked now; only the first is called |
| Buffer types tagged | `type=0` depth and `type=1` motion vectors only, 853x853. **No `ScalingInputColor`** |
| Per-view guides | Most depth/MV resources appear under **both** MAIN and VRCAM; only 2 of 20 were unique to VRCAM |

That last row is unresolved and matters: if the two views are handed the same guide buffers, then
per-eye neural rendering may not produce per-eye output even where it is achievable. It should be
settled before any future attempt at per-eye work, whatever the mechanism.


# Reverse-engineered reference

**No public documentation of any of this exists.** The addon's source is not in the RenoDX
repository (which ships only `renodx-fpslimiter` and `renodx-devkit`), and the community
distribution that packages it — [RR-and-DLSS-5-RenoDX-for-the-Games][dist] — carries binaries,
notes and an installer profile table only. Its Cyberpunk entry is
`{"Cyberpunk2077.exe", "bin/x64", renoDxFirst=true, rrInMenu=true}` and it defines no addon
settings. Everything below was derived by instrumenting the host and disassembling the binary; it
is the only record.

## The addon binary

```text
renodx-dlss5.addon64   391,168 bytes   file version 0.2026.0827.2036
SHA-256  87aef9ddd937c7241e6bf8d8efea0045d63559135e254c60dab316db3d3a4aee
exports  NAME, DESCRIPTION            (const char* globals, not functions)
imports  KERNEL32.dll only, 97 symbols
sections .text 0x3E0E8, .detourc / .detourd  (Microsoft Detours)
```

It binds to a host by calling `K32EnumProcessModules` and taking the **first** module that exports
`ReShadeRegisterAddon` — it does not link against ReShade. It requires `nvngx_dlssnr.dll` beside
itself (`"nvngx_dlssnr.dll was not found beside the addon"`).

## Config keys, and where each is reachable

The INI is read **once, at registration**; the F10 page writes the addon's live variables. So the
same setting has two routes with different timing: **INI is next-launch, F10 is immediate.**

| `[RenoDX.DLSS5]` key | NGX parameter | F10 label |
|---|---|---|
| `NeuralUplift` | — | Enable DLSS Neural Rendering |
| `NREnableUpscaling` | — | Enable Upscaling (locked off; see below) |
| `NRPreset` | `DLSSNR.Hint.Render.Preset` | NR Preset |
| `NRStyle` | `DLSSNR.Style` | NR Style |
| `NRIntensity` | `DLSSNR.Intensity` | NR Intensity |
| `NRLocalTone` | `DLSSNR.LocalToneStrength` | Local Tone Strength |
| `NRLocalStructure` | `DLSSNR.LocalStructureStrength` | Local Structure Strength |
| `NRSkinStructure` | `DLSSNR.SkinStructureStrength` | Skin Structure Strength |
| `NRAutoMask` | `DLSSNR.UseAutoMask` | Automatic Mask |
| `NRUICorrection` | `DLSSNR.UICorrection` | NR UI Correction |
| `NRDepthMode` | `DLSSNR.DepthInverted` | Depth Convention |
| `NRMVecScaleX` / `NRMVecScaleY` | `DLSSNR.MVecScaleX` / `Y` | Motion Scale X / Y Multiplier |
| `NRPaperWhiteScale` | — (codec) | Scene Paper-White Scale |
| `NRTransferStrength` | — (codec) | HDR Transfer Strength |
| `NRColorStrength` | — (codec) | Color Strength |

Resource parameters the runtime consumes, all readable from the contract:
`DLSSNR.Color`, `DLSSNR.Output`, `DLSSNR.MVec`, `DLSSNR.Depth`, plus
`DLSSNR.Input/Output Width/Height`, the `Color`/`Output` subrect quadruples, and `DLSSNR.Reset`.

## What the host reports, and what follows from it

The host presents exactly **one** device, queue, swapchain and effect_runtime, and dispatches
`present` once per frame. Its NGX detours fire twice, because the game drives those. That topology
is why per-swapchain state exists once — although the single-slot cache above means announcing two
swapchains changes nothing.

## Host flags: live versus restart

| Live | Restart required |
|---|---|
| every control on the addon's page | `[host] enabled` |
| `draw_overlay` | `dispatch_events`, `dispatch_present`, `present_per_view` |
| the port's native post-process sliders | all `[RenoDX.DLSS5]` keys |
| | `native-post.ini` file edits (F10 sliders are live) |

## Locks

`NRTransferStrength` / `NRColorStrength` were pinned to zero in the F10 slider and on both sides of
the config ABI. The read-side clamp mutated the store, so a hand-edited INI reverted on every read.
All three are removed: a lock only prevents an immediately visible accident, and zero is always
reachable. `Enable Upscaling` remains locked off — a live native/upscaling transition produced
three feature generations and fell to 11.5 FPS while XR kept submitting.

[dist]: https://github.com/renodxdlss5/RR-and-DLSS-5-RenoDX-for-the-Games


# Visual and performance results

| Configuration | Result |
|---|---|
| Native `3072²`, Natural | User reported no low-resolution shimmer; median measured gameplay about 38.3 FPS |
| Native `2560²`, Natural | Feature 18 active with `853²` Ultra Performance guides; measured gameplay samples about 44–55 FPS |
| Addon upscaling `1024² -> 3072²` | Approximately 55–61 FPS with user-reported temporal shimmer |
| Addon upscaling `1536² -> 3072²` | Approximately 30 FPS; user reported less shimmer than `1024²`, but shimmer remained |

Multiple native-NR runs produced a user-visible left/right difference in reflection or lighting
brightness. Logs in those runs showed separate MAIN/VRCAM feature creation, no resource-pointer
overlap, and no sustained stale-eye fallback. The source of the binocular output difference is
unresolved; do not describe allocation isolation as proof of identical per-eye model output.

# Load-transition lifecycle failure

Native `2560²` exposed the decisive current blocker. At the first in-session save transition,
Cyberpunk called same-size `ResizeBuffers(2560x2560)`. The addon did not recreate feature 18 or
adopt the replacement guide/output resources. It then emitted 71,978 consecutive rows:

```text
DLSS5 Generic: skipped an NGX evaluation with incompatible guide/output dimensions
```

The approximately 90 FPS observed after that transition was ordinary DLSS with Neural Rendering no
longer applied—not an NR performance breakthrough. Until recovery is implemented, the only valid
experimental workflow is one save per process: exit Cyberpunk completely before testing another
save.

Recovery is unproven. The acceptance criterion is a second in-process save load that logs fresh
native resources, fresh MAIN/VRCAM feature creation, and continuing successful evaluations without
incompatible-guide spam or a renderer stall.

# Crash separation

The third in-session load on the lifecycle-failed run crashed while spawning the player at
`Cyberpunk2077.exe+0x1F51F5`, with the same bad descriptor sentinel and register pattern as the
pre-DLSSNR 2026-08-23 crash. This is not a new DLSS5 crash family, although the addon may alter its
timing. See the [second-eye load crash](/fixes/second-eye-load-crash.md).

There was no OOM, device removal, or GPU fault; reported GPU memory use was 9,884 MB. The run also
proves the optional low-spec INI is not required for this crash family.

## A second, unattributed load-transition fault site

2026-08-31 08:40:55 produced `EXCEPTION_ACCESS_VIOLATION` **writing** to `0x30000000E` at
`Cyberpunk2077.exe+0x19088D0`, 71 s in, with `IsLoadingSavedSession=true` and
`numberOfStateMachines=0` — mid-load, before the player spawned, on the first save of the process.

```text
RCX 000000030000000E        the faulting store goes through it at function entry
[rsp+0] Cyberpunk2077.exe+0x14BABD    the caller supplied the pointer
```

The stack is `Cyberpunk2077.exe` frames to `kernel32!BaseThreadInitThunk`; no port, addon, NGX or
D3D12 frames appear. `0x3_0000_000E` is a value assembled from two 32-bit halves rather than a
corrupted heap address.

This site appears in **0 of 58** prior dumps (2026-08-07 to 2026-08-30) and is not the
`+0x1F51F5` family — different address, and a write rather than the read at `…0AB0`. The
`[descguard]` detour at `+0x774384` was installed and fired zero times that session.

**Unattributed.** A fault site new as of the 2026-08-30/31 work, in a path where none of our frames
appear, is suspicion and not attribution. Isolation order if it recurs: `enabled=0` in
`reshade-addons.ini`, then `Enabled=0` in `native-post.ini`, then the F10 descriptor-guard
checkbox.

# Evidence locations

```text
%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-114131-dlss5-render-stall
%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-122405-dlss5-upscaling-dirty-flicker
%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-124921-dlssnr-stereo-census
%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-174430-dlssnr-native-natural
%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-175503-dlssnr-load-transition-crash
```

# Release boundary

Normal DLSS SR remains the supported path. Neural Rendering is experimental and cannot ship enabled
by default while it fails across in-session loads, depends on a closed leaked runtime/addon, and
cannot maintain the 90 Hz native cadence. Keep `nvngx_dlssg.dll` absent because its leaked
replacement produced the separate repeatable startup-crash family.

# Citations

[1] [RenoDX public repository](https://github.com/clshortfuse/renodx)
[2] [DLSS5-Feeder](https://github.com/jlrouzies-fr/DLSS5-Feeder)
[3] [FF7R DLSS5 integration](https://github.com/zhubaohi/FF7R-DLSS5)
[4] [NVIDIA input clarification reported by TechPowerUp](https://www.techpowerup.com/347585/nvidia-dlss-5-takes-2d-frame-and-motion-vectors-as-input)
