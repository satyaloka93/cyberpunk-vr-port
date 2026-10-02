---
type: Configuration
title: Upstream 0.1.7 PSVR2 release changes
description: Scope and validation status for isolated haptics and empty-overlay pacing.
tags: [psvr2, release, haptics, pacing]
timestamp: 2026-10-02T22:00:00+09:00
---

# Scope

The upstream 0.1.7-based port has an explicit FOV override and PSVR2 input
mapping on this branch already. The follow-up haptics change publishes swing
and hit-check pulses on `Local\CyberpunkVR_PSVR2_Haptics_017_v1`; it does not
reuse upstream input slots 157–160. The external PSVR2Toolkit v0.3.1 release
owns the matching bridge, Sense driver and launcher. The port ZIP does not
install the driver or embed the bridge.

The F10 STEREO > PERFORMANCE checkbox retains an empty overlay submission and
fence when checked, even when the weapon dot is absent. This is the tested
default pacing behavior for the user's external OFXR bike case. Unchecking
skips that work for a session; the prior ride/dismount slowdown may return.
It is not a frame-generation toggle or a guaranteed FPS improvement.

# Validation boundary

An isolated checkout built the MSVC Release plugin from the committed source
and pinned SDK revisions. The code was unchanged from commit `d57af2c` through
release commit `a839acb`. DLL SHA-256 is
`04db4378fc95636cd4fa662049f54e9162fd9f2fe341d4792336bf7bb72fd4d2`.
Published prerelease [`0.1.7-psvr2.1`](https://github.com/satyaloka93/cyberpunk-vr-port/releases/tag/0.1.7-psvr2.1)
contains `CyberpunkVRPort-0.1.7-psvr2.1.zip` (SHA-256
`567042039099859b5b675c5a5887663c2eecc0195ba19a1c54f9effb0301e7f0`)
and `CyberpunkVRPort-Switcher.zip` (SHA-256
`13231143f0995aac358b4c477d4e2e7bcd9f8b4f947dc99bdafca69713fa1f22`).
The switcher passed 30 assertions using the real game ZIP against a temporary
fake game, including install, restore and failure gates. Five grip-only Lua
tests passed. These are build/fixture checks, not runtime compatibility tests.

With Cyberpunk closed, only the clean-built DLL and matching PDB were deployed;
their installed hashes matched the build and the previous pair was backed up
under the ignored local `dist/live-test-backups/` directory. The live Weapon
CET script matched the package byte-for-byte. The user reports the headset
result is mostly good, with some tuning still needed. The full switcher plan
would have disabled ReShade/HUDitor, so it was not applied; other game files
and settings were left unchanged. Experimental GPU/HUD diagnostics remain
outside the release commit.
