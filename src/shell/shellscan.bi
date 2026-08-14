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

'' THE PROJECT TIER'S OWN COUNTERS, kept separate on purpose (7c step 6). The two scans cost
'' different amounts -- one file's text against a whole include graph read from disk -- and
'' averaging them would hide exactly the number step 6 exists to produce.
dim shared as long g_nLastProjMs
dim shared as long g_nProjCount


'' ========================================================================================
'' THE WORKER (7c step 7) -- and what came back with it.
''
'' Step 5 shipped this file SYNCHRONOUS and its header listed four things that dropped out
'' BECAUSE there was one thread. Step 6 measured 1.2 SECONDS on a 134-file include graph, so
'' the parse moved off the UI thread and three of those four are back. Each is re-earned
'' below rather than re-copied from clsScanMgr:
''
''   1. THE RETIRE QUEUE. fbcparser_free has a same-thread contract: a set displaced on the
''      UI thread by InstallSet must be freed on the WORKER. So the UI hands it back.
''   2. THE STALE-ROOT TEST. A scan started before a tab switch can now land after it, and
''      installing it would resurrect the previous document's symbols.
''   3. LIFETIME. A worker still running at exit is how a clean quit becomes an intermittent
''      crash.
''
'' ---- WHAT THE UI THREAD DOES AND WHAT THE WORKER DOES ----------------------------------
''
'' UI:      copies the text (SCINTILLA IS NOT THREAD-SAFE), fills a request slot, signals.
'' WORKER:  parses, builds indexes, publishes into a done slot, posts PSEV_USER.
'' UI:      on PSEV_USER, installs into gSymDb and reloads the panel.
''
'' NOTHING BUT THE PARSE HAPPENS ON THE WORKER. gSymDb, the panel, the documents and every
'' widget stay single-threaded, which is what makes this a change of ONE function's timing
'' rather than a change to the toolkit's threading model.
''
'' ---- LATEST WINS, ONE SLOT PER TIER ----------------------------------------------------
''
'' tiko's rule (clsScanMgr.inc:288-300): a newer request replaces a pending one in its own
'' tier. Typing produces a request every time the debounce fires, and queueing them would
'' mean parsing states the user has already moved past.
'' ========================================================================================

const SH_MAX_RETIRE = 16

type ShellScanRequest
    bValid  as boolean
    sText   as string        '' buffer tier only -- copied on the UI thread
    wszRoot as DWSTRING
    wszDir  as DWSTRING      '' project tier's one include path
end type

dim shared as PsThread    g_scanThread
dim shared as PsMutex     g_scanMtx
dim shared as PsCondition g_scanCond
dim shared as boolean     g_bScanQuit          '' guarded by g_scanMtx
dim shared as boolean     g_bScanStarted

dim shared as ShellScanRequest g_reqBuf, g_reqProj    '' pending, guarded
dim shared as PARSERESULTSET ptr g_doneBuf, g_doneProj  '' finished, guarded

'' Sets the UI displaced and the worker still owes a free. Guarded.
dim shared as PARSERESULTSET ptr g_retire(0 to SH_MAX_RETIRE - 1)
dim shared as long g_nRetire

'' The code posted through g_plat.events.Post. Any nonzero value would do; it is named so
'' the pump's arm reads as something other than a magic number.
const SH_USER_SCAN_DONE = 1


