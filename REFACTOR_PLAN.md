# tiko × fbcParser Integration Plan

Replace tiko's homegrown symbol machinery (`modParser` + `clsDB2`) with **fbcParser.dll**
(`C:\dev\fbcParser`) — a background worker thread continuously rescans as the user types,
and a new in-memory symbol database indexes the DLL's flat results in place, powering
codetips, autocomplete, the Functions panel, symbol search, F12 go-to-definition, and the
TODO panel.

> **Status: planning complete (2026-07-19), implementation not started.**
>
> **What the plan got wrong, recorded honestly:** *(filled in as phases land.)*

**Git protocol:** feature branch per phase off `development`, `--no-ff` merge, branch deleted
after; the author drives commits and pushes.
**Precondition:** the working tree carries an in-progress CTabBar fold-in (~25 modified
files) — that must be committed before Phase 0; every phase diffs against a clean tree.

## Decisions taken (2026-07-19, with the author — don't re-litigate)

1. **Two-tier scanning.** (a) *Project scan*: `FBCPARSER_SCAN` of the project's main file
   from disk — on project open and any file save. (b) *Buffer scan*: `FBCPARSER_SCAN_TEXT`
   of the active document's live text (virtual name = real absolute path) — debounced
   ~500 ms after last `SCN_MODIFIED`, immediate on file open / tab switch. Buffer-scan
   symbols for the active file override the project scan's copy of that file.
2. **Index-in-place storage.** The `FBCP_RESULT` pointers are the backing store; tiko builds
   thin indexes over them (case-insensitive name hash, per-file chains, child chains).
   Atomic swap on the UI thread; retired results freed on the worker (same-thread contract).
3. **Debounced idle-timer trigger** (plus open/save/tab-switch). The old
   `bNeedsParsing`/`ParseDocument` synchronous path dies.
4. **No disk cache / session persistence** — a scan is monolithic (always re-parses all
   includes; ~0.4 s at windows.bi scale), so a cache can't speed anything up.
5. **New query API; consumers rewritten.** `clsDB2` + `modParser` deleted at the end.
6. **Static codetips: FB intrinsics table only** survives (calltip fallback for `LEN`,
   `MID`, ...), as a separate sorted lookup — not records in the DB. WinAPI/WinFBX tables die.
7. **Diagnostics stored per scan + query API; no new UI this refactor.**
8. **TODO comments**: fbcParser can't see comments → a small dedicated line scanner on the
   worker over the same text copy feeds the TODO panel.

## Prerequisite: fbcParser change (verified gap, settled by inspection)

fbc represents `TYPE B EXTENDS A` as a compiler-generated field named `base$`
(`symb-struct.bas:107`), and the collector's filter drops every `$`-containing name
(`collector.bas:181`) — so **the EXTENDS base is not recoverable from FBCP output today**,
and `DereferenceLine` needs it (every AfxNova `CWindow`-derived type). Fix in the fbcParser
repo, before Phase 0: when collecting a `FBCP_KIND_TYPE`/`FBCP_KIND_UNION` symbol whose
`udt.base <> NULL`, pool the base type's name into the record's `typeOffset` (currently -1
for TYPEs). No struct change, no version bump; document in fbcParser.bi + README; verify
with a probe file (`type B extends A`) through fbcparser_test. Own commit in that repo.

## Architecture

```
UI thread                                 Worker thread (owns ALL fbcParser calls)
---------                                 ----------------------------------------
SCN_MODIFIED → debounce timer ──┐
file open / tab switch ─────────┼─→ gScanMgr.RequestBufferScan(pDoc)
file save / project open ───────┴─→ gScanMgr.RequestProjectScan()
   (copies doc text + snapshots include paths
    on the UI thread; latest-wins mailbox; SetEvent)
                                          loop: drain retire queue (fbcparser_free)
                                                take buffer req, else project req
                                                fbcparser_scan[_text]()
                                                build PARSERESULTSET indexes
                                                TODO-scan the text copy (buffer tier)
                                                store done slot; PostMessage(
                                                  HWND_FRMMAIN, MSG_USER_PARSE_COMPLETE, tier, 0)
MSG_USER_PARSE_COMPLETE handler:
   pOld = gSymDb.Swap(tier, gScanMgr.TakeCompleted(tier))
   gScanMgr.RetireResult(pOld)   ─→ freed by worker next loop
   refresh Functions / TODO panels
```

