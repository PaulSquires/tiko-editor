# Phase 7c, step 8 — three toolkit gaps, and the pane that finally shows a project

Step 7 put the scan on a worker. The author opened the binary on `src\tiko.bas` and the pane
showed **nothing**.

Every part of that was working as built. The default pane was Bookmarks, which is empty by
construction until someone sets one. The Functions pane listed **open tabs**, of which there was
one — and that one file is 500 lines of `#include` whose 4,496 symbols live in the 133 files it
pulls in. Nothing was broken. Nothing was useful either.

---

## THE MEASUREMENT

`src\tiko.bas`, the same file every step since 6 has measured.

| | before | after |
| --- | --- | --- |
| **the pane** | **0 rows** | **10 headers, 41 procedures, 51 rows** |
| reload cost | — | **81ms**, stable across runs |

### And the ceiling is the PARSER, not this loader

51 looks small beside 4,496, so the intermediate numbers matter:

| what | count |
| --- | --- |
| files `EnumUserFiles` answers | **134** |
| of those, holding procedure symbols | 41 |
| procedure symbols in them | **573** |
| ...carrying a **body line** | **41** |

A row that jumped to line 0 would be a row that lies about where the code is, so a symbol with no
body line is skipped — tiko does the same, at the same place. The 532 that drop out are declares,
type members (`function clsFoo.Bar` is recorded against the header that declares it, not the file
that defines it) and **line-continued signatures**, which fbcParser does not record with a body at
all — the same behaviour step 7 found when `WinMain` was the only procedure in `tiko.bas` and the
database held none.

**tiko's own Functions pane reaches this through the identical call and has the identical
ceiling.** The pane is now as good as the database; the database is what is short.

---

## The order was forced, and that is the interesting part

The pane could not be done first.

`OnSelChange` could not tell a click from an arrow key, so the shell wired it straight to
goto-row. A pane listing 134 files, each of which **opens from disk when chosen**, would have
loaded a file per keypress on the way down the list. **The toolkit gap had to close before the
feature could land** — so this step is three commits of PsPlatform and PsCore work before one
commit of the thing that was actually wanted.

---

## The three `PsListTree` gaps, all additive

Each had been documented in `shellpanel.bi` at the time it was worked around, as belonging in the
control. All three turned out to need **no breaking change** — the four existing callers
(`PsSciPopup`, gallery, ideshell, the suites) were untouched.

**The second data slot was half-built already.** `itemData2` had been in `PsLtRow` and zeroed by
`PsLtResetRow` all along; it was simply unreachable. The shell's bit-packing — a shift, a mask and
a clamp to fold a tab index and a line number into one 64-bit integer — existed because nobody had
exposed a field that was already there.

**`SetAltRows` is a flag, not a colour, and that is the whole fix.** The workaround set
`clrRowAlt = clrBack`, and `OnThemeChanged` re-reads `clrRowAlt` from the theme — so the flatten
had to be reapplied after *every* theme load, and the one that was forgotten brought the stripes
back with no error. The suite now asserts striping is still off **after the startup theme apply**,
which is the exact thing the workaround kept failing to hold.

**`GetSelSource` is read inside the handler rather than passed to it.** A fourth argument on
`PsLtRowProc` would have broken every existing handler to serve one of them. It is stamped at the
**entry points**, not at the five notify sites: there is exactly one mouse-driven selection path in
the file and everything else is a key, so the classification is a property of where the event came
in. `SetCurSel` stamps `CODE` even though nothing observes it — a host reading the source outside a
callback must not be told MOUSE because of a click three programmatic selections ago.

---

## `PsPathOriginalCase` was not written, because it already existed

The plan called for a PsCore helper that recovers a path's real case from the disk. **PsCore has
had one since before this port started**: `PsFileRealCase`, walking each component against the real
directory entries, preferring an exact match so a directory legitimately holding both `README` and
`readme` resolves to the one that was asked for.

The commit that was going to add it added **assertions** instead — and the reason it needed to is
the useful part. The one existing assertion fed `RealCase` a **correctly** cased path and checked it
came back correct, *which a function returning its argument would also pass*. The direction with a
customer — wrong case in, right case out — was not covered at all.

---

## `FilenameOriginalCase`: five steps blocked on a blocker that was gone

The shell's version returned its argument. The link-debt entry beside it said it needed *"a PsCore
canonical-path call first"*.

**That call already existed.** The entry had been re-counted at every audit and never re-read.

