# Phase 7c, step 7 — the thread, and the item that took three steps to justify

Threading has been on this port's list three times. It opened step 5's plan as **item 1** and was
deleted before a line was written, because measuring said a parse was 4–20ms. It came back as
**item 4** on the handoff, demoted for want of evidence. Step 6 measured the same call on a real
include graph and it became **item 1 again** — this time with a number.

This step spent it.

---

## THE MEASUREMENT

Same file as step 6, same machine, `src\tiko.bas` — 134 files, 4,496 symbols.

| | before (step 6) | after (step 7) |
| --- | --- | --- |
| **what the UI thread pays** | **1,244ms** | **32–37µs** |
| what the parse costs | 1,244ms | 1,319–1,417ms |
| where the parse runs | the UI thread | a worker |

Three runs: 36µs, 32µs, 37µs on the UI thread against 1,363 / 1,417 / 1,369ms of parsing. **The
work did not get smaller. It stopped being in the way** — a factor of roughly 38,000 on the only
thread the user can feel.

What the UI thread still does is exactly what it cannot delegate: copy the text out of Scintilla
(36,069 bytes here), fill a request slot, signal. **Scintilla is not thread-safe**, so that copy
happens on the thread that owns the view — which is what tiko does, for the same reason
(`clsScanMgr.inc:331-347`).

### The first attempt at this number measured nothing

The instrumentation was written against `PsTimerNow()` and printed **0ms**, then **0µs** when the
resolution was raised. `PsTimerNow` is not a clock. It is a **settable virtual clock**
(`PsTimer.inc:39` returns `g_psTimerNow`, whatever the pump last stored) — deliberately so, which
is what lets the debounce assertions be deterministic instead of flaky.

**A zero that means "the clock never moved" is indistinguishable from a zero that means "the work
was free",** and the second was the answer this step wanted to hear. Re-measured with FreeBASIC's
`timer`.

---

## What came back with the thread

Step 5's header listed four things that dropped out *because* the scan was synchronous, each with
a note saying it existed only to serve a thread. Three returned, and each was re-earned rather
than re-copied:

1. **The retire queue.** `fbcparser_free` has a same-thread contract, so a set displaced on the UI
   thread by `InstallSet` is handed **back to the worker** to free. If the queue fills, the set is
   **leaked deliberately and loudly** — freeing it on the wrong thread is a corruption that
   surfaces somewhere else entirely.
2. **The stale-root test.** A scan started before a tab switch can now land after it, and
   installing it would resurrect the previous document's symbols under the current file's name.
3. **Lifetime.** The worker is joined on all three exit paths. A thread still running at process
   exit is the standard way a clean quit becomes an intermittent crash.

The fourth — the project tier — arrived in step 6 and needed nothing.

**Latest wins, one slot per tier.** A newer request overwrites a pending one instead of queueing:
typing produces one per debounce, and a queue would parse states the user has already moved past.
tiko's rule (`clsScanMgr.inc:288-300`).

---

## The delivery half already existed and had never been used

Nothing was added to PsPlatform to get a result *back* to the UI:

* **`PSEV_USER = 1000`** — `PsEvent.bi:90`, commented *"application-defined, posted from worker
  threads"*.
* **`g_plat.events.Post` is thread-safe** — `PsSdl3.inc:774` calls itself *"the sanctioned channel
  from a worker to the UI, the direct analogue of the existing CreateThread + PostMessage
  idiom"*.

Both were written for exactly this and had no caller until now. So the worker computes, posts
`PSEV_USER`, and **the pump installs on the UI thread**. `gSymDb`, the panel, the documents and
every widget stay single-threaded. **Nothing in the toolkit became thread-safe by adding a
thread** — that is what keeps this a change to one function's timing rather than to the toolkit's
threading model.

### `PsThread` — and why it is not SDL's

`PsPlatform/src/ui/core/PsThread.bi/.inc` wraps start/join, a mutex and a condition. Shaped like
`PsTimer`: **a module, not a `g_plat` service.** A backend is a *choice* (Win32 or SDL, decided at
init); threads are not — they are the language's, the same everywhere.

**It wraps FreeBASIC's primitives, not SDL3's**, and the first reason decided it:

