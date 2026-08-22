# Phase 7c, step 24 — the action icons, and geometry that answered nothing

The last of tiko's Explorer. Up to three glyph buttons at the right of the hot row — add, rename,
delete — so the common operations need no right-click.

All three commands already existed. What made a button *inside* a row possible is `PsListTree`
handing a host a **left click with its position**: ten callbacks and not one of them did.
`pfnContext` does exactly that for the right button (step 21); this is its twin.

**Offered before the twisty test**, and that is the design. The original control offers its
equivalent *after* it has already toggled a header, and `LoadExplorerFiles` carries a paragraph
about working around exactly that — root groups had to become plain parents so the host could own
the gesture. Offering the click first means a host never has to arrange its data to get its
gestures back.

---

## One implementation, two callers

`ShellExplorer_IconRects` is called by the painter and the hit test alike. tiko states the rule in
its own header, and the failure it prevents is **an icon drawn one place and clicked another** —
which reads as the button not working rather than as a bug.

This version honours it better than tiko's. tiko's hit test has to **rebuild the row rect from the
control's client rect** and hope it matches what the painter drew into; here the overlay is handed
`pi->rc` and the click asks `RowRect`, so both callers pass the same rect from the same source.

**A group offers Add only**, and only where the category takes folders: its caption comes from
`gConfig.Cat` and its existence is what every file's `ProjectFiletype` names — the five categories
are structural, not user data. **Laid out from the right inwards**, so each row right-aligns its
own set and reading order stays Add, Rename, Delete. tiko's choice, with tiko's reason: a ragged
left edge across rows with different icon counts reads worse than a straight right one.

**The caption is already painted by the time an overlay runs**, which is this hook's one real
limit. tiko clips the text short of the icons *before* drawing it, because it replaces the whole
row. An overlay cannot, so the icons clear their band to the row's own resolved `clrBack` first.
Same result, different mechanism — worth saying, so the fill does not look stray.

---

## The defect the assertions found

The icon arithmetic came out with **a right edge of zero**, so every icon landed at negative x and
the hit test found nothing.

`PsLtEnsureLayout` was called from `OnPaint` and `OnEvent` **and nowhere else**. So a host asking
`RowRect`, `TwistyRect`, `HitTestRow`, `HitTestColumn`, `HitTestTwisty`, `RowIndentPx` or
`ItemsPerPage` **before anything had painted** got zeros and a `FALSE` — silently, and
indistinguishably from *"there is no such row"*.

Clicks were always fine, because `OnEvent` ensures. **Every other host use of geometry was
paint-order dependent.** All seven queries ensure now.

**And two assertions had already passed on the zeroed rect** — *"a folder offers all three"* and
*"in reading order"* are about widths and relative order, and both hold perfectly at the wrong
place. Relations again, for the third step running.

## The fixture tripped over step 23's own feature

With the layout fixed the hit test *still* failed, and the reason is a genuine interaction rather
than a fixture bug:

`NewFolder` ends by opening an editor (step 23). The folder-commands block above had run it twice.
The icon block's first `EnsureVisible` **scrolled**; the scroll **committed the edit** (step 23's
commit-on-scroll dance, working exactly as designed); the commit called `ShellExplorer_Load`; and
the rebuild landed in the middle of a block that had already resolved its row indices.

The fixture closes any open editor first now, and says why.

---

## Two harness failures, both caught, both worth recording

**Removing `PsLtEnsureLayout` from `RowRect` alone left the section green.** The other three
assertions each ensure the layout too, so whichever runs first repairs the state for the rest.
They constrain **the fix**, not any one call site — the test file says so, and making the helper a
no-op is what turns them red at 338/4.

**And four tiko reverts reported numbers that were nothings.** Every patch failed to apply — wrong
working directory — and the harness printed `481 passed, 0 failed` four times, which is exactly
what a green revert looks like. The `assert` inside the patch script is the only thing that said
otherwise. **The harness refuses to report a result when the patch did not apply now**, alongside
the checks it already had for a failed build and a crash.

That is three guards on one harness, each added after it lied once: build failure (step 20),
crash-with-no-summary (step 23), patch-did-not-apply (step 24).

---

## Verification

| | |
| --- | --- |
| PsPlatform `build.cmd check` | 48 suites, 0 failures; `pstree` **326 → 342** |
| `_check_scihost` | green — the gate that catches a PsPlatform change reaching tiko |
| `_compile_fast`, `_compile_shell` | exit 0, zero warnings |
| `_run_shell --selftest` | **464 → 481** |
| `_check_selftests` | **33 / 20,328 in a clean tree** — the two untracked files moved aside for the run, per step 23 |
| `_check_shell`, `_check_app_layer`, `_check_app_standalone`, `_check_package` | green |
| revert-to-red | two in PsPlatform (the click hook: 331/7 and 335/3; the layout helper: 338/4), four in tiko (480/1, 480/1, 479/2, 480/1) |

**NOT VERIFIED BY ME:** where the icons land, and whether clicking one feels right. The rect
arithmetic and the hit test are asserted; the drawing is not.

---

## What is left

**tiko's Explorer is ported.** Tree, glyph, click-to-open, selection following the tab, context
menu, folder new / rename / delete, in-place editing, action icons. What remains of
`frmExplorer.inc` is drag-and-drop — which needs `PsListTree`'s missing drop-validation callback,
the one gap from step 19's list that is still real.

1. **The interactive pass.**
2. **Drag and drop** — `SetDragReorder` exists with no way to veto a drop, so file-into-folder
   cannot be ported correctly.
3. **Run step 18's Linux scripts.**
4. **fontconfig** — still never executed anywhere.
