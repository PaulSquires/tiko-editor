# Phase 7c, step 21 — the right button was a defect

**There was no more Explorer progress on the tiko side without a PsPlatform change.** Step 20
narrowed what that meant: `PsTooltip`, `PsTextBox`, `PsPopupHost`, `PsTipHost` and `PsIconPanel`
all exist and are tested — **the controls do not route to them.**

This step adds two hooks to `PsListTree` and uses one of them.

---

## The defect

`PsListTree`'s `PSEV_MOUSE_DOWN` handler **never inspected `ev->mouse.button`.** A right-click:

* landed on the twisty test and **toggled the node**,
* otherwise selected the row and **armed a drag**, so a right-drag reordered rows,
* and returned `true`, so no host ever saw it.

Live in every host in the tree, PsPlatform's own demos included, for as long as the control has
existed — **because nothing had yet right-clicked a tree.** The same shape as step 17's
`PsSplitter`: a path no caller had taken.

**And the fix was wrong on its first attempt, in a way the suite caught.** It called
`PsLtClickRow`, which honours `selMode` — and in `PSLT_SEL_MULTI` a plain click **toggles**. So a
right-click outside a three-row selection made it four, and a menu would have acted on rows the
user had moved away from. The assertion read *"selects that row alone"* and got 4. **The
expectation was right and the implementation was not**, which is the less common way round.

`PsLtSelectOnly` is what a context click means in every mode, so it is spelled directly.

## The overlay, and the two fields that make it usable

`OnPaintRow` **replaces** the row: it runs first and a `true` return skips the built-in painter.
Right for a host that wants a different row, useless for one that wants the ordinary row **plus** a
glyph — painting over the top means running *after*, and there was no after.

`OnPaintOverlay` runs at the end of the per-row block. Its return is ignored: an overlay draws
over, there is nothing left to skip. It runs **even when the row was replaced wholesale** — *"the
overlay runs after the row is painted"* is a simpler contract than *"unless somebody else painted
it"*, and nothing stops one host being both.

**`PsLtPaintInfo` now carries the resolved `clrBack`/`clrText`**, and without them the hook would
be unusable: those fields are members of the control, so a host drawing in the row's foreground
would re-resolve them from `PsThemeColorK` and carry a second copy of the cascade — the exact
objection recorded in `7c-step19.md`. The cascade is hoisted above the pre-paint hook so both
hooks and the built-in path read one set of values.

**Asserted on a real paint, not on call counts.** A call-count test passes while the colours are
garbage. `pstree` 271 → 290.

---

## What the reverts bought, which is most of this step

| revert | result | what it bought |
| --- | --- | --- |
| drop the unique-name loop | 436/3 | — |
| **`DeleteFolder` stops checking the row kind** | **439/0** | an assertion that a file row denotes no category |
| `NewFolder` does not select the new row | 438/1 | — |
| `FolderPathFromRow` answers for a file row too | 439/1 | — |
| **`NewFolder` ignores `CatAllowsFolders`** | **440/0** | an assertion that New Folder is refused on a group that forbids folders |

**Two of the three guards I wrote in the shell are redundant early-outs.**

* `DeleteFolder`'s kind test — `FolderPathFromRow` answers `catIndex = -1` for every kind it does
  not handle, and the `catIndex < 0` test catches all of them.
* `NewFolder`'s `CatAllowsFolders` test — `ProjectFolders_Add` tests the same thing on its first
  line (`modProjectFolders.inc:225`).

Both are kept, and both are now **labelled as early-outs** so nobody reads the shell layer as the
place the rule lives. **It lives in the model** — written defensively long before it had a caller
— and that, rather than anything added in this step, is what protects Main and Resource from
growing folders.

That is a pleasant inversion of this document's usual finding. The recurring shape has been *a
claim nothing tested*; here it is **a guard nothing needed**, and the reason is that the layer
underneath was already careful.

---

## The Explorer's folder commands

New Folder and Delete Folder, on a group or folder row, through a `PsPopupMenu` opened by the new
context callback and routed through the `PsMenuHost` the shell already wires.

**Delete ports whole**: `ProjectFolders_Rebase` onto the parent — a *dissolve*, children re-attach
one level up, which is why tiko asks for no confirmation.

**New Folder stops one line short of tiko's**, which ends by opening the in-place editor on the row
it just made. That makes tiko's unique-name loop carry more weight here than there: in tiko the
generated name is a two-second placeholder; here it is **the name that stays**.

**Rename is absent from the menu rather than greyed.** A disabled item promises a feature.

---

## Verification

| | |
| --- | --- |
| PsPlatform `build.cmd check` | 48 suites, 0 failures; `pstree` **271 → 290** |
| `_check_scihost` | green — **the gate that catches a PsPlatform change reaching tiko**, and the page records `PsTheme` shipping green in PsPlatform while breaking tiko's link |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **426 → 443** |
| `_check_selftests` | 33 suites, 20,328 passed, 0 failed; `settings/` and `tiko.tiko` unchanged |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_package` | green |
| revert-to-red | five rules; three bit, and **the two that did not each bought an assertion** |

**Two process notes.** The revert harness now restores from a **file snapshot** and refuses to
report a number when the build failed — step 19 lost uncommitted work to `git checkout --` and
step 20 nearly read a build failure as a red.

**And a patch helper produced `CR CR LF` line endings** by appending a CRLF block into a CRLF
file. fbc counted the extra CR as content: **its error line numbers drifted fourteen lines from
the file's**, and every byte-exact match afterwards failed against bytes that looked identical
under `cat -A`. The helper collapses them now.

**NOT VERIFIED BY ME:** the menu on screen. `OpenRoot` needs a compositor, so everything asserted
here is what the menu would *do* once it closed.

---

## What is left

1. **The interactive pass** — the menu, the strip, the Explorer, and whether any glyph renders.
2. **The Explorer's file/folder glyph**, now that the overlay exists. Held back deliberately until
   the strip's Segoe MDL2 glyphs are known to render at all.
3. **Label edit** on `PsListTree`, wired to the `PsTextBox` that already exists, and folder rename
   behind it.
4. **Run step 18's Linux scripts.**
5. **fontconfig** — still never executed anywhere.
