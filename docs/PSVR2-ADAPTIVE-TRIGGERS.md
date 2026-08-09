# PS VR2 Sense adaptive triggers and grip haptics

This is an **optional PS VR2 add-on** for CyberpunkVR Port. It combines the gameplay profiles from [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156) with the [PSVR2Toolkit Cyberpunk DSX bridge](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0).

The Cyberpunk mod decides which weapon, vehicle, scanner, menu, and other gameplay effect is active. The bridge reads that state directly, translates its DSX-style trigger profile to native PS VR2 Sense commands, and sends synthesized 3000 Hz PCM recoil/fire textures to the controller grips. DSX itself is not used.

## Additional dependencies

These extend the normal CyberpunkVR Port requirements **only when adaptive triggers or grip haptics are wanted**:

1. [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156).
2. [Native Settings UI](https://www.nexusmods.com/cyberpunk2077/mods/3518), required by Enhanced DualSense Support.
3. [PSVR2Toolkit Cyberpunk DSX Bridge v0.2.0](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.2.0), which includes the matching Toolkit driver.

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

## Recommended Sense weapon overrides

Enhanced DualSense Support labels its weapon-category override page **Not Recommended** because overriding Default replaces some per-model DualSense effects with a category-wide preset. That trade-off is usually undesirable on DualSense, but its complex defaults do not transfer cleanly to PS VR2 Sense. Testing found the simpler official presets more nuanced and controllable on Sense:

| Weapon category | Override | Stored value |
|---|---|---:|
| Handguns | Very Soft | 4 |
| Shotgun | Hard | 7 |
| Submachine Gun | Choppy | 3 |

These settings are intentional for the PSVR2 bridge and are not unsafe. They alter R2 while the mod continues to provide gameplay state and L2 behavior. Leave Double-Barrel Shotgun, Light Machine Gun, Heavy Machine Gun, and other untested categories at Default until calibrated. Back up `DualSense Support\config\settings.json` before changing many categories.

## Effect fidelity

DualSense and PS VR2 Sense reuse custom mode numbers `0x22`, `0x23`, and `0x27`, but their parameter layouts differ. Directly sending DSX's packed Bow bytes to Sense made ordinary handgun profiles excessively stiff. The Sense-tuned bridge instead uses each weapon's start/end/strength parameters to create gradual take-up and increasing resistance, followed by a motor release at the detected shot event.

Official feedback, weapon, vibration, slope, and multi-position effects translate directly. Galloping and Machine use safe official trigger vibration while their temporal detail is carried by grip PCM.

Grip haptics combine two sources. A semantic layer detects weapon transitions—including same-mode shotgun breakpoint jumps—and adds explicit handgun, revolver, shotgun, support-hand, automatic-fire, and charge effects. A full-game layer captures the Windows default audio output only while Cyberpunk is running, extracts a stereo 28–320 Hz tactile band plus sharp transients, compresses it, and converts it to 3000 Hz Sense PCM. This extends feedback to explosions, impacts, vehicles, ambience, and other audible gameplay instead of limiting haptics to trigger changes. It is not a bit-perfect copy of Cyberpunk's inaccessible original DualSense waveform.

The default audio-haptic gain is `1.35`. Advanced users can run the bridge with `--audio-haptics-gain 0..3` or disable that layer with `--no-game-audio-haptics`.

## Troubleshooting

- **Bridge says CAPI cannot be located:** start SteamVR with the modified Toolkit driver before starting the bridge.
- **No `DualSenseXConfig.txt`:** verify Enhanced DualSense Support and Native Settings UI load in CET, then enter the game once.
- **UDP bind failed:** close DSX, `UDPClient.exe`, or another bridge instance.
- **Triggers work but grip haptics do not:** look for both `Cyberpunk grip PCM haptics enabled` and `Full-game audio haptics active` in the bridge window or `bridge.log`. Confirm Cyberpunk is playing through the Windows default output device captured when the bridge starts.
- **Effects remain active after a crash:** restart the bridge and stop it with Ctrl+C, or restart SteamVR.
- **A PlayStation VR2 App update removes the effects:** Steam may have restored Sony's driver; close SteamVR and rerun `INSTALL_RAW_TRIGGER_DRIVER.cmd` from the matching bridge release.

To remove the add-on, stop the bridge and restore its timestamped `driver_playstation_vr2.pre-raw-trigger-*.bak` file as `driver_playstation_vr2.dll` while SteamVR is closed.
