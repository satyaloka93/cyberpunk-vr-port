---
type: Fix
title: Overlay load-transition guard
description: Restore the full GPU drain only inside resource-churn windows, removing the DXGI_ERROR_DEVICE_HUNG faults that middle-ground pacing introduced without giving up its throughput.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Overlay/ImGuiOverlay.cpp
tags: [performance, dxgi, d3d12, frame-pacing, overlay, crash, device-hung]
timestamp: 2026-08-24T11:20:00+09:00
---

# Which tree this describes

The fix was developed on `psvr2-tweaks` against the pre-0.1.2 layout (`src/vr/{core,overlay,stereo}/`)
and re-implemented by upstream when 0.1.2 collapsed the port into one tree. **Both forms are live in
different branches and the symbol names differ** — read the row for the branch you are on:

| | `psvr2-tweaks` (pre-0.1.2 layout) | `upstream-0.1.3-psvr2` (current) |
|---|---|---|
| Mode selector | `constexpr int kOverlayPacingMode = 3` | `CyberpunkVR_OverlayPacing` — an exported `int32_t` defaulting to `3`, so it is **live-tunable** instead of compile-time |
| Overlay file | `src/vr/overlay/imgui_overlay.cpp` | [src/Overlay/ImGuiOverlay.cpp](../../src/Overlay/ImGuiOverlay.cpp) |
| Self-identifies in the log | yes — an `[OVERLAY-PACING]` line plus `mode=` in every `[PERF]` | **no** — replaced by exported counters, see Diagnostics |
| Guard drain bound | 1000 ms | **2000 ms** |

Upstream's header comment credits the measurements below to `satyaloka93, psvr2-tweaks 980f4406 +
ce8d36f1`, so the two implementations agree on the reasoning even where they differ on the spelling.

# Outcome

Mode `3`: [middle-ground pacing](overlay-middle-ground-pacing.md) during normal rendering, and Mode 0's
full queue drain for a bounded 5-second window after any event that signals the game is
churning render resources. Each event refreshes the deadline, so a transition that emits a
stream of events holds the guard open for its whole duration.

Loads are rare and already slow, so the throughput cost is confined to frames nobody is
looking at. Measured duty cycle is under 10% of runtime, and cadence is unchanged:

| Configuration | Median present | Outcome |
|---|---|---|
| Mode 0 — drain every frame | 43.8 fps | stable |
| Mode 2 — middle-ground pacing | 62.2 fps | eight GPU hangs |
| **Mode 3 — pacing plus guard** | **70.3 fps** | **stable** |

# The failure it removes

Eight `DXGI_ERROR_DEVICE_HUNG` (`0x887a0006`) faults, every one with the same shape: the
engine asserts in `gpuApiDX12Error.cpp(42)`, and the game's breadcrumb log shows one command
list *In progress* stalled at the `FinalFlushBarriers` marker with all later work *Not
started*. The stalled pass varies — `Lighting`, `Hair_Opaque`, `HologramDepth_and_Distortion`
— but the marker never does, which points at resource-state transitions rather than any
pass's shader work.

Where the plugin log survived, the overlay's bounded wait names the exact frame the GPU died
on, because its fence sits behind the overlay draw on Cyberpunk's shared Present queue and
cannot complete while that queue is stuck:

```text
Overlay fence wait timed out for previous-overlay throttle (target=4790 completed=4789)
   -> breadcrumbs stall in frame 4790
```

That line is the overlay **observing** the hang, not causing it at that instant.

# What the A/B established

Both arms were built from one source differing only in the pacing constant, and both were run
on a fresh boot to keep accumulated driver state out of the comparison.

| Arm | Survived | Loads | Result |
|---|---|---|---|
| Mode 0 | ~6 min, 33,000 XR cycles | 4 | clean |
| Mode 2 | 87 s | 1 | hang |

Removing the drain is therefore the variable. Two other hypotheses were tested and dropped:

- **Concurrent Present workers corrupting backbuffer barriers.** Instrumented directly: the
  overlay-record → real-Present window never contained more than one worker, and the same
  backbuffer was never recorded twice without an intervening Present. Cyberpunk serialises
  Present itself despite presenting from sixteen registered threads.
- **A DLSS version mismatch** (super-resolution 310.7 against 310.1 frame-generation and
  ray-reconstruction). Reverting to the coherent stock 310.1 set did not stop the hangs. It
  may shorten time-to-failure; it is not necessary for it.

# Guard triggers

Chosen by measuring candidate signal rates in captured logs, not by reasoning about which
sounded load-like.

All four call `OverlayArmLoadGuard(reason)`. The sites moved in the 0.1.2 restructure, so they are
listed per branch — the `psvr2-tweaks` paths do not exist in the current tree and vice versa:

| Signal | `upstream-0.1.3-psvr2` site | `psvr2-tweaks` site |
|---|---|---|
| save-load transition | [LiveControlsPoll.cpp](../../src/Core/LiveControlsPoll.cpp) | `src/vr/core/vr_core.cpp` |
| swapchain invalidate | [ImGuiOverlay.cpp](../../src/Overlay/ImGuiOverlay.cpp) | `src/vr/overlay/imgui_overlay.cpp` |
| sight PSO substitution, both graphics-desc and stream-desc paths | [DeviceHooks.cpp](../../src/Stereo/DeviceHooks.cpp) | `src/vr/stereo/sync_stereo.cpp` |
| VRCAM component re-bind | [FrameGraph.cpp](../../src/Stereo/FrameGraph.cpp) | `src/vr/stereo/sync_stereo.cpp` |

**Regression, found 2026-08-24 and fixed the same day.** The `88bd4ef` port to 0.1.3 carried the
guard mechanism across but wired only the first two triggers. The sight-PSO and VRCAM-re-bind arms
— the two added *after the eighth hang, on direct evidence* — were silently dropped, so the branch
ran for a fortnight on half a guard. Two fossils proved it was an accident rather than a decision:
`FrameGraph.cpp` still carried `#include "Overlay/ImGuiOverlay.hpp"   // OverlayArmLoadGuard` with
no call anywhere in the file, and the "COUNTED, NOT LOGGED" note in `OverlayArmLoadGuard` still
asserts *"One caller is the VRCAM component re-bind"* — describing a caller that did not exist.
Anyone porting this subsystem forward again should diff the `OverlayArmLoadGuard(` call sites first;
the count is the check, and it is four.