'' ---------------------------------------------------------------------------------------
'' HAND A SET BACK TO THE WORKER TO FREE.
''
'' fbcparser_free has a SAME-THREAD CONTRACT -- the thread that allocated a result is the
'' one that may release it -- so a set displaced on the UI thread cannot simply be deleted
'' there. tiko carries the identical queue (clsScanMgr.inc:376-393) for the identical
'' reason, and step 5 deleted it with a note saying it existed only because of the thread.
''
'' IF THE QUEUE IS FULL THE SET IS LEAKED, deliberately and loudly. The alternative is
'' freeing it on the wrong thread, which is a corruption that surfaces somewhere else
'' entirely; sixteen outstanding is already far beyond what two tiers can produce.
'' ---------------------------------------------------------------------------------------
sub ShellScan_Retire( byval pRSet as PARSERESULTSET ptr )
    if pRSet = 0 then exit sub

    PsMutexLock( g_scanMtx )
    if g_nRetire < SH_MAX_RETIRE then
        g_retire(g_nRetire) = pRSet
        g_nRetire += 1
        PsCondSignal( g_scanCond )
        PsMutexUnlock( g_scanMtx )
    else
        PsMutexUnlock( g_scanMtx )
        print "tikoshell: retire queue full -- leaking a result set rather than " & _
              "freeing it on the wrong thread"
    end if
end sub


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

    '' ---- THE TEXT OF *THIS* DOCUMENT, WHICH IS NOT WHAT GetText ALONE ANSWERS ----------
    ''
    '' clsDocument.GetText reads the ACTIVE VIEW, and this binary has ONE view for every
    '' tab. Asking a BACKGROUND document for its text therefore returns the FOREGROUND
    '' document's -- and the scan then installs those symbols under the background file's
    '' name. Two tabs, and the Functions panel lists one file's procedures under the
    '' other's heading.
    ''
    '' FOUND BY THE SUITE, which is worth recording because almost nothing else in this port
    '' was: a tier assertion reported "tab1=3" for a file containing four print statements
    '' and no procedures at all.
    ''
    '' It is the SAME DEFECT the bookmarks loader has in shellpanel.bi, whose header
    '' explains it at length -- written two commits earlier, by me, and not carried across
    '' to the scanner. The fix is the same dance: point the view at the document, read,
    '' point it back, and restore the caret, because SCI_SETDOCPOINTER resets it.
    dim as string sText
    dim as long idxTab = ShellTabs_IndexOfDoc( pDoc )

    if (idxTab >= 0) andalso (g_view <> 0) andalso (g_tabDocs(idxTab).pSciDoc <> 0) then
        dim as any ptr pWasDoc = cast( any ptr, g_view->Msg(SCI_GETDOCPOINTER, 0, 0) )
        dim as long nWasPos    = g_view->Msg( SCI_GETCURRENTPOS, 0, 0 )
        dim as long nWasFirst  = g_view->Msg( SCI_GETFIRSTVISIBLELINE, 0, 0 )
        if pWasDoc <> 0 then g_view->Msg( SCI_ADDREFDOCUMENT, 0, cast(integer, pWasDoc) )

        g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, g_tabDocs(idxTab).pSciDoc) )
        sText = pDoc->GetText()

        if pWasDoc <> 0 then
            g_view->Msg( SCI_SETDOCPOINTER, 0, cast(integer, pWasDoc) )
            g_view->Msg( SCI_RELEASEDOCUMENT, 0, cast(integer, pWasDoc) )
            g_view->Msg( SCI_GOTOPOS, nWasPos, 0 )
            g_view->Msg( SCI_SETFIRSTVISIBLELINE, nWasFirst, 0 )
        end if
    elseif gAppHost.IsViewAlive( pDoc->hWindow(0) ) then
        '' No tab of its own -- nothing in this binary reaches here today, but a document
        '' the host created outside the tab model would.
        sText = pDoc->GetText()
    end if

    '' A document that has been loaded but never shown holds its text here instead.
    if len( sText ) = 0 then sText = pDoc->TextBuffer
    if len( sText ) = 0 then return false

    dim as DWSTRING wszRoot = ShellScan_RootName( pDoc )

    '' ---- FROM HERE THE UI THREAD IS DONE. Everything above ran here because it had to:
    '' Scintilla is not thread-safe, so the text is copied on the thread that owns the view,
    '' which is exactly what tiko does (clsScanMgr.inc:331-347) and for the same reason.
    ''
    '' LATEST WINS. A pending request is overwritten rather than queued -- typing produces
    '' one per debounce, and a queue would parse states the user has already moved past.
    PsMutexLock( g_scanMtx )
    g_reqBuf.bValid  = true
    g_reqBuf.sText   = sText
    g_reqBuf.wszRoot = wszRoot
    PsCondSignal( g_scanCond )
    PsMutexUnlock( g_scanMtx )

    '' TRUE MEANS "ASKED FOR", NOT "DONE", and every caller of this function had to be
    '' re-read when that changed. It is the same word with a different meaning, which is the
    '' shape of change that quietly breaks callers -- see the drain the self-test needs.
    return true
