# OFXR + Cyberpunk — test strategy

Written 2026-09-08 after **five single-question test runs**, because that was the wrong way to
work and the owner said so. Companion to [WORKLOG-OFXR.md](WORKLOG-OFXR.md).

---

## How I was failing

Each run answered exactly one yes/no question, and each answer produced a new question:

| run | question | answer | why it didn't finish the job |
|---|---|---|---|
| 1 | is the launch crash a race? | no — an `int 3` | fine; this one was necessary |
| 2 | are empty end-frames the flashing? | no, EMPTY 1 | the count I needed was a *different* counter |
| 3 | which layer kind, then? | QUAD 85% | cumulative — can't tell menus from gameplay |
| 4 | is menu mode really on? | 4 transitions only | sampled from the wrong thread at the wrong time |
| 5 | which term made it true? | `menuMode`, 5310 | still cumulative, so still ambiguous |

Two mistakes, both mine:

1. **Cumulative totals for a question about time structure.** "90% of the session was menus and
   loading" and "gameplay submits quads" produce the *identical* total. No number of reruns
   distinguishes them; only a per-window number does.
2. **One probe per build.** Every probe was cheap — a counter and a log line. There was no reason
   to add them one at a time, and doing so put the cost of my uncertainty onto the owner's evenings.

**Rule from here: a build adds every probe that could discriminate between the live hypotheses,
not the one for the current favourite.** Instrumentation is cheap; a test run is not.

---

## What is established (do not re-test)

* Launch crash was ReShade's OpenXR layer raising `int 3` in `dxgi.dll` inside SteamVR's
  D3D11-on-12 setup. Fixed per-process. **Closed.**
* The frame loop is correct: `PAIRED 1:1`, `EMPTY` 1–3 per session. **Empty frames are not the
  problem.** Dead hypothesis, twice confirmed.
* Null handles are not the problem: `BAD 0`, every session.
* Of ~6000 layered ends, only ~600–1000 carry a **projection**; the rest are the **menu quad**,
  and the quad branch is entered because `GetMenuMode() != 0` (`rect 0, menuMode 5310`).
* Menu mode changes **4 times a session** — two long stretches, not toggling.
* OFXR cannot pair a quad: `continuity_reset` ~4338, `presenter_transition` ~546.
* Cyberpunk presents 17–26 fps while we submit ~57/s (`perPresent` 2.2–3.3), and the runtime's
  display period **oscillates between 45 and 90 Hz**.

---

## The one live question

**Does the quad branch dominate during GAMEPLAY, or was the session mostly menus and loading?**

| | if menus/loading | if gameplay |
|---|---|---|
| `[xrsplit]` in a gameplay window | `PROJ` high, `QUAD` 0 | `QUAD` high, `PROJ` 0 |
| meaning | OFXR is behaving correctly; flashing is a menu-only artefact and the fix is cosmetic or "don't generate on menus" | the port is claiming menu mode during play — a real bug in `MenuMode.cpp`'s pattern or its notion of "menu" |
| next move | leave the loop alone; consider suppressing generation while `menuMode != 0` | fix menu-mode detection, or submit a projection layer with the quad composited over it |

Both are cheap to act on once known. Neither is safe to act on now.

---

## What this build adds (all at once)

1. **Wall-clock timestamp on every log line.** The port's log was the only untimestamped log in
   the stack — OFXR, ReShade and the event log all have one. Without it, correlating "I saw the
   flash here" with the log meant counting lines.
2. **`[xrsplit]` — the split differenced per window**, printed next to the timestamp:
   ```
   [xrsplit] this window: PROJ n + QUAD n + BAD n + EMPTY n = N  -> generator-usable X%  | menuMode=N
   ```
   A menu stretch reads `QUAD` high with `PROJ` 0. Gameplay reads the reverse. Unmistakable.
3. Cumulative `[xrloop]` breakdown and the `rect`/`menuMode` attribution retained, so the totals
   still line up against OFXR's own counts.

---

## Test protocol — one run, several answers

Everything below comes out of a **single** session. Do not do these as separate runs.

1. Launch. **Sit in the main menu ~15 s** without loading.
2. Load a save. Let it load.
3. **Stand still in gameplay ~30 s.** Do not open any menu.
4. **Open the inventory/map ~15 s.**
5. Back to gameplay ~15 s. Look around, move.
6. Quit normally.

Note roughly when you see flashing — with timestamps on every line, "about a minute in, in
gameplay" is enough to find the window.

### What each phase settles

| phase | reads the answer for |
|---|---|
| 1 main menu | baseline: `QUAD` should be ~100%, confirming the counter means what I think |
| 3 gameplay, no menu | **the live question.** `PROJ` ~100% here = menus explain everything; `QUAD` here = menu-mode bug |
| 4 inventory | confirms the transition is detected at all, and its latency |
| 5 gameplay again | whether menu mode *clears* properly, or latches after a menu |
| all | `[xrrate]` display-period flapping and the 17–26 fps present rate, against wall clock |

Pair it with the OFXR flight log from the same session
(`%LOCALAPPDATA%\OFXR Bridge\RuntimeLayer\v068\`), which is millisecond-stamped — the two can now
be lined up directly.

---

## Standing rules for this investigation

* **Never ask for a run that answers one yes/no question.** If a second probe would cost ten lines,
  it goes in the same build.
* **Differenced, not cumulative**, for anything about when something happens.
* Read a counter on the thread that sets it. `menuRect=0 menuMode=0 NOW` was sampled from the
  Present thread at report time and was worse than useless — it looked like an answer.
* State which hypotheses a run can kill *before* asking for it. If the answer is "none of them
  cleanly", the build isn't ready.
