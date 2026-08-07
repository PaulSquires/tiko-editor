# Handoff — the tiko → PsPlatform port

Written 2026-08-06. Branch `feat/cross-platform` @ `b0de22ee`; PsPlatform `master` @ `0739fa3`.
Both push cleanly, both build warning-free, and tiko runs.

Read [Learnings.md](../../../Learnings.md) first — the run-derived traps are there, not here.
This page is where the work stands and what to pick up.

---

## The one-paragraph version

Phase 7d is **done**: tiko's editor is a `PsSciView` rendered with Blend2D, hosted in a Win32
window through PsPlatform's bridge. **The DWSTRING type swap has landed** — `DWSTRING` means
PsCore's everywhere, `PsCompat.bi` is deleted, and with it Phase 7c's last blocker: the
standalone gate is **7 clean / 0 with errors**. gas64 builds warning-free, tiko runs, and all
27 suites are byte-identical to the pre-swap capture. The swap cost 501 errors and, far more
to the point, **four defects in PsCore's string type, three of which compiled cleanly** — see
`type-swap-scope.md`. What remains is deleting the scaffolding the swap frees up.

## What is verified, and how

Every change below was checked against the **27-suite oracle**, compared **paired** — capture
before, capture after, diff. Not against a stored baseline: several suites read `settings/`,
so an old capture reports yesterday rather than the change.

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
| `_check_package.bat` | tiko runs with **only the Windows directories on PATH** | green |
| ^ | it killed only tiko, and `-NoNewWindow` means every child it spawned inherited this console and the redirected handles — so the tree survived holding them and cmd waited for a console nobody released. `taskkill /T` plus a `WaitForExit`; it also stops as soon as the line appears, so a healthy package costs ~1s rather than a flat 20 | |
| `_check_app_layer.bat` | `src/app` names no Win32 or AfxNova token (26 files) | green |
| `_check_app_standalone.bat` | `src/app` **compiles** against PsCore alone | green — **7 clean, 0 with errors** |

The ratchet is the weak one and knows it: it greps a hand-written vocabulary. Three gaps in
three audits. The standalone compile is the real test — read a green ratchet as evidence,
never as proof.

---

## Where the phases stand

### 7d — the editor. Done.

`tikoSciHost` (`src/frmSciHost.*`) is a window class wrapping a `PsSurface` + `PsSciView`.
Four `CreateWindowEx(0, "Scintilla", …)` sites became `SciHost_Create`, and **no other call
site changed** — one branch carries them all:

```
if (nMsg >= SCI_START) andalso (nMsg < 5000) then
    return SciPs_Send(pSt->pView->pSci, nMsg, wParam, lParam)
```

because `SciExec` is `SendMessage`. Confirmed by hand: rendering, typing, caret, syntax
colouring, font size.

**The interactive pass has now been done, and it found three defects.** Confirmed working by
hand afterwards: selection, autocompletion, the context menu, the split view,
Find-in-Project, the wheel in both split and unsplit, and Ctrl+wheel zoom. Teardown is still
not asserted — the obvious assertion was vacuous and was deleted rather than reworded.

**Shift+wheel is the one thing still untried.** It should scroll sideways and the horizontal
thumb should follow: `frmEditorHScroll_UpdateScrollBars` is driven from the message pump's
`handleMouseShowScrollBar`, which polls `SCI_GETXOFFSET` on every mouse message, and a wheel
is one. Note the deliberate asymmetry if it ever looks wrong — the H bar's own wheel step
honours `SPI_GETWHEELSCROLLCHARS`, while PsCore's Shift+wheel hard-codes 3 columns so both
platforms scroll identically from the same event. On a machine where that setting is not 3,
the two paths move by different amounts.

**The mouse wheel did nothing in the editor.** `PsSciDispatch` had no `PSEV_MOUSE_WHEEL`
case, so the Win32 host translated the message correctly, `PsSciView` forwarded it correctly,
and the dispatcher fell straight through to `return FALSE`. Scintilla has no
`SciPs_MouseWheel` and never has — wheel behaviour is not in ScintillaBase at all, because
"how far is a notch" is a system setting. Every platform layer builds it on `SCI_LINESCROLL`,
and now so does this one, with Ctrl to zoom and Shift for sideways. Fractional notches
accumulate, or a precision touchpad floors every message to zero and looks dead.

