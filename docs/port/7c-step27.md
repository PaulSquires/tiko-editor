# Phase 7c, step 27 — the Find bar, and the first search in the shell

Step 26 moved the engine into `app/`. This step is the bar, and **it is the first thing in this
binary that searches** — the first editor feature beyond typing.

**The engine was reachable before a line was written, and that was verified rather than assumed:**
`AssignTextBuffer` calls `CreateScintillaWindows`, which goes through the seam, and
`ShellHost_CreateView` hands back `g_view` for index 0. So every shell document's `pSci(0)` is the
editor, and `GetActiveScintillaPtr()` — which falls through to `pSci(0)` when
`hWndActiveScintilla` is unset, as it always is here — returns it.

---

## It is an arrangement, not a port

`frmFind.inc` is 662 lines, most of them a hand-rolled painter, a tooltip backend, and three
`PsIconPanel`s laid out against a rect the tab control owns. What carries across is **field,
toggles, navigation, count** — and every piece is a control this port has already proven:
`PsTextBox` by step 23's label editor, `PsIconPanel` by step 20's pane switcher.

**It does not search.** The bar writes `gFind` and calls the engine. That split is the whole
reason step 26 came first.

**Match Case and Whole Word are `PSICON_TOGGLE`**, which is the one place step 20 found the toolkit
simpler than tiko: tiko paints its own latch because its items are commands, and `PsIconPanel`
carries selection *on the item*. Their ids are **negative**, so a toggle can never be mistaken for
a menu command by the one handler both strips share.

**Prev, Next and Close carry real menu ids** and go through `OnMenuCommand` — step 20's rule, so a
button and its menu entry cannot come apart. `IDM_FIND` toggles the bar, which is why the close
icon carries that id rather than one of its own.

**Seeding is deliberately not ported.** `FindControls_Show` reseeds the field from the selection,
which is exactly the behaviour that needed an exception this week for the case-only echo. Adding
the seeding without the exception would be re-importing a fixed bug.

**`RefreshFindBar` is real now**, and its comment is rewritten. It read *"EMPTY, AND CORRECT — this
binary has no Find bar yet"* — true for exactly one step, and the sort of comment that stops being
true the moment somebody builds the thing it names.

---

## Every count in the search assertion was wrong on the first run

The assertion that matters is not the bar's shape — it is **a search over a known buffer returning
a known count**. Step 26's gates were green through an engine that could not update a count, could
not replace text, and replaced the wrong number of matches; a bar whose widgets all exist and
whose search finds nothing would look identical from a shape assertion.

**So I wrote the search assertion, and asserted 3.** The probe file has three procedures. The
engine found **5** and was right: the file is

```
' probe
#include once "open_probe2.bas"
sub ProbeAlpha() … function ProbeBeta() … declare sub ProbeGamma()
```

so case-insensitive `"Probe"` is the comment, the include's filename, **and** the three names.
Match Case on `"probe"` is 2. Whole Word is 1 — only the comment's stands alone.

All three of my numbers were guesses dressed as expectations. **The engine was correct in every
case and the fixture was wrong in every case**, which is the same failure step 26 is a monument
to, one layer along: *an expectation asserted without being derived*. They are derived in a
comment now, so the next person to edit that probe file can see what they are moving.

## And a fourth unasserted wiring, in the fourth consecutive step

Removing `gFind.nMatchCase = iif(bOn, 1, 0)` from the toggle handler left the suite at **508/0**.
Every assertion above it set the flag **directly**, so none of them touched the handler that is
supposed to.

That is steps 22, 23, 24 and now 27: the policy tested by calling the callback, the wiring never
driven. Three assertions added that drive `ShellFind_OnIcon` with the item latched first — because
the control flips a `TOGGLE` before the callback runs, and the handler reads that rather than
flipping again.

---

## Verification

| | |
| --- | --- |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings — **tiko did not move**, which is the point of running the first one on a shell step |
| `_run_shell --selftest` | **493 → 511**, including a real search, both toggles, and the field writing the term through its own callback |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_scihost`, `_check_package` | green |
| `_check_selftests` | 34 suites, 20,440, 0 failed — floors at 33 / 20,328 |
| layout oracle | **unchanged**. The band's rect is the layout's, exactly as it was when it was a stub |
| revert-to-red | the field not writing the term 506/2, toggles as `COMMAND` 507/1, the toggle not setting the flag 510/1, nav without menu ids 507/1 |

**NOT ASSERTABLE HEADLESSLY:** `RefreshFindBar` doing nothing changes no assertion, because a
repaint has no model effect. Its **installation** is gated — the seam's completeness check exits 2
if it is unset, which is how step 26 found it in the first place.

**NOT VERIFIED BY ME:** the bar on screen. Its geometry is arithmetic against a band the oracle
pins, and the search behind it is asserted — but whether the field, the two strips and the count
land where they should is the author's, and **step 26 is why that sentence is not boilerplate.**

The pass: **Ctrl+F** opens the bar with the caret in the field; typing searches and the count reads
`n/m`; **F3 / Shift+F3** walk and wrap; **Match Case** and **Whole Word** each change the count;
**Esc or the X** closes it and returns the caret to the editor.

---

## What is left

1. **The interactive pass above.**
2. **The Replace bar** — its own band, its own step, and `Preserve Case` belongs with it.
3. **Run step 18's Linux scripts** — three `.sh` files, unrun ten steps later.
4. **fontconfig** — still never executed anywhere.
