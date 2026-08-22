# Phase 7c, step 25 — drag and drop, and a rule the suite kept breaking

**The last item on step 19's list of PsPlatform gaps**, and the last of `frmExplorer.inc`.
Everything else on that list either turned out not to be a gap (the correction in `517bc5ba4`) or
has since been filled.

---

## The name was the trap

**tiko's `frmExplorer_CanDropCallback` returns `false` at the end. Always.**

It is not a validator that permits the control's reorder. It **performs the whole move itself**
and then refuses. Reading the name as a permission check and building a veto would have produced
the wrong hook entirely — and the hook is the part that has to be right, because it is in the
other repo and every future host inherits it.

So `PsLtDropProc` is *"the host handles this drop"*: **TRUE means carry on and reorder**, which is
what a tree with no callback does today; **FALSE means the host already moved something in its own
model and the tree must not touch its rows.**

That second case is the whole reason it exists. A host whose rows are a **view** of something else
— a folder tree built from a table, a file list built from a directory — does not want a reorder.
It wants to change the thing the rows are derived from and rebuild.

**The drag is torn down before the callback is asked** — capture released, cursor restored,
autoscroll stopped — so a host cannot leave the mouse captured or a drop line on screen by
refusing. There is an assertion for that rather than a promise.

---

## Four more redundant guards, and one that is merely untested

`ProjectFolders_MoveFolder` tests **onto-itself** and **into-a-descendant** on its own lines
409-412, under the same condition and in the same order as the handler ported from tiko.
`ShellPanel_SetMode` sets the drag flag on every switch, so setting it at install is belt too.

**That is the third time in this port** (step 21 found two more), and the pattern is worth more
than the lines: **`modProjectFolders` was written defensively long before it had a caller**, so a
handler ported from tiko restates rules the model already enforces. All four are kept — the port
is a port and tiko's shape is the reference — and all four are now **labelled as early-outs**, so
nobody reads the shell layer as the place the rules live.

**And one guard is untested rather than redundant**, which is a different thing and now says so.
`catDest < 0` needs a document whose `ProjectFiletype` matches no category at all, and no fixture
here can produce one. Nothing below repeats it; the suite simply cannot see it.

---

## The suite broke this handler's own first rule, twice

The handler snapshots its source **before touching anything**, and says why: *row indices stop
meaning anything the moment the model changes.* Every drop calls `ShellExplorer_Load`.

**The suite written to check it did not.**

* One assertion read the file's path **after** the drop, through the row index it had used
  **before** it. It reported `(no doc)`.
* Two more reused a folder row index across a drop, and failed once the weaker assertions above
  them were fixed.

**And two assertions were weak in the same way, which reverting exposed.** They compared
`ProjectFolders_Count()` — **and a move preserves the count.** So "a folder dropped on itself does
not move" and "into its own child is refused" both held whether the folder had moved or not, and
reverting the guards came back green. They compare **paths** now.

That is the sharpest version yet of this document's recurring shape. Not a claim nothing tested,
and not a guard nothing needed: **an assertion measuring a quantity the defect does not change.**

---

## Verification

| | |
| --- | --- |
| PsPlatform `build.cmd check` | 48 suites, 0 failures; `pstree` **342 → 356** |
| `_check_scihost` | green — the gate that catches a PsPlatform change reaching tiko |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **481 → 493** |
| `_check_selftests` | **33 / 20,328 in a clean tree** |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_package` | green |
| revert-to-red | PsPlatform: ignoring the refusal 354/2, never asking 350/6. tiko: a draggable group **crashes** (the guard is load-bearing and then some), the folder never set 492/1, the hook uninstalled 492/1 |

**NOT VERIFIED BY ME:** dragging with a mouse. The handler's decisions and the tree's
non-interference are asserted; the gesture is the author's.

---

## What is left

**`frmExplorer.inc` is ported.** 1,300 lines and 28 procedures, across steps 19, 21, 22, 23, 24
and 25 — tree, glyph, click-to-open, selection following the tab, context menu, folder
new/rename/delete, in-place editing, action icons, and drag and drop.

1. **The interactive pass** on the drag.
2. **Run step 18's Linux scripts** — three `.sh` files, still unrun, seven steps later.
3. **fontconfig** — still never executed anywhere.
4. **The next form.** 7c is 48 forms; the Explorer was the largest, and it is done.
