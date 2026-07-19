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
' it consistently for the life of the document.
private function ScanMgr_GetRootName( byval pDoc as clsDocument ptr ) as DWSTRING
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if instr( wszFile, "\" ) > 0 then return wszFile
    return AfxGetExePathName & "UNTITLED-" & hex( cast( ulong_ptr, pDoc ) ) & ".BAS"
end function


' Only FB source files are worth a scan. Unsaved documents (no path yet) are
' assumed to be FB code.
private function ScanMgr_IsScannable( byval pDoc as clsDocument ptr ) as boolean
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if instr( wszFile, "\" ) = 0 then return true
    dim as string sExt = lcase( AfxStrPathname( "EXTN", wszFile ) )
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
end sub


' Snapshot everything a scan needs while still on the UI thread (gConfig and
' gApp are never touched from the worker). Include search paths, in order:
' the root file's own directory, the user's configured include directories,
' and the toolchain's inc directory (same derivation modCompile uses).
private function clsScanMgr.BuildRequest( _
            byval nTier as long, _
            byref wszRootFile as wstring _
            ) as SCANREQUEST ptr

    dim as SCANREQUEST ptr pReq = new SCANREQUEST
    pReq->tier = nTier
    pReq->wszRootFile = wszRootFile

    ' NOTE: config path values may be in {CURDRIVE} placeholder form at any
    ' moment (SaveConfigFile converts the LIVE gConfig values as a side effect
    ' of saving) - always expand via ProcessFromCurdriveApp before use, the
    ' same way modCompile does before every compile.
    dim as DWSTRING wszPaths = AfxStrPathname( "PATH", wszRootFile )

    dim as long numPaths = AfxStrParseCount( gConfig.CompilerIncludes, ";" )
    for i as long = 1 to numPaths
        dim as DWSTRING wszP = trim( AfxStrParse( gConfig.CompilerIncludes, i, ";" ) )
        wszP = ProcessFromCurdriveApp( AfxStrRemove( wszP, chr(34) ) )
        if len( wszP ) then wszPaths = wszPaths & ";" & wszP
    next

    dim as DWSTRING wszCompiler = ProcessFromCurdriveApp( gConfig.FBWINCompiler64 )
    if len( wszCompiler ) then
        wszPaths = wszPaths & ";" & AfxStrPathname( "PATH", wszCompiler ) & "inc"
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
sub clsScanMgr.RequestProjectScan()
    if m_bStarted = false then exit sub
    if m_bShutdown then exit sub

    dim as clsDocument ptr pDoc = gApp.GetMainDocumentPtr()
    if pDoc = 0 then exit sub
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if instr( wszFile, "\" ) = 0 then exit sub    ' never saved - nothing on disk to scan

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
    if pDoc->hWindow(0) then sText = pDoc->GetText()
    if len( sText ) = 0 then sText = pDoc->TextBuffer
    if len( sText ) = 0 then exit sub

    dim as DWSTRING wszRoot = ScanMgr_GetRootName( pDoc )
    dim as SCANREQUEST ptr pReq = this.BuildRequest( ScanTierBuffer, wszRoot )
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
            dim as long numPaths = AfxStrParseCount( pReq->sIncludePaths, ";" )
            if numPaths < 1 then numPaths = 1
            redim as wstring * MAX_PATH wszPaths( 0 to numPaths - 1 )
            redim as wstring ptr pPaths( 0 to numPaths - 1 )
            dim as long cntPaths = 0
            for i as long = 1 to numPaths
                dim as DWSTRING wszP = trim( AfxStrParse( pReq->sIncludePaths, i, ";" ) )
                if len( wszP ) then
                    wszPaths( cntPaths ) = wszP
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
            delete pReq

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
