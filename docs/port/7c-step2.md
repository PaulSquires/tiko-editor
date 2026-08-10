# Phase 7c, step 2 — the pump collapse, and the first two dialogs

Step 1 built a shell that lays out like tiko. Step 2 answers the question it deliberately did
not touch: **does `frmMain`'s message pump port, or does it collapse?**

It collapses. The residue is one dialog traversal policy, it is **four lines**, and it is
written. That was the largest named unknown in `d2-decision.md` and it is now a known quantity.

**READ THAT AS ONE UNKNOWN CLOSED, NOT AS 7c GETTING SHORTER.** 7c is still 48 forms and
~45,000 lines. Nothing on this page measures any of the rest.

---

## What is NOT verified, and it is more than what is

This section is first because in this step it is the more important one. Three separate times
an assertion that looked like proof turned out to constrain nothing, and each was caught by
deliberately reverting the fix rather than by reading.

**NOTHING ABOUT A MODAL DIALOG IS ASSERTED ANYWHERE, in either repo.** `PsModalHost.Run` needs
a compositor and `build check` is headless by design — no suite calls `PsPlatformInit`. Both
defects this step found in `Run` were fixed with the buggy line restorable and **all 46
PsPlatform suites plus all 194 shell assertions still green**, checked both times. The only
evidence either fix reaches its defect is the author dismissing a real dialog.

**Specifically unproven:** that `Run` calls `PsSurfaceDetachRoot` rather than `SetRoot`; that it
sets initial focus; that a dialog is centred, correctly sized at 175%, or painted at all; that
the OS half of modality holds (`g_plat.window.SetModal`); that `Run`'s failure path works — it
needs window creation to fail and nobody has made it.

**No key has been pressed in any of it.** Tab order is asserted against `PsSurface.FocusNext`
directly, never through a keystroke. The path from hardware through the backend into dispatch is
as untested as it was in step 1.

**An open menu closing before its command runs is unreachable.** `PsMenuHost` takes the popup's
command slot precisely so it can close the chain first; `OpenRoot` declines without a window, so
none of it can be driven headlessly.

**Eight of the sixteen claim points in `frmMain`'s chain never fired in either driven session**
(`docs/port/pump-census.md`). They are carried as *unexercised*, never as *dead* — a context menu
was not opened, Alt was not pressed on some runs, the User Tools and Build Config accelerator
tables need a bound tool and a configured project. Where a verdict in the census rests on a zero,
it rests on the call site instead, and the row says so.

**Accelerator tables 2 and 3 are empty in this binary.** The shell does not load `settings.ini`,
so the precedence rule is exercised only by synthetically filled tables — never by a real user
tool or build configuration.

**One defect fixed itself and the cause was never identified.** The input box did not display;
the only change before it did was adding print statements and rebuilding. Almost certainly a
stale `_shell\tikoshell.exe` was being run. Recorded because "it works now" without a cause is
not a fix.

**The estimate for the other 45,000 lines is not re-measured**, and this page makes no claim
about it.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| which filters actually fire | **the pump oracle**, two driven sessions — `docs/port/pump-census/` |
| the pump's shape under PsPlatform | the census, one row per site, verdict + mechanism |
| accelerator precedence across three tables | self-test group G, reverted to confirm it bites |
| the dialog key policy | group H — Tab, Shift+Tab, Enter, Escape, wrap, write-back |
| the nested pump's routing | group I, and it fails when PsPlatform's table is broken |
| the layout did not move | shell dump byte-identical to the oracle at every commit |
| tiko still builds and behaves | gas64 at every commit; nine dialog suites paired at the one that touched them |
| PsPlatform is not broken | `build check` 46 suites; `_check_scihost` after each PsPlatform commit |

**194 assertions, up from 118.** The count is not the point; which ones bite is. Every new rule
in commits 2–5 was reverted to check its assertion went red. Three did not, and are recorded in
place — see below.

---

## What step 2 found, which is the useful part

**THE COUNTS WERE WRONG THREE TIMES, IN BOTH DIRECTIONS.** `7c-step1.md` said 15 pumps and 13
`IsDialogMessage` calls; this step's own plan said 19 and 16 from a grep that counted comment
lines. The truth is **17 and 15**. The original survey missed the `W`-suffixed API forms
entirely, so `PsColorPicker` and `PsMessageBox` each own a modal pump that appeared in no earlier
list. Fourth short hand-written list against this surface, after the three vocabulary-ratchet
gaps already on record.

**THE HEADLINE NUMBER WAS THE OPPOSITE OF WHAT IT LOOKED LIKE.** The first trace read
`IsDialogMessage claims 4115 of 5517` — 75%, against the one Win32 mechanism with no PsPlatform
analogue. It is not a hole: **`IsDialogMessage` dispatches what it processes**, so a TRUE return
means "I was the dispatcher". The capture gave itself away by listing `WM_PAINT` among its claims
while the editor repainted throughout. Split by message class, its keyboard share in `frmMain` is
**53 in 7167 — 0.7%, and visibly ordinary typing**; in `frmOptions` it is 87 in 1614 and the
descriptors are `VK_TAB`, `Shift+VK_TAB`, the arrows and `VK_RETURN`.

**So the irreplaceable behaviour lives in the DIALOGS, not in `frmMain`** — the opposite of where
the plan put the risk, and the reason the collapse is as clean as it is.

**AND THE REPLACEMENT IS FOUR LINES, because PsPlatform already did the hard half.**
`PsDispatch` gives Tab to the focused widget first and only moves focus if nothing wanted it —
the rule a text editor needs — and `FocusNext` walks the tree in document order. What was
missing is Enter and Escape, which are application policy in any toolkit.