**And the wheel did nothing over the editor's HORIZONTAL scrollbar** while working over the
vertical one. `PsHScrollBar` deliberately has no `WM_MOUSEWHEEL` case — "a horizontal bar
answers horizontal gestures only" — so it fell through to `DefWindowProc` and bubbled up to
`frmMain`, which ignored it. `frmMain` now routes it to the editor, **guarded by cursor
position**: it is the parent of every panel in the window, so forwarding every bubbled wheel
would make a roll over an unrelated pane scroll the document.

**The context menu was one of these, and it WAS broken.** An interactive pass found no
right-click menu in the editor. Nothing sends `WM_CONTEXTMENU` — `DefWindowProc` synthesises
it from the right-button release and walks it up to the parent, and `tikoSciHost` returned 0
for the whole button-up group, so the chain stopped at the editor and `frmMain_OnContextMenu`
never fired. `WM_RBUTTONUP` now returns `DefWindowProc` after the bridge has had it. **This is
exactly the class of defect the suites cannot see** — every assertion passed throughout.

### 7c — the app layer. Done.

`clsDocument.bi` is free of Win32; the menu vocabulary, localization and two `gApp` flags moved
into `app/`; `clsConfig`'s UI defaults split out. `clsSymbolDb.inc` was the one left, and its
11 errors were **the type swap** — they went with it. The gate is 7 clean, 0 with errors.

### The DWSTRING swap. LANDED.

`docs/port/type-swap-scope.md` has the whole measurement. The arc:

    1010  before any of this work
     711  after work that stood on its own and was committed
     512  after four scripted passes
     501  after the .Utf8 class and 14 pure signatures landed ahead of it
       0  the swap itself

`DWSTRING` means PsCore's everywhere. `PsCompat.bi` is deleted. `namespace PsC` SURVIVES, for
a reason nobody had written down: it was also fencing PsCore's UI layer off from tiko's, and
both sides have a `PsBufferPaint`.

**THE SWAP'S REAL COST WAS NOT THE 501 ERRORS.** It was four defects in PsCore's DWSTRING,
**three of which compiled cleanly**:

* **no constructor from a native `wstring`** — fbc silently used the `zstring ptr` overload
  and the process died of `STATUS_HEAP_CORRUPTION` far from any string
* **`Wz()` returned NULL for an empty string**, so `*s.Wz()` dereferenced null
* **`len(<DWSTRING>)` returned 24**, the descriptor size, at every unconverted site
* no ordering operators, which was the one that failed to build

All four are fixed in PsPlatform. The lesson for the rest of the port is on the tin: a clean
build of a swapped tree is the START of the verification, not the end of it.

---

## What I would do next, in order

1. **An interactive pass on the swapped editor**, and it is first for a reason. The swap
   changed the string type under 1600 sites; the suites say the model is unmoved and they
   said exactly that while the context menu and the mouse wheel were both broken.
2. **Delete the scaffolding the swap frees up.** `PsWin32Host` goes; `DocView`'s forwarding
   goes. `namespace PsC` does NOT — see above.
3. **Shrink `modAfxBridge.bi` to nothing.** `git grep -c "AfxW("` is the honest count of how
   much AfxNova text still crosses into tiko, and the file is deleted rather than rewritten
   when it reaches zero.
4. **The three sites left alone during the `.Utf8` work** — the `open`/`kill` paths and
   `CompileCmd_Tokenize`, which byte-tokenises a command line. `type-swap-scope.md` says what
   each needs. `PsText` is the trap: `str()` in the old `PsCompat.bi`, `.Utf8` in PsCore.

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

Beyond `Learnings.md`, three specific to this tree:

* **`modDeclares.bi`'s enum ends at 1038 and `app/modMenuIds.bi` starts at 1039.** They were
  one enum, and the menu ids are persisted in `keybindings.ini` **as numbers**. Adding a
  `MSG_USER_*` message collides with `IDM_FILE_START` and silently reassigns every shortcut
  every user has set. There is no compile-time guard — fbc's preprocessor cannot evaluate an
  enum constant. Both files say so.
* **Include order is load-bearing.** `modScintilla.bi` before PsPlatform's bind headers;
  `vbcompat.bi` hoisted before `namespace PsC`; C and runtime externs at **global scope**,
  because a namespace mangles them to `PSC::…` and fails at link with a clean compile.
* **Packaging is `PATH`-free but hand-rolled.** `_package.bat` stages five DLLs derived from
  `objdump -p`; re-derive rather than edit by hand. `_check_package.bat` is the proof.
