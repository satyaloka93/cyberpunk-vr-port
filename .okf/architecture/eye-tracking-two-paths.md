---
type: architecture
title: Two eye-tracking paths — ours and Cheeky's bridge
description: The port reads gaze from OpenXR directly; CheekyFoveatedDLSS reads it through its own API layer. Same eye data, different delivery, different failure modes, different keys.
tags: [openxr, eye-tracking, cheeky, reshade, psvr2, registry]
timestamp: 2026-09-09
---

# Two eye-tracking paths

Both read the **same** source. Neither has privileged access:

```
PSVR2 eye cameras -> PSVR2Toolkit SteamVR driver -> SteamVR -> XR_EXT_eye_gaze_interaction
```

The difference is only in how each piece of software reaches it — and the two have completely
different installation requirements and failure modes. Confusing them cost a working day.

See also [eye-tracked NR foveation](eye-tracked-foveation.md) for what our path drives.

## Side by side

| | **ours (the port)** | **Cheeky's bridge** |
|---|---|---|
| what it is | an OpenXR application | a ReShade addon + an OpenXR API layer |
| how it reads gaze | `xrLocateSpace` on its own action space | layer reads it, addon calls into the layer |
| hops from runtime | 1 | 2 |
| extra files | none | `CheekyOpenXRLayer.dll` |
| registry needed | none | yes (see below) |
| environment needed | none | `XR_ENABLE_API_LAYERS` |
| steers | the **DLSS-NR** region | the **DLSS-SR** fovea |
| fails | loudly, in `cyberpunkvrport.log` | **silently** — falls back to a fixed centre |

## Ours: nothing to install

The port creates the OpenXR instance and session, so it simply asks:

```cpp
xrStringToPath(m_instance, "/interaction_profiles/ext/eye_gaze_interaction", &profile);
xrStringToPath(m_instance, "/user/eyes_ext/input/gaze_ext/pose", &gazePath);
xrCreateActionSpace(m_session, &gazeSpaceInfo, &m_eyeGazeSpace);
xrLocateSpace(m_eyeGazeSpace, m_viewSpace, displayTime, &gaze);
```

**No registry key, no environment variable, no extra DLL, no install step.**

Log evidence of health:

```
OpenXRManager: eye gaze (XR_EXT_eye_gaze_interaction) advertised=1
OpenXRManager[Eye]: suggest eye_gaze_interaction -> 0
OpenXRManager[Eye]: gaze action space res=0 handle=...
OpenXRManager[Eye]: gaze tan=(0.164, 0.215) ... flags=0xF   <- 0xF = VALID|TRACKED
```

### `CHEEKY_OPENXR_LAYER_DISABLE` does NOT affect us

That variable names **Cheeky's manifest only**. Our gaze was verified `flags=0xF` while it was set
to `1` — the cleanest possible proof the two paths are independent. The name misleads; the scope
does not.

## Cheeky's: two components that must match

`CheekyFoveatedDLSS.addon64` is a ReShade addon with no OpenXR session of its own, so it cannot
call `xrLocateSpace`. It ships a layer to do that for it, and reaches across:

```cpp
GetModuleHandleW(L"CheekyOpenXRLayer.dll");
GetProcAddress(module, "CheekyOpenXR_GetGazeSnapshot");
```

Layer exports: `CheekyOpenXR_GetGazeSnapshot`, `CheekyOpenXR_SetSimulatedGaze`,
`CheekyOpenXR_SetSimulationPattern`.

### Everything Cheeky's path requires

