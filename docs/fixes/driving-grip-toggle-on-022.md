# Driving grip toggle on 0.2.2

Restore the RC5 `de19d872` wheel/handlebar latch. Click near the animated
wheel hand to grab; release the physical grip without losing that grab. Click
again to release, even away from the handlebar. Each hand is independent.
The existing F10 driving control enables this behavior and describes it.

Retain 0.2.2's physical weapon-hand exclusion, tracking checks, input ownership,
steering and camera-miss heartbeat. Losing tracking, leaving the vehicle,
disabling wheel grabs or drawing a weapon in that hand releases it. A camera
miss cannot acquire a new grab but can release an existing one. A held button
through a blocked state is not treated as a fresh click afterward.

The regression compiles the actual two production decision blocks and exercises
click/release, independent hands, missing camera, tracking loss, weapon ownership
including left-handed weapons, and vehicle exit. Run:
`python3 tools/driving_grip_tests/test_driving_controls.py`.

Suggested independent PR: **Latch driving wheel grabs until the next grip click**.
No dependency on the haptics or framegen changes. In-game confirmation remains
necessary for animation feel and steering with the newer 0.2.2 vehicle code.

## Separate bike/car camera offsets restored

The first latch backport omitted RC5's camera controls. Restore its mounted-bike
classification, independent X/Y/Z settings, F10 controls, persistence and the
selection at LocateCamera. Upstream passenger-window combat continues to suppress
vehicle offsets. Older INIs without bike keys inherit the existing shared/car
values per axis, preserving fit. Tests exercise both camera choices and passenger
suppression in addition to the production latch blocks.
