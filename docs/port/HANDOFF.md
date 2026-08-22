# Handoff — the tiko → PsPlatform port

**7c STEP 30 COMPLETE. THE FOUR SELECTION RULES COME BACK FROM tiko.** Four defects were reported
against tiko's Selection icon over three rounds; the shell is the PORT of that bar and had two of
them. **This step is not a port of code -- it is a port of four things that were learned by hand.**
One capture rule in one place (the arm had carried its own copy since step 29), and a disarm that
drops the text selection but a REFUSAL that does not.

**AND IT IS THE FIRST PLACE IN THE RUN WHERE A CONDITION WAS PINNED RATHER THAN A BRANCH:** the
guard is reverted to `false` AND to `true`, each failing exactly one assertion. One revert would
have proved only that the branch runs sometimes. `--selftest` 566 -> 571, all five driving the
GESTURE -- open, select, click.

---

**AND THE SAME BUG WAS IN tiko, WHICH IS THE BINARY IT WAS REPORTED AGAINST.** Two rounds of fixes
went into **tikoshell** before a screenshot settled it: `"Aa"` and `"W"` sitting INSIDE the field
frame is tiko's arrangement, and `wszIconMatchCase`/`wszIconWholeWord` are those literal strings.
The shell's toggles are PUA glyphs in a strip outside it. **Read the binary off the screenshot before
diagnosing the code.**

**tiko's cause is structural, not intermittent.** `CurrentSelection` has exactly two writers -- the
editor's `SCEN_KILLFOCUS` fills it, the editor's `SCEN_SETFOCUS` clears it -- so **while the editor
has the focus it is always uninitialized, and that is when Ctrl+F is pressed.** `FindControls_Show`
checks `isInitialized` before believing the line range, so the multi-line branch could never run:
no clearing, no arming, no markers, nothing for the search to narrow to. `FindReplace_
EnsureSelectionCaptured` does what `SCEN_KILLFOCUS` would have, at the moment it is needed.

**ASSERTED IN THE REAL BINARY**, in the `TIKO_FINDPROJ_*` family: `TIKO_FINDSEL_SELFTEST=1` opens a
six-line temp file, selects lines 1..3 **the way a drag does** (column 0 to column 0, because
`GetSelectedLineRange` decrements `endLine` at column 0), throws the capture away to reproduce the
focused-editor state, and drives Ctrl+F. Nine assertions. **Reverted, it reproduces the report line
for line** -- `the box is EMPTY, not seeded with the selected text   got: dim Zeta as long`.

---

**7c STEP 29 COMPLETE. SELECTION AND THE SEEDING.** The two halves step 27 deferred, and they
arrive together because they cannot be separated: the rule is *"the box is a picture of the
selection"*, and the same code is where the **case-only echo** lives. Porting the rule without the
exception would have re-imported a bug the author had fixed by request three steps earlier.

**THE CAPTURE IS NOT PORTED FROM WHERE tiko KEEPS IT.** tiko fills `CurrentSelection` from
`SCEN_KILLFOCUS` because its bar is a separate `HWND`. This shell has no such moment, and building
one means handling `PSEV_FOCUS_LOSE` in `PsSciView` -- **a PsPlatform change**. It is also
unnecessary: focus is not what clobbers the selection, the incremental search is. Read at SHOW
TIME instead.

**AND AN ASSERTION FOUND WHAT THAT DECISION COST, IN CODE WRITTEN FOR A REASON I HAD JUST
DOCUMENTED.** Arming Selection laid down no markers: **two notions of "the selection" are in play**
-- the toggle's RULE reads the captured one, `SetMarkerHighlight` reads the LIVE one, which by then
is the last match. It would have marked that match's line, or on a one-line match marked NOTHING
while leaving the flag set. tiko's `SCEN_SETFOCUS` restore is now done at that one moment. The
header had predicted the gap in the abstract and still got the consequence wrong.

**SELECTION IS THE ONLY TOGGLE ON EITHER BAR THAT CAN REFUSE TO LATCH**, which makes it the first
real customer for step 28's `SyncToggles` -- there the machinery closed a staleness gap; here it is
the only path by which a decision reaches the control.

**A FIFTH UNASSERTED WIRING, IN THE FIFTH SUCH STEP.** Gutting the capture left 556/0, because
every assertion set `CurrentSelection` BY HAND. Steps 22, 23, 24, 27, 29. Five assertions now drive
the whole gesture -- a real multi-line selection, close, open, capture then seed then arm -- which
also turned out to be the only cover for the multi-line seeding branch.

**NOT PORTED, AND NAMED:** the general `SCEN_SETFOCUS` restore. After a search the live selection is
the last match rather than what the user had, everywhere but the Selection arm. Closing that is a
PsPlatform change.

**AND THE PASS FOUND THAT SELECTION DID NOT WORK, TWICE OVER.** The capture runs at SHOW time and
nothing re-ran it, so selecting lines AFTER opening Find was invisible and the icon refused -- the
"searching ignores the selection" half was that one's SHADOW. And `= true` is **-1** while the shell
wrote **1**, so the engine's navigation range stayed the whole document; **every assertion here
tested `<> 0`, which is true of both.** Fixed, with five assertions covering the end-to-end path
nothing did. **A comment had the mechanism wrong too**: the restriction is the MARKERS, not the flag.

[`7c-step29.md`](7c-step29.md) carries both, and the pass.

---

**7c STEP 28 COMPLETE. THE SHELL REPLACES.** `Ctrl+H` opens the pair -- Find above, Replace below,
with Preserve Case inside the field's frame and Replace / Replace All outside it. **Replace implies
Find in both directions**, because the engine reads `gFind.txtFind` and a Replace bar over a hidden
Find bar replaces the empty string.

**AND PORTING THE SIBLING FOUND THE DEFECT STEP 27 SHIPPED.** tiko makes Preserve Case a COMMAND and
paints its latch from `gFind`, saying why: *"the control never keeps a second copy of state
FindReplace_DoReplace already reads."* **A `PSICON_TOGGLE` is exactly that second copy** -- and tiko
HAS a site that breaks it, `frmFindInProject.inc:3140`, which clears `nMatchCase` and `nWholeWord`
behind the bar's back. It would have arrived **silently, with Find in Project**. Closed by a push
from the model into the items at every show. **First step where the port of a sibling, rather than a
gate or an interactive pass, is what found it.**

**THE FIXTURE WAS WRONG THREE TIMES AND THE CODE ONCE.** Replace is a TWO-PRESS gesture from a cold
caret (the single branch compares the selection to the phrase and returns without replacing when
they differ); layout is LAZY, so both fields were 0 wide and *"the two share a left edge"* passed as
`0 = 0` -- step 24's finding in a second control; and that is what exposed the real bug, **a child's
`bounds` are its PARENT's coordinates**, subtracted once too often for `-413` against `6`.

**ONE REVERT CAME BACK GREEN, AND THE COMMENT WAS THE THING THAT WAS WRONG.** Deleting the
`foundCount` guard left 537/0. My comment had said it stops DoReplace *"operating on wherever the
caret happens to be"* -- **invented**; tiko has the line and gives no reason. The guard stays for
fidelity, the comment now says its necessity could not be shown, and the assertion is relabelled so
it stops implying coverage it does not have.

**A CORRECTION TO THE ENTRY BELOW:** `_compile_shell` emits **one warning 38**, in the step-22
pane-switcher assertions, and it does so on `HEAD` too. Step 27's page said "zero warnings" for both
compile gates; **that was measured on `_compile_fast` only.**

**THE INTERACTIVE PASS RAN FOR BOTH BARS AND FOUND NOTHING, 2026-08-22.** Steps 27 and 28 are the
first pair in this run to survive it clean -- step 26's found four things in under a minute. Every
defect in these two was found by an ASSERTION or a REVERT before the author ever saw the bars.

[`7c-step28.md`](7c-step28.md) carries the pass, and
names the one piece read from a header rather than observed -- Preserve Case's glyph is the literal
text `"AB"`, resolved by the text engine's per-character fallback chain.

---

**7c STEP 27 COMPLETE. THE SHELL SEARCHES.** `Ctrl+F` opens a real bar -- field, Match Case,
Whole Word, prev/next/close, and an `n/m` count -- and it is the first editor feature in this
binary beyond typing. It does not search: `app/modFindReplace.inc` does, since step 26, and the
bar only writes `gFind` and calls it. **That split is the whole reason the engine moved first.**

**EVERY COUNT IN THE ONE ASSERTION THAT MATTERS WAS WRONG ON THE FIRST RUN.** I asserted 3 matches
for `"Probe"` because the probe file has three procedures; the engine found **5** and was right --
the file's comment and an `#include` filename contain it too. Whole Word 1, Match Case 2, likewise.
**The engine was correct in every case and my fixture was wrong in every case**, which is step 26's
failure one layer along: an expectation asserted without being derived. They are derived in a
comment now.

**AND A FOURTH CONSECUTIVE STEP FOUND AN UNASSERTED WIRING.** Gutting the toggle handler left the
suite green, because every assertion above it set `gFind.nMatchCase` **directly** and none drove
the handler that is supposed to. Steps 22, 23, 24 and now 27: the policy tested by calling the
callback, the wiring never driven.

**NOT ASSERTABLE HEADLESSLY, and said rather than papered over:** `RefreshFindBar` doing nothing
changes no assertion, because a repaint has no model effect. Its **installation** is gated -- the
seam's completeness check exits 2 if it is unset, which is how step 26 found it.

**THE LAYOUT ORACLE DID NOT MOVE.** The band's rect is the layout's, exactly as it was when it was
a stub, so plan item E turned out to be nothing to do.

**NOT VERIFIED BY ME:** the bar on screen. [`7c-step27.md`](7c-step27.md) carries the five-gesture
pass -- Ctrl+F, typing, F3/Shift+F3 and the wrap, and the two toggles each changing the count.

---

**7c STEP 26 COMPLETE. THE FIND/REPLACE ENGINE IS IN app/. **

**AND THE SEARCH HALF WAS ALREADY PORTABLE.** Every function was `SciExec` on an HWND -- and
SciExec IS SendMessage, which `app/modScintilla.bi` has declared in portable types since step 3
saying exactly that. The port is a rename at sixty call sites and an HWND that becomes an any ptr.
**Each of the three shell reaches had a seam already waiting**: TabDocAt/TabActiveIndex from step
13, GetActiveScintillaPtr from step 3, and `gAppHost.SetViewRedraw`, **which existed and had never
had a caller.**

**THE SEAM CAUGHT THE ONE NEW FIELD BEFORE A SINGLE ASSERTION RAN** -- "tikoshell:
AppNotify.RefreshFindBar is not set (build error)", exit 2. tikoshell's body for it is EMPTY AND
CORRECT: it has no find bar. That is the distinction this seam is built on -- a Notify field can
legitimately be empty and no Services field can -- and twenty-six steps in, this is the first time
the check has had to say so about a field added in the same commit. **An empty body is a decision;
an unset pointer is an omission.**

**isupper/islower ARE GONE, TESTED INLINE AS ASCII.** crt/ctype.bi would have brought a
LOCALE-DEPENDENT answer above 0x7F into a layer where PsUCase and SymDb_NameEqW both fold ASCII
only -- and the difference would present as "replace mangled my accented text" on one machine and
not another.

**AND ONE OF THE SIX FUNCTIONS WAS NOT PORTED AT ALL -- IT WAS RECONSTRUCTED FROM ITS DOC
COMMENT.** My read of the original had been truncated, and rather than going back for the
remaining thirty lines I wrote an implementation of what the comment described. It compiled, it
linked, it left 20,328 assertions standing, and **the author found it in under a minute**: Match
Case and a tab switch both left the old n/m on screen until F3. Three early returns the original
does not have. Restored verbatim, and the other five then checked MECHANICALLY -- **the first
attempt at that check reported all six identical and was a NOTHING**, because the extractor failed
on every file and `diff -q` on two empty files says they match.

**THE INTERACTIVE PASS RAN AND FOUND FOUR THINGS. THREE WERE MINE.** In order: Match Case and a
tab switch left the old n/m on screen (the reconstructed function); Replace and Replace All
replaced with an EMPTY STRING (`gFind.txtReplace` has never been written by anything, and
DoReplace used to read the control directly); and Replace behaved like Replace All -- **the new
colour parameter went FIRST, boolean converts to ulong silently, and all three call sites shifted
one place**. The fourth was pre-existing and changed by request: the find term echoing back in the
document's casing, now skipped at BOTH reseed sites when it differs only in case.

**THE PATTERN ACROSS ALL THREE IS ONE THING: AN EQUIVALENCE ASSERTED WITHOUT BEING CHECKED.** That
a body matched its comment. That two sources of a string agreed -- asserted in a commit message,
when one grep showed txtFind with seven writers and txtReplace with none. That adding a parameter
was additive. Each was cheap to verify and none was verified.

**AND `_check_selftests` WAS GREEN THROUGH ALL OF IT** -- 20,328 assertions, zero failures, on a
find engine that could not update a count, could not replace text, and replaced the wrong number
of matches. It floors the assertion total now as well as the suite count, **and that would not
have caught these either**: re-introducing the first defect deliberately left the total unchanged.

Confirmed working by the author since: the count, F3/Shift+F3 and the wrap, Match Case, Whole
Word, Selection, Replace, Replace All, Preserve Case, and the tab switch.

**THIS IS A MOVE OF LIVE CODE, NOT AN ADDITION**, and the gates cannot see what matters. Every one
of them proves it compiles, links and leaves 20,328 assertions standing; **none proves it still
finds anything.** [`7c-step26.md`](7c-step26.md) carries the six-item interactive pass it needs.

---

**7c STEP 25 COMPLETE. frmExplorer.inc IS PORTED -- ALL 1,300 LINES AND 28 PROCEDURES.**

Steps 19, 21, 22, 23, 24 and 25: tree, glyph, click-to-open, selection following the tab, context
menu, folder new/rename/delete, in-place editing, action icons, and now DRAG AND DROP -- the last
item on step 19's list of PsPlatform gaps.

**THE NAME WAS THE TRAP.** tiko's `frmExplorer_CanDropCallback` returns FALSE at the end, ALWAYS.
It is not a validator that permits a reorder -- it performs the whole move itself and then refuses
the control's. Reading it as a permission check would have produced the wrong hook, in the other
repo, for every future host to inherit. `PsLtDropProc` is "the host handles this drop" instead.

**FOUR MORE REDUNDANT SHELL-SIDE GUARDS, THE THIRD TIME THIS HAS COME UP.**
ProjectFolders_MoveFolder tests onto-itself and into-a-descendant on its own lines 409-412.
**modProjectFolders was written DEFENSIVELY long before it had a caller**, so a handler ported
from tiko restates rules the model already enforces. All labelled as early-outs. And one guard is
UNTESTED rather than redundant, which is a different thing and now says so.

**AND THE SUITE BROKE THIS HANDLER'S OWN FIRST RULE, TWICE.** The handler snapshots its source
before touching anything, because every drop reloads and row indices stop meaning anything. Three
assertions reused an index across a drop. **AND TWO MORE COMPARED ProjectFolders_Count() -- A MOVE
PRESERVES THE COUNT** -- so they held whether the folder had moved or not, and reverting the
guards came back green. They compare paths now.

That is the sharpest version yet of this page's recurring shape: not a claim nothing tested, and
not a guard nothing needed, but **an assertion measuring a quantity the defect does not change.**
See [`7c-step25.md`](7c-step25.md).