1. **FB's runtime needs its per-thread state set up.** `threadcreate` does that; a thread created
   behind the runtime's back gets a half-initialised environment, and anything in it touching an
   FB string, array or file handle is relying on luck.
2. `SDL_CreateThread` **is a macro** on Windows, expanding to `SDL_CreateThreadRuntime` with
   `_beginthreadex` thunks appended — CRT symbols not declared unless `crt/process.bi` is pulled
   in. It produced **"Argument count mismatch, found ')'"** on a call with visibly correct arity,
   because the preprocessor had spliced two more arguments in.

FB's threading is portable across every platform this toolkit targets, so nothing is given up.

**What `PsThread` deliberately is not**: no timed condition wait (FB's `condwait` has no timeout —
a worker that must notice a shutdown is *signalled* on shutdown), no thread pool, no queue, no
futures (a queue is a **policy** and every caller wants a different one), no cancellation (there
is no portable "stop that thread").

---

## The suite, which was the hard part

**Every existing scan assertion read `gSymDb` immediately after asking for a scan.** They passed
because the scan was synchronous, and they all went red the moment it was not. That was the
expected first result of the commit, not a surprise.

The fix is a **bounded drain**: pump events until the result lands or a deadline passes, then
assert. Not a sleep — a deadline, so a slow machine waits longer and a fast one does not pay.
Written once, used by every scan assertion.

**`tests/psthread` has no clock in it at all.** *"Start a thread, sleep 50ms, check it ran"*
passes on this machine and fails on a loaded one, and a suite that fails one run in fifty is worse
than no suite: it teaches everyone to re-run until green. Every one of its 17 assertions is a
**happens-before** enforced by a join or a signal.

It also **does not** claim to prove the mutex excludes under contention. Two threads hammering a
counter pass whether or not the lock works, unless they happen to interleave badly on the run you
watched. That is the runtime's contract, not this file's invention, and a test pretending to check
it would be a test that reports luck.

Shell suite: **343 assertions**, 0 failed.

---

## What is NOT verified

**Responsiveness has not been felt.** The 36µs is measured; whether typing into a 134-file
document *feels* smooth, and whether SDL's event queue survives a 1.4-second parse intact, is the
author's interactive pass. **That is the entire point of the step and no assertion can see it.**

**No race has been provoked.** The stale-root test and the retire queue are exercised in the
direction they are used, not under a deliberate race. Threading defects that only appear under
contention are, by construction, not what this suite catches.

**One worker, one tier at a time.** Two tiers share one thread, so a project scan and a buffer
scan serialise. On `tiko.bas` that is 1.3s + 1.3s before both panes are current.

**Shutdown mid-parse is joined, not interrupted.** Quitting during a 1.4-second parse waits for
it. There is no cancellation, by design — the exit is clean but not instant.

---

## A defect this step did NOT introduce, found while looking for one

The author reported the panel empty at startup. It is three separate facts, none of them a
regression:

1. **The default mode is Bookmarks**, and `tiko.bas` has none.
2. Forced to Functions mode it is *still* empty, correctly: `tiko.bas` is 500 lines of `#include`
   plus one procedure, and the parser records **15 symbols for it, all `#define`s and types**.
   `WinMain` is the only proc in the file, its signature is line-continued with `_`, and
   **fbcParser does not record it**.
3. **The pane lists open tabs only.** The 4,496 symbols live in the 133 *included* files, which
   are not open. That is step 6's limitation, unchanged.

Opening a file with ordinary signatures proves the path end to end — `clsSymbolDb.inc` gives
`enum=13, rows=14`, installed by the worker and reloaded by the pump.

Two parser behaviours noted in passing, neither in scope and both present in tiko proper:
**line-continued signatures are not recorded**, and `EnumProcsInFile` returned **13 for a file
with 48 procedures** — the 38 `clsSymbolDb.Foo` members do not come back.

---

## What step 8 has to decide

1. **The three `PsListTree` gaps** from step 4, still open and still worked around.
2. **Encoding detection on read**, still outstanding from step 3.
3. **Whether the pane should list more than the open tabs.** A project of 134 files currently
   contributes one heading, because only one of them is open. The database has the rest.
4. **Whether two tiers deserve two workers.** They serialise today; the number above says how
   much that costs.
