# PS VR2 Sense adaptive triggers and grip haptics

This is an **optional PS VR2 add-on** for CyberpunkVR Port. It combines the gameplay profiles from [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156) with the [PSVR2Toolkit Cyberpunk DSX bridge](https://github.com/satyaloka93/PSVR2Toolkit/tree/cyberpunk-dsx-bridge-slot-guard). Use the guarded bridge build paired with this branch; older v0.2.1 builds understand the pulse slots but do not contain the fail-closed protocol check.

The Cyberpunk mod decides which weapon, vehicle, scanner, menu, and other gameplay effect is active. The bridge reads that state directly, translates its DSX-style trigger profile to native PS VR2 Sense commands, and sends synthesized 3000 Hz PCM recoil/fire textures to the controller grips. DSX itself is not used.

## Additional dependencies

These extend the normal CyberpunkVR Port requirements **only when adaptive triggers or grip haptics are wanted**:

1. [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156).
2. [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518), required by Enhanced DualSense Support.
3. [PSVR2Toolkit Cyberpunk DSX Bridge guarded build](https://github.com/satyaloka93/PSVR2Toolkit/tree/cyberpunk-dsx-bridge-slot-guard), plus the matching Toolkit raw-trigger driver from the existing bridge package.

Cyber Engine Tweaks and RED4ext are already requirements of CyberpunkVR Port. Do **not** install or run DSX for this path.

## Installation

1. Install CyberpunkVR Port and its normal dependencies.
2. Install Native Settings UI and Enhanced DualSense Support manually into the Cyberpunk game root.
3. Launch Cyberpunk once so Enhanced DualSense Support creates:

   ```text
   bin\x64\plugins\cyber_engine_tweaks\mods\DualSense Support\config\DualSenseXConfig.txt
   ```

4. In Enhanced DualSense Support's Native Settings page, turn **UDP autostart off**, then exit the game. Close any `UDPClient.exe` or DSX process.
5. Download and extract the Toolkit bridge package somewhere outside the game directory.
6. Close SteamVR completely and run `INSTALL_RAW_TRIGGER_DRIVER.cmd`. It backs up the currently installed Toolkit driver before replacing it.
7. Start SteamVR and connect both Sense controllers.
8. Run `run_bridge.cmd`. It discovers the Steam Cyberpunk installation and listens on `127.0.0.1:6969`.
9. Start Cyberpunk. Keep the bridge window open while playing.

For a nonstandard or non-Steam game location, start it from PowerShell with:

```powershell
.\run_bridge.ps1 -GameRoot "D:\path\to\Cyberpunk 2077"
```

## Enhanced DualSense Support compatibility

The bridge deliberately bypasses Enhanced DualSense Support's bundled `UDPClient.exe` and its RED4ext process-launcher DLL. They are not needed.

- Keep **UDP autostart disabled**.
- Do not use the mod's **Restart UDP Client** button; restart `run_bridge.cmd` instead.
- Do not run DSX concurrently. DSX and the bridge would compete for port/controller ownership.
- If RED4ext rejects `red4ext\plugins\DualSenseSupport\DualSense Support.dll` because it targets an older runtime, rename it to `DualSense Support.dll.disabled` or move it out of that plugin directory. Never patch only its advertised runtime revision; that was tested and made the game freeze/exit.
- `UDPClient.exe` may remain on disk, but it must not be running.

The bridge writes `DSXData.json` next to `DualSenseXConfig.txt`, allowing the mod's status panel to see a connected bridge without its native client.

## Sense weapon overrides — leave at Default

**Leave every weapon category at `Default`.** Enhanced DualSense Support's stock per-weapon
effects are the correct choice on PS VR2 Sense, and its **Not Recommended** label on the
category-override page is accurate.

Earlier releases of this document recommended a category profile (Very Soft handguns, Soft
revolvers, Choppy automatics, Medium rifles, Hard heavy weapons). **That advice was wrong and
has been withdrawn.** It was measured while the mod's own UDP client was autostarting
alongside the PSVR2Toolkit bridge, so two clients were driving the controller-effect path at
once. The inconsistent, partly missing trigger effects that the presets appeared to fix were
caused by that conflict, not by the stock effects.

Retested with only the bridge running, the overrides make things worse: haptics break and
trigger effects become inconsistent. If you applied the old profile, set every category back
to `Default`.

Stored values in `DualSense Support\config\settings.json` are `1=Default`, `3=Choppy`,
`4=Very Soft`, `5=Soft`, `6=Medium`, `7=Hard`. Back the file up before changing anything.

### UDP autostart re-enables itself

The mod's built-in default for UDP autostart is **on**, so using its *reset to defaults* button
— including while resetting weapon categories — silently restores the conflict. This is the
single most important setting to keep off.

This release locks it in the mod's own files: the default is changed to off, the Native
Settings switch refuses to latch and snaps back, and the stored value is off. NativeSettings
has no greyed-out control state, so snap-back is the closest equivalent. **Updating Enhanced
DualSense Support from Nexus will overwrite these edits — reapply them afterwards.**

## VR melee motion haptics

Physical melee swings and impacts produce haptics in the weapon hand — confirmed on katana and
machete. Swing strength scales with how fast you actually swing; impact is stronger and longer
so contact is distinguishable from the whoosh.

This needs a **bridge build containing the VR motion watcher**. The plugin publishes the
events, but an older bridge has nothing consuming them and you will feel nothing.

Game audio cannot provide this. The melee whoosh sits outside the bridge's 28-320 Hz tactile
band and is inaudible to it even at `--audio-haptics-gain 2.5`, and because that layer is
derived from stereo output it buzzes both grips rather than the hand holding the weapon.

| Flag | Effect |
|---|---|
| `--vr-motion-gain 0..3` | scales melee pulses only, default `1.0` |
| `--no-vr-motion-haptics` | disables them |

On startup the bridge prints `VR motion haptics watcher enabled at gain 1 (waits for
Cyberpunk).` After the compatible game DLL begins publishing, it also prints `VR motion haptics
protocol v1 active (driving-safe slot layout).` It attaches when the game launches, so bridge-first
startup is still correct. Gun and vehicle haptics are unaffected — melee pulses are mixed into the
same engine rather than bypassing it.

### Driving isolation

The bridge's pulse ABI remains `[157..160]`. Vehicle/reload inputs were moved to distinct slots:
right B `[164]`, left Y `[165]`, R3 `[166]`, and analog R2 `[167]`; the wheel ownership mask remains
`[163]`. Steering and wheel-arm blend values never enter the haptic record. A protocol marker,
version, and live OpenXR heartbeat in `[168..170]` must also validate before the guarded bridge reads
a pulse. This prevents wheel motion, trigger travel, or buttons from becoming per-frame haptic
floods, including when the bridge retains the shared mapping across game restarts.

Enhanced DualSense Support's own car/bike adaptive-trigger profiles and the audio-derived vehicle
rumble continue through the bridge normally. They mix in the same `HapticsEngine`; no vehicle path
is disabled.

### Nothing else may drive the actuators

Only the bridge may produce controller haptics. Driving them from the VR plugin as well — for
example an OpenXR vibration action — competes with Toolkit CAPI for the same Sense actuators
and costs most of the game's gun feedback, even when every pulse is accepted by the runtime.


## Effect fidelity

DualSense and PS VR2 Sense reuse custom mode numbers `0x22`, `0x23`, and `0x27`, but their parameter layouts differ. Directly sending DSX's packed Bow bytes to Sense made ordinary handgun profiles excessively stiff. The Sense-tuned bridge instead uses each weapon's start/end/strength parameters to create gradual take-up and increasing resistance, followed by a motor release at the detected shot event.

Official feedback, weapon, vibration, slope, and multi-position effects translate directly. Galloping and Machine use safe official trigger vibration while their temporal detail is carried by grip PCM.

Grip haptics combine two sources. A semantic layer detects weapon transitions—including same-mode shotgun breakpoint jumps—and adds explicit handgun, revolver, shotgun, support-hand, automatic-fire, and charge effects. A full-game layer captures the Windows default audio output only while Cyberpunk is running, extracts a stereo 28–320 Hz tactile band plus sharp transients, compresses it, and converts it to 3000 Hz Sense PCM. This extends feedback to explosions, impacts, vehicles, ambience, and other audible gameplay instead of limiting haptics to trigger changes. It is not a bit-perfect copy of Cyberpunk's inaccessible original DualSense waveform.

The default audio-haptic gain is `1.35`. Advanced users can run the bridge with `--audio-haptics-gain 0..3` or disable that layer with `--no-game-audio-haptics`.

## Troubleshooting

- **Bridge says CAPI cannot be located:** start SteamVR with the modified Toolkit driver before starting the bridge.
- **No `DualSenseXConfig.txt`:** verify Enhanced DualSense Support and Native Settings UI load in CET, then enter the game once.
- **UDP bind failed:** close DSX, `UDPClient.exe`, or another bridge instance.
- **Triggers work but grip haptics do not:** look for `Cyberpunk grip PCM haptics enabled`, `Full-game audio haptics active`, and `VR motion haptics protocol v1 active` in the bridge window or `bridge.log`. Confirm Cyberpunk is playing through the Windows default output device captured when the bridge starts.
- **Bridge reports an incompatible shared-slot layout:** the guarded watcher intentionally disabled only VR motion pulses. Install the matching CyberpunkVR Port DLL; gun/audio/vehicle haptics and adaptive triggers remain independent.
- **Effects remain active after a crash:** restart the bridge and stop it with Ctrl+C, or restart SteamVR.
- **A PlayStation VR2 App update removes the effects:** Steam may have restored Sony's driver; close SteamVR and rerun `INSTALL_RAW_TRIGGER_DRIVER.cmd` from the matching bridge release.

To remove the add-on, stop the bridge and restore its timestamped `driver_playstation_vr2.pre-raw-trigger-*.bak` file as `driver_playstation_vr2.dll` while SteamVR is closed.
