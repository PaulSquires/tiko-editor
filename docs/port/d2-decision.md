# D2, re-decided — deferred, and now DUE

`docs/port/7c-starting-position.md` records **D2 — SDL3 on both platforms, no Win32 backend**
and says of it:

> That was decided as a forecast. Gate 6 has now landed on all three targets, so it can be
> re-decided on evidence instead, and it should be, before 45,000 lines are committed to one
> shape.

This page is that re-decision. **Its conclusion was that the decision was not yet due** — three
shared prerequisites came first, identical under either answer.

**THOSE THREE ARE NOW DONE** (PsPlatform `41e2b6a`), so the deferral has expired and the
decision is due. It is still not taken here; see "Where that leaves D2" at the bottom for why,
and "What the prerequisites actually taught" for the four things now known that were not known
when Shape A and Shape B were written below.

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
| no frame scheduler | `PsTimer`, and all five admitted timer defects closed |
| no theme engine | `PsTheme` — tiko's own format, all 26 controls, 225 fields |
| no IDE-shaped layout anywhere | `demos/ideshell`, verified by hand in four palettes |
| `PsWin32Host` was 582 lines of scaffolding | it now also owns tiko's clipboard and its ticker clock |

---

## Shape A — hold D2. One shell flip.

`frmMain` becomes an SDL3 window; chrome and editor convert together, every dock panel
stubbed, as a runnable binary not merged until 7c completes.

**For.** One shape. No second backend. Linux from day one. `PsWin32Host` and the last 30
`PsC.` prefixes deleted.

**Against.** Un-shippable in the middle, by construction. **13 message loops** — `frmMain` plus
12 modal dialogs, each with its own `GetMessage` — collapse into one event loop in a single
step. The three `HACCEL` tables and `IsDialogMessage` have no PsPlatform analogue.

~~**The hard blocker is WebView2.**~~ **CHECKED, AND IT IS NOT A BLOCKER — see
[`webview2-decision.md`](webview2-decision.md).** Three of the four sentences that stood here
were wrong, and they are kept struck rather than deleted because the shape of the error is the
lesson:

> ~~It needs a real HWND permanently and there is no SDL3 path. Under A the Help Center becomes
> an embedded native child window or it is dropped. This is the one constraint that cannot be
> engineered away.~~

`frmHelpCenter` is created `WS_POPUP or WS_OVERLAPPEDWINDOW` — **a separate top-level window**,
opened on demand from Help and F1. It is not a pane inside `frmMain`, so under Shape A it stays
exactly what it is and nothing embeds anything. SDL3 hands over the native HWND anyway
(`SDL_PROP_WINDOW_WIN32_HWND_POINTER`, already in the binding). And the content is **local static
HTML this project generates itself**, so the Linux answer is the default browser via
`SDL_OpenURL` rather than a second engine.

**One form uses WebView2 in the whole of tiko**, and the only tiko-side coupling worth removing —
one `ExecuteScript` that fills the site's search box — is replaced by a `?q=` parameter in
`helpgen`, a repository we own.

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

**Status: 3 of 3 DONE, as of PsPlatform `41e2b6a`.** The prerequisites are finished, which
means **this page's own recommendation has expired**: the reason to defer D2 was that the
evidence had not been gathered, and now it has. Read "What the prerequisites actually taught"
at the bottom before re-reading Shape A and Shape B — three of the findings bear directly on
the choice, and one is new information about how expensive Shape B has already become.

### 1. A timer / frame scheduler — ~~by far the largest gap~~ **DONE**

**Landed in PsPlatform `d1055ef`** as `src/ui/core/PsTimer.*`, with 87 assertions in
`tests/pstimer`. What follows is the gap as it was; read it for the shape of the problem, not
for the state of the tree.

Three things about the answer are worth carrying forward, because they are not what this page
assumed:

* **It is not in the backend, and `AddTimer`/`KillTimer_` were DELETED from `IEventBackend`
  rather than implemented.** `SDL_AddTimer` fires on its own thread, so the UI-thread queue is
  needed either way; tiko's real host is `PsWin32Host`, so a backend timer would have to be
  written a second time as `WM_TIMER`; and the only primitive a timer actually needs —
  `Ticks()` — both backends already had. A second implementation is exactly what D2 exists to
  prevent, so the scheduler is shared code driven by whoever pumps.