The save-load signal alone is insufficient: it fires only for respawn-type loads and only
*after* the load completes, so menu loads of a different save were never covered.

PSO substitution was added after the eighth hang, on direct evidence. Pipeline-state creation
is heavyweight D3D12 work arriving in tight bursts, and in the crashed session one burst sat
inside a guard window and passed while the next was unguarded and hung:

```text
crashed:  PSO burst 469-474      guard armed at 475   -> survived
          PSO burst 1877-1880    unguarded            -> hang at 1881
```

## Rejected triggers

Both would have collapsed Mode 3 into Mode 0 while appearing to work:

| Candidate | Measured rate | Verdict |
|---|---|---|
| depth-gate `menu=1` | true in 59 of 103 sampled lines | over half of runtime |
| `[PERF] vrcam=0.0` | 18–33% of samples across seven sessions | too frequent |

Any new trigger must be measured against captured logs before it is wired in. A trigger that
fires often enough to matter costs roughly a third of the frame rate.

# Diagnostics

**On `psvr2-tweaks`** every run self-identifies, so no log can be attributed to the wrong
configuration:

```text
[OVERLAY-PACING] ... injectedDxgiWait=0 mode=previous-overlay-fence+load-guard
[PERF] present=... mode=previous-overlay-fence+load-guard ...
Overlay load guard armed for 5000 ms (sight PSO substitution).
```

**On `upstream-0.1.3-psvr2` none of those three lines exist.** There is no startup mode line and no
`mode=` field in `[PERF]`; individual arms are deliberately counted rather than logged, because the
VRCAM re-bind can fire often enough to flood the log. Only the window edges appear:

```text
Overlay load guard engaged -- full drain.
Overlay load guard released -- back to previous-overlay pacing.
Overlay fence wait timed out (load guard, 2000 ms) -- continuing.
```

Mode and arm counts are read from exported symbols instead (x64dbg, or any DLL export reader):

| Symbol | Meaning |
|---|---|
| `CyberpunkVR_OverlayPacing` | live mode: 0 drain-every-frame, 2 pacing with no guard, 3 pacing+guard |
| `CyberpunkVR_DebugOverlayGuardArms` | total arms. Climbing while `released` never appears = guard permanently open, pacing buying nothing |
| `CyberpunkVR_DebugOverlayDrains` | full drains performed |
| `CyberpunkVR_DebugOverlayWaitTimeouts` | bounded waits that expired |
| `CyberpunkVR_DebugOverlayWaitMs` | duration of the most recent wait |

The fence-timeout line is capped at **four occurrences per session** in code, so its absence late in
a long log proves nothing. The guard's drain is bounded — 1000 ms on `psvr2-tweaks`, 2000 ms here —
rather than the original `INFINITE`, so a genuinely dead GPU cannot wedge a Present worker
permanently.

# Regression checks

1. Confirm the mode. On `psvr2-tweaks`, startup reports `mode=previous-overlay-fence+load-guard`.
   On `upstream-0.1.3-psvr2` there is no such line — read `CyberpunkVR_OverlayPacing` and expect `3`,
   then confirm `OverlayArmLoadGuard(` still has **four** call sites in the tree.
2. Median present cadence stays near 62–70 fps. **A median near 44 means a trigger is firing
   far more often than measured** — count the armings by reason before changing anything else.
3. Zero `Overlay fence wait timed out` lines across a session. All eight hangs logged exactly
   one immediately before dying.
4. Run at least 6 minutes with 4 or more loads, including a death respawn and a menu load of a
   different save. Failures clustered at 64–192 s but one took roughly 9 minutes.
5. Stop on any new `DXGI_ERROR_DEVICE_HUNG`, device removal, or driver reset.

# Known limitations

The guard mitigates without explaining the mechanism. The remaining candidate is that Mode 0's
drain also serialised the plugin's *other* GPU work — `OverlayRender` runs before the OpenXR
capture and the inline XR frame pump in `HookedPresent`, so under Mode 0 every overlay
submission completed before that work was issued, and under Mode 2 they overlap with in-flight
game work. Untested.

A separate `EXCEPTION_ACCESS_VIOLATION` inside `D3D12Core.dll` (null + `0xF0`) occurred once
during a menu load, at a VRCAM component re-bind that was **not** inside a guard window. It is
a different failure from the hang — no device removal, no breadcrumbs — and has not recurred.
That crash class also predates this work.

Related: [middle-ground frame pacing](overlay-middle-ground-pacing.md),
[ray-tracing stereo limitations](../architecture/ray-tracing-stereo-limitations.md).
