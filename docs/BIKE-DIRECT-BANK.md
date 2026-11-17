# Physical lean adds native bike bank

Experimental option in F10 → Controls → Driving → Bike. Enable **Physical lean
adds bike bank**, then choose Head tilt or Sideways body lean. Handlebars always
retain their steering. Turning this off restores the existing upstream or
head-tilt steering-assist mode. Source default is off; the local test enables it.

The result is a balance target, not a forced mesh rotation: native target angle
plus the physical lean contribution, clamped to the bike's native maximum tilt.
The original tilt-speed/PID/angular-force controller remains responsible for
reaching that angle. Terrain, steering, speed and physics still influence the
actual bike. This does not promise a one-to-one instantaneous chassis rotation.

## Controls

- Head tilt uses measured head roll from the grab-time neutral. Default gain 1
  adds one degree per degree beyond the 3-degree deadzone; a 13-degree tilt adds
  10 degrees. Maximum added bank defaults to 20 degrees, adjustable 0–45.
- Body lean estimates torso angle from sideways neck displacement about a point
  50 cm below the neck, after a 3 cm deadzone. It is not a torso-tracker reading.
- Direct mode records horizontal facing at grab time, so body lean and bars use
  that direction rather than fixed room axes. Looking around does not rotate
  this reference. Release both grips and regrab upright to reset it.
- Both hands give the cleanest isolation: common hand translation cancels in the
  line between them. With one hand, its translation around the virtual handlebar
  pivot can still steer. Turning handlebars while leaning remains possible.
- Only the currently mounted player bike receives the added angle, with a held
  or latched grip, fresh tracking (100 ms), wheel contact, and no menu/scripted
  scene. Native custom tilt requests are left alone. Airborne controls unchanged.
- F10 shows the last native angle, requested addition, resulting target, actual
  bank and gating reason. Values are saved samples while the menu is open.
  `[bike-bank]` logs these at most once per five seconds, only for the player bike.

Saved keys: `xr_bike_direct_bank`, `xr_bike_bank_gain`, `xr_bike_bank_max_deg`.
Existing `xr_bike_head_tilt` selects the source. Its full-lock steering slider has
no effect on direct bank, so it is hidden in this mode. Head deadzone still applies.

## Native evidence and guarded hook

Examined local CP2077 2.5 executable SHA256
`d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4`.
All addresses below are RVAs; image base is 0x140000000.

- BikeObject's SetCustomTargetTilt wrapper at 0x1554AB4 loads object+0x2D0
  (drive model) and writes degrees to model+0xE24. EnableTiltControl is a separate
  control. Neither API is overridden or called by this implementation.
- Target routine 0x21D4570..0x21D4794, 548 bytes: Windows x64 RCX=model,
  XMM1=dt, float return XMM0. It handles a scripted target first, then ground
  contact, speed/input curves, model+0xE64 maximum tilt, and degrees-to-radians.
- Caller 0x2BBBF3C passes the returned angle to the existing balance controller
  and tilt-speed/force computation. It records actual bank at model+0xE28.
  The hook calls the original once, then adds radians to its returned target.
- Init at 0x6EE52C obtains BikeObject via dynamic cast 0x6DF858 and stores it at
  model+0xDE0. We compare that engine-owned pointer to the atomically published
  mounted identity, then validate its +0x2D0 back-pointer. No remembered vehicle
  address is dereferenced on its own.
- Contact helper 0x2BB1BEC counts grounded wheels; it is the same helper the
  original target routine calls. Added bank is suppressed with no contact.
- Install requires the supported PE identity without an unknown-build override,
  plus six exact-byte guards covering entry, caller, owner assignment, degree
  fields and contact query. A mismatch leaves the hook unavailable in F10.

REA/Ghidra decompiled the extracted complete target routine at its original VA.
Its generic raw-binary signature inference is not the ABI source: use original
PE disassembly and the caller for that. External references are unmapped in the
extracted REA session; constants and owner identity were verified in the full PE.
No live native bank behavior has yet been claimed from this analysis.

Artifacts: `build/bike-direct-bank-20261117/` in the workspace root contains
raw-function provenance, REA export, full-PE disassembly, byte-guard verification,
build/test output and deployment backup/receipt. Extracted routine SHA256
`f46a3683ffa17c913544b72faedf0eae28ec7df2970a19820db1f5be4719813e`.
Do not redistribute game binary excerpts as source.

## Validation and next headset check

26 numerical driving cases pass with ASan/UBSan: bank/steer independence across
five headings, either/both hands, both sides, body translation, limits, gain,
recenter, release, stale tracking, original steering/prediction regressions.
16,200 default-mode outputs remain bit-identical to pinned upstream 6f33af62.
Windows x64 Release build passes. Framegen source is unchanged by this work.

For the first headset check, grab both bars upright, ride slowly, hold the bars
neutral and tilt left/right. Then steer while maintaining a tilt. The native
readout/log must show an added target and corresponding actual bank; direction
and handling still require this live validation. No need for a performance A/B.

User confirmed the direct bank works well on 2026-11-17. The subsequent wheelie/
stoppie input work keeps this bank hook and its physical steering math unchanged.
