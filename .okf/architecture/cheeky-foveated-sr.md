---
type: architecture
title: Foveated super resolution via CheekyFoveatedDLSS
description: How the port runs Cheeky's gaze-driven DLSS foveation alongside its own NR foveation — what it does, what it costs, and every file, key and setting the arrangement needs.
tags: [dlss, foveation, cheeky, reshade, streamline, eye-tracking, psvr2]
timestamp: 2026-09-09
---

# Foveated super resolution via CheekyFoveatedDLSS

Two different bills, two different foveators:

| | foveates | owned by | drives it |
|---|---|---|---|
| **NR** — denoiser cost | DLSS-NR feature 18 | **the port** | our own OpenXR gaze |
| **SR** — upscaler/shading cost | the game's DLSS | **Cheeky addon** | its own gaze bridge |

They are complementary and target different NGX features. Gaze plumbing for each is separate —
see [two eye-tracking paths](eye-tracking-two-paths.md).

## Why the port cannot do this itself

Our NR foveation works by **rewriting the game's NGX subrects in place**, so NR renders into a
smaller region. That technique cannot change the fovea's DLSS *quality mode*, because there is only
one feature and one mode per evaluation.

Cheeky can, because it **creates its own private DLSS feature**. Owning a feature is what allows
`PerfQualityValue` and `DLSS.Hint.Render.Preset.*` to be set independently of the game's. That is
the whole architectural difference, and it is why `CenterForceDlaa` is possible there and not here.

Cyberpunk reaches DLSS through **Streamline**, not raw NGX (`sl.interposer.dll` proxies it), so
Cheeky hooks Streamline: `slEvaluateFeature`, `slSetTag`, `slSetTagForFrame`, `slSetConstants`,
`slGetFeatureFunction`, plus IAT patches on `Cyberpunk2077.exe`.

## What the technique actually is

Not "DLAA in the centre, nothing outside". It is **DLAA everywhere at two scales**:

* **fovea** — private DLSS feature over the gaze rect, quality mode forced to DLAA (1:1), optionally
  supersampled: `CenterSupersampling` enlarges its output then area-downsamples, so 1.4 renders
  ~2x the pixels of 1.0 before filtering down. This is the only step that recovers real detail,
  because DLSS is temporal and accumulates at the higher output resolution.
* **periphery** — DLAA at `PeripheralDlaaScale` (e.g. 0.64), i.e. downsampled first and
  reconstructed back up. Still proper reconstruction, just from fewer samples.

An earlier reading of this as "the centre is rendered denser" was wrong: the engine renders **one
resolution per frame**, so both regions read the same render. What differs is the reconstruction.

## Measured

Same scene class, gaze active, 3072x3072 output:

| configuration | rate |
|---|---|
| nothing on | 90 fps |
| our NR foveation, 35% gaze-tracked | ~50 fps, locked **45 Hz** |
| **Cheeky foveated SR, NR off** | **90 fps, locked 90 Hz, `perDisplay 1.00`, `LATE 0`, per-cycle work ~0.9 ms** |

Foveated SR is effectively free here. NR remains the expensive one.

## The arrangement, exactly

Cheeky needs **real ReShade** — our addon host cannot serve it (eight events against the host's two,
API 20 against the host's API-18 argument shapes; the one dispatched event faults as
`ACCESS_VIOLATION in D3D12Core.dll reading FFFFFFFFFFFFFFFF`). See
[two eye-tracking paths](eye-tracking-two-paths.md) for the detail.

### Files in `bin\x64\`

| file | note |
|---|---|
| `dxgi.dll` | real ReShade. Keep the prior state as `dxgi.dll.disabled-reshade-6.8.0.2155` |
| `CheekyFoveatedDLSS.addon64` | the addon |
| `ReShade.ini` | settings live here, section `[CheekyFoveatedDLSS]` |
| `ReShadePreset.ini`, `reshade-shaders\` | ReShade expects both |
| `renodx-dlss5.addon64` | NR. Rename `.off` to take NR out of a test |
| `reshade-addons.ini` | set `[host] enabled=0` so our host does not also load addons |

**Two hosts must not both load the same addon.** With real ReShade present, disable ours.

### Keys and variables

Covered in full in [two eye-tracking paths](eye-tracking-two-paths.md). The port-side one:

```ini
; vrport.ini
xr_enable_api_layers=XR_APILAYER_CHEEKY_foveated_dlss
```

### Hotkeys — verified across all four input layers

ReShade's overlay defaults to **Home (36)**, which collides with CET. Checked and free: **F12 (123)**.

| key | claimed by |
|---|---|
| Home | ReShade default **and** CET |
| F10, Insert | the port's overlay |
| F11 | `r6\input\CyberpunkVRPort_ScannerHud.xml` |
| End | `r6\input\HUDitor.xml` |
| F9 | `r6\config\inputUserMappings.xml` |
| **F12, Pause, ScrollLock** | free |

```ini
; ReShade.ini
KeyOverlay=123,0,0,0
```

## Settings that matter

```ini
[CheekyFoveatedDLSS]
Enabled=1
GazeEnabled=1          ; their shipped snapshot has 0 -- eye tracking off
NrEnabled=0            ; the PORT owns NR; leave Cheeky's off
CenterMode=3           ; gaze mode
CenterForceDlaa=1      ; fovea quality mode -> DLAA
CenterPreset=13
CenterSupersampling=1.4
Width=0.36  Height=0.36
Roundness=1            ; fully elliptical fovea
TransitionWidth=0.04   ; feathered edge, not a hard seam
PeripheralDlaa=1
PeripheralDlaaPreset=5
PeripheralDlaaScale=1  ; see the bug below -- do NOT persist a value below 1
```

`NrEnabled=0` is not optional in this arrangement. Cheeky also handles feature 18; leaving it on
puts two foveators in the same NGX path.

## Known bug: `PeripheralDlaaScale` below 1 at startup

Initialising **at** a scale below 1.0 produces sparkle/shimmer in the periphery. Raising to 1.0
clears it; returning to the original value does **not** bring it back. The value is not the
problem — initialising at it is.

```
SL eval=2 peripheral DLAA ready size=983x983 scale=0.64 convertedMV=yes
```

983 = 0.64 x 1536, and `convertedMV=yes`. Sparkle is the classic signature of motion vectors that do
not match the buffer they are applied to, and MV conversion is exactly what has to be re-derived on
a scale change — so the scale is applied at startup without the dependent state being rebuilt.

**Workaround:** persist `PeripheralDlaaScale=1` and lower it in the F12 overlay once per session.
Upstream bug; not fixable from here without their source.

## Roundness, for reference

Cheeky's composite shader lerps between two distance metrics, which is how one float gives every
shape from rectangle to ellipse:

```hlsl
return lerp(max(scaled.x, scaled.y),   // Chebyshev -> square
            length(scaled),            // Euclidean -> circle
            saturate(ShapeRoundness));
```

The feather is `smoothstep` over the last `Feather` of that distance, and the final pixel is
`lerp(bilinear, dlss, weight)`. **The DLSS region itself stays a rectangle** (`RectBase`/`RectSize`);
roundness lives entirely in the composite. Our port cannot copy this directly: NR writes in place and
the feature-18 stub ends in `jmp rax`, so there is no post-evaluation point at which to blend.

## Reverting

```
rename dxgi.dll -> dxgi.dll.<something>.bak
copy ReShade.ini.pre-cheeky.bak -> ReShade.ini
rename CheekyFoveatedDLSS.addon64 -> .off
reshade-addons.ini: [host] enabled=1
```
