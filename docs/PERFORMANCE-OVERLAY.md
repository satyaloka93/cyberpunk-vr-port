# Portable delivery overlay

F10 → FPS overlay → **Delivery (large green)** selects three large green lines.
Position, size, distance and offsets use the existing overlay controls. Selecting
this layout initially places it at the top right; those settings then persist in
`vrport.ini` (`xr_framegen_overlay_layout=3`).

- **OUT**: accepted fresh real plus generated OpenXR submissions per second;
  repeated images are excluded. This is not physical headset scanout FPS.
- **REPEAT**: percentage of accepted submissions reusing content over two seconds.
- **AGE**: mean image-content capture-to-submit age over two seconds. Generated
  content uses its supplied midpoint timestamp. This is not motion-to-photon latency.
  Unknown or expired age displays `--`, not a fabricated zero.

The overlay is independent of the V557 backend, SteamVR scheduling changes and
external recorder. It does not change image processing or enable debug logging.
It records only successful submissions, including the last-image resubmit path.

## Porting to the maintained fork

Cherry-pick the overlay commit alone. The integration points are
`Framegen.hpp/.cpp` (metrics and settings), `DeliveryWindow.hpp` (bounded history),
`StatsOverlay.cpp` (drawing), `OpenXRFrameLoop.cpp` (successful submission hooks)
and `OverlayPanels.cpp` (layout selection/help). If the fork already tracks age
or repeats, adapt those hooks rather than recording the same submission twice.
Do not import V557 or the flight recorder merely to use the layout.

Validation uses `tools/runtime_recorder/delivery_window_test.cpp` and
`tools/runtime_recorder/test_overlay.py`: stale/unknown timestamps, repeats,
clock resets, ring wrap, three bounded green rows and the unknown-age placeholder.
