# tiko × fbcParser Integration Plan

Replace tiko's homegrown symbol machinery (`modParser` + `clsDB2`) with **fbcParser.dll**
(`C:\dev\fbcParser`) — a background worker thread continuously rescans as the user types,
and a new in-memory symbol database indexes the DLL's flat results in place, powering
codetips, autocomplete, the Functions panel, symbol search, F12 go-to-definition, and the
TODO panel.

> **Status: COMPLETE (2026-07-19) — all phases (P, 0–6, plus the 5b author-feedback
> follow-up) implemented, verified and merged. `modParser` and `clsDB2` are gone;
> fbcParser.dll and the two-tier clsSymbolDb are the only symbol machinery.**
>
> **What the plan got wrong, recorded honestly:**
> - *(Phase 3)* The prerequisite audit missed a second fbcParser location gap: a proc
>   with a separate DECLARE reported the **declare's** file/line while `bodyLine`
>   numbered lines in the **implementation** file — so F12 on any member proc landed in
>   the `.bi`, and `FindVariable`'s enclosing-proc test could never match a caret inside
>   a `.inc` member body (the proc record chained under the `.bi`'s file index). Found
>   by the Phase 3 harness's cross-file F12 assert; fixed in the fbcParser repo
>   mid-phase (see Phase 3 notes), same pattern as Phase P.

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
   *(Amended 2026-07-19, author requirement: TODO coverage must be **project-wide**, not
   active-file-only. The project tier's worker pass re-reads every user file the scan
   touched from disk and line-scans it; the buffer tier still scans the live text copy and
   its rows take precedence for the active document. TODO text must survive **non-latin
   encodings**: the byte→wide conversion goes through `DWSTRING.utf8` for UTF-8 content —
   per-document via `FileEncoding` for buffer text, BOM + validity heuristic for disk
   files; UTF-16 disk files are skipped, covered once opened/converted in-buffer.)*

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

*(done 2026-07-19 — `clsSymbolDb.bi/.inc` live: PARSERESULTSET (moved here from clsScanMgr.bi)
with FNV-1a name hash + per-file + parent→child chains built on the worker right after each
scan; `gSymDb.InstallSet` swaps atomically on the UI thread in the parse-complete handler and
retires the displaced set to the worker. Full query API + `BuildCalltip` + `gFBIntrinsics()`
(LoadCodetipsFB now additionally fills the sorted intrinsics table; gdb2 feed kept until
Phase 3/6). Cross-check on the real project (env-gated, removed before merge): **773 gdb2
USERCODE functions → 5 missing; 78 types → 0 missing**; member enumeration + EXTENDS walk +
calltip synthesis all verified against old output. One merge-semantics bug found and fixed:
a context-starved buffer scan of a lone .inc (117 symbols vs ~500) SHADOWED the project's
complete copy of that file — name lookups now fall back to the suppressed project copy on a
miss (verified: 9 private-proc misses → 0 with CTabBar.inc active). Known-difference classes,
recorded not fixed: (1) old parser lines are 0-based, fbcParser 1-based — **Phase 3 must
subtract 1 when driving Scintilla**; (2) multi-line proc headers: bodyLine = first body line,
a few lines past the `function` keyword; (3) the 5 remaining misses are explicit
constructors — fbc names them with `$` so the collector filter drops them; future fbcParser
addition if ctor calltips are wanted; (4) files open in tabs but not #include'd by the main
file (e.g. the in-progress frmTopTabsMenu.inc) are visible only while active — designed
two-tier semantics; (5) calltip type-texts are semantically resolved (HWND → `HWND__ ptr`,
`AFXNOVA.DWSTRING`) rather than source-verbatim — cosmetic, possible future fbcParser
improvement; (6) dynamic member arrays emit twin FIELD records — deduped in EnumMembers.
Also: fbc's `SWAP` keyword forbids a `Swap` method (renamed `InstallSet`), and
object-returning functions can't mix `function =` with `return`. NOT verified: no consumer
reads gSymDb yet (Phase 3); intrinsics lookup exercised only by unit call, not by a codetip.
The author's parallel TopTabsMenu WIP (modDeclares.bi, frmMain positioning, new
frmTopTabsMenu files, root tiko.exe deletion) was left uncommitted — the phase commit
carries only fbcParser work.)*

