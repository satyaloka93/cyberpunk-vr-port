---
type: Playbook
title: Host machine stability as a crash confound
description: This development machine has a hardware-level fault pattern independent of the port. Read it before attributing any GPU hang to a code change, and read the retractions before citing it as an excuse.
tags: [crash, diagnostics, hardware, gpu, confound]
timestamp: 2026-08-27T09:35:00+09:00
---

# Why this page exists

It cuts **both** ways, and both failures have happened here.

* Attributing a machine-level GPU hang to a code change sends work after a fix that cannot exist.
* Attributing a genuine port defect to "the machine" retires a real bug. Every crash family
  currently tracked was at some point waved at with this page's argument and turned out to be
  real code.

Default to investigating the port. Reach for this page only when the markers below are present.

# What survives scrutiny

* **GPU engine timeouts continuously from 2026-07-23 to now, across two driver versions.**
  `nvlddmkm` events and `LiveKernelEvent 141` (`VIDEO_ENGINE_TIMEOUT_DETECTED`).
* **`vrserver.exe` (SteamVR) crashing seven times in eight days**, including heap corruption
  (`0xc0000374`) in `ntdll` and `0xc0000005` in `libcrossipc.dll`. The OpenXR runtime host being
  unstable is a confound for anything measured through it.
* **`0x139` `KERNEL_SECURITY_CHECK_FAILURE` subtype 3** (LIST_ENTRY corruption) on 07-23, 07-24,
  08-10 and 08-12 — and **stopping at 08-12**, the day the current driver went in. That update
  appears to have fixed the kernel corruption while leaving the GPU timeouts untouched.

Untried hardware-side tests: Memtest86 on the 2×32 GB DDR5 (running 5000 MT/s on AM5), a GPU
power-limit or core offset, and reducing per-eye resolution below 3072×3072 to test margin.

# Retracted — do not cite these again

| Claim | Why it is wrong |
|---|---|
| "Other titles crash on this machine too" (Stalker 2, The Outer Worlds 2, SHf dumps in `CrashDumps`) | Separate issues, already resolved by the user. Withdrawn at their correction |
| "The 07-22 driver correlates with the 07-23 first watchdog event" | `Win32_PnPSignedDriver.DriverDate` is the driver's **release** date. `oem13.inf` and the NVIDIA installer record show 32.0.16.1088 was **installed 2026-08-12**, three weeks *after* symptoms began. This removes driver version as an explanation rather than supporting it |
| "There was a blue screen" | There was not. The WER entry referenced `081226-7718-01.dmp`, an **old queued report** from 08-12, not a live event. Check the date inside a queued report before treating it as this session's |

# Separating a machine hang from a port defect

Do not use "it hung" as the discriminator — check the markers:

| | Machine/GPU hang | Port defect |
|---|---|---|
| `gpucrash-*.log` written | yes | no |
| `nvlddmkm` event at the crash **minute** | yes | no |
| `Overlay fence wait timed out` | two lines | none |
| Breadcrumbs | a pass *In progress* at `FinalFlushBarriers` | unavailable or irrelevant |
| Device Removed Reason | `0x887A0006` | varies — `0x887A0001` seen with **no** driver event at all |

The last row is the trap. A `DEVICE_HUNG` reason code with **no `nvlddmkm` event at that time** is
not this page's problem; see the settings-change family in
[second-eye load crash](../fixes/second-eye-load-crash.md) for how the families separate.

**Match the timestamp to the minute.** `nvlddmkm` entries from hours earlier in the same day belong
to a different crash and have twice been misread as belonging to the one under investigation.

# Related

[Overlay load-transition guard](../fixes/overlay-load-transition-guard.md),
[second-eye load crash](../fixes/second-eye-load-crash.md).
