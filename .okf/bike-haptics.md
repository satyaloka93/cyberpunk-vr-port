---
type: Bug Fix
title: Bike grip road haptics and right-trigger shift pop
description: Mirror low-frequency audio to both PS VR2 grips and move the bike's complete resistance/shift sequence from L2 to R2.
tags: [psvr2, haptics, cyberpunk, motorcycle, triggers, bridge]
timestamp: 2026-10-04T11:20:00+09:00
---

# Evidence and behavior

The active Enhanced DualSense Support bike mode is `LTResistanceRTMachine`: stock L2 receives speed-dependent Resistance; stock R2 receives the RPM-dependent Machine effect. Its selected `Twitching` gear-change mode overwrites both trigger profiles with Machine pulses. Version 1 merely suppressed the left shift pulse; R2 was unchanged. The user confirmed left grip feedback but reported the clutch effect disappeared. Source inspection explains why that was insufficient: stock L2 transitions Resistance -> Machine -> Resistance, whereas stock R2 transitions Machine -> Machine -> Machine. The engine and shift use the same strength (2) at the current settings, so R2 mainly changes frequency.

Version 2 swaps both baseline trigger types and parameters for bikes before collision and shift processing. The existing bike-only shift wrapper then preserves the new L2 engine baseline. This transfers the exact old L2 resistance/shift/resistance sequence to R2 without inventing new pulse strength or timing. Cars are unchanged. The installer upgrades version 1, supports fresh installation, refuses live-game modification, and saves a timestamped backup.

The PSVR2Toolkit bridge WASAPI capture previously filtered left and right audio independently and sent each channel's bass to its corresponding grip. Panning could therefore leave the left grip quiet during a right-biased engine/road sound. The applied bridge patch selects the stronger signed low-frequency channel per sample and sends it to both grip outputs. It deliberately avoids averaging, which can cancel out-of-phase stereo bass. Existing transient and semantic haptics are unchanged.

# Validation boundary

The bridge implementation is committed in PSVR2Toolkit as `c080162` on `cyberpunk-dsx-bridge` (`projects/psvr2_toolkit_dsx_bridge/audio_haptics_capture.cpp` and `audio_haptics_policy.h`). It is built and deployed; left grip feedback is user-confirmed. The bridge was running during the follow-up investigation and needs no restart for version 2 of the CET script.

The revised installer passed fresh-install, version-1 upgrade, CheckOnly non-mutation, and repeat-install checks against copies of the actual CET script. A Lua regression compiles the complete patched script and executes its actual routing blocks with the installed EDS mode and Twitching functions: R2 matches original L2 before/during/after the shift, L2 retains the engine profile, and car behavior stays unchanged. Deployed after Cyberpunk closed at 2026-10-04 11:23 KST; the same regression passed against the live installed file. Rollback: `init.lua.before-psvr2-bike-shift-v2-20261004-112330-522.bak` alongside the installed file. The user subsequently confirmed version 2 works: the clutch effect is now felt on R2.
