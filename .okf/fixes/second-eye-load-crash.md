---
type: Fix
title: Second-eye load crash (descriptor -1)
description: A silent exit at menu-initiated save loads, traced to a frame-graph node dispatched for the second eye consuming an unallocated render-table index. Open; the mechanism is established and four hypotheses are eliminated.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Stereo/NodeDispatch.cpp
tags: [crash, stereo, frame-graph, load-transition, vrcam, open]
timestamp: 2026-08-26T10:30:00+09:00
---

# Which tree this describes

`upstream-0.1.3-psvr2` only, at and after the 0.1.4 merge — though **the crash itself predates that
merge** and was reproduced byte-identically on a pre-merge build (see the table below). Every symbol
named here (`Detour_NodeDispatch`, `CyberpunkVR_VrcamRebindBlindMs`, `CyberpunkVR_RebindTraceCount`,
the `[rebind-trace]` breadcrumb) exists **only** in the post-0.1.2 single-tree layout. None of it is
present on `psvr2-tweaks`, and no capture from that branch is comparable.

# Status

**Open.** The mechanism is established and the window is known. What is still missing is the identity
of the node, which needs one dump captured in the same session as a log.

# The fault, identically five times

```
EXCEPTION 0xC0000005      read at 0x...0AB0
FAULT   Cyberpunk2077.exe+0x1F51F5
Rdx     0x00000000FFFFFFFF        R15  0x000000AFFFFFFF50
caller  Cyberpunk2077.exe+0x7743C9
```

The faulting routine takes a **one-based index** and scales it by `0xB0`:

```asm
mov  r14,[table]          ; global render/format table
dec  edx                  ; index - 1
imul r15,rdx,0B0h         ; * 0xB0 stride
cmp  qword ptr [r15+r14+5C0B60h],0   ; <-- faults
```

With `edx = -1`: `dec` gives `0xFFFFFFFE`, the multiply gives `r15 = 0xAFFFFFFF50`, and the compare
reads unmapped memory. So **-1 is the engine's unallocated sentinel and can never be a valid index
here** — the arithmetic guarantees a wild address. The caller reads that -1 out of the first DWORD
of a descriptor object.

# It is our detour in the call chain, not a bystander

A real `cdb` `kvn` unwind — not the stack **scan** `tools/crash/read_dump.py` performs — gives:

```
00  Cyberpunk2077+0x1F51F5
01  Cyberpunk2077+0x7743C9
03  Cyberpunk2077 (node work)
04  CyberpunkVR_Stereo!cvr::detail::Detour_NodeDispatch+0x958
07  CyberpunkVR_Stereo!cvr::detail::Detour_NodeDispatch+0x958   (SceneDrv re-enters per pass)
0d  kernel32!BaseThreadInitThunk
```

So the -1 is consumed **inside a frame-graph node dispatched for the second eye**, in the window
immediately after a VRCAM component re-bind. The `[rebind-trace]` breadcrumb confirms the window
independently: the log ends mid-trace, ~199 dispatches past the last re-bind, guard still engaged.

**The exe ships no PDB**, so every `Cyberpunk2077!AK::...` symbol in that unwind is the nearest
export plus a six-digit offset and is meaningless. Only the frame *structure* carries information,
and `!analyze -v` blaming `Cyberpunk2077.exe!Unknown` is correct but useless for the same reason.

# Eliminated, each by direct test

| Hypothesis | How it died |
|---|---|
| The 0.1.4 merge | Byte-identical fault on a **pre-merge** build: same RVA, same `RDX`, same caller, same `…0AB0`. The merge also touches no file in the render path |
| Depth submit (`xr_depth_submit`) | Controlled session with it verifiably off (`[DEPTH] depth submit disabled`, 0 `depth=1` submits) — crash recurred unchanged |
| The `AutoSpawnOnTerrain` NULL-argument burst | Gating those indirect draws drove `skipped ExecuteIndirect` to **0** and the crash happened anyway. The burst is a co-symptom, not the carrier |
| `wide_report` / `temporal_report` frames | They appear only in the stack **scan**, never in the real unwind. Both are pure logging: take a mutex, read a census array, format a string. Neither touches a GPU resource or descriptor |

# Two failed fixes, and why

* **Broad blind window** (skip *every* second-eye node for 400 ms after a re-bind). It suppressed the
  target — the 42-call burst went to zero — and then died at the **first** save load with a
  *different* fault, `+0x1375AE` reading `0x7E19C`, a near-null dereference. A node that never runs
  is a node that never allocated or registered what it was going to, so the skip was manufacturing
  unallocated slots of its own. This is the asymmetry the rule at the top of `NodeDispatch.cpp`
  already warns about. Kept behind `CyberpunkVR_VrcamRebindBlindMs = 0`.
* **Narrow gate** on `AutoSpawnOnTerrain`'s indirect draws (`CyberpunkVR_VrcamRebindIndirectMs`,
  400 ms). Correct in itself — the node still runs, so its allocations still happen — and it does
  catch **stale non-NULL** argument pointers the old NULL guard was blind to (`args=…D780`, with
  `skipped ExecuteIndirect` at 0 for the run). It simply does not stop this crash.

The lesson both share: **suppressing a symptom that is cleanly suppressible tells you it was not the
cause.** Two independent gates achieved that and neither fixed anything.

# What the next capture needs

The `[rebind-trace]` breadcrumb logs node RVA, name, depth, scene id and **`tid=`** for second-eye
dispatches after a re-bind, budgeted by count (`CyberpunkVR_RebindTraceCount`, 400).

The budget is shared across threads, and **15 threads dispatch concurrently** — six were mid-dispatch
in the final seven lines of one capture — so the last line names nothing on its own. The faulting
thread id from a dump picks the thread out of the trace; that pairing is the missing step.

Capture with:

```
procdump.exe -accepteula -ma -w -e -n 2 Cyberpunk2077.exe F:\crashdumps
```

**`-e`, not `-t`.** These crashes do raise `0xC0000005`, so `-e` catches them; `-t` fires on a normal
quit as well and has already burned a single-shot capture on an ordinary shutdown. WER `LocalDumps`
is not a substitute — it failed to fire twice, because the exit reaches no unhandled-exception path
Windows can see.

# Not to be confused with

The **GPU hang** family, which is a different failure with a different signature: `gpucrash-*.log`
written, `nvlddmkm` events, **two** `Overlay fence wait timed out` lines, and breadcrumbs showing a
pass *In progress* at `FinalFlushBarriers`. See
[overlay load-transition guard](overlay-load-transition-guard.md). This crash produces **none** of
those — no GPU log, no `nvlddmkm`, no fence timeouts.
