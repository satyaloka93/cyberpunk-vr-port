# Selected 0.2.0/0.2.1 fixes on PSVR2 RC4

Based on RC4 plus the committed DLAA and dual-shot/recoil fixes (3eec7518).
Upstream sources: dariulone/cyberpunk-vr-port tags 0.2.0 and 0.2.1.

- Extend VRIK capacity from 800 to 2048 bones, including parent storage and
  parent-copy limits. Move enlarged configure scratch to static storage under
  the existing exclusive rebind lock. This supports clothing/cyberware rigs
  that previously exceeded the capacity; it does not force every rig to solve
  2048 bones each frame.
- Release support grip and its blend/steering state while both weapon records
  are present and the reload module's dual-wield lease is live. Adapted to the
  fork's existing published records instead of importing the new left-handed
  subsystem. Single-weapon support grips retain their normal path.
- Import the scope-only render-graph dependency patch. F10 General exposes
  Parallel camera-scope rendering, default on, persisted as xr_scope_graph_dag.
  Holster and redraw after a toggle. Exact call-site/preamble guard fails closed;
  page-protection failure is checked and trampoline instruction cache flushed.
- Deploy upstream's byte-identical planar water shader with the SBS eye-half
  early-exit guard. Each dispatch writes its own eye. No RT/RR preset changes.

No lobby, loading-panel timeout, upside-down-start, trigger-threshold, background
input, tracking-recovery, recenter, revolver-motion or DLSS-allocation changes in
this build. Previously pending experiments remain in the working tree but were
isolated during compilation. Existing DLAA and per-hand aim/recoil fixes remain.

Validation: actual-game scope instruction check; compiled production grip guard
for occupied/free/expired states; existing DLAA and dual-shot/projectile tests;
production solver harness with an added 2048-bone FK case and short-buffer
rejection. The water shader SHA-256 is
7ed8d2439b15cdbb522f64dca534902fda99689dc3f898d72f41316225493690.
Build/test logs and exact source patch are under build/batch021-isolation in the
parent repository. Deployment receipt records the DLL, shader and rollback copy.

Headset validation: check arm cyberware, two guns close together while squeezing
support grip, ordinary two-handed aim, camera scope stability/performance, and
water reflections per eye. No local scope FPS gain is claimed before measurement.

Solver suite: 62/63 pass. The launcher_pose wrist-roll expectation also fails
with original HEAD solver and 800-bone header in a separate baseline build;
pre-existing test failure, not a new failure from the enlarged bone capacity.
