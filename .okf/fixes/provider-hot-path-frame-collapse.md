---
type: Fix
title: Orientation-provider and PhysicalRay hot-path frame collapse
description: VirtualQuery on two per-entity hot paths dropped the frame rate from 60-70 to about 10 near security cameras and turrets; both are now gated so only rays and slots that carry the player's shot pay the syscall.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Natives/OrientationProvider.cpp
tags: [performance, hooks, orientation-provider, weaponaim, virtualquery, upstream-0.1.4]
timestamp: 2026-08-24T09:30:00+09:00
---

# Behavior

Two hooks installed across every entity in the world each performed a `VirtualQuery` — a kernel
transition — before any cheap test could reject the caller. Cameras and turrets evaluate these paths
**every frame**, from several worker threads, so the cost scaled with how many were active nearby.
Upstream measured 60–70 FPS falling to about 10 near either, and confirmed it by gating the hook
family off and watching the drop disappear.

Neither fix weakens a guard. Both keep the identical conjunction and the identical `__try`; only the
**order** and the **reachability** of the expensive test changed.

| Path | Before | After |
|---|---|---|
| `Hooked_WaPhysicalRayOrigin` | `WaPlayerOwnedWeapon` (3 × `VirtualQuery`) ran first, so every ray in the world paid it | Free checks first — `WaMuzzle` reads our own shared block, the distance bound reads the buffer the original evaluator just wrote. A camera across the street is rejected before the first syscall |
| `ProvStub` out-buffer read | `IsReadable(outp, 16)` on all 48 vtable slots × 3 provider classes | Read only when the slot carries the shot, the manual native override points at it, or `CyberpunkVR_XrDeepDiag` is on |

Removing the `IsReadable` guard outright is **not** the alternative: upstream tried it and it crashes
during loading, because the engine's vectored handler takes the access violation before the `__try`
sees it. The guard stays; what goes is the read itself wherever it only ever fed a diagnostic.

# The slot gate is class-blind on purpose

```cpp
constexpr bool kSlotCarriesTheShot = (S == 30) || (C == 2 && (S == 33 || S == 34));
```

`S == 30` carries **no class test**, and that is load-bearing. The launch-override block below it is
gated on `S == 30` across any provider class (pistol = `entFunc`, grenade = `entEntity`). An earlier
draft read `C == 2 && ...`, which is `entFunc` alone — for a grenade or any projectile arriving
through `entEntity` the out-buffer was never read and the whole override block never ran. Slots 33
and 34 are the hitscan orientation reads and those genuinely are `entFunc`-only.

# Supporting changes

* **`CyberpunkVR_CaptureMarkers` initialiser 1 → 0.** RenderDoc scaffolding that shipped armed: every
  change of (command list, node, view, cascade) formatted a label and called `SetMarker` on one of
  the game's own command lists, and a camera feed or monitor classifies as an extra view, so each
  active one multiplied the marker count. In practice `ApplyLauncherDebugGate` already forced this
  flag to `0` whenever launcher DEBUG was unticked, so the initialiser change only hardens the window
  **before** the gate runs — it is not the source of the in-play collapse.
* **`WeaponAim` kill switch.** The family is now installed through `CVR_HOOK_IF` with a compile-time
  gate, kept because it is what isolated the bug. Leave it at `1`; at `0` the log says
  `[hooks] boot WeaponAim skipped (disabled)` at the cost of muzzle-origin bullets, hitscan hand
  recoil, and the shot signal the barrel dot uses. The hook installs **at boot**, which is why
  deleting CET and redscript mods proved nothing during the hunt.

# Two-hand grip radius

`CyberpunkVR_TwoHandRadius` moved from a hardcoded `0.12` to a default `0.06`, exposed as the live
ini key `xr_two_hand_radius` and clamped to `[0.02, 0.30]` on read. At 12 cm the two-handed hold was
offered to a hand merely passing the weapon. This is a feel value, so it no longer costs a rebuild
per attempt. It appears in F10-persisted profiles automatically; existing profiles pick up the
default until the key is written.

# Validation

* Merged into `upstream-0.1.3-psvr2` from upstream tag `0.1.4` with **zero conflicts** — the fork's
  PSVR2/Quest work touches disjoint regions of the two shared files, and `WeaponAim.cpp`,
  `TwoHandGrip.cpp`, and `CommandListCensus.cpp` were untouched by the fork.
* Compiles clean under MSVC 2022 Release x64 (`CyberpunkVR_Stereo.dll`, no errors). The new
  `constexpr` gate raises benign `C4127 conditional expression is constant` — that is the 141 other
  stub instantiations folding the test to `false`, which is the point.
* The fork's haptic-gain, bike-offset, and throttle-trim keys all survive in
  [controller input pipeline](../architecture/controller-input-pipeline.md) order alongside the new
  radius key.

**Not yet measured on this fork's hardware.** The frame-rate figures above are upstream's. A PSVR2 or
Quest session near a security camera or turret is the outstanding regression check, and it matters
more here than upstream because [Quest vehicle work](../architecture/vehicle-interaction-pipeline.md)
already runs an audio-loopback haptics worker on the same frame budget.

# Citations

* Upstream `0.1.4`: *PhysicalRay stops charging every ray in the world, and a weapon's slots go in BOTH files*
* [Ray-tracing stereo limitations](../architecture/ray-tracing-stereo-limitations.md) — the other standing per-view cost