### Phase 3 — Codetips, dotted dereference, autocomplete, F12
Rewrite `DereferenceLine` (returns `SYMBOLREF`), `ShowCodetip`, `ShowAutocompleteList`
internals; F12 cascade → new API; codetip-path `ParseDocument` calls removed.
**Verify:** temporary trace in `DereferenceLine` printing each hop's resolved type for
scripted expressions (`gApp.`, `pDoc->`, chained `a.b.c`, EXTENDS member, local UDT var) —
assert the chain, don't eyeball popups.
**NOT verified (author's interactive pass):** popup feel, calltip positioning,
`=`-termination path.

*(done 2026-07-19 — all three consumers rewired to gSymDb. `DereferenceLine` keeps its
line-parsing prologue, then resolves part 1 via `FindVariable` (locals/params incl. the
implicit THIS — resolved naturally as the INSTANCE param, no special case) →
`FindProc` → `FindType`, and each later part via `SymRefTypeOf` (new helper: TYPE-ish ref
is its own type, else `ResolveTypeText` on the type text) → `FindMemberOf`; a final
unresolved part returns the previous hop's ref (that leniency is what the autocomplete
rebuild path feeds on — old code did the same implicitly). `ShowCodetip` =
`BuildCalltip` on SUB/FUNCTION refs, miss → `FindIntrinsicCalltip` on the identifier
before the paren. `ShowAutocompleteList`: DIM-AS = static builtin-type list +
`EnumPrefix(types)` (CONSTRUCTOR/DESTRUCTOR filtered to files outside toolchain inc);
member = `EnumMembers`; **plus the planned bare-word mode** (`AUTOCOMPLETE_WORD`, ≥2
chars, never after `.`/`->`, never overriding a sticky mode on backspace rebuilds) =
`EnumLocalsInScope` + `EnumPrefix` skipping LOCAL-flagged symbols; lists capped at
`AUTOCOMPLETE_MAX_ITEMS` (1000 — full windows.bi type list would otherwise cost ~0.5 s
of quadratic pipe-string dedupe). F12 cascade: member-of-dereferenced-type →
`FindProc` → `FindVariable` → `FindType`, jump = bodyLine|line **minus 1** (fbcParser
1-based → Scintilla 0-based). Codetip-path `ParseDocument` call removed; gdb2 is now
unreferenced by codetips/autocomplete/F12 (still fed for Phase 4/5 consumers).
**Mid-phase fbcParser fix (own repo, merged+pushed 820551a):** the harness's cross-file
F12 assert exposed that proc records carried the DECLARE's file/line with the body's
line range — see "what the plan got wrong". Collector now reports body file/line for
procs with a body (`dbg.incfile`); probe `_testfile_procbody.bas/.bi` added; smoke test
unchanged (41/0), REENTRANCY + TEXTMODE OK; DLL re-vendored via `_copy_fbcparser.bat`.
**Verified headless** (env-gated harness + timer bootstrap, removed before merge; dev
build run from repo root — `src\tiko.exe` can't start standalone, its settings/bin live
at root; harness output written straight to a file because CRT full-buffers redirected
stdout): 15/15 asserts — 8 dereference chains (local UDT var, 2-hop chain, EXTENDS walk,
gApp from project set, `->` through `clsDocument ptr`, member-fn calltip synthesis
`GetLine(nLine as long) as string`, THIS, THIS chain), 3 autocomplete list builds
(member list = exactly own9+Meth9+inner9-via-EXTENDS, DIM-AS = 1019 capped, bare word =
the local), 3 F12 jumps (member field line, TYPE line, cross-file member proc into
clsDocument.inc:781 exactly), intrinsic LEN. NOT verified (author's pass): popup feel /
calltip positioning / `=`-termination; bare-word popup ergonomics are **new UX** — the
2-char threshold and Enter-selects-from-popup behavior may want tuning; DIM-AS list is
larger than gdb2's (all scanned TYPEs, capped, arbitrary subset until a prefix is
typed). Session side effect of the headless runs: tiko.bas + clsDocument.inc were
opened/navigated (documents restored via undo+savepoint, never saved) and three default
`settings\*.ini/.session` files were created at the repo root by app exit.)*

### Phase 4 — Panels: frmFunctions, frmSearchSymbol
frmFunctions → `EnumUserFiles` + `EnumProcsInFile` (its `ParseDocument` sweep at :30 dies;
refresh on parse-complete; rebuild through the panel's existing `SyncListboxFromModel` path —
Learnings.md listbox lesson); frmSearchSymbol → `EnumAllProcsTypes`. Behavior upgrade
recorded: panels now see never-opened include files (user = path not under toolchain inc).
**Verify:** old-vs-new item count trace; navigation into a never-opened include lands on the
right line (author confirms interactively).