end function


'' ---------------------------------------------------------------------------------------
'' THE PARSE ITSELF, which is all the worker does. No gSymDb, no panel, no document -- it
'' takes bytes and a name and hands back a result set.
''
'' STATIC-FREE AND UI-FREE ON PURPOSE: everything it touches is either a parameter or the
'' two counters at the top of this file, and those are written here and read by the suite
'' after a join or a drain.
'' ---------------------------------------------------------------------------------------
private function ShellScan_RunParse( byval nTier as long, _
                                     byref sText as string, _
                                     byval wszRoot as DWSTRING, _
                                     byval wszDir as DWSTRING ) as PARSERESULTSET ptr
    dim as FBCP_OPTIONS opts
    opts.version = FBCP_VERSION
    opts.includeCount = 0

    '' The project tier's one include path -- the root file's own directory. See
    '' ShellScan_Project for why the other two entries tiko adds are absent.
    dim as wstring * MAX_PATH wszPath0
    dim as wstring ptr pPaths(0 to 0)
    if (nTier = ScanTierProject) andalso (PsLen(wszDir) > 0) then
        wszPath0 = *wszDir.Wz()
        pPaths(0) = @wszPath0
        opts.includeCount = 1
        opts.includePaths = @pPaths(0)
    end if

    dim as double t0 = timer
    dim as FBCP_RESULT ptr pRes = 0
    dim as long rcScan
    if nTier = ScanTierBuffer then
        rcScan = fbcparser_scan_text( strptr(sText), wszRoot.Wz(), @opts, @pRes )
    else
        rcScan = fbcparser_scan( wszRoot.Wz(), @opts, @pRes )
    end if

    dim as PARSERESULTSET ptr pRSet = new PARSERESULTSET
    pRSet->pResult     = pRes
    pRSet->tier        = nTier
    pRSet->scanRc      = rcScan
    pRSet->scanMs      = clng( (timer - t0) * 1000 )
    pRSet->wszRootFile = *wszRoot.Wz()

    '' BUILT ON THE WORKER, which is where tiko builds them and says why: "off the UI
    '' thread". Step 5 had to do it on the UI thread and noted the cost; this is that note
    '' being paid off.
    pRSet->BuildIndexes()
    return pRSet
end function



'' ========================================================================================
'' WHY THERE IS ONE WORKER AND NOT TWO -- ANSWERED 7c STEP 13, AND THE QUESTION WAS WRONG.
''
'' The handoff carried "whether two tiers deserve two workers" as an open decision for six
'' steps, on the observation that a buffer scan and a project scan serialise here. It is not
'' a scheduling decision. THE PARSER CANNOT BE ENTERED TWICE:
''
''     fbcParser.bi:166-169
''     "THREADING CONTRACT: the engine is a single global compiler instance. Exactly one scan
''      may run at a time, and all calls must come from the same thread (serialize scans on
''      one worker thread; a second concurrent call returns FBCP_E_BUSY as a safety net, not
''      as a synchronization mechanism)."
''
'' fbcParser is a fork of the fbc front end and carries module-level state in symb.bas,
'' fb.bas and lex.bas. A second worker would either block on the same global instance --
'' buying nothing -- or corrupt it. The same-thread half of that contract is already visible
'' from here: it is why the retire queue exists, in both binaries.
''
'' SO THE DECISION IS PARSER REENTRANCY, WHICH IS A COMPILER FRONT-END REWRITE, and nothing
'' about the queue above changes it. Buffer-before-project stays the right priority: the
'' buffer tier is what the user is looking at.
''
'' The "1.3s + 1.3s on tiko.bas" figure this was argued from is docs/port/7c-step7.md's,
'' measured then. It is also beside the point -- whatever the number is, a second worker
'' cannot overlap two scans.
'' ========================================================================================