---

**7c STEP 24 COMPLETE. tiko's EXPLORER IS PORTED.**

Tree, glyph, click-to-open, selection following the tab, context menu, folder new / rename /
delete, in-place editing, and now the ACTION ICONS. What remains of `frmExplorer.inc` is
drag-and-drop, which needs the one gap from step 19's list that is still real.

**A LEFT CLICK WITH ITS POSITION** -- ten callbacks and not one of them did. Offered BEFORE the
twisty test, which is the design: the original offers its equivalent after it has already toggled
a header, and `LoadExplorerFiles` carries a paragraph about working around exactly that.

**AND PsListTree'S GEOMETRY ANSWERED NOTHING BEFORE THE FIRST PAINT.** PsLtEnsureLayout was called
from OnPaint and OnEvent and nowhere else, so RowRect, TwistyRect, HitTestRow and four more
returned zeros and a FALSE -- silently, indistinguishable from "there is no such row". Clicks were
fine because OnEvent ensures; every OTHER host use of geometry was paint-order dependent. Found by
an assertion that laid icons out from a row rect and got a right edge of ZERO.

**AND TWO ASSERTIONS HAD ALREADY PASSED ON THAT ZEROED RECT** -- "offers all three" and "in
reading order" are about widths and relative order, and both hold perfectly at the wrong place.
Relations again, third step running.

**THE FIXTURE THEN TRIPPED OVER STEP 23'S OWN FEATURE**, which is a real interaction rather than a
fixture bug: NewFolder ends by opening an editor, the first EnsureVisible SCROLLED, the scroll
COMMITTED the edit, the commit rebuilt the tree, and the rebuild landed in a block that had
already resolved its row indices. The commit-on-scroll dance working exactly as designed.

**AND FOUR REVERTS REPORTED NUMBERS THAT WERE NOTHINGS** -- every patch failed to apply (wrong
directory) and the harness printed "481 passed, 0 failed" four times, which is what a green revert
looks like. **THAT IS THREE GUARDS ON ONE HARNESS, EACH ADDED AFTER IT LIED ONCE:** build failure
(step 20), crash-with-no-summary (step 23), patch-did-not-apply (step 24).
See [`7c-step24.md`](7c-step24.md).

---

**7c STEP 23 COMPLETE. PsListTree EDITS LABELS, AND FOLDERS RENAME.**

**A NOTE THAT OUTLIVED ITS OWN CONDITION.** PsListTree's header carried label editing under WHAT
IS NOT PORTED, blocked on "a real single-line editor with a caret and a selection, WHICH IS
PsTextBox'S PHASE". PsTextBox landed, and the paragraph went on saying the feature was blocked on
it. Step 19's correction put that lesson in the table below; here it is again, and the header
says so now instead.

**NOTHING WAS ADDED TO EITHER CONTROL TO MAKE THE KEYS WORK.** Enter is PsTextBox's existing
OnEnterPressed; Escape arrives by BUBBLING, because PsTextBox leaves it unclaimed on purpose and
PsDispatch walks the parent chain; focus loss is OnFocusChange.

**THE COMMIT-ON-SCROLL DANCE HAS TWO CHOKEPOINTS, NOT ELEVEN** -- PsLtDirty for every structural
change and PsLtSetTopVis for every scroll -- and the scroll hook sits INSIDE the "did it move"
test, so a clamped no-op cannot close the field the user is typing into.

**AND THE INSTALLATION WAS UNASSERTED AGAIN, IN THE VERY NEXT STEP, ON THE LIST STEP 22 CREATED
FOR IT.** The rename policy is asserted by calling the callbacks DIRECTLY, so deleting the lines
that install them changed nothing. Twice in two steps is the reason that list exists.

**A GATE NUMBER TURNS OUT TO DEPEND ON STRAY FILES.** `_check_selftests` read 34 suites / 20,362
where this page says 33 / 20,328, stable across three runs, with tiko.exe's sources unchanged --
**and it is the two untracked scratch files in the tiko root.** Moved aside: 33 / 20,328 exactly.
The gate survives because it asserts a FLOOR rather than a total. See [`7c-step23.md`](7c-step23.md).

---

**7c STEP 22 COMPLETE. THE EXPLORER'S ROWS HAVE THEIR GLYPH.**

**CONFIRMED BY THE AUTHOR: THE PANE SWITCHER'S GLYPHS RENDER.** Step 20 shipped Segoe MDL2
private-use codepoints that nothing on this machine could see, and step 21 held the Explorer's
glyph back rather than build a second feature on the same unproven assumption. That assumption is
now a fact.

**AND THE OVERLAY DRAWS LESS THAN tiko'S PAINTER, which is what the toolkit catching up looks
like.** tiko replaces the row wholesale and paints a chevron, a dot and the caption, because it
leaves SetTreeIndent OFF and does its own indent arithmetic. This panel has SetTreeIndent and
ShowTwisty ON, so PsListTree draws the chevron and reserves its band; the overlay adds the ONE
thing the control has no notion of -- a file row's U+00B7.

**U+00B7 IS NOT A PRIVATE-USE CODEPOINT AND THE ASSERTION CHECKS THE VALUE**, not "not empty". A
middle dot is in every text font and cannot come out as a box, which is why this glyph was safe to
write before any interactive pass where the switcher's were not -- and `PsLen(g) = 1` would have
been satisfied by a PUA character just as well.

**THEN REMOVING THE OnPaintOverlay CALL LEFT ALL FIVE ASSERTIONS GREEN.** They test a pure
function that NOTHING HAD TO BE CALLING. A correct decision reaching no painter is a pane with no
icons and a suite that says otherwise. Three assertions added that the hooks are installed at all
-- and **the two row callbacks carrying every click since step 4 had never been checked either.**
They work, so nothing ever asked. See [`7c-step22.md`](7c-step22.md).

---

**7c STEP 21 COMPLETE. PsListTree HEARS THE RIGHT BUTTON, AND THE EXPLORER HAS FOLDERS.**

**IT WAS A DEFECT, NOT A GAP.** `PsListTree`'s PSEV_MOUSE_DOWN never inspected
`ev->mouse.button`: a right-click landed on the twisty test and TOGGLED THE NODE, or else
selected the row and ARMED A DRAG -- so a right-drag reordered rows -- and then returned true, so
no host ever saw it. Live in every host in the tree, the demos included, for as long as the
control has existed. Same shape as step 17's PsSplitter: a path no caller had taken.

**AND THE FIX WAS WRONG FIRST, THE LESS COMMON WAY ROUND.** It called PsLtClickRow, which honours
selMode -- and in PSLT_SEL_MULTI a plain click TOGGLES, so a right-click outside a three-row
selection made it four. The assertion read "selects that row alone" and got 4: **the EXPECTATION
was right and the implementation was not.**

**PsListTree ALSO HAS A PAINT OVERLAY NOW**, and `PsLtPaintInfo` carries the RESOLVED
clrBack/clrText -- without which the hook is unusable, because a host drawing in the row's
foreground would carry a second copy of the control's cascade. Asserted on a REAL PAINT: a
call-count test passes while the colours are garbage. pstree 271 -> 290.

**AND TWO OF THE THREE GUARDS I WROTE IN THE SHELL ARE REDUNDANT, which reverting is what said
so.** DeleteFolder's kind test and NewFolder's CatAllowsFolders test can each be removed with the
suite still green: FolderPathFromRow answers -1 for every kind it does not handle, and
ProjectFolders_Add tests CatAllowsFolders on its first line. Both are kept and both are LABELLED
as early-outs -- **the rule lives in the model, written defensively long before it had a caller.**

That is a pleasant inversion of this page's usual finding. The recurring shape is *a claim nothing
tested*; this time it is **a guard nothing needed**, because the layer underneath was already
careful. Each green revert still bought an assertion for the guard that does fire.
See [`7c-step21.md`](7c-step21.md).

---

**7c STEP 20 COMPLETE. THE SIDE PANEL HAS ITS PANE SWITCHER.**

tiko's `frmPanelMenu`, ported -- and **the first time in twenty steps the toolkit made something
SHORTER than the original.** tiko's items are COMMAND items and its painter hand-checks each id
against the current pane to draw the highlight; PsIconPanel carries selection ON THE ITEM and has
`SelectExclusive`, so three TOGGLE items give the active-pane highlight from the BUILT-IN painter
with no host painting at all.

**THE FIRST THREE REVERTS ALL CAME BACK GREEN, AND THAT IS THE STEP.** Removing the mode sync,
making the items COMMAND so nothing can latch, and setting the strip's height to **ZERO** each
left the suite at 417/0. The geometry assertions written for the new band are RELATIONS -- strip
above tree, union equals tiko's band -- and **a zero-height strip satisfies every one of them**
(`0 + 802 = 802`). That is step 1's finding word for word, nineteen steps later, in a file that
quotes it. Nine behaviour assertions and one NUMBER later, the same four reverts give 422/4,
421/5, 425/1, 425/1.

**AND THE ORACLE HAS ONE ROW WHERE THE SHELL NOW HAS TWO**, which is not a difference:
`HWND_FRMPANEL` is a CONTAINER in tiko and `modLayoutDump` dumps the container. Asserted as the
UNION -- 53 + 749 = 802 -- rather than by hand-editing the oracle's PANEL row, because the union
is what tiko pins and either rect alone can be wrong in a way the other cancels.

**THE GAPS ARE ONE GAP.** `PsIconPanel` has `OnClick` and nothing else; `PsListTree`'s paint hook
replaces the row wholesale. Across three controls it is a single shape: **PsPlatform's controls
carry the model and the default painting, and what they lack is host PAINT and TOOLTIP hooks.**
`PsTooltip`, `PsTextBox` and `PsPopupHost` all exist; the controls do not route to them. That is
much smaller than the five unrelated gaps step 19 claimed. See [`7c-step20.md`](7c-step20.md).

---

**7c STEP 19 COMPLETE. THE SIDE PANEL HAS ALL THREE PANES.**

The Explorer is ported -- read-only: the tree, the row scheme, click-to-open, and selection
following the tab. `frmExplorer.inc` is 1,300 lines and 28 procedures against ~160 for Bookmarks,
and **the model half needed NOT ONE LINE** -- gProjectFolders, gConfig.Cat, gApp.pDocList and the
rest were already in `app/`. That is what fourteen steps of moving things down were for.

**AND THE PANE RENDERED NOTHING, WITH NOTHING TO SAY SO.** `gConfig.SetCategoryDefaults` lived in
the SHELL half of the split class and was called from exactly one place: `tiko.bas`. So in
tikoshell `ubound(gConfig.Cat)` was -1, every category loop ran zero times, and the Explorer drew
an empty tree -- which is exactly what a workspace with no files in it looks like. Found by
writing the assertion as a COUNT ("one root group per displayed category") rather than as "more
than zero rows"; the weaker form would have been green.

**A GUARD tiko HAS NEVER ACTUALLY RUN.** `frmExplorer.inc:109` refuses a row that `IsCollapsed`,
and IsCollapsed asks whether the row's OWN subtree is folded. A file row is a LEAF. The test is
false for every row it is applied to, in both binaries. Reverting the ported guard changed nothing
-- 411/0 either way -- which is what prompted asserting it at all. And fixing it exposed a second:
the early "already selected" branch returns true first, and a selection SURVIVES a collapse.

**AND AN ASSERTION PASSED WHILE PRINTING EVIDENCE THAT CONTRADICTED IT** -- `ok  SelectPath lands
on that row  (-1 wanted 2)`. fbc does not promise argument evaluation order and built the MESSAGE
before the call that changes what the message reports. Right, and unreadable.

**THE GLYPH PAINTER WAS DROPPED, AND IT IS A PsPlatform GAP RATHER THAN A CUT.** PsListTree's
paint hook runs BEFORE the built-in painter and replaces the row wholesale -- there is no overlay
point -- and the resolved colours are private with no getters, so a host painter would carry a
second copy of the control's theme resolution.

**AND IT IS THE ONLY ONE OF STEP 19'S "GAPS" THAT SURVIVED BEING CHECKED. THREE OF THE OTHERS
WERE FALSE AND I COMMITTED ALL THREE.** I read PsListTree's callback list and concluded about THE
TOOLKIT without opening `src/ui/controls/`:

| I wrote | what is in the tree |
| --- | --- |
| "PsPlatform has no PsIconPanel -- a control-sized job" | **PsIconPanel.bi/.inc exists**, is covered by `tests/pslists`, is demoed in `demos/gallery`, and ITS OWN HEADER NAMES tiko AS THE SOURCE OF ITS MODEL |
| "Tooltips -- no hook" | **PsTooltip.bi/.inc exists** with `OnTipText`, covered by `tests/pstip` and `tests/pstiphost`; PsListTree parks tooltips on "tier 7" popup surfaces and `PsPopupHost.inc` landed before step 17 used it |
| "In-place label edit -- no begin/end hooks" | PsListTree parks it on "a real single-line editor... which is PsTextBox's phase". **PsTextBox.bi/.inc exists** -- caret, selection, undo, clipboard -- covered by `tests/pstextbox` |

**What is narrowly true is that PsListTree has none of those four hooks. What is false is every
claim about why they cannot be added** -- in all three rows the stated prerequisite is present and
tested, and I quoted a header written before its own prerequisite landed instead of checking it.
This page has a TABLE about exactly this, seven rows long, and it now has an eighth entry
contributed by the person maintaining it. See [`7c-step19.md`](7c-step19.md).

**AND THE REVERT-TO-RED HARNESS DESTROYED UNCOMMITTED WORK.** Restoring with `git checkout --
<file>` reverts to HEAD, not to the pre-patch state, so two reverts ran against a half-built tree,
the build failed, and the runs printed nothing. **A BUILD FAILURE IS NOT A RED; IT IS A NOTHING
WEARING A RED'S CLOTHES** -- the same shape as step 18's patch that matched `
` against a CRLF
file. Restore from a file snapshot, and refuse to report a result when the build did not succeed.

---

**7c STEP 18 COMPLETE. THE LINUX BUILD PATH EXISTS AND HAS NOT BEEN RUN.**

**Three `.sh` files, and there was not one in this repo before.** `src/fbcParser/build.sh`,
`_compile_shell.sh`, `_run_shell.sh`. **NOTHING IN THEM IS VERIFIED** — no Windows session
compiles a `.so`. `bash -n` and the guard chains are all that was exercised. Run them in this
order and the first failure is worth more than anything on this line:

```
bash src/fbcParser/build.sh && bash _compile_shell.sh && bash _run_shell.sh --selftest
```

