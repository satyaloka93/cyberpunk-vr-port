---
type: Fix
title: Render FOV, cover versus span
description: Upstream sizes the render frustum to COVER a canted headset's panel, which spends about a sixth of the linear resolution on frustum the wearer cannot see. xr_fov_mode=1 asks for the de-canted span instead, trading one edge for sharpness.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Hooks/CameraFov.cpp
tags: [fov, image-quality, psvr2, quest3, openxr, stereo]
timestamp: 2026-08-26T11:00:00+09:00
---

# Which tree this describes

`upstream-0.1.3-psvr2`. **`xr_fov_mode` is this fork's addition and does not exist upstream or on
`psvr2-tweaks`** — on those trees cover sizing is unconditional and the only dial is `xr_force_fov`,
which upstream has always had. Cover sizing itself arrived upstream in PR #24; the pre-0.1.2
`psvr2-tweaks` layout predates it and sizes differently again.

# The problem upstream is solving, and what it costs

The engine renders **one symmetric frustum** per eye and derives the vertical from the render
aspect. What the runtime reports is **not** symmetric: `angleLeft` and `angleRight` differ, each eye
seeing further toward its own temple than toward the nose.

> **On the word "cant".** The codebase calls this asymmetry *cant* — `deCantedHFov`,
> `ComputeRuntimeFovCorrection`, `correctionYaw` — and this page follows that vocabulary because the
> code does. It is a name for the **measured frustum asymmetry**, not a claim about the physical
> optics. Asymmetric per-eye frusta are normal on nearly every headset and generally reflect the eye
> sitting off the lens axis and the nose occluding the inner field, rather than physically toed-in
> panels. Nothing here depends on the cause: the sizing math operates on the reported angles
> whatever produces them.

The submit layer recentres that frustum on the eye axis but **never rotates the pose to match the
asymmetry**. So a frustum sized to the panel's actual angular *span* leaves the outer edge — and on
some headsets the bottom — unrendered, which is the black border upstream fixed.

Their fix sizes to **cover**:

```
coverHFov = 2 * max(|angleLeft|, |angleRight|)
```

That renders the frustum *together with its own mirror image* and throws half the widening away.
The border goes, and so does a large fraction of the pixel density — this is the blur users report.

# Measured on this machine, both headsets

Taken from each headset's own `OpenXRManager[FOV]` line in `bin\x64\cyberpunkvrport.log`, not from
a table. The PSVR2 row reproduces verbatim as:

```
systemName="SteamVR/OpenXR : playstation_vr2"
raw left =(L=-61.500 R=43.446 U=53.040 D=-53.040)
    right=(L=-43.446 R=61.500 U=53.040 D=-53.040)
runtimeHFov=104.946  deCantedHFov=104.946  correctionYaw=9.027  correctionPitch=0.000
```

`correctionYaw` is half the left/right difference — `(61.500 - 43.446) / 2`. Anyone reproducing this
should read their own line rather than trusting the table, since these are per-headset **and** vary
with IPD.

| Headset | Eye frusta (L/R) | Cover | De-canted span | Linear px/deg lost |
|---|---|---|---|---|
| PSVR2 (SteamVR 2.17.7) | −61.500 / +43.446 | **123.000** | **104.946** | ~17% (24.98 → 29.26 px/deg at 3072 px) |
| Quest 3 (VirtualDesktopXR) | −54.000 / +40.000 | **108.000** | **94.000** | ~15% |

In both cases roughly **a third of the rendered solid angle falls outside the lens**.

# The switch

`xr_fov_mode` in `vrport.ini`:

* **`1` — span. Default.** Asks for the de-canted span
  (`GetCorrectedGameHorizontalFovDeg(ComputeRuntimeFovCorrection(lf, rf))`).
* **`0` — cover.** Restores upstream sizing exactly
  (`GetPanelCoveringHorizontalFovDeg(lf, rf, aspect)`).

Live-settable from the F10 overlay; `LiveControlsPoll.cpp` parses and persists it, and
`LauncherConfig.cpp` seeds `xr_fov_mode=1` into a fresh ini.

**It is ungated and self-adapting.** It was PSVR2-only while PSVR2 was the only headset measured,
then ungated once Quest 3 was measured too. That is safe because the number is derived from the
**connected headset's own reported frusta** rather than looked up by name — which also sidesteps the
naming trap in `kHeadsetFovDefaults`, where VDXR reports a Pico 4 as `"Oculus Quest2"`. No headset
name is consulted on this path at all.

# The cost is real and differs by headset

The submitted frustum stays symmetric, so whichever side the cant favours goes short:

* **PSVR2** is vertically symmetric (U = D = 53.040), so **only the outer horizontal edge** is
  exposed. Confirmed in play: clarity improved, no objectionable border.
* **Quest 3** is canted **down** as well (U = 44.000, D = −55.000, `correctionPitch = −5.5`), so at
  span the **bottom edge is ~5° short** while the top gains ~6°. Also confirmed in play as
  acceptable, but it is the headset more likely to want mode 0 or a middle value.

# Precedence, highest first

1. **`xr_force_fov`** — an absolute horizontal in degrees, honoured when `1.0 < forced < 170.0`.
   Not headset-aware; whatever is typed is used. `0` disables it. Use it for a **middle value**
   between span and cover when the exposed edge is unwelcome — e.g. PSVR2 at ~112.
2. **`xr_fov_mode`** — span or cover, both derived from the live frusta.
3. **`GetRuntimeHorizontalFovDeg()`** — fallback if the frusta are not yet readable.

`xr_force_fov=0.000` with `xr_fov_mode=1` means *no forced value; use span*. There is no "forced FOV"
hiding anywhere in that state — the 0 **is** the off switch.

The ini lives beside the plugin, at
`Cyberpunk 2077\red4ext\plugins\CyberpunkVR_Stereo\vrport.ini`.

# Recipe for someone on stock upstream

They do not need this build. Stock upstream already prints, ungated, on every launch:

```
OpenXRManager[FOV]: raw left=(...) right=(...) runtimeHFov=... deCantedHFov=104.946 ...
```

Setting `xr_force_fov` to their own logged `deCantedHFov` reproduces mode 1 exactly, and any value
between that and `2 * max(|angleLeft|, |angleRight|)` buys back a proportional share of the edge.

# Related

The overlay reads `g_engineHorizontalFovDeg` — what the engine *actually* renders after the
vertical solve — rather than the requested value, so overlay projection follows the mode
automatically. See `src/Overlay/OverlayProjection.cpp`.