'' ---------------------------------------------------------------------------------------
'' THE WORKER LOOP. Mirrors clsScanMgr.WorkerLoop (clsScanMgr.inc:395) with the Win32 event
'' objects replaced by one condition, and the two tiers kept in the same latest-wins slots.
'' ---------------------------------------------------------------------------------------
private sub ShellScan_Worker( byval user as any ptr )
    do
        dim as long nTier = -1
        dim as string sText
        dim as DWSTRING wszRoot, wszDir

        PsMutexLock( g_scanMtx )

        '' ---- WAIT IN A LOOP, NOT AN `if`. A condition wait may return without a signal,
        '' and re-testing the predicate is also what makes a lost wakeup survivable.
        do while (g_bScanQuit = false) andalso _
                 (g_reqBuf.bValid = false) andalso (g_reqProj.bValid = false) andalso _
                 (g_nRetire = 0)
            PsCondWait( g_scanCond, g_scanMtx )
        loop

        '' ---- THE RETIRE QUEUE, DRAINED HERE AND NOWHERE ELSE.
        '' fbcparser_free has a same-thread contract: the sets below were displaced by
        '' InstallSet on the UI thread, and this is the thread that allocated them.
        for i as long = 0 to g_nRetire - 1
            if g_retire(i) <> 0 then
                if g_retire(i)->pResult then fbcparser_free( g_retire(i)->pResult )
                delete g_retire(i)
                g_retire(i) = 0
            end if
        next
        g_nRetire = 0

        if g_bScanQuit then
            PsMutexUnlock( g_scanMtx )
            exit do
        end if

        '' BUFFER BEFORE PROJECT, which is tiko's order too: the buffer tier is what the
        '' user is looking at, and a project scan can take a second.
        if g_reqBuf.bValid then
            nTier   = ScanTierBuffer
            sText   = g_reqBuf.sText
            wszRoot = g_reqBuf.wszRoot
            g_reqBuf.bValid = false
            g_reqBuf.sText  = ""        '' released here, not held until the next request
        elseif g_reqProj.bValid then
            nTier   = ScanTierProject
            wszRoot = g_reqProj.wszRoot
            wszDir  = g_reqProj.wszDir
            g_reqProj.bValid = false
        end if

        PsMutexUnlock( g_scanMtx )
        if nTier < 0 then continue do

        '' ---- OUTSIDE THE LOCK. This is the 1.2 seconds, and the whole point is that
        '' nothing else waits on it.
        dim as PARSERESULTSET ptr pRSet = ShellScan_RunParse( nTier, sText, wszRoot, wszDir )

        '' ---- PUBLISH. A result the UI has not collected yet is replaced, and the old one
        '' is freed RIGHT HERE -- same thread, same contract, and it is ours because the UI
        '' never saw it.
        PsMutexLock( g_scanMtx )
        if nTier = ScanTierBuffer then
            if g_doneBuf <> 0 then
                if g_doneBuf->pResult then fbcparser_free( g_doneBuf->pResult )
                delete g_doneBuf
            end if
            g_doneBuf = pRSet
        else
            if g_doneProj <> 0 then
                if g_doneProj->pResult then fbcparser_free( g_doneProj->pResult )
                delete g_doneProj
            end if
            g_doneProj = pRSet
        end if
        PsMutexUnlock( g_scanMtx )

        '' THE SANCTIONED CHANNEL BACK. g_plat.events.Post is thread-safe and wakes a
        '' blocked pump -- PsSdl3.inc:774 calls it "the direct analogue of the existing
        '' CreateThread + PostMessage idiom", which is precisely what this replaces.
        g_plat.events.Post( SH_USER_SCAN_DONE, 0, 0 )
    loop
