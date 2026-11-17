# Crouch and snap-turn backport

Source: upstream 0.2.45, commit `6f33af62c50a85f88eceaa63f776539d8fc6f395`.
Applied selectively to the maintained 0.2.3 fork with its 0.2.45 backports.

- Downward right-stick travel sends one 100 ms R3 tap. Holding the stick does
  not hold the game's ToggleCrouch button; return below half travel to rearm.
- Snap turns use raw horizontal travel, with upstream defaults 0.80 to fire and
  0.50 to rearm, rather than the deadzoned/scaled smooth-look axis. Exact threshold
  equality counts. Invalid values cannot produce a snap or a nonfinite angle.
- Native vehicle/ladder/camera look retains its processed analog axis. Scanner
  zoom consumes the raw snap axis too.
- Additional integration correction: crouch observes physical downward travel
  even when scanner zoom consumes the output axis. A held stick must return
  before crouching on exit from scanner/device control. No synthetic crouch
  is emitted during those blocked contexts.

PSVR2 Triangle/Create, Quest thumbrest/inventory combo, grip scanner, driving
stunts and haptics are retained. No saved settings changed. Smooth turn feel and
bike sensitivity are not changed by the snap threshold.

Validation: 15 analog input cases under ASan/UBSan, scanner regression binary,
29 PSVR2/input-combination policy checks, and full Windows Release build pass.
Input tests include raw travel across deadzone/full-input settings, hysteresis,
native ownership, invalid values, crouch hold and scanner exit. These are code
checks, not headset validation. Artifact: build/rc-0245-fixes-20261118 in root.
