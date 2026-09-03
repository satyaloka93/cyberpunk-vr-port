# Hosting ReShade addons in the plugin, and what DLSS 5 Neural Rendering cost

This is the engineering record for Cyberpunk's experimental native host for the RenoDX DLSS5
addon. The host runs inside `CyberpunkVR_Stereo.dll`; it does **not** load ReShade and is not a
general ReShade implementation. Durable conclusions are summarized in
[DLSS 5 Neural Rendering in stereo VR](../.okf/architecture/dlss5-neural-rendering.md).

## Current publication status

- Exact supported addon: RenoDX DLSS5 Generic v4.1.5 / file version `0.2026.0828.0517`, SHA-256
  `9150097cdee2953cdc9894d2e5606ea5100e6c8f95fc7bb1b407328b4391a07a`.
- Exact NR runtime: `nvngx_dlssnr.dll` 310.8, SHA-256
  `8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206`.
- The port's exact-build duplicate-output bypass is required for binocular output because
  Cyberpunk's sequential eyes reuse one ordinary DLSS Output pointer in an addon frame.
- F10 offers live 35%/50% center-box and 65%/80% full-height stereo-slab performance profiles.
  Changes latch on a MAIN-to-VRCAM gameplay boundary and defer while a menu/loading state is active.
- `Enable Upscaling` must remain off. ReShade and `nvngx_dlssg.dll` must remain absent.
- **Release blocker:** active NR can still hang the GPU across pause/map/inventory/save transitions.
  The matched signature is `0x887A0006` with `HologramDepth_and_Distortion` and
  `DecoupledParticleLighting` stopped at final barriers. Prefer one save per process.

The remainder is the chronological investigation that produced those constraints.

### Superseding results from the final 2026-08-30 runs

Later tests corrected three conclusions in the chronological investigation below:

1. The addon's low-resolution `Enable Upscaling` path is not the viable path. DLSSNR 310.8 writes
   only the active network-sized region and the addon's shader bilinearly samples it into the
   larger target. `1024² -> 3072²` shimmer is therefore expected; presets cannot restore missing
   guide detail.
2. Native Natural NR can look clean. At `3072²` it removed the upscaling shimmer but measured about
   38 FPS median. At `2560²` it initially measured about 44–55 FPS and made faces/lighting subtly
   different.
3. Native NR currently fails across an in-session save transition. A same-size
   `ResizeBuffers(2560²)` left the addon in 71,978 consecutive incompatible-guide skips without
   recreating feature 18. The later 90 FPS was ordinary DLSS after NR had stopped, not a speedup.

The older Discord addon discussed below is preserved only as historical evidence. It is superseded
for current builds by the exact public addon identity listed under **Current publication status**.

---

## Part 1 — the crash that came with the DLSS 5 files

### Symptom

Five crashes in twenty minutes on 2026-08-29, starting ~2 minutes after a leaked Streamline/NGX set
was extracted into `bin\x64`. Two distinct faults, initially conflated.

### Fault A: `+0x73DBFD`, new

Byte-identical across three processes with different ASLR bases:

```
RAX = R  (region base)        RIP = R + 0xD        fault: read [R + 0x73DBFD]

bytes at R:  40 55 56 57 41 56          push rbp/rsi/rdi/r14
             48 81 EC B8 00 00 00       sub  rsp, 0B8h
             48 8B 05 E9 DB 73 00       mov  rax, [rip+73DBE9]   <-- RIP, faults
```

A function prologue dying on the **first data reference it makes** — a RIP-relative load of a global
7.6 MB into its own image. `R` is in no loaded module.

**The discriminator is one file.** `nvngx_dlssg.dll` — DLSS frame generation — is loaded in every
dump:

| | `nvngx_dlssg` SizeOfImage | `+0x73DBFD` crashes |
|---|---|---|
| driver's build (pre-swap) | `0x75B000` | none in any of the 47 prior dumps |
| the dropped build | `0x735000` | 5 |

`0x73DBFD` is **inside** a `0x75B000` image and **`0x8BFD` past the end** of a `0x735000` one. Code
minted against the driver's frame-gen snippet reaching for a global that falls off the end of the
one that replaced it.

