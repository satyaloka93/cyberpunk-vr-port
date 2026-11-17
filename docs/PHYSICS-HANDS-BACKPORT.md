# Physics hands adaptation — experimental test deployment

Source: dariulone/cyberpunk-vr-port **6f33af62c50a85f88eceaa63f776539d8fc6f395** (0.2.45).
Maintained destination: `cp2077-2.5-base-0.2.2`, the source branch for our **0.2.3 PSVR2 RC1** derivative.

This brings across the native PhysX hand bodies, contact fingers, nearby NPC
capsules, constrained NPC limb poses and physical object carrying. It includes
the HandCollision Lua producer, native registrations and the held-object submit
callback. Existing scanner grip, bike controls, aiming, framegen reconstruction,
scheduling and lightweight overlay remain in place. No upstream lobby, minimap,
framegen changes or image-addon changes are included. Distant Reach/pulling is
disabled in the adapted collision script.

## Controls and installation boundary

F10 **Controls → Physics hands (experimental)** is a saved master switch
(`xr_hand_physics`), **off by default**. Hand collision, vehicle collision,
loose-object pushing and grabbing have their own saved options alongside it.
The off state suppresses initial physics worker/census startup and contact
application. After activation, switching off parks hand/contact bodies through
the physics callback; the worker remains available until orderly shutdown.
Native physics never places hands on the vehicle: existing script collision
handles the seated body, while steering ownership excludes held handlebars.

A test deployment must include the built DLL **and** all six files from
`mods/cet/CyberpunkVRPort_HandCollision/`, plus the updated
`mods/cet/CyberpunkVRPort_VRIK/init.lua` time-scale publisher. This document is not
a deployment record. The user's bike-test installation was deliberately left
unchanged while this adaptation was built.

## Defects addressed during adaptation

- Upstream proximity selection could retain an already-penetrating palm pose as
  a grab anchor. The adapter resolves the actual palm/finger volumes out of NPC
  capsules, then selects the touched part again. New seats and grip compliance
  are checked too. The bounded solve rejects invalid or unresolvable geometry
  instead of committing a partial correction (8 cm limit, 17 probes).
- Upstream deliberately excludes forearms from self-torso collision. A separate
  elbow correction evaluates cached NPC/torso capsules, trying at most 12 bend
  directions around the shoulder-to-wrist axis. It preserves both segment
  lengths and the wrist. It is excluded while weapon/grip ownership needs the
  existing aiming/reload solve. It reduces penetration when an available bend
  allows it; it cannot guarantee a solution to physically impossible tracking.
- A point-distance query inside a physics shape returned the interior point as
  its supposed surface. A tiny-sphere separation query now finds an exterior
  point for that case.
- Plain concurrent `memcpy` guarded only by a sequence counter is a C++ data
  race. Physics snapshots now protect the POD copy; readers try once and never
  wait. Writers hold the lock only for the copy, not physics or file operations.
- Hand physics callbacks are serialized independently of engine simulate/fetch
  calls, preserving the subsystem's single-physics-writer assumption even when
  different scenes are serviced by different jobs.
- The worker is joinable, handles event/thread creation failure and stops before
  plugin shutdown. Shutdown waits for active custom physics work before closing
  its event. It does not join a detached worker after the module is unloaded.
- Cached static meshes are retained by private, unattached PhysX shapes rather
  than relying on the original streamed sector's lifetime. References are
  released when a back buffer is recycled, on scene change and during shutdown
  while the PhysX singleton remains alive. Shape count is bounded by the two
  existing 1536-entry caches; primitives need no retained shape.
- The finger worker now reads the front-cache index once under its read lock;
  invalidation during save loading cannot produce an index of -1.
- Scene changes retire owned hand/NPC actors instead of dropping their handles.
  Retirement waits until the owning scene is safe to mutate, or the actor is
  detached. Cleanup continues after the feature is switched off.
- Surface seating rejects distant body capsules by bounds before exact queries.
  This limits work while gripping near several NPCs.
- The native loose-object contact filter now obeys the F10 push switch.
- Carry lead/contact-margin input rejects NaN and applies finite bounds.

## Validation completed

- Windows Release DLL build and link (working CP 2.5 RED4ext SDK).
- Eleven contact/snapshot regression cases under AddressSanitizer and UBSan:
  exterior contact, multi-surface corner, bounded/invalid/unsolvable seats,
  capsule distance degeneracies, elbow geometry and multiwriter/read coherence.
- Four geometry cases against the game's actual `PhysX3Common_x64.dll` in a
  separate CPU-only test process: interior point to exterior for box, sphere
  and capsule; actual palm-box seating against a head capsule.
- Upstream held-grip test against the adapted headers: 72 rotation combinations,
  independent grips, stretch bounds, angular following at 45/60/90/120 Hz.
- Existing production driving latch/reset test and scanner-input test pass.
- Seven edited/copied Lua files parse with Lua 5.2 (syntax only).

## Remaining limitations / audit follow-up

NPC bodies use bone-fitted capsules, **not the currently skinned visual mesh**.
Different face shapes, thick clothes and moving hair may disagree with these
approximations. Forearm correction is bounded and may change elbow pose visibly;
its comfort, stability and actual in-game CPU cost require a headset test.

The cache/actor lifetime changes are implemented but still require repeated
save loads and sector streaming in-game. Retired actors belonging to a different
active scene wait for that scene's physics callback. Shutdown releases detached
actors only; actors still attached to a possibly simulating scene are left to
PhysX teardown. Do not treat this as support for unloading/reloading the plugin
while the game keeps simulating. The additional cache shapes retain meshes;
they do not duplicate mesh vertex data, but their allocation cost needs a run.

Ownership follows the reference counting in NVIDIA's
[NpShape implementation](https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/PhysX/src/NpShape.cpp).
Scene release removes actors rather than releasing their storage, as documented
in [PxScene](https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Include/PxScene.h).
The existing native-call slot guards and the added actor-getScene guard were
checked against the installed PhysX DLL. No new acquireReference slot is used.
Offline REA could read the binary, but did not resolve the requested procedure
for decompilation; the retention design uses the existing guarded shape-creation
and release calls, not a guessed additional vtable slot.

This is an experimental source candidate, not a runtime-validated collision
system. No live memory edits or injections were performed during the bike test.

Test first: empty palms on a wall/table, face contact before/after grip, an arm
across the torso, then release, scanner slow motion, reload and bike controls.
Check save-load/scene changes and shutdown before considering publication.

## Deployment update — 2026-11-16

Deployed at user request, with physics master/collision ON and other INI values
preserved. This fork loads scripts from `red4ext/plugins/cyberpunkvrport/lua/`;
source `mods/cet` files must go there, not the external CET mods folder.
The initial staging layout was corrected before deployment. Root artifact
`build/physx-adaptation-20261116/deployment.json` records all nine verified files
and the backup location. Headset validation remains pending.