**Single-threaded read invariant:** consumers and swaps run only on the UI thread, so a
result set never changes or dies under a reader — no read-path locking, by construction.
Only the mailbox/done/retire slots are shared, under one `CRITICAL_SECTION`.
Threading idiom: FB `ThreadCreate`/`ThreadWait`, already established in tiko
(modCompile.inc:662 compile thread, frmDebug.inc:1285 gdb listener).

## New modules

| File | Contents |
|---|---|
| `src\fbcParser.bi` | vendored copy of `C:\dev\fbcParser\fbcParser.bi` |
| `src\clsScanMgr.bi/.inc` | `clsScanMgr` + `gScanMgr`: worker thread, latest-wins mailbox, retire queue, options marshalling, TODO scanner, shutdown handshake |
| `src\clsSymbolDb.bi/.inc` | `PARSERESULTSET`, `SYMBOLREF`, `clsSymbolDb` + `gSymDb`: indexes, two-tier merge, query API, calltip synthesis, FB-intrinsics lookup, diag/TODO stores |

### Key TYPEs (sketch)

```freebasic
enum SCAN_TIER : ScanTierProject = 1 : ScanTierBuffer = 2 : end enum

type SCANREQUEST
    tier          as long
    wszRootFile   as wstring * MAX_PATH   ' absolute; buffer tier: the virtual name
    pszText       as zstring ptr          ' buffer tier: heap copy of doc text; 0 for project
    cchText       as long
    sIncludePaths as DWSTRING             ' semicolon list, snapshotted on UI thread
end type

type clsScanMgr        ' one CRITICAL_SECTION + auto-reset wake event
    ' one pending slot per tier (latest-wins; buffer preferred when both pending),
    ' one done slot per tier, retire(any) queue of PARSERESULTSET ptr
    declare sub StartWorker()
    declare sub RequestProjectScan()
    declare sub RequestBufferScan( byval pDoc as clsDocument ptr )
    declare function TakeCompleted( byval tier as long ) as PARSERESULTSET ptr
    declare sub RetireResult( byval pSet as PARSERESULTSET ptr )
    declare sub Shutdown()   ' retire everything, signal, bounded ThreadWait join
end type

type PARSERESULTSET    ' built on worker, immutable afterwards
    pResult        as FBCP_RESULT ptr
    tier           as long
    rootFileIndex  as long
    wszRootFile    as wstring * MAX_PATH  ' ucased absolute
    hashSize       as long                ' FNV-1a over ucased pooled name; pow2 >= 2*count
    hashHead(any)  as long : hashNext(any)  as long
    fileHead(any)  as long : fileNext(any)  as long   ' per-file chains
    childHead(any) as long : childNext(any) as long   ' from parentIndex, decl order
end type

type SYMBOLREF : pSet as PARSERESULTSET ptr : idx as long : end type  ' idx -1 = miss
' never stored across swaps — acquired and dropped within one UI callback
```

**Merge rule** (recomputed in `Swap`): the buffer set contributes its root file (the
override) plus any file the project set lacks; the project set contributes everything except
the file matching the buffer root. Queries walk buffer-first with those masks. All filename
compares go ucased through one helper (fbcParser filenames are UPPERCASED absolute paths).

**Text handoff:** `RequestBufferScan` copies on the UI thread via the doc's existing
`GetText` path into an `allocate`d zstring buffer (Scintilla returns doc bytes, ANSI or
UTF-8 — both acceptable to SCAN_TEXT; FB identifiers are ASCII). Superseded pending copies
freed at replace time. Unsaved/new docs get a synthetic virtual name (`UNTITLED-n.BAS`).

