# Environment report — comparing VR setups

"It works on mine" is not a useful bug report, and neither is a prose description of a
mod list. This produces a flat, machine-readable spec sheet of a CyberpunkVR Port
install: hardware, XR runtime, game build, mod stack, every VR port setting including
the F10 live-controls menu, the game's graphics options, and a behaviour baseline from
the last run.

Two people on different headsets, GPUs and runtimes each run it, and the difference
between their setups becomes a plain text diff.

## Running it

```powershell
pwsh scripts\Collect-VRPortReport.ps1 `
    -GameRoot "D:\SteamLibrary\steamapps\common\Cyberpunk 2077" `
    -Label    "yourname-quest3" `
    -Hmd      "Meta Quest 3 (Link)" `
    -Json
```

Writes to `build\reports\vrport-env-<label>-<timestamp>.txt`, plus `.json` with
`-Json`. It is read-only, needs no elevation, and changes nothing.

`-Hmd` is free text because no runtime reports headset model reliably across
SteamVR / Oculus / WMR / Virtual Desktop. State it accurately; it is the single most
important field for comparison.

## Format

Flat `key = value`, one per line, dotted namespaces, `####` section markers as comments.

Two rules make it comparable:

1. **Every key is always emitted.** Missing things print `(absent)` rather than
   disappearing, so lines stay aligned between machines.
2. **One key, one line.** Newlines in values are collapsed.

```
gpu.name = NVIDIA GeForce RTX 4090
gpu.driver_version = 32.0.15.9186
vrport.xr_stereo_scale = 0.910
lastrun.present_fps_median = 43.8
```

Compare two reports with any diff tool:

```powershell
Compare-Object (Get-Content mine.txt) (Get-Content theirs.txt)
```

```bash
diff mine.txt theirs.txt
```

## Sections

| Section | What it answers |
|---|---|
| `report` | Schema version, when, whose |
| `system` | OS build, CPU, RAM (installed/usable, speed vs part-number rating), VBS |
| `gpu` | Primary adapter and driver, **plus every other adapter** — virtual display drivers from streaming apps are kernel-mode and matter |
| `xr` | Active OpenXR runtime, **implicit API layers**, SteamVR version and supersampling, declared HMD |
| `game` | Cyberpunk build, patch, store |
| `mods` | RED4ext/CET versions, plugin and mod lists, ReShade, and the DLSS/Streamline DLLs that sit in the Present path |
| `vrport_build` | SHA-256 of the deployed plugin — the only unambiguous build identity |
| `vrport` | **The F10 live-controls menu.** Stereo scale, world/IPD scale, sharpness, smoothing, prediction, snap turn, movement source, menu FOV, and render resolution |
| `vrport_hud` | F10 HUD layout offsets and scales |
| `vrport_vrik` | VRIK bridge settings and grip calibration |
| `vrport_vrcam` | Which VRCAM component and resolution are selected |
| `graphics` | Every `/graphics`, `/video` and `/display` option: upscaler mode, ray tracing, frame generation, resolution |
| `lastrun` | Pacing mode, XR cycles, save loads, median/min/max present FPS, and counts of fence timeouts, device removals and overlay faults |
| `stability` | Game crash reports, GPU crash reports, bugchecks and codes, GPU driver events over 90 days |

## Reading it

A few fields are easy to misread:

- **`xr.api_layers_implicit`** — the Khronos registry value is a *disable* flag, so
  `0` means active. The report already resolves this to `[enabled]` / `[disabled]`.
  Use `xr.api_layers_implicit_enabled` for the count that matters. Layers inject into
  every OpenXR app; ReShade and streamer compatibility layers both show up here.
- **`system.ram_speed_mhz`** is the speed in use. The rating lives in the part number
  (`F5-6000…` = DDR5-6000). Part number above the reported speed means EXPO/XMP is off.
- **`gpu.adapter[n].cm_error = 22`** means the device is disabled, which is often
  deliberate.
- **`lastrun.present_fps_max`** can be inflated by frames captured after the XR session
  ends. Trust the median.
- **`vrport_build.stereo_dll_sha256`** is the build identity. MSVC embeds timestamps, so
  rebuilding identical source gives a different hash — compare against a published
  hash, not your own rebuild.

## When filing an issue

Attach the `.txt`. If you have a working configuration and a broken one, attach both
and say which is which — the diff is usually the answer.

For crashes, also attach:

- `bin\x64\cyberpunkvrport.log`, and the archived
  `cyberpunkvrport-<timestamp>.log` for the session that actually crashed (the plugin
  archives the previous session on every launch, so relaunching no longer destroys it)
- the newest folder under `%LOCALAPPDATA%\REDEngine\ReportQueue\`, which contains the
  GPU breadcrumb log and an Aftermath dump

## For agents

The `.json` output is the same flat key/value map. Treat it as the ground truth for the
target environment and do not infer values that are `(absent)`. To reproduce a
configuration elsewhere, the fields that change behaviour are, in rough order of impact:

1. `vrport.*` — the F10 menu; `xr_stereo_scale`, `xr_world_scale`, `xr_ipd_scale`,
   `xr_sharpness`, `xr_motion_predict_ms`, `xr_hmd_smooth`, `xr_mono_submit`
2. `vrport_launcher.width` / `.height` — render target, and `hmd_type`
3. `graphics.graphics.presets.*` — upscaler, preset and sharpness
4. `xr.*` — runtime and enabled API layers
5. `mods.upscaler.*` — Streamline/DLSS DLL versions in the Present path
6. `vrport_build.stereo_dll_sha256` — the plugin build itself

Settings under `vrport.*` are written by the running plugin. Edit `vrport.ini` only
with the game closed, or the next save from the F10 menu will overwrite the change.
