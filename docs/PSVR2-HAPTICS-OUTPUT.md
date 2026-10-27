# Optional PSVR2 haptics output (unreleased)

This change follows 0.1.8-psvr2.1-rc1. It is not in that published ZIP.
Keep upstream 0.1.8 asymmetric projections; do not bring back the PR's old FOV
presets or its 0.1.7 camera assumptions. Quest 3 stays on native OpenXR. Its
missing ordinary-melee swing pulses and vehicle rumble are now restored from
the earlier fork; gun pulses again follow weapon strength with a weaker
support hand. Upstream's belt, grenade, block/parry and deflect effects remain.

## Select output

F10 > Controls > **PSVR2 haptics output (restart)**:

* **Auto (detect bridge at startup)** is the default for new configurations.
  Quest/other headsets always use OpenXR. PSVR2 uses the compatible running
  bridge if detected; otherwise it receives the same generic OpenXR effects.
* **OpenXR (standard vibration)** explicitly selects native output. Stop the
  external Toolkit bridge before launching the game in this mode. Native
  weapon recoil and interaction envelopes are enabled; no Toolkit heartbeat or
  melee pulses are published. Advanced trigger effects require the bridge mode.
* **Prefer Toolkit (OpenXR fallback)** retains the old saved value `1`.
  Start the compatible bridge before the game. Port vibration events go only
  to Toolkit; native shot envelopes are suppressed because EDS already handles
  recoil. Ordinary-melee swing feedback uses the existing bridge protocol.
  No OpenXR vibration calls are made when Toolkit is active. If the compatible
  bridge is not detected at startup, OpenXR is used rather than silently
  discarding haptics.

The saved key is `xr_psvr2_haptic_backend=0` (OpenXR), `1` (prefer Toolkit),
or `2` (Auto). Missing values use Auto; invalid values use OpenXR.
Existing explicit native preferences are preserved. Non-PSVR2 headsets always use OpenXR,
even with a saved Toolkit preference. Changes are saved immediately but the
active output remains fixed until the game/OpenXR session is restarted.
The UI distinguishes saved and active output. It does not launch/stop the
external bridge, change its driver, or edit SteamVR bindings.

The restart requirement is intentional: existing Toolkit bridges have
independent EDS trigger and audio writers. Stopping the port's heartbeat alone
cannot silence those. Do not leave the bridge running in OpenXR mode.
Start/stop the bridge **before** starting the game; this is not live handoff.
Detection requires a same-user-session `psvr2_toolkit_dsx_bridge_017.exe`
process owning a loopback UDP socket. It does not mistake the installed driver,
the game's own shared mapping, DSX, or another bridge version for that process.
This verifies a listener, not an acknowledgement that physical actuators work;
the released bridge has no return readiness/ownership channel. Renamed bridges
are not detected. Restart after a bridge exits or is started during a run.

`Controller vibration` and `Vibration strength` affect the port's envelopes
and Toolkit swing pulses. They do not control the external bridge's EDS
effects/audio; those remain in Mods > DualSense settings > PSVR2 Haptics.

Native gain controls in F10 > Controls:

* `OpenXR gun/swing/vehicle gain` (`xr_openxr_haptic_gain`, default `1.25`).
* `OpenXR vehicle audio rumble` (`xr_openxr_vehicle_haptic_gain`, default `1`).
  Set this to zero to disable only driving rumble.

Vehicle rumble restores the old default-output WASAPI 28–320 Hz envelope;
it is active only while driving a car/bike. Music and other programs on the
same audio output can contribute. It is not transmission telemetry or adaptive
trigger resistance. Processing skips audio filtering on foot; no audio work or
bridge process scanning runs in the render loop. The audio worker only publishes
amplitudes; OpenXR calls remain on the synchronized XR frame thread.

## If PSVR2 OpenXR vibration is silent

Use **Test left vibration** and **Test right vibration** beside the strength
slider, with Controller vibration enabled and strength above zero. These
queue a short native belt-touch event through the normal XR-thread path.

Check the port log for `OpenXRManager[Haptics]`: selected route, actual bound
output paths, and first/changed `xrApplyHapticFeedback` result per hand. A
successful call is not proof that the actuator vibrated; zero bound outputs
indicates a missing binding. Tests require an active/focused VR session.

On the development system, SteamVR selects Cyberpunk Workshop binding
`3788385174`. Its cached JSON contains `"haptics": []`. This is a concrete
configuration gap that can explain silence before our integration. It is not
proof that upstream lacks PSVR2 output support, nor proof that adding bindings
alone resolves every runtime/driver problem.

In SteamVR's Cyberpunk controller binding editor, assign the gameplay **Haptic
Feedback** output to both left and right haptic outputs, saving a personal
copy. Preserve the existing poses, Triangle touch, Create and other inputs.
Do not reset every binding or edit SteamVR's system/screenshot binding to fix
game vibration. The port's native output suggestions already include both
hands for its Touch-emulation route; an older custom binding can omit them.

Alternatively, with SteamVR and the game closed, run
`scripts/Repair-PSVR2-HapticBinding.ps1` for a preview, then run it with `-Apply`.
It copies your selected Cyberpunk Sense binding, adds only the missing outputs,
and selects that personal copy. It backs up original settings and does not edit
the Workshop binding or your system/Create controls. It does not stop a running
Toolkit bridge for you. Start SteamVR again after repair.

Toolkit setup remains external:
[PSVR2Toolkit bridge v0.3.2](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2).
No Steam DSX purchase is required. Existing compatible Toolkit drivers can be
kept. OpenXR vibration and physical PSVR2 acceptance still require testing.

## PR scope

Update existing PR96 onto 0.1.8 with controls, this optional bridge route,
adapted ordinary-melee feedback and default-on pacing. Leave asymmetric FOV
unchanged and omit release packaging and broad performance diagnostics.
Publishing RC1 did not update PR96; this local change does not publish it either.