**Shutdown:** end of `frmMain_OnClose`, before `DestroyWindow`: retire live sets, set the
shutdown flag, wake, bounded `ThreadWait` — the thread is joined before teardown unloads
the DLL.

### Query API (clsSymbolDb) → consumer mapping

```freebasic
FindProc(name) / FindType(name)            ' typedef/ALIAS resolved, cycle-guarded
FindMemberOf(refType, member)              ' fields + member-proc declares, walks EXTENDS
FindVariable(name, file, nCaretLine)       ' scope-aware: locals/params of enclosing proc
                                           '   (LOCAL flag + bodyLine..bodyEndLine), then
                                           '   module-level of file, then SHARED anywhere
ResolveTypeText(typeText)                  ' "WIDGET PTR" -> FindType("WIDGET")
EnumMembers(refType, r(), bWalkExtends)    ' autocomplete after . / ->
EnumPrefix(prefix, kindMask, r())          ' autocomplete lists
EnumLocalsInScope(file, nCaretLine, r())
EnumProcsInFile(file, r()) / EnumUserFiles(r())   ' Functions panel (user = not under toolchain inc)
EnumAllProcsTypes(r(), bUserFilesOnly)     ' symbol search feed
EnumDiags(r())                             ' DIAGREF {pSet, idx}; stored, no UI yet
SymName/SymTypeText/SymFile(ref) as wstring ptr ; SymLine/SymBodyLine/SymKind/SymFlags(ref)
BuildCalltip(refProc) as DWSTRING          ' from FBCP_KIND_PARAM children (skip INSTANCE):
                                           '   byref/optional decoration + " as <ret>"
FindIntrinsicCalltip(name) as DWSTRING     ' binary search gFBIntrinsics() (sorted, from
                                           '   rewritten clsConfig.LoadCodetipsFB)
```

