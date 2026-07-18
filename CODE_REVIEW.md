# Tiko Editor — Code Review

**Scope:** `C:\dev\tiko\src` (133 files, ~1.3 MB of FreeBASIC/Win32/AfxNova source)
**Focus:** readability, maintainability, descriptive naming, low coupling, simple logic.
**Date:** 2026-07-18

This review was done by reading the structural core in full (entry point, `clsApp`, `clsDocument`,
`clsConfig`, `frmMain`, `modRoutines`, `modCompile`, `modMsgPump`, several dialogs) and sweeping
the rest with targeted searches for systemic patterns (resource pairing, globals, duplication,
threading). File/line references below are to the state of the tree on the review date.

---

## 1. Overall assessment

This is a mature, internally consistent codebase, and that consistency is its biggest asset.
The `frmXxx.bi` / `frmXxx.inc` split is applied uniformly; forms follow the same
WndProc + `HANDLE_MSG` + `_Show` shape everywhere; comments overwhelmingly explain *why*
(with dates and forum links) rather than restating the code; there is no `goto` spaghetti;
`GetDC`/`ReleaseDC` pairs balance exactly (10/10); the newer owner-drawn controls
(`CListBox`, `CVScrollBar`, `CStatusBar`) carry their per-instance-state discipline into the
tree cleanly. There is also essentially **no over-engineering** — the code errs toward plain
procedural clarity, which is the right side to err on.

The maintainability risks are concentrated in four areas, in descending order of importance:

1. **A handful of real defects** — a worker thread that blocks the UI anyway, busy-wait loops,
   per-open font leaks, and a working-directory-relative DLL load. Small fixes, worth doing first.
2. **Coupling through global state** — one 40-handle window registry plus `gApp`/`gConfig`/
   `gTTabCtl` touched from everywhere, and `clsApp` acting as a bag of unrelated flags.
3. **Duplication** — three nearly identical keyword-options forms, Find/Replace form twins,
   triple-maintenance of every config setting, MRU logic × 3.
4. **Low-cohesion "junk drawer" modules** — `modRoutines` above all.

None of this needs a rewrite. Every recommendation below is a narrow, incremental change.

---

## 2. Defects and correctness risks (fix before any refactor)

### 2.1 Update-check thread blocks the UI thread it was created to protect
[frmMain.inc:1542](src/frmMain.inc:1542)

```freebasic
' Check for any Tiko updated version (use a separate thread in case check hangs)
dim as any ptr pThread = ThreadCreate( @DoCheckForUpdates )
ThreadWait( pThread )
```

`ThreadWait` immediately after `ThreadCreate` makes this synchronous: the UI thread blocks
for the full network round-trip (up to the 5-second `WaitForResponse` timeout, plus connect
time) — exactly the hang the comment says the thread exists to avoid.

**Suggestion:** let the thread run free and have `DoCheckForUpdates` post a completion message
(`PostMessage(HWND_FRMMAIN, MSG_USER_..., ...)`) that sets `gApp.IsUpdateAvailable` and calls
`frmMenuBar_PositionWindows()` on the UI thread. That also removes the cross-thread write to
`gApp.IsUpdateAvailable` at [modRoutines.inc:1883](src/modRoutines.inc:1883) and matches the
worker-thread convention you already use elsewhere.

### 2.2 Busy-wait loops burn a CPU core
Two spots spin without yielding:

- [frmMain.inc:1526](src/frmMain.inc:1526) — `MSG_USER_PROCESS_STARTUPUSERTOOLS` waits up to
  5 seconds for `IsWindowVisible(HWND_FRMMAIN)` in a tight `do/loop` with no `sleep`. At 100%
  of one core. A `sleep 50` inside the loop (or just trusting the message ordering — this
  message is posted after the window is shown) fixes it.
- [modCompile.inc:665](src/modCompile.inc:665) — the compile waits with
  `do until pCompile->bCompileThreadComplete : AfxDoEvents() : loop`. `AfxDoEvents` pumps
  messages, so this is not a pure spin, but it (a) runs hot, and (b) makes the whole compile
  re-entrant: every menu command, timer, and paint is dispatched while the loop runs, relying
  on scattered `IsCompiling` checks to keep the user out. A `MsgWaitForMultipleObjects`-style
  wait, or completion via `PostMessage`, is calmer and closes the re-entrancy door.