**AND THE SCRIPTS WERE THE SMALL HALF.** `src/shell` and `src/app` were clean of Win32
IDENTIFIERS — which is all `_check_app_layer` and `_check_shell` ever checked — and full of Win32
SEPARATORS and `%TEMP%`. Four scratch directories that resolve to nothing on Linux; ~35 backslash
path literals; three *"does this document have a path"* tests answered by searching for `\`.
**A separator is not an identifier, which is the same blind spot this page already records one
layer down.**

**AND clsConfig's SETTINGS PATHS WERE ALREADY BROKEN ON WINDOWS.** `PsExePath` has returned
FORWARD slashes on both platforms since it was written, so `exe_path & "settings\settings.ini"`
built `C:/dev/tiko/settings\settings.ini` and handed it to `WritePrivateProfileString`. Windows
opens it, so nothing ever said so.

**FIXING IT TOOK --selftest FROM 395/0 TO 377/17, AND THAT IS THE STEP.** Two lookups compared
paths as RAW STRINGS: `clsApp.GetDocumentPtrByFilename` (a miss opens a SECOND clsDocument for a
file already in a tab, and saving one discards the other's edits) and `SymDb_FileNameEq` (a miss
is a Functions pane with no rows). Both survivable only because every path had been through
`FilenameOriginalCase`, which ends in `PsPathToNative` and hands back backslashes — **two
documented conventions, both correct on their own terms, meeting at a raw `=`.**

**AND THE ASSERTION WRITTEN TO PIN IT WAS VACUOUS.** It compared the two lookups to each other;
the parser's file table holds a MIXED spelling, so with the fix reverted BOTH miss and it compared
0 with 0 and printed `ok` beside two real failures. **Fourth time in this port the obvious
assertion constrained nothing, and the fourth time reverting the fix is what said so.** The
revert-to-red harness ALSO failed silently first — a patch matching `\n` against a CRLF file
changed nothing and reported green, which reads exactly like "the fix was not load-bearing". The
assert on the patch application is the only reason that was one line and not a conclusion.
See [`7c-step18.md`](7c-step18.md).

---

**7c STEP 17 COMPLETE. LINUX RUNS, WITH A WINDOW.**

**Every Windows gate below was RE-RUN on 2026-08-14, not read.** That pass corrected one number:
the self-test gate said 20,329 assertions and reports **20,328** — frmAbout lost an assertion when
the Proprietary pill went in step 15, and the page was never told. The Linux rows are marked
AUTHOR-RUN because no session here can reproduce them.

| repo | branch | where the work is |
| --- | --- | --- |
| tiko | `feat/cross-platform` | steps 1-13 |
| PsPlatform | `main` | `PsFont`, the face chain, the font seam, **and the Linux fixes** |
| HelpCenter | `main` | untouched since 7c began |
| ~/PsPlatform | `main` | the author's Fedora 42 clone -- native storage, NOT a Windows share |

**THE HEAD AND PUSH COLUMNS ARE GONE, DELIBERATELY.** They were wrong in BOTH directions across
steps 11-13 -- once claiming pushed work that was not, once the reverse -- because a commit hash
and a push state are stale the moment anyone commits, which is the next thing that happens after
this page is written. `git log origin/<branch>..HEAD` answers it in a second and is never wrong.
Everything through step 13 was pushed on 2026-08-14.

Both tiko binaries build **warning-free**; every gate in the table below is green; tiko runs.

**THE LAST LINK DEBT IS CLOSED AND THE RATCHET HAS NO BASELINE.** Step 13 took
`_check_app_standalone` from 1 to 0 without touching `clsTopTabCtl`, whose involvement was the
FOURTH stated blocker in that file not to survive being re-read. The Character Set combo is gone,
and "two tiers, two workers" turned out not to be a scheduling decision at all — the parser is a
single global compiler instance and says so in its own header. See [`7c-step13.md`](7c-step13.md).

**THE DEMOS RUN ON FEDORA AT 150%, UNDER X11 AND WAYLAND.** `platformprobe` 28/28;
`widgets` drags correctly at fractional scale; **`build.sh check` 48 suites, 0 failures** on
Fedora 42 / GCC 15.

**STEP 17 GOT THREE BUG REPORTS AND THEY HAD THREE DIFFERENT VERDICTS**, which was the whole
step: ideshell's tabs were never wired to anything (not a defect); its splitters had never been
draggable ON ANY PLATFORM (`SetPos` clamped to a `[0,0]` default and nothing ever called
`SetRange`); and minieditor's Ctrl+Space was a real Linux defect (SDL gives a popup the keyboard,
so the editor was told it lost focus and Scintilla cancelled autocompletion). Both fixed, both
confirmed by the author on Fedora. See [`7c-step17.md`](7c-step17.md).

**AND "THE FIRST LINUX RUN" IS WRONG -- I SAID IT REPEATEDLY BEFORE CHECKING.** PsPlatform's own
`docs/STATUS.md` and `README.md` record native Fedora from the start: Gates 0-4 closed there, the
`hellotext` digest pinned as matching on Fedora, and Gate 4 closed *"after two rounds on native
Fedora"* with an interactive pass that found two real bugs. What had never run there is phase 7c's
own additions -- and specifically SCINTILLA, which `STATUS.md` lists as `not run` on Fedora. All
five defects sit in or behind that work.

**FIVE DEFECTS, AND NOT ONE IN THE PORTABLE CODE** -- every one was in a binding, a build script
or a gate. `structsizes` passed first time (the LP64 layouts were right), and the render digest
matched Windows BYTE FOR BYTE. See [`7c-step16.md`](7c-step16.md).

**STILL NEVER RUN ANYWHERE: the fontconfig path in `PsFont.inc`.** `tikoshell` has a Linux BUILD PATH
now -- step 18's three `.sh` files -- and has still never been BUILT there. It lives in THIS repo, so
it needs both trees checked out side by side, and every line of those scripts is unverified.

**THIRTY-THREE SELF-TEST REPORT LINES, 20,328 ASSERTIONS, IN ONE GATE -- IN A CLEAN TREE, and
step 23 found that the qualifier matters: stray files in the tiko root take it to 34 / 20,362.** `_check_selftests.bat`
covered the STARTUP suites in step 14; step 15 added the five that need a real dialog, which
needed AUTOCLOSE hooks the pattern already existed for. **Its first run found three failures in
suites that had never run.** See [`7c-step15.md`](7c-step15.md).

**AND TWO CLAIMS ABOUT WHY THOSE HOOKS EXIST WERE MEASURED, AND BOTH WERE WRONG** -- including
the replacement written for the first one, which survived about twenty minutes because a
revert-to-red happened to test it. Step 14's lesson was that a suite nothing runs is
indistinguishable from a suite that passes; step 15's is the same thing one level up, about
REASONS.

**UNTIL STEP 14, NOTHING RAN ANY OF THEM.** That is why one had been failing indefinitely.

**AND THE FAILING ONE WAS THE ORACLE, NOT THE CODE.** Every label id in the options bind test was
wrong; five found no row and failed honestly, and FOUR FOUND THE WRONG ROW AND PASSED because the
macro compared a boolean value and any two unrelated rows agree half the time. It pins the field
POINTER and the visible LABEL TEXT now.

**CONFIRMED BY THE AUTHOR (2026-08-14): step 13's interactive pass passed.** A project round-trips
through the moved `ProjectSaveToFile` with its tab order and active tab intact, and Options ->
Colors lays out correctly without the Character Set row.

**BOLD IS BOLD AND FALLBACK EXISTS.** Step 12 closes handoff items 9 and 9b, which were never two
problems: `FontPs` used `fp.size` and dropped the face name, the weight and the italic flag, so
one path could express neither "Consolas bold italic" nor "and if the glyph is missing, try
these". `PsTextEngine` now holds a chain of faces, `PsFont` maps a name to a file on both
platforms, and `PlatPs` ASKS the host instead of holding a value. Windows' chain comes from
`FontLink\SystemLink` — the table GDI itself used, which is why the pre-port editor rendered
Korean in Consolas without being asked to. See [`7c-step12.md`](7c-step12.md).

**CONFIRMED BY THE AUTHOR (2026-08-14): the interactive pass passed.** Korean renders in
Consolas with the font setting untouched, and bold renders bold. That is the gate that mattered —
no suite in this document can see a glyph.

**THE EDITOR FONT SETTING WORKS, AND DID NOT BEFORE STEP 11.** `SCI_STYLESETFONT` carries a family
NAME and `FontPs` discarded it, so the editor rendered `consola.ttf` hard-coded whatever Options
said. **Confirmed by the author: changing the font applies LIVE, with no restart.** See
[`7c-step11.md`](7c-step11.md).

**STEP 10 CLOSED THREE ITEMS THIS PAGE CALLED BLOCKED, AND NONE OF THEM WAS.** UTF-16BE has an
encoding id; `AppHostServices.LoadFileText` is deleted (the seam is 19 fields); and the app layer
no longer includes a shell header by relative path, which takes the **link debt to 1**. See
[`7c-step10.md`](7c-step10.md).

**THE SHELL DECODES NOW.** Step 9 gave both binaries ONE reader (`Doc_ReadFromDisk`, in `app/`)
and moved the encoding suite into `app/` so the shell runs it headlessly. See
[`7c-step9.md`](7c-step9.md).

**BUT ITS ANSI CHANGE WAS REVERTED (2026-08-11), AT THE AUTHOR'S REQUEST.** Step 9 made ANSI a
disk format and retired invariant E1; that removed the warning box shown before a switch to ANSI
destroys characters, and the box was wanted. **E1 is back**, `ConvertTextBuffer` converts again,
and the reader passes ANSI bytes through untouched — which the revert could not carry by itself,
because the ANSI decoding arrived one commit EARLIER than the change that was undone. A partial
revert left a UTF-8 buffer under codepage 0, which is mojibake above 0x7F, and the encoding suite
caught it.

**THE PORT HAS A THREAD, AND PsPlatform HAS `PsThread`.** The symbol scan runs on a worker; the
UI thread's share of it went from **1,244ms to 32–37µs** on a 134-file include graph. **tiko
links PsPlatform too**, so `_check_scihost` is the gate that catches a change here reaching
there. See [`7c-step7.md`](7c-step7.md).

**AND THE FUNCTIONS PANE SHOWS A PROJECT NOW — 0 rows to 51 on `src\tiko.bas`.** Step 8 closed
the three `PsListTree` gaps (all additive), moved `FilenameOriginalCase` into `app/` (link debt
**3 → 2**), and pointed the pane at the symbol database instead of the tab bar. **The startup
pane is Functions**, because Bookmarks is empty by construction until someone sets one. See
[`7c-step8.md`](7c-step8.md).

**FOUR ITEMS ON THIS PAGE WERE BLOCKED ON THINGS THAT HAD ALREADY BEEN REMOVED**, across three
consecutive steps:

| step | the note said | what already existed |
| --- | --- | --- |
| 7 | threading needed a delivery channel | `PSEV_USER` + a thread-safe `Post`, never called |
| 8 | "needs a PsCore canonical-path call first" | `PsFileRealCase` |
| 8 | PsListTree has one data slot | `itemData2`, in the struct, zeroed, unreachable |
| 9 | "only reading still needs Win32" | `PsEncDecodeAuto`, with 53 assertions over it |
| 10 | UTF-16BE is "decoded, never written" | a complete BE arm in `PsEncEncode`, round-tripped by the suite |
| 10 | menu ids are "persisted in keybindings.ini" as numbers | the file stores the NAME; `app/modKeyBindings.bi:48` says so |
| 10 | `app/` needs a `VK_*` to validate a key name | that function's header: **membership** is the test, not the return |
| **19** | **"PsPlatform has no PsIconPanel -- a control-sized job"** | **`PsIconPanel.bi/.inc`, tested in `tests/pslists`, demoed in `demos/gallery`, its header naming tiko** |
| **19** | **"Tooltips -- no hook"** | **`PsTooltip` with `OnTipText`, plus `PsPopupHost` -- the "tier 7" prerequisite its header waits on** |
| **19** | **"label edit -- no begin/end hooks"** | **`PsTextBox`, the "real single-line editor with a caret and a selection" PsListTree's header waits on** |

**Every one of those notes was accurate about the code in front of it and was never checked
against the library it was ruling out.** Several were re-read at an audit and re-COUNTED rather
than re-tested. If you take one habit from this page, take that one: a blocker is a claim about
two things, and the second one moves.

**THE LAST THREE ROWS WERE ADDED BY THE PERSON MAINTAINING THIS TABLE, ONE STEP AFTER WRITING
IT UP.** Not from a stale note inherited from someone else -- written fresh in step 19, from
`PsListTree`'s callback list, about a directory I did not open. **And two of the three were the
control's OWN HEADER naming a prerequisite that had since landed** -- reading the header felt
like checking, and it is not: a header is a claim with a date on it. The rule the last three
rows add to the seven above them is that the file most likely to be out of date about a
dependency is the file that depends on it.

**AND THE STEP-9 ROW WAS WRITTEN BY SOMEONE WHO THEN MADE THE STEP-10 MISTAKE IN THE SAME STEP.**
The "decoded, never written" comment was believed, and the conclusion drawn from it went into a
source comment, a commit message, `7c-step9.md` and this page before anyone opened `PsEncEncode`.
Two of step 10's false comments were **load-bearing for decisions already taken**. Reading the
code the comment describes is cheap; nobody was doing it.

**THIS BRANCH NOW CARRIES TWO UNRELATED WORKSTREAMS.** The 7c port is `src/app` and
`src/shell`; **F1Markdown** (`src/F1Markdown`, `F1Markdown.exe`) is the author's own and is
interleaved with it in the log. Neither builds the other. One F1Markdown commit swept up 90
lines of in-progress `shell/shellpanel.bi`, so `git log -- <path>` is more reliable than
reading commit subjects when tracing who changed what. **F1Markdown has its own handoff at
[`../f1markdown/HANDOFF.md`](../f1markdown/HANDOFF.md)**.

**`feat/f1markdown` NO LONGER EXISTS.** It was cut from this branch, ran three commits ahead,
was fast-forwarded back into `feat/cross-platform` on 2026-08-10 and then deleted, locally and
on the remote. Both workstreams share this branch again — so the warning above about commit
subjects applies going forward, not only to the history.

**EVERYTHING IS PUSHED as of 2026-08-11**: tiko `feat/cross-platform` at `8f3c71cc4`,
PsPlatform `main` at `045f6bf`, HelpCenter `main` at `02a4c18`, all three with a clean tree.
**Run `git log origin/feat/cross-platform..HEAD` rather than believing that sentence** — it has
already been wrong in BOTH directions on this page, and a claim about what is pushed is stale
the moment anyone commits.

**THERE ARE TWO BINARIES IN tiko NOW.** `tiko.exe` from `tiko.bas`, unchanged and building at
every commit; and `_shell\tikoshell.exe` from `src\shell\tikoshell.bas`, which is phase 7c's
shell. Build them with `_compile_fast.bat` and `_compile_shell.bat`; run the second with
`_run_shell.bat`, which puts SDL3 on `PATH` — without it you get exit 127 and no message.
`_shell\` is gitignored, unlike `tiko.exe`.

**PsPlatform'S TEXT ENGINE HAS NO FONT FALLBACK, AND THE WIN32 BUILD DID.** Open a UTF-8
Korean file in tiko today and every glyph is a box; the same file in the old
`Scintilla64.dll` build rendered, because GDI/Uniscribe font-links and PsTextEngine's single
`FT_Face` does not. **This is a toolkit-wide regression, not an editor one** — every
PsPlatform control paints through that engine. Items 9 and 10 of the live list carry the
evidence.

**AND IT WAS MISDIAGNOSED TWICE BEFORE THE CODE WAS OPENED**, which is the same failure this
page already has a table about: first as an encoding bug (it was not — the bytes were always
intact and Notepad proved it), then as a font-charset setting (inert: `PlatPs.cxx` never
reads `lfCharSet`). The question that found it was the author's: **"why did this work in the
ORIGINAL tiko using the same Consolas font?"** — and the answer was a THIRD thing again, bigger
than either guess: **the font setting had never reached the renderer at all**, so no font could
have been chosen to work around it. Step 11 fixed that. The fallback gap is what remains.

**THREE REPOS NOW, NOT TWO.** `C:\dev\HelpCenter` was version-controlled on 2026-08-09 and
lives at `PaulSquires/HelpCenter`. The GENERATOR is tracked; the OUTPUT is not — `site/`,
`cache/` and `data/` are 300 MB of derived files that a deterministic rebuild reproduces
byte-for-byte, and `publish.config.json` holds SFTP credentials and is the `.gitignore`'s
first rule. The rendered site is still captured, inside tiko, as the bundled copy under
`settings/help/helpcenter`.

**tiko's WebView2 removal and PsPlatform's `PsThemeLoadFile` split go together.** The split is
what lets tiko's `_check_scihost` build at all; a tree with one and not the other does not
compile. Both are pushed, so this is a note for anyone rewinding rather than a live hazard.

**The live docs at planetsquires.com/docs still serve the OLD `app.js`.** Publishing was not
run. `?q=` therefore works from tiko (which reads the bundled copy) and does nothing on the
public site until someone publishes.

**PsPlatform's default branch was renamed `master` → `main` on 2026-08-09.** Anything of yours
that names the old one — a script, a checkout, a `git show master:…` — is now silently pointing
at nothing. tiko's own branches are unaffected: `main`, `development`, `feat/cross-platform`.

**EVERY COUNT AND EVERY GATE ON THIS PAGE WAS RE-RUN ON 2026-08-11**, at the end of step 11 —
not read, run. The gate table below is that pass. If you are reading this later, do it again
before quoting anything: the commands are beside each number.

**What that pass found: the gate table was correct and the PROSE was not.** Every number
matched. The sentence that had to change was "PsPlatform is pushed, tiko's last commits are
NOT", which had been true when written and was false by the time it was read — the same failure
mode as the counts, in a sentence nobody thinks to re-run.

**And three counts rotted inside a single day earlier in this port**, caught by re-running
rather than by reading: `_check_app_layer` went 30 files → 36, `_check_app_standalone` 7 clean →
11, and the binding count quoted as "112" is **109** — 112 was a `grep -c` of the call sites,
not the array. That last one had already been corrected once and came back.

## If you are picking this up cold

Read in this order, and do not skip the first:

1. [`Learnings.md`](../../../Learnings.md) — the run-derived traps. Longer than this page and
   more useful.
2. This page's **one-paragraph version**, then **"What is verified, and how"**. The second
   matters more than it looks: five green gates and 27 green suites coexisted with an editor
   that had no right-click menu, no mouse wheel, and no horizontal scrolling at all.
3. [`d2-decision.md`](d2-decision.md) — the decision 7c hangs on. **TAKEN 2026-08-09: Shape A.**
   Read its two closing sections first — what the prerequisites taught, and the struck item 2,
   which is the clearest thing on this shelf about what a re-measurement is worth: the strongest
   argument for the losing shape, refuted by `wc` and `grep`.
4. [`7c-step1.md`](7c-step1.md) — what the shell binary is, and what it is evidence FOR. Read
   its "what is NOT verified" section, which is deliberately the longer half. Its results
   section is four lines and its caveats are twenty, and that ratio is the honest one.
5. [`7c-step2.md`](7c-step2.md) — **the pump collapse, measured.** The largest named unknown
   in `d2-decision.md`, closed: seventeen pumps become one loop and a call, and the whole
   residue is a four-line dialog policy. Read it beside
   [`pump-census.md`](pump-census.md), which is the evidence, and note that its
   not-verified section is again the longer half — this time because NOTHING about a modal
   dialog is asserted anywhere in either repo.
6. [`7c-step3.md`](7c-step3.md) — **the document model, measured and moved.** Read its first
   two sections in order: five defects found by running the binary against zero found by any
   gate, and then the two gates of my own that turned out to measure nothing — a paired
   27-suite sweep that compared a tree with itself, and an assertion that would have hung the
   suite the moment the code under it became real. Beside it,
   [`document-model-blockers.md`](document-model-blockers.md), which is what re-measurement by
   compiler looks like after three wrong greps.
7. [`7c-step4.md`](7c-step4.md) — **the first real form, and the only number 7c was never
   sized against.** Read THE NUMBER section and then the four defects under it: the ratio is
   1.35 and it is a floor, and three of the eight commits were fixing things that only appear
   when the program runs. Also the shortest list on this shelf of what a single panel demanded
   from the toolkit underneath it.
8. [`7c-step5.md`](7c-step5.md) — **the blocker that was not one.** This page called
   threading 7c's largest blocker and put it first for step 5; measuring removed it from the
   top before a line was written, and the panel it was said to gate shipped without it. Read
   THE MEASUREMENT and then "The real limit, and it is not the thread" -- the constraint that
   actually binds is the symbol database holding one file at a time.
9. [`7c-step6.md`](7c-step6.md) — **the measurement that corrected the previous
   measurement.** Step 5 said 4–20ms and demoted threading; step 6 measured the same call on a
   134-file include graph at 1.2 SECONDS and promoted it back. Read THE MEASUREMENT, then the
   two-line explanation of why both numbers were real. **The pair is the most useful thing on
   this shelf about what a benchmark is worth: step 5's sample did not represent the workload,
   and nothing about the method would have told you that.**
10. [`7c-step7.md`](7c-step7.md) — **the item that took three steps to justify, and the
    measurement that first measured nothing.** 1,244ms of UI stall became 32–37µs. Read THE
    MEASUREMENT, including why the first attempt printed `0µs`: it was written against
    `PsTimerNow()`, which is a **settable virtual clock**, not a wall clock — and *"the clock
    never moved"* is indistinguishable from *"the work was free"*, which was the answer the step
    wanted to hear.
11. [`7c-step30.md`](7c-step30.md) — **porting what was learned, not what was written.** Four
    defects found by hand in tiko, two of them live in the shell too. Read "Pinning a
    condition, not a branch": the first revert pair in this run that flips a guard BOTH ways,
    because the wrong answer was a condition that is always true.
12. [`7c-step29.md`](7c-step29.md) — **a headless assertion finding the flaw in a decision
    whose rationale had just been written down.** Read "And the assertion found what that
    decision cost": two different notions of "the selection", one captured and one live, and
    a feature that would have marked the wrong lines or none. Then the fifth unasserted
    wiring in five such steps.
13. [`7c-step28.md`](7c-step28.md) — **the step where porting the SIBLING found the defect
    the first one shipped.** Read "Preserve Case is step 27's question, answered the other
    way": tiko refuses to keep a second copy of a flag the engine owns, this port kept one,
    and tiko has the site that would have broken it. Then the revert that came back GREEN --
    where the thing that was wrong was the COMMENT, not the code.
14. [`7c-step27.md`](7c-step27.md) — **a fixture that was wrong three times over while the
    code under it was right every time.** Read "Every count in the search assertion was wrong
    on the first run": three expected match counts, all guessed from a glance at the file, all
    contradicted by the engine. Then the fourth-consecutive unasserted wiring below it.
15. [`7c-step26.md`](7c-step26.md) — **the step where a function was RECONSTRUCTED FROM ITS
    DOC COMMENT instead of ported, and every gate passed.** Read "And one of the six was not
    ported at all". Then the rest of it: a move of LIVE code, where the gates cannot see
    what matters.** Every one of them proves the find engine compiles, links and leaves
    20,328 assertions standing; none proves it still FINDS anything. Read the interactive
    pass at the end -- it is six specific gestures, and it is the whole verification.
16. [`7c-step25.md`](7c-step25.md) — **an assertion measuring a quantity the defect does
    not change.** Two checks compared ProjectFolders_Count(), and a MOVE PRESERVES THE
    COUNT -- so they held whether the folder had moved or not. Read "The suite broke this
    handler's own first rule, twice". frmExplorer.inc is ported as of this step.
17. [`7c-step24.md`](7c-step24.md) — **geometry that answered nothing before the first
    paint, and a harness that lied for the third time.** Read "The defect the assertions
    found" and then "Two harness failures": four reverts reported green because every
    patch had failed to apply. tiko's Explorer is ported as of this step.
18. [`7c-step23.md`](7c-step23.md) — **a note that outlived its own condition, and a gate
    number that depends on stray files.** PsListTree said label editing was blocked on
    PsTextBox; PsTextBox had landed. Then read "A gate number that depends on stray
    files": two untracked .bas files in the tiko root move _check_selftests by a whole
    suite, reproducibly, with no source change.
19. [`7c-step22.md`](7c-step22.md) — **a correct decision wired to nothing.** Read "The
    finding": five assertions over a pure function stayed green when the call that reaches
    it was deleted -- and the two row callbacks carrying every click since step 4 turned
    out never to have been checked either. They work, so nothing asked.
20. [`7c-step21.md`](7c-step21.md) — **the right button was a DEFECT, and two of the
    guards written against it were redundant.** Read "What the reverts bought": two reverts
    came back green because the MODEL already enforced the rule, and each bought an
    assertion for the guard that does fire. The usual finding here is a claim nothing
    tested; this one is a guard nothing needed.
21. [`7c-step20.md`](7c-step20.md) — **the step where the first three reverts all came
    back GREEN.** Read "THE FIRST THREE REVERTS": a zero-height strip satisfies every
    relation written for it, which is step 1's finding arriving again nineteen steps later
    in a file that quotes it. Also the first time the toolkit made something SHORTER than
    tiko's original.
22. [`7c-step19.md`](7c-step19.md) — **the Explorer pane, and three defects each found
    by a different thing.** Read "Three defects": a pane that rendered NOTHING with nothing
    to say so, an assertion that passed while printing evidence against itself, and a guard
    tiko has never once run. Then "The painter was dropped" -- a gap named rather than a
    corner cut.
23. [`7c-step18.md`](7c-step18.md) — **a whole portability class that every gate was blind
    to, because a separator is not an identifier.** Read "The defect the fix exposed": two
    documented conventions, both correct on their own terms, meeting at a raw `=`. Then the
    vacuous assertion -- and the revert-to-red HARNESS that failed silently before it, which is
    the sharper of the two.
24. [`7c-step17.md`](7c-step17.md) — **three bug reports, three different verdicts, and the
    only thing that told them apart was running both platforms.** Read "What actually solved it":
    two of my diagnoses died, and the one-sentence observation that settled the third was
    "right click popup menus work perfectly". Also the sharpest variant yet of this page's
    recurring shape -- a SUITE that drove the widget correctly and passed, because its FIXTURE
    did the one thing no real caller did.
25. [`7c-step16.md`](7c-step16.md) — **phase 7c's code meets Linux, and the step where I made
    this page's own mistake.** Five real defects, all in bindings, build scripts and gates rather
    than in portable code — and one of them made a suite report a PASS for the wrong reason on a
    platform where the feature did not work at all. Then read "What this step is really about":
    I called it "the first Linux run" repeatedly without opening PsPlatform's STATUS.md, which
    says on its second screen that Fedora had been in use since Gate 0.
26. [`7c-step15.md`](7c-step15.md) — **the step where a revert-to-red falsified the CORRECTION,
    not the code.** Two explanations for the same hook, both plausible, both measured, both
    false -- and the second one was mine, written in this step. Read "Two claims about why
    these hooks exist".
27. [`7c-step14.md`](7c-step14.md) — **READ "What this step is really about".** Every stale
    claim this port has found was in PROSE; this one was in a SUITE, and that is worse. A number
    carries authority a sentence does not, and "11 passed" was believed for as long as it was
    printed. A suite nothing runs is indistinguishable from a suite that passes.
28. [`7c-step13.md`](7c-step13.md) — **the step where the live list emptied, and four of its
    items closed as "the blocker was not one".** Read the last section: a suite that has been
    failing six assertions because nothing runs it. That is this port's recurring failure
    wearing a suite instead of a comment.
29. [`7c-step12.md`](7c-step12.md) — **the step where the revert-to-red pass caught the SUITES
    rather than the code, twice.** A prefix match left every style assertion green because
    `RegEnumValueW` happens to enumerate in the helpful order; removing a whole feature dropped
    a suite from 35 assertions to 30 and still printed "0 failed". Read that section before
    writing any assertion that depends on a lookup order or is wrapped in a skip.
30. [`7c-step11.md`](7c-step11.md) — **the step that started as an encoding bug and was
    neither.** Read "What was actually wrong": a setting that had never reached the code it
    named. Then the assertion that failed on its first run — the CODE was right and the SUITE
    was wrong, which is the other way round from every other failure on this page.
31. [`7c-step10.md`](7c-step10.md) — **three items closed, three false comments, and one of
    them mine.** Read the three-row table at the top. Then the revert-to-red section: every rule
    went red for the first time in six steps, and the one that nearly did not is instructive —
    `Doc_EncodingName`'s `case else` is `"ANSI"`, so a missing arm does not fail, it puts the
    word ANSI beside a UTF-16 file.
32. [`7c-step9.md`](7c-step9.md) — **the encoding step, and the third blocker in three steps
    that had already been removed.** Read the table at the top, then "A defect found next to the
    one being fixed": the seam's `LoadFileText` had NO AGREED POLARITY — one implementation
    returned false on success, the other true — which was invisible with one implementation and
    live from the moment there were two.
33. [`7c-step8.md`](7c-step8.md) — **the step where two blockers turned out to have been
    removed before anyone noticed.** The pane went 0 → 51 rows, but read the two intermediate
    tables first: the one that shows the pane's ceiling is fbcParser (573 procedure symbols, 41
    with a body line) and the revert-to-red table, where one revert found not a missing test but
    a **false claim in a comment** — which is the other thing reverting is for.
30. [`webview2-decision.md`](webview2-decision.md) — the constraint that page called
   irreducible, investigated. **It was not a blocker and never had been.** Short, and it is the
   clearest example on this whole shelf of a claim that survived because nobody checked it.
   **Its recommendation has since been implemented**: WebView2 is gone from the tree.

**The one habit worth copying from this run:** every defect found came from running the program
or reading the callers, and almost none from the gates.

**"ALMOST" ARRIVED IN STEP 5, AND IT IS THE FIRST EXCEPTION IN FIVE STEPS.** A suite assertion
caught the scanner reading whatever the shared view was showing — it printed `tab1=3` for a file
with no procedures in it. What made that possible is worth copying rather than the exception
itself: the assertion compared **two independent answers to the same question** (what the symbol
database holds, and what the panel displays) instead of checking one of them against a constant.

When a note here says something is done,
blocked, or not yours to decide, check it before believing it — that claim was true when written
and this page's own record is that it stops being true quickly.

**THE MENUS PROVED IT FOUR TIMES IN A ROW, and all four were live in EVERY HOST IN THE TREE**
— PsPlatform's demos included, for as long as `PsMenuBar` has existed. Each was reported by the
author within a minute of opening a menu, and none of them is reachable by any headless suite,
because each is about what a menu *does next*:

1. **The popup never closed after a click.** `PsMenuHostOnCommand` existed for exactly this and
   said so in its own comment — *"running something and leaving the menus up is the one
   behaviour no menu has"* — and **was never installed**. `gallery2` looked fine only because
   the handler it exercises is a TOOLBAR command, which never goes through a popup.
2. **The menubar title stayed lit.** `PsMenuBar.bi:135-138` says the host must call
   `NotifyClosed`, and `PsMenuHost.OnClosed` is the hook for it. No host wired it.
3. **A reopened menu came back wearing its last selection.** `nHot`, `nPinned` and `bHoverSel`
   survive a close; the only thing that reset them was `clear()`, which frees every item, so
   nothing on the show path could use it.
4. **Clicking the OPEN title did not dismiss it.** `PsMenuBar.inc:324-328` is explicit that
   "the obvious gesture — clicking the thing you just clicked" must close rather than reopen,
   and fires `OnCloseRequest` to say so. Nothing was listening, so the bar cleared its own
   state and the dropdown stayed on screen.
5. **Hovering along the titles with a menu open opened nothing** — and **this one I caused, by
   fixing #2.** `OpenRoot` begins with `CloseAll` to drop what was open, and that fired
   `pfnClosed`. So `OpenMenu` set `nActive`/`bMenuOpen`, fired `pfnOpen`, and the handler's
   `OpenRoot` reached back through `OnClosed` -> `NotifyClosed` and wiped both. The bar then
   early-outs of its own move handler at `if bMenuOpen = false`. Fixed with a `bReopening`
   flag: an internal close during a reopen does not notify; a close the USER asks for does.

**THE PATTERN IS THE POINT, AND IT IS WORTH MORE THAN THE FIVE BUGS.** `PsMenuBar` and
`PsMenuHost` know nothing about each other BY DESIGN — the bar asks, the host opens, and the
APPLICATION is the only thing that owns both. There are four callbacks across that gap, two in
each direction, and **every one of them had been written, documented, and left unconnected.** A
demo that opens a menu and never closes it looks finished, so nothing in the tree ever
connected them. When you wire a menubar, wire all four: `OnOpenRequest`, `OnCloseRequest`,
`PsMenuHost.OnCommand`, `PsMenuHost.OnClosed`.

**And #5 is the tail of that same pattern: the first host to wire all four is the first to find
out what they do to each other.** Nothing was wrong with any callback. What was wrong was that
one of them could not distinguish a state change the user asked for from one made in passing —
a distinction that only exists once something is listening.

**THE ASSERTION FOR #5 IS VACUOUS AND THE FILE SAYS SO.** It passes with the fix reverted,
because `OpenRoot` never succeeds windowlessly, so `nDepth` stays 0 and `pfnClosed` never fires
on that path at all. It is kept for what it does cover — the bar's state across repeated opens —
under a comment stating what it does not. **Third time in this port the obvious assertion turned
out to constrain nothing, and the third time deliberately reverting the fix is what said so.
That habit is the transferable part of this whole section.**

**And the fix for the first one broke Scintilla's context menu inside one build** — `psslist`
went 44/0 to 43/1. `PsPopupMenu` has ONE command slot and two parties want it; taking it for
the host disconnected `PsSciPopup.inc:228`, which had already claimed it. The host CHAINS now.
That is the useful shape of the story: the suites could not find any of the four, and caught
the regression the fix introduced, immediately.

**Wiring two of them creates a LOOP that has to be checked.** The bar asks the host to close;
the host tells the bar it closed. That terminates only because `NotifyClosed` clears the bar's
fields directly instead of going back through `CloseMenu` (`PsMenuBar.inc:195-200`). The shell
asserts it, because the failure mode is a stack overflow on a mouse click.

**Step 1 said it twice more, and the second time is the sharper one.** The shell shipped a
commit whose UI was visibly unscaled while 21 assertions passed — every one of them a RELATION
between rectangles, and relations hold perfectly at the wrong scale. Then the *fix* for that
shipped an assertion advertised as "the one that would have caught it" which **would not
have**: it read the surface's scale live and passed either way. A green assertion that
constrains nothing looks exactly like a green assertion that constrains a lot. Both were found
by looking at the screen.

**The sharpest instance, because it cuts the other way:** `PsTheme` landed in PsPlatform with a
clean build and 43 green suites, and was pushed three times — while carrying a defect that made
tiko's `_check_scihost` fail to LINK. `PsTheme.inc` is reached from `PsWidget.inc`, so pulling
`PsFile.inc` into it dragged `vbcompat.bi` inside tiko's `namespace PsC`, where `now()` mangles
to `PSC::fb_Now`. Clean compile, link failure, invisible in the repo that caused it.
**That probe is the only coverage PsPlatform has of being CONSUMED rather than built**, and it
is worth more than its 42 assertions suggest. Run tiko's gates after touching PsPlatform's
include graph, not just PsPlatform's own suites.

---

## The one-paragraph version

**Phase 7d is done, 7c's PREREQUISITE is done, the DWSTRING type swap has landed, and this
page's whole follow-up queue is closed.** tiko's editor is a `PsSciView` rendered with
Blend2D, hosted in a Win32 window through PsPlatform's bridge. `DWSTRING` means PsCore's
everywhere; `PsCompat.bi` is deleted. All gates are green — **six now**, and
`_check_app_standalone` **links** as well as compiles, at 11 clean / 0 errors.

**7c's STEP 1 IS DONE AND 7c IS NOT.** `_shell\tikoshell.exe` runs: `frmMain`'s chrome, the
editor, and every dock panel stubbed, in one binary that is not merged. Its layout is checked
against the oracle and **no edge differs from tiko's by more than 2 pixels**. See
[`7c-step1.md`](7c-step1.md) — read its "what is NOT verified" section, which is longer than
its results and is the more useful half.

**STEP 2 IS DONE TOO, AND IT CLOSED THE PUMP.** `frmMain`'s message loop was the largest
unknown `d2-decision.md` named. It collapses: seventeen pumps become one `PsSurface` loop,
fourteen `PsModalHost.Run` calls and two drains, and the sixteen ordered claim points in
`frmMain` become four host `RouteEvent` calls the shell already makes, three `PsAccelTable`s
in a loop, one precedence rule and two rows deferred with the Find bars. The residue everyone
expected to be large — the `IsDialogMessage` replacement — is **four lines**, because
`PsDispatch` already does Tab and only Enter and Escape were missing.

**AND IT COST TWO DEFECTS IN `PsModalHost`, BOTH BECAUSE THE SHELL WAS ITS FIRST CALLER
ANYWHERE.** `Run` deleted its caller's dialog on every dismissal — `SetRoot(0)` deletes, under
a comment saying it detaches — which killed the process silently; and no dialog it raised had
ever had initial keyboard focus. Fixed in `61f56bb` and `be10064`, and **both fixes are
confirmed only by the author using the program** — the box dismisses without killing the
process, the field has focus on open, and Alt+F does nothing while a box is up.

**Neither fix is asserted anywhere**: restoring either bug leaves all PsPlatform suites (46 when
this was checked, **47 since `psthread` landed in step 7**) and
all 194 shell assertions green, checked both times. `Run` needs a compositor and `build check`
is headless by design. A defect class found twice in one step and guarded by nothing afterwards
is the thing to fix first if modal work continues.

**AND STEP 3 IS DONE: THE DOCUMENT MODEL IS OUT OF THE SHELL.** `clsDocument` and `clsApp` are
in `src/app` — **46 files, 9,901 lines that compile against PsCore alone** — and `tikoshell`
opens files, tabs between them and **saves them through the same `clsDocument.SaveFile` that
tiko calls**, with no AfxNova and no `HWND` in its half. The app-host seam that makes that
possible is two records: `AppHostServices` (20 fields, the host answers) and `AppHostNotify`
(11, fire-and-forget) — and the split is load-bearing, because every `Notify` field can be
safely stubbed and no `Services` field can. See [`7c-step3.md`](7c-step3.md).

**THREE THINGS DID NOT MOVE AND ARE NAMED**: `clsTopTabCtl` (a facade over a Win32 control
whose item data *is* the document list), `clsScanMgr` (**PsPlatform has no threading or
synchronisation service at all**), and `modDocViews`. The shell has its own 258-line tab model
instead, and no background parsing.

**FIVE DEFECTS, ALL FOUND BY THE AUTHOR RUNNING THE BINARY, NONE BY ANY GATE** — blank tabs, no
caret, clicks landing up-and-left of the pointer, an arrow cursor over text, and doubled text
after my own wrong fix for the third. **Three of the five were in PsPlatform and had been
reachable by every host in the tree since those widgets existed.** Same shape as step 2's menus.

**AND STEP 4 PUT A REAL FORM IN IT, WHICH IS WHERE THE ONLY UNMEASURED NUMBER FINALLY GOT A
VALUE.** The Bookmarks panel is ported end to end: grouped by file, click-to-jump across tabs,
all four accelerators, a margin icon. **~216 lines of port code replaced ~160 of tiko's — a
ratio of 1.35 — in EIGHT commits, three of which were fixing defects that only appeared when
the program ran.** Read the ratio as a FLOOR: bookmarks was the easiest real panel, its model
was already portable and its control already existed on both sides. See
[`7c-step4.md`](7c-step4.md).

**FOUR DEFECTS, ALL FOUND BY THE AUTHOR RUNNING THE BINARY, NONE BY ANY GATE** — the binary died
at startup with a file argument, Ctrl+F2 threw the caret to line 1, the panel rows were striped,
and a bookmarked line was highlighted end to end instead of showing a margin icon. Fourth step
running, fourth time this is the finding. **The startup crash was invisible to the suite BY
CONSTRUCTION**: it opens its files after the tree is built, and the defect was about *when* the
work happened, not what it did.

**THREE PsPlatform GAPS CAME OUT OF ONE PANEL**, none fixed here: `PsListTree` has one item-data
slot where tiko's control has two; its row striping is unconditional with no `SetAltRows`; and
`OnSelChange` cannot tell a mouse selection from a keyboard one, so **arrowing the list moves the
editor**. Each is worked around at the call site with the workaround named.

**AND STEP 5 KILLED THIS PAGE'S OWN "LARGEST BLOCKER" BEFORE WRITING A LINE.** The list below
put THREADING first for step 5, because the Functions panel needs `clsScanMgr` and PsPlatform
exposes no threading at all. Measuring said otherwise: the panel reads **`gSymDb`**, not the
scanner; `clsSymbolDb` and `PARSERESULTSET` were already in `app/`; the parse is ONE DLL CALL;
and `gAppNotify.RequestBufferScan` was already a seam field this shell stubbed. **A buffer-tier
parse on the UI thread measures 4–20ms on files up to 260 KB** — imperceptible behind a
debounce. `clsScanMgr`'s 382 code lines became **112**, because most of it was thread
machinery. See [`7c-step5.md`](7c-step5.md).

**THAT 4–20ms IS TRUE ONLY OF FILES WHOSE `#include`s DO NOT RESOLVE, and step 6 corrected it:
the same call on `tiko.bas` takes 1.2 SECONDS.** Left standing rather than rewritten, because
the useful part is how it was wrong — a real measurement, on a sample that turned out not to
represent the workload.

