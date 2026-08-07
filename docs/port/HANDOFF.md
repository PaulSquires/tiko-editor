# Handoff — the tiko → PsPlatform port

tiko `feat/cross-platform` @ `ebea00ca`; PsPlatform `master` @ `46b0255`. Both build
warning-free, both push cleanly, and tiko runs.

Read [Learnings.md](../../../Learnings.md) first — the run-derived traps are there, not here.
This page is where the work stands, what is proven, and what to pick up.

---

## The one-paragraph version

**Phases 7c and 7d are done, and the DWSTRING type swap has landed.** tiko's editor is a
`PsSciView` rendered with Blend2D, hosted in a Win32 window through PsPlatform's bridge.
`DWSTRING` means PsCore's everywhere; `PsCompat.bi` is deleted. All five gates are green,
including `_check_app_standalone` at **7 clean / 0 with errors** — `src/app` now compiles
against PsCore with nothing else in scope, which was 7c's last blocker. What is left is
deleting the scaffolding the swap frees up, and an interactive pass on the swapped editor.

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

### 7c — the app layer. Done.

`clsDocument.bi` is free of Win32; the menu vocabulary, localization and two `gApp` flags
moved into `app/`; `clsConfig`'s UI defaults split out. `clsSymbolDb.inc` was the last file,
and its 11 errors were the type swap — they went with it.

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
needs `*x.Wz()`. 253 sites.

**`modAfxBridge.bi` is the way back.** AfxNova still owns the windows and returns its *own*
DWSTRING; fbc will not chain that to PsCore's. `AfxW()` is the one named conversion, and
`git grep -c "AfxW("` — **26 today** — is the honest measure of how much AfxNova text still
crosses into tiko. The file is deleted, not rewritten, when that reaches zero.

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

## What I would do next, in order

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

## Open decisions, not mine to take

* **Format Options' lang ids.** 39 ids (593–669) are asserted by tests and **do not exist in
  `english.lang`**, so those labels render blank in the UI. The tests now fail loudly instead
  of reading garbage. Adding them means real translations in six files.
* **`modKeyBindings`' `case "A" to "Z"`.** The lexicographic-range trap from `Learnings.md` is
  still live: an unrecognised multi-character key name resolves to its own initial. A
  vocabulary question, deliberately left alone.
* **Non-ASCII path case-folding.** `SymDb_FileNameEq` folded the full Unicode range via
  `lstrcmpiW` and now folds ASCII, matching PsCore and the fact that Linux paths are
  case-sensitive. Recorded in the file.

## Things that will bite

Beyond `Learnings.md`, four specific to this tree:

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
* **Packaging is `PATH`-free but hand-rolled.** `_package.bat` stages five DLLs derived from
  `objdump -p`; re-derive rather than edit by hand. `_check_package.bat` is the proof — and it
  kills the process **tree**, because `-NoNewWindow` means every child inherits this console
  and the redirected handles, and killing only tiko left cmd waiting on a console nobody had
  released.

## Loose ends in the working tree

`toolchains/fbc-win-USTRING/` is untracked and is not mine — it predates this run. Nothing in
the build references it.
