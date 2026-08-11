# Fixes

* [PSVR2 Triangle-touch D-pad shifting](psvr2-triangle-dpad.md) — Triangle capacitive touch substitutes for left thumbrest while the right stick emits D-pad directions.
* [VR HUD layout and one-shot widget adjustments](center-overlay-scaling.md) — F10 regions and controller-specific wrappers keep dynamic UI readable without D-pad panning or retained widget references.
* [Overlay load-transition guard](overlay-load-transition-guard.md) — Restores the full drain only inside resource-churn windows, removing the DXGI_ERROR_DEVICE_HUNG faults that middle-ground pacing introduced while keeping its throughput.
* [Overlay middle-ground frame pacing](overlay-middle-ground-pacing.md) — Why the per-frame full GPU drain was removed and what the per-resource fences protect; superseded on its own by the load-transition guard.
* [Crouch-preserving sprint gate](crouch-sprint-stance-gate.md) — Full stick tilt requests the fastest movement the current stance allows instead of asserting sprint and standing the player up.
