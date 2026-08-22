# Phase 7c, step 19 — the Explorer pane

The side panel was two of three. `shellpanel.bi` named the missing one on itself, and step 8 had
already had to work around its absence — the shell starts on Functions because *"a bookmarks pane
on a freshly opened file is empty by construction"* and the pane tiko actually opens with did not
exist.

`frmExplorer.inc` is **1,300 lines and 28 procedures** against ~160 for Bookmarks. This is its
read-only half: the tree, the row scheme, click-to-open, and selection following the tab.

---

## The model needed nothing

Not one line of it. `gProjectFolders`, `ProjectFolders_ParentPath` / `LeafName` / `Exists`,
`gConfig.Cat`, `gApp.pDocList`, `gApp.IsProjectNamed`, `clsDocument.docData.wszFolder` and
`ProjectFiletype` are all in `app/` already, and the loader calls them exactly as tiko does.

**What was ported is the control half and the row scheme** — which is the first time in this port
that has been the whole of a form's cost, and is what fourteen steps of moving things into `app/`
were for.

## Two slots, three meanings

`PsListTree` has `itemData` and `itemData2`; **so does tiko's control**, so the scheme transfers
rather than being re-invented. But slot 2 already meant *line number* for the other two panes, and
the Explorer needs it for the row KIND.

So **slot 2 is a function of the MODE**, as slot 1 is a function of the KIND — which is
`frmExplorer.bi`'s own rule one level up:

| mode | slot 1 | slot 2 |
| --- | --- | --- |
| Bookmarks / Functions | file index | line |
| Explorer, `EXPKIND_ROOT` | `gConfig.Cat()` index | kind |
| Explorer, `EXPKIND_FOLDER` | `gProjectFolders` index | kind |
| Explorer, `EXPKIND_FILE` | **file index**, not a `clsDocument ptr` | kind |

The guard lives inside `ShellPanel_LineOf` rather than at every caller. Without it a click on a
file row reads `EXPKIND_FILE`, which is **3**, and throws the caret to line 3 of the file it just
opened — a defect that reads as a scroll bug and is a units bug.

**And `GotoRow` needed a second guard.** The other panes have one kind of clickable row, so *"is
it a header"* was the whole test. The Explorer has four kinds, three of them are not places, and
**none of the three is a header** — a root group is a plain parent on purpose. A folder row would
have been read as a file index and opened a real file, silently the wrong one.

---

## Three defects, and each was found by a different thing

### 1. The pane rendered nothing, and nothing said so

`gConfig.SetCategoryDefaults` sat in `src/clsConfig.inc` — the *shell* half of the split class —
and was called from exactly one place: `tiko.bas`. So in `tikoshell` `ubound(gConfig.Cat)` was
**-1**, every category loop ran zero times, and the Explorer drew an empty tree.

**Not an error and not a warning.** An empty tree is exactly what a workspace with no files in it
looks like.

It is six filetype ids and six localized captions with no Win32 anywhere; it moved to
`app/clsConfig.inc` unchanged. It stays out of the constructor for tiko's own reason — the
captions are `L()` lookups, so the localization file has to load first.

**Found by writing the assertion as a count** — *one root group per displayed category* — rather
than as *"more than zero rows"*. The weaker form would have been green.

### 2. A passing assertion whose own evidence contradicted it

```
ok    SelectPath lands on that row  (-1 wanted 2)
```

`Check "…", SelectPath(f) andalso (GetCurSel() = nHit), str(GetCurSel()) & …`. **fbc does not
promise the order it evaluates arguments in**, and it built the MESSAGE before running the call
that changes what the message reports.

The assertion was right and unreadable. Anyone reading that log would have gone looking for a bug
that was not there. Both results go into locals first now.

### 3. A guard tiko has never actually run

`frmExplorer.inc:109` is `if PsListTree_IsCollapsed(hCtl, i) then return false`, and it was ported
as written. **`IsCollapsed` asks whether row `i` is itself collapsed** — whether its own subtree is
folded shut. **A file row is a leaf.** It has no subtree, so it is never collapsed, so the test is
false for every row it is ever applied to.

The question being asked is *can the user see this row*, and the answer is the visible map:
`ModelToVisible` returns -1 for a row hidden under a folded ancestor at any depth.

**Reverting the ported guard changed nothing** — 411/0 either way — which is what prompted writing
an assertion for it at all. No fixture had ever collapsed anything, so the guard was carried by
prose.