Also in that compile path: the `ThreadCreate` handle is never passed to `ThreadWait`/
`ThreadDetach` after the flag-loop finishes, which **leaks one thread handle per compile**.
(`frmDebug`'s `gdb_threadListener` deserves the same audit.)

### 2.3 Per-open HFONT leaks in dialogs
`pWindow->CreateFont` returns a font the *caller* owns; `AfxSetWindowFont` does not transfer
ownership. Four dialogs create a font each time they open and never delete it:

- [frmAbout.inc:122](src/frmAbout.inc:122)
- [frmOptions.inc:399](src/frmOptions.inc:399)
- [frmOptionsKeywords.inc:128](src/frmOptionsKeywords.inc:128),
  [frmOptionsKeywordsWinApi.inc:126](src/frmOptionsKeywordsWinApi.inc:126),
  [frmOptionsKeywordsExtra.inc:126](src/frmOptionsKeywordsExtra.inc:126)

One HFONT per dialog-open is small, but it is the exact class of leak your own review checklist
puts at priority #1, and `frmBuildConfig` / `frmUserTools` already demonstrate the correct
pattern in this same tree (create before `DoEvents`, `DeleteObject` after —
[frmBuildConfig.inc:875](src/frmBuildConfig.inc:875)/[1000](src/frmBuildConfig.inc:1000)).
Copy that pattern into the four offenders.

### 2.4 Scintilla DLLs load relative to the *current directory*, not the exe
[tiko.bas:230](src/tiko.bas:230)

```freebasic
dim as any ptr pLibLexilla = dylibload("bin\Lexilla64.dll")
dim as any ptr pLibScintilla = dylibload("bin\Scintilla64.dll")
```

A path with a separator is resolved against the process working directory. That works when
tiko is launched from its own folder, but a shortcut with a different "Start in", or a future
file-type association, hands you a different CWD and the load fails with the misleading
"install the C++ redistributable" message. The fix is one line each:
`dylibload(AfxGetExePathName & "bin\Lexilla64.dll")` — the same way the font file is loaded
a few lines up.

This matters more than it looks because the app *changes* its CWD at runtime
([modCompile.inc:595](src/modCompile.inc:595) `chdir pCompile->MainFolder`, and
`frmUserTools` chdirs around tool launches). Any other CWD-relative lookup —
`CompleteIncludeFilename`'s `AfxGetCurDir` fallback at
[modRoutines.inc:897](src/modRoutines.inc:897) — silently changes meaning after the first
compile. Worth a one-time sweep: everything the *editor* owns should resolve against
`AfxGetExePathName`; only user-project operations should honour the CWD.

### 2.5 Inconsistent / misleading return contracts
- [frmEditorVScroll.inc:24](src/frmEditorVScroll.inc:24) — header comment says
  "Returns True if RECT is not empty"; the function ends `function = 0` unconditionally.
  Either honour the contract or change the comment and return type.
- `clsConfig.SaveConfigFile` / `LoadConfigFile` return `true` **on error** (`return true ' error`
  at [clsConfig.inc:359](src/clsConfig.inc:359) and [676](src/clsConfig.inc:676)) while most
  functions in the tree return 0 meaning nothing at all. Callers ignore the result either way.
  Pick one convention (suggest: `boolean`, `true = success`, and actually check it at the two
  or three call sites where failure matters, e.g. settings not writable).
- Many `as long` functions always return 0 and are really subs. Not worth a mass conversion,
  but new code should use `sub` when there is nothing to return.

---

## 3. Coupling and structure

### 3.1 The global surface is large but *organized* — tighten, don't dismantle
[modDeclares.bi](src/modDeclares.bi) holds ~40 shared `HWND_FRM*` handles, the localization
array, fonts, find state, menubar state, icons, and layout constants; on top of that sit
`gApp`, `gConfig`, `gTTabCtl` ([tiko.bas:72](src/tiko.bas:72)). For a single-window,
single-threaded (UI-wise) app this is a workable pattern, and the strict `HWND_FRMX_CONTROL`
naming makes the handle registry navigable. I would **not** recommend a dependency-injection
rewrite — that would be over-engineering here.

What *is* worth doing:

- **Split `modDeclares.bi` by topic.** It currently mixes command IDs, window handles, fonts,
  icons, find/replace state, menubar state, and layout metrics. Five small headers
  (`modCommandIDs.bi`, `modWindowHandles.bi`, `modFonts.bi` + icons, `modFindState.bi`,
  `modLayoutMetrics.bi`) would let a reader open the one they need and would document, by
  file membership, what each global is *for*.
- **`clsApp` is a flag bag, not a class.** [clsApp.bi](src/clsApp.bi) mixes the document list
  (its one real responsibility) with splitter-drag state, spinner state, timer IDs, session
  state, project state, and compile state. The commented-out fields inside it show it's
  already being eroded ad hoc. Suggest grouping into named sub-types
  (`type APP_DRAG_STATE`, `APP_SESSION`, `APP_PROJECT` …) kept as members of `gApp` — same
  storage, but `gApp.Project.Filename` tells the reader which subsystem owns the field, and
  each group can later move out wholesale (e.g. project state → the fbcParser integration).
- **`FINDREPLACE_TYPE` mixes settings with paint state.** Persistent search options
  (`txtFind`, `nMatchCase`…) share a struct with per-paint hit-test rectangles (`rcMatchCase`,
  `rcClose`… — [modDeclares.bi:237](src/modDeclares.bi:237)). The rects belong to the
  Find/Replace *forms*, not the shared search state. Splitting them would also stop
  `gFindInFiles` from carrying a dead copy of UI-layout fields.

### 3.2 `modRoutines` is a junk drawer
[modRoutines.bi](src/modRoutines.bi) declares, in one module: RichEdit helpers, text-encoding
conversion (`Utf8ToAnsi`, `AnsiToUtf8`, `isUTF8encoded`…), shell file dialogs
(`AfxIFileOpenDialogW`…), ListView wrappers (`LV_*` **and** fifteen `frmListView_*`
functions), process enumeration (`IsProcessRunning`…), localization loading, the version
check, and assorted one-offs. Cohesion is the casualty: nobody can guess where a function
lives, so new helpers get appended here, which is how 1,600-line junk drawers grow.

Suggested split (pure moves, no logic changes):

| New module | Contents |
|---|---|
| `modEncoding` | `Utf8ToAnsi`, `AnsiToUtf8`, `UnicodeToUtf8`, `isUTF8encoded`, `ConvertTextBuffer`, `GetFileToString` |
| `modFileDialogs` | the three `AfxIFile*Dialog*` wrappers |
| → `frmListView.*` | all `frmListView_*` functions (they are that control's API and belong beside it) plus `LV_*` |
| `modProcess` | `GetProcessImageName`, `findprocessid`, `IsProcessRunning`, `SpawnPreviousInstance` |
| `modUpdateCheck` | `ConvertTikoVersion`, `DoCheckForUpdates` |

What remains in `modRoutines` (localization, `OpenSelectedDocument`, curdrive path mapping)
is then small enough to reason about.

### 3.3 Two owner-drawn scrollbar implementations coexist
The tree contains the imported `CVScrollBar` (used by `CListBox`/panels) **and** the older
hand-rolled `frmEditorVScroll` / `frmEditorHScroll` pair that proxies Scintilla scrolling.
The editor pair has a genuine extra job (mirroring `SCI_GETFIRSTVISIBLELINE` etc.), so this
is not automatically dead duplication — but two implementations of thumb math, drag capture,
and auto-hide is two places for the same class of bug (your Learnings.md documents exactly
these traps). The refactor plan should take a position: either port the editor scrollbars onto
`CVScrollBar` with a Scintilla-backed data source, or document why they stay separate.

### 3.4 The single-translation-unit include chain is load-bearing
[tiko.bas](src/tiko.bas) includes all 130 files in one fixed order, with dependencies implied
by position (e.g. `clsDB2.inc` before `clsConfig.inc`, custom controls before the forms that
use them, `frmMain.inc` last). That's a legitimate FreeBASIC pattern and I'm not suggesting
changing it — but the ordering constraints live only in your head. A short comment block in
`tiko.bas` above the include list stating the ordering rules ("controls before forms; forms
before frmMain; X before Y because Z") would cost ten lines and save the next person (or you,
in a year) a broken-build afternoon.

---

## 4. Duplication

### 4.1 The keyword-options triplet (highest-value merge)
`frmOptionsKeywords.inc`, `frmOptionsKeywordsWinApi.inc`, `frmOptionsKeywordsExtra.inc`
(plus their three `.bi` files) are ~135 lines each and roughly two-thirds *line-for-line
identical*; the diffs are control IDs, the config field read/written, and a caption. Same for
their `_Show` scaffolding and the leaked font noted in §2.3 — bugs in one are bugs in three.

**Suggestion:** one `frmOptionsKeywordsPanel` parameterized by a small descriptor
(`caption`, pointer/getter for the keyword string, filename). Three files become one; the
Options dialog creates it three times. This is the cleanest single win in the codebase.

### 4.2 Find vs Replace top-tab forms
`frmTopTabsFind.inc` (533 lines) and `frmTopTabsReplace.inc` (402 lines) share large runs of
identical layout/hit-test/paint logic (~45% of the smaller file diffs, the rest matches).
Full unification may not pay — Replace genuinely has more controls — but the shared geometry
and drawing helpers (option-button hit tests, cue-banner handling, combo history) should be
extracted so the two forms hold only what differs.

### 4.3 Every config setting is maintained in three places
Adding one option means touching the `clsConfig` field declaration, a `WriteLine` in
`SaveConfigFile`, and a `case` in `LoadConfigFile` — ~90 settings × 3 sites, and a missed one
fails silently (setting never persists). Two options, in increasing order of ambition:

1. *Cheap:* a comment at the top of `clsConfig.bi` stating the three-place rule, and keep the
   three lists in the same order so a missing entry is visually obvious.
2. *Better:* a table-driven approach — one shared array of `{ name, type, offset/pointer }`
   entries that both save and load iterate. FreeBASIC makes this slightly clunky (no
   reflection), but a `procptr`-free version using `@this.Field` pointers in an init routine
   is straightforward and collapses ~180 hand-maintained lines into one list.

Related smaller repeats, all worth folding when touched:
- `WriteMRU` / `WriteMRUProjects` / `WriteMRUSessions` and the three matching parse blocks in
  `LoadConfigFile` ([clsConfig.inc:760–782](src/clsConfig.inc:760)) — one helper taking the
  array and key prefix.
- The two window-placement save blocks (main window vs help viewer,
  [clsConfig.inc:313–354](src/clsConfig.inc:313)) — one helper taking `hwnd` and a
  destination struct.
- `LoadCodetipsFB` vs `LoadCodetipsGeneric` — the FB variant looks like a hand-specialized
  copy of the generic one; converge if the differences are incidental.

---

## 5. Readability and naming

### 5.1 Type-naming conventions have drifted
Current styles in active use: `TYPE_BUILDS`, `TYPE_TOOLS` (prefix); `FINDREPLACE_TYPE`,
`LASTPOSITION_TYPE`, `TOPMENU_TYPE` (suffix); `MENUBAR_ITEM`, `COMPILE_DIRECTIVES`,
`SELECTION_INFO`, `PROJECT_FILELOAD_DATA` (bare). Any one of these is fine; three at once
means a reader can't guess a name. Suggest standardizing on the bare descriptive form
(`BUILD_CONFIG`, `USER_TOOL`, `FIND_STATE`…) for new types and renaming the rest
opportunistically — **but only after the tree is under version control** (§7).

### 5.2 Member prefixes are inconsistent
Booleans appear as `bDragActive`, `IsCompiling`, `doubleClickReceived`, and
`PreventConfigLoad` within the same type ([clsApp.bi](src/clsApp.bi)); `DWSTRING` members are
sometimes `wsz`-prefixed (`wszCommandLine`) and sometimes not (`IncludeFilename`,
`SessionName`). `clsConfig` uses `long` for booleans with a comment explaining the
file-format reason — that's a *good* documented exception; the rest is drift. A written
two-line convention ("booleans: `Is`/`Has` verb prefix; `DWSTRING`: no prefix" — or whatever
you prefer) applied to new code stops the bleeding without a big-bang rename.

### 5.3 Dead code should be deleted — once git exists
Notable accumulations: the 18-line commented Alt-key block in
[modMsgPump.inc:95–112](src/modMsgPump.inc:95), commented-out fields in
[clsApp.bi:33–34](src/clsApp.bi:33), debug `print` lines in
[modRoutines.inc:1877](src/modRoutines.inc:1877), the commented `CTabBar.inc` include in
[tiko.bas:98](src/tiko.bas:98), and `Subfolder/TestFile*.bi` (two identical test files that
appear to be scratch data living inside `src`). Right now, deleting them destroys the only
copy — which is precisely the argument for §7.

### 5.4 Long functions — mostly benign, two worth splitting
For the record: `frmMain_WndProc` ~200 lines and `frmMain_OnCommand` ~330 lines are flat
message/command dispatches — long but shallow, and fine as-is (the `frmMainFile/Edit/Search/
View/Project/Compile` split already keeps the handlers themselves elsewhere; that structure
is good). `clsDocument.ApplyProperties` (~385 lines) is one linear run of Scintilla styling
calls — cohesive, leave it. The two I *would* split:

- `frmMain_PositionWindows` (~185 lines, [frmMain.inc:705](src/frmMain.inc:705)) — the layout
  brain for the whole frame. It already has sibling helpers (`PositionSplitDocLeft` etc.);
  extracting the panel/output/statusbar regions the same way would make each region's
  geometry assertable in isolation (per your own verification rule: assert geometry, never
  eyeball it).
- `clsConfig.LoadConfigFile` (~300 lines) — the prefix-dispatch blocks (`CATEGORY_`,
  `USERTOOL_`, `BUILD_`, `MRU_`…) are also mis-indented in places
  ([clsConfig.inc:710–718](src/clsConfig.inc:710)), which actively misleads about nesting.
  One helper per record type fixes both.

### 5.5 Localization by bare index
`#Define L(e,s) LL(e)` ([modDeclares.bi:212](src/modDeclares.bi:212)) — the descriptive
string is discarded, so nothing checks that index 74 still means "About". The scheme works
and I wouldn't replace it wholesale, but two cheap hardenings: (a) a build-time or startup
assertion that the english `.lang` file has at least `max-used-index` entries; (b) note that
each phrase is capped at `MAX_PATH` (260) characters by the `wstring * MAX_PATH` array
element type — a coincidental constant that will confuse someone; define
`MAX_PHRASE_LEN = 260` and use that.

### 5.6 Timer IDs are scattered
`idTimerOutputPanel = 110`, `SpinnerTimerID = 101`, `DebugTimerID = 102` (clsApp),
`idTimer = 100` (clsDocument), `idAutoSaveTimer = 999` (clsConfig). All on the same windows'
ID space, defined in five places. One `enum APP_TIMERS` in a shared header removes the
collision risk and documents the full set.

---

## 6. What I checked and did *not* find problems with

Worth recording so the refactor plan doesn't re-litigate them:

- **GDI pairing** outside §2.3 is clean: `clsDoubleBuffer` correctly restores and deletes;
  `frmPopupMenu`, `frmBuildConfig`, `frmUserTools` all clean up; `ghFont(...)` global fonts
  are deleted in `frmMain`'s destroy path ([frmMain.inc:1730](src/frmMain.inc:1730)).
- **`GetDC`/`ReleaseDC`**: 10 acquisitions, 10 releases, same files.
- **Calling conventions**: 64-bit-only target, WndProcs registered via AfxNova — no
  stdcall/cdecl mismatch exposure found.
- **No `goto`**, no deep nesting pathologies, no clever-macro abuse beyond `L()`/`SciExec`
  (both reasonable).
- **Threading model** otherwise matches your convention (worker + message back), once §2.1/2.2
  are fixed.
- **Comment quality** is genuinely good — dated decision notes with forum links
  (e.g. the `-m` flag explanation in [modCompile.inc:613](src/modCompile.inc:613)) are exactly
  what a future maintainer needs.

---

## 7. One process recommendation before any refactor

You said no git for now — noted, and none of §2's fixes need it. But the moment the refactor
plan includes renames (§5.1), dead-code deletion (§5.3), or file splits (§3.2), version
control stops being optional: those changes are only safe when they're one `git diff` away
from review and one `git revert` away from undo. The sibling projects (`CListbox`,
`CStatusBar`, `CTabBar`, `fbcParser`) all went through their refactor plans under git for
exactly this reason. Suggested sequencing: **fix §2 now → `git init` → then the structural
work.**

---

## 8. Suggested priority order for the refactor plan

| Phase | Items | Risk |
|---|---|---|
| 0 — defect fixes | §2.1 update thread, §2.2 busy-waits + thread-handle leak, §2.3 font leaks, §2.4 DLL paths | Low — small, local diffs |
| 1 — git + hygiene | `git init`; delete dead code (§5.3); move `Subfolder/` test files out of `src` | Trivial once git exists |
| 2 — duplication | keywords triplet (§4.1), MRU + placement helpers (§4.3), Find/Replace shared helpers (§4.2) | Medium — mechanical but wide |
| 3 — module reorg | split `modRoutines` (§3.2), split `modDeclares.bi` (§3.1), timer-ID enum (§5.6) | Medium — include-order care needed |
| 4 — structure | `clsApp` sub-types (§3.1), config table (§4.3 option 2), `PositionWindows` extraction (§5.4) | Higher — touch-everything changes |
| 5 — decide | editor scrollbars vs `CVScrollBar` (§3.3) | Design decision first |

---

## Not verified

- **No build was run** — this review is from reading source; nothing above was compiled.
- Leak/behavior claims (§2.1–2.4) are reasoned from the code, not observed at runtime; §2.4
  in particular should be confirmed by launching the exe with a foreign working directory.
- Duplication percentages come from line-level `Compare-Object` diffs, which understate
  similarity when lines differ only by an identifier.
- Roughly 15 of 133 files were read in full; the rest were sampled via searches. Files not
  individually inspected include most of `frmDebug.inc`, `frmOutput.inc`, `modParser.inc`,
  and `frmHelpViewer.inc` — the same systemic patterns likely apply, but no file-specific
  claims are made about them.
