'' ========================================================================================
'' shellscan -- the symbol scan, SYNCHRONOUS. The counterpart of tiko's clsScanMgr, and
'' deliberately about a tenth of its size.
''
'' ---- WHY THIS IS NOT A PORT OF clsScanMgr ----------------------------------------------
''
'' 7c step 5's plan opened with "threading in PsPlatform", because the Functions panel needs
'' gSymDb populated and tiko populates it from a worker thread. Measuring killed that:
''
''   * The panel reads gSymDb, NOT the scanner (frmFunctions.inc:448).
''   * clsSymbolDb and PARSERESULTSET are ALREADY IN app/. The whole data path -- parse
''     result, indexes, symbol database, panel -- is inside the portable layer already.
''   * The parse is ONE DLL CALL. Everything else in clsScanMgr's 544 lines is queueing,
''     locking, retiring and thread lifetime.
''
'' So this file does what the worker does between taking a request and publishing it, on the
'' calling thread, in one function. What it gives up is asynchrony; what it costs is measured
'' rather than assumed -- see the scanMs the install path prints, and docs/port/7c-step5.md.
''
'' ---- WHAT IS DELIBERATELY NOT HERE -----------------------------------------------------
''
''   * THE PROJECT TIER. This binary has no project system, so there is nothing to root a
''     project-wide scan at. Buffer tier only.
''   * THE TODO SCAN. ScanMgr_ScanTodoBytes belongs to the TODO store, which this binary
''     does not have -- gAppNotify.RefreshTodoList is a legitimate no-op here.
''   * THE RETIRE QUEUE. It exists because a set displaced on the UI thread must be freed on
''     the WORKER thread (fbcparser_free's same-thread contract). With one thread there is
''     no contract to honour: the displaced set is freed where it is displaced.
''   * THE STALE-ROOT TEST. tiko discards a result whose root no longer matches the active
''     document, because a scan started before a tab switch can land after it. Synchronously
''     the result cannot arrive late -- there is no window in which the root can change.
''
'' Each of those four is a consequence of NOT having a thread, which is the point: most of
'' clsScanMgr is not parsing, it is the machinery around a parse that happens elsewhere.
'' ========================================================================================

#pragma once


'' The elapsed milliseconds of the last scan, and how many there have been. Read by the
'' self-test and by docs/port/7c-step5.md's measurement -- THE number this step exists to
'' produce, since it is what decides whether a thread is needed at all.
dim shared as long g_nLastScanMs
dim shared as long g_nScanCount


'' ---------------------------------------------------------------------------------------
'' Only source files are worth parsing. tiko's ScanMgr_IsScannable (clsScanMgr.inc:210),
'' unchanged: a document with no path is an unsaved buffer and IS scannable -- it is being
'' typed into, which is exactly when the panel should follow it.
'' ---------------------------------------------------------------------------------------
private function ShellScan_IsScannable( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsInStr( wszFile, "\" ) = 0 then return true
    select case PsLCase( PsPathExt(wszFile) ).Utf8
        case ".bas", ".bi", ".inc" : return true
    end select
    return false
end function


'' The name the parser roots the scan at. tiko's ScanMgr_GetRootName, unchanged: a document
'' with no path gets a synthetic one keyed by its address, so two untitled buffers do not
'' collide in the symbol database.
private function ShellScan_RootName( byval pDoc as clsDocument ptr ) as DWSTRING
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsInStr( wszFile, "\" ) > 0 then return wszFile
    return PsExePath & "UNTITLED-" & hex( cast( uinteger, pDoc ) ) & ".BAS"
end function


'' ---------------------------------------------------------------------------------------
'' Scan one document's buffer and install the result. TRUE if gSymDb took it.
'' ---------------------------------------------------------------------------------------
function ShellScan_Buffer( byval pDoc as clsDocument ptr ) as boolean
    if ShellScan_IsScannable( pDoc ) = false then return false

    '' THE TEXT AS SCINTILLA HAS IT, falling back to the document's own buffer. Both are
    '' bytes -- ANSI or UTF-8 -- and the DLL takes either. A document that has been loaded
    '' but never shown has no view, which is the case the second line is for.
    dim as string sText
    if gAppHost.IsViewAlive( pDoc->hWindow(0) ) then sText = pDoc->GetText()
    if len( sText ) = 0 then sText = pDoc->TextBuffer
    if len( sText ) = 0 then return false

    dim as DWSTRING wszRoot = ShellScan_RootName( pDoc )

    '' NO INCLUDE PATHS. tiko builds them from the active build configuration; this binary
    '' has none, so an #include resolves against the file's own directory or not at all --
    '' the same answer ShellHost_ResolveIncludePath already gives.
    dim as FBCP_OPTIONS opts
    opts.version = FBCP_VERSION
    opts.includeCount = 0

    dim as double t0 = timer
    dim as FBCP_RESULT ptr pRes = 0
    dim as long rcScan = fbcparser_scan_text( strptr(sText), wszRoot.Wz(), @opts, @pRes )

    dim as PARSERESULTSET ptr pRSet = new PARSERESULTSET
    pRSet->pResult    = pRes
    pRSet->tier       = ScanTierBuffer
    pRSet->scanRc     = rcScan
    pRSet->scanMs     = clng( (timer - t0) * 1000 )
    pRSet->wszRootFile = *wszRoot.Wz()

    '' THE INDEXES ARE BUILT HERE, where tiko builds them on the worker "off the UI thread".
    '' There is no off-the-UI-thread in this binary, and that is the cost being measured.
    pRSet->BuildIndexes()

    g_nLastScanMs = pRSet->scanMs
    g_nScanCount += 1

    '' InstallSet hands back whatever it displaced. tiko gives that to the worker to free
    '' under fbcparser_free's same-thread contract; with one thread it is freed right here,
    '' which is the same contract trivially satisfied.
    dim as PARSERESULTSET ptr pOld = gSymDb.InstallSet( ScanTierBuffer, pRSet )
    if pOld then
        if pOld->pResult then fbcparser_free( pOld->pResult )
        delete pOld
    end if

    print "tikoshell: scanned " & PsPathName( wszRoot ).Utf8 & _
          " -- rc=" & rcScan & " ms=" & pRSet->scanMs & _
          iif( pRes <> 0, " symbols=" & str(pRes->symbolCount), " (no result)" )

    return true
end function


'' ========================================================================================
'' THE DEBOUNCE -- tiko's IDT_PARSER_DEBOUNCE, ported.
''
'' A parse per KEYSTROKE would be 18ms of dead UI per character (see the measurement in
'' docs/port/7c-step5.md); a parse when TYPING STOPS is 18ms the user is not waiting on.
'' That distinction is the whole reason tiko can afford a background scan at all, and it is
'' why this is a port rather than a simplification: the debounce is doing more work here
'' than it does in tiko, because there the parse is on another thread anyway.
''
'' tiko restarts it from SCN_MODIFIED (frmMainOnNotify.inc:439) and SCN_CHARADDED (:568),
'' both with the same 500ms, and the WM_TIMER arm kills the timer before requesting the scan
'' so it fires exactly once per pause (frmMain.inc:1476-1479).
''
'' ---- THE POLICY IS SPLIT FROM THE TIMER, and that is what makes it assertable ----------
''
'' PsTimerSet needs a widget and a live surface. ShellScan_DebouncePolicy needs neither: it
'' is (what happened, what is pending) -> (what to do), which the self-test drives over a
'' synthetic clock. tiko's version of this rule is spread across three files and is reachable
'' only by typing.
'' ========================================================================================

'' The same 500ms tiko uses (PARSER_DEBOUNCE_MS, modDeclares.bi:263).
const SH_PARSER_DEBOUNCE_MS = 500

'' The timer id on the editor widget. tiko's is 501 on frmMain; the number is arbitrary and
'' only has to be unique among timers set on the SAME widget, which is why it does not
'' matter that it differs from tiko's.
const SH_TIMER_PARSE = 1

enum ShellDebounceAction
    SHDB_IGNORE = 0      '' nothing to do -- not an edit, or a load in progress
    SHDB_RESTART         '' an edit arrived: (re)start the timer
    SHDB_SCAN            '' the pause elapsed: kill the timer and parse
end enum

'' PURE. `nCode` is the Scintilla notification code, or 0 for "the timer fired"; `nModType`
'' is the notification's modificationType, ignored for anything but SCN_MODIFIED.
function ShellScan_DebouncePolicy( byval nCode as long, _
                                   byval nModType as long, _
                                   byval bLoading as boolean ) as ShellDebounceAction
    '' NOT WHILE LOADING, which is tiko's guard (frmMainOnNotify.inc:437): filling a
    '' document generates modification notifications for the text being inserted, and the
    '' load path requests its own scan when it finishes. Without this, opening a 260 KB file
    '' would queue a second parse of it for 500ms later.
    if bLoading then return SHDB_IGNORE

    select case nCode
        case 0
            return SHDB_SCAN

        case SCN_MODIFIED
            '' ---- INSERT OR DELETE ONLY, AND THIS IS THE LOAD-BEARING LINE.
            ''
            '' SCN_MODIFIED IS NOT "the text changed". Scintilla raises it for styling
            '' (SC_MOD_CHANGESTYLE), for markers, for fold levels and for the BEFORE* pair
            '' as well as for real edits -- and this binary styles continuously, so a policy
            '' that restarted on the bare code would rearm the timer forever and the parse
            '' would never fire, or fire during syntax colouring that changed no text.
            ''
            '' tiko tests exactly these two bits (frmMainOnNotify.inc:381-383).
            if (nModType and SC_MOD_INSERTTEXT) orelse (nModType and SC_MOD_DELETETEXT) then
                return SHDB_RESTART
            end if
            return SHDB_IGNORE

        case SCN_CHARADDED
            return SHDB_RESTART
    end select
    return SHDB_IGNORE
end function


'' ---------------------------------------------------------------------------------------
'' THE SINK AND THE TIMER, which is everything the policy above cannot be tested with.
''
'' The sink must OUTLIVE the attach -- PsSciNotify.inc says so in as many words: "held by the
'' host, for as long as the editor lives", because the trampoline dereferences its address on
'' every notification. A local would be a use-after-free with a delay on it.
'' ---------------------------------------------------------------------------------------
dim shared as PsSciNotifySink g_sciSink

'' g_bScanSuppressed -- TRUE while a document is being filled -- is declared with the other
'' cross-file flags in tikoshell.bas, NOT here. shelltabs.bi sets it around
'' AssignTextBuffer and is included BEFORE this file, so a declaration here is too late:
'' "Variable not declared", in the other file, naming a symbol this one owns.
''
'' It is set around the fill rather than read from gApp.IsFileLoading, which is tiko's flag
'' driven by tiko's open path -- a path this binary does not run.


'' Runs the pause's scan. The timer's callback, and also called directly by the self-test,
'' which cannot wait 500ms of wall clock.
sub ShellScan_DebounceFire( byval user as any ptr = 0, byval nId as long = 0, _
                            byval nNow as ulongint = 0 )
    '' ---- THIS KILL IS REDUNDANT WITH bRepeat = FALSE, AND BOTH ARE KEPT ---------------
    '' Measured, not assumed. Removing this line leaves "fires once and is gone" GREEN,
    '' because PsTimer removes a one-shot itself; setting bRepeat = true leaves it green
    '' too, because this line removes it. Only breaking BOTH turns it red -- which says the
    '' assertion is sound and the mechanism is double-covered, not that either is idle.
    ''
    '' Both stay. The kill is what makes this safe to call DIRECTLY -- the self-test does,
    '' and so would any future "rescan now" command -- with a timer still pending.
    ''
    '' tiko needs its KillTimer for a harder reason: a Win32 timer REPEATS by default, so
    '' the arm at frmMain.inc:1478 would fire every 500ms forever without it.
    PsTimerKillProc( @g_sciSink, SH_TIMER_PARSE )
    dim as clsDocument ptr pDoc = ShellTabs_CurrentDoc()
    if pDoc = 0 then exit sub
    ShellScan_Buffer( pDoc )
end sub


'' ---- THE PROC FORM OF PsTimer, NOT THE WIDGET FORM, and the choice is forced.
''
'' A widget timer is delivered to that widget's OnEvent. The obvious owner here is the
'' EDITOR -- it is where the typing happens -- but PsSciView is PsPlatform's widget and its
'' OnEvent is not this binary's to extend, so a PSEV_TIMER sent there would arrive somewhere
'' the shell cannot act on. PsTimer.bi keeps the proc form for exactly this shape of caller
'' ("hosts that are not widgets"), and warns what it costs: the widget form is purged by
'' ~PsWidget, this one is not, so a host that forgets PsTimerKillProc leaves a timer calling
'' into freed memory. ShellScan_DebounceFire kills it first thing, every time.
''
'' The key is (user, id); @g_sciSink is a stable address that outlives the pump.
private sub ShellScan_ArmTimer()
    PsTimerSetProc( @g_sciSink, SH_TIMER_PARSE, SH_PARSER_DEBOUNCE_MS, PsTimerNow(), _
                    @ShellScan_DebounceFire, false )
end sub


'' The notification sink itself. Everything it does is decided by the policy above.
private sub ShellScan_OnSciNotify( byval user as any ptr, byref ev as PsEvent )
    select case ShellScan_DebouncePolicy( ev.notify.code, ev.notify.modificationType, _
                                          g_bScanSuppressed )
        case SHDB_RESTART : ShellScan_ArmTimer()
    end select
end sub


'' Attach the sink to the editor. Called once, after the views exist.
sub ShellScan_Install()
    if g_view = 0 then exit sub
    PsSciNotifyAttach( g_sciSink, g_view->pSci, @ShellScan_OnSciNotify, 0 )
end sub
