# Phase 7c, step 1 — done, and what it is actually evidence for

`7c-starting-position.md` defines step 1 as *"`frmMain`'s chrome and the editor pane, every
dock panel stubbed, as a runnable binary that is not merged."* That exists:
`_shell\tikoshell.exe`, built from `src\shell\tikoshell.bas` (2057 lines) by
`_compile_shell.bat`, run by `_run_shell.bat`.

**7c ITSELF IS STILL 48 FORMS AND ~45,000 LINES.** This is one binary with one form in it.
Read the numbers below as a measurement of the *approach*, not of the progress.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| the layout matches tiko's | **the oracle**, field by field, ten states — see below |
| the widget tree is well-formed | `--selftest`, 92 assertions, 0 failing |
| the app layer is usable from a second binary | 8 menu titles resolve through `L()`; 85 chords parse |
| tiko still builds | `_compile_fast.bat` at every one of the 14 commits |
| PsPlatform is not broken by any of it | `build check` 45 suites; `_check_scihost` after each PsPlatform commit |
| the boundary holds | `_check_shell.bat`, and `_check_app_standalone` at 11 clean / 0 errors |

**THE ORACLE IS THE ONE THAT MATTERS.** Every other line above is a relation, and a
wrong-but-self-consistent layout satisfies relations. `docs/port/layout-oracle/` holds
`frmMain`'s real rectangles for ten states and the shell's for the same ten, and the
comparison is:

> **No edge differs from tiko's by more than 2 pixels, and nothing differs by more than
> that at all.** One pixel is the splitter-grab rounding — `CWindow.ScaleX` rounds half to
> even, `PsScaleBy` half away from zero; two is the left/right split, where the grab enters
> the arithmetic twice. Every Y coordinate and every height not downstream of `nLeft` is
> exact.

Re-derive that bound rather than trusting this page: parse both dumps, compare the four
edges of each child in each state, take the maximum. The README beside them says what a
difference outside that class would mean.

## What is NOT verified, and it is most of the program

**NOTHING WAS TYPED, CLICKED OR DRAGGED.** Every assertion drives synthetic events or reads
geometry. Specifically:

* **No key has been pressed.** `PsAccel` resolves a synthetic `Ctrl+S` to `IDM_FILESAVE`; the
  path from real hardware through the backend into the table is untested. On a US keyboard it
  could not distinguish physical from layout mapping anyway.
* **No menu has been opened by hand.** The self-test asserts the host *answers*
  `OnOpenRequest` and is handed the right dropdown — not that a popup appears.
  `PsPopupHost.OpenAt` declines when the surface has no `hWin`, and `--selftest` has no
  window by design.
* **No splitter has been dragged.** The bars are laid out and have no drag behaviour at all;
  every split position comes from the state record.
* **The clipboard and the caret are wired and unexercised.** `PsSciUseSystemClipboard` is
  called and never tested; the caret period is set and no frame was watched.
* **The scrollbars are geometry only.** Neither is connected to the view.
* **One display, one scale factor.** The `PSEV_RESIZE` font reopen is written from the
  documented contract and has never run — it needs two monitors at different DPI.
* **No suite sweep since the oracle landed.** The 27-suite sweep was run paired for commit 0
  and was identical. Later commits changed `tiko.exe` only by moving declarations between
  headers with identical values and construction order.

**AND THE 14–20 WEEK ESTIMATE IS STILL NOT RE-MEASURED.** `d2-decision.md` said so when D2
was taken and nothing here changes it. What step 1 does say is that the approach works and
where the cost actually sat — see below.

---

## What step 1 found, which is the useful part

Almost none of it was in the plan. Listed because the same shapes will recur.

**THE APP LAYER HAD NEVER LINKED ON ITS OWN, and the gate could not tell.**
`_check_app_standalone.bat` ran `fbc -c` — compile only — so a missing body was invisible to
it by construction, and it reported 7 clean / 0 errors throughout. `app/clsConfig.bi` declares
`dim shared gConfig`, so including the header instantiates it, while the constructor lived in
the shell. The gate links now, and found three more of the same shape; it carries them as a
counted debt at baseline 3.

**FOUR THINGS HAD TO MOVE DOWN INTO `app/`**, all of them already PsCore-clean and none of
them a judgement call once found: `clsConfig`'s constructor, `modTextFile.inc`,
`LoadLocalizationFile`, and the key-binding model (`gKeys` and the 112 defaults). Each was
found by the shell needing it, not by reading.

**A PATH IS NOT AN IDENTIFIER.** `app/modMenuDefinitions.inc:22` includes
`"../modKeyBindings.bi"` — the app layer reaching *up* into the shell — and the vocabulary
ratchet cannot see it, because it matches tokens. That is a fifth gap in that ratchet, after
the three the docs already record. `_check_shell.bat` checks the other direction by reading
`#include` lines only.

**RELATIONS ARE BLIND TO SCALE.** The shell shipped one commit with a visibly unscaled UI
while 21 assertions passed, because every one of them was a relation between rectangles and
all of those hold perfectly at the wrong scale. The author found it by looking at the window.

**AND A GREEN ASSERTION CAN CONSTRAIN NOTHING.** The first fix for that shipped
`g_menubar->ScaleY(100) = PsScaleBy(100, 1.5)` as "the assertion that would have caught it".
It would not have: `ScaleY` reads the surface's scale live and passes whether or not the tree
was ever told. Caught by deliberately reverting each fix to check the new assertions went
red — two did, that one did not.

**THREE DEFECTS CAME OUT OF `demos/ideshell`** by copying it, and all three were live there:
its menubar answers nothing so it drops nothing; it never sets `surf.hWin`; it never scales.
Fixed in PsPlatform `db73895`. A demo that is the nearest prior art is a demo people copy.

**TWO tiko DEFECTS FOUND AND DELIBERATELY NOT FIXED**, both recorded in the oracle README:
with the side panel docked right the top-tabs icon strip is drawn over the panel; and the
top/bottom split is never centred, because `SplitY` is a bare `height/2` used as an absolute
Y while `SplitX` is a midpoint. The port reproduces both — the oracle records what tiko does,
and changing tiko's behaviour is a separate decision with its own verification.

---

## Where the shell actually is

Twenty children: a real `PsMenuBar`, `PsStatusBar` and two `PsSciView`s sharing one document,
plus sixteen stubs. `frmMain_PositionWindows` is `Shell_LayoutAll`, a pure function of
`(w, h, fScale, ShellLayoutState)` — which is what makes the whole thing assertable, and was
the largest single item in the step, exactly as the plan warned.

**What a stub is:** a real `PsWidget`, in the tree, in the right place, painting its own name
*and its own bounds*, with nothing behind it. Not an empty real control — an empty
`PsListTree` looks correct in the wrong band.

## What comes next, and it is not more of this

Step 1 answers "does the approach work". The question it does **not** answer is the one
`d2-decision.md` flagged as where the estimate's variance lives: **the pump collapse.**
Fifteen message loops, 13 `IsDialogMessage` sites, eight ordered filter claims, three
accelerator tables — and `PsModalHost` still proven exactly once, interactively, for one
message box, with no headless test.

None of that got easier here. A `tests/psmodalhost` suite is the cheapest thing that would
tell you whether it is going to.