**TWO DEFECTS IN `PsModalHost`, BOTH FOUND BY BEING ITS FIRST CALLER ANYWHERE.** `Run` had no
caller in either tree: `PsModalRoute.bi` records it as exercised once interactively by a demo
that no longer calls it.

1. **It deleted the caller's dialog.** Teardown called `surf.SetRoot(0)` under a comment saying
   that detached the root. `SetRoot`'s first statement is `if pRoot <> 0 then delete pRoot`, and
   `PsMessageBoxShowModal` takes `byref box` from its caller — normally a local. Every dismissal
   deleted a stack address and then read `GetResult()` from freed memory. The process died
   instantly and silently. Fixed by `PsSurfaceDetachRoot` (PsPlatform `61f56bb`), in **two**
   places: the teardown, and the failure path, where the same line meant a dialog that could not
   get a window freed its caller's box and then reported `ResolveCancelId()` out of it. The
   second site was found by reverting the first and re-reading, not by any test.
2. **No dialog it raised had ever had initial keyboard focus.** Win32 does this in the dialog
   manager on `WM_INITDIALOG`; `Run` did not do it at all. The message box hid it — a box of
   buttons still looks right with focus nowhere, and the first Tab quietly supplies what should
   already have been there. A text field cannot be typed into at all. Fixed in `be10064`, with
   the limitation named: no control can claim focus on attach because there is no attach hook,
   so `PsMessageBox.ResolveFocusIndex` still cannot be honoured.

**tiko's `WM_CHAR` GUARD PORTS TO SOMETHING ELSE ENTIRELY.** `frmMain.inc:2239` drops control
characters below 32 because Win32 manufactures the character message independently of who handled
the key. `PsEvent.bi` says that bug class is deleted — and it is, **but only for claims made
inside the surface**. `PsDispatch` sets `bKeyConsumed` when a *widget* consumes a key-down. The
shell's pump claims keys *before* `surf.Dispatch` — menu host, Alt mnemonic, accelerators — so
the surface never sees them and the paired text event is delivered anyway. The rule is not
deleted for a host that filters ahead of dispatch; it is inherited. A `< 32` test would have been
the wrong port: SDL commits printable text, so tiko's literal symptom cannot arise, while a bound
letter chord firing its command *and* typing its letter can.

**THREE ASSERTIONS DID NOT CONSTRAIN WHAT THEY APPEARED TO, and the revert habit is what said so.**

* "Escape resolves to Cancel" passed with `SetCancelId` removed — Cancel *is* the last button and
  the unset convention resolves to the last button. It now asserts the thing the explicit call
  defends: a box built Cancel-first, where the convention alone would make Escape mean **Yes**.
* The write-back rule — Cancel must not commit edited text — was unreachable inside
  `ShellInputBoxShow`, which needs a window. Reverting it left all 169 assertions green. It is
  now `ShellInputBox_Commit`, a pure function of the answer, and reverting it now fails two.
* Step 1's menubar-switch assertion is still vacuous and still says so in place.

**Splitting the decidable part out of the windowed part turned three untestable rules into tested
ones** — `BuildExitBox`, `ShellInputBox_Commit`, and `PsModalRoute` before them. That is the
transferable technique from this step.

**"WINDOWLESS" DESCRIBES WHAT A TEST BUILDS, NOT WHAT IT CAN BE MADE TO DO.** `--selftest` hung
on first write of the modal assertions: `PsPlatformInit` *is* called because `PsSciView` needs
the text engine, so `Run` created a real window and pumped it, waiting for a button nobody would
press. Any future assertion reaching a `Show`/`Run`/`DoModal` has the same trap under it.

**THE SHELL'S ASSERTIONS NOW BITE ACROSS THE REPO BOUNDARY.** Breaking `PsModalRouteEvent`'s quit
arm, then its owner-close arm, each failed exactly one tiko assertion. A PsPlatform routing
change surfaces in tiko's suite rather than in a running dialog.

---

## The estimate

`d2-decision.md` and `7c-step1.md` both named the pump collapse as the largest single unknown and
as where the 14–20 week variance lived. It is now measured:

> **Seventeen pumps become one pump and a call.** One `PsSurface` loop; fourteen
> `PsModalHost.Run` call sites; two bounded drains. Sixteen claim points in `frmMain` become four
> host `RouteEvent` calls the shell **already makes**, three `PsAccelTable`s walked in a loop, one
> precedence rule, and two rows deferred with the Find/Replace bars.

**The 14–20 week estimate is not re-opened upward, and the risk that page named is reduced.** The
work that remains in the pump is the dialog traversal policy, and it is done — once, not fourteen
times, because all twelve dialogs want the same policy.

**What replaces it as the largest unknown is not measured here.** The candidates are the document
model (`clsDocument`, `gTTabCtl`, the tab control) and the 48 forms' *content* — every dialog
this step censused still has to have its controls built, and step 2 built two dialogs holding
four controls between them. The pump was the cheap half.

---

## Out of scope, and still out

The Find/Replace bars are stubs, so `handleEscKeyModeless` and `handleKeysFindReplace` are
censused and not ported. No autocomplete popup, no code tips, no context menus beyond Scintilla's.
No document model. No `PsWin32Host` removal, no `PsC.` prefix deletion. No merge. Twelve of the
fourteen dialog pumps are censused, not ported: two dialogs is the proof, fourteen is the work.