**And the fix exposed a second one immediately.** With the loop corrected the assertion *still*
failed: the early *"it is already selected"* branch returns true before the loop is reached, and a
selection **survives a collapse** — it is a model index and the fold does not clear it. So
`SelectPath` answered *"yes, it is selected"* for a row no longer on screen, and the new guard sat
three lines below it, unreachable. tiko's version has the same early-out and does not ask either.

---

## The painter was dropped, and the reason is a toolkit gap

The plan said this step would carry a glyph painter. **It does not, and that is a finding rather
than a cut.**

`PsListTree`'s paint hook runs **before** the built-in painter and returning `true` skips the
built-in entirely — its own comment says *"A HOST PAINTER REPLACES THE ROW WHOLESALE"*. There is
no overlay point, so a glyph column means re-implementing the row: background, hot/selected
states, the twisty, the indent and the text.

**And the host cannot do that faithfully**, because the resolved colours are private members of
`PsListTree` (`clrBack`, `clrHot`, `clrSel`, `clrText`, `clrTwisty`) with no getters. A host
painter would have to re-resolve them from `PsThemeColorK` with the same keys — a second copy of
the control's own theme resolution, drifting the moment either side changes.

Deferred as **a fifth PsPlatform gap**: the paint hook is replace-only, and what this wants is an
overlay called after the default row paint. The tree renders correctly meanwhile, with the
control's own twisty, indent, text and theme colours; what is missing is the file/folder glyph.

---

## What is deferred, and each is a named PsPlatform gap

`PsListTree` has six callbacks; tiko's Win32 control has paint, message, can-drop, begin/end label
edit and tooltip.

1. **Drop validation** — no `CanDropCallback`. `SetDragReorder` exists with no way to veto a drop,
   so file-into-folder reordering cannot be ported correctly.
2. **In-place label edit** — no begin/end hooks. Folder **new / rename / delete** are all built on
   it, so all three defer with it.
3. **Tooltips** — no hook.
4. **A row-paint OVERLAY** — see above. New in this step.
5. **Raw message callback** — deliberately NOT wanted. `frmExplorer_MessageCallback` is 180 lines
   of `WM_*`; the portable half is the two row callbacks that already exist.

Also deferred: the action icons and the folder context menu (both need 2 and 4), and the
`PsIconPanel` strip — PsPlatform has no equivalent, and the mode enum already does what the strip
does for the user.

---

## Verification

| | |
| --- | --- |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **397 → 414** |
| `_check_selftests` | **33 suites, 20,328 passed, 0 failed**; `settings/` and `tiko.tiko` unchanged |
| `_check_shell` | 5 shell files, 53 files for the separator rules |
| `_check_app_layer` | 48 files |
| `_check_app_standalone` | 18 clean, 0 errors, debt 0 |
| `_check_scihost`, `_check_package` | green |
| PsPlatform `build.cmd check` | **48 suites, 0 failures** |

**Reverted to red, one rule at a time** — the mode guard in `LineOf` (410/1), the kind guard in
`GotoRow` (410/1), the `SetCategoryDefaults` call (410/1), `SetRowSelectable(false)` on a root
group (410/1), file rows tagged `EXPKIND_NONE` (405/6), the loop's visibility test (413/1), and
the early-out's (413/1). Each fails its own assertion.

**AND THE REVERT HARNESS DESTROYED UNCOMMITTED WORK, WHICH IS THE PROCESS FINDING.** Restoring
with `git checkout -- <file>` reverts to **HEAD**, not to the pre-patch state — so two reverts run
against a half-built tree, the build failed, and the runs printed nothing at all. A build failure
is not a red; it is a nothing wearing a red's clothes. The restore is a file snapshot now, and the
harness refuses to report a result when the build did not succeed.

**NOT VERIFIED BY ME:** what the pane looks like. Every assertion here is about the tree's shape,
and the port's record on that distinction is unambiguous — four of step 4's defects and five of
step 3's were found by the author running the binary and none by any gate.

**Also not verified:** anything on Linux. Step 18's three `.sh` files are still unrun.

---

## What is left

1. **The interactive pass**, which is the gate that matters for a panel.
2. **The five PsPlatform gaps above**, in the order folder CRUD needs them: label edit, then drop
   validation, then the paint overlay.
3. **Run step 18's Linux scripts.**
4. **fontconfig** — still never executed anywhere.
