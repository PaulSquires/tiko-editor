# Phase 7c, step 28 — the Replace bar, and the defect step 27 shipped

Step 27 was the Find bar. This is its pair, and porting the pair is what exposed the thing the
Find bar got wrong on its own.

---

## The two rules that only exist because they are a pair

**Replace implies Find**, and it is not a convenience. tiko's `FindReplace_SetVisible` opens with
`if bReplace then bFind = true`, and the reason is that the *replace engine reads
`gFind.txtFind`* — a Replace bar over a hidden Find bar is a box that replaces the empty string.
Enforced in **both** directions here, where tiko gets the second for free by having one function
do both.

**Ctrl+H focuses the *Find* field**, not this bar's. That is the surprising half, it is tiko's last
line in `OnCommand_SearchReplaceDialog`, and it follows from the rule above: with no term there is
nothing for a replacement to replace, so the term is what has to be typed first.

---

## Preserve Case is step 27's question, answered the other way — and the other way is right

tiko makes Preserve Case `IP_KIND_COMMAND` and paints the latch from `gFind.nPreserve`.
`frmReplace_IconPaint` says why:

> "the control never keeps a second copy of state `FindReplace_DoReplace` already reads. Asking the
> model at paint time cannot go stale."

Step 27 made Match Case and Whole Word `PSICON_TOGGLE` and called it *the one place the toolkit is
simpler than tiko*. **`PsIconPanel` carries selection on the item, so a `PSICON_TOGGLE` is exactly
the second copy tiko refused** — and tiko has a site that breaks it: `frmFindInProject.inc:3140`
clears `nMatchCase` and `nWholeWord` behind the bar's back. Not live in this binary yet, so it
would have arrived **silently, with Find in Project**, one step after anybody could remember why
the latch was a copy.

Closed by keeping the toggle and adding `ShellFind_SyncToggles` / `ShellReplace_SyncToggle`: a push
from the model into the items at every show. The model is still the one truth; the copy is
refreshed from it at the one moment it can be looked at.

**This is the first step where porting the sibling found the defect in what shipped**, rather than
a gate or an interactive pass finding it.

---

## The fixture was wrong three times; the code, once

Step 27's headline was a fixture whose three expected counts were all guesses. Same shape here,
three more times — and this time one of the three was a real bug in the code under it.

**Replace is a two-press gesture from a cold caret.** `DoReplace`'s single-match branch opens by
comparing the *selection* against the find phrase and, when they differ, moves to the nearest match
and **returns without replacing**. So the first press selects and the second replaces. My fixture
pressed once and asserted the match was gone. The fixture is now Find Next, then Replace — which is
what a user's hands do.

**Layout is lazy, and "the two fields share a left edge" passed as `0 = 0`.** `SetBounds` only marks
the widget dirty; `OnLayout` runs from `EnsureLayout`, which a windowless run never reaches because
nothing paints. Both fields were zero wide and the relation held. **This is step 24's finding in a
second control** — and the only reason it surfaced is that the width check underneath it failed
where the equality did not. Driven from the **root** now, in tree order, because this bar's
`OnLayout` reads the Find field's rect.

**And that is what found the real bug: a child's `bounds` are its *parent's* coordinates.** The
first draft subtracted the bar's own `x` to "convert" them, giving `-413` against `6`. The rect is
taken as-is now, and the comment records that borrowing it is only valid because
`Shell_LayoutAll` gives both bands the same left edge and width.

---

## One revert came back green, and the comment was the thing that was wrong

Deleting the `foundCount = 0` guard from the three replace actions left the suite at **537/0**.

My comment had said it stops `DoReplace` "operating on wherever the caret happens to be."
**Invented.** tiko has the line and gives no reason for it; I supplied one that reads well.
`DoReplace` already refuses an empty term, and with a term that matches nothing both of its
branches find nothing to do.

The guard stays — this is a port and tiko has it. The comment now says its necessity **could not be
shown**, and the assertion beside it is relabelled so it stops implying coverage it does not have:
what it actually pins is that a term matching nothing disturbs nothing, guard or no guard.

*An unexplained guard that nothing can break is how a real one gets deleted later.* Recording why
it is there is cheaper than the alternative.

---

## Verification

| | |
| --- | --- |
| `_compile_fast` | exit 0, **zero warnings** — tiko did not move |
| `_compile_shell` | exit 0, **one warning, pre-existing** — see below |
| `_run_shell --selftest` | **511 → 537**, including a real replace over the probe buffer, undone afterwards |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_scihost`, `_check_package` | green |
| `_check_selftests` | 34 suites, 20,440, 0 failed — floors at 33 / 20,328 |
| layout oracle | unchanged. `DumpChild` names the band, not the bar's children, so the internals are pinned by `--selftest` instead |

**Revert-to-red, eight mutations, seven caught:** field never writes `txtReplace` 535/2; replace
does not imply find 533/4; hiding find leaves replace 535/2; sync does nothing 536/1; preserve flag
not set 536/1; fake action ids 536/1; field rect not borrowed 536/1. The eighth is the guard above.

**PRE-EXISTING, AND A CORRECTION TO STEP 27's PAGE:** `_compile_shell` emits one *warning 38,
suspicious logic operation* in the step-22 pane-switcher assertions. Confirmed by building `HEAD`,
which emits the same one, so it is not this step's. **Step 27's page said "zero warnings" for both
compile gates; that was measured on `_compile_fast` only.** Left alone rather than fixed in a
Replace-bar commit.

**NOT VERIFIED BY ME:** the bar on screen, and one thing on it specifically — **Preserve Case's
glyph is the literal text `"AB"`**, which is tiko's own (`wszIconPreserveCase`), not a symbol
codepoint. tiko needs two font handles for that, one per panel; this relies on `PsTextEngine`'s
fallback chain resolving `A` and `B` in the primary face and the PUA glyphs beside them in the
symbol face, **per character**. That is read from the engine's header, not observed.

The pass: **Ctrl+H** opens both bars with the caret in the **Find** field; typing a term and a
replacement, then **Replace**, changes one match (press it twice from a cold caret); **Replace All**
changes them all; **Preserve Case** latches and makes `abc`/`ABC` come back lowercase/uppercase;
closing **Find** closes Replace with it, and closing **Replace** alone leaves Find open.

---

## What is left

1. **The interactive pass above.**
2. **Selection**, the fold icon, and tooltips on both bars — deferred since step 27.
3. **Run step 18's Linux scripts** — three `.sh` files, unrun eleven steps later.
4. **fontconfig** — still never executed anywhere.
