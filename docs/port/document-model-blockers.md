# What still stops `clsDocument` moving into `app/`

Measured 2026-08-10, after 7c step 3 commit 2 (`c36ca11a1`) removed the Win32 vocabulary.

**Method: compile it.** A prelude of PsCore's five headers plus every `app/*.bi`, then
`clsDocument.bi` and `clsDocument.inc`, compiled with `fbc -c` against PsPlatform only. Not a
grep. Step 3's plan sized this from greps three times and was wrong three times — 15, then 40,
then 40 different things.

**23 unresolved names.** Down from 40 before commit 2, and one of the plan's four categories
turned out not to exist.

---

## A. The Scintilla vocabulary — 10 names, tractable

`SciMsg`, `SciHost_DirectPointer`, and eight constants: `SC_CASE_LOWER/MIXED/PROPER/UPPER`,
`SC_CP_UTF8`, `SC_FOLDLEVELHEADERFLAG`, `SC_FOLDLEVELNUMBERMASK`, `SCFIND_MATCHCASE`.

The constants are plain `#Define`s with integer values in `modScintilla.bi` — 1193 lines that the
ratchet reports as **6 violations, of which only 4 are real**:

* the `Scintilla_DirectFunction` typedef names `UINT`, `WPARAM`, `LPARAM`, `LRESULT`
  (`modScintilla.bi:91-95`). PsPlatform's `SciPs_Send` already declares the identical function in
  portable terms — `(any ptr, ulong, uinteger, integer) as integer` — and its header says keeping
  that shape is "the whole reason Phase 6 is affordable".
* `Sci_RangeToFormat` has two `HDC` fields (`:1144-1145`). **That type is declared and never used
  anywhere in tiko** — verified by grep across every `.bi` and `.inc`.

**The other two are FALSE POSITIVES, and they are a ratchet finding rather than a code one.**
`Sci_NotifyHeader`-adjacent fields at `:1172-1173` are *named* `wParam` and `lParam`. The checker
matches whole words case-insensitively, so a struct FIELD called `wParam` trips the rule for the
Win32 TYPE `WPARAM`. Worth knowing before someone "fixes" a struct definition that was correct.

**Verdict: move `modScintilla.bi` into `app/` after four edits.** `SciHost_DirectPointer` is
different — it lives in `frmSciHost.bi` and is genuinely the host bridge, so it becomes an
`AppHostServices` field beside `CreateView`.

## B. The `TH*KEY` theme macros — 0 names. This category does not exist.

`clsDocument.inc` references `THFOREKEY`, `THBACKKEY` and the rest **zero times**.

They appeared in the earlier measurement because that probe included `app/modThemeKeys.bi`
unconditionally. That file is an X-macro DATA LIST: it expands only inside the macro context
`modThemes.inc` sets up, so including it on its own is meaningless and reports every macro as
undefined. My probe created this category; nothing in the code did.

## C. Shell functions — 7 names, mixed

| function | home | that file's state |
| --- | --- | --- |
| `Doc_ConfirmLossySave`, `Doc_ReportWriteFailure` | `modEncodingUi.bi` | 29 lines, **already clean** |
| `FindProject_OnDocumentClosing`, `FindProject_OnDocumentSaved` | `modFindProject.bi` | 234 lines, **1 violation** |
| `CompleteIncludeFilename`, `GetFileToString`, `Scintilla_StripTrailingWhitespace`, `Scintilla_GetTextBytes` | `modRoutines.bi` | 76 lines, **12 violations** (9 `HWND`, 3 AfxNova) |

Two of the four in `modRoutines` take `byval hEdit as hwnd` and are Scintilla helpers; the other
two are file and include-path helpers. **`modRoutines` is a grab-bag and wants splitting rather
than moving** — the four the document needs are not obviously the ones carrying the 12
violations, and that has to be checked before anything is promised.

## D. `gApp` and `gScanMgr` — 2 names, 9 uses. The real unknown.

```
gApp.ProjectSetFileType   gApp.pfnCreateLexerfn   gApp.IsProjectLoading
gApp.SuppressNotify (x4)  gApp.IsProjectNamed()   gScanMgr.RequestBufferScan
```

These are application STATE, not window state — which is what makes them the hard case. They are
not host services and cannot become `AppHostServices` fields without turning that record into a
second copy of `clsApp`. `clsApp` is shell-side and has not been measured; step 2 hit the same
wall in miniature and parked `g_sCommandLine` as a shell global rather than reach `gApp`.