end sub


'' ---------------------------------------------------------------------------------------
'' THE UI SIDE OF A FINISHED SCAN. Called from the pump's PSEV_USER arm, and by the
'' self-test's drain. Returns TRUE if anything was installed.
''
'' EVERYTHING HERE IS ON THE UI THREAD: gSymDb, the panel, the documents. The worker's only
'' contribution is a pointer handed over under the lock.
'' ---------------------------------------------------------------------------------------
function ShellScan_Collect() as boolean
    dim as PARSERESULTSET ptr pBuf = 0, pProj = 0

    PsMutexLock( g_scanMtx )
    pBuf = g_doneBuf  : g_doneBuf  = 0
    pProj = g_doneProj : g_doneProj = 0
    PsMutexUnlock( g_scanMtx )

    if (pBuf = 0) andalso (pProj = 0) then return false

    dim as boolean bInstalled = false
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet = iif( pass = 0, pBuf, pProj )
        if pRSet = 0 then continue for

        '' ---- THE STALE-ROOT TEST, BACK BECAUSE THE RESULT CAN NOW ARRIVE LATE.
        '' A buffer scan started before a tab switch lands after it, and installing it would
        '' put the PREVIOUS document's symbols in the database while the user looks at
        '' another file. tiko discards the same way (frmMain.inc:1713-1737).
        dim as boolean bStale = false
        if pRSet->tier = ScanTierBuffer then
            dim as clsDocument ptr pActive = ShellTabs_CurrentDoc()
            if pActive = 0 then
                bStale = true
            else
                bStale = (PsUCase(DWSTRING(pRSet->wszRootFile)) <> _
                          PsUCase(ShellScan_RootName(pActive)))
            end if
        end if

        if bStale then
            print "tikoshell: discarding a stale " & _
                  iif(pRSet->tier = ScanTierBuffer, "buffer", "project") & " scan of " & _
                  PsPathName( pRSet->wszRootFile ).Utf8
            ShellScan_Retire( pRSet )
            continue for
        end if

        if pRSet->tier = ScanTierBuffer then
            g_nLastScanMs = pRSet->scanMs
            g_nScanCount += 1
        else
            g_nLastProjMs = pRSet->scanMs
            g_nProjCount += 1
        end if

        print "tikoshell: " & iif(pRSet->tier = ScanTierBuffer, "scanned ", "project scan ") & _
              PsPathName( pRSet->wszRootFile ).Utf8 & _
              " -- rc=" & pRSet->scanRc & " ms=" & pRSet->scanMs & _
              iif( pRSet->pResult <> 0, _
                   " files=" & str(pRSet->pResult->fileCount) & _
                   " symbols=" & str(pRSet->pResult->symbolCount), " (no result)" )

        '' InstallSet hands back what it displaced, and THE WORKER OWES ITS FREE -- see
        '' ShellScan_Retire.
        dim as PARSERESULTSET ptr pOld = gSymDb.InstallSet( pRSet->tier, pRSet )
        if pOld then ShellScan_Retire( pOld )
        bInstalled = true
    next

    '' ---- AND THE PANEL FOLLOWS, which is what makes the Functions list LIVE.
    '' tiko does the same immediately after InstallSet -- "a fresh scan can add/remove files
    '' and procs: refresh the Functions panel when it is showing" (frmMain.inc:1750). Only
    '' when it IS showing: rebuilding a bookmarks list because a parse finished would be
    '' work for a list the parse cannot have changed.
    if bInstalled andalso (g_panelMode = SHPANEL_FUNCTIONS) then ShellPanel_Reload()
    return bInstalled
end function


