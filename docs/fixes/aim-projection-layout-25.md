# Laser projection and the game 2.5 camera layout

A user reports accurate centre aiming but impacts below the laser when tilted up.
The first RC5 restoration fixed per-hand drawing but did not resolve this report.

`SlConstantsInner` still read FOV/zoom/near/far/P11 at absolute view offsets
0x90/0x9C/0xB0/0xB4/0x214, from game 2.31's descriptor base 0x70. Game 2.5 uses
base 0xE0 (`Render/ViewCameraLayout.hpp`, also used by AsymProjection), with the
same descriptor-relative fields. The resulting ADS multiplier is consumed by
laser projection and world markers. Reading another matrix element instead of
P11 can make projection scale depend on orientation; an incorrect scale can
agree at the centre while departing increasingly from the rendered world off-axis.

Use the shared 2.5 camera base and offsets 0x20/0x2C/0x40/0x44/0x1A4. The actual
view addresses are 0x100/0x10C/0x120/0x124/0x284. VRCAM's companion writes use the
same descriptor base, so they no longer write the old header fields. Invalid
FOV/P11 clears the ADS multiplier to 1 instead of retaining a stale scoped scale.
No hand aim angles, arbitrary dot offsets, DLSS input tags or framegen settings
change. This correction is independent of the RC5 per-hand aiming hooks.

`tools/dual_aim_tests/test_camera_projection.py` compiles the production sampler
and projection helper. It poisons the old offsets with plausible changing values,
checks pitch 0/15/30/60 degrees and zoom 1/1.5/4.25, compares off-axis projected
rays to the actual P11 and tests invalid input. The paired-dot, hitscan-selection
and projectile-spread regressions also pass. MSVC Release build passed.
Headset confirmation of the reported tilt-dependent error remains required.

Suggested upstream PR: **Use the 2.5 view-camera layout when recovering ADS zoom**.
