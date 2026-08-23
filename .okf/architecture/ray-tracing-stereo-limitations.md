---
type: Known Limitation
title: Ray-tracing stereo resource isolation
description: Why ray-traced reflections and lighting currently differ between the two VRCAM stereo views.
tags: [ray-tracing, reflections, lighting, gi, denoising, stereo, vrcam]
timestamp: 2026-08-10T07:52:00+09:00
---

# Observed behavior

In the current stereo mapping, the left eye is the MAIN game view and the right eye is VRCAM. A headset test isolated two ray-tracing failures:

- with ray-traced reflections enabled, the left eye lacked reflections visible in the right eye;
- with ray-traced lighting enabled, the left eye was darker than the right eye.

The same run logged matching MAIN/VRCAM render-mask categories for lights, environment probes, fog, emissive objects, and gameplay post-processing. This rules out a simple missing generic-lighting capability on VRCAM.

# Cause

Cyberpunk's real-time ray tracing is stochastic and temporally accumulated. Reflections and diffuse lighting depend on view-specific inputs such as jitter, motion vectors, reprojection matrices, reservoirs, denoiser history, and exposure.

The port gives VRCAM its own camera and DLSS viewport and already repairs the common temporal constants that the render-to-texture view originally skipped. However, important ray-tracing managers, caches, and denoiser resources remain global or shared. The GI compatibility path deliberately prevents VRCAM from rebuilding the shared cache because allowing both views to overwrite it caused ambient-light and shadow instability.

Consequently, one eye can consume missing, stale, or other-eye RT data. Separate random samples can also denoise to noticeably different binocular shading even when both passes execute.

# Current support boundary

Ray-traced lighting and ray-traced reflections are not stereo-safe in the current VRCAM renderer. The recommended configuration is to leave both disabled. Ray-traced shadows may be tested independently, but they should not be assumed safe until both eyes are compared.

Copying one eye's screen-space RT result to the other is not a correct fix because reflections, disocclusion, and lighting edges require eye-specific parallax. A complete solution needs either:

- isolated per-eye RT reservoirs, histories, denoiser instances, and outputs; or
- an alternate-eye rendering design in which each complete game frame owns one eye's temporal pipeline.

# Diagnostic interpretation

| Symptom | Likely failure class |
|---|---|
| Stable per-eye brightness difference | stale or shared GI/denoiser history |
| Reflection present in only one eye | missing or overwritten RT reflection output/history |
| Independent sparkle or boiling | uncorrelated stochastic samples |
| Alternating lighting or flicker | both views writing shared resources |
| Ghosting during head movement | invalid temporal reprojection or motion vectors |

# Citations

[1] [NVIDIA RTXPT issue: Path-Tracing SDK for VR](https://github.com/NVIDIA-RTX/RTXPT/issues/18)
[2] [NVIDIA Real-Time Denoisers](https://github.com/NVIDIA-RTX/NRD)
