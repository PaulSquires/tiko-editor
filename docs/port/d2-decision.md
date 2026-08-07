# D2, re-decided — or rather, deliberately not yet

`docs/port/7c-starting-position.md` records **D2 — SDL3 on both platforms, no Win32 backend**
and says of it:

> That was decided as a forecast. Gate 6 has now landed on all three targets, so it can be
> re-decided on evidence instead, and it should be, before 45,000 lines are committed to one
> shape.

This page is that re-decision. **Its conclusion is that the decision is not yet due**, and
that saying so is worth more than picking a side early.

Phase 7c is 48 forms, ~45,000 lines, a third of tiko, estimated 14–20 weeks. It is the most
expensive thing left to get wrong.

---

## FIRST, TWO STALE FACTS THAT WOULD MISLEAD ANYONE PLANNING THIS

Both were believed while framing this decision, and both are wrong.

**1. Gate 5 is DONE.** `PsPlatform/docs/STATUS.md` listed *"the 25-control port — not
started"*. `src/ui/controls/` holds **26 controls**, and STATUS's own "full remaining set"
table further down the same file shows every one struck through as done. **The widgets 7c
needs already exist.** That one line made the whole phase look blocked on work that had
already happened; it is corrected in the same commit as this page.

**2. The bridge is not what D2 assumed.** D2's reasoning was that there is nothing to bridge a
half-converted shell with, which makes 7c un-shippable in the middle. **`PsWin32Host` now
exists, works, and is carrying tiko's editor today.** That is the bridge D2 said would not be
there.

Both corrections point the same way: **the ground under D2 moved, and nobody re-checked.**

---

## What has actually changed since D2 was taken

| then | now |
| --- | --- |
| SDL3 backend a forecast | Gates 0–5 passed on Windows, WSL2 and Fedora/Wayland |
| no proof Scintilla works in a widget tree | `demos/minieditor` is a working editor on `PsSciView` |
| no way to bridge a half-converted shell | `PsWin32Host` ships in tiko and hosts the editor |
| 25 controls unported | 26 widgets exist under `src/ui/controls/` |

---

## Shape A — hold D2. One shell flip.

`frmMain` becomes an SDL3 window; chrome and editor convert together, every dock panel
stubbed, as a runnable binary not merged until 7c completes.

**For.** One shape. No second backend. Linux from day one. `PsWin32Host` and the last 30
`PsC.` prefixes deleted.

**Against.** Un-shippable in the middle, by construction. **13 message loops** — `frmMain` plus
12 modal dialogs, each with its own `GetMessage` — collapse into one event loop in a single
step. The three `HACCEL` tables and `IsDialogMessage` have no PsPlatform analogue.

**The hard blocker is WebView2.** `frmHelpCenter.inc` parents an AfxNova `CWebView2` to a
`CreateWindowExW(0, "STATIC", …)` host. It needs a real HWND permanently and there is no SDL3
path. Under A the Help Center becomes an embedded native child window or it is dropped.
**This is the one constraint that cannot be engineered away**, and it deserves to drive the
answer rather than be discovered halfway.

## Shape B — promote `PsWin32Host` to a real Win32 `IWindowBackend`

tiko keeps its HWND shell; panels convert one at a time; the branch stays runnable and
mergeable throughout.

**For.** Incremental and verifiable per panel. No 45,000-line jump. WebView2 and the
accelerator tables keep working untouched.

**Against.** A second `IWindowBackend` is precisely what D2 exists to prevent. Today there is
exactly one implementor, `PsSdl3.inc`, filling every table; a second means every platform
feature is implemented and tested twice, forever, and Windows and Linux drift apart in ways
that only show up on the platform you were not looking at. `PsWin32Host` stops being 582 lines
of scaffolding and becomes product code with a maintenance cost.

---

## THE RECOMMENDATION: DO THE SHARED PREREQUISITES, DECIDE LATER

**The next several weeks are identical under both answers.** Taking the decision now buys
nothing and spends evidence that has not been gathered. Three things are required by A and B
alike, none is wasted either way, and each is bounded and independently verifiable.

### 1. A timer / frame scheduler — by far the largest gap

`PsSdl3.inc`'s `AddTimer` **returns 0 and does nothing**; `KillTimer_` is empty. Its own
comment says timers arrive with the frame scheduler.

tiko has **43 `SetTimer` sites across 27 files**, and **20 of its 24 controls** need timers —
hover dwell, auto-repeat, scroll acceleration, caret blink. PsPlatform already carries the
absence as a list of admitted defects:

- the caret does not blink (`PsTextBox.bi`)
- no auto-repeat when a spinner button is held (`PsNumericUpDown.bi`)
- no drag auto-scroll in a list (`PsListTree.bi`)
- marquee is host-stepped, tooltip dwell and submenu hover are host-clocked

**Nothing shell-shaped is worth starting before this lands.** A converted panel without timers
is not a converted panel; it is a screenshot.

### 2. A theme engine (`PsTheme`)

Does not exist. From `STATUS.md`: *"That is PsTheme's job and PsTheme does not exist yet, so
`clrBack` is a field and the host sets it."*

tiko is heavily themed and has a 364-assertion theme suite. Setting ~15 colour fields per
control instance by hand is not a migration path — it is a way to convert one panel and then
stop.

### 3. An IDE-shell composition demo

No demo assembles a menubar + splitters + docked panels + a statusbar into one layout;
`gallery` places the parts side by side in a grid. That composition **is** what a converted
`frmMain` does first, and building it as a demo is the cheapest way to find out what 7c
actually costs — before committing 45,000 lines to the answer.

---

## What this buys

Those three turn D2 from a forecast into a measurement. After them, the choice between A and B
is made against a real IDE-shaped layout with real timers and real theming, instead of against
an estimate. And the WebView2 question — the only genuinely irreducible constraint here — can
be settled separately, on its own, at any time.

**The decision stays open, deliberately, and this page is the reason rather than an omission.**
