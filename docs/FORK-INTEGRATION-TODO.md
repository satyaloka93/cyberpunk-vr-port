# Required before the next fork release

The user requested a local commit checkpoint on November7, not a release or push.
The 0.2.2 framegen checkpoint does not constitute the complete PSVR2/Quest fork.

Restore from the proven fork source and OKF history, rather than recreating:

- Full fork haptics: improved OpenXR behavior, PSVR2 Toolkit/bridge routing and
  automatic selection, per-firing-hand dual-wield vibration, adaptive triggers,
  configurable audio/vehicle feedback and the associated controls/settings.
- Independent dual-wield aiming: left shots follow the left weapon aim rather
  than the right laser; recoil stays with the weapon actually firing.
- Driving grip toggle: retain steering grip in cars and on bikes without
  requiring continuous squeeze, with the existing release/exit behavior.

Audit what upstream0.2.2 already supplies and transfer only missing behavior.
Preserve the tested framegen baseline during this work. Test the merged controls,
haptics and aiming before packaging. Do not publish a release until requested.
