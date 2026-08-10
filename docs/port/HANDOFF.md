# Handoff — the tiko → PsPlatform port

tiko `feat/cross-platform` @ `e285254a1`; PsPlatform **`main`** @ `be10064`;
HelpCenter **`main`** @ `02a4c18`. All build warning-free, all are pushed, and tiko runs.

**THERE ARE TWO BINARIES IN tiko NOW.** `tiko.exe` from `tiko.bas`, unchanged and building at
every commit; and `_shell\tikoshell.exe` from `src\shell\tikoshell.bas`, which is phase 7c's
shell. Build them with `_compile_fast.bat` and `_compile_shell.bat`; run the second with
`_run_shell.bat`, which puts SDL3 on `PATH` — without it you get exit 127 and no message.
`_shell\` is gitignored, unlike `tiko.exe`.

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

**Every count on this page was re-verified on 2026-08-09**, and the gate table and shell
figures again at the end of that day. If you are reading this later, re-run them before quoting
them — the commands are beside each number, and this page's record is that its numbers rot
faster than its prose.

**Three of them rotted inside a single day and were caught by re-running, not by reading**:
`_check_app_layer` went 30 files → 36, `_check_app_standalone` 7 clean → 11, and the binding
count quoted as "112" is **109** — 112 was a `grep -c` of the call sites, not the array. That
last one had already been corrected once and came back.

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
6. [`webview2-decision.md`](webview2-decision.md) — the constraint that page called
   irreducible, investigated. **It was not a blocker and never had been.** Short, and it is the
   clearest example on this whole shelf of a claim that survived because nobody checked it.
   **Its recommendation has since been implemented**: WebView2 is gone from the tree.

**The one habit worth copying from this run:** every defect found came from running the program
or reading the callers, and none from the gates. When a note here says something is done,
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
is worth more than its 26 assertions suggest. Run tiko's gates after touching PsPlatform's
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

**Neither fix is asserted anywhere**: restoring either bug leaves all 46 PsPlatform suites and
all 194 shell assertions green, checked both times. `Run` needs a compositor and `build check`
is headless by design. A defect class found twice in one step and guarded by nothing afterwards
is the thing to fix first if modal work continues.

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
| `_check_scihost.bat` | the editor works — 26 assertions, incl. an **A/B against a stock Scintilla window in the same process** | green |
| `_check_package.bat` | tiko runs with **only the Windows directories on PATH** | green, ~1s |
| `_check_app_layer.bat` | `src/app` names no Win32 or AfxNova token (36 files) | green |
| `_check_app_standalone.bat` | `src/app` compiles against PsCore alone **and LINKS as one unit** | green — 11 clean, 0 errors, debt 3 |
| `_check_shell.bat` | `src/shell` includes no Win32 shell header, and carries no `PsC.` | green |

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

The link half carries a counted DEBT, baseline 3: `ProcessFromCurdriveApp` (already pure
PsCore), `FilenameOriginalCase` (real Win32, wants a PsCore canonical-path call first), and
`KeyBindings_PickListKeyToValue`. It fails on a fourth. **Delete the baseline when it reaches
zero** — the file says so too.

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

### 7c's first increment — the app layer. Done. **7c itself has not started.**

This heading used to read "7c — the app layer. Done." **That is wrong twice over**: 7c is the
shell — 48 forms, ~45,000 lines — and the app layer is the increment
`7c-starting-position.md` recommends doing *before* it, precisely so the shell binary has a
translation unit with no AfxNova in it. Reading the old heading, you would conclude a third of
the codebase had already been converted.

What is actually done: `clsDocument.bi` is free of Win32; the menu vocabulary, localization
and two `gApp` flags moved into `app/` (30 files); `clsConfig`'s UI defaults split out.
`clsSymbolDb.inc` was the last file, and its 11 errors were the type swap — they went with it.
`_check_app_standalone` compiling those files against PsCore alone is the proof.

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
