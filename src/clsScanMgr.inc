' ========================================================================================
' clsScanMgr implementation. See clsScanMgr.bi for the threading model.
'
' Locking rules: the critical section guards ONLY the mailbox, done and retire
' slots. Scans run outside the lock. Consumers never see a PARSERESULTSET that
' the worker still owns: sets cross threads exclusively through TakeCompleted
' (worker -> UI) and RetireResult (UI -> worker).
' ========================================================================================

destructor SCANREQUEST()
    if this.pszText then deallocate( this.pszText )
    this.pszText = 0
end destructor


' The ThreadCreate entry point - immediately delegates to the member proc.
private sub ScanMgr_ThreadProc( byval pParam as any ptr )
    dim as clsScanMgr ptr pMgr = cast( clsScanMgr ptr, pParam )
    pMgr->WorkerLoop()
end sub


' Root filename a buffer scan attributes symbols to. Real files use their
' absolute path (also anchors same-directory #include resolution). Unsaved
' documents get a synthetic per-document name so Phase 2's merge can key on
' it consistently for the life of the document. (Public: declared in the .bi -
' DereferenceLine, F12 and the TODO store use it as the document's DB identity.)
function ScanMgr_GetRootName( byval pDoc as clsDocument ptr ) as DWSTRING
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsInStr( wszFile, "\" ) > 0 then return wszFile
    return PsExePath & "UNTITLED-" & hex( cast( ulong_ptr, pDoc ) ) & ".BAS"
end function


' Heuristic used for BOM-less disk files: no high bytes -> ANSI and UTF-8 agree
' (returns false, the plain conversion is used); high bytes forming valid UTF-8
' sequences -> true; malformed sequences -> false (a legacy ANSI file).
private function ScanMgr_LooksUtf8( byval pB as ubyte ptr, byval cb as long ) as boolean
    dim as boolean bHasHigh = false
    dim as long i = 0
    while i < cb
        dim as long b = pB[i]
        if b < &h80 then
            i += 1
        else
            bHasHigh = true
            dim as long nCont = 0
            if (b and &hE0) = &hC0 then
                nCont = 1
            elseif (b and &hF0) = &hE0 then
                nCont = 2
            elseif (b and &hF8) = &hF0 then
                nCont = 3
            else
                return false
            end if
            for j as long = 1 to nCont
                if i + j >= cb then return false
                if (pB[i + j] and &hC0) <> &h80 then return false
            next
            i += nCont + 1
        end if
    wend
    return bHasHigh
end function


' Scan a byte buffer for 'TODO: comment lines and APPEND them to the result
' set's todoItems()/todoCount, stamped with wszFile. This is modParser's match
' rule, lifted before its deletion: an apostrophe comment whose text - after
' optional spaces - begins with "todo:" case-insensitively (no spaces inside
' the word). Only the FIRST apostrophe outside a string literal counts,
' matching the old tokenizer which only ever tested the start of a comment.
' Line numbers are 1-based. bUtf8 selects the byte->wide conversion of the
' captured text (DWSTRING.utf8 vs plain ANSI assign). Worker thread.
private sub ScanMgr_ScanTodoBytes( _
            byval pRSet as PARSERESULTSET ptr, _
            byval pB as ubyte ptr, _
            byval cb as long, _
            byval wszFile as DWSTRING, _
            byval bUtf8 as boolean _
            )

    if (pB = 0) orelse (cb <= 0) then exit sub

    dim as long cchText = cb
    dim as long i = 0
    dim as long nLineNum = 1
    dim as boolean bInString = false
    dim as boolean bLineDone = false     ' a comment was already seen on this line

    while i < cchText
        dim as long ch = pB[i]

        if ch = 10 then                  ' LF: next line (CRLF counts once)
            nLineNum += 1
            bInString = false
            bLineDone = false
        elseif bLineDone then
            ' skip the rest of the line
        elseif ch = 34 then              ' double quote toggles string mode
            bInString = not bInString
        elseif (ch = 39) andalso (bInString = false) then
            bLineDone = true
            ' skip spaces/tabs between the apostrophe and the marker
            dim as long j = i + 1
            while j < cchText
                dim as long cs = pB[j]
                if (cs <> 32) andalso (cs <> 9) then exit while
                j += 1
            wend
            ' "todo:" ?
            dim as boolean bMatch = false
            if j + 4 < cchText then
                dim as string sTag = lcase( chr( pB[j], pB[j+1], pB[j+2], pB[j+3], pB[j+4] ) )
                if sTag = "todo:" then bMatch = true
            end if
            if bMatch then
                ' capture the remainder of the line
                dim as long k = j + 5
                dim as long e = k
                while e < cchText
                    dim as long ce = pB[e]
                    if (ce = 13) orelse (ce = 10) then exit while
                    e += 1
                wend
                dim as string sText
                for x as long = k to e - 1
                    sText &= chr( pB[x] )
                next
                sText = trim( sText, any chr(32, 9) )

                if pRSet->todoCount > ubound( pRSet->todoItems ) then
                    redim preserve pRSet->todoItems( 0 to pRSet->todoCount * 2 + 7 )
                end if
                with pRSet->todoItems( pRSet->todoCount )
                    .wszFile = wszFile
                    .nLineNum = nLineNum
                    ' byte->wide: UTF-8 documents/files must convert through
                    ' the DWSTRING utf8 property or non-latin text garbles
                    if bUtf8 then
                        .wszText.utf8 = sText
                    else
                        .wszText = sText
                    end if
                end with
                pRSet->todoCount += 1
            end if
        end if
        i += 1
    wend
end sub


' Project tier: fbcParser read everything from disk, so no text copy exists.
' Re-read every USER file the scan touched (skipping the toolchain include
' folder) and line-scan it, so the TODO panel is project-wide instead of
' active-file-only (author requirement, 2026-07-19). UTF-16 files are skipped -
' the editor converts those to UTF-8 in-buffer, so their TODOs appear once the
' document is opened and buffer-scanned. Worker thread, ~230 small files per
' project scan; the read cost is dwarfed by the 3 s symbol scan itself.
private sub ScanMgr_ScanProjectTodos( _
            byval pRSet as PARSERESULTSET ptr, _
            byref wszIncPrefix as const wstring _
            )

    if pRSet->pResult = 0 then exit sub
    dim as long fc = pRSet->pResult->fileCount

    for i as long = 0 to fc - 1
        dim as wstring ptr pFile = @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ]
        if len( wszIncPrefix ) > 0 then
            if ucase( left( *pFile, len( wszIncPrefix ) ) ) = wszIncPrefix then continue for
        end if

        ' PsFileReadAll, not `open`: fbc's Open takes the path as an fbc `string` and reads
        ' it in the ANSI codepage, so a source file under a non-ASCII directory was silently
        ' skipped -- the `continue for` below made that indistinguishable from a file that
        ' genuinely could not be read. PsFile uses the wide CRT.
        dim as DWSTRING wszF = *pFile
        dim as boolean bReadOk
        dim as string sBytes = PsFileReadAll( wszF, bReadOk )
        if bReadOk = false then continue for
        dim as long cb = len( sBytes )
        if cb <= 0 then continue for

        dim as ubyte ptr pB = cast( ubyte ptr, strptr( sBytes ) )
        dim as long nStart = 0
        dim as boolean bUtf8 = false
        if cb >= 2 then
            dim as boolean bUtf16 = ((pB[0] = &hFF) andalso (pB[1] = &hFE)) orelse _
                                    ((pB[0] = &hFE) andalso (pB[1] = &hFF))
            if bUtf16 then continue for
        end if
        if cb >= 3 then
            if (pB[0] = &hEF) andalso (pB[1] = &hBB) andalso (pB[2] = &hBF) then
                nStart = 3
                bUtf8 = true
            end if
        end if
        if bUtf8 = false then bUtf8 = ScanMgr_LooksUtf8( pB + nStart, cb - nStart )

        ScanMgr_ScanTodoBytes( pRSet, pB + nStart, cb - nStart, *pFile, bUtf8 )
    next
