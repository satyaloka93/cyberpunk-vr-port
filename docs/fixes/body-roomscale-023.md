# 0.2.3 body rotation and roomscale backport

Source: upstream `e91394502e1cd1c10e95243f8f6a5b8ac4cb1a9e` (0.2.3), compared
against 0.2.2 `0a2f4f10`. Applied to the customized 0.2.2 branch after `29196735`.

## Behaviour

- Adds Head-led body rotation: a 10-degree upright free-look cone, followed by
  body rotation at 300 degrees/second; looking down beyond 30 degrees widens the
  cone to 70 degrees so looking at equipment need not twist the body.
- Corrects physical-turn neck pivot versus stick-turn eye pivot, roomscale capsule
  alignment, and the body origin relative to the rendered head pose.
- Adds physical-walking hysteresis, bend/crouch transitions, squat foot planting,
  knee/toe alignment, and upstream forearm roll sharing.
- Retains existing body modes in F10. Head-led defaults on when its new setting
  is absent (unless tracked mode takes precedence). No deployment INI rewrite.
  F10 persists `xr_head_led_body_rotation` alongside the other mode flags.

The backport preserves customized aiming, independent dual-wield shots, haptics,
Quest controls, car/bike offsets, driving grip latch and SteamVR FG scheduling.
It does not include upstream scene comfort/suspension changes, mirror handling,
reload changes, scanner/melee changes, launcher or installer changes.

## Validation

MSVC Release DLL build passed. Windows CTest: body rotation 43/43, roomscale 57/57
(including native hook-site verification against installed game 2.5), IK 63/63,
driving 22/22. Five existing actual-source Python regressions passed: camera
projection, dual barrel dots, dual projectile orientation, dual shot selection,
and driving grip controls.

The legacy driving camera-loss test expected hold-to-grip semantics. Updated
its assertions to require grip release to retain the latch, the second click to
release it, and further clicks without camera/proximity to remain disengaged.
Production wheel behaviour was not changed by this batch.

Headset validation is pending. This is a body alignment/interaction update;
no measured frame-rate improvement is claimed. Test physical 90/180-degree turns,
look down at belt equipment, crouch/stand, roomscale collision, two-handed aiming,
and bike/car grip latch. Check tracking recovery and older body modes if needed.

## Deployment

DLL-only deployment with previous-build backup and SHA-256 verification. Port
configuration, game graphics settings, Cheeky runtime, bridge executable and
haptics settings are protected. Local provenance and receipt are stored as
`body023-provenance.json` and `body023-deployment.json`. No release or push.
