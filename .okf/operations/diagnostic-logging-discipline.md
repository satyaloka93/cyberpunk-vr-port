---
type: Playbook
title: Diagnostic logging discipline
description: Throttle on novelty, never on a session-wide counter, and never discard the field that discriminates between callers. Both mistakes have already blinded this project at the exact moment a crash happened.
tags: [diagnostics, logging, crash, instrumentation]
timestamp: 2026-08-27T09:30:00+09:00
---

# The rule

1. **Throttle on quiet, not on a count.** An event is interesting because it arrives after a lull,
   not because it is the seventh of its kind.
2. **Never discard the discriminator.** If several call sites raise the same event, the caller's
   identity is the whole value of the line. Throttle the *volume*, keep the *identity*.
3. **Log the rare thing unconditionally.** A branch that fires a handful of times a session costs
   nothing to log and is exactly what a post-mortem needs.
4. **A suppression is a decision about which future question you can answer.** Write down which
   question you are giving up, or do not add the suppression.

# Why: three suppressions, one blind spot

Each was individually defensible. Together they blinded a `DEVICE_HUNG` at a graphics-setting
change so completely that the log could not say which of **five** call sites armed the guard.

| Suppression | Intent | What it actually cost |
|---|---|---|
| `OverlayArmLoadGuard` did `(void)reason` | One caller fires often; a line per arm would flood | The only field that distinguishes a swapchain rebuild from a PSO burst |
| `pso_note_creation` used `arms <= 12 \|\| arms % 100 == 0` | Keep startup warm-up quiet | Logged twelve boring startup arms, then went silent for eighty-seven — including the arm at two separate crashes |
| `ResizeBuffers` logged only when the forced size differed | Avoid noise when the override changes nothing | A settings change asks for the size already forced, so the one call that releases every overlay render target left **no trace at all** |

The fixes were mechanical: store the reason and print it on the **engage edge** (one line per
window, not per arm); throttle PSO bursts on `>= 2000 ms since the previous arm`, which keeps
startup quiet for free while always printing the post-quiet burst; log both `ResizeBuffers` and
`ResizeBuffers1` unconditionally.

# This was learned twice

The identical mistake had already been made and written up:

> `[EI-DIAG]` NULL-argument logging was throttled to `n == 1 || n % 500`. In **four consecutive
> load crashes** the line appeared exactly once, as the final line of the log, so the burst shape
> was never visible — while the measurement that originally justified the guard had seen eight
> calls with `argOff` marching 0, 20, 40 … 140.

It was widened to the first 64 occurrences plus the node name and RVA, and that widening is what
made [second-eye load crash](../fixes/second-eye-load-crash.md) legible at all. The lesson did not
generalise at the time, and the same shape recurred in a different file three days later. Hence
this page.

# Corollary: absence is only evidence if the line is unthrottled

A missing log line proves nothing until you have checked its throttle. Recorded examples on this
project:

* The fence-timeout line is capped at **four occurrences per session in code**, so its absence late
  in a long log means nothing.
* `[pso] burst` under the old rule was absent for arms #13–#99, which is most of a session.

Before concluding "X did not happen", grep the emitter for its rate limit.

# Related

[Second-eye load crash](../fixes/second-eye-load-crash.md) — where the widened `[EI-DIAG]` and the
`tid=` breadcrumb came from, and what evidence is still missing.
[Overlay load-transition guard](../fixes/overlay-load-transition-guard.md) — the five call sites
whose identity the `armed by:` field now records.