**THE REAL LIMIT IS THE SYMBOL DATABASE, NOT THE THREAD.** `gSymDb`'s buffer tier holds exactly
ONE result set and `InstallSet` replaces, so the shell's Functions pane is *"the active file's
procedures"*, never the project's — tiko's project tier is what covers a whole workspace. That
is what step 6 has to decide, and it is asserted rather than described so a future project tier
fails and says so.

**AND THE SUITE FOUND A DEFECT FOR ONCE**: the scanner read whatever the shared view was
showing, so scanning a background tab parsed the foreground document and filed its symbols
under the wrong filename. **The same defect the bookmarks loader already documents at length** —
written two commits earlier, by me, and not carried across.

**AND STEP 6 OVERTURNED STEP 5'S ANSWER WITH ANOTHER MEASUREMENT.** The project tier is 39 lines
of code — `clsSymbolDb` has handled two tiers all along, so the panel needed no change — and
measuring it produced this:

| root | files | symbols | buffer scan | project scan |
| --- | --- | --- | --- | --- |
| `tikoshell.bas` | 5 | 710 | 19ms | 19ms |
| `tiko.bas` | **134** | 4,496 | **1,244ms** | **1,212ms** |

**STEP 5'S "4–20ms, SO THREADING IS UNJUSTIFIED" WAS TRUE ONLY OF FILES WHOSE `#include`s DID
NOT RESOLVE.** `fbcparser_scan_text` follows includes too — the *buffer* scan of `tiko.bas`
returns the same 4,496 symbols from the same 134 files as the project scan. The cost is a
property of the **include graph**, not of the tier, and both tiers pay it.

