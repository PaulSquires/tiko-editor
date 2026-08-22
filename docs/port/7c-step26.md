# Phase 7c, step 26 — the find/replace engine moves down

`frmExplorer.inc` is ported. The next form worth having in the shell is the **Find bar** — the
most-used editor feature, a bar rather than a dialog, and `PsTextBox` and `PsIconPanel` are both
proven now.

**But its engine was in the shell.** This step moves it. **The bar is step 27**, which is the
pattern every earlier step used — `clsDocument` in step 3, `Doc_ReadFromDisk` in step 9,
`ProjectSaveToFile` in step 13 — for the same reason: a form ported on top of a model still in the
shell cannot be gated.

---

## The search half was already portable and nobody had noticed

Every function was `SciExec(hEdit, …)` on an `HWND`. **`SciExec` is `SendMessage`**, and
`app/modScintilla.bi` has declared the same call in portable types since step 3, saying so in its
own comment:

```
(any ptr, ulong, uinteger, integer) as integer
```

So the port is a rename at sixty-odd call sites and an `HWND` that becomes an `any ptr`.

**And each of the three shell reaches had a seam already waiting for it:**

| reach | where it went |
| --- | --- |
| `gTTabCtl.GetActiveDocumentPtr()` | `TabDocAt(TabActiveIndex())` — added in **step 13** for precisely this question |
| `pDoc->hWndActiveScintilla` | `GetActiveScintillaPtr()`, an `any ptr` since **step 3** |
| `SetWindowRedraw` | `gAppHost.SetViewRedraw` — **already existed and had never had a caller** |

One new field was needed: `RefreshFindBar`, a Notify.

## The type lost exactly two members

* **`hCueBannerFont as HFONT` — dead.** Nothing has read it since the cue banner became the text
  control's own business. **Deleted rather than moved**: carrying a dead Win32 handle into the
  portable layer to keep a diff small would have been the worse of the two choices.
* **`rcResults as RECT` — stayed**, as `gFindResultsRect`. Five reads in `frmFind.inc`, all
  layout, and a rectangle measured by a Win32 window belongs with the window. The same split
  `clsConfig` took in step 3.

Everything else in the 24-member type is `DWSTRING`, `long` and `boolean`.

## Two signatures changed, and one of them is about locale

**`HighlightSearches` and `DoReplace` take the occurrence colour.** They read
`theme.editor.occurrence` directly, and `theme` lives in the shell's `modThemeTypes.bi` — so the
alternative was moving the whole theme tree down to satisfy one indicator colour.

**`isupper` / `islower` are gone, tested inline as ASCII.** They are not declared in this layer,
and `crt/ctype.bi` would have brought back a **locale-dependent** answer for anything above 0x7F —
where `PsUCase` and `SymDb_NameEqW` both fold ASCII only and say so. A Preserve Case replace that
behaved differently depending on the C locale would be the odd one out, and the difference would
present as *"replace mangled my accented text"* on one machine and not another.

A non-ASCII character is therefore neither upper nor lower, so a string containing one falls to
the "mixed" arm and is replaced verbatim. That is the safe answer.

---

## The seam caught the new field before a single assertion ran

```
tikoshell: AppNotify.RefreshFindBar is not set (build error)
```

Exit 2, immediately. **`tikoshell`'s body for it is empty and correct** — it has no find bar, so
there is nothing to repaint — and that is the distinction this seam was built on: **a Notify field
can be legitimately empty; no Services field can.**

**An empty body is a decision. An unset pointer is an omission.** The completeness check exists so
the second cannot masquerade as the first, and this is the first time in twenty-six steps it has
had to say so about a field added in the same commit.

---

## AND ONE OF THE SIX WAS NOT PORTED AT ALL

**`FindReplace_UpdateResultsFromCaret` was reconstructed from its doc comment.** My read of the
original had been truncated at the comment, and rather than going back for the remaining thirty
lines I wrote an implementation of what the comment described.

It compiled, it linked, it left 20,328 assertions standing, and **the author found it in under a
minute**:

* clicking **Match Case** did not update `n/m` until F3
* **switching tabs** did not update `n/m` until F3

**The cause is three early returns the original does not have.** It guards the *count* with
`(lenFind > 0) andalso (endPos > 0)` and then writes `gFind.wszResults` and repaints
**unconditionally**. Mine returned early on a zero count, an empty term and a zero `endPos` — so
every caller that had just changed something and wanted the number refreshed got the *previous*
document's number left on screen, until F3 came through the navigating path and rewrote it.

The matching rule differed too: the original takes the first match whose **end** is at or past the
caret, which is what makes a caret at either edge of a match report *that* match rather than the
next one.

**Restored verbatim.** The other five were then checked mechanically against `feacc7d33~1`, and
every difference in all five is an intended substitution — the seam, the `any ptr`, the colour
parameter, `gFind.txtFind`, the comment-marker style. One worth naming: `DoReplace`'s selection
compare was `ucase(string) <> PsUCase(DWSTRING)` and is now `PsUCase` on both sides — consistent
where it was mixed, which is a change however small.

**The first attempt at that check reported all six identical and was a nothing.** The `awk`
extracting each body failed on every file, both sides came out empty, and `diff -q` on two empty
files says they match. **Fourth harness to lie in this session, and the first outside the
revert-to-red script** — which is the argument for the guard being a property of *every* comparison
rather than of one script.

