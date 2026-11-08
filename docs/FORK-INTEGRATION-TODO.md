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

## November 8 integration checkpoint

Source restored in three isolated additions (no release/push):

1. `03378817`: RC5 OpenXR/Toolkit route, per-hand impacts, F10 audio/strength,
   physical swing and vehicle audio feedback. External Toolkit/DSX binaries and
   user trigger/audio profiles remain unchanged. PSVR2 Triangle/Create controls
   already exist in 0.2.2 and are retained.
2. `85a64797`: RC5 grip-click latch for cars and bikes, retaining upstream
   physical handedness and camera-miss behavior.
3. Independent dual-wield firing selection, projectile muzzle/spread and recoil
   restored in the commit adding this checkpoint. Retain 0.2.2 firing-state bits
   and its existing two-eye dots.

See `docs/fixes/rc5-haptics-on-022.md`, `driving-grip-toggle-on-022.md`, and
`dual-wield-aim-on-022.md` for validation and proposed independent PR boundaries.
The source gaps above are addressed; headset regression testing and user approval
for publishing are still outstanding. Framegen source/settings are preserved.
