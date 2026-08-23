# Quest and generic OpenXR controller haptics

CyberpunkVR Port drives Quest/Touch, Valve Index, HTC Vive, Windows MR, and simple OpenXR controller vibration directly through the game's OpenXR session. **Do not run DSX or the PSVR2Toolkit bridge for Quest.** PSVR2 uses the separate guarded bridge and rejects this generic output automatically.

## Feedback currently provided

- Confirmed gun rounds: weapon-weighted right-hand recoil.
- Two-hand firearm grip: weaker support-hand recoil.
- Physical melee: swing pulse plus stronger confirmed-impact pulse.
- Cars and motorcycles: continuous engine/road texture plus stronger audible gear-change and impact transients in both hands.

Vehicle feedback uses Windows WASAPI loopback on the default multimedia output. It is gated by the port's driver state and stops shortly after leaving the vehicle. The source is the same 28–320 Hz audio evidence used by the PSVR2 bridge, reduced to OpenXR amplitude/duration because Quest controllers do not support Sense PCM. Gear shifts are audio-derived, not an internal transmission event. Other applications playing through the same endpoint can contribute while driving.

## Settings

Open `F10 → Controls`:

- **OpenXR controller haptic gain**: global `0..2`, default `1.25`; `0` disables all generic haptics.
- **OpenXR vehicle audio rumble**: vehicle-only `0..2`, default `1.0`; `0` keeps gun/melee feedback but disables engine/road/shift audio.

The corresponding `vrport.ini` keys are:

```ini
xr_openxr_haptic_gain=1.25
xr_openxr_vehicle_haptic_gain=1.00
```

## Validation

On startup with Quest, `cyberpunkvrport.log` should contain:

```text
OpenXRManager[VehicleHaptics]: audio-derived engine/gear capture active ...
```

After the first gun/melee/vehicle vibration:

```text
OpenXRManager[Haptics]: generic OpenXR controller output active ...
```

After driving produces low-frequency audio:

```text
OpenXRManager[VehicleHaptics]: driving audio envelope confirmed ...
```

If capture is unavailable, ensure Virtual Desktop/Windows selected the headset audio device as the default multimedia output **before launching Cyberpunk**, then restart the game. Test a car and a motorcycle; fire while driving to confirm the event pulse remains distinct from the continuous engine texture.