end sub


' Only FB source files are worth a scan. Unsaved documents (no path yet) are
' assumed to be FB code.
private function ScanMgr_IsScannable( byval pDoc as clsDocument ptr ) as boolean
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsInStr( wszFile, "\" ) = 0 then return true
    dim as string sExt = PsLCase( PsPathExt(wszFile ) ).Utf8
    select case sExt
        case ".bas", ".bi", ".inc"
            return true
    end select
    return false
end function


''
''
sub clsScanMgr.StartWorker()
    if m_bStarted then exit sub

    InitializeCriticalSection( @m_cs )
    m_hWakeEvent = CreateEvent( 0, FALSE, FALSE, 0 )    ' auto-reset
    m_hExitEvent = CreateEvent( 0, TRUE, FALSE, 0 )     ' manual-reset
    redim m_retire( 0 to 15 )
    m_retireCount = 0
    m_bShutdown = false

    m_pThread = ThreadCreate( @ScanMgr_ThreadProc, @this )
    m_bStarted = ( m_pThread <> 0 )

    ' If the thread never started, m_bStarted stays false and Shutdown() returns without
    ' doing anything -- so the critical section and both events above would be leaked for
    ' the life of the process, with no worker to justify them. Undo the setup instead.
    if m_bStarted = false then
        if m_hWakeEvent then CloseHandle( m_hWakeEvent ) : m_hWakeEvent = 0
        if m_hExitEvent then CloseHandle( m_hExitEvent ) : m_hExitEvent = 0
        DeleteCriticalSection( @m_cs )
        erase m_retire
    end if
end sub


' Snapshot everything a scan needs while still on the UI thread (gConfig and
' gApp are never touched from the worker). Include search paths, in order:
' the root file's own directory, the user's configured include directories,
' and the toolchain's inc directory (same derivation modCompile uses).
private function clsScanMgr.BuildRequest( _
            byval nTier as long, _
            byval wszRootFile as DWSTRING _
            ) as SCANREQUEST ptr

    dim as SCANREQUEST ptr pReq = new SCANREQUEST
    pReq->tier = nTier
    pReq->wszRootFile = *(wszRootFile).Wz()

    ' NOTE: config path values may be in {CURDRIVE} placeholder form at any
    ' moment (SaveConfigFile converts the LIVE gConfig values as a side effect
    ' of saving) - always expand via ProcessFromCurdriveApp before use, the
    ' same way modCompile does before every compile.
    dim as DWSTRING wszPaths = PsPathDirWithSep(wszRootFile )

    dim as long numPaths = PsStrParseCount( gConfig.CompilerIncludes, ";" )
    for i as long = 1 to numPaths
        dim as DWSTRING wszP = PsTrim( PsStrParse( gConfig.CompilerIncludes, i, ";" ) )
        wszP = ProcessFromCurdriveApp( PsStrRemove( wszP, chr(34) ) )
        if PsLen( wszP ) then wszPaths = wszPaths & ";" & wszP
    next

    dim as DWSTRING wszCompiler = ProcessFromCurdriveApp( gConfig.FBWINCompiler64 )
    if PsLen( wszCompiler ) then
        wszPaths = wszPaths & ";" & PsPathDirWithSep(wszCompiler ) & "inc"
        pReq->sIncPrefix = PsUCase( PsPathDirWithSep(wszCompiler ) & "inc" )
    end if

    pReq->sIncludePaths = wszPaths
    function = pReq
end function


' Latest-wins: a newer request replaces (and frees) any pending one in its
' tier's slot. The two tiers coexist.
private sub clsScanMgr.EnqueueRequest( byval pReq as SCANREQUEST ptr )
    if pReq = 0 then exit sub
    EnterCriticalSection( @m_cs )
    if pReq->tier = ScanTierBuffer then
        if m_pBufReq then delete m_pBufReq
        m_pBufReq = pReq
    else
        if m_pProjReq then delete m_pProjReq
        m_pProjReq = pReq
    end if
    LeaveCriticalSection( @m_cs )
    SetEvent( m_hWakeEvent )
end sub


''
''
sub clsScanMgr.RequestProjectScan( byval wszRootOverride as DWSTRING = "" )
    if m_bStarted = false then exit sub
    if m_bShutdown then exit sub

    dim as DWSTRING wszFile = wszRootOverride
    if PsLen( wszFile ) = 0 then
        dim as clsDocument ptr pDoc = gApp.GetMainDocumentPtr()
        if pDoc = 0 then exit sub
        wszFile = pDoc->DiskFilename
    end if
    if PsInStr( wszFile, "\" ) = 0 then exit sub    ' never saved - nothing on disk to scan

    this.EnqueueRequest( this.BuildRequest( ScanTierProject, wszFile ) )
end sub


''
''
sub clsScanMgr.RequestBufferScan( byval pDoc as clsDocument ptr )
    if m_bStarted = false then exit sub
    if m_bShutdown then exit sub
    if pDoc = 0 then exit sub
    if ScanMgr_IsScannable( pDoc ) = false then exit sub

    ' Copy the document text NOW, on the UI thread. Scintilla returns the
    ' document bytes (ANSI or UTF-8, both fine for the DLL); documents not
    ' yet realized in a Scintilla window still hold their text in TextBuffer.
    dim as string sText
    if DocView(pDoc, 0) then sText = pDoc->GetText()
    if len( sText ) = 0 then sText = pDoc->TextBuffer
    if len( sText ) = 0 then exit sub

    dim as DWSTRING wszRoot = ScanMgr_GetRootName( pDoc )
    dim as SCANREQUEST ptr pReq = this.BuildRequest( ScanTierBuffer, wszRoot )
    ' ANSI documents keep codepage 0 in Scintilla; everything else is UTF-8
    ' in-buffer - the TODO scanner needs to know which conversion to apply.
    pReq->bTextUtf8 = ( pDoc->FileEncoding <> FILE_ENCODING_ANSI )
    pReq->cchText = len( sText )
    pReq->pszText = allocate( pReq->cchText + 1 )
    if pReq->pszText = 0 then
        delete pReq
        exit sub
    end if
    *pReq->pszText = sText

    this.EnqueueRequest( pReq )
end sub


''
''
function clsScanMgr.TakeCompleted( byval nTier as long ) as PARSERESULTSET ptr
    if m_bStarted = false then return 0
    dim as PARSERESULTSET ptr pRSet = 0
    EnterCriticalSection( @m_cs )
    if nTier = ScanTierBuffer then
        pRSet = m_pBufDone
        m_pBufDone = 0
    else
        pRSet = m_pProjDone
        m_pProjDone = 0
    end if
    LeaveCriticalSection( @m_cs )
    function = pRSet
end function


' Append to the retire queue. Caller must hold the critical section.
private sub clsScanMgr.RetireLocked( byval pRSet as PARSERESULTSET ptr )
    if m_retireCount > ubound( m_retire ) then
        redim preserve m_retire( 0 to m_retireCount * 2 + 7 )
    end if
    m_retire( m_retireCount ) = pRSet
    m_retireCount += 1
end sub


''
''
sub clsScanMgr.RetireResult( byval pRSet as PARSERESULTSET ptr )
    if pRSet = 0 then exit sub
    EnterCriticalSection( @m_cs )
    this.RetireLocked( pRSet )
    LeaveCriticalSection( @m_cs )
    SetEvent( m_hWakeEvent )
end sub



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

''
''
sub clsScanMgr.WorkerLoop()
    do
        WaitForSingleObject( m_hWakeEvent, INFINITE )

        do
            ' ---- under the lock: free retired sets, take one request ----
            dim as SCANREQUEST ptr pReq = 0
            dim as boolean bStop

            EnterCriticalSection( @m_cs )
            for i as long = 0 to m_retireCount - 1
                fbcparser_free( m_retire(i)->pResult )
                delete m_retire(i)
            next
            m_retireCount = 0
            bStop = m_bShutdown
            if bStop = false then
                if m_pBufReq then
                    pReq = m_pBufReq
                    m_pBufReq = 0
                elseif m_pProjReq then
                    pReq = m_pProjReq
                    m_pProjReq = 0
                end if
            end if
            LeaveCriticalSection( @m_cs )

            if bStop then
                SetEvent( m_hExitEvent )
                exit sub
            end if
            if pReq = 0 then exit do

            ' ---- outside the lock: build options and run the scan ----
            dim as long numPaths = PsStrParseCount( pReq->sIncludePaths, ";" )
            if numPaths < 1 then numPaths = 1
            redim as wstring * MAX_PATH wszPaths( 0 to numPaths - 1 )
            redim as wstring ptr pPaths( 0 to numPaths - 1 )
            dim as long cntPaths = 0
            for i as long = 1 to numPaths
                dim as DWSTRING wszP = PsTrim( PsStrParse( pReq->sIncludePaths, i, ";" ) )
                if PsLen( wszP ) then
                    wszPaths( cntPaths ) = *(wszP).Wz()
                    pPaths( cntPaths ) = @wszPaths( cntPaths )
                    cntPaths += 1
                end if
            next

            dim as FBCP_OPTIONS opts
            opts.version = FBCP_VERSION
            opts.includeCount = cntPaths
            if cntPaths > 0 then opts.includePaths = @pPaths(0)

            dim as double t0 = timer
            dim as FBCP_RESULT ptr pRes = 0
            dim as long rcScan
            if pReq->tier = ScanTierBuffer then
                rcScan = fbcparser_scan_text( pReq->pszText, @pReq->wszRootFile, @opts, @pRes )
            else
                rcScan = fbcparser_scan( @pReq->wszRootFile, @opts, @pRes )
            end if

            dim as PARSERESULTSET ptr pRSet = new PARSERESULTSET
            pRSet->pResult = pRes
            pRSet->tier = pReq->tier
            pRSet->scanRc = rcScan
            pRSet->scanMs = clng( (timer - t0) * 1000 )
            pRSet->wszRootFile = pReq->wszRootFile

            ' TODO line-scan (fbcParser itself never sees comments): the buffer
            ' tier scans the same text copy the symbol scan consumed; the
            ' project tier re-reads its user files from disk (project-wide
            ' TODO coverage - author requirement).
            if pReq->tier = ScanTierBuffer then
                ScanMgr_ScanTodoBytes( pRSet, cast( ubyte ptr, pReq->pszText ), _
                                       pReq->cchText, pReq->wszRootFile, pReq->bTextUtf8 )
            else
                ScanMgr_ScanProjectTodos( pRSet, *pReq->sIncPrefix.Wz() )
            end if
            delete pReq

            ' Build the query indexes here on the worker (off the UI thread).
            pRSet->BuildIndexes()

            ' ---- under the lock: publish into the done slot ----
            EnterCriticalSection( @m_cs )
            if pRSet->tier = ScanTierBuffer then
                ' the UI never claimed the previous set - we are the worker,
                ' so freeing it right here honors the same-thread contract
                if m_pBufDone then
                    fbcparser_free( m_pBufDone->pResult )
                    delete m_pBufDone
                end if
                m_pBufDone = pRSet
            else
                if m_pProjDone then
                    fbcparser_free( m_pProjDone->pResult )
                    delete m_pProjDone
                end if
                m_pProjDone = pRSet
            end if
            LeaveCriticalSection( @m_cs )

            PostMessage( HWND_FRMMAIN, MSG_USER_PARSE_COMPLETE, pRSet->tier, 0 )
        loop
    loop
end sub


' Bounded shutdown handshake. Call on the UI thread after the user has
' committed to exit, BEFORE DestroyWindow: pending requests are dropped, all
' live sets are routed to the retire queue, and the worker drains it before
' exiting. A scan in flight (~3 s for a full project scan) finishes first;
' scans cannot hang (fbcParser hardening), but the join is bounded anyway.
sub clsScanMgr.Shutdown()
    if m_bStarted = false then exit sub

    EnterCriticalSection( @m_cs )
    m_bShutdown = true
    if m_pBufReq then
        delete m_pBufReq
        m_pBufReq = 0
    end if
    if m_pProjReq then
        delete m_pProjReq
        m_pProjReq = 0
    end if
    if m_pBufDone then
        this.RetireLocked( m_pBufDone )
        m_pBufDone = 0
    end if
    if m_pProjDone then
        this.RetireLocked( m_pProjDone )
        m_pProjDone = 0
    end if
    LeaveCriticalSection( @m_cs )
    SetEvent( m_hWakeEvent )

    if WaitForSingleObject( m_hExitEvent, 15000 ) = WAIT_OBJECT_0 then
        ThreadWait( m_pThread )
        CloseHandle( m_hWakeEvent )
        CloseHandle( m_hExitEvent )
        DeleteCriticalSection( @m_cs )
    ' else: worker still busy after the bound - abandon the handle and skip
    ' all cleanup; the process is exiting and must not free objects the
    ' thread may still touch (fbc 1.10 has no ThreadDetach)
    end if
    m_pThread = 0
    m_bStarted = false
end sub