**Nothing should be promised about the move until `clsApp` has had the same treatment this page
gave `clsDocument`.**

## E. Win32 the ratchet does not know — 2 names, 7 sites. A miss, and a gap.

`IsWindow` (5 sites) and `SetWindowRedraw` (2 sites) survived commit 2 **because neither is in
`g_banned`**. `IsWindowVisible` is; `IsWindow` is not.

Commit 2 converted what the checker flagged and stopped, which is the same mistake as trusting a
grep — one layer up. `gAppHost.IsViewAlive` already exists and is exactly what those five sites
want; `SetWindowRedraw` needs either a new field or a decision that redraw-suppression is not a
document concern.

**Both names should go into `g_banned`.** That is the fourth widening the list has needed, and
its own header already says: *"A checker is only as good as its vocabulary. Widen it whenever
something slips through, and read a green run as evidence rather than as proof."*

---

## What this means for the move

**Categories A, B and E are cheap or already done.** B dissolves, E is seven mechanical edits
against a field that exists, A is four edits plus a file move.

**C needs one decision** — split `modRoutines`, or route four functions through the host record.

**D is the whole question.** `clsApp` decides whether the document model can leave the shell at
all. It has now been measured — see below.


---

# `clsApp`, measured

Same method. **628 lines, 5 ratchet violations, 8 unresolved names.** Smaller and more tractable
than `clsDocument` was.

**IT DOES NOT SAY NO.**

| blocker | sites | verdict |
| --- | --- | --- |
| `hwnd` and `LRESULT` in signatures | 4 | mechanical — the same rename commit 2 applied to `clsDocument` |
| `AfxIFileSaveDialog` + `CoTaskMemFree`, for the PROJECT save | 1 | `gAppHost.AskSavePath` **already exists** and covers it |
| `DocView(pDoc, 0/1)` in `GetDocumentPtrByWindow` | 2 | compare `pDoc->hWindow(i)` directly, exactly as commit 2 did in `clsTopTabCtl` |
| `ScanMgr_GetRootName` | 2 | `clsScanMgr.bi` is **clean** |
| `frmMain_PositionWindows`, `frmTopTabsInfo_PositionWindows`, `frmOutput_UpdateToDoListview` | 4 | **the only new work**: three host notifications |

The last row is the whole of it. `clsApp` reaches into three forms to say "re-lay-out" and
"refresh the TODO list". Those are notifications, not queries, and they are the natural shape for
`AppHostServices` fields — the record already carries `CloseTab`, which is the same kind of thing
and is flagged in its own comment as the one to watch.

## The real find is one level down: `clsScanMgr`

`clsDocument` calls `gScanMgr.RequestBufferScan`, so the scan manager comes with it. Its header is
clean; **`clsScanMgr.inc` is 544 lines with 5 violations, and they are not cosmetic**:

* **four `CloseHandle`** on `m_hWakeEvent` and `m_hExitEvent` — Win32 **event objects**, used to
  wake and stop a worker thread.
* **one `PostMessage(HWND_FRMMAIN, MSG_USER_PARSE_COMPLETE, …)`** — worker to UI. This one maps
  straight across: `g_plat.events.Post` exists and its own comment describes it as
  *"application-defined, posted from worker threads"*.

**PsPlatform HAS NO THREADING OR SYNCHRONISATION SERVICE.** Verified: `Platform.bi` mentions
threads only in a comment about `SDL_AddTimer`, and the only mutex code in the tree is the
vendored `bind/SDL3/SDL3/SDL_mutex.bi`, which is never surfaced through `g_plat`. So a background
scanner that moved into `app/` would need one built.

