# Phase 7c, step 4 — the first real dock panel, and what a form actually costs

Steps 1–3 built a shell that lays out like tiko, collapsed the pump, and moved the document
model into `src/app`. **Every form was still a stub.** Step 4 ports one — the Bookmarks panel —
end to end, because the number the whole of 7c is sized on had never been measured.

`d2-decision.md` says so in as many words: the 14–20 week estimate "was not re-measured, and it
is the one that decides whether A was affordable rather than whether it was right." 49 forms and
45,309 lines hang off it.

**The panel works.** Bookmarks appear grouped by file, clicking a row jumps to it across tabs,
Ctrl+F2 / F2 / Shift+F2 / Ctrl+Shift+F2 all do what they do in tiko, and the marker is an icon in
the margin. Confirmed by the author on screen, which is the only way anything on this page was
confirmed.

---

## THE NUMBER

| | |
| --- | --- |
| the form ported | `frmBookmarks.inc` — **252 lines, 129 of them code** |
| its commands | `OnCommand_SearchBookmarks` — 5 arms, ~45 lines |
| what replaced it | `shell/shellpanel.bi` — **443 lines, 166 of them code** |
| plus wiring | ~50 lines across `tikoshell.bas` / `shelltabs.bi` / `shellhost.bi` |
| **port code written** | **~216 lines to replace ~160** — a ratio of **1.35** |
| assertions added | 63 (243 → **306**), 71 `Check` lines |
| total shell diff | **1,205 insertions / 58 deletions** across 4 files |
| commits | **8** — five planned, **three unplanned defect fixes** |

**READ THE 1.35 AS THE FLOOR, NOT THE FIGURE.** Bookmarks was chosen for being the *easiest*
real panel: its model was already portable, its control already existed on both sides, and it
needs no threading. The commit count is the honest half — **three of eight commits were fixing
defects that only appeared when the program ran.**

**AND THE RATIO EXCLUDES WHAT DID NOT PORT** (below). Counting those in, the same panel would
cost more, and two of them are blocked on things this port does not own.

---

## What is NOT verified, and what did not port

**FOUR DEFECTS, ALL FOUND BY THE AUTHOR RUNNING THE BINARY, NONE BY ANY GATE.** 306 assertions,
five gates, 27 tiko suites and 46 PsPlatform suites were green through every one of them:

1. **The binary died at startup** with a file argument — access violation, no window, no output.
2. **Ctrl+F2 threw the caret to line 1**, every time.
3. **The panel rows were striped**, which no bookmark list should be.
4. **A bookmarked line was highlighted end to end** instead of showing a margin icon.

Fourth step running, fourth time this is the finding. The suite could not have caught #1 in
particular *by construction* — it opens its files after the tree is built, and the crash is
about **when** the work happens, not what it does.

**WHAT DID NOT COME ACROSS FROM `frmBookmarks`:**

* **`NavHistory`** — tiko wraps both jumps so Back and Forward retrace them.
  `modNavHistory.inc` is **not linkable in this binary**: it reaches `gTTabCtl` and
  `pDoc->hWndActiveScintilla` at five sites. Only its *header* went into `app/` during step 3.
  A missing feature, not a difference of opinion.
* **The paint callback** (61 lines) — the widget paints itself, which is what forced the
  zebra-stripe workaround.
* **The tooltip callback** — no per-row tooltips.
* **`ExpandAll` / `CollapseAll`** — the control supports both; no command is wired to them.
* **`PreventDoubleClick`, the scrollbar colours and width** — PsListTree has no equivalent of
  the first and themes its own scrollbar.

**THE PANEL SHOWS BOOKMARKS ONLY.** tiko's side panel is three panes behind an icon strip —
Explorer, Functions, Bookmarks. There is no strip here and no way to switch. **Functions needs
`clsScanMgr`, which is blocked on PsPlatform having no threading at all; Explorer needs the
project system.** Neither is a bigger version of this work.

**ARROW KEYS JUMP, AND tiko's DO NOT.** See the divergence below.

**Nothing about the panel's appearance is asserted** — not the row height, the indent, the
twisty, the header colour, or the margin icon. No assertion in either repo reads a pixel.

---

## What is verified, and by what