| what | where | value |
|---|---|---|
| layer manifest | registry, **`HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit`** | value name = full path to `XR_APILAYER_CHEEKY_foveated_dlss.json`, DWORD data `0` |
| layer enable | env `XR_ENABLE_API_LAYERS` | must contain `XR_APILAYER_CHEEKY_foveated_dlss` |
| layer DLL | beside the manifest | `CheekyOpenXRLayer.dll` (manifest uses `.\\`) |
| kill switch | env `CHEEKY_OPENXR_LAYER_DISABLE` | must be **absent** |
| addon | game `bin\x64\` | `CheekyFoveatedDLSS.addon64` |
| host | game `bin\x64\` | `dxgi.dll` (real ReShade) — our host cannot serve it, see below |
| settings | `bin\x64\ReShade.ini` | section `[CheekyFoveatedDLSS]` |

The layer DLL and the addon must come from the **same build**. A mismatched pair was registered
here (`43dc21e5…` vs the addon's build `f67990ac…`) and had to be replaced.

### Explicit vs implicit — the trap

Their installer registers under `ApiLayers\Explicit`. An explicit layer is **registered but inert**
until something names it in `XR_ENABLE_API_LAYERS`. An implicit layer loads on its own.

**Enumerated is not enabled.** `xrEnumerateApiLayerProperties` lists explicit layers that are
merely *available*, so a probe printing "API layers visible to the loader …
XR_APILAYER_CHEEKY_foveated_dlss" proves nothing about whether it will load. That line was read as
success and it was not.

### `reg add` does not propagate an environment variable

Writing `XR_ENABLE_API_LAYERS` with `reg.exe add` puts it in the registry but does **not** broadcast
`WM_SETTINGCHANGE`, so Steam and everything it launches keep their old environment and the layer
silently never loads. `setx` does broadcast.

**The reliable fix is in the port**: `src/Core/XrLayerOverrides.cpp` sets the variable in *our own
process* before `xrCreateInstance`, driven by `vrport.ini`:

```ini
xr_enable_api_layers=XR_APILAYER_CHEEKY_foveated_dlss
```

That cannot be defeated by propagation and affects no other application. Confirmed by:

```
[XR] XR_ENABLE_API_LAYERS=XR_APILAYER_CHEEKY_foveated_dlss set for this process only
```

### Proving the layer actually loaded

Cheeky logs every DLL that enters the process. The definitive check is in
`bin\x64\CheekyFoveatedDLSS.log`:

```
HOOKDBG DLL LOAD seq=41 name=CheekyOpenXRLayer.dll base=00007FFEB2DD0000 size=217088
OpenXR gaze history reset (shared) view=1 reason=1
```

Absence of that `DLL LOAD` line, with the manifest registered, means **enabled** is the missing
piece — not the driver, not the runtime, not the addon.

## Why our addon host cannot serve Cheeky

Our host (`src/Addons/ReShadeAddonHost.cpp`) implements all ten ReShade entry points Cheeky
imports, and after adding the `AddonInit` call it does initialise, read config and hook Streamline.
It still fails, for two reasons:

* Cheeky subscribes to **eight** events (3, 16, 56, 59, 61, 70, 72, 74); the host dispatches two.
* Cheeky requests **API version 20**, RenoDX requests 18, and the host passes API-18-shaped
  arguments. The one event it does dispatch — present, id 74 — hands Cheeky objects of the wrong
  shape, which faults as `ACCESS_VIOLATION in D3D12Core.dll reading FFFFFFFFFFFFFFFF`.

Closing that gap means implementing ReShade's object model. **Cheeky needs real ReShade.**

`AddonInit` is worth keeping regardless: ReShade calls `AddonInit(addon, reshade)` after loading and
an addon may do all its real work there. Omitting it looks exactly like a working addon — Cheeky
registered, took the ImGui table, then subscribed to nothing, read no config and wrote no log.

## Two readers, one tracker

With both active there are **two independent consumers** of the same eye tracker, each smoothing and
dead-banding separately. They can momentarily disagree under fast saccades.

If that matters, the layer exports `CheekyOpenXR_SetSimulatedGaze` — the port could push its own
gaze in and make one source of truth. Not needed while the bridge works; it is the fallback.

## Failure decision table

| symptom | look at | likely cause |
|---|---|---|
| our `[Eye]` lines absent | `advertised=0` in log | runtime/driver not offering the extension |
| our gaze `flags=0x0` forever | session state | never reached **FOCUSED** — actions are inactive outside it |
| Cheeky foveates a fixed centre | `CheekyFoveatedDLSS.log` for `CheekyOpenXRLayer.dll` | layer not loaded: registered but not enabled |
| no `CheekyFoveatedDLSS.log` at all | host | `AddonInit` never called, or addon not loaded |
| Cheeky loads then faults in `D3D12Core` | host | running under our host instead of real ReShade |