* **The pump's wait timeout now comes from the next deadline**, replacing the hard-coded 30ms
  all six pump sites carried.
* **tiko does not use it yet, and tiko's own pump does not service it.** tiko's 43 `SetTimer`
  sites are still Win32's, and nothing in tiko has been converted. Wiring `PsTimerService`
  into tiko's message loop is a one-line change whenever the first converted panel needs it —
  but it has not been made, and nothing here should be read as saying tiko has timers.

`PsSdl3.inc`'s `AddTimer` **returned 0 and did nothing**; `KillTimer_` was empty. Its own
comment said timers would arrive with the frame scheduler.

tiko has **43 `SetTimer` sites across 27 files**, and **20 of its 24 controls** need timers —
hover dwell, auto-repeat, scroll acceleration, caret blink. PsPlatform already carries the
absence as a list of admitted defects:

- the caret does not blink (`PsTextBox.bi`)
- no auto-repeat when a spinner button is held (`PsNumericUpDown.bi`)
- no drag auto-scroll in a list (`PsListTree.bi`)
- marquee is host-stepped, tooltip dwell and submenu hover are host-clocked

**Nothing shell-shaped is worth starting before this lands.** A converted panel without timers
is not a converted panel; it is a screenshot.

**That list is now closed.** All five are converted: the caret blinks at 530ms, the spinner
auto-repeats at 400/60, the list drag-scrolls on a timer, and both hover delays run themselves.
The marquee stays host-stepped **deliberately** — stepping by call is what makes the animation
assertable, and a timer should call `StepMarquee` rather than replace it.

Two things the conversions cost, both worth knowing before 7c:

* **The spinner's step moved from the RELEASE to the PRESS.** A control that waits for the
  release cannot repeat while the button is held. That is the only caller-visible behaviour
  change in the whole scheduler work.
* **The scheduler grew a second door.** `PsTipHost` and `PsMenuHost` are not widgets — both own
  popup surfaces — so there is no `OnEvent` to deliver a `PSEV_TIMER` to. `PsTimerSetProc` is a
  callback timer keyed on `(user, id)`, and it carries a hazard the widget form does not:
  **nothing purges it**, so both hosts have a destructor whose only job is killing theirs.

### 2. A theme engine (`PsTheme`) — **DONE**

**Landed in PsPlatform `cc459bb` and `39f9011`**: the engine, the loader, and all 26 controls —
225 fields across 27 widget types. 84 assertions in `tests/pstheme`.

**The model is tiko's, deliberately and almost verbatim.** Same file format, same role names,
same three-step resolution — `key -> role -> built-in` — so a tiko `.theme` file drives
PsPlatform's controls with nothing re-authored. All ten of tiko's themes load.

**The number that justifies the design: EIGHT of those ten name no widget keys at all.** They
set the twenty-one roles and stop. So the middle step is not a convenience — without it, eight
themes out of ten would render every control in built-in colours. It also means a field with the
wrong ROLE is wrong in eight themes and right in the two that happen to name its key, which is
why each control passes its role explicitly rather than looking one up in a table.

**Nothing moves until `PsThemeApply` is called**, so no render digest moved and no control
changed colour by PsTheme merely existing.

**Three `PsColor` fields are deliberately NOT themed**, and asserted as such: `PsColorPicker`'s
`clrCurrent` and `clrInitial` are the colour being EDITED, and `PsMessageBox`'s
`clrIconOverride` is a per-instance host choice.

**What it does NOT cover, and 7c hits immediately:** `PsThemeApply` walks the WIDGET tree, so
**it does not reach the editor**. Scintilla's colours live in its own style table behind `SCI_*`
messages. tiko already has that seam and drives it from its `editor.*` keys; the shell demo has
to do the same by hand. See prerequisite 3.

### 3. An IDE-shell composition demo — **DONE**