**IT SHOULD NOT MOVE, AND THAT IS THE RECOMMENDATION.** Background parsing is a shell service by
the same argument `modDocViews.bi` uses for editor windows: it belongs to whoever owns the
process's UI thread. `RequestBufferScan` becomes one more notification on the record, the scan
manager stays in `src/`, and PsPlatform is not made to grow a threading layer to satisfy one
caller — which is the mistake `PsMenuBar.bi` warns about in a different context ("Promoting a
guess into the library is how you get an API you then unpick").

## So what does the move actually cost

Adding up this page, with everything measured rather than estimated:

| | |
| --- | --- |
| `clsDocument` categories A, B, E | cheap: B does not exist, E is 7 edits against a field that exists, A is 4 edits plus a file move |
| `clsDocument` category C | one decision — split `modRoutines`, or route 4 functions through the record |
| `clsDocument` category D → `clsApp` | tractable: 7 mechanical sites, 3 new notification fields |
| `clsScanMgr` | **stays in the shell**; one more notification field |
| PsPlatform | **no new capability required** |

**Nothing here says no.** The document model can leave the shell, and what it costs is roughly
four more record fields, two file moves, and one decision about `modRoutines`. That is a smaller
answer than step 3's plan assumed in either direction, and it is the first number on this page
that came from compiling everything involved rather than from reading it.


---

# `modRoutines`, measured — and the answer is SPLIT

The open question was whether the four functions `clsDocument` needs are among `modRoutines.bi`'s
twelve violations or clear of them. **Two are clear; two are not, and both convert mechanically.**

`modRoutines.bi` is 76 lines holding **25 unrelated declarations** — rich-edit centring, process
enumeration, a WM_COPYDATA reader, the AfxNova file-dialog wrappers, a listbox row helper. The 12
violations sit on 9 of those declarations. It is a grab-bag, and nothing about it wants moving
wholesale.

| what `clsDocument` needs | where | state |
| --- | --- | --- |
| `CompleteIncludeFilename` | `.bi:43` | **clean** |
| `GetFileToString` | `.bi:55` | **clean** |
| `Scintilla_GetTextBytes` | `.bi:53` | `hEdit as hwnd`; body makes **4** `SciExec` calls |
| `Scintilla_StripTrailingWhitespace` | `.bi:54` | `hEdit as hwnd`; body makes **14** `SciExec` calls |

**`SciExec` is a MACRO, not a function**: `#Define SciExec(h, m, w, l) SendMessage(h, m, w, CAST(LPARAM, l))`
(`modDeclares.bi:143`). So every one of those 18 sites is a `SendMessage` to an editor window, and
that — not the parameter type — is what binds these two functions to the shell.

The conversion is the same one commit 2 applied inside `clsDocument`: take the view POINTER and
call `SciMsg` instead. 18 mechanical edits, plus the signature.

**`Scintilla_GetTextBytes` has six call sites in four files** — `clsDocument` ×3, `frmMainFile`,
`modEncoding`, `modFormatApply` ×2 — so the signature change ripples to three other files. All are
shell-side and all have a document or a view in hand.

**Verdict: SPLIT, not route.** These are pure Scintilla and file logic, not host services; putting
them behind `AppHostServices` would be the wrong shape — that record is for things only the host
can answer, and "strip trailing whitespace from a buffer" is not one of them.

## The number nobody has counted: 420 `SciExec` sites

Measured across every `.inc` and `.bi`, excluding comments and the `#Define` itself:

| wrapper | sites | binds to | portability |
| --- | --- | --- | --- |
| `SciMsg(pSci, …)` | **374** | the view POINTER | free — `PsScintilla.bi` keeps the signature deliberately so these "are not edited, they are relinked" |
| `SciExec(hWnd, …)` | **420** | an editor **HWND**, via `SendMessage` | each one ties its file to a window |

Concentrated in `frmFindInProject` (92), `modFormatApply` (56), `modFindReplace` (39),
`modFindProject` (39), `modAutoInsert` (25) and `modRoutines` (21).

**Only 18 of the 420 matter for the document model**, and they are the two functions above.
The other 402 are in shell-side files that are not moving. But they are the port's real Scintilla
debt and no plan has counted them, so they are counted here.

**Two existing figures do not match the tree, and the arithmetic suggests why.** `modDocViews.bi`
says "~212 SciExec"; `PsScintilla.bi` says "801 SciMsg sites across 34 files". Today it is 420 and
374 — and 420 + 374 = 794, which is close enough to 801 that the second figure almost certainly
counted BOTH wrappers rather than `SciMsg` alone. Neither number should be quoted as the count of
one wrapper without re-deriving it.

## Updated cost of the move

| | |
| --- | --- |
| `clsDocument` A, B, E | B does not exist; E is 7 edits against an existing field; A is 4 edits plus a file move |
| `clsDocument` C | **split `modRoutines`**: 2 functions move as they are, 2 convert (18 `SciExec` → `SciMsg`, 6 call sites) |
| `clsApp` | 7 mechanical sites, 3 new notification fields |
| `clsScanMgr` | stays in the shell; one more notification field |
| PsPlatform | no new capability required |

Everything the move needs is now measured. Nothing in it is unknown, and nothing in it says no.
