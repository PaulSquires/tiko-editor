# Phase 7c, step 29 — Selection, and the seeding step 27 deferred

Two items off the deferred list, and **they arrive together because they cannot be separated.**

The seeding rule is *the box is a picture of the selection*: opening Find with nothing selected
**clears** it rather than re-offering the last phrase. That is by request, and it is why tiko's old
`bSeedField` flag is gone. But the same code is where the **case-only echo** lives — type
`afxnova`, press Ctrl+H, and the field comes back `AfxNova`, because the incremental search has
already selected the match and a selection carries the *document's* casing. That was fixed by
request in step 26. **Porting the rule without the exception would have re-imported it.**

---

## The capture is not ported from where tiko keeps it

tiko fills `pDoc->CurrentSelection` from `SCEN_KILLFOCUS`: its Find bar is a separate `HWND`, so
opening it takes the focus off the editor, and the selection as it was at that instant is what the
bar has to reason about.

**This shell has no such moment.** Building one would mean handling `PSEV_FOCUS_LOSE` in
`PsSciView`, which does not handle it today — **a PsPlatform change**. It is also unnecessary:
*focus* is not what clobbers the selection, the incremental search that runs afterwards is. So the
live selection is read **at show time**, one instruction before anything can disturb it, with
`isInitialized` as the latch that stops a second show overwriting the original.

## And the assertion found what that decision cost

The first run of "arming Selection lays down a marker highlight" **failed**, and it was not a
fixture error.

**Two different notions of "the selection" are in play.** The toggle's *rule* reads
`CurrentSelection` — the captured one. `SetMarkerHighlight` reads the **live** one. By the time
anybody clicks the icon, the incremental search has moved the live selection onto the last match.
Selection would have marked **that match's line** instead of the range the user chose — or, on a
one-line match, marked **nothing at all** while leaving the flag set against no markers.

tiko's `SCEN_SETFOCUS` restore is now done at that one moment, inside the Selection arm. The
header's note that the restore "is not ported" is still true of the general case and now says which
exception it names, instead of implying the gap is cosmetic.

**This is the first defect in the run that a headless assertion found in code I had just written
for a reason I had just documented** — the header predicted the gap in the abstract and still got
the consequence wrong.

---

## Selection is the only toggle on either bar that can refuse to latch

"Search within the selection" needs a selection to be within. tiko arms it only when the original
selection spanned lines, or when a marker highlight is already down, and forces it **off**
otherwise.

`PsIconPanel` latches a `PSICON_TOGGLE` *before* the callback runs, so the refusal has to be pushed
back into the icon — which makes Selection the first real customer for **step 28's
`ShellFind_SyncToggles`**. That machinery was added to close a staleness gap; here it is the path by
which a decision reaches the control at all.

## Opening Replace now clears both copies, which tiko does not

`ReplaceControls_Show` blanks the **box** on every open — and only the box. **Programmatic setters
are silent**, so `gFind.txtReplace` keeps the previous replacement while the field shows empty, and
the next Replace uses a term nothing on screen names. Both are cleared here.

A deliberate divergence, and it is the one-source-of-truth rule this pair already runs on rather
than a new idea.

---

## The fifth step running where a wiring went unasserted

Gutting `ShellFind_CaptureSelection` left the suite at **556/0**. Every assertion above it set
`CurrentSelection` **by hand**, so none of them touched the thing that is supposed to fill it.

That is steps **22, 23, 24, 27 and 29**: the policy tested by calling the callback, the wiring never
driven. Five assertions added that drive the whole gesture instead — a real multi-line selection in
the buffer, close, open, **capture → seed → arm**. It turned out to be the only cover for the
multi-line seeding branch as well, which nothing had exercised either.

---

## Verification

| | |
| --- | --- |
| `_compile_fast` | exit 0, zero warnings — tiko did not move |
| `_compile_shell` | exit 0, one warning — the **pre-existing** step-22 warning 38 |
| `_run_shell --selftest` | **537 → 561** |
| the five `_check_*` gates | green |
| `_check_selftests` | 34 suites, 20,440, 0 failed — floors at 33 / 20,328 |
| layout oracle | unchanged |

**Revert-to-red, ten mutations, all caught** (the capture only after the five assertions above were
added): capture 557/4, seeding 556/5, case exception 555/1, reopen guard 555/1, Selection always
arms 551/5, no sync 555/1, closing keeps the latch 555/1, closing keeps markers 550/6, no restore
555/1, Replace clears the box only 555/1.

**NOT PORTED, AND NAMED RATHER THAN LEFT TO BE FOUND:** the general `SCEN_SETFOCUS` restore. After a
search the live selection is the last match rather than what the user had, everywhere except the
Selection arm. Closing that needs a focus hook on the view, which is a **PsPlatform** change.

**NOT VERIFIED BY ME:** any of it on screen.

The pass: select a few lines, **Ctrl+F** — the box is **empty** and **Selection is already lit**,
with the chosen lines shaded; searching finds matches only inside them. Select a single word,
Ctrl+F — the box holds **that word**. Type `probealpha` over it, **Ctrl+H** — the box still says
`probealpha`, not `ProbeAlpha`. With nothing selected, Ctrl+F — the box is **empty**, not the last
phrase. Clicking Selection with nothing multi-line selected — the icon **does not stay lit**.
Closing the bar drops the shading, and reopening does not bring the latch back. Ctrl+H always opens
with an **empty** replacement.

---

## What is left

1. **The interactive pass above.**
2. **Tooltips on both bars**, and the fold icon (Find in Project only) — the rest of step 27's
   deferred list.
3. **Run step 18's Linux scripts** — three `.sh` files, unrun twelve steps later.
4. **fontconfig** — still never executed anywhere.
5. **The `SCEN_FOCUS` pair**, if the general restore is ever wanted — PsPlatform.
