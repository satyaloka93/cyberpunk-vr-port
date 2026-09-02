---
type: Fix
title: Second-eye load crash (descriptor -1)
description: A silent exit at menu-initiated save loads, traced to a shared second-eye render-resource helper consuming an unallocated one-based descriptor index.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/upstream-0.1.3-psvr2/src/Stereo/NodeDispatch.cpp
tags: [crash, stereo, frame-graph, load-transition, vrcam, open]
timestamp: 2026-09-03T07:45:00+09:00
---

# Which tree this describes

`upstream-0.1.3-psvr2` only, at and after the 0.1.4 merge — though **the crash itself predates that
merge** and was reproduced byte-identically on a pre-merge build (see the table below). Every symbol
named here (`Detour_NodeDispatch`, `CyberpunkVR_VrcamRebindBlindMs`, `CyberpunkVR_RebindTraceCount`,
the `[rebind-trace]` breadcrumb) exists **only** in the post-0.1.2 single-tree layout. None of it is
present on `psvr2-tweaks`, and no capture from that branch is comparable.

# Status

**Open, with a post-prepare exact-sentinel candidate ready for retest.** The invalid descriptor and
unchecked direct consumer are established. Both function-entry guards sampled the descriptor before
the caller's own preparation routine could change it to `-1`. The candidate now intercepts the first
post-prepare read and uses one captured valid value for all downstream uses. Repeated second-load
validation is required.

# The fault, identically at least eight times

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

# Paired dump and log evidence

Two same-session dump/log pairings now exist. Both have the exact fault signature, but their fault
threads ended under different named VRCAM work:

| Fault thread | Final started work on that thread |
|---|---|
| `11492` | `EndRenderTargetsGBuffer` |
| `29676` | `BindLightingGlobalConstants` |

The trace is written immediately before node dispatch and both budgets remained live. This retracts
the earlier single-node conclusion: the common carrier is below multiple frame-graph nodes.

Static disassembly identifies it. Direct caller `Cyberpunk2077.exe+0x774384` reads the first DWORD
of a descriptor object, subtracts one, calls the faulting helper at `+0x1F51C4`, and later reuses the
same scaled index at `+0x774428` and `+0x77446F`. It performs no sentinel/range check. Another caller
at `+0x1F4700` explicitly rejects invalid one-based indices before invoking the same helper. The
missing validation at `+0x774384` is therefore concrete; who leaves the descriptor at `-1` during
the VRCAM re-bind remains unknown.

Native DLSS Neural Rendering was present in some captures, but the RVA, bad address suffix,
`RDX=-1`, `R15`, and caller are byte-for-byte the pre-DLSSNR signature. Neural Rendering did not
create a new crash family. The optional low-spec INI was absent in a matched capture, disproving it
as a prerequisite. See [DLSS 5 Neural Rendering in stereo VR](/architecture/dlss5-neural-rendering.md).

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

# Why both function-entry guards missed

The first repair hooked exact caller `+0x774384`, validated its Cyberpunk 2.31 prologue, and sampled
the descriptor at function entry. One version required direct VRCAM TLS attribution; the next added
a bounded menu/tier/rebind fallback. Both reproduced the identical second-load fault while
`LoadingStage=Spawning player`, `numberOfStateMachines=0`; both detours appear immediately above
`+0x7743C9` in their dumps and recorded zero skips. Evidence:
`20260903-073018-second-load-descriptor-guard-miss` and
`20260903-075909-second-load-descriptor-context-miss`.

The second capture contains every intended context signal, so disassembly was revisited rather than
adding another attribution guess. It shows the descriptor is prepared *after* function entry:

```asm
+0x77439C  mov  rbx,rcx
+0x77439F  call +0x1F405C      ; prepare/update descriptor
+0x7743A4  mov  edx,[rbx]      ; first consumed post-prepare value
...
+0x7743BC  mov  edx,[rbx]      ; second, racy read
+0x7743C4  call +0x1F51C4
```

The entry detour could read a valid value before `+0x1F405C` replaced it with `-1`. Removing context
gates at that same location would not close this TOCTOU race.

The next candidate validates the exact Cyberpunk 2.31 bytes through `+0x7743C3` and intercepts
`+0x7743A4`, after preparation. Its runtime stub captures the index once. A valid value feeds both
the `0xB0` table offset and helper argument, eliminating the second read. For `0xFFFFFFFF`, it
balances preparation with the caller's normal cleanup at `+0x1F7164`, records the skip, and uses the
original epilogue. Eye/rebind context is diagnostic only. Every valid descriptor remains unchanged,
and the intervention is one invalid operation, never a frame-graph node.

# Validation boundary for a repair

Do not broadly skip a named frame-graph node: the failed blind-window experiment already proved that
suppressing producers/cleanup manufactures different missing-resource faults. The candidate remains
scoped to when `+0x774384` receives descriptor index `-1`, records the available calling node, and
allows all valid descriptor operations through unchanged.

It is not validated until repeated in-process save loads complete, the invalid-call counter is
observable, both eyes resume, and no different missing-resource, GPU-hang, or stale-eye failure
replaces the original crash.

If another external dump is needed, use:

```
procdump.exe -accepteula -ma -w -e -n 2 Cyberpunk2077.exe F:\crashdumps
```

**`-e`, not `-t`.** `-t` also fires on normal quit and has already consumed a single-shot capture.

# Not to be confused with

The **GPU hang** family, which is a different failure with a different signature: `gpucrash-*.log`
written, `nvlddmkm` events, **two** `Overlay fence wait timed out` lines, and breadcrumbs showing a
pass *In progress* at `FinalFlushBarriers`. See
[overlay load-transition guard](overlay-load-transition-guard.md). The latest `+0x1F51F5` crash
occurred while that guard was already engaged in full-drain mode, proving the guard does not prevent
this CPU descriptor fault. This crash produces no GPU log, `nvlddmkm` event, or fence timeout.
