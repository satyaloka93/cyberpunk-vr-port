---
type: Fix
title: Overlay load-transition guard
description: Restore the full GPU drain only inside resource-churn windows, removing the DXGI_ERROR_DEVICE_HUNG faults that middle-ground pacing introduced without giving up its throughput.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Overlay/ImGuiOverlay.cpp
tags: [performance, dxgi, d3d12, frame-pacing, overlay, crash, device-hung]
timestamp: 2026-08-11T15:10:00+09:00
---

# Outcome

`kOverlayPacingMode = 3` in [ImGuiOverlay.cpp](../../src/Overlay/ImGuiOverlay.cpp):
[middle-ground pacing](overlay-middle-ground-pacing.md) during normal rendering, and Mode 0's
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

| Site | Signal |
|---|---|
| [VrCore.cpp](../../src/Core/VrCore.cpp) | save-load transition |
| [ImGuiOverlay.cpp](../../src/Overlay/ImGuiOverlay.cpp) | swapchain invalidate |
| [SyncStereo.cpp](../../src/Stereo/SyncStereo.cpp) | VRCAM component re-bind |
| [SyncStereo.cpp](../../src/Stereo/SyncStereo.cpp) | sight PSO substitution, both graphics-desc and stream-desc paths |

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

Every run self-identifies, so no log can be attributed to the wrong configuration:

```text
[OVERLAY-PACING] ... injectedDxgiWait=0 mode=previous-overlay-fence+load-guard
[PERF] present=... mode=previous-overlay-fence+load-guard ...
Overlay load guard armed for 5000 ms (sight PSO substitution).
Overlay load guard engaged -- full drain.
Overlay load guard released -- back to previous-overlay pacing.
```

The guard's drain is bounded at 1000 ms rather than the original `INFINITE`, so a genuinely
dead GPU cannot wedge a Present worker permanently. In a healthy frame the two are
equivalent; `Overlay full drain did not complete` means the bound was hit.

# Regression checks

1. Startup reports `mode=previous-overlay-fence+load-guard`.
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
