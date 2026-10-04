# PSVR2 tweaks on upstream 0.1.7: first commit

Base: official `0.1.7`, `a0c23fe110530bb74a73b3adf1ffa7d83050c43d`.
Branch: `psvr2-tweaks-0.1.7`. This is not the older `psvr2-tweaks` implementation.

## Scope

This first change restores the measured Sense input behavior and provides a safe
launcher for the existing optional PSVR2Toolkit DSX-compatible bridge. It does
not add Quest/general OpenXR haptics, alter rendering, change shared-memory
slot meanings, or port unrelated old gameplay remaps. General OpenXR haptics
must be a separate commit with explicit actuator ownership.

## Controls

| Sense input | Result |
|---|---|
| Touch Triangle, move right stick | D-pad, threshold 0.5; no right-stick turning during the whole touch |
| Click Triangle | Y remains available |
| Left Create / application SystemButton, release before 500 ms | Start / pause |
| Hold Create at least 500 ms | Back / in-game menu once; release does not also emit Start |
| Triangle touch + R3 without L3 | Same menu tap/hold fallback; suppresses R3's gameplay action |
| L3+R3 | Upstream overlay shortcut, priority over the fallback |
| L3+right stick | Existing upstream D-pad fallback and threshold unchanged |

The new touch action and SystemButton identity are created only for PSVR2,
identified from the OpenXR system name. Other headset bindings/menu behavior are
unchanged. SteamVR presents Sense as Oculus Touch; Triangle touch is
`/user/hand/left/input/y/touch`.

If Create does not arrive, open SteamVR controller bindings for CyberpunkVR and
assign physical **left Create** to **SystemButton (Sense Create)**. The Oculus
left-menu suggestion remains present, but SteamVR's Sense auto-remapper has
previously dropped that suggestion. An old custom binding may need updating to
include the touch action. Right Options/the SteamVR dashboard remain reserved;
this application action does not open the SteamVR dashboard.

SystemButton timing runs at XInput polling, not as a transient XR-frame pulse.
Losing input/focus or overlay capture cancels pending taps. Shift also clears
physical/Steam XInput look contributions and pending snap rotation.

## Optional adaptive triggers / advanced haptics

The supported bridge/launcher belongs to the external
[PSVR2Toolkit v0.3.1 release](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.1), not the
Cyberpunk game-root ZIP. Use the Toolkit's Cyberpunk **0.1.7** bridge build;
do not substitute its older legacy shared-slot bridge. Its installer/driver
steps are intentionally separate from switching the game mod.

Run the Cyberpunk 0.1.7 launcher supplied by the external PSVR2Toolkit
**after starting SteamVR and before the game**. For this developer checkout,
`scripts\Start-PSVR2Bridge.bat` is also available; it is not bundled in the
game ZIP or switcher asset because the Toolkit owns the release launcher.

Requirements installed separately:

- Enhanced DualSense Support CET mod and Native Settings UI.
- Compatible PSVR2Toolkit bridge/CAPI driver setup.
- Enhanced DualSense Support `UDPautostart=false`.
- No DSX app, bundled UDPClient, second bridge or other Sense actuator owner.
- Disable the unnecessary Enhanced DualSense Support native launcher DLL if
  present. Never patch its advertised game version to bypass RED4ext validation.

## Release switcher

The separate `CyberpunkVRPort-Switcher.zip` contains the Windows PowerShell
switcher, batch entrypoint, pinned dependency manifest and this guide. Extract
it **outside** the game folder. It can stage/install the official upstream
0.1.7 ZIP or the PSVR2-fork game-root ZIP. Its generated release metadata pins
the fork ZIP's planned GitHub URL and SHA-256; automatic download works after
the corresponding GitHub asset is published. Before publication, use
`-ForkArchive` with the local ZIP. Pass `-ForkSha256` for a differently named
local ZIP. Existing Nexus mods are kept by presence/version
checks; missing GitHub dependencies download with pinned SHA-256, while Nexus
account downloads require a manual ZIP. The switcher backs up replaced port
files and can restore them. It preserves `xr_force_fov` and other personal fit
settings, but resets experimental render/FG options for a clean migration.

The switcher prints the Toolkit link when it detects PSVR2/Sense hardware or
the fork is selected. It never installs a driver or registers an OpenXR layer.
For a no-write dependency/plan check, use `-Action Prepare`; installation
requires the game to be closed and an explicit confirmation.

## F10 rendering choices

