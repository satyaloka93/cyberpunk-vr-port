# Scene camera pose labels for frame generation

Source: dariulone/cyberpunk-vr-port 0.2.3, e91394502e (2026-11-08).

Backport only the scene pose identity additions in Hooks/LocateCamera.cpp.
After the final camera-buffer writes, publish the exact pose used by the scene
camera. The write-site route requires the same frame sequence and recenter
origin; the local route requires a valid numbered one-sample-per-frame pose.
Takeover cameras retain their existing publisher. This allows depth/motion
captures to match the submitted image during braindances and cinematic scenes.
No synthesis queue, frame deadline, adaptive pacing, smoothing or body changes.

Validation: test_scene_label.py compiles the actual selection/publication blocks
and exercises both sources, stale frames, changed origin, invalid/unnumbered
samples and takeover exclusion. MSVC Release builds. In-headset scene/braindance
validation remains pending; this is not a measured general FPS improvement.
