# WORKLOG — OFXR Bridge + Cyberpunk VR port

Shared between agents. Volatile working state; what is *established* belongs in
[`.okf/`](.okf/index.md). Last updated **2026-09-08**.

Companion clone: **`../OFXR-Bridge`** (tig3rmast3r/OFXR-Bridge, tag `0.2.0` = internal V068).
This tree: `cyberpunk-vr-port-upstream-psvr2`, branch **`bd-camera-port`**, remote **`fork`**
(`satyaloka93/cyberpunk-vr-port`). This is the tree the owner actually runs — the sibling
`cyberpunk-vr-port` on `psvr2-tweaks` builds a 1.3 MB DLL against this one's 3.0 MB. Check the
size before deploying anything.

---

## What we are trying to do

Run OFXR Bridge (OpenXR frame generation) on top of the Cyberpunk VR port. Two problems, one
closed and one open.

| # | Problem | State |
|---|---|---|
| 1 | Game would not launch at all | **Closed** — ReShade's OpenXR layer, fixed in `308ff68` |
| 2 | Flashing artifacts with OFXR armed | **Open** — cause identified, one measurement outstanding |

---

## Problem 1 — launch crash (CLOSED)

### Symptom
Log stopped mid-startup. **No** WER `Application Error`, **no** TDR, **no** DRED dump, **no**
minidump. Attaching procdump made it launch every time — which reads as a race and is not one.

### Cause
`STATUS_BREAKPOINT` — a compiled-in `int 3` — at `dxgi.dll+0x90092`. The stack, resolved from the
PDB:

```
OpenXRManager::SubmitThreadMain   OpenXRManager.cpp:559
  OpenXRManager::FrameThreadMain  OpenXRFrameLoop.cpp:2726
    xrEndFrame
      vrclient_x64.dll  x8        SteamVR's OpenXR runtime
        ReShade64.dll
          d3d11.dll  x2
            ReShade64.dll  x3
              dxgi.dll+0x90092 -> int 3
```

On the first `xrEndFrame` the SteamVR runtime builds its D3D11-on-12 compositor interop.
ReShade's XR layer intercepts that **and** the `CreateDXGIFactory2` d3d11 makes internally
(logged with `Flags = 0x80000000`, not a defined `DXGI_CREATE_FACTORY_*` value), and DXGI breaks.
**Nothing handles a breakpoint with no debugger attached**, so the process ended silently. That is
the entire "it works under procdump" mystery — the debugger absorbed it.

ReShade's own log then shows it *skipping* the session (`without a proxy Direct3D 12 device`), so
it was doing nothing for us while breaking the launch.

### Fix
`src/Core/XrLayerOverrides.cpp` sets ReShade's own `disable_environment` name
(`DISABLE_XR_APILAYER_reshade_1`) **in this process only**, before `xrCreateInstance`. The
registry switches for an implicit layer are global and taking ReShade from every other title was
explicitly rejected by the owner. Configurable:

```ini
xr_disable_api_layers=DISABLE_XR_APILAYER_reshade_1   ; absent = this default; empty = disable none
```

Other layer disable names, for reference:
`CHEEKY_OPENXR_LAYER_DISABLE`, `XRFG_DISABLE_OFXR_BRIDGE`.

`src/Core/CrashDump.cpp` is what found it and stays in: vectored handler ahead of the engine's
top-level filter, exit hooks on `RtlExitUserProcess`/`TerminateProcess`, per-distinct-code
exception census, module+RVA resolution, and a breakpoint step-over kept as a fallback
(`crash_dump_continue_breakpoint=0` disables).

### Verified
`[XR] api-layer override: DISABLE_XR_APILAYER_reshade_1 set for this process only` in the log, no
`[CRASH] breakpoint` lines, game launches and runs.

---

## Problem 2 — flashing/tearing (CAUSE FOUND, corrected 2026-09-08)

**The cost is OFXR's, not ours.** Measured with OFXR's registration deleted, gameplay, same
machine, same scene class:

```
[xrcycle] 90 cycles | wait 10.15 ms mean | work 0.99 mean, 3.1 peak | LATE 0 | period 11.11 ms
[xrrate]  presents 90/s | cycles 90/s | submits 90/s | 90 Hz | perDisplay 1.00 perPresent 1.00
[xrcap]   captures 90 ok | skipped: all 0 | fence waits 0, mean 0.00 ms
```

Our per-cycle work is **0.69-1.04 ms**, zero fence waits, zero late cycles, cadence locked 1:1 to
the display. fpsVR agrees: 90 fps, 1.6 ms. The capture path costs nothing.

With OFXR armed the same loop showed 13-25 ms per cycle, 23-41 late cycles a second, and a display
period flapping 90 -> 45 -> 22.5 Hz.

### The error that produced the wrong answer, twice

The first reading compared MENUS against GAMEPLAY in a session with OFXR armed, saw ~1 ms against
13-25 ms, and concluded our gameplay work was heavy. The confound: **OFXR generates nothing in
menus** (quad layer, nothing to pair) and generates in gameplay. So the contrast was never
menu-vs-gameplay -- it was OFXR-idle vs OFXR-working. Two variables moving together, and the wrong
one got the blame.

