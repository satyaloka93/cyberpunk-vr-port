---
type: Configuration
title: Upstream 0.1.7 PSVR2 release changes
description: Scope and validation status for isolated haptics and empty-overlay pacing.
tags: [psvr2, release, haptics, pacing]
timestamp: 2026-10-02T17:00:00+09:00
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

An isolated checkout at commit `d57af2c` built the MSVC Release plugin from
the committed source and pinned SDK revisions. DLL SHA-256 is
`04db4378fc95636cd4fa662049f54e9162fd9f2fe341d4792336bf7bb72fd4d2`.
The two local packages are `CyberpunkVRPort-0.1.7-psvr2.1.zip` (SHA-256
`08bf536819ca2dd6d2db088d4f17dfb7b5a98e0b97e4b5a24e6b561f11ab2e06`)
and `CyberpunkVRPort-Switcher.zip` (SHA-256
`678bac4f51087070989a352fd535d81f58ff2c20f23c4c8ab747c823f1f1bc47`).
The switcher passed 30 assertions using the real game ZIP against a temporary
fake game, including install, restore and failure gates. Five grip-only Lua
tests passed. These are build/fixture checks, not runtime compatibility tests.

The user previously confirmed PSVR2 trigger feel and external-OFXR bike
stability in headset runs, but this exact packaged build and F10 toggle have
not received a combined headset test. No live game installation or GitHub
release has been performed. Experimental GPU/HUD diagnostics remain outside
the release commit.
