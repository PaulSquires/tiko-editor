# Handoff — the tiko → PsPlatform port

tiko `feat/cross-platform` @ `bd6fc545`; PsPlatform `master` @ `d1055ef`. Both build
warning-free and tiko runs. **PsPlatform `d1055ef` is unpushed.**

**Every count on this page was re-verified on 2026-08-07.** Three were stale and are corrected
below. If you are reading this later, re-run them before quoting them — the commands are
beside each number, and this page's record is that its numbers rot faster than its prose.

## If you are picking this up cold

Read in this order, and do not skip the first:

1. [`Learnings.md`](../../../Learnings.md) — the run-derived traps. Longer than this page and
   more useful.
2. This page's **one-paragraph version**, then **"What is verified, and how"**. The second
   matters more than it looks: five green gates and 27 green suites coexisted with an editor
   that had no right-click menu, no mouse wheel, and no horizontal scrolling at all.
3. [`d2-decision.md`](d2-decision.md) — what the next phase is waiting on, and why it is
   deliberately not started.

**The one habit worth copying from this run:** every defect found this session came from
running the program or reading the callers, and none from the gates. When a note here says
something is done, blocked, or not yours to decide, check it before believing it — that claim
was true when written and this page's own record is that it stops being true quickly.

---

## The one-paragraph version

**Phase 7d is done, 7c's PREREQUISITE is done, the DWSTRING type swap has landed, and this
page's whole follow-up queue is closed.** tiko's editor is a `PsSciView` rendered with
Blend2D, hosted in a Win32 window through PsPlatform's bridge. `DWSTRING` means PsCore's
everywhere; `PsCompat.bi` is deleted. All five gates are green, including
`_check_app_standalone` at **7 clean / 0 with errors**.

**7c ITSELF HAS NOT STARTED, and an earlier version of this page said it was done.** That was
wrong and it is the most consequential error this page has carried. 7c is *the shell* — 48
forms, ~45,000 lines, a third of tiko (`7c-starting-position.md`). What is finished is the
increment that page recommends doing *first*: making `src/app` compile against PsCore alone.
The app layer closing is 7c's precondition, not 7c.

**THE NEXT STEP IS NOT `frmMain`, AND IT IS NOT A DECISION EITHER.** `frmMain` becoming a
`PsSurface` is phase 7c — 48 forms, ~45,000 lines, 14–20 weeks — and it hangs on decision
**D2**, which `7c-starting-position.md` says should be re-decided on evidence before that much
code is committed to one shape. [`d2-decision.md`](d2-decision.md) is that re-decision, and its
answer is **not yet**: the next several weeks are identical under either answer, so three
shared prerequisites come first, none of them wasted whichever way D2 lands. **One of the
three is now done** — the frame scheduler — and the other two have not started.

1. ~~**A timer / frame scheduler in PsPlatform.**~~ **DONE** — PsPlatform `d1055ef`,
   `src/ui/core/PsTimer.*`, 87 assertions in `tests/pstimer`. It is **not** in the backend:
   `AddTimer`/`KillTimer_` were deleted from `IEventBackend` rather than implemented, because
   `SDL_AddTimer` fires on its own thread and tiko's real host is `PsWin32Host` — a backend
   timer would have to be written twice, which is what D2 exists to prevent. Timers are shared
   code over `Ticks()`, serviced by whoever pumps, and the pump's wait now comes from the next
   deadline instead of a hard-coded 30ms. `PsTextBox`'s caret is the first client and blinks.
   **Three things this does NOT mean:** tiko's 43 `SetTimer` sites are untouched; tiko's own
   message loop does not call `PsTimerService` (nothing in tiko needs it yet); and the
   spinner's auto-repeat, the list's drag auto-scroll, the marquee and the two hover delays
   are still host-driven — now unconverted rather than blocked.
2. **A theme engine (`PsTheme`).** Does not exist; the host sets every colour field by hand.
   **Not started.**
3. **An IDE-shell composition demo** — menubar + splitters + docked panels + statusbar in one
   layout. No demo does this yet, and it is what a converted `frmMain` does first.
   **Not started.**

Two facts that made this look otherwise were stale and are now fixed: **Gate 5 is done, not
"not started"** (26 widgets exist), and `PsWin32Host` — which D2 assumed could not exist — is
running tiko's editor today.

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
| `_check_app_layer.bat` | `src/app` names no Win32 or AfxNova token (30 files) | green |
| `_check_app_standalone.bat` | `src/app` **compiles** against PsCore alone | green — 7 clean, 0 errors |

The ratchet is the weak one and knows it: it greps a hand-written vocabulary, and has had
three gaps in three audits. The standalone compile is the real test — read a green ratchet as
evidence, never as proof.

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

**TWO THINGS THAT LIST DOES NOT CONTAIN, AND BOTH WERE BROKEN.** Read the gap as the
warning it is: "caret" above means the caret is drawn and moves, not that it blinks, and
copy/paste is simply absent from the list. Reported by the author, not found here.

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

Beyond `Learnings.md`, six specific to this tree:

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
