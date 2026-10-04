---
type: Behavior
title: Per-hand vehicle grip toggle
description: Grip clicks latch/release each VR arm against Cyberpunk's animated car wheel or motorcycle handlebars.
tags: [vehicle, driving, motorcycle, input, animation]
timestamp: 2026-10-03T00:00:00+09:00
---

# Input behavior

When `xr_wheel_grab=1` and the player is driving, each tracked hand works
independently:

1. Move within `xr_wheel_radius` of the hand pose supplied by the game's driving
   animation and click that hand's grip. Its arm blends back to the native
   driving pose, including native finger curl.
2. The physical grip may be released; the wheel grab stays latched.
3. Click that grip again to return the arm to controller-driven IK, even if the
   hand is no longer near the wheel.

The right hand cannot grab while a weapon is equipped. Leaving the driver seat,
disabling the feature, or losing that hand's tracking releases its latch. A
camera-pair miss alone does not release a still-tracked hand. The wheel-grab
armed mask continues to keep these clicks out of holster/reload/smoking actions.

# Implementation and regression boundary

`WheelUpdate` handles the grip rising edge and proximity-gated first click.
`WheelMaintainGrab` only maintains ownership and records the current grip edge
during camera-pair misses; it does not interpret a held grip as the latch.
`TrackedWheelSteering` still owns tracked pose freshness, steering continuity,
and prediction; this change does not alter those algorithms. The same game
animation pose is used for cars and bikes, with no per-vehicle wheel-bone table.

The upstream 0.1.7 base used squeeze-to-hold semantics. The earlier fork's
`88bd4ef` vehicle implementation and the August 23 project notes specify
click-to-toggle semantics; this adaptation restores that ownership behavior
while retaining 0.1.7 tracking guards and the later steering pipeline.
`tools/driving_tests/grab.cpp` covers latching, second-click release,
camera-pair loss, and tracking-loss release. Automated results do not replace
an in-headset car and motorcycle test. The user confirmed the deployed toggle
works on 2026-10-03; the production wheel ownership regression passed again
during commit preparation on 2026-10-04.
