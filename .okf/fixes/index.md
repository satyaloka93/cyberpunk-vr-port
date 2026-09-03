# Fixes

* [Render FOV, cover versus span](fov-cover-versus-span.md) — Upstream sizes the frustum to COVER a canted panel and spends roughly a third of the rendered solid angle outside the lens; `xr_fov_mode=1` asks for the de-canted span instead, trading one edge for sharpness.
* [Second-eye load crash (descriptor zero)](second-eye-load-crash.md) — **Validated.** The exact post-prepare guard rejected 37 zero-valued one-based descriptors during the reproduced load window; three games loaded in one process, binocular NR resumed, and OpenXR remained healthy.
* [Orientation-provider and PhysicalRay hot-path frame collapse](provider-hot-path-frame-collapse.md) — A `VirtualQuery` ahead of every cheap test made security cameras and turrets cost most of the frame; both paths now reject foreign callers before the syscall.
* [PSVR2 Triangle-touch D-pad shifting](psvr2-triangle-dpad.md) — Triangle capacitive touch substitutes for left thumbrest while the right stick emits D-pad directions.
* [VR HUD layout and scanner-details placement](center-overlay-scaling.md) — HUDitor owns standard widgets while a one-shot wrapper moves the complete dynamic scanner/quickhack details panel into view.
* [Overlay load-transition guard](overlay-load-transition-guard.md) — Restores the full drain only inside resource-churn windows, removing the DXGI_ERROR_DEVICE_HUNG faults that middle-ground pacing introduced while keeping its throughput.
* [Overlay middle-ground frame pacing](overlay-middle-ground-pacing.md) — Why the per-frame full GPU drain was removed and what the per-resource fences protect; superseded on its own by the load-transition guard.
* [Crouch-preserving sprint gate](crouch-sprint-stance-gate.md) — Full stick tilt requests the fastest movement the current stance allows instead of asserting sprint and standing the player up.