Editing `tiko.bas` in this shell would stall **1.2 seconds** on every typing pause, every tab
switch, and twice at startup. **That is the first hard evidence for a thread in six steps**, and
it is now item 1. See [`7c-step6.md`](7c-step6.md).

**That is ONE FORM AND TWO DIALOGS. 7c is 48 forms and ~45,000 lines**, so read step 1 as a measurement of
the approach rather than of the progress. An earlier version of this page said 7c was *done*,
which was the most consequential error it has carried; the correction is not an excuse to
overclaim in the other direction.

**The next real question is the one step 1 does not touch:** the pump collapse — 15 message
loops, 13 `IsDialogMessage` sites, eight ordered filter claims. That is where `d2-decision.md`
said the estimate's variance lives, and nothing in step 1 moved it.

**`PsModalHost` IS HALF-ANSWERED NOW, and knowing which half is the point.** It used to be
"proven exactly once, interactively, for one message box, with no headless test". Its routing
decisions moved into `PsModalRouteEvent` — a pure function of `(event kind, bMine)`, no window,
no state — and `tests/psmodalhost` asserts them exhaustively: every event kind, both values of
`bMine`, so a hole in the table fails rather than delivering one kind to nobody.

**What that closed:** the four ways a nested pump goes wrong, three of which look like a hang.
Chiefly that `PSEV_QUIT` does not consult `bMine` — route a quit by surface first and the box
ends, the outer loop never learns, and the application runs on with its main window gone. Both
that bug and "dispatch the owner's resize" were introduced deliberately to check the assertions
bite; each was caught by the one written for it.

**What it did NOT close, which is most of `Run()`:** window creation, both `SetModal` calls and
their ORDER, measuring the root only after attaching it, the back buffer, the paint loop, the
timer service, and the teardown sequence. Every one needs a compositor. **A green
`psmodalhost` says the pump DECIDES correctly — not that the dialog works, and not that
modality holds.**

**And the obstacle is worth knowing before you try to extend it.** `build check` is HEADLESS BY
DESIGN: no suite calls `PsPlatformInit`, and the CI workflow says why — *"hello calls
SDL_Init(0) — no video subsystem, so no display needed"*. A suite that opens a window passes on
this machine and fails on every Linux runner. That constraint is why the decisions had to be
split out at all, and it applies to anything else here that wants testing.

**D2 IS TAKEN — SHAPE A, AND THE NEXT STEP IS THE SHELL SKELETON.** Decided by the author on
2026-08-09: **SDL3 on both platforms, no Win32 backend.** `frmMain` becomes a `PsSurface`;
chrome and editor convert together, every dock panel stubbed, as a runnable binary that is
**not merged** until 7c completes. [`d2-decision.md`](d2-decision.md) carries the evidence.

**Re-measuring beat re-arguing, and it is the only reason the decision could be taken.** Three
facts the memo reasoned from had moved, all three toward A, and each cost one command:
WebView2 is *removed from the tree* rather than merely ruled out; `PsWin32Host` was said to have
grown into most of a second backend and implements **zero** of the 18 entry points (its own
header, `PsWin32Host.bi:48`, has said so all along); and the three host obligations are a cost
on B, which carries each of them twice for the whole conversion.

