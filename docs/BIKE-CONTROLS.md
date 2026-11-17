# Bike controls

The maintained 0.2.3 fork offers head-tilt steering and the earlier upstream
0.2.45 handlebar/body-lean mode. Neither requires the newer vehicle minimap.
Existing car steering, vehicle camera offsets and road haptics are retained.

## Direct physical bank (experimental)

**Physical lean adds bike bank** sends head tilt or body lean to the native bike
balance target while handlebars steer independently. See [direct bank implementation
and test guide](BIKE-DIRECT-BANK.md). The steering-assist modes below apply with
this option OFF. Source default remains upstream steering; the local direct-bank
test opts in with head tilt, gain 1.0 and a 20-degree added-angle cap.

## Handlebars + head tilt assist (optional)

F10 → Controls → Driving → Bike → **Bike steering mode → Handlebars + head tilt assist**.

- Handlebars retain their normal steering. Tilting an ear toward a shoulder adds
  steering toward that side, replacing upstream's sideways body-translation term.
- This is **steering assistance, not an independent sideways bank command**.
  Upstream also sends its body lean to the steering axis. The game's vehicle
  input mappings expose `TurnX` (left/right) and `LeanFB` (forward/back), not a
  separate sideways lean action. Bike curve data includes `SpeedToTilt` and
  `InputToTilt`; do not describe this input assist as directly setting bank angle.
- Default full added steering at 25 degrees, with a 3 degree head-tilt deadzone.
  A 14-degree tilt adds 50% steering at these defaults. The sum of handlebars
  and tilt is clamped to full lock; opposite contributions cancel.
- Grab at least one handlebar to activate. Release both and regrab upright to
  recapture neutral. Recenter and mode changes also reset the references.
- Looking sideways or up/down is not a tilt command. Near vertical head poses
  disable tilt assistance while valid handlebar steering remains available.
  Missing/stale head tracking returns zero input until tracking recovers.
- Hand prediction and the general deadzone affect the bars only. Tilt retains
  its own deadzone. Body translation does not add a second lean contribution.

Saved keys remain `xr_bike_head_tilt=1`, `xr_bike_tilt_lock_deg=25`,
`xr_bike_tilt_dead_deg=3`. This corrects the earlier implementation that bypassed
handlebar steering in head-tilt mode. Default mode remains upstream (value 0).

## Physical wheelies and stoppies

F10 → Controls → Driving → Bike → **Physical wheelies / stoppies**. Source
setting defaults off; the local test opts in. This uses the game's existing
`LeanFB`/`RockFB` actions through left-stick Y, without a new native hook.

- Grab the bars with your torso in a comfortable neutral position.
- Lean your torso **back and accelerate** for a wheelie.
- Lean your torso **forward and brake** for a stoppie (endo).
- Default full lean input at **15 cm** forward/back displacement, with a **3 cm**
  deadzone. These are adjustable separately from side banking and bar steering.
- Head orientation alone is compensated with the existing estimated neck model.
  Nodding/looking up or down is not intended as a stunt gesture. Individual neck
  motion differs; increase the deadzone if ordinary posture movement activates it.
- Both hands recommended initially. One attached hand also enables it; as with
  bar steering, translating one controller can turn it about its virtual pivot.
- Explicit stick Y overrides physical lean, even in the opposite direction.
  Releasing both grips, stale tracking, menus, scripted scenes and weapon/gun
  throttle-trim mode stop the physical input. Regrab upright to reset centre.
- Speed, traction, engine/brake force and balancing remain the game's decision.
  No automatic throttle/brake press, pitch transform, extra torque, forced wheel
  lift, fake impact or stunt sound is added. In the air this native lean axis also
  participates in the game's normal pitch controls.

