# Fixes

* [PSVR2 Triangle-touch D-pad shifting](psvr2-triangle-dpad.md) — Triangle capacitive touch substitutes for left thumbrest while the right stick emits D-pad directions.
* [VR HUD layout and one-shot widget adjustments](center-overlay-scaling.md) — F10 regions and controller-specific wrappers keep dynamic UI readable without D-pad panning or retained widget references.
* [Overlay middle-ground frame pacing](overlay-middle-ground-pacing.md) — Replaces the per-frame full GPU drain with exact resource fences and a validated previous-overlay throttle, without injected DXGI waits.
