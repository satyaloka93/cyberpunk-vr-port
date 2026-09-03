---
type: Integration Policy
title: Selective upstream import policy
description: Import upstream changes by isolated responsibility without replacing validated PSVR2, stereo, DLSSNR, or save-load behavior.
tags: [upstream, integration, regression, packaging]
timestamp: 2026-09-03T06:53:51Z
---

# Rule

Do not cherry-pick a large upstream release merely because Git can merge it. Import one independently
reviewable responsibility per commit, preserve current defaults and payloads, build before deployment,
and require runtime validation before calling the result safe.

Upstream `0.1.6` changes 122 files and overlaps 31 files changed on this branch. Most overlaps merge
textually, which makes a wholesale merge more dangerous rather than less: semantic conflicts can arrive
without conflict markers.

# Accepted 0.1.6 imports

The first staged imports are deliberately separable:

- Exact standalone loot UI redscript.
- Braindance-only scanner scale, whose multiplier is `1.0` outside braindance.
- Stale command-queue aging before VR copies.
- Correct OpenXR colour/depth swapchain resting states.
- An eight-entry UI-only archive with zero depot-path overlap against the retained baseline archive.

These changes are build-validated but not yet headset-validated. They must not be described as release-safe
until the UI, menu transitions, repeated save loading, binocular output, and current DLSSNR profiles pass.

# Rejected bulk imports

The monolithic `0.1.6` archive is not used. Relative to the baseline it adds UI, laser/targeting, and player
replacement resources while removing `base/gameplay/focus_mode.envparam`. The eight UI resources were split
into `cyberpunkvrport_ui_016.archive` instead.

Native view attribution, environment production, finished-frame capture, braindance/turret cameras, input,
weapons, and grip changes remain deferred because they intersect validated local ownership and lifecycle
work. See the exact payload record in `docs/upstream-0.1.6-safe-imports.md`.

# Regression gate

Before deployment or publication:

1. Confirm redscript compiles without duplicate wrappers.
2. Verify loot, phone, subtitles, dialogue, tutorial, radio, vehicle panel, and scanner placement in-headset.
3. Repeat save loading and menu transitions with NR off, then with NR on as a separate experimental run.
4. Confirm MAIN/VRCAM pairing, profile latching, and OpenXR submit cadence remain healthy.
5. Keep every import commit independently revertible; do not push without approval.