Frame generation was **off** in `UserSettings.json` throughout (`FrameGeneration = "Off"`,
`DLSSFrameGen = false`). Streamline loads every plugin at startup to query capability, so *loaded*
and *active* are different things — and loaded was enough.

**Resolution:** remove `bin\x64\nvngx_dlssg.dll`. NGX then falls back to the driver's matched build.

| window | duration | `+0x73DBFD` crashes |
|---|---|---|
| dlssg present | ~2 h | 5 |
| dlssg removed | ~4 h | 0 |

The rest of the leaked drop — `sl.common`, `sl.dlss`, `sl.pcl`, `sl.reflex`, `sl.interposer`,
`nvngx_dlss`, `nvngx_dlssnr` — stayed installed and caused no trouble.

### Fault B: `dxgi.dll+0x90092`, ReShade 6.8 only

`EXCEPTION_BREAKPOINT (0x80000003)`, ~20 ms after ReShade logs `Initialized.`, **five for five**.

| ReShade | image size | sessions | dxgi breakpoint |
|---|---|---|---|
| 6.3.3.1921 | `0x4CD000` | 18, back to 08-07 | 0 |
| 6.8.0.2155 | `0x59A000` | 5 | 5 |

Not a bad build in general: 6.8 installed as a `dxgi.dll` **proxy** in another game ran three
minutes and exited cleanly. It is specific to how 6.8 enters Cyberpunk — the OpenXR API layer path.
Untestable in isolation because the 6.3.3 binary was overwritten by the 6.8 installer with no
backup.

### Method note

Two things that made this tractable:

* A **negative control**. All 51 REDEngine dumps back to 2026-08-07 were scanned for the signature.
  It appears only after the swap. Combined with `find -newermt` over the game folder showing the
  DLSS/Streamline files as the *only* change since the last clean session, that is a controlled
  comparison, not a correlation.
* **Not trusting the first plausible story.** A NaN in `UnifixRender` looked like a lead and was
  dropped: it appears in logs from 08-14 onwards, up to 199 times a session, long before any of
  this.

---

## Part 2 — the ReShade addon host

### Why

`renodx-dlss5.addon64` drives `nvngx_dlssnr.dll`, a DLSS 5 NGX feature Cyberpunk 2.31 has no call
site for. That DLL sat in `bin\x64` unloaded across eight crash dumps because nothing in the process
knew it existed.

Reaching it through ReShade was not available: 6.8 kills the process in a second (fault B), and
6.3.3 predates the addon API version the addon asks for. So the plugin hosts it directly.

### The contract

An addon does not link against ReShade. It calls `K32EnumProcessModules`, takes the **first** module
exporting `ReShadeRegisterAddon`, and binds to it. Exporting these ten entry points is therefore
sufficient to be that module:

```
ReShadeRegisterAddon      ReShadeUnregisterAddon
ReShadeRegisterEvent      ReShadeUnregisterEvent
ReShadeRegisterOverlay    ReShadeUnregisterOverlay
ReShadeLogMessage         ReShadeGetConfigValue / ReShadeSetConfigValue
ReShadeGetImGuiFunctionTable
```

Three things the host has to get right, in the order the addon exercises them:

1. **`ReShadeRegisterAddon(module, api_version) -> bool`.** The gate. renodx-dlss5 asks for **18**.
2. **`ReShadeGetImGuiFunctionTable(imgui_version) -> const void*`.** Not optional, and returning
   null is *not* the safe choice — `reshade.hpp`'s `register_addon` returns false on a null table,
   so the addon aborts before doing anything. It gets a buffer of stubs.
3. **Events are recorded, and delivery is separate.** See below.

### Forwarding, and why refusing is not neutral

Exporting those symbols makes the plugin a candidate host for *every* addon in the process,
including ones happily using a real ReShade. The addon takes the first module that exports the
symbol and **does not fall back** if it refuses. So with hosting off, every call is forwarded to a
real ReShade if one is loaded; with hosting on, the host stands down if ReShade is present, so
nothing is loaded twice.

### Three bugs worth remembering

