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
all, and it has not been measured. Everything else on this page is arithmetic; this is the part
that could still say no.
