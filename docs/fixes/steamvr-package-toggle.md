# SteamVR framegen controls

F10 > FRAMEGEN now separates two controls:

- **SteamVR framegen optimizations (restart FG)** changes refresh correction,
  synthesis queue isolation, producer gating and submission pacing. Turn FG OFF,
  select the option, then turn FG ON. No game restart needed for this control.
- **SteamVR presentation queue isolation (restart game)** selects the queue bound
  to the OpenXR session. Its requested and actual states are displayed separately.
  Leave ON for the normal optimized setup. Changing it needs a game restart.

Upstream **Adaptive real-frame pacing** remains independent and live. The two
SteamVR controls apply to Steam Link too; VDXR/non-SteamVR bypass both.

## Persistence and migration

`xr_framegen_steamvr_optimizations` retains the timing/synthesis preference.
`xr_framegen_steamvr_presentation_isolation` stores the new startup preference.
An old INI without the new key inherits its old combined toggle once, preserving
both ON and OFF installs. Explicit presentation isolation wins regardless of key
order; the next settings write saves both independently. Defaults remain ON.

## Transition ownership

UI settings changes only request a transition. SetSettings records an internal,
non-persisted restart serial whenever FG enabled changes. Even quick
OFF/ON clicks between display cycles cannot disappear. Editing optimization
while FG is running stays pending until the next stop; editing while already
OFF applies after GPU work drains.

The XR thread calls Presenter::ApplySteamVrSettings before xrWaitFrame. It polls
and resets the generator only once both generation and last consuming GPU fence
are complete. Until then it shows real frames, suppresses preparation/generation,
and retains prepared endpoints and capture leases. Real drain submissions never
advance the old generator's consumer fence (which could prevent retirement under
load); rejected submissions are not counted as delivered frames.

After safe retirement it clears cadence/history, applies runtime-gated timing,
resets adaptive/producer/submit timing, and primes with a real frame. No blocking
fence wait and no OpenXR session/swapchain recreation. The presentation binding
is never changed by this transition. Refresh monitoring restarts with a fresh
sample and its existing successful-submission/establishment gate.

External OFXR still prevents private presentation and native generation under
the existing safeguards. DLSS-G retains its existing synthesis routing; the
private OFA/FidelityFX synthesis path is not forced onto DLSS-G.

## Validation

Production MSVC Release build and the 16-test Windows framegen suite. Extended
production-Presenter fixture covers outstanding GPU work, pinned prepared images,
fast off/on, first-real history, no new generation while draining, consumer-fence
starvation prevention and rejected-submission accounting. Actual settings and
refresh-worker fixtures cover legacy migration, independent roundtrip, internal
restart serial, dynamic disable/re-enable and non-SteamVR bypass. Existing real-GPU
generator tests cover output and consumer fences blocking reset.

Headset validation remains pending. This changes control/application behavior,
not the algorithms or tuning of the previously tested optimizations. A dynamic
OFF comparison retains the same presentation queue; to compare that part too,
change its separate setting and restart the game. No release or push.