'' ========================================================================================
'' THE PROJECT TIER -- the port of clsScanMgr.RequestProjectScan, and what makes the
'' Functions panel able to show more than one file.
''
'' ---- WHY IT EXISTS, WHICH IS NOT "MORE SYMBOLS FOR THEIR OWN SAKE" --------------------
''
'' gSymDb's BUFFER tier holds exactly ONE result set and InstallSet replaces, so until this
'' commit the panel could only ever list the file the user was looking at -- it rescans on
'' every tab switch to keep that true. The PROJECT tier is a second slot, filled from DISK
'' by following #includes, and clsSymbolDb already merges the two: RecomputeContrib
'' (clsSymbolDb.inc:290-317) SUPPRESSES the project tier for whichever file the buffer is
'' rooted at, so the active file keeps its live-as-you-type symbols and every other file
'' comes from here.
''
'' THE PANEL NEEDED NO CHANGE AT ALL. EnumProcsInFile has always searched both tiers.
''
'' ---- THE ROOT AND THE INCLUDE PATHS ARE tiko's RULE, DEGRADED HONESTLY ----------------
''
'' tiko roots at gApp.GetMainDocumentPtr() (clsScanMgr.inc:311) and builds include paths
'' from three things (clsScanMgr.inc:265-279): the root file's own directory,
'' gConfig.CompilerIncludes, and the configured compiler's `inc` directory.
''
'' THE LAST TWO ARE EMPTY HERE, AND THAT IS CORRECT RATHER THAN MISSING. This binary never
'' loads settings.ini, so it has no configured compiler and no user include paths -- which
'' is exactly the state tiko is in for a loose file outside a project. The graph is
'' therefore the root file plus whatever resolves beside it, and the report says so rather
'' than implying a whole-tree scan.
''
'' A ROOT IS NOT ALWAYS AVAILABLE: GetMainDocumentPtr answers null with nothing open, or
'' with nothing that is a .bas. tiko exits the request; so does this, and the pane then
'' behaves exactly as it did before the tier existed.
''
'' ---- FROM DISK, WHICH HAS A CONSEQUENCE WORTH NAMING ---------------------------------
''
'' fbcparser_scan reads FILES; fbcparser_scan_text reads a buffer. So a background tab with
'' UNSAVED edits contributes its LAST SAVED symbols here. tiko has the same property and
'' reconciles it the same way -- by rescanning the project after a save.
'' ========================================================================================
function ShellScan_Project() as boolean
    dim as clsDocument ptr pRoot = gApp.GetMainDocumentPtr()
    if pRoot = 0 then return false

    dim as DWSTRING wszRoot = pRoot->DiskFilename
    '' NEVER SAVED MEANS NOTHING ON DISK TO SCAN -- tiko's own guard, and the reason it
    '' tests for a backslash rather than for emptiness: an untitled document's name is
    '' "Untitled1", which is a perfectly good string and a perfectly bad path.
    if PsInStr( wszRoot, "\" ) = 0 then return false

    '' ONE INCLUDE PATH: the root's own directory. See the header for why the other two
    '' entries tiko adds are absent. It is resolved HERE rather than on the worker because
    '' it reads gApp, and gApp belongs to the UI thread.
    PsMutexLock( g_scanMtx )
    g_reqProj.bValid  = true
    g_reqProj.wszRoot = wszRoot
    g_reqProj.wszDir  = PsPathDirWithSep( wszRoot )
    PsCondSignal( g_scanCond )
    PsMutexUnlock( g_scanMtx )

    '' Again: TRUE means "asked for". The result arrives through PSEV_USER.
    return true
end function


'' ========================================================================================
'' THE WORKER'S LIFETIME.
''
'' A THREAD STILL RUNNING AT PROCESS EXIT IS HOW A CLEAN QUIT BECOMES AN INTERMITTENT
'' CRASH -- it reads globals the runtime is tearing down, and it does so on a schedule
'' nobody controls. tiko sets an exit event and waits 15 seconds for the worker to
'' acknowledge (clsScanMgr.inc:531-537); this sets a flag, broadcasts, and JOINS, which is
'' the same contract without the timeout because the worker's only blocking wait is the
'' condition it is being woken from.
'' ========================================================================================
sub ShellScan_StartWorker()
    if g_bScanStarted then exit sub
    if PsMutexCreate( g_scanMtx ) = false then
        print "tikoshell: no mutex -- the scanner will not start"
        exit sub
    end if
    if PsCondCreate( g_scanCond ) = false then
        print "tikoshell: no condition -- the scanner will not start"
        exit sub
    end if
    g_bScanQuit = false
    if PsThreadStart( g_scanThread, @ShellScan_Worker, 0 ) = false then
        print "tikoshell: the scan worker would not start"
        exit sub
    end if
    g_bScanStarted = true
end sub


sub ShellScan_StopWorker()
    if g_bScanStarted = false then exit sub

    '' THE FLAG UNDER THE LOCK, THEN THE WAKE. The other order is the lost-wakeup bug: the
    '' worker is woken, re-tests a flag that is not set yet, and goes back to sleep -- and
    '' the join below then waits forever on a thread that will never look again.
    PsMutexLock( g_scanMtx )
    g_bScanQuit = true
    PsCondBroadcast( g_scanCond )
    PsMutexUnlock( g_scanMtx )

    PsThreadJoin( g_scanThread )
    g_bScanStarted = false

    '' AFTER THE JOIN, so nothing is racing for these. Anything still in a slot never
    '' reached the UI, so freeing it here is this thread's to do -- the worker is gone.
    PsMutexLock( g_scanMtx )
    for i as long = 0 to g_nRetire - 1
        if g_retire(i) <> 0 then
            if g_retire(i)->pResult then fbcparser_free( g_retire(i)->pResult )
            delete g_retire(i)
            g_retire(i) = 0
        end if
    next
    g_nRetire = 0
    if g_doneBuf <> 0 then
        if g_doneBuf->pResult then fbcparser_free( g_doneBuf->pResult )
        delete g_doneBuf : g_doneBuf = 0
    end if
    if g_doneProj <> 0 then
        if g_doneProj->pResult then fbcparser_free( g_doneProj->pResult )
        delete g_doneProj : g_doneProj = 0
    end if
    PsMutexUnlock( g_scanMtx )

    PsCondDestroy( g_scanCond )
    PsMutexDestroy( g_scanMtx )
end sub


'' ---------------------------------------------------------------------------------------
'' A BOUNDED WAIT FOR THE WORKER TO CATCH UP. FOR THE SELF-TEST, which has no pump to
'' deliver PSEV_USER to.
''
'' THIS IS NOT "SLEEP AND HOPE". It waits for a CONDITION -- a result present, or nothing
'' outstanding -- with a deadline, so a slow machine waits longer and still passes, and a
'' broken worker fails in bounded time rather than hanging the suite. The sleep inside the
'' loop is 1ms and exists only so the wait does not spin a core.
''
'' Returns TRUE if something was installed.
'' ---------------------------------------------------------------------------------------
function ShellScan_DrainFor( byval nMaxMs as long ) as boolean
    dim as double tEnd = timer + (nMaxMs / 1000.0)
    dim as boolean bAny = false

    do
        if ShellScan_Collect() then
            bAny = true
            '' Keep going: a buffer and a project result can be outstanding together, and
            '' the second may not have been published when the first was collected.
        end if

        dim as boolean bPending
        PsMutexLock( g_scanMtx )
        bPending = g_reqBuf.bValid orelse g_reqProj.bValid orelse _
                   (g_doneBuf <> 0) orelse (g_doneProj <> 0)
        PsMutexUnlock( g_scanMtx )
        if bPending = false then
            '' NOTHING QUEUED AND NOTHING WAITING -- but the worker may be MID-PARSE, which
            '' neither flag shows. One more short wait and one more collect is what covers
            '' that gap; a longer answer would need the worker to publish "busy", and a busy
            '' flag read without the result is a race of its own.
            if bAny then return true
        end if

        sleep 1, 1
    loop while timer < tEnd

    '' One last look, because the loop may have expired between a publish and a collect.
    if ShellScan_Collect() then bAny = true
    return bAny
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