The same shape as the earlier `no_projection_views` mistake: a cumulative or confounded
comparison read as if it isolated one thing. **Before attributing a cost, remove the other
variable and re-measure.** Deleting the OFXR registry value takes one command and would have
settled this at the start.

### What this means

The port does not need frame generation on this hardware -- it already holds 90 Hz with ~1 ms of
overhead and perfect pacing. OFXR is for titles that cannot reach display rate. Here it costs
13-25 ms a cycle, destabilises the cadence, and has hung the game once (Problem 3).

Still unexplained, low priority: brief settling windows at session start show `work 7.19 mean,
LATE 62` before dropping to ~1 ms and LATE 0, and one window logged `presents 24.1/s` with
`perPresent 3.72` while the loop held 90 -- the game's own render rate, not ours.

## Problem 3 — hang then self-termination (OFXR, 2026-09-08)

Opening the menu killed the game. Full chain, from three logs lined up by wall clock:

```
15:05:45.779  MenuMode 1 -> 0                    (menu closing; OFXR starts generating again)
15:05:46.352  OFXR: downstream_first_end_frame phase=B ... and NO phase=E. It never returns.
15:05:46.425  Overlay fence wait timed out (overlay frame ownership, 100 ms)
15:05:46.473  [xrcycle] work 7.38 ms mean, 562.1 ms PEAK
15:05:47.767  presents 0.8/s | xr cycles 0.0/s   -- loop stopped, blocked in xrEndFrame
   (23 seconds of nothing)
15:06:10.505  int 3 at Cyberpunk2077.exe+0x2A43F4B, then the SAME stack calls
              TerminateProcess(self, 0x00000001) from Cyberpunk2077.exe+0x29CEA69
```

**OFXR hung inside its own downstream submit** with a generated pair ready
(`generation_prepare result=0 b=2` = kind `pair`). The app thread blocked there, our overlay fence
degraded as designed rather than deadlocking, and the engine's watchdog eventually asserted and
killed the process. The `int 3` is the engine's assert-then-die path, **not** the ReShade
breakpoint from Problem 1 -- stepping over it changed nothing, the same stack terminated anyway.

This is upstream's bug, not ours. Suspect `enqueue_presenter_pair` followed by
`wait_for_presenter_capacity(state, 2)` in `src/layer/openxr_layer.cpp`: if the presenter thread is
itself blocked in the runtime, the application thread waits on capacity that never frees.

The flight log for it is
`RuntimeLayer/v068/ofxr-bridge-flight-20260908-150447-pid27124.log` -- worth attaching to an
upstream issue as-is.

## OFXR facts worth not re-deriving

- **Two mappings, not one.** Cyberpunk creates one swapchain **per eye**
  (`OpenXRCapture.cpp:1205`, `m_eyeSwapchains[eye].handle`, 3072×3072 each), so
  `build_projection_resource_mappings` — which dedupes by `subImage.swapchain` — returns 2. Every
  UEVR title measured returns 1 (both eyes in one swapchain). OFXR has **no projection-mapping
  tests at all**, so this layout is untested upstream. This correlated with the early crashes and
  that correlation was a **red herring** — the crash was ReShade. Do not re-assert it.
- **`XOffset` is mirrored per eye** and force-zeroed unless both eye roles latch. Not currently in
  play for Cyberpunk.
- OFXR's tray is a registry editor with an icon: arming writes an implicit-layer manifest +
  registry value, disarming removes them. It does **not** load the layer — the OpenXR loader does,
  in-process. A stale `manual-*.json` registration therefore survives closing the tray, and the
  tray's `disarm_bridge()` returns early when `state.armed` is false, so it cannot clean a
  registration made by an earlier tray session.
- OFXR runtime is SteamVR (`steamxr_win64.json`); graphics binding is D3D12 (`1000028000`) in
  every session logged, working and failing alike.
- Flight logs: `%LOCALAPPDATA%\OFXR Bridge\RuntimeLayer\v0NN\ofxr-bridge-flight-*.log`, rotated at
  32 MB (`max_file_mb`), so a big one has no header.

---

## Standing traps

1. Deploy with the game **closed**, then `md5sum` both ends.
2. **Build from `cyberpunk-vr-port-upstream-psvr2`**, not the sibling tree. 3.0 MB vs 1.3 MB.
3. Logs append or truncate per run — check mtime before attributing a line to your run.
4. ReShade and OFXR both rewrite their config on exit; edits made while running are lost.
5. `setx` only reaches processes started afterwards that get the environment broadcast; Steam, if
   already running, hands the game a stale environment. Registry or in-process is reliable.
6. A stopping point in a log is not a fault site. `D3D11On12CreateDevice` was read as one and
   appears in healthy runs too.
7. Don't push to `origin` (dariulone) or `ipower`. `fork` is the owner's.

## Open items

- [ ] Run once and read `[xrloop]`; resolve the ends-per-cycle discrepancy.
- [ ] Then decide on re-submitting the last projection layer instead of the empty end.
- [ ] Confirm whether two frame loops are pumping (`Inline frame pump` + `VR submit thread`).
- [ ] Consider an upstream OFXR issue for the two-swapchain layout — untested, not yet shown broken.
- [ ] `DebugGate.cpp` change is uncommitted.