**A non-recursive mutex across `LoadLibrary`.** The addon binds from inside its own `DllMain`, on
the thread still inside our `LoadLibrary`. So `ReShadeRegisterAddon` re-enters a lock that thread
already holds. MSVC throws `system_error` out of the re-lock, the exception unwinds through the
addon's `DllMain`, and the loader reports `ERROR_DLL_INIT_FAILED (1114)` with no indication why.
The lock is `std::recursive_mutex` because that re-entry is legitimate.

**One wait where there were two.** NGX is in the process well before the game creates its D3D12
device — Streamline's interposer pulls it in early — so waiting on NGX alone reached `LoadLibrary`
with `CyberpunkVR_GetGameDevice()` still null and delivered no `init_device`. Loading keys off NGX
(the addon's detours want to be in place before the first DLSS create); delivery keys off the
device, separately.

**An ini rewrite per `SetConfigValue`.** During discovery the addon believed every widget changed on
every frame, so this was a full file write per frame. Debounced to 2 s, with the in-memory store
authoritative.

### Measure, don't guess

Every unknown was answered by making the host record what the addon asked for, rather than by
reading a header and hoping. Four rounds:

| unknown | method | answer |
|---|---|---|
| does it find us? | log before any lock or decision | yes, API 18 |
| which events? | record subscriptions, dispatch none | `0 init_device`, `1 destroy_device`, `74 present` |
| which `api::device` methods? | 128-slot vtable, each thunk records its index | **none** — it stores the pointer and never calls a method |
| which ImGui functions? | 512-slot table of recording stubs | 7 slots: 80, 103, 104, 111, 115, 129, 144 |

The vtable result is the load-bearing one: there is **no ReShade object model to reproduce**. The
addon takes the device handle and gets everything else from its own NGX and Streamline detours.

`init_device` alone was not enough — event 74 is `present`, and that is where it installs its hooks:

```
[addon] vtable::Hook(NVSDK_NGX_D3D12_CreateFeature   hooked ...)
[addon] vtable::Hook(NVSDK_NGX_D3D12_EvaluateFeature hooked ...)
[addon] DLSS5 Generic: D3D12 NGX hooks installed; inline DLSS contract capture armed
[addon] DLSS5 Generic: Streamline slEvaluateFeature hook installed; NGX/Streamline deduplication armed
[addon] DLSS5 Generic: signed DLSSNR 310.8.0 D3D12 runtime initialized
[addon] DLSS5 Generic: inline feature 18 evaluation succeeded (count=60, NR input 3072x3072
                       (guides 1024x1024), output 3072x3072 [native])
```

Present dispatch runs from `HookedPresent`, one line after `OverlayRender`, wrapped in SEH: a fault
disables dispatch for the session and names the last recorded slot, instead of being a hard crash on
every subsequent frame.

### Correction: use the exact API-18 table, not inferred stack shapes

The addon asks for `IMGUI_VERSION_NUM 19250`. ReShade's matching
`imgui_function_table_19250.hpp` is public in the exact ReShade submodule revision used by RenoDX,
so there is no reason to infer these entries. The actual mapping is:

```
slot  80  void Separator()
slot 103  void TextUnformatted(const char*, const char*)
slot 104  void TextV(const char*, va_list)
slot 111  bool Button(const char*, const ImVec2&)
slot 115  bool Checkbox(const char*, bool*)
slot 129  bool Combo(const char*, int*, const char* const[], int, int)
slot 144  bool SliderFloat(const char*, float*, float, float, const char*, flags)
```

The earlier `E184 u32=256` reading was just an argument-register/stack preview attached to a
no-argument separator; it was **not** an unlabelled upscaling control. Likewise
`Control-compatible color transfer` is text, not a checkbox. Every control of the same type shares
one slot, so the correct slot-115 forwarder exposes all of the addon's checkboxes, including its
explicit `Enable Upscaling`, and slot 144 exposes all of its float controls.

A second ABI error was more serious. ReShade API 18 declares:

```
ReShadeGetConfigValue(module, runtime, section, key, value, value_size)
ReShadeSetConfigValue(module, runtime, section, key, value)
```

The first host version omitted `module`, shifting every argument. That is why it persisted the
nonsense host key `RenoDX.DLSS5=NRPreset`: `RenoDX.DLSS5` was really the section and `NRPreset` the
key; the actual value never reached the host. The uncommitted follow-up uses the exact signatures
and exact ImGui slots.

### `overlay_stub_returns` is dangerous once anything is real

A widget returning `true` means *"the user changed this"*. With stub returns forced to 1, every
**unimplemented** slot claimed a change every frame, the addon committed whatever was in its stack
local, and rebuilt its NR feature — 11 rebuilds against 2 evaluation log lines, flip-flopping
`native` / `upscaling`, and the game crawling. Slot 80 is what that flip-flop tracks.

The two knobs are now mutually exclusive: enabling real widgets forces stub returns to 0.

An earlier explanation of that stall — a mis-signed `Checkbox` scribbling memory — was **wrong** and
is recorded here so it is not re-adopted. The claim that slot 80 selected native/upscaling was also
wrong: slot 80 is `Separator`. Stub-true discovery can still force actual bool-returning widget
paths (checkboxes, combos and sliders) to commit repeatedly, so it remains discovery-only.

---

## Part 3 — what DLSS 5 Neural Rendering actually does here, and what it costs

### It is not ray reconstruction

Ray Reconstruction replaces the engine's **denoiser** and needs the pre-denoise radiance signal,
which only the engine can provide — hence it being an in-game toggle (`DLSS_D`). This addon never
sees that buffer. It runs the DLSS 5 network over the finished frame using DLSS's own depth and
motion-vector guides.

Proof it is image-space: ray tracing was **off** throughout
(`"Gpu/RayTracing/Enabled":"false"`) and NR evaluated happily anyway.

### Two configurations

```
created inline NR resources 3072x3072 -> 3072x3072 (native)
created inline NR resources 1024x1024 -> 3072x3072 (upscaling)
```

* **native** — runs *after* DLSS has upscaled; a refinement pass, resolution unchanged.
* **upscaling** — the neural pass does the scaling itself from the render resolution, and DLSS SR's
  result is deduplicated (hence the `slEvaluateFeature` hook).

The addon's `Enable Upscaling` checkbox (another call through slot 115) chooses between those, not
whether the game upscales at all. From inside a generic addon that name is reasonable; in a game
that already has DLSS it reads backwards. The host must verify the addon's own status says both
`requested yes` and `active yes`; a colour change or a checked box is not proof that deduplication
replaced the normal DLSS pass.

### Cost, from the port's own pacing counters

| | in-world frame | fps |
|---|---|---|
| before any of this (08-17 session) | 16.4 – 18.9 ms | 53 – 61 |
| NR running, native mode | 22.2 – 25.6 ms | 39 – 45 |

Different sessions and different scenes, so treat the absolute numbers as indicative. The
within-session evidence is firmer: menu frames unchanged, world frames up by a consistent margin.

Menu/idle frames stay at 11.1 ms, which is the signature of a cost that only lands when the scene is
rendered. Roughly **+5 to +7 ms per frame** for a second neural network at 3072² per eye at 90 Hz,
on top of the DLSS upscale already being paid for.

Observed colour shift with settings changes is the addon's own tonemap path
(`created Control-equivalent soft-clip/sRGB/UpgradeToneMap codec`), not the neural pass and not
ReShade — ReShade is not in the process.

### Assessment

Native `3072²` is below the 45 FPS reprojection floor in ordinary gameplay (about 38.3 FPS median),
but it produced a clean Natural image without the low-resolution path's shimmer. Native `2560²`
was materially more promising at about 44–55 FPS while feature 18 remained active. This is still
not 90 Hz native cadence, but it can meet 45 Hz reprojection in some scenes.

The earlier 2026-08-30 native session also exposed a failure after repeated live setting changes:
the last real render measurement was 186.76 ms / 5.4 fps, then XR resubmitted a stale image through
107,998 cycles. Correct config ABI, zero-return stubs, and fixed settings avoid that rebuild thrash;
the evidence remains preserved under
`%LOCALAPPDATA%\CyberpunkVRPort\diagnostics\20260830-114131-dlss5-render-stall`.

The explicit low-resolution **upscaling** path is rejected. The model writes at its active network
resolution and the addon's own sampling path enlarges that populated region bilinearly. Ultra
Performance reached about 55–61 FPS but shimmered; Performance was slightly cleaner at about 30 FPS.
The viable visual path is ordinary DLSS SR followed by native NR.

The current NR blocker is lifecycle, not initial creation: active-NR menu/save transitions have
repeatedly produced a matched GPU-hang signature. A separate second-load CPU descriptor fault is
tracked independently and must not be attributed to feature 18 without matching evidence.

The installed addon is build 4.55 (`FileVersion 0.2026.0828.0517`, internal RenoDX DLSS5 Generic
v4.1.5, SHA-256 `9150097cdee2953cdc9894d2e5606ea5100e6c8f95fc7bb1b407328b4391a07a`).
Its source is not public; preserve this exact file unless the user explicitly chooses another build.

**What may be worth keeping is the host**, after an explicit addon allowlist, exact ABI, and a
fail-closed load-transition recovery path exist.

---

## Current state

### Files

| file | |
|---|---|
| `include/Addons/ReShadeAddonHost.hpp` | new — host ABI and status struct |
| `src/Addons/ReShadeAddonHost.cpp` | new — the host |
| `src/Main.cpp` | `+1` call at the end of `Load` |
| `src/Hooks/SwapChain.cpp` | `+1` call in `HookedPresent` for event 74 |
| `src/Overlay/OverlayPanels.cpp` | F10 panel under **DLSS 5** |

Cost on the normal path when nothing is armed: one relaxed atomic load per frame.

### `bin\x64\reshade-addons.ini`

```ini
[host]
enabled=1              ; current experimental session; host the addon next launch
dispatch_events=1      ; deliver init_device
dispatch_present=1     ; deliver event 74 (present)
draw_overlay=1         ; expose the addon's settings page inside F10
overlay_stub_returns=0 ; required: unimplemented widgets must never claim a change
overlay_widgets=1      ; exact ImGui 1.92.5 forwarding for the measured controls
```

### Live master switch

F10 exposes **Enable DLSS 5 Neural Rendering** once, in the parent DLSS 5 section. It is the closed
addon's real live feature-18 switch and does not require a game restart. The duplicate copy from the
addon's Image style page is suppressed. `[host] enabled=1` only makes the in-process host available
at startup; it is not presented as a second Neural Rendering switch.

### Preconditions if re-enabling

* ReShade's OpenXR layer off — `C:\ProgramData\ReShade\ReShade64_XR.json` renamed to `.off`.
  The host stands down if ReShade is loaded.
* `renodx-dlss5.addon64` and `nvngx_dlssnr.dll` both in `bin\x64` — the addon refuses without the
  latter beside it.
* `bin\x64\nvngx_dlssg.dll` **absent**, per Part 1.

---

## Open

* **Load-transition recovery** — after same-size `ResizeBuffers`, release stale inline resources and
  recreate MAIN/VRCAM feature 18 against the replacement guides. The addon's event-1 cleanup exists,
  but dispatching it at resize and safely re-arming hooks is untested.
* **Guide capture with hook chaining** — the addon replaces `slSetTag`/`slSetTagForFrame` pointers
  after startup. Future diagnostics must coexist with those detours rather than overwrite them.
* **Global codec synchronization** — MAIN and VRCAM feature allocations are distinct with zero
  pointer overlap, strongly weakening shared-history concerns, but one four-texture inline/codec set
  appears globally reused and its sequential synchronization is not proven.
* **ReShade 6.8 D3D12 proxy** — only the crashing OpenXR API-layer path was tested in Cyberpunk. A
  controlled `dxgi.dll` proxy with the OpenXR layer disabled may retire the custom ABI bridge, but
  cannot fix model cost or the closed addon's lifecycle.
* **`+0x1F51F5` second-eye load crash** — reproduced again on 2026-08-30 with the exact pre-DLSSNR
  signature. A paired log/dump narrows the final started node on the fault thread to
  `EndRenderTargetsGBuffer`; it remains open and is not a new DLSS5 crash family.
