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

## The perk is declared, not queried

`Crouch-sprint perk owned` in the F10 **DLSS / Debug** section, persisted as
`xr_crouch_sprint_perk` in `vrport.ini`, default **off**.

The perk is deliberately not read from TweakDB: naming a record that silently returns false
would be indistinguishable from a bug. The default is the stance-preserving one, which is the
desired behaviour for a player without the perk. Tick it once the perk is unlocked.

# Signal path

Lua can read shared slots through `GetVRSharedSlot` but has no writer, so state the CET mods
observe reaches the runtime through purpose-built natives in the Hands plugin. A generic
setter is deliberately not offered — the shared block carries seqlocks and pose data, and a mod
writing the wrong index is how the smoking bridge once produced a self-igniting lighter.

| Slot | Contents |
|---|---|
| `[157]` | haptic sequence, written last so the payload is complete before it is observed |
| `[158]` | haptic hand |
| `[159]` | haptic amplitude |
| `[160]` | haptic duration ms |
| `[161]` | crouched |

Slots `[157..160]` are wired and firing but currently have no consumer — see
[PSVR2 adaptive triggers and grip haptics](../operations/psvr2-adaptive-triggers.md).

# Regression checks

1. Crouch, push fully forward: the player creeps at full crouch speed and stays crouched.
2. Stand, push fully forward: the player sprints as before.
3. Partial deflection still walks in both stances.
4. `crouch: locomotion state N observed` appears for standing and crouching. If instead
   `locomotion blackboard unreadable` appears, the gate is inactive and crouch will break out
   exactly as before the fix.
5. D-pad shifting via left-stick click still works — the sprint change must not disturb it.