CDPR documents these controls in the [Update 2.1 patch notes](https://www.cyberpunk.net/en/news/49597/update-2-1-patch-notes).
Installed `inputContexts.xml` VehicleDriveBase maps `LeanFB` to
`LeftY_Axis_Vehicle` and `RockFB` to `LeftY_Axis_Vehicle_RockFB`.
`inputUserMappings.xml` maps both to `IK_Pad_LeftAxisY`; forward is +1, back -1.
Neither XML is modified. Input is injected after weapon throttle-trim processing
and excluded from that mode; sprint detection still uses the original stick.
The normal gamepad inner/outer deadzone encoding is reused without changing user
settings. No physical contribution means the existing pad value stays exact.

Native vehicle simulation and sounds continue normally. Existing OpenXR road
haptics, Toolkit routing, EDS engine/gear/collision effects and audio settings are
preserved. This preserves the existing feedback routes; it does not establish
that every stock gamepad rumble effect has an OpenXR equivalent or add a new
wheelie-specific haptic profile. Felt feedback still requires a headset test.

Saved keys: `xr_bike_stunts`, `xr_bike_stunt_full_m`, `xr_bike_stunt_dead_m`.
F10 shows saved fore/aft distance, requested lean and outgoing stick Y; a bounded
`[bike-stunts]` log records those values at most once per five seconds.

29 driving cases pass with ASan/UBSan, including longitudinal movement at five
headings, head-turn rejection, lateral isolation, stale/tracking/recenter/grip/
vehicle lifecycle, explicit stick priority, deadzones and 30–120 Hz input.
Windows Release build passes. Local artifacts: `build/bike-native-stunts-20261117`.

## Grip ownership

**Toggle grip** defaults off in source. Hold a grip near a handlebar to keep that
hand attached. With toggle enabled, squeeze once to attach, release your fingers,
then squeeze again to detach. Each hand works independently. Losing tracking,
weapon ownership or leaving the vehicle releases the appropriate hand. The local
user's existing toggle setting is preserved by the head-tilt deployment.

## Upstream: handlebars + body lean (default)

Select **Upstream: handlebars + body lean (default)** for upstream 0.2.45 steering:

- Handlebar full lock defaults to 40 degrees (15–60 adjustable). Push one hand
  forward and pull the other back. One hand uses a virtual pivot; two hands use
  their connecting line.
- Body lean defaults to 0.50. A 15 cm sideways neck displacement adds half
  steering lock, with a 3 cm deadzone. Neutral is captured at the grab.
- This mode has recenter-axis dependence and one-hand translation coupling;
  a torso movement that carries one controller can add opposing/excess steering.
  Head-tilt assist replaces body translation only; handlebars still contribute.

Saved keys: `xr_bike_grip_toggle`, `xr_bike_bar_lock_deg`, `xr_bike_body_lean`.

## Validation

24 numerical driving cases pass under ASan/UBSan, including head tilt across
five yaw headings, three pitch angles, either/both hands and both turn directions;
body-motion rejection and preserved handlebar control, deadzone/full lock, neutral/recenter, tracking
loss and recovery, near-vertical guard, live settings and mode switches.
Production grip-latch/reset checks and Windows Release build pass. These are
code-level checks; in-headset comfort and steering feel remain to be tested.

## Mode defaults and hover instructions — 2026-11-17

Upstream is now the source and local-install default. Its baseline settings are
40-degree bar full lock, 0.50 body lean, 0-degree steering deadzone and prediction
off. Existing bike grip-toggle preference is separate and preserved; hold/toggle
does not change steering math while a hand is attached. The mode selector and
individual sliders explain posture/recentering, one/two-hand behavior, direction
of sensitivity adjustment and the independent head-tilt deadzone.

The upstream bike branch uses the pinned 6f33af62 formula, including its exact
one-hand pivot expression. Cars retain their existing implementation. Comparison
against independently extracted upstream headers replays grips, hand changes,
tracking loss, recentering, yaw, translation, bar motion and optional prediction.
The reference artifacts are in root `build/bike-steering-modes-20261117/`.

## Wheelie balance, contact steering and seat correction — 2026-11-17

The earlier native bank and fore/aft stunt build was user-confirmed. This candidate
adds independent, saved options under F10 → Controls → Bike. New options default
OFF in source; the local test install enables them.

- **Hands balance a wheelie**: torso rearward lean plus throttle initiates it.
  Both hands must stay attached and tracked through liftoff. After front contact
  is lost, average hand height relative to the estimated neck controls native
  pitch input: lift both hands for more rearward input, lower for forward input.
  The last grounded hand height is retained; launch input fades over180ms to
  neutral so it cannot pin the bike rearward. Full travel applies once per
  direction, rather than doubling the input gain. Default full
  hand travel 12 cm, 1.5 cm deadzone. Body translation is not counted twice.
  Forward body input can override rearward hand input; explicit stick Y takes
  priority. Hand loss, recenter, confirmed landing and vehicle changes reset the phase.
  A brief front-contact flicker under30ms does not erase hand balance.
- **Front lifted: lean steering only**: physical bars contribute zero while
  native front-wheel contact is absent. In direct-bank mode physical sideways
  lean still feeds the native bank controller. In the original steering mode,
  only its sideways lean term feeds steering. Front touchdown restores the
  continuously tracked bars. Stoppies keep bars: rear-wheel loss is not mistaken
  for front-wheel loss. When fully airborne, this adds no new bank torque; native
  airborne controls remain. Missing/stale/unsupported contact data cannot be
  interpreted as a wheelie. Source default OFF preserves upstream behavior.
- **Wheelie angle assist**: default test target 35° relative to road pitch just
  before liftoff. Smoothly eases rearward input over the last 12°, reduces throttle
  over the last 6°, and adds forward input above the limit. Rising pitch predicts
  120 ms ahead, bounded to 18°. Both wheels grounded always passes the launch input and throttle unchanged.
  It never adds throttle, presses brake, teleports
  the bike or synthesizes haptic effects. This is a soft aid, not a hard physical
  cap; collisions/momentum can still flip the bike. Stale data and airborne or
  rear-lift conditions pass through. Explicit stick retains priority over body/
  hands, but the enabled angle assist still limits the final native request.
- **Stable lateral bike seat (experimental)**: removes native lateral camera
  animation relative to the current render-owner frame, using a grounded upright
  reference. The current render transform is used, never a delayed physics
  position. Physical head translation, stereo IPD, vertical/fore-aft native
  movement and saved fit offsets remain. Applies before the shared MAIN/body
  anchor publication so both eyes and the body use the same base. Reset on
  vehicle/camera/owner change, dismount, weapon/scene exclusion or option off.
  It is a candidate for the reported left-bank viewpoint slide; the cause has
  not yet been confirmed with a runtime native-camera trace. Log corrections
  are bounded to one line per five seconds.

**Full forward/back lean (cm) is sensitivity, not maximum pitch.** Raising it
above 15 cm needs more torso travel for the same native input. Use the separate
angle assist to reduce backflips instead. The local test retains full15/dead3 cm,
body bank source, gain2, maximum added bank20°, grip toggle and saved camera fit.

Keys: `xr_bike_hand_balance`, `xr_bike_hand_full_m`,
`xr_bike_air_lean_only`, `xr_bike_wheelie_assist`,
`xr_bike_wheelie_limit_deg`, `xr_bike_stable_seat`.

Native provenance (installed 2.5 exe SHA256
`d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4`):

- `2BBADF0` reads chassis orientation through model+60 → physical+2D8 → state+160.
  REA/Ghidra orientation analysis corroborated with full Win64 disassembly and
  native quaternion normalization/axis helpers. Game forward is +Y, up +Z.
- `6EA8BC` exposes wheels from model+5E0, count+C20, stride190.
  `AreFrontWheelsCentered` wrapper `2B98AD8` checks the first entry's steering
  angle (+104); for the supported two-wheel bike layout it is the front wheel.
- `53BFF0` tests contact as wheel+B4 <= 0. `IsPerformingAWheelieOrEndo` wrapper
  `21CCBA0` also uses that predicate through `53BFB4`. The existing bank-contact
  helper's +168 > .001 is a load threshold, not the per-wheel touch predicate.
- New consumers require count==2, successful reads, current mounted owner and age
  <=100 ms. Contact matches the native ordered +B4<=0 predicate, including
  its handling of unordered/no-hit values. Pitch requires a valid finite pose;
  contact haptics and hand balance do not depend on that independent pose.
  Independent byte guards protect orientation, wheel range, first-front
  access and the contact predicate. These are static layout findings; live
  wheelie/endo response still requires a headset test.

Validation: 33 ASan/UBSan driving cases pass; original upstream steering replay
16200 samples bit-identical; MSVC Release DLL builds. Mirrored seat correction is
numerically tested at large world coordinates with a moving render owner.
All46 Framegen files match the prior overlay-isolation reference;157 installed
haptic Lua/INI/JSON files are preserved. Artifact folder:
`build/bike-limit-seat-20261117` in the root repository.

## Wheel contact haptics — 2026-11-17

Driving road/engine rumble is suspended while native FRONT wheel contact is
absent, including wheelies and fully airborne jumps. A confirmed liftoff produces
a short75ms pulse (base strength .55); touchdown produces a stronger125ms pulse
(.80, or .90 for simultaneous two-wheel landing). Rear-wheel transitions also
produce contact feedback; a stoppie with the front grounded retains road rumble.

This uses the existing road mixer and active OpenXR/Toolkit actuator route.
Driving strength, master scale, OpenXR legacy gain and per-hand grip requirement
apply. Contact envelopes outrank road audio and light belt/menu ticks; shots,
blocks and parries keep priority. Gear-shift adaptive triggers remain independent.
The coordinated Toolkit update also mutes EDS vehicle Machine/Galloping grip
rhythm while the front is up and no weapon is equipped. This matters even with
full-game audio gain zero. Adaptive trigger commands, shift effects and explicit
lift/landing pulses stay independent. Optional v1 mailbox slots171/172 publish a
mute flag and separate sequence; the bridge lease expires after150ms without
an update. Old publishers/bridges ignore the extension. Toolkit full-game audio
is separate and is not wheel-contact filtered (the local bridge gain is zero).
Existing already-dispatched road pulses finish their short45ms OpenXR/90ms Toolkit
tails; no blanket actuator stop is sent that could cut a gun/trigger effect.

Front loss suppresses new road pulses immediately. Contact events and restoration
require30ms stable native physics samples, not repeated XR polls. Each event type
has a150ms cooldown; a short wheelie can still have separate liftoff and landing.
Initialization, vehicle changes, stale/unsupported data, menus and session clears
rebaseline without synthesizing transitions. No new INI setting is required;
Toolkit users need the coordinated bridge update to pause EDS grip rhythm too. Native observation remains enabled with driving haptics even if the
physical lean/hand-balance options are off.

Validation:21 contact-policy checks under ASan/UBSan and MSVC;55 checks against
production Haptics.cpp verify both output routes, wheelie silence, stronger
landing, rumble resumption, gain/grip gates and existing weapon/menu behavior.
Release DLL builds. Artifact: `build/bike-contact-haptics-20261117` at repository
root. In-headset pulse feel awaits user testing.

## Wheelie repair candidate after failed feedback test

Last run:51 periodic stunt samples,14 reported invalid motion/contact, none
captured hand-balance/angle-assist active. Logs detected contact events but did
not report mixer acceptance. The exact reason for each invalid sample was not
recorded; do not claim a proven nonfinite native sentinel in that run. New bounded
state logs include raw gaps, count, pose validity, age and held-hand validity.
Native initializer uses FLT_MAX; ordered native contact predicate also handles
infinity/NaN as no-contact. Consumers now mirror it and separate contact from pose.
Input time is captured AFTER the native snapshot to avoid false future samples.

Verified local bridge provenance: previous build-vs output7d214ed4... matches the
installed bridge before the known one-byte8ms polling patch (installed a4162627...).
Updated that same Toolkit source, preserving its Machine release policy, TOW2
path and native trigger translation. Standalone bridge polling remains8ms for
Cyberpunk and25ms for TOW2. No driver update is needed.

33 driving cases,23 contact-policy checks,63 real port-mixer checks and five
Toolkit test executables pass. Toolkit test runs the actual threaded PCM mixer
with a fake driver sink: grounded rhythm, air silence, surviving contact pulse,
and restored grounded rhythm. Physical wheelie controllability and pulse feel
still require the headset test. Artifacts:root build/bike-wheelie-repair-20261117.

## Reset lean and stunt defaults

F10 → Controls → Driving → Bike → **Reset lean / wheelie / stoppie defaults**
restores all 19 bike steering, bank and stunt settings from the declared defaults
and saves immediately. This selects upstream handlebars + body lean and turns
off experimental bank, physical stunts, hand balance, angle assist, front-lift
steering override and stable seat.

Tuning returns to 40° bars, 0.5 body lean, 1.0 bank gain / 20° maximum bank,
25° head tilt / 3° deadzone, 15 cm body travel / 3 cm deadzone, 12 cm hand travel
and a 35° wheelie limit. Grip toggle, grab radius, shared/car steering, camera fit
and haptics stay unchanged. Regrab both bars upright to establish neutral.

## Simplified controls and wheelie stabilization — 2026-11-17

The Bike panel now uses two selectors:

- **Lean mode:** Upstream: handlebars + body lean; Head-tilt steering assist;
  Body lean banking; Head-tilt banking. This maps the existing settings exactly.
- **Wheelie control** (when Wheelies / stoppies is on): Body lean + throttle;
  Hands + throttle; Stabilized hands + throttle.

Stabilized control includes both target-angle balance and the upper-angle aid.
No separate stabilization checkbox is required. It shows one target-angle slider;
travel/deadzones, bank strength, upper-angle tuning, front-lift steering and stable
viewpoint are under **Advanced bike tuning**. Measurements are under **Bike input
readout**. User-facing experimental labels have been removed. Reset defaults
covers all 19 lean/stunt fields, including the new hold settings; existing saved
choices are preserved until the user changes a control.

Stabilization requires both grips attached, valid tracking and native attitude,
a grounded road/hand reference, deliberate backward initiation and some throttle.
After front contact has been absent for 30 ms and pitch exceeds 8° above the road,
it smoothly catches the current pitch and moves toward the selected target
(default 25°). Hand displacement trims the target ±12°, using existing hand travel
and a 1.5 cm deadzone. The target slews at at most 30°/s and remains at least 8°
below the upper-angle setting. The native input correction uses proportional,
pitch-rate damping and bounded integral terms, with anti-windup and output slew.
It never adds throttle or brake and never writes the vehicle transform.

Throttle release, brake, deliberate forward body lean, hand/tracking loss,
manual stick input, rear-wheel lift, vehicle/origin change and confirmed landing
end the hold. A fresh grounded initiation is needed to rearm; enabling it while
already in the air does not invent a ground reference. A front-contact flicker
under 30 ms is tolerated. Existing manual hand control and body-only stunts remain
available. Stoppies keep their existing forward-lean/brake path.

New saved settings: `xr_bike_wheelie_hold` (default 0) and
`xr_bike_wheelie_hold_deg` (default 25, range 5–45). Stabilized mode enables hand
control and angle limiting together. Even a legacy INI with its separate angle
aid disabled receives angle limiting while stabilized mode is selected.

35 driving test cases pass under ASan/UBSan, including cancellation, target trim,
contact debounce, repeated-poll invariance and response at 30/60/90/144 Hz. The
closed-loop test uses a simple synthetic pitch plant; it does not establish the
correct gains for every actual Cyberpunk bike. Headset behavior still needs a test.
Artifacts: root `build/bike-balance-hold-20261117`.

## Banking turn/recovery update — 2026-11-17

Body/head-tilt banking now also turns through native steering while both wheels
are grounded. In Advanced bike tuning, **Lean turn strength** controls that
contribution (existing saved body-lean value, default0.50; zero means bank only).
A20degree bank at0.50 contributes25% steering; the contribution is capped35%.
Handlebars add to it. Stronger same-side native bank and requested lean are
coordinated rather than added twice. The90degree/s lean ramp eases upright
recovery, without synthesizing an opposite steering pulse.

Banking mode also recovers the original two-hand steering reference after a
one-hand interval and treats a tracked-hand dropout as suspension. Car and
upstream-mode steering remain unchanged. See BIKE-DIRECT-BANK.md for tests,
contact limitations and the difference between turning and sideways movement.

## Neutral correction — 2026-11-18

In either physical-bank mode, the first complete two-hand grip sets the bar and
lean neutral together. Sit naturally and grab both bars. The lean axes come from
the horizontal line between the hands, so looking aside during the grab does
not make forward/back torso movement steer sideways. Subsequent one-hand/pair
transitions preserve the existing turn and pair-reference recovery. Release both
and regrab to establish a new neutral. Cars and upstream steering mode retain
their original calibration behavior.

Lean turn strength now uses physical lean after its deadzone, before bank gain
and angle cap. Bank strength changes the bike angle without multiplying the
extra steering. No saved setting values changed. Both neutral regression cases
failed on the previous implementation and pass now; all42 driving cases pass
ASan/UBSan. Headset handling remains to be validated.