*(done 2026-07-19 — both panels rewired. **Item identity moved from pDoc pointers to
filenames**: never-opened include files have no clsDocument, so the Functions listbox now
stores an index into a shared `gFuncPanelFiles()` array (ItemData) + 0-based line
(ItemDataExtra); the click handler resolves a pDoc by filename when one exists, else
`OpenSelectedDocument` from disk; `frmFunctions_SelectItemData` matches by filename. Rows are
**implementations only** (`SymBodyLine > 0` — declare-only prototypes never appeared in the
old parsed-bodies panel either), captions are `gSymDb.QualifiedName` (new method:
"clsType.Proc" like gdb2's ElementName), tooltips synthesize via `BuildCalltip`, files sort
by display name. `frmFunctions_ReparseFiles`' sync sweep is now a `RequestBufferScan` of the
active doc; parse-complete refreshes the panel when it is visible. frmSearchSymbol: gSymbols
stores **plain data copies** (caption/file/line/isEnum), never SYMBOLREFs — the picker's
modal loop lets a background scan swap+free a result set mid-search; file rows now come from
`EnumUserFiles` too. `QuickSortpDocs` moved to frmExplorer.inc (its only remaining consumer);
new `QuickSortFilenames` shared by both panels. Verified headless (env-gated harness opening
tiko.tiko, removed before merge): 134 user files; per-open-file old-gdb2 vs new counts
**identical for every file except five, each exactly one lower — the five explicit
constructors** the collector's `$`-filter drops (same list Phase 2 recorded); tree = 134
headers + 1063 rows, flat list = 1063 (consistent); SelectItemData(active doc) = true;
search feed = 1285 qualified user symbols. Known differences, recorded not fixed: (1)
property procs no longer carry a "(get)"/"(set)" suffix — FBCP has no property kind, a
get/set pair lists as two same-named rows; (2) unsaved/untitled documents no longer appear
in the panel (their synthetic scan name has no disk file); (3) **without a project open the
panels only see the active document's buffer scan** — old code parsed every open document
individually; designed two-tier consequence, flagged for the author's judgement. NOT
verified (author's pass): click-through into a never-opened include landing on the right
line, tooltip feel, panel refresh cadence while typing.)*

### Phase 5 — TODO scanner and panel
Worker line-scanner (modParser's match rule lifted before deletion), per-file store keyed by
ucased filename, frmOutput reads it, close-file removes entries, parse-complete refreshes
the listview.
**Verify:** scripted N / N+1 / gone-on-close counts by trace.
**NOT verified:** listview click-through navigation (path unchanged) — author's pass.

*(done 2026-07-19 — `ScanMgr_ScanTodos` runs on the worker over each buffer-scan text copy
right before the copy is freed; the lifted match rule: the FIRST apostrophe outside a string
literal, optional spaces, then "todo:" case-insensitively — same effective rule as
modParser's tokenizer (which only tested the start of a comment, so a second `'` on the line
is never considered). Items ride the PARSERESULTSET (`todoItems()/todoCount`) to the UI;
`clsSymbolDb.InstallSet` (buffer tier) replaces that file's rows in a **flat UI-thread store**
(`gTodoItems()` in clsSymbolDb — flat rather than the planned nested per-file arrays, which
would have needed redim-preserve of UDTs with var-len array members); `clsApp.RemoveDocument`
/ `RemoveAllDocuments` drop a closed document's rows and refresh. `frmOutput_UpdateToDoListview`
reads the store; line numbers are **1-based** now, which is what the click-through
(`SetDocumentErrorPosition`'s `val(text)-1`) always expected — the old panel showed the
0-based parser line, so old click-through was off by one; fix, not regression.
`ScanMgr_GetRootName` promoted to a public declare (the store, DereferenceLine and F12 all
use it as a document's DB identity). Verified headless (env-gated scripted harness, removed
before merge): scratch file with 2 TODOs opened → store shows exactly 2 with correct 1-based
lines and text; append a third + rescan → 3; undo + rescan → 2; close via the real
`OnCommand_FileClose` path → 0 (gone-on-close). Population semantics recorded honestly:
the store only fills as documents get buffer-scanned (open/tab-switch/typing) — TODOs in
never-activated files don't appear (old code parsed every open doc on load; project-wide
TODO coverage would need a disk-side scan pass, deliberately not built — decision 8 scopes
the scanner to the in-memory text copy). NOT verified (author's pass): listview
click-through navigation and multi-file accumulation feel during a real session.)*

*(5b, done 2026-07-19 — two author-reported gaps fixed same day. **(1) Non-latin TODO text
garbled**: the scanner assigned captured bytes to DWSTRING as ANSI; UTF-8 content (the
editor's default in-buffer encoding) mangled. Now the byte→wide conversion is
encoding-aware: buffer scans carry `bTextUtf8` (from `pDoc->FileEncoding` — ANSI docs keep
Scintilla codepage 0, everything else is UTF-8 in-buffer) and convert via `DWSTRING.utf8`.
**(2) Project-wide scanning** (decision 8 amended): `ScanMgr_ScanProjectTodos` runs on the
worker after each project scan, re-reading every user file from disk (toolchain inc
excluded via a prefix snapshotted into the request), BOM-aware (UTF-8 BOM skipped, UTF-16
files skipped entirely — covered once opened), UTF-8-vs-ANSI decided by a sequence-validity
heuristic for BOM-less files. `clsSymbolDb.InstallProjectTodos` replaces each swept file's
rows (zero-item files get cleared, so on-disk deletions propagate) while leaving the active
document's fresher buffer rows alone; the panel now refreshes on both tiers. Verified
headless: project sweep found tiko.bas:54 / frmOutput.inc / frmPanel.bi TODOs with **no**
file ever activated; a UTF-8-BOM scratch with `' TODO: 这是一项待办事项` round-tripped
byte-exactly through the store (8 wide chars, 24 utf8 bytes) AND through the listview
(cell text read back equals the expected codepoints); gone-on-close still passes. Known
cosmetics: project-sourced rows show the scan's UPPERCASED path until the file's first
buffer scan replaces them with the original-case name; legacy ANSI files with high-bytes
that happen to form valid UTF-8 would convert as UTF-8 (heuristic limit). NOT verified
(author's pass): listview click-through, CJK rendering in the actual owner-draw font.)*

### Phase 6 — Deletion and de-configuration
Delete `modParser.bi/.inc`, `clsDB2.bi/.inc`; remove `bNeedsParsing`/`ParseDocument`/
dirty-flag sets/sync-parse call sites; `clsConfig` loses WinAPI/WinFBX codetip loading +
settings (grep frmOptions* for their UI); `LoadCodetips` reduced to the intrinsics load.
**Verify:** zero grep hits for `gdb2|DB2_|ctxParser|bNeedsParsing|ParseDocument`; full smoke
run (open project, type, calltip, autocomplete, F12, panels, TODO, clean exit); the Phase 1
leak loop repeated once more and the number recorded.
**NOT verified:** long-session soak — the author's normal use is the soak test.

*(done 2026-07-19 — `modParser.bi/.inc` and `clsDB2.bi/.inc` deleted (git rm), every call
site removed: the `ParseDocument` method and `bNeedsParsing` field are gone from
clsDocument; the SCN_CHARADDED/SCN_MODIFIED dirty-flag sets, the save-path and
find-replace sync-parses (find-replace now issues a `RequestBufferScan`), and the
gdb2 delete calls in clsApp/frmMainFile/frmMainProject are all removed.
`OpenSelectedDocument`'s FunctionName lookup was rewritten onto `gSymDb.FindProc`
(no live caller uses that path today, but the signature keeps working).
`clsConfig`: `LoadCodetipsGeneric`/`LoadCodetipsWinFBX`/`LoadCodetipsWinAPI` deleted with
their filename fields; `LoadCodetipsFB` is now purely the `gFBIntrinsics()` load;
`LoadCodetips` reduced to it. No WinAPI/WinFBX settings UI existed beyond the master
Codetips checkbox, which stays. Verified: `grep -E "gdb2|DB2_|ctxParser|bNeedsParsing|
ParseDocument"` over src = **zero hits**; build clean, zero warnings; headless smoke run
against the tiko project all-PASS (12 asserts): intrinsic calltip, FindProc+BuildCalltip,
FindType clsApp + 57 members, EnumPrefix, 129 user files, Functions panel 1114 rows, TODO
store populated, real posted-WM_CHAR typing produced a fresh buffer set through the
debounce, F12 from an `OpenSelectedDocument(` call site landed in modRoutines.inc 4 lines
into the multi-line header (bodyLine semantics), clean WM_CLOSE exit rc 0. Leak loop
(50 buffer scans of a small starved file, working-set delta): three runs measured
**−1.3, −2.0 and +39.9 KB/scan** — the sign flips between runs, so working-set noise
dominates any per-scan residual at this scale; no linear growth, consistent with the
accepted upstream residual. NOT verified: long-session soak (the author's normal use),
and the interactive feel of everything the phase notes above already flagged.)*

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
