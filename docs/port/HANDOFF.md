# Handoff — the tiko → PsPlatform port

Written 2026-08-06. Branch `feat/cross-platform` @ `b0de22ee`; PsPlatform `master` @ `0739fa3`.
Both push cleanly, both build warning-free, and tiko runs.

Read [Learnings.md](../../../Learnings.md) first — the run-derived traps are there, not here.
This page is where the work stands and what to pick up.

---

## The one-paragraph version

Phase 7d is **done**: tiko's editor is a `PsSciView` rendered with Blend2D, hosted in a Win32
window through PsPlatform's bridge. It builds, runs, edits, colours and packages. Phase 7c —
closing `src/app` so a shell binary with no AfxNova can include it — is **6 of 7 files clean**.
The last file and the deletion of the scaffolding both wait on the **DWSTRING type swap**,
which is measured at **512 errors** and is the single thing gating the rest of the port.

---

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
| `_check_app_layer.bat` | `src/app` names no Win32 or AfxNova token (26 files) | green |
| `_check_app_standalone.bat` | `src/app` **compiles** against PsCore alone | **6 clean, 1 with errors** |

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

**Not verified:** selection, autocompletion, the context menu, scrolling, the split view, and
the Find-in-Project excerpt panes. Teardown is not asserted — the obvious assertion was
vacuous and was deleted rather than reworded.

### 7c — the app layer. 6 of 7.

`clsDocument.bi` is free of Win32; the menu vocabulary, localization and two `gApp` flags moved
into `app/`; `clsConfig`'s UI defaults split out. `clsSymbolDb.inc` is the one left, and its
11 errors are **the type swap**, not a boundary problem — see below.

### The DWSTRING swap. 512, and this is the gate.

`docs/port/type-swap-scope.md` has the full measurement. Short version:

    1010  before any of this work
     711  after what is committed and standing on its own
     512  after four scripted passes that take minutes to re-apply

The four passes are tabulated in that doc. The remaining 512 are individual judgements:
168 `Type mismatch`, 148 `Invalid assignment/conversion` (the `.Utf8` class), 73
`Invalid data types`, and a tail.

**The 148 are the dangerous ones.** Today those sites convert implicitly and fbc's
`wstring` → `string` goes through the **ANSI codepage**; `.Utf8` is UTF-8. A wrong call there
does not fail to compile — it silently changes an encoding. `TIKO_ENCODING_SELFTEST` is the
only thing that catches it.

Three rules learned the hard way, all with worked examples in the doc:

* `.Utf8` on **assertion text** is safe. On a **file path** returned to a caller expecting
  ANSI it is a bug — `CompleteIncludeFilename` is the example.
* fbc reports the **resolved** callee, not the identifier written: `AfxSetWindowText(…)` is
  reported as `parameter 2 of SETWINDOWTEXTW()`. Scripts that match on the name cannot find
  wrapper calls, which is most of them.
* A partly-swapped tree does not build, so there is nothing to run the oracle against. **It
  lands whole or not at all.** Two attempts have been reverted for exactly this.

---

## What I would do next, in order

1. **The swap, in one dedicated run.** Re-apply the four passes from `type-swap-scope.md`
   (minutes), then work the remainder by hand. **The `.Utf8` class is now done** —
   landed on the unswapped tree, 153 → 52 on the same probe, all 27 suites unmoved.
   The 52 that remain under that error code are not `.Utf8` sites at all; the table in
   `type-swap-scope.md` says what each one actually needs.
2. **Then `clsSymbolDb`, `PsCompat.bi`, `namespace PsC` and `DocView`'s forwarding** all
   fall out — they exist only because two DWSTRINGs coexist.
3. **Then 7c proper**: the shell. `PsWin32Host` is scaffolding and is deleted when it lands.
4. **An interactive pass on the editor**, which needs a human, and is the only thing that
   will find what the suites cannot.

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
