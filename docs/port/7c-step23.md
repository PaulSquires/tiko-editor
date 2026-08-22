# Phase 7c, step 23 — the note that outlived its own condition

`PsListTree.bi` carried label editing under **WHAT IS NOT PORTED** with a reason:

> IN-PLACE LABEL EDITING (BeginEdit, F2, Enter-edits, the commit-on-scroll dance). It needs a real
> single-line editor with a caret and a selection, **which is PsTextBox's phase.** The colour
> picker could hand-roll its fields because they hold at most three digits; a label editor cannot.

Every word of that was true. **PsTextBox landed** — caret, selection, undo, clipboard, UTF-8
offsets snapped to character boundaries, covered by `tests/pstextbox` — and the paragraph went on
saying the feature was blocked on it.

That is the lesson step 19's correction added to the handoff's blocker table, arriving again:
**a note naming a prerequisite has to be re-read when the prerequisite ships, and the file least
likely to notice is the one that depends on it.** The header says so now instead.

---

## Nothing was added to either control to make the keys work

* **Enter** — `PsTextBox.OnEnterPressed`, a host callback that was already there.
* **Escape** — `PsTextBox` leaves it **unclaimed on purpose** (*"a dialog's Cancel is more useful
  than anything this field could do with it"*), and `PsDispatch` bubbles an unhandled key up the
  parent chain. It arrives at the tree with neither control knowing about the other.
* **Focus loss** — `OnFocusChange`, guarded against the close itself, since `EndEdit` moves focus
  back to the tree and would otherwise commit twice.

`pEdit` is created **once** in the constructor and hidden, exactly as `pScroll` and `pHeader` are.
`PsTextBox`'s constructor is plain field init — no engine, no platform, no allocation — so a tree
that never edits pays for a hidden child and nothing else.

## The commit-on-scroll dance has two chokepoints, not eleven

The field is positioned in **row coordinates**, so anything that moves rows must close it first or
it floats over a different row still holding the old one's text — and the next Enter renames the
wrong thing.

* **`PsLtDirty`** — every structural change routes through it, eleven call sites, so collapse,
  expand, delete, clear and every bulk load are covered by one line.
* **`PsLtSetTopVis`** — the one place the scroll position moves, whatever moved it.

The scroll hook sits **inside** the *"did it actually move"* test: a clamped no-op scroll must not
close the field the user is typing into.

**No recursion**, and that is why `EndEdit` clears `nEditRow` *before* running the host's callback:
the host is expected to rebuild the list from its own model — tiko's Explorer does, because its
folder table is the truth — and the rebuild lands back in `PsLtDirty` with nothing to commit.

---

## Every assertion passed on the first run, which is not a reason to trust them

`pstree` 290 → 326, green immediately. Six reverts, each failing its own:

| revert | |
| --- | --- |
| no commit on structural change | 323/3 |
| no commit on scroll | 323/3 |
| Escape not handled | 325/1 |
| the host's refusal ignored | 325/1 |
| an invisible row may be edited | 324/2 |
| Enter not wired | 318/8 |

## And the installation was unasserted **again**

The Explorer's rename policy is asserted by calling the two callbacks **directly**, so deleting
the lines that install them changed nothing — **463/0 either way.**

That is step 22's finding, **in the very next step, on the list step 22 created for it.** The list
has the edit pair on it now. Twice in two steps is the reason it exists.

**And removing the name validation does not merely fail — it segfaults.** An empty name reaches
`ProjectFolders_Combine` and then `Rebase`; the assertion catches it first and the process dies
afterwards. The guard is load-bearing for more than politeness.

---

## A gate number that depends on stray files

`_check_selftests` reported **34 suites / 20,362** where the handoff says 33 / 20,328. tiko.exe's
sources are unchanged since that reading, and the new number reproduced identically across three
runs — so it was not noise and not code.

**It is the two untracked scratch files in the tiko root.** With `korean_text.bas` and
`chinese_text.bas` present: 34 / 20,362. Moved aside: **33 / 20,328**, exactly.

`TIKO_EXPLORER_SELFTEST` 109 → 111, `TIKO_FINDPROJ_SELFTEST` 33 → 36, and a whole
`TIKO_UNUSED_SELFTEST (layout)` line appearing.

**So the gate's headline number cannot be quoted without saying what was in the directory**, and
the handoff quotes it in two places. What saves the gate is that it asserts a **floor** — at least
33 report lines, zero failures — rather than an exact total, which is why it passes at 34 and
would fail at 32. The floor was written in step 14 for a different reason (a tiko that crashes
early prints nothing and sums to zero failures) and it covers this too.

**Not fixed here.** Making the suites ignore stray files in the working directory is tiko.exe
work, and the two files are the author's to keep or delete.

---

## Verification

| | |
| --- | --- |
| PsPlatform `build.cmd check` | 48 suites, 0 failures; `pstree` **290 → 326** |
| `_check_scihost` | green — the gate that catches a PsPlatform change reaching tiko |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **452 → 464** |
| `_check_selftests` | **33 / 20,328 in a clean tree**; 34 / 20,362 with the two untracked files present. 0 failures either way; `settings/` and `tiko.tiko` unchanged |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_package` | green |
| revert-to-red | six in PsPlatform, four in tiko — one of which came back green and bought the installation assertion, and one of which segfaulted |

**NOT VERIFIED BY ME:** typing into the field. The caret, the focus ring and the keyboard need a
compositor; what is asserted is every state transition around them. **The Explorer's rename is now
the first thing in this port whose whole value is invisible to every gate that exists.**

---

## What is left

1. **The interactive pass.** More load-bearing than usual: rename is a keyboard feature.
2. **The action icons** — add / rename / delete on the hot row. The overlay can draw them and all
   three commands now exist.
3. **Run step 18's Linux scripts.**
4. **fontconfig** — still never executed anywhere.
5. **The two untracked files**, which are now known to move a gate's numbers.