**The lesson is not "read the whole function".** It is that this report claimed, one paragraph
earlier, that the gates could not see whether the engine still found anything — and then the very
next thing that happened was a defect the gates could not see. **The claim was right and I acted
as though it were rhetorical.**

## THE INTERACTIVE PASS RAN, AND IT FOUND FOUR THINGS

Confirmed by the author, in order, after the gates had all been green for hours:

| | |
| --- | --- |
| type in the find field | works |
| **Match Case** did not update `n/m` until F3 | **defect** |
| **switching tabs** did not update `n/m` until F3 | same defect |
| **Replace** / **Replace All** replaced with an empty string | **defect** |
| **Replace** behaved like **Replace All** | **defect** |
| the find term echoed back in the document's casing | pre-existing, changed by request |
| **Preserve Case** | works |
| **F3 / Shift+F3**, including the wrap at each end | works |

**Three of the four were mine, and every one of them was a thing the gates cannot see.**

1. **`UpdateResultsFromCaret` was reconstructed from its doc comment**, not ported. Three early
   returns the original does not have. Section above.
2. **`gFind.txtReplace` has never been written by anything.** `DoReplace` used to read the control
   directly; once it was in `app/` it read a permanently-empty field. My commit message for the
   move asserted the opposite — *"the field's change handler writes those, so they agree by
   construction"* — and one grep would have shown `txtFind` with seven writers and `txtReplace`
   with none.
3. **The new `clrOccurrence` parameter went first**, and `boolean` converts to `ulong` silently, so
   all three `DoReplace` call sites kept compiling and shifted one place.
   `DoReplace(false, true)` — *replace this one, then move on* — became `fReplaceAll = TRUE`.

   The twelve `HighlightSearches` call sites were caught by the compiler because that function
   takes **one** parameter. `DoReplace` already had two optional ones, so adding a third at the
   front broke nothing and changed everything. **The fix was the signature, not the call sites**:
   the colour is last and mandatory now, so an un-updated caller is a compile error.

**The pattern across all three is one thing: an equivalence asserted without being checked.** That
the body matched its comment. That two sources of a string agreed. That adding a parameter was
additive. Each was cheap to verify and none of them was verified.

---

## What the gate had to say, and what it did not

`_check_selftests` was **green for every one of those defects**. 20,328 assertions, zero failures,
through a find engine that could not update a count, could not replace text, and replaced the
wrong number of matches.

Its header already said the right thing — *"a DROP in assertions is visible"* — and **visible is
not asserted**. It floors the suite count and now floors the assertion total too.

**But that floor would not have caught these either**, and the comment in the gate says so:
re-introducing defect 1 deliberately left the total unchanged. It catches a suite that stops; it
does not catch a suite that never tested the thing.

**And the totals moved twice today in ways I could not attribute** — 33/20,328 versus 34/20,443,
then 20,443 versus 20,440 — both in suites that count data rather than statements, both with zero
failures, neither reproducible from a code change. That is why the floor sits at the lowest
observed number rather than the highest, and why this gate's absolute total is not a signal at
three-assertion granularity.

**The honest summary of step 26's verification is: the gates proved the move compiled and linked,
and the author proved it worked.** Four defects, four found by hand, none found by any gate.

## Verification

| | |
| --- | --- |
| `_check_app_layer` | **48 → 50 files**, free of Win32 and AfxNova |
| `_check_app_standalone` | **18 → 19 clean**, 0 errors, debt 0 and still no baseline |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | 493, unchanged — the shell does not call the engine yet |
| `_check_selftests` | **33 / 20,328 in a clean tree** |
| `_check_shell`, `_check_scihost`, `_check_package` | green |
| revert-to-red | unsetting `RefreshFindBar` in the shell → `exit 2` and the field named, before any assertion |

**NOT VERIFIED BY ME, AND THIS IS THE STEP WHERE THAT MATTERED MOST -- SEE ABOVE FOR WHAT THE
PASS THEN FOUND.** This is a move of
**live code that `tiko.exe` runs every time anyone presses Ctrl+F** — not an addition beside
existing behaviour. Every gate here proves it compiles, links, and leaves 20,328 assertions
standing. **None of them proves it still finds anything.**

The interactive pass this needed was narrow and specific, and ALL OF IT HAS NOW RUN:

* [confirmed] type in the find field — the count should read `n/m`, not `0/0`
* [confirmed] **F3 and Shift+F3**, including the wrap at each end
* [confirmed, after a fix] **Match Case**, **Whole Word**, **Selection** — each should change the count
* [confirmed, after two fixes] **Replace** and **Replace All**, and **Preserve Case** on a mixed-case match
* [confirmed, after a fix] switching tabs with a search active — the count should follow the document

**The one A/B break:** `DoReplace` read the two text boxes through `GetDlgItem` +
`AfxGetWindowText` and now reads `gFind.txtFind` / `txtReplace`. The field's change handler writes
those, so they agree by construction — but Replace is the path to watch.

---

## What is left

1. **The interactive pass above.**
2. **The Find bar in the shell** — step 27, and the engine is now reachable from it.
3. **Run step 18's Linux scripts** — three `.sh` files, unrun eight steps later.
4. **fontconfig** — still never executed anywhere.
