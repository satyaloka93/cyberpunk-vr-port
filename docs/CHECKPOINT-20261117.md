# Local development checkpoint — 2026-11-17

These commits checkpoint the current 0.2.3 PSVR2 RC1 derivative on
`cp2077-2.5-base-0.2.2`. The directory name `upstream-0.2.2-scope` is historical.
This checkpoint is not a release and has not been pushed.

## Isolated changes

```
644dc0d3 lobby: recognize the recorded short grading signature
65354d47 launcher: expose lobby, VR interface and hand physics switches
48acffd1 haptics: suspend bike road rumble in air and signal wheel contacts
27de1159 driving: add physical bike bank and stabilized wheelie controls
f12fb19a hands: adapt upstream PhysX contact and constrained NPC grips
84b89071 overlay: keep delivery display independent of costly telemetry
```

The scanner fix is already committed as `3f2e958b` and the original bike/grip
backport as `ca513de4`.

## Installed candidate

The currently deployed port DLL has SHA-256
`811e2c4b802c1d2588a5f9e89efa6dad4b000db08c36f7ca0600c13287cb9038`.
It includes the overlay, physics, bike, haptics and startup switches above.
**The lobby signature fix is committed but was excluded from that DLL.** A build
of the complete new HEAD therefore also includes the lobby fix. See
[the lobby audit](LOBBY-COMPATIBILITY.md) before deploying it.

No settings were reset, no new DLL installed and no game process changed while
making these commits. Working-file hashes were unchanged through the split.
The wheelie target hold is deployed awaiting headset validation; contact and
physics tests do not establish actual bike feel or visual mesh conformity.

Validation already completed for the unchanged source: Windows Release builds;
35 sanitizer driving cases; 23 contact-policy and 63 production haptic mixer
checks; physics contact/snapshot/native geometry tests; launcher interaction
and Lua interface fallback checks; 42 lobby fixture checks. No assertion that
all intermediate commits received separate full Windows builds.

## Companion Toolkit dependency

The installed Toolkit bridge has SHA-256
`949f5c2f0489683101aab1b6042d6ed014fad0f8ed18d9ed839720ea0ee345fc`.
Suppressing its independent Machine-derived bike rhythm requires its companion
implementation of optional mailbox slots 171/172 and a 150 ms mute lease.
OpenXR contact haptics do not require the bridge. Old bridges safely ignore the
extension but continue their own rhythm.

Toolkit lives in a separate repository. Its matching source tree also contains
uncommitted TOW2 work and earlier protocol changes. This Cyberpunk checkpoint
does not commit or claim a clean Toolkit release. The exact bike-only patch
against its preserved pre-edit source is saved under
`patches/psvr2toolkit-bike-road-20261117.patch`, with pre/post file hashes in
`patches/psvr2toolkit-bike-road-20261117-preimages.json`. Do not apply it blindly
to another Toolkit revision. The patch documents the already deployed companion;
it is not an installer and does not bundle driver binaries.

## Deliberately unchanged

Unrelated LUT/image agent files, installer/dependency artifacts and historical
root-tree changes remain outside these source commits. No new radio binding:
installed and October 26 merged mappings both assign D-pad left to headlights
while driving, pocket radio on foot, and vehicle radio to Right Shoulder/R.
The latest bike work did not alter these mappings.