| claim | how |
| --- | --- |
| a real file opens, registers with `gApp`, and does not open twice | group M, end to end on a file in `%TEMP%` |
| a background tab's bookmarks are ITS bookmarks | the two-document block — fails with the doc-pointer switch reverted |
| the packed `(tab, line)` slot | boundary values, plus read back off a real row |
| the four commands | driven through `OnMenuCommand`, not the model |
| Clear All spares breakpoints | a breakpoint is set, cleared past, and survives |
| clicking a row switches tab AND lands on the line | driven by row number in the two-document setup |
| a relayout arriving before the tree exists is dropped | the guard, both ways |
| the symbol margin has a width | three assertions; the icon itself is not covered |
| the layout did not move | dump byte-identical to the oracle at **every** commit |

**Every new rule was reverted to check its assertion went red.** Eleven were; **three were
not**, and each is recorded in place rather than left to be believed:

* `"a huge line number does not leak into the tab"` **survived three separate breakages** — a
  16-bit mask, a missing clamp, and `shl 16` — because it packed tab *zero*, which reads back
  zero whatever the arithmetic does. Rewritten against the last tab, it fails with the rest.
* `"the editor still holds its document"` does not bite on a missing `ADDREF`: with one file
  open the refcount has slack.
* The null guard in `IndexOfDoc` did not bite until an empty tab was seeded.

---

## The three PsPlatform gaps this found

None is fixed here. A port task should not be redesigning the toolkit it ports onto, and each is
recorded where the workaround lives.

1. **`PsListTree` has one item-data slot; tiko's Win32 control has two.** Worked around by
   packing `(tab index, line)` into the 64-bit slot — sound here because the shell is
   index-based, and *not* a general answer: a host with pointers to store has nowhere to put
   them.
2. **Row striping is unconditional** — `if (v and 1) = 1 then cBack = clrRowAlt`, no switch.
   Flattened by assigning `clrRowAlt = clrBack`, which **must be redone after every theme load**
   because `OnThemeChanged` re-reads both. The real fix is `SetAltRows(bOn)`.
3. **`OnSelChange` cannot tell a mouse selection from a keyboard one.** tiko jumps on a single
   click and does nothing on arrows; `OnSelChange` fires for both and `OnActivate` fires for
   neither. The shell wires `OnSelChange`, so **arrowing the list moves the editor** and the
   panel loses its own keyboard navigation. A source argument on the callback would fix it.

---

## Two places tiko disagrees with itself

Flagged, not changed — the author's call on their own editor.

1. **Clear All Bookmarks deletes breakpoints too.** `frmMainSearch.inc:449` passes `-1` to
   `SCI_MARKERDELETEALL` — every marker type — while the Clear-All-*Documents* arm four lines
   below passes `MARKER_BOOKMARK`. The shell passes `MARKER_BOOKMARK` in both.
2. **The Clear-All-Documents box defaults to Yes**, because no default is set and the first
   button wins. A reflexive Return deletes every bookmark in every open document. The shell
   defaults to No.

---

## What this step also fixed, which was not planned

**`ShellTabs_Open` created documents nothing in the app layer could see.** A bare
`new clsDocument` meant `gApp.pDocList` was permanently empty in this binary — so
`GetDocumentPtrByFilename`, `IsValidDocumentPointer`, `GetDocumentCount` and every panel tiko has
saw *no documents at all*. The tabs worked, so nothing said so. It surfaced only because a panel
needed the list.

Fixing it **deleted a field**: `ShellTabEntry.sPath` was a second copy of `DiskFilename`, with
the maintenance a second source always needs (a re-read after Save As). `gApp` owns the only one
now, and the tab array's one job is tab order.

---

## The shape the shell is now in

| | |
| --- | --- |
| `src/app` | 46 files, 9,901 lines, PsCore only, link debt 4 |
| `src/shell` | 4 files, **5,612 lines** |
| what it does | the full layout, menus, accelerators, dialogs, a file picker, N tabbed documents, open and save, **and one working dock panel** |
| what it does not | no Find, no Explorer, no Functions, no project system, no background parsing, no encoding detection, no close |

## What step 5 has to decide

1. **Threading in PsPlatform.** Functions is the next panel and `clsScanMgr` is blocked on it
   outright. This is the largest single blocker left and it is not tiko's to solve.
2. **The three PsListTree gaps** — whether they are fixed in the control or worked around again
   in every host that follows.
3. **Encoding detection on read**, still outstanding from step 3 and now reachable, because the
   shell saves.
4. **Whether the per-form ratio holds on a panel that is not the easy one.** 1.35 came from the
   panel with a portable model and an existing control. Functions has neither.