> **A debt list decays into a fact about the past unless its entries are checked rather than
> tallied.** This one had been quoted correctly in three audits and was wrong in all of them.

So the body moved into `app/` and the ratchet went **3 → 2**, with the baseline lowered to match —
a baseline that is not lowered with the count stops being a ratchet and becomes a licence.

**Two binaries had been answering this differently**, which is the reason to move it rather than
copy it: tiko resolved case, the shell returned its argument, and both key the same
filename-keyed symbol database. A key is not a place for two opinions.

### What tiko gave up, deliberately and with the author's decision

`GetFinalPathNameByHandleW(FILE_NAME_NORMALIZED)` resolved **symlinks, junctions and `subst`'d
drives** to their final target as well as fixing the case. `PsFileRealCase` does not. Open a file
through a junction and tiko now shows the path you gave rather than the target — arguably the
better caption, and a change either way.

### Two wrappers, both of which fail silently

* **The fallback.** `RealCase` answers EMPTY for a path that is not there, and the names this is
  asked about are routinely gone — deleted since the scan, or synthetic. Handing that out would
  blank a filename in a panel and turn a database key into `""`.
* **The separators.** PsCore's canonical form is forward slashes; every consumer here compares
  against paths from a command line or a dialog. Forward slashes do not fail loudly — **the
  document lookup just misses, and the pane opens a second copy of a file that is already open.**

---

## What is NOT verified

**The mouse-versus-keyboard gate has no assertion.** No mouse or key event reaches a windowless
surface, so "arrowing the pane does not jump the editor, and clicking does" is the author's
interactive pass. It is the single most user-visible rule in the step and the suite cannot see it.
The assertion beside it says so in its own comment rather than implying coverage.

**Nor does the striping itself.** Nothing in `pstree` captures painter output, so removing the
`bAltRows` guard from the painter changes no assertion. The flag's independence from the theme
*is* asserted; the pixels are not.

**The 81ms reload has not been felt under typing.** It runs on every scan install, so a debounced
edit pays it after each parse. Measured, not experienced.

**`FilenameOriginalCase` now costs one directory listing per path component**, called once per
file. That is inside the 81ms, but on a network path or a much deeper tree it is the part that
would grow.

**No junction or `subst`'d drive was tested** after tiko lost its symlink resolution.

---

## Revert-to-red

| commit | rules reverted | went red |
| --- | --- | --- |
| PsListTree | 5 | **4** (3, 1, 1, 6 failures) |
| psfile | 1 | 1 (2 failures) |
| the workarounds | 4 | **3** (1, 9, 2) |
| FilenameOriginalCase | 3 | 3 (1 each) |
| the pane | 4 | **3** (8, 1, 3) |

**Three did not, and each is recorded where it lives rather than here.** Two are the unassertable
ones above. The third is worth more than it looks: "register a file only once it has rows" was
written with a comment claiming it prevented *"one file's procedures under another file's name"*.
Reverting it failed nothing — an unreferenced table entry is invisible, because the table is only
ever read through a row's slot. **The comment was rewritten to call it tidiness.** The revert did
not find a missing test; it found a false claim, which is the other thing reverting is for.

---

## Counts, re-run on 2026-08-11

| gate | count |
| --- | --- |
| PsPlatform `build.cmd check` | **47 suites**, 0 failures |
| `pstree` | 242 → **271** |
| `psfile` | 91 → **94** |
| shell `--selftest` | 343 → **357** |
| `_check_app_standalone` | **debt 2**, baseline 2 |
| `_check_app_layer` | 47 files |
| `_check_shell` | 5 files |
| `_check_scihost` | green |
| tiko `_compile_fast` | 0 warnings |

---

## What step 9 has to decide

1. **The pane's ceiling is the symbol database.** 51 rows from 573 procedure symbols. Whether
   declares, type members and line-continued signatures should reach a Functions pane is an
   fbcParser question, and it is the same question in tiko.
2. **Encoding detection on read**, still outstanding from step 3 and now the oldest open item.
3. **`clsTopTabCtl`: portable rewrite, or a Win32 facade forever?**
4. **Two tiers, one worker.** They serialise; `tiko.bas` is 1.3s + 1.3s before both are current.
5. **The two remaining link-debt bodies**, one of which is an `app/` file reaching UP into the
   shell by relative path — invisible to a token scan, which is why `_check_shell` reads
   `#include` lines instead.