The FOV override remains under STEREO VIEW, with PSVR2/Quest presets and an
explicit enable checkbox. The experimental performance-test buttons have been
removed from F10. A session-only **Stable pacing with no overlay graphics**
checkbox is under STEREO > PERFORMANCE. On (the tested default) retains an
empty overlay frame in the runtime scheduling path and stabilized the bike
ride with external OFXR on this setup. Off skips it and may reduce submission
work, but the earlier bike slowdown can return. This is not a universal FPS
gain, and changing it does not alter FOV or frame-generation settings.

The wrapper does not edit those third-party settings, install drivers, start/kill
processes automatically, or expose an OpenXR vibration-output action. The
current 0.1.7 bridge uses its isolated haptic IPC for melee pulses, without
reusing upstream's input slots. Weapon/adaptive-trigger profile translation
and audio-derived grip haptics remain available.

## Driving and riding

The bridge mirrors the stereo audio's low-frequency road/engine rumble to both
grip motors, while retaining the existing shared transient layer. The optional
`scripts/Install-PSVR2BikeShiftPop.ps1 -ModRoot "<CET mods>\DualSense Support"`
patch moves the bike's original L2 resistance/shift/resistance sequence to R2
and its continuous engine trigger profile to L2; cars and weapon
profiles are unchanged. It requires Cyberpunk to be closed and backs up the
third-party CET `init.lua`. Reapply after updating Enhanced DualSense Support.

With **Grab the wheel with the grips** enabled in F10, bring either hand within
the configured grab radius of the animated wheel/handlebar and click that
hand's grip once. The arm latches to Cyberpunk's driving animation, including
its natural finger curl; release the physical grip and it stays latched. Click
that grip again to release the arm back to controller tracking, even after
moving the hand away. Each hand toggles independently. A drawn weapon blocks
the right-hand wheel grab; the left-hand grab remains available for one-handed
steering. Steering continuity and tracking-loss behavior remain unchanged.

Both grip toggling and the corrected right-trigger bike shift have been
confirmed in game. The bike script change takes effect on the next game launch;
an already-running matching Toolkit bridge does not need to restart.

For rapid automatic firearms, the switcher asset includes the optional
`scripts/Install-PSVR2AutoFireGrip.ps1`, which installs
a narrow Enhanced DualSense Support hook with the game closed. It reads the
equipped weapon's live CycleTime, not a name list. `FullAuto` or tech `Charge`
discharge above 5 shots/s receives extra right-hand grip rhythm only when the
stock profile lacks one. It does **not** replace the adaptive R2 mode or force
curve. The earlier `Install-PSVR2FastAutoTrigger.ps1` was retired after a
headset regression: substituting Machine vibration removed resistance feel.
The current installer backs up the third-party `init.lua`; mod updates may
require reapplying it. Category settings remain stock.

### Why the legacy motion watcher is not used

The old bridge interpreted shared float slots 157-160 as sequence, hand,
amplitude and duration. Upstream 0.1.7 assigns those same slots to right B,
left Y, R3 and right-trigger analog. Enabling the old watcher would misinterpret
input as haptic events. The later 0.1.7 bridge restoration added a separate
versioned IPC channel for swing/impact pulses; upstream input slots are never
overwritten. Use this launcher, not the old bridge shortcut that enables its
legacy watcher.

## Validation and headset checklist

Standalone policy tests: `tools/psvr2_input_tests`. Test tap/hold boundaries,
focus cancellation, headset identity, all D-pad directions, shift suppression,
Triangle-click preservation and unchanged non-PSVR2 policy. Build with CMake and
run CTest; the test executable prints its check count.

Before considering a bundle release, test on the headset:

1. Touch Triangle and select all directions; verify no turn/crouch/dash.
2. Release touch and verify turning resumes. Triangle click still switches weapons.
3. Tap and hold Create; verify one correct menu event, including release behavior.
4. Test Triangle+R3 fallback and L3+R3 overlay priority; L3 alone still works.
5. Open/close the VR overlay and lose/regain focus; no delayed menu event.
6. With only the safe bridge running, compare an already-good Power SMG against
   a fast tech automatic and full-auto rifle, then verify semi-auto and charging
   still feel distinct. Check katana swing/impact and audio haptics too.

Successful builds, action suggestions and preflight checks do not establish
physical Sense input or haptic correctness; retain the runtime/headset test gate.

Sources: the old fork's `.okf/fixes/psvr2-triangle-dpad.md` and
`.okf/operations/psvr2-adaptive-triggers.md`; upstream `Utils/SharedSlots.hpp`;
PSVR2Toolkit `vr_motion_haptics.cpp` and `run_bridge.ps1`.
