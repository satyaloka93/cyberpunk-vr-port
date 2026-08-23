---
type: Operations Guide
title: Optional low-spec engine profile
description: Why the historical vrcam_cpu_tweaks.ini is not a VR dependency, which visible settings it changes, and how to A/B it safely.
tags: [performance, fidelity, streaming, graphics, configuration, quest-3, psvr2]
timestamp: 2026-08-23T17:30:00+09:00
---

# Decision

`vrcam_cpu_tweaks.ini` is not required for OpenXR, VRCAM, true stereo, VRIK, input, or haptics. Cyberpunk restores its compiled/platform defaults when the loose override is absent. The release therefore packages the historical profile only as:

```text
OPTIONAL\low-spec\vrcam_cpu_tweaks.ini
```

Extraction must not place it in `engine\config\platform\pc`, and `sync_assets.ps1 -Push` must not reactivate it.

# Why its old name is misleading

The profile contains some CPU/streaming cadence changes, but it also changes visible fidelity:

- decal hide distances (`25/15`; the PC platform decal override is `40`),
- rain and distant-shadow batch/triangle budgets,
- grass, hair, particle, terrain, and tree LateVS allocations,
- maximum world streaming distance and auto-hide distance,
- visibility-query activation distance,
- offscreen particle lifetime/update cadence.

Its own comments describe handheld, integrated-GPU, shared-VRAM, and 720p–1080p targets. It also sets one loading thread and a very large `MaxNodesPerFrameWHileLoading=65536`; those are not universal high-end desktop defaults and should not be mixed into crash diagnosis unless deliberately under test.

# Current high-end test system

The Quest run uses a Ryzen 7 9800X3D, RTX 4090 24 GB, 64 GB RAM, and `3072×3416` per-eye output. The active game is on a SATA SSD. Logs show roughly `42–62 FPS` at a 90 Hz display cadence under simultaneous true stereo, while the game already uses Low crowd and DLSS Ultra Performance (`1024×1139` internal per eye).

Recommendation: leave the optional INI inactive. This system does not need handheld VRAM/detail reductions, and the existing image-quality settings are already more aggressive than the optional profile can justify. Establish a stable 45 FPS cadence first; then test DLSS Performance or Balanced separately if GPU headroom permits.

# Safe A/B

1. Stop Cyberpunk and all bridge/helper processes.
2. Back up the active INI if present, then remove it from `engine\config\platform\pc`.
3. Use the same save, route, weather, resolution, runtime, refresh/throttling, and duration.
4. Compare game frame time, one-percent lows, pop-in, decals, distant shadows, vegetation, particles, and save-load behavior.
5. Only install the optional profile if a repeatable frame-time improvement outweighs those losses.
6. Delete the copied active file to revert; no cache rebuild or settings reset is required.

The 16:57 load crash ended with an invalid `-1` render descriptor during `Loading world`. The profile is not established as its cause, but removing unneeded loading/streaming overrides produces a cleaner reproduction baseline.
