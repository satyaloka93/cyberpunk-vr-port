---
type: Reference
title: CyberpunkVR Port knowledge overview
description: Entry point for durable architecture and hardware-compatibility knowledge.
tags: [getting-started, architecture, compatibility]
timestamp: 2026-08-09T13:00:00+09:00
---

# Scope

This bundle records verified constraints and measured compatibility facts that are easy to lose in the larger engineering notes.

# Start here

- [Hardware compatibility](/hardware/) contains runtime- and headset-specific findings.
- [Architecture](/architecture/) describes invariants that must survive future changes.
- [Fixes](/fixes/) records tested corrections and their regression checks.
- [Operations](/operations/) contains dependency and deployment runbooks.

# Current compatibility work

The bundle captures [PSVR2 through SteamVR/OpenXR](/hardware/psvr2-steamvr.md), the [VRCAM resolution catalogue invariant](/architecture/vrcam-resolution-catalog.md), the [OpenXR-to-XInput controller pipeline](/architecture/controller-input-pipeline.md), [PSVR2 Triangle-touch D-pad shifting](/fixes/psvr2-triangle-dpad.md), the [runtime dependency compatibility gate](/operations/runtime-dependency-compatibility.md), and the optional [PSVR2 adaptive-trigger/haptic integration](/operations/psvr2-adaptive-triggers.md).