**Shape A's costs, re-measured rather than quoted:** 49 forms and **45,187** lines, and the pump
collapse is **15 loops, not 13** — `frmMain`, 12 modal forms, and two that are not forms at all
and no form-by-form plan will find: `PsMessageBox.inc` and `PsColorPicker.inc` each own a
`GetMessage` loop. Plus 3 `HACCEL` tables and **13** `IsDialogMessage` sites.

**The 14–20 week estimate was NOT re-measured**, and it is the number that decides whether A was
affordable rather than whether it was right.

**AND WEBVIEW2 IS GONE FROM THE TREE** — not merely ruled out. `frmHelpCenter` was 970 lines
hosting an embedded Edge pane; it is now a URL builder and one `ShellExecute`, and with it went
`CWebView2.inc`, `WebView2Loader.dll`, the `settings/webview2` profile and `_copy_webview2.bat`.
F1 keeps its search through `index.html?q=<symbol>`, which `helpgen`'s `app.js` now honours —
a change in OUR generator that DELETES a coupling rather than porting one. See
[`webview2-decision.md`](webview2-decision.md) for why it was never a blocker.

**That question — a second `IWindowBackend` maintained forever, against a 45,000-line jump that
is un-shippable in the middle — was answered on 2026-08-09 in favour of the jump.** Not because
the jump got cheaper: it got dearer, by two message loops. Because the second backend turned out
never to have been started, and because the half-converted state pays for the three host
obligations twice.

1. ~~**A timer / frame scheduler in PsPlatform.**~~ **DONE** — `src/ui/core/PsTimer.*`, 113
   assertions. **Not** in the backend: `AddTimer`/`KillTimer_` were deleted from
   `IEventBackend` rather than implemented, because `SDL_AddTimer` fires on its own thread and
   tiko's real host is `PsWin32Host` — a backend timer would have to be written twice, which is
   what D2 exists to prevent. All five admitted timer defects are closed; only the marquee stays
   host-stepped, deliberately, because stepping by call is what makes it assertable.
2. ~~**A theme engine (`PsTheme`).**~~ **DONE** — `src/ui/core/PsTheme.*`, all 26 controls, 225
   fields, 84 assertions. **The model is tiko's**: same file format, same role names, same
   `key → role → built-in` resolution, so a tiko `.theme` file drives PsPlatform's controls with
   nothing re-authored. All ten of tiko's themes load. **Eight of those ten name no widget keys
   at all**, which is why the role fallback is the load-bearing half.
3. ~~**An IDE-shell composition demo.**~~ **DONE** — `demos/ideshell`: menubar and toolbar
   docked top, status bar bottom, two splitters, an explorer, a tab bar over a `PsSciView`, an
   output pane. 36 headless assertions, four palettes, verified by hand. **It found three
   defects nothing headless could see** — see `d2-decision.md`.

**WHAT IS STILL TRUE OF `tiko.exe`, AND MATTERS MORE THAN THE TICKS ABOVE:** tiko's 43
`SetTimer` sites are untouched, its message loop does not call `PsTimerService`, and **nothing
in `tiko.exe` is themed by `PsTheme`**. All of that work landed in PsPlatform. tiko's only
stake in it so far is the editor: its clipboard, its caret blink and its Scintilla styling are
wired by hand in `frmSciHost.inc`, because **the editor is not a widget and none of the three
reaches it automatically**.

**THE SHELL BINARY IS WHERE ALL THREE ARE ACTUALLY USED**, which is the point of it: it runs
`PsTimerService` in its pump, applies a real tiko `.theme` through `PsThemeApply`, and drives
`PsAccel` from tiko's own 109 bindings (85 of which carry a chord). None of that has reached `tiko.exe` and none of it
should until 7c lands — but it is no longer true that the toolkit's three prerequisites have
no consumer.

**AND THE EDITOR SEAM IS STILL THREE HOST OBLIGATIONS WITH NO DEFAULT.** The shell hit every
one of them again from scratch — clipboard, caret, and Scintilla styling — plus a fourth
nobody had written down: **the editor's FONT SIZE**. `PsTextEngine` draws the widgets;
Scintilla keeps its own style table, so reopening the engine at a scaled size leaves the code
tiny in a correctly scaled window. `minieditor` is the only host in PsPlatform that ever
called `SetFontPixelSize`. Whatever shape 7c takes inherits all four.

Two facts that made this look otherwise were stale and are now fixed: **Gate 5 is done, not
"not started"** (26 widgets exist), and `PsWin32Host` — which D2 assumed could not exist — is
running tiko's editor today, and has since grown a Win32 clipboard and a `WM_TIMER` ticker
driver on top.

Everything this page previously listed as the next step turned out to be already done, already
impossible, or already wrong — see the two struck sections below, and read that as a statement
about handoff pages rather than about these particular items.

---

## What is verified, and how

Every change is checked against the **27-suite oracle**, compared **paired** — capture before,
capture after, diff. Never against a stored baseline: several suites read `settings/`, so an
old capture reports yesterday rather than the change.

```bash
powershell -File _selftest_all.ps1 -Out before.txt     # ... rebuild ...
powershell -File _selftest_all.ps1 -Out after.txt
powershell -File _selftest_all.ps1 -Diff before.txt after.txt
```

**Two of the 27 are not evidence.** `TIKO_FORMAT_SELFTEST` used to read past the end of an
array; that is fixed, but one other suite is nondeterministic outright (24/18, 33/9, 23/19 on
one unchanged binary). Movement in that one is noise. The runner's own header records both.

### The gates

