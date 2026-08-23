---
type: Fix
title: Crouch-preserving sprint gate
description: Full stick tilt means "as fast as this stance allows" rather than "sprint", so moving forward while crouched no longer stands the player up.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/core/vr_core.cpp
tags: [input, locomotion, crouch, sprint, xinput, psvr2]
timestamp: 2026-08-11T15:10:00+09:00
---

# Behavior

Full left-stick deflection requests the fastest movement the **current stance** allows, and
never changes the stance:

| Situation | Result |
|---|---|
| Upright, full tilt | sprint |
| Crouched, full tilt, no crouch-sprint perk | fastest crouch movement, stance kept |
| Crouched, full tilt, perk owned and declared | crouch-sprint |

Partial deflection behaves as the game's normal analog walk/jog in every case, so a crouched
player can creep.

# Why it needed fixing

Sprint was derived from stick magnitude rather than from a click, and asserted level-triggered:

```cpp
const bool wantSprint = (ly > 0.90f);   // -> XINPUT_GAMEPAD_LEFT_THUMB
```

On PSVR2 Sense sticks 0.90 is trivially easy to reach, so ordinary forward movement asserted
L3 continuously. Cyberpunk cancels crouch on sprint, so a crouched player walking forward
popped upright.

The obvious fix — route the physical left-stick click to L3 like a normal pad — is not
available. That click is already the D-pad shift modifier used by
[Triangle-touch D-pad shifting](psvr2-triangle-dpad.md); the code reads it and deliberately
discards it (`(void)XB_LEFT_THUMB`). Right-stick click *is* routed, which is why crouch
responds to a real click and sprint does not.

Suppressing sprint whenever crouched is also wrong: the crouch-sprint perk makes crouch plus
sprint a legitimate state, and the game resolves it from the player's perks. The plugin must
not pre-empt that decision.

# Implementation

The CET weapon mod publishes locomotion state; the XInput merge gates the sprint assert:

```cpp
const bool crouched      = OpenXRManager::Get().GetSharedSlot(161) > 0.5f;
const bool sprintAllowed = !crouched || (g_liveControls.xrCrouchSprintPerk != 0);
const bool wantSprint    = (ly > 0.90f) && sprintAllowed;
```

Stick deflection is always passed through unmodified, so withholding L3 costs no speed — it
only removes the stance change.

## Reading the locomotion blackboard

`PlayerStateMachine` is a **local instanced** blackboard keyed on the player entity.
`GetBlackboardSystem():Get()` returns nil for it and every read fails silently:

```lua
local bb = Game.GetBlackboardSystem():GetLocalInstanced(pl:GetEntityID(), d.PlayerStateMachine)
local st = bb:GetInt(d.PlayerStateMachine.Locomotion)
```

The mod logs each distinct locomotion value once (`crouch: locomotion state N observed`) and
reports an unreadable blackboard explicitly, so the crouch constant is confirmed against the
live build rather than assumed.

## Crouch-to-sprint remains opt-in

The native tuning export `CyberpunkVR_SprintFromCrouch` defaults to `0`, which blocks the full-stick sprint detent while locomotion is Crouch, CrouchSprint, or CrouchDodge. Setting it to `1` allows the game to decide whether a full detent should transition from crouch. It is not currently exposed in F10 or persisted in `vrport.ini`.

# Signal path

The current 0.1.3 port no longer sends crouch state through shared slot `[161]`. The VRIK CET mod calls the purpose-built `SetVRLocomotionState` native, which writes same-DLL `g_VRLocomotionState`; the XInput hook reads that global directly. Slot `[161]` now belongs exclusively to physical reload's trigger override.

The adjacent `[157..160]` block is the external PSVR2Toolkit motion-haptic ABI and has no role in stance. Keeping locomotion out of that record prevents a state transition from being interpreted as a pulse. See [PSVR2 adaptive triggers and grip haptics](../operations/psvr2-adaptive-triggers.md).

# Regression checks

1. Crouch, push fully forward: the player creeps at full crouch speed and stays crouched.
2. Stand, push fully forward: the player sprints as before.
3. Partial deflection still walks in both stances.
4. Verify the VRIK CET mod calls `SetVRLocomotionState` without a scripting error; a missing native leaves the gate in its explicit unknown-state fallback.
5. D-pad shifting via Triangle touch or the L3 fallback still works — the sprint change must not disturb it.
