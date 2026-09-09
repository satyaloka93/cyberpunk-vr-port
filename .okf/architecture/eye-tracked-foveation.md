---
type: architecture
title: Eye-tracked NR foveation
description: How the port gets eye gaze from OpenXR and uses it to place the DLSS-NR foveation region.
tags: [openxr, eye-tracking, foveation, dlssnr, psvr2]
timestamp: 2026-09-09
---

# Eye-tracked NR foveation

The DLSS-NR foveation region follows the eye instead of sitting at a fixed centre. Gaze comes
straight from the OpenXR runtime — **the port has no dependency on any third-party eye-tracking
bridge**, and none of this involves the Cheeky layer or
`%LOCALAPPDATA%\CheekyFoveatedDLSS`.

Related: [DLSS5 neural rendering](dlss5-neural-rendering.md).

## Proven on this hardware

PSVR2 through the PSVR2Toolkit SteamVR driver, verified with `tools/xr_probe` (`xrprobe.exe`):

```
Runtime : SteamVR/OpenXR v2.17.8
System  : SteamVR/OpenXR : playstation_vr2
          XR_EXT_eye_gaze_interaction (v2) advertised
          supportsEyeGazeInteraction = TRUE
Verdict : WORKING -- 61/598 frames tracked, flags 0xF (VALID|TRACKED), quaternion live
```

### The trap that made it look broken

**Action data only exists while the session is FOCUSED.** `xrSyncActions` returns
`XR_SESSION_NOT_FOCUSED` outside it and every action stays inactive, so gaze reads `flags=0x0` —
identical to a headset with no eye tracker.

The first probe run stopped after 60 frames, having only reached `VISIBLE (4)`, and its verdict
blamed the driver. The corrected run is unambiguous: **537 frames at `0x0`, focus arrives at frame
538, and gaze is tracked on every frame after.** Tracked frames equalled focused frames exactly.

`xrprobe` now refuses to give a verdict without focus and separates four outcomes: not advertised,
never bound, focused-but-never-tracked, working. Anything that reports "no eye tracking" without
confirming FOCUSED has not tested it.

## How the port gets gaze

| step | where | note |
|---|---|---|
| request extension | `OpenXRManager.cpp` instance creation | only when advertised — naming an unsupported extension fails `xrCreateInstance` outright and costs the whole VR session |
| create action | `bindings_done:` in `OpenXRManager.cpp` | **in the existing `m_actionSet`**, see below |
| suggest binding | same | profile `/interaction_profiles/ext/eye_gaze_interaction`, path `/user/eyes_ext/input/gaze_ext/pose` |
| create action space | after `xrAttachSessionActionSets` | a space made from an unattached action locates to nothing |
| sample | `OpenXRFrameLoop.cpp`, after `xrSyncActions` | located in **VIEW** space |

### Why the action lives in the hands' action set

`xrAttachSessionActionSets` may be called **once per session**, and the hands already claim that
call. A separate gaze action set would attach nothing and be indistinguishable from a headset with
no eye tracker. One set covers hands and eyes together.

### Tangent space, not pixels

The pose is located in VIEW space, so it is head-relative without subtracting a head pose. The
forward vector is rotated out of the quaternion and divided by z:

```cpp
const float fx = -2.0f * (q.x * q.z + q.w * q.y);
const float fy = -2.0f * (q.y * q.z - q.w * q.x);
const float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
const float tanX = fx / -fz, tanY = fy / -fz;
```

That is the same space the projection FOV tangents live in, so it maps onto the eye image without
knowing the resolution at the point of measurement.

**Both eyes take the same world direction.** That is what keeps the two regions describing one
place. The pre-existing ±4% per-eye shift stays as the base and gaze displaces from there.

## The deadband, and why smoothing was not enough

`ComputeNrRegion` snaps the region origin to 8 pixels (`NrAlign8Down`). Exponential smoothing is
always approaching and never arriving, so a smoothed centre **creeps**, and a creeping centre keeps
stepping across those 8-pixel boundaries. DLSS-NR is temporal: every step is a discontinuity in its
history, and a train of them reads as **shimmer**. The first build did exactly that.

The fix is a deadband, not more smoothing. The committed centre holds completely still until gaze
leaves a threshold, then moves once — which matches how eyes behave: long fixations separated by
saccades, with vision suppressed during the saccade that moves the region.

| value | result |
|---|---|
| `0.06` tangent (~3.4°) | 811 relocations in a session — churn, residual shimmer |
| `0.105` tangent (~6°) | current |

`[Eye]` log lines carry a `moves=` counter. A steady gaze should barely increment it; a racing
count means the deadband is too tight for the tracker's noise.

Staleness: if gaze stops being TRACKED the region reverts to the fixed centre after 500 ms rather
than freezing where it last looked (`kFovealGazeStale`).

## What it is worth

Measured, same scene class, gaze tracking active, 3072² output:

| configuration | rate |
|---|---|
| NR off | 90 fps, per-cycle work ~1 ms, `perDisplay 1.00` |
| NR on, foveation hook **failed** (full frame) | ~30 fps |
| NR on, 50% preset | ~45–48 fps, locked 45 Hz |
| NR on, 35% preset, gaze-tracked | ~50 fps, `perDisplay` 1.13–1.18 |

Shrinking 50% → 35% bought only ~2–5 fps, so **NR coverage is no longer the binding constraint**.
Shrinking further will not help; the remaining cost is elsewhere.

## Limits, stated

- **The NR region is a rectangle and cannot be round.** DLSS-NR takes a rectangular subrect per
  plane (`Color`, `Depth`, `MVec`, `Output`) through the NGX parameter block. Rounded edges would
  have to be a feathered composite, and the port has only a **prehook** — the feature-18 stub ends
  in `jmp rax`, so there is no point after evaluation at which to blend. Adding one means
  restructuring the detour in the file that owns the DLSSNR device-hang family.
- **The tangent→pixel mapping is linear** (`kFovealHalfTan = 1.84f`), not a per-eye projection
  inverse. Y is symmetric and X asymmetry is already approximated by the ±4% shift, so an exact
  mapping would add a live-FOV dependency for a correction smaller than the smoothing lag. Revisit
  if the region visibly trails toward the edges.
- **Gaze cannot change the fovea's DLSS quality mode.** That requires owning a private DLSS
  feature, which the port does not — it rewrites the game's subrects in place. See the note on
  Cheeky's `CenterForceDlaa` in [DLSS5 neural rendering](dlss5-neural-rendering.md).

## Do not confuse with Cheeky's eye tracking

Two independent paths, and mixing them up cost a debugging session:

| | ours | Cheeky's |
|---|---|---|
| source | OpenXR runtime directly | `CheekyOpenXRLayer.dll` implicit layer |
| needs a registered layer | no | yes |
| affected by `CHEEKY_OPENXR_LAYER_DISABLE` | **no** | yes — the loader skips the layer entirely |

Our gaze was verified working *while* that variable was set to 1. It names Cheeky's manifest and
has no bearing on the port.