| script | asserts | state |
| --- | --- | --- |
| `_compile_fast.bat` | gas64 build, zero warnings | green |
| `_check_scihost.bat` | the editor works — **42 assertions** (2026-08-11; this line said 26 until it was re-run), incl. an **A/B against a stock Scintilla window in the same process** | green |
| `_check_package.bat` | tiko runs with **only the Windows directories on PATH** | green, ~1s |
| `_check_app_layer.bat` | `src/app` names no Win32 or AfxNova token (**50 files**, 2026-08-17 — the find engine came down in step 26) | green |
| `_check_app_standalone.bat` | `src/app` compiles against PsCore alone **and LINKS as one unit** | green — **18 clean**, 0 errors, **debt 0 and NO BASELINE** (2026-08-14) |
| `_check_shell.bat` | `src/shell` includes no Win32 shell header, and carries no `PsC.` (5 files); **and NO WINDOWS SEPARATOR IN A PATH LITERAL and no `environ("TEMP")` across `src/shell` + `src/app` -- 53 files, new in step 18.** Two rules, because a separator is not an identifier and `%TEMP%` has no separator in it | green |
| `_run_shell.bat --selftest` | the shell's own suite — **571 assertions** (2026-08-22; +2 step 18, +17 step 19, +12 step 20, +17 step 21, +9 step 22, +12 step 23, +17 step 24, +12 drag and drop in step 25, +18 the Find bar in step 27, +26 the Replace bar in step 28, +24 Selection and the seeding in step 29, +10 the fixes and the four rules in step 30 -- **including a real search AND a real replace over a known buffer**, the only assertions here that would notice the engine going dead) | green |
| tiko `TIKO_FONTFILE_SELFTEST=1` | the font resolver and the callback Scintilla drives — **13 assertions** (2026-08-14) | green |
| **`_check_selftests.bat`** | **35 report lines, 20,449 assertions since TIKO_FINDSEL_SELFTEST joined (2026-08-22); 34 / 20,440 before it. It FLOORS BOTH since step 26, at 33 and 20,328 — and the totals have moved twice UNATTRIBUTED (step 26), so they are not a signal at three-assertion granularity.** Step 23 found that the number DEPENDS ON WHAT IS IN THE DIRECTORY: two untracked .bas files in the tiko root take it to 34 / 20,362, reproducibly, with no source change. Quote the floor, not the total (re-run 2026-08-14; the page said 20,329 until then — frmAbout lost the id-397 assertion when the Proprietary pill went). Fails on any failure AND on fewer than 33 report lines, so "it did not run" cannot look like "it passed". Backs up and restores `settings.ini` and `tiko.tiko`, because a clean exit saves them. Opens real dialogs, so it needs a desktop. ~20s; run it deliberately, not on every build. | green |
| ...including `TIKO_THEME_SELFTEST` | **929** | green |
| ...and `TIKO_OPTIONS_SELFTEST` | the options rows bind to gConfig — **26** (was 11 passed / 6 failed until step 14 repaired the ORACLE) | green |
| ...and the five DIALOG suites | Keyboard layout, User Tools, Build Configurations, Project Options, About — reachable only since step 15 gave them AUTOCLOSE hooks | green |
| ...and `TIKO_KEYBOARD_SELFTEST` | **18,148**, which is most of the total | green |
| ...and the encoding suite it now runs | **48 assertions**, moved into `app/` in step 9 | green |
| PsPlatform `build.cmd check` | **48 suites** (2026-08-14, `psfont` is the newest). Now builds the Scintilla library FIRST -- it never did, which is invisible on Windows and fatal on a clean tree | green, 0 failures |
| **PsPlatform `build.sh check` on LINUX** | **48 suites** on Fedora 42 / GCC 15. `structsizes` confirms the LP64 layouts; the render digest `377B903CA1166763` is IDENTICAL to Windows. **AUTHOR-RUN, 2026-08-14** — no Windows session can re-run this, so it is a report, not a measurement | green, 0 failures |
| **the DEMOS on Linux** | `platformprobe` 28/28 on Wayland (density 1.5, input converted to pixels); `widgets` on X11 AND Wayland; `ideshell` and `minieditor` driven by hand at 150%. **AUTHOR-RUN, 2026-08-14**, and the two fixes it produced are NOT verifiable on Windows — one is invisible there by construction, the other was equally broken | green after both fixes |
| `deps/check-host.sh` | what a Linux host is missing. Said "Host is ready" while `libstdc++-static` was absent, which only surfaces at the final link after every dependency has been built | green, and it checks that now |
| PsPlatform `pstree` | **356 assertions** (242 → 271 step 8, → 290 step 21, → 326 step 23, → 342 step 24, → 356 step 25's drop hook) | green |
| PsPlatform `psfile` | **94 assertions** (was 91; the three that cover `PsFileRealCase`'s useful direction) | green |
| PsPlatform `psencoding` | **54 assertions** (was 53; the one that pins BE ENCODING, not just its round trip) | green |
| PsPlatform `pstext` | **78 assertions** (was 47 before step 12's face chain) | green |
| PsPlatform `psfont` | **38 assertions**, new in step 12 | green |
| PsPlatform `pstec` | **18 assertions** (was 16; `AddFallback` across the C ABI, both directions) | green |
| PsPlatform `psdrag` | **67 assertions** (was 64; the three that catch a splitter with NO range — see step 17) | green |

The ratchet is the weak one and knows it: it greps a hand-written vocabulary, and has had
three gaps in three audits — **five now**. The fourth was `KeyBindings_PickListKeyToValue`,
reached without naming an `Afx` or Win32 token at all. The fifth is the one worth remembering:
`app/modMenuDefinitions.inc:22` includes `"../modKeyBindings.bi"`, the app layer reaching UP
into the shell **by relative path**, and no token scan can see it because A PATH IS NOT AN
IDENTIFIER. That is why `_check_shell.bat` reads `#include` lines instead.

**`_check_app_standalone` LINKS NOW, AND THAT IS NEW.** It ran `fbc -c` — compile only — for
its whole life, so a missing BODY was invisible to it by construction, and it reported 7 clean
/ 0 errors while `src/app` **had never linked on its own**. `app/clsConfig.bi` declares
`dim shared gConfig`, so including the header instantiates it, and the constructor was in the
shell. Found by the shell binary, which is the first thing that ever linked the layer.

The link half carries a counted DEBT, **baseline 4 as of 7c step 3**: `FilenameOriginalCase`
(real Win32, wants a PsCore canonical-path call first), `KeyBindings_PickListKeyToValue`
(declared in a *shell* header — the app layer calling up, which no token ratchet can see),
`TodoStore_RemoveFile`, and `clsConfig::ProjectSaveToFile` (the class is split). It fails on a
fifth. **Delete the baseline when it reaches zero** — the file says so too.

**IT WENT 3 → 6 → 4, AND THE MIDDLE NUMBER IS THE INSTRUCTIVE ONE.** Moving `clsDocument` and
`clsApp` into the layer pulled in callers whose callees were still outside it. That is what the
counter is for; it is lowered whenever the count is, or it stops being a ratchet and becomes a
licence.

**And read every green tick above as evidence too.** All 27 suites and all five gates were
green throughout the period when the editor had no right-click menu and no mouse wheel. The
suites test the model; nothing here tests the window.

---

## Where the phases stand

### 7d — the editor. Done, and hand-checked.

`tikoSciHost` (`src/frmSciHost.*`) is a window class wrapping a `PsSurface` + `PsSciView`.
Four `CreateWindowEx(0, "Scintilla", …)` sites became `SciHost_Create`, and **no other call
site changed** — one branch carries them all:

```
if (nMsg >= SCI_START) andalso (nMsg < 5000) then
    return SciPs_Send(pSt->pView->pSci, nMsg, wParam, lParam)
```

because `SciExec` is `SendMessage`.

**Confirmed by hand:** rendering, typing, caret, syntax colouring, font size, selection,
autocompletion, the context menu, scrolling, the split view, Find-in-Project, the wheel in
both split and unsplit, and Ctrl+wheel zoom.

**TWO THINGS THAT LIST DID NOT CONTAIN, AND BOTH WERE BROKEN.** Read the gap as the warning
it is: "caret" above means the caret is drawn and moves, not that it blinks, and copy/paste
was simply absent from the list. Reported by the author, not found here.

**Both fixed, and CONFIRMED BY HAND on 2026-08-09** — the caret blinks, and cut, copy and
paste all work. That confirmation is the evidence; the suites were green throughout the
period both were dead.

* **Copy and paste did nothing.** `ScintillaPs.cxx` had `void Paste() override {}`, and
  `CopyToClipboard` assigned to a process-local `std::string` whose only reader was declared
  in `PsScintilla.bi` and called from nowhere. tiko's Edit menu sends `SCI_COPY`/`SCI_PASTE`
  straight into those. Fixed in PsPlatform `be58cf8` (host hooks, no default — the two hosts
  do not share a clipboard) and wired here against the Win32 clipboard.
* **The caret did not blink.** `SCI_SETCARETPERIOD, 0` was set deliberately, because
  Scintilla's FineTickers were recorded and never fired. The tickers now call the host; tiko
  drives them from `WM_TIMER` on the host window, at `GetCaretBlinkTime()`.

**The caret ticker only starts once Scintilla has focus** — `CaretSetPeriod` checks
`caret.active`. A host that does not forward focus gets no blink whatever period it asks for.

**Also confirmed by hand, after the fix below:** the horizontal scrollbar — thumb drag, track
paging, Shift+wheel and caret tracking off the right edge.

**And after item 4:** saving a theme to disk from the Options dialog. That path is
`Theme_WriteFile`, which stopped using fbc's `open` — the suites prove the write/parse round
trip but not the dialog that drives it.

**Not verified:** teardown. If Shift+wheel ever looks wrong, the asymmetry is deliberate: the
H bar's own wheel step honours
`SPI_GETWHEELSCROLLCHARS`, while PsCore's Shift+wheel hard-codes 3 columns so both platforms
move identically from the same event. Teardown has no assertion because the obvious one was
vacuous and was deleted rather than reworded.

#### The four defects the interactive pass found

All four were invisible to every suite.

1. **No right-click menu.** Nothing *sends* `WM_CONTEXTMENU` — `DefWindowProc` synthesises it
   from the right-button release and walks it to the parent. `tikoSciHost` returned 0 for the
   whole button-up group, so the chain stopped at the editor. `WM_RBUTTONUP` is now its own
   case and returns `DefWindowProc` after the bridge has had the message.
2. **The wheel did nothing.** `PsSciDispatch` had no `PSEV_MOUSE_WHEEL` case at all: the host
   translated it correctly, `PsSciView` forwarded it correctly, and the dispatcher fell
   through to `return FALSE`. There is no `SciPs_MouseWheel` and there should not be —
   Scintilla puts wheel behaviour in the platform layer because "how far is a notch" is a
   system setting. Built on `SCI_LINESCROLL`, with Ctrl to zoom and Shift for sideways;
   fractional notches accumulate, or a precision touchpad floors every message to zero.
3. **The wheel did nothing over the H scrollbar** while working over the V one.
   `PsHScrollBar` deliberately has no `WM_MOUSEWHEEL` case, so it bubbled to `frmMain`, which
   ignored it. `frmMain` now routes it — **guarded by cursor position**, because it is the
   parent of every panel and forwarding every bubbled wheel would scroll the document from an
   unrelated pane.
4. **Horizontal scrolling did not work at all.** PsPlatform's `ScintillaPs.cxx` had
   `SetHorizontalScrollPos() override { xOffset = 0; }` — Scinterm's body, where a curses
   backend genuinely cannot scroll sideways. Scintilla's `SCI_SETXOFFSET` handler assigns
   `xOffset` and *then* calls that, so the write was wiped one line later. The override means
   "push the position out to the platform's scrollbar widget"; with no widget it is a no-op,
   as the vertical sibling directly above it already was. **One stub took out the bar,
   Shift+wheel and caret tracking together**, and each read as its own missing feature. Fixed
   in PsPlatform `e6a4956`; suite output byte-identical before and after. Found by reading
   `SCI_GETXOFFSET` back immediately after the set, at the call site — `newPos=316 xBefore=16
   xAfter=16` names it in one line, and nothing that watches only the write can see it.

### 7c's app layer — the DOCUMENT MODEL is in it now. **7c's forms have not started.**

This heading used to read "7c — the app layer. Done." **That is wrong twice over**: 7c is the
shell — 48 forms, ~45,000 lines — and the app layer is the increment
`7c-starting-position.md` recommends doing *before* it, precisely so the shell binary has a
translation unit with no AfxNova in it. Reading the old heading, you would conclude a third of
the codebase had already been converted.

The first increment: `clsDocument.bi` free of Win32; the menu vocabulary, localization and two
`gApp` flags into `app/` (30 files); `clsConfig`'s UI defaults split out.

**7c STEP 3 THEN MOVED THE MODEL ITSELF** — `clsDocument` (2,078 lines with its header),
`clsApp`, `modScintilla`, `modSciText`, the encoding write path — taking `app/` to **46 files
and 9,901 lines**, with **52 `gAppHost.` and 23 `gAppNotify.` call sites** where Win32 calls
used to be. `_check_app_standalone` compiling *and linking* those files against PsCore alone is
the proof, at debt 4.

**THE SIZING OF THAT MOVE WAS WRONG THREE TIMES — 15 blockers, then 40, then 40 different ones,
two of the three mine.** Work stopped and it was re-measured by *compiling*: three commits of
measurement before a line of the move was written
([`document-model-blockers.md`](document-model-blockers.md)). Both findings that changed the
plan's shape came out of that, and neither was visible to any grep. **A text search over a
language whose type names carry no prefix will be wrong in a direction that looks plausible.**

### The DWSTRING swap. Landed.

    1010  before any of this work
     711  after work that stood on its own and was committed
     512  after four scripted passes
     501  after the .Utf8 class and 14 pure signatures landed ahead of the swap
       0  the swap itself

[`type-swap-scope.md`](type-swap-scope.md) has every pass, every count and every judgement
call. The three things worth knowing before touching any of it:

**`namespace PsC` SURVIVES.** It was introduced so two types called DWSTRING could coexist,
so the swap should have deleted it — but it was also, undocumented until it was removed,
fencing PsCore's UI layer off from tiko's. Both sides have a `PsBufferPaint`; PsCore's paint
backend and tiko's `PsImage` both define `PsBgrToArgb`. The **core** headers are global, which
is what makes DWSTRING one type; only the UI is fenced. 30 `PsC.` prefixes remain, all in
`frmSciHost.*`.

**`.Wz()` has two spellings and they are not interchangeable.** It returns a `wstring ptr`.
That binds to a Win32 `LPCWSTR` parameter and **not** to AfxNova's `byref as wstring`, which
needs `*x.Wz()`. **241 sites** — `grep -roh '\.Wz()' src/ | wc -l`.

**`modAfxBridge.bi` is the way back.** AfxNova still owns the windows and returns its *own*
DWSTRING; fbc will not chain that to PsCore's. `AfxW()` is the one named conversion, and
`git grep -c "AfxW("` — **10 today**, down from 26 — used to be the honest measure of how much
AfxNova text still crosses into tiko. **It no longer measures that**: all 10 survivors read
out of an AfxNova subsystem tiko has not replaced (8 are `PsTextBox`'s RichEdit and clipboard,
1 `AfxBrowseForFolder`, 1 `AfxCommand`). Track those three subsystems, not the number. The
file is deleted, not rewritten, when it reaches zero.

---

## THE MOST IMPORTANT THING ON THIS PAGE

**The swap cost 501 compile errors and four defects in PsCore's DWSTRING. Three of the four
compiled cleanly.** All are fixed in PsPlatform `46b0255`; they are listed here because the
same shape will recur wherever this type meets new code.

| defect | what it looked like |
| --- | --- |
| **no constructor from a native `wstring`** | fbc silently used the `zstring ptr` overload. Not mojibake — **heap corruption**. tiko built clean, ran to the sixth popup menu, died with `STATUS_HEAP_CORRUPTION` in an allocation nowhere near a string, and **the crash site moved every time a print was added to find it** |
| **`Wz()` returned NULL for an empty string** | `m_buf` is 0 until something is appended, so `*s.Wz()` — the spelling the whole boundary uses — dereferenced null for every untitled window and empty filter |
| **`len(<DWSTRING>)` returned 24** | fbc falls back to `SizeOf` and yields the descriptor size, silently, at every unconverted site. Found through five keyboard assertions reading `len(VKToName(vk)) = 0` that were all reporting a defect that did not exist; the `len(x) > 0` sites had the mirror problem and reported nothing |
| no ordering operators | the only one that failed to build |

`PsCompat.bi` had warned about the `len` trap three phases ago and it still landed, because
the warning was about sites being *converted* and these were sites nobody had converted.
PsCore now declares `operator len`, so unconverted sites are right rather than quietly wrong.
**A clean build of a swapped tree is the start of the verification, not the end of it.**

---

## The live list — EMPTY as of 7c step 13

**Every item that was on this list is closed, and FOUR of them closed as "the recorded blocker
was not one".** The list is left in place with its outcomes rather than deleted: what happened
to it is more useful than the list was.

Item 12 is not an inheritance from this list -- it is a defect step 13 found while closing it.

The four items below this one are all closed and are kept as a record. **These are the open
ones**, and each is a decision rather than a task.

**THREADING WAS ITEM 1, THEN ITEM 4, THEN ITEM 1 AGAIN — EACH MOVE MADE BY A MEASUREMENT — AND
STEP 7 SPENT IT.** Step 5 demoted it (a parse is 4–20ms, so nothing needed a thread); step 6
promoted it back (the same call on a 134-file include graph is 1.2 SECONDS); step 7 moved the
parse onto a worker and the UI thread's share fell to **32–37µs**. **Read that as the pattern
rather than as two corrections: the page's ordering is only ever as good as the last thing that
was measured, and every number was real — the first was measured on a sample that did not
represent the workload.**

1. ~~**THREADING.**~~ **DONE IN STEP 7.** `PsThread` in PsPlatform (FreeBASIC's primitives, not
   SDL's — `SDL_CreateThread` is a macro that splices `_beginthreadex` thunks in and will not
   compile, and FB's runtime needs its per-thread state set up), the scan on a worker, and the
   three obligations step 5 deleted brought back: the retire queue, the stale-root test, and the
   join at exit. **The way back to the UI needed nothing new** — `PSEV_USER` and the thread-safe
   `g_plat.events.Post` were written for exactly this and had never had a caller. See
   [`7c-step7.md`](7c-step7.md).
2. ~~**A PROJECT TIER.**~~ **DONE IN STEP 6** — 39 lines of code, because `clsSymbolDb` handles
   two tiers already and the panel needed no change. It is what produced item 1's number.
3. ~~**The three `PsListTree` gaps step 4 found.**~~ **DONE IN STEP 8**, all three additive, so
   the four existing callers were untouched. `itemData2` **was already in the row struct** and
   simply unreachable — the shell's bit-packing existed because nobody had exposed a field that
   was already there. `SetAltRows` is a flag rather than a colour, which is what stops a theme
   load undoing it. `GetSelSource` is read inside the handler rather than passed to it, and is
   stamped at the ENTRY POINTS — one mouse path in the file, everything else a key.
4. ~~**Encoding detection on read.**~~ **DONE IN STEP 9.** Both binaries share
   `Doc_ReadFromDisk`; `GetFileToString`'s seventy lines of Win32 are gone; ANSI became a disk
   format and the editor is UTF-8 always. **UTF-16BE closed in step 10** —
   `FILE_ENCODING_UTF16BE_BOM`, about fifteen lines. The sentence that stood here said
   *"`PsEncEncode` refuses to write big-endian... the real fix is in PsEncoding"*, and it was
   false: PsEncEncode has always had a big-endian arm. **That claim was written into this page,
   a step report, a source comment and a commit message off the strength of an enum comment
   nobody tested.**

   THE OLD TEXT, for the record: `ShellHost_LoadFileText` read
   bytes and called them UTF-8;
   tiko's read path decodes UTF-16 through `WideCharToMultiByte` and is still shell-side.
   The shell *saves* now, so a UTF-16 file opened there will not round-trip.
5. ~~**`clsTopTabCtl`: portable rewrite, or a Win32 facade forever?**~~ **NEITHER, AND THE
   TREE HAD ALREADY ANSWERED.** There are TWO implementations behind one seam --
   `src/shell/shelltabs.bi`, 311 lines on `PsTabBar` since step 3, and
   `src/clsTopTabCtl.inc`, 424 lines of Win32 -- which is the pattern the whole port runs
   on. It was a facade only while something in `app/` had to reach through it, and step 13
   stopped that being true. No rewrite is owed.

6. ~~**The ONE link-debt body**~~ **CLOSED IN STEP 13, AND THE BASELINE IS DELETED.**
   `_check_app_standalone` is a plain failure now, not a ratchet with a floor of 0 -- which
   would have reported the next undefined symbol as "debt" instead of failing.

   **ITS STATED BLOCKER WAS THE FOURTH IN THAT FILE NOT TO SURVIVE BEING RE-READ.** It said
   `ProjectSaveToFile` "cannot close until the tab control does, which is the largest open
   item in the port". `clsTopTabCtl` was not touched. A project file records which documents
   are open and in what ORDER -- a question about a list -- so four fields went onto
   `AppHostServices` and `SaveActiveTabIndex`'s twenty lines became `Tabs_SaveActiveIndex`
   in `app/`, where the shell's suite can reach them.

   **AND CLOSING IT EXPOSED THE NEXT ONE INSIDE THE SAME COMMIT**: `ProcessToCurdriveProject`
   became undefined and moved too. 1 -> 1 -> 0. That is what the counter was always for.

7. **What the per-form ratio actually depends on.** Step 4 measured 1.35 on a whole form; step
   5's panel came in UNDER 1 — and did less, while its scanner replaced 382 lines with 112
   because most of `clsScanMgr` was thread machinery. **The cost tracks what the platform
   already provides far more than it tracks the form's size**, so a single ratio is the wrong
   instrument for estimating the remaining 47 forms. **AND THE 112 DID NOT HOLD** — step 7 put
   the thread back and the queue, the retire list and the stale-root test came with it. A ratio
   measured against a deliberately reduced port measures the reduction, not the port.
8. ~~**Whether the Functions pane should list more than the open tabs.**~~ **DONE IN STEP 8** —
   0 rows to 51 on `src\tiko.bas`, and rows carry a FILE index now, so choosing one opens it
   from disk. **THE CEILING IS fbcParser AND IT IS THE SAME CEILING tiko HAS**: 134 files,
   573 procedure symbols, **41 with a body line**. Line-continued signatures are not recorded
   with one, and type members are filed against the header that declares them. Whether any of
   those belong in a Functions pane is a parser question, not a port one.
9. ~~**NO FONT FALLBACK IN PsPlatform'S TEXT ENGINE.**~~ **DONE IN STEP 12.**
   `PsTextEngine` holds a chain of up to twelve faces; the first covering a codepoint wins.
   Windows' chain comes from `FontLink\SystemLink` — the table GDI itself used, which is
   exactly why the pre-port build rendered Korean without being asked to. Linux uses
   fontconfig and **has never been run**.

   **THE CAP WAS MEASURED, NOT CHOSEN.** It was 8 until the real chain was read back: eight
   installed files plus the primary is nine, so the symbol font fell off the end — a chain
   that fails at exactly the codepoints it exists for and reports nothing. Now 12.

   Two things this item asserted are worth correcting rather than deleting. **The workaround
   it recommended is no longer needed** — "pick a font that covers the script" was the answer
   while there was no chain, and it cost monospace alignment. And **`.ttc` collections are
   handled**, which step 11 recorded as unsupported and dodged: entries carry `path|faceName`,
   the face is selected by matching `family_name`, and a name that is not in the file is
   REFUSED rather than silently becoming index 0.

9b. ~~**BOLD AND ITALIC ARE IGNORED, EVERYWHERE.**~~ **DONE IN STEP 12.** `FontPs` keys its
   engine on `(faceName, bold, italic, size)` and the host answers with a file per style. The
   threshold is `FontWeight::SemiBold`, not `Bold`, because that is where Scintilla's own
   `ViewStyle` puts it — treating only 700 as bold would leave half the themes flat.

   **A family with no bold cut still renders regular** — the author's decision, taken
   deliberately: no synthetic embolden, no oblique matrix. Emboldening an outline when a real
   bold exists elsewhere looks worse than not doing it, and every editor font in common use
   ships all four files. **`PsTextEngine` still has no weight concept**, and does not need one
   while the resolution happens above it.

10. ~~**THE OPTIONS "CHARACTER SET" COMBO IS DEAD UI.**~~ **REMOVED IN STEP 13.** The
    combo, the `gConfig` field, both `settings.ini` directions, `GetFontCharSetID` and the two
    `SCI_STYLESETCHARACTERSET` sends. Step 12 had removed its last possible meaning -- a
    charset was GDI's way of saying which face covers a script, and the coverage chain answers
    that per codepoint without being told.

    Lang id 286 is BLANKED in all six files and never renumbered; control id 9403 is retired
    rather than reused. The `settings.ini` READ went too, so an existing file keeps its line
    and is simply not consulted.

11. ~~**Whether two tiers deserve two workers.**~~ **THE QUESTION WAS WRONG, ANSWERED IN STEP
   13.** Not a scheduling decision: `fbcParser.bi:166-169` says the engine is a single global
   compiler instance, one scan at a time, all calls from one thread, with `FBCP_E_BUSY` as a
   safety net rather than a lock. A second worker would block on that instance or corrupt it.
   The real decision is parser reentrancy, which is a compiler front-end rewrite.

   **The same-thread half of that contract was already visible from both worker loops** -- it
   is why the retire queue exists in both binaries -- and neither loop said what it was a
   consequence of. Both do now.

12. ~~**THE OPTIONS BIND SUITE IS FAILING, AND NOTHING RUNS IT.**~~ **BOTH HALVES CLOSED IN
   STEP 14, and the second half was the larger one.**

   **The suite was wrong, not the code.** Every label id in it named a different string --
   216 is "Description", 275 is "Windows API Keywords", 81 is "Next Function". Five found no
   row and failed honestly; FOUR FOUND THE WRONG ROW AND PASSED, because the macro compared a
   boolean VALUE and every field it checks is 0 or -1. The one "ROW/FIELD MISMATCH" it
   reported was its own. It pins the field POINTER and the visible LABEL TEXT now, and
   11 passed / 6 failed became 26 / 0.

   **AND NOTHING RAN ANY OF THEM.** Twenty-five report lines fired at startup behind
   `TIKO_*_SELFTEST` variables -- 19,965 assertions AS AT STEP 14, which is history now; the
   gate is 33 lines and 20,328 today -- and no gate started a tiko.
   `_check_selftests.bat` does, and it asserts a MINIMUM SUITE COUNT as well as zero
   failures: a tiko that crashes before the suites prints nothing, sums to zero failures and
   would otherwise pass. Proved by reverting to red: one suite disabled gives "24 suites,
   19948 passed, 0 failed" and the gate still fails.

   **WHAT IS STILL NOT GATED**: the `*_AUTOOPEN` family, which drives real dialogs. Whether a
   gate can drive those without becoming interactive is open -- and it is the same question
   step 11 answered "no" to for the font suite, wrongly, before step 12 did it anyway.

**And one process point steps 3 and 4 both earned: drive every milestone by hand before calling
it done.** Every claim a suite supports has survived. Every claim about what the user sees came
from a person opening the binary — nine for nine across the two steps.

## What I would do next, in order — ALL FOUR ARE NOW CLOSED

Left in place with the outcomes, because what happened to this list is more useful than the
list was. Item 1 found four defects. Item 2 could not be done at all. Item 3 could be done
only part way, and the number it tracked stopped meaning what it said. Item 4 was larger than
written in one direction and smaller in another — one of its three sites needed nothing.

**Not one of the four was accurate a week after it was written**, and every correction came
from reading the code rather than from the page. Treat what follows as a record, and check
before acting on any of it.

1. **An interactive pass on the swapped editor.** First, for the reason above: the swap
   changed the string type under ~1600 sites, and the suites said everything was fine while
   the context menu and the wheel were both dead.
2. ~~**Delete the scaffolding the swap frees up.**~~ **Checked, and there is none.** An
   earlier version of this list said `PsWin32Host` and `DocView`'s forwarding both go. Neither
   does, and both headers now say so on themselves:
   * **`PsWin32Host` is the editor.** Its own header said "deleted when 7c completes", which
     assumed 7c completing meant the shell flipping to SDL3. It didn't — tiko still creates
     its own HWNDs. Today the bridge is the editor's only paint and input path (13 calls in
     `frmSciHost.inc`: `Attach`, `Resize`, `PaintTo`, `Detach`, and `HandleMessage` for every
     mouse, key and wheel message). The real trigger is **frmMain becoming a `PsSurface`**.
   * **`DocView`'s "step 2" is moot.** It existed because `clsDocument` was said to still
     declare its views as `HWND`. It doesn't — both members are `any ptr`, and
     `_check_app_standalone` at 7 clean is the proof. `DocView` is now permanent: the one
     place the portable `any ptr` becomes a shell HWND, and the one null/bounds guard 142
     sites rely on. Inlining it would delete that guard and scatter the cast.
   * `namespace PsC` does **not** go either — see above.
3. **Shrink `modAfxBridge.bi` to nothing.** 24 → **10**, and the remaining 10 are not the
   same kind of thing. What went was text tiko routed through AfxNova out of habit:
   `AfxGetWindowText` ×9 became `modRoutines`' `WindowText()`, and `AfxStrExtract` ×5 became
   `PsStrExtract` — bar the two comment-stripping sites, which relied on AfxStrExtract
   returning the *whole string* when its delimiter is absent where `PsStrExtract` returns `""`.
   Each of the 10 left reads out of an AfxNova **subsystem** tiko has not replaced: 8 are
   `PsTextBox`'s RichEdit and clipboard, 1 is `AfxBrowseForFolder`, 1 is `AfxCommand` (the
   *wide* command line — fbc's `command()` is ANSI, so it needs `CommandLineToArgvW` and its
   own splitting, not a rename). The count now tracks three subsystems, not conversion debt.
4. ~~**The three sites deliberately left alone during the `.Utf8` work.**~~ **Done, and one
   of the three needed nothing.** The `open`/`kill` paths in `frmFindInProject` and
   `modThemeApply` now go through `PsFile` and its wide CRT — plus the same shape in
   `modThemes` and `clsScanMgr`, which the list had missed. **`CompileCmd_Tokenize` was
   already correct**: the swap put `.Utf8` on the way in, and the way out binds DWSTRING's
   `zstring ptr` (UTF-8) overload, not its `wstring` one — so byte-splitting is sound,
   because the delimiters are ASCII and no UTF-8 continuation byte can be mistaken for one.
   Asserted rather than argued: `TIKO_COMPILECMD_SELFTEST` is 30 → 35, covering a `café` exe
   path and a quoted non-ASCII argument by content **and unit count**.

## Open decisions — the two real ones are closed, and NEITHER was the decision described

Kept rather than deleted, because the pattern is the point. Both were parked here as
judgement calls needing the author. One turned out to need no decision at all — the work it
described had already been done and this page had not noticed. The other was a live defect
wearing a decision's clothes. **Reading the callers, or the data, took minutes in each case
and was worth more than the note that said not to.**

(The third bullet was never a decision — it is a record of one already taken.)

The rule that falls out: an entry that says "not mine to take" is a claim about the *state of
the code*, and the code moves. Re-check the claim before honouring it.

* ~~**Format Options' lang ids.**~~ **There was never anything to decide, and this entry was
  wrong in a way worth keeping as a warning.** It claimed 39 ids (593–669) were missing from
  `english.lang` and that those labels rendered blank, needing translations in six files.
  **None of that is true.** Format Options uses ids `470, 473–478, 494–502`; every one is
  populated, in **all six** `.lang` files (522 entries each, no gaps, no blanks among these),
  and nothing renders blank. No source file anywhere uses an `L()` id above 521, which is
  exactly `MAXIMUM:521`.

  593–669 were never call sites. The ids were **renumbered** into the existing range, and the
  only thing left behind was a hardcoded id list inside the Format Options self-test. That is
  worse than a stale list, because `L(id,"default")` is `#Define L(e,s) LL(e)` — a raw index
  into a dynamic array, unchecked, and fbc adds no check. Asserting id 638 against a
  522-element array was an **out-of-bounds read**, so whatever sat past the array decided
  pass/fail: nondeterministic within one binary and systematically different between two. It
  cost two wrong conclusions during 7d. `frmFormatOptions.inc` now asserts the real ids and
  tests bounds *first*, so an id past the end fails loudly and identically every run.

  **What to carry forward instead:** `english.lang` has exactly **three blank slots — 100,
  101, 133**. A new phrase claims one of those; appending past 521 means moving `MAXIMUM` in
  all six files. And `L()` is an unchecked index at **812 call sites** — `grep -roh 'L( *[0-9]\+'
  src/ | wc -l`, and this page said 748 until it was re-counted — all currently in range, which
  is a fact with a shelf life.
* ~~**`modKeyBindings`' `case "A" to "Z"`.**~~ **Fixed — and it was not the vocabulary
  question this page called it.** The trap was live, not confined to the self-tests:
  `KeyBindings_ApplyAccelerators` feeds `AccelKeyToValue` the last `+`-separated token
  straight out of `keybindings.ini`, so a file carrying the historical spelling `"PageUp"`
  installed a **working accelerator on plain P**, colliding with Ctrl+P. Only the pick-list
  hosts were safe, and only because `KeyBindings_PickListKeyToValue` tests membership in
  `gKeyNames()` rather than trusting a non-zero return. The two range arms now require a
  single character; `gKeyNames` only ever supplied single characters to them, so nothing
  that resolved correctly stopped. **The vocabulary question that remains is the narrow
  one:** should `"PageUp"`/`"PageDn"` be accepted *aliases* for `"PgUp"`/`"PgDn"`? Today
  they resolve to 0 and the binding is skipped. Still yours.
* **Non-ASCII path case-folding.** `SymDb_FileNameEq` folded the full Unicode range via
  `lstrcmpiW` and now folds ASCII, matching PsCore and the fact that Linux paths are
  case-sensitive. Recorded in the file.

## Things that will bite

Beyond `Learnings.md`, nine specific to this tree:

* **NEVER SET `pM->OnCommand` ON A MENU A `PsMenuHost` OPENS.** There is ONE command slot and
  two parties want it: the application, which wants to run the command, and the host, which has
  to CLOSE THE CHAIN first. `PsMenuHostWire` claims it on every open, at every level.
  **Register with the host — `g_menus.OnCommand(...)` — not with the menu.**

  Setting it per-popup fails two ways at once and neither says so: the host's hook is
  overwritten, so the menu never closes; and submenus created after the wiring never get one at
  all, so their rows click to nothing. tiko's shell had both — File and View worked while the
  MRU list, Settings, Format and the theme rows were silently inert.

  The host CHAINS to whatever held the slot before it, so code that claims it directly still
  runs — `PsSciPopup.inc:228` does, for Scintilla's context menu. That chaining exists because
  the first version of the fix did not have it and broke `psslist` in one build.

* **A MENUBAR NEEDS ALL FOUR CALLBACKS, and every one of them was unwired in every host.**
  `PsMenuBar` and `PsMenuHost` are deliberately ignorant of each other; the application is the
  only thing that owns both, so it has to carry all four legs:

  | callback | direction | what breaks without it |
  | --- | --- | --- |
  | `PsMenuBar.OnOpenRequest` | bar → host | the menubar drops nothing at all |
  | `PsMenuBar.OnCloseRequest` | bar → host | clicking the OPEN title leaves it open |
  | `PsMenuHost.OnCommand` | host → app | the command never reaches the application |
  | `PsMenuHost.OnClosed` | host → bar | the title stays lit and the next hover re-opens it |

  **The last two point at each other**, so check the loop terminates: it does, because
  `NotifyClosed` clears the bar's fields directly rather than re-entering `CloseMenu`. The
  failure mode is a stack overflow on a mouse click, so assert it rather than trusting it.

* **A PsPlatform CHANGE CAN BREAK tiko WITH BOTH TREES GREEN.** tiko wraps the toolkit in
  `namespace PsC`, and PsPlatform has nothing that does — so any header reaching PsPlatform's
  widget layer that declares C or fbc-runtime externs compiles cleanly there and fails at LINK
  here, mangled to `PSC::…`. `PsTheme` did exactly this by including `PsFile.inc` (which pulls
  `vbcompat.bi`), and it survived three pushes. **After touching PsPlatform's include graph, run
  `_check_scihost.bat` — it is the only thing in either tree that compiles the widget layer
  inside a namespace.**

* **NEVER hand fbc's `open` or `kill` a path.** They take an fbc `string`, and on Windows that
  goes through the **ANSI codepage** — so a `DWSTRING` spelled `*p.Wz()` or `.Utf8` at the call
  addresses a different file, or none, under any non-ASCII directory. Use `PsFile`
  (`PsFileReadAll` / `PsFileWriteAll` / `PsFileAppendAll` / `PsFileDelete`), which uses the wide
  CRT. All 13 sites in `src/` were converted 2026-08-07; `grep -n 'open( \|kill(' src/*.inc`
  should stay empty. This one had reached `Theme_WriteFile`, the real theme **save** path.
* **`for i as uinteger = 0 to X.Length - 1` WRAPS when the length is 0** and runs 2^64 times.
  It has landed **five times** in PsCore. `At()` bounds-checks, so there is no crash to find —
  a read-only loop just spins, and a loop that *appends* took the process to 49 GB and froze
  tiko's compile of any single `.bas`. Guard at the site, and when you find one, `grep` the
  shape rather than fixing the instance.

* **`PsText` is not the same function on both sides of the swap.** The old `PsCompat.bi`'s
  returned `str()` — the ANSI codepage. PsCore's returns `.Utf8`. Anything that reads like a
  behaviour-preserving rename to `PsText` is an encoding change.
* **`modDeclares.bi`'s enum ends at 1038 and `app/modMenuIds.bi` starts at 1039.** They were
  one enum, and the menu ids are persisted in `keybindings.ini` **as numbers**. Adding a
  `MSG_USER_*` message collides with `IDM_FILE_START` and silently reassigns every shortcut
  every user has set. There is no compile-time guard — fbc's preprocessor cannot evaluate an
  enum constant. Both files say so.
* **Include order is load-bearing.** `modScintilla.bi` before PsPlatform's bind headers;
  PsCore's core headers **after** AfxNova's, because both declare a DWSTRING and the
  unqualified name means whichever came last — moving that block up silently gives ~1600
  sites the other type with no error anywhere. C and runtime externs stay at **global scope**,
  because a namespace mangles them to `PSC::…` and fails at link with a clean compile.
* **`libpsscintilla.dll` SITS BESIDE `tiko.exe` AND IS TRACKED IN GIT.** Windows loads that
  copy in preference to anything on `PATH`, so a clean `_compile_fast.bat` against
  `PsPlatform\build\out\win64` proves nothing about what the exe will load. Add an export to
  the shim without refreshing it and tiko **fails to start with exit 127, no message and no
  dialog** — every self-test then reports "(no result line)", which reads as a broken test
  harness rather than a broken binary. Copy it whenever the shim's exports change, and
  commit it. `Learnings.md` has the two-command way to tell a loader failure from a code one.
* **Packaging is `PATH`-free but hand-rolled.** `_package.bat` stages five DLLs derived from
  `objdump -p`; re-derive rather than edit by hand. `_check_package.bat` is the proof — and it
  kills the process **tree**, because `-NoNewWindow` means every child inherits this console
  and the redirected handles, and killing only tiko left cmd waiting on a console nobody had
  released.

## Loose ends in the working tree

Two untracked things, both deliberate:

* `toolchains/fbc-win-USTRING/` — predates this run, not mine, nothing in the build references
  it.
* `settings/themes/paul-dark_custom.theme` — the author's own theme, saved during the
  interactive test of the new `PsFile` save path. **Left uncommitted on purpose**: whether a
  personal theme ships with tiko is the author's call, not a source change to make for them.
  (It is also incidental evidence the new writer works — the theme suite reads every file in
  that directory and went 929 → 948 assertions, all passing, when it appeared.)