**Landed in PsPlatform `41e2b6a`** as `demos/ideshell`: menubar and toolbar docked top, status
bar bottom, two splitters dividing the rest into an explorer, a tab bar over a `PsSciView`, and
an output pane. 36 headless assertions on the layout and the splitter drags; `Ctrl+T` cycles
four palettes; `--theme <path>` loads a real `.theme` file. Verified by hand in all four.

**It found three defects before it was finished, none of them visible to any headless suite** —
which is the argument for having built it at all:

* **`PsThemeApply` does not reach the editor.** The shell came up perfectly themed with a WHITE
  PANE in the middle of it.
* **`STYLECLEARALL` does not reach the margin.** An editor themed to the last glyph keeps a
  white strip down its left edge. `minieditor` **has that strip today** and works around it in
  an assertion rather than fixing it.
* **The requested window size is not clamped to the display.** At 1.75 scale a 1100x700 request
  becomes a 1925x1225 window: larger than the screen, status bar below the bottom edge, no way
  to drag it into view. Every demo in that tree makes the same assumption.

---

---

## What the prerequisites actually taught

They were meant to turn D2 from a forecast into a measurement, and they did. Four things are
known now that were not when Shape A and Shape B were written above.

**1. THE EDITOR IS NOT A WIDGET, AND IT COST THREE SEPARATE SEAMS.** Each was found by running
something, none by reading:

| seam | what the host had to supply |
| --- | --- |
| clipboard | `SciPs_SetClipboardHooks` — copy filled a buffer nobody read; paste was `void Paste() override {}` |
| caret blink | `SciPs_SetTickerCallback` — `FineTickerStart` only recorded, so a period could be asked for and never honoured |
| theming | roles translated to `SCI_STYLESET*` by hand, margin included |

**Every one is a HOST obligation with no default**, because the two hosts do not share an
implementation: a demo has SDL3's clipboard, tiko has Win32's and never calls `PsPlatformInit`
at all. **Whatever shape 7c takes inherits all three**, and a half-converted shell carries each
of them twice for as long as the conversion lasts. That is a real cost on Shape B this page did
not know about when it was written.

**2. `PsWin32Host` HAS GROWN, NOT SHRUNK.** It was 582 lines of scaffolding "deleted when 7c
completes". It is now the editor's only paint and input path in tiko, **plus** a Win32 clipboard
and a `WM_TIMER` ticker driver — because a `GetMessage` loop is not a pump that services
`PsTimer`. Shape B's "second `IWindowBackend`" is no longer hypothetical: a good deal of it
already exists and is load-bearing.

**3. THE SHARED WORK WAS GENUINELY SHARED.** Nothing in the scheduler, the palette or the shell
demo had to be written twice or thrown away. That is what this page predicted, and it is the one
prediction it is fair to call confirmed.

**4. WEBVIEW2 IS SETTLED, AND IT WAS NEVER THE BLOCKER.** Investigated in
[`webview2-decision.md`](webview2-decision.md) after the prerequisites landed. It constrains
neither shape: the Help Center is already its own top-level window, SDL3 exposes the HWND
regardless, and the content is local static HTML whose portable answer is the user's own
browser. **The last thing standing between D2 and a decision turned out not to be standing
there.**

## Where that leaves D2

**The reason for deferring is gone.** The prerequisites are done, the evidence exists, and the
choice can be made against a running IDE-shaped layout instead of an estimate.

**It has not been taken here.** This page exists because a forecast was made once and not
re-checked; picking a side in its closing paragraph — on a question whose deciding constraint is
WebView2, which nothing in this work touched — would be the same mistake pointing the other way.
What it can say is that the ground has moved again since the top of this page: the bridge D2
assumed could not exist is now carrying three subsystems, and the cost of the half-converted
state is measured rather than guessed.

**WebView2 is now settled and did not decide anything**, which leaves exactly one question:
a second `IWindowBackend` maintained forever, against a 45,000-line jump that is un-shippable in
the middle. That is a judgement about which cost this project would rather carry, and it is the
author's to make — but it is now the ONLY question left, and every fact it needs is on this page
or the three it links to.
