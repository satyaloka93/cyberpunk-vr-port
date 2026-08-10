---
type: Reference
title: CyberpunkVR Port knowledge overview
description: Entry point for current architecture, compatibility, fixes, limitations, and validation guidance.
tags: [getting-started, architecture, compatibility]
timestamp: 2026-08-10T09:08:00+09:00
---

# Scope

This bundle records the current conclusions that are easiest to lose in the larger engineering notes: verified hardware behavior, architectural constraints, tested fixes, known limitations, and safe operating procedures.

# Start here

- [Hardware compatibility](hardware/) contains runtime- and headset-specific findings.
- [Architecture](architecture/) describes invariants and known rendering limitations.
- [Fixes](fixes/) records tested corrections and their regression checks.
- [Operations](operations/) contains dependency, deployment, and documentation-maintenance runbooks.

# Current compatibility work

Start with [PSVR2 through SteamVR/OpenXR](hardware/psvr2-steamvr.md), then follow the topic relevant to the task:

- [VRCAM resolution catalogue](architecture/vrcam-resolution-catalog.md)
- [OpenXR-to-XInput controller pipeline](architecture/controller-input-pipeline.md)
- [PSVR2 Triangle-touch D-pad shifting](fixes/psvr2-triangle-dpad.md)
- [VR HUD layout and one-shot widget adjustments](fixes/center-overlay-scaling.md)
- [HUDitor VR layout workflow](operations/huditor-vr-layout.md)
- [Overlay middle-ground frame pacing](fixes/overlay-middle-ground-pacing.md)
- [Ray-tracing stereo limitations](architecture/ray-tracing-stereo-limitations.md)
- [Runtime dependency compatibility gate](operations/runtime-dependency-compatibility.md)
- [Optional PSVR2 adaptive triggers and haptics](operations/psvr2-adaptive-triggers.md)

For bundle editing and publication, use [OKF maintenance and GitHub links](operations/okf-maintenance.md).