| Consumer | New calls |
|---|---|
| `ShowCodetip` (modCodetips.inc:260) | `FindProc`→`BuildCalltip`; members via dereference→`FindMemberOf`; miss→`FindIntrinsicCalltip` |
| `DereferenceLine` (modCodetips.inc:109) | `FindVariable` → per-hop `ResolveTypeText` → `FindMemberOf` (EXTENDS inside); returns `SYMBOLREF` |
| `ShowAutocompleteList` (modCodetips.inc:356) | `" AS "`/`NEW`/`EXTENDS` → `EnumPrefix(types)`; `.`/`->` → `EnumMembers`; bare word → `EnumPrefix` + `EnumLocalsInScope` |
| F12 (frmMainSearch.inc:150-204) | same cascade over new API; jump = `SymBodyLine` if non-zero else `SymLine`, via `OpenSelectedDocument` |
| frmFunctions | `EnumUserFiles` + `EnumProcsInFile`; refresh on parse-complete |
| frmSearchSymbol | `EnumAllProcsTypes(true)` into the existing `fuzzy_match_positions` ranker |
| TODO panel (frmOutput.inc:51-77) | per-file TODO store (worker line-scanner; lift modParser's match rule) |

Locals-in-scope: proc-body granularity this refactor; block scopes noted as future.

## Include paths for FBCP_OPTIONS (snapshotted on the UI thread, same sources as compile)

1. The document's own directory (`AfxStrPathname("PATH", wszRootFile)`)
2. `gConfig.CompilerIncludes` (semicolon list, parsed as modCompile.inc:236-248 does)
3. Toolchain inc: `AfxStrPathname("PATH", gConfig.FBWINCompiler64) & "inc\"`
   (modCompile.inc:213 idiom)

The worker parses the snapshot into a `wstring ptr` array; `gConfig` is never touched
off-thread.

## Packaging

- Vendor `fbcParser.bi` + `libfbcParser.dll.a` into `src\`; implicit link (the .bi's
  `lib "fbcParser"` declares) with `-p .` added to `_compile.bat`'s fbc line.
- New `_copy_fbcparser.bat` (per the `_copy_*` convention): `fbcParser.dll` → `C:\dev\tiko\`
  (next to tiko.exe), refresh `src\fbcParser.bi` + `src\libfbcParser.dll.a`. Re-run after
  any fbcParser rebuild.

---

## Phases

Every phase: feature branch off `development` → `_compile.bat` clean, zero new warnings →
trace-verify as listed → *(done — notes)* recorded → `--no-ff` merge. Trace output always
passes through a `dim as string` first (DWSTRING/wstring print interleaving, Learnings.md).
Scans of unknown input run under the PowerShell hang guard (`Start-Job` + `Wait-Job -Timeout`).

### Phase P (fbcParser repo) — EXTENDS base exposure
The collector.bas change above; a probe file through fbcparser_test proves `typeOffset` of a
derived TYPE names its base; the existing smoke test still passes
(`FBCPARSER_NOPAUSE=1 fbcparser_test.exe _testfile_clean.bas` → 41 symbols, 0 diags).

*(done 2026-07-19 — implemented in `hTypeText`'s new `FB_SYMBCLASS_STRUCT` case
(collector.bas): base name taken from `udt.base->subtype` origname, so original casing is
preserved. Probe `_testfile_extends.bas` added to the repo: `Dog as Animal`, chained
`Wolf as Dog`, `Critter as OBJECT` for `extends object`, and no type text on baseless
TYPE/UNION — all as expected. Smoke test unchanged (41 symbols, 0 diags); REENTRANCY OK and
TEXTMODE byte-identical on both probe and clean file; build clean, zero warnings. Merged
`--no-ff` to fbcParser `master`, pushed (8359159). The rebuilt `fbcParser.dll` sits in
`C:\dev\fbcParser` ready for Phase 0's vendoring. NOT verified: nothing in tiko yet — no
tiko code was touched this phase.)*

### Phase 0 — Packaging & plumbing proof
Vendor artifacts, wire link + copy bat; temporary startup block scans `src\tiko.bas` via
`FBCPARSER_SCAN` (hardcoded paths), prints symbolCount/fileCount/diagCount/elapsed; probes
EXTENDS/typedef/local records; frees every result.
**Verify:** plausible counts (~30k+ symbols, sub-second), EXTENDS base visible. Temporary
block removed before merge.

*(done 2026-07-19 — vendored `src\fbcParser.bi` + `src\libfbcParser.dll.a`, DLL to root and
`src\` via new `_copy_fbcparser.bat`; `_compile.bat` gained `-p .`. Probe results (env-gated
block, removed before merge): full `src\tiko.bas` scan = rc 0, **191,858 symbols / 310 files /
0 diags** — ~6× the windows.bi benchmark, and it costs **~3.1 s cold AND warm** (no warm-up
benefit; the engine re-inits per scan). So a full-project scan is a ~3 s worker-thread
operation, not ~0.4 s: fine for save-triggered project scans and latest-wins-coalesced edits
of tiko.bas itself, and irrelevant for buffer scans of individual .inc files (no includes →
fast). Result memory at this scale ≈ 12 MB pool + 7.7 MB symbols per result. Text-entry-point
probe: EXTENDS base visible through the DLL (`Dog : Animal`), TYPEDEF (`DogAlias : Dog`) and
scope-aware local (`localDog`, flags LOCAL+UDTTYPE, parented to its sub) all correct.
**parentIndex can point forward** (locals are harvested before the global walk) — Phase 2's
index build must not assume parents precede children. Build clean, zero warnings, console
subsystem confirmed for `_compile.bat` builds so `print`-tracing works. NOT verified: nothing
interactive; root `tiko.exe` (author's `-s gui` build) not rebuilt; `src\fbcParser.dll` and
`src\tiko.exe` left as untracked dev residue.)*

### Phase 1 — Scan manager, worker thread, messages
New `clsScanMgr`; `MSG_USER_PARSE_COMPLETE` + `IDT_PARSER_DEBOUNCE` in modDeclares.bi;
handler in frmMain.inc (~:1510 select block) traces a summary and retires the set; triggers
wired: debounce restart at frmMainOnNotify.inc:427/:552, save → project scan
(frmMainFile.inc:203 area), project open, tab switch, file open (clsDocument.inc:306 area);
`gScanMgr.StartWorker()` at startup, `Shutdown()` in `frmMain_OnClose`. The old gdb2 path
stays live.
**Verify by trace:** one scan ~500 ms after a typing burst; mid-scan supersede frees the
pending copy; buffer preferred over project; clean exit join; no `FBCP_E_BUSY` ever; 50-scan
loop + `GetProcessMemoryInfo` working-set delta recorded (expect ~2–30 KB/scan upstream
residual); UTF-8 doc with non-ASCII comments scans with sane counts.

*(done 2026-07-19 — full pipeline live: `clsScanMgr.bi/.inc` (worker thread via
`ThreadCreate`, latest-wins mailbox with per-tier pending/done slots, retire queue, bounded
shutdown join on a manual-reset exit event — fbc 1.10 has no `ThreadDetach` and the FB thread
handle is not a Win32 HANDLE, so the join waits on the event, not the thread), triggers wired
(debounce restart in SCN_MODIFIED/SCN_CHARADDED, tab switch, file open, save → project scan,
project open → project scan), `MSG_USER_PARSE_COMPLETE` handler traces and retires. Verified
headless by driving the real exe (console-subsystem dev build + WM_CHAR posted to the
Scintilla child — the session foreground lock defeats SendKeys): project scan of tiko.tiko =
191,941 symbols / 312 files / 0 diags / 3.0 s, byte-identical counts to the fbcparser_test
harness; buffer scan served before a queued project scan; 8 single-keystroke bursts → exactly
8 scans; 6 rapid chars in one debounce window → exactly 1 scan; done-slot supersede observed
(12 scans → 11 deliveries, worker freed the unclaimed set); clean WM_CLOSE exit every run;
rc always FBCP_OK; UTF-8 comments fine; ~12.5 KB/scan working-set growth on a small file
(8-scan sample). **One real bug found and fixed:** `SaveConfigFile` converts the LIVE
`gConfig` path values to `{CURDRIVE}` placeholder form as a side effect, so the include-path
snapshot must run everything through `ProcessFromCurdriveApp` exactly like modCompile does —
without it the toolchain inc dir silently drops and a project scan shrinks to 33 files/1 diag.
Also: `pSet` is unusable as a variable name (fbc `PSET` keyword). NOT verified: the
pending-request supersede branch (scans finish too fast to stack two requests; same logic as
the verified done-slot supersede), save→project-scan trigger end-to-end (no project open in
the typing runs; the call site is one line), interactive feel/latency while typing — the
author's pass. The old gdb2 sync-parse path is untouched and still live.)*

### Phase 2 — clsSymbolDb: indexes, merge, query API, side-by-side check
Indexes, merge masks, full query API, `BuildCalltip`, intrinsics load (`LoadCodetipsFB`
rewritten to fill `gFBIntrinsics()`; WinAPI/WinFBX loading untouched for now); the handler
now swaps into `gSymDb`. Temporary debug command: walk every gdb2 USERCODE FUNCTION/TYPE,
query the new DB for the same name, print misses/line disagreements; dump `EnumMembers` on
known TYPEs (one with EXTENDS) and `BuildCalltip` vs old `CallTip` for ten procs.
**Verify:** diff summary recorded; known-difference classes (old-parser misses, casing,
better line numbers) recorded, not "fixed". Harness removed before merge.

### Phase 3 — Codetips, dotted dereference, autocomplete, F12
Rewrite `DereferenceLine` (returns `SYMBOLREF`), `ShowCodetip`, `ShowAutocompleteList`
internals; F12 cascade → new API; codetip-path `ParseDocument` calls removed.
**Verify:** temporary trace in `DereferenceLine` printing each hop's resolved type for
scripted expressions (`gApp.`, `pDoc->`, chained `a.b.c`, EXTENDS member, local UDT var) —
assert the chain, don't eyeball popups.
**NOT verified (author's interactive pass):** popup feel, calltip positioning,
`=`-termination path.

### Phase 4 — Panels: frmFunctions, frmSearchSymbol
frmFunctions → `EnumUserFiles` + `EnumProcsInFile` (its `ParseDocument` sweep at :30 dies;
refresh on parse-complete; rebuild through the panel's existing `SyncListboxFromModel` path —
Learnings.md listbox lesson); frmSearchSymbol → `EnumAllProcsTypes`. Behavior upgrade
recorded: panels now see never-opened include files (user = path not under toolchain inc).
**Verify:** old-vs-new item count trace; navigation into a never-opened include lands on the
right line (author confirms interactively).

### Phase 5 — TODO scanner and panel
Worker line-scanner (modParser's match rule lifted before deletion), per-file store keyed by
ucased filename, frmOutput reads it, close-file removes entries, parse-complete refreshes
the listview.
**Verify:** scripted N / N+1 / gone-on-close counts by trace.
**NOT verified:** listview click-through navigation (path unchanged) — author's pass.

### Phase 6 — Deletion and de-configuration
Delete `modParser.bi/.inc`, `clsDB2.bi/.inc`; remove `bNeedsParsing`/`ParseDocument`/
dirty-flag sets/sync-parse call sites; `clsConfig` loses WinAPI/WinFBX codetip loading +
settings (grep frmOptions* for their UI); `LoadCodetips` reduced to the intrinsics load.
**Verify:** zero grep hits for `gdb2|DB2_|ctxParser|bNeedsParsing|ParseDocument`; full smoke
run (open project, type, calltip, autocomplete, F12, panels, TODO, clean exit); the Phase 1
leak loop repeated once more and the number recorded.
**NOT verified:** long-session soak — the author's normal use is the soak test.

---

## fbc landmines to honor throughout (Learnings.md)

- Reserved-word params: no `pos`/`dir`/`width`/`line` parameter names — `nPosition`, etc.
- DWSTRING/wstring trace prints through `dim as string` first (concats stay wide).
- Warning 38: hoist booleans into locals; no mixed boolean/non-boolean `andalso` chains.
- No field named like its own type modulo case (`m_hWakeEvent`-shapes are safe).
- `NULL` is not FreeBASIC — use `0` in the new modules.
- Export names are UPPERCASE — already encoded in fbcParser.bi's aliases.
- `\` integer division with the divisor guarded *before* dividing (hash sizing).
- One scan at a time on one thread — an `FBCP_E_BUSY` sighting is a tiko bug, never a
  condition to retry around.

## Risks / accepted limitations

- **UTF-8/ANSI handoff:** FB identifiers are ASCII; non-ASCII comment/string bytes can't
  hurt symbol extraction. Verified in Phase 1.
- **Buffer copy per debounce:** one full text copy per fire; measured in Phase 1's trace.
- **Leak growth:** ~2–30 KB/scan upstream residual, accepted; measured in tiko at Phases 1
  and 6. Future option if it ever matters: out-of-proc scan worker, recycled. Not built now.
- **Unterminated-TYPE recovery quirk:** fbc parents trailing decls into a broken TYPE
  mid-edit; `EnumMembers`/autocomplete must tolerate absurd member lists (cap list sizes).
- **Stale line numbers inside the debounce window:** F12 can land lines off after heavy
  unscanned edits; the next scan self-corrects. Accepted.
- **Project scan reads disk:** unsaved edits in *non-active* files are invisible until
  saved — by design (decision 1).
