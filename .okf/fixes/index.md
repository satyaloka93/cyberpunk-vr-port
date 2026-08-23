# Fixes

* [PSVR2 Triangle-touch D-pad shifting](psvr2-triangle-dpad.md) — Triangle capacitive touch substitutes for left thumbrest while the right stick emits D-pad directions.
* [VR HUD layout and scanner-details placement](center-overlay-scaling.md) — HUDitor owns standard widgets while a one-shot wrapper moves the complete dynamic scanner/quickhack details panel into view.
* [Overlay load-transition guard](overlay-load-transition-guard.md) — Restores the full drain only inside resource-churn windows, removing the DXGI_ERROR_DEVICE_HUNG faults that middle-ground pacing introduced while keeping its throughput.
* [Overlay middle-ground frame pacing](overlay-middle-ground-pacing.md) — Why the per-frame full GPU drain was removed and what the per-resource fences protect; superseded on its own by the load-transition guard.
* [Crouch-preserving sprint gate](crouch-sprint-stance-gate.md) — Full stick tilt requests the fastest movement the current stance allows instead of asserting sprint and standing the player up.
