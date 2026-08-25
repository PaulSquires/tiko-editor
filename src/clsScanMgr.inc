' ========================================================================================
' clsScanMgr implementation. See clsScanMgr.bi for the threading model.
'
' Locking rules: the critical section guards ONLY the mailbox, done and retire
' slots. Scans run outside the lock. Consumers never see a PARSERESULTSET that
' the worker still owns: sets cross threads exclusively through TakeCompleted
' (worker -> UI) and RetireResult (UI -> worker).
' ========================================================================================

' For frmExplorer_CatIndexForDoc - the workspace scan mirrors the Explorer's display rule
' rather than restating it. Already pulled in ahead of us by clsApp.inc; stated here so the
' dependency is visible at the use site.
#include once "frmExplorer.bi"

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
    if instr( wszFile, "\" ) > 0 then return wszFile
    return AfxGetExePathName & "UNTITLED-" & hex( cast( ulong_ptr, pDoc ) ) & ".BAS"
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
' set's todoItems list, stamped with wszFile. This is modParser's match
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
            byref wszFile as const wstring, _
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
                ' nTodoStart/nTodoEnd, not k/e. Both are on RFC-0004 s5's forbidden list: FB
                ' identifiers are case-insensitive and a generic type parameter resolves
                ' against the instantiation site's scope, so a local named k or e near a
                ' container breaks the instantiation with an error naming fb/map.bi or
                ' fb/result.bi and never this file. PARSERESULTSET is about to carry one.
                dim as long nTodoStart = j + 5
                dim as long nTodoEnd = nTodoStart
                while nTodoEnd < cchText
                    dim as long ce = pB[nTodoEnd]
                    if (ce = 13) orelse (ce = 10) then exit while
                    nTodoEnd += 1
                wend
                dim as string sText
                for x as long = nTodoStart to nTodoEnd - 1
                    sText &= chr( pB[x] )
                next
                sText = trim( sText, any chr(32, 9) )

                dim as TODOITEM todo
                with todo
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
                pRSet->todoItems.Push( todo )
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

        dim as DWSTRING wszF = *pFile
        ' nFileNum, not f -- same rule as nTodoStart/nTodoEnd above.
        dim as long nFileNum = freefile
        if open( wszF for binary access read as #nFileNum ) <> 0 then continue for
        dim as long cb = lof( nFileNum )
        if cb <= 0 then
            close #nFileNum
            continue for
        end if
        dim as string sBytes = space( cb )
        get #nFileNum, 1, sBytes
        close #nFileNum

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
    if instr( wszFile, "\" ) = 0 then return true
    dim as string sExt = lcase( AfxStrPathname( "EXTN", wszFile ) )
    select case sExt
        case ".bas", ".bi", ".inc"
            return true
    end select
    return false
end function


' The root name a WORKSPACE scan is attributed to. Not a real file: it names the synthetic
' include-only root built below. Under the exe folder for the same reason ScanMgr_GetRootName
' puts its UNTITLED- names there - the parser resolves a plain #include against the root's
' own directory, and that has to be a directory that exists.
function ScanMgr_WorkspaceRootName() as DWSTRING
    function = AfxGetExePathName & "WORKSPACE.BAS"
end function


' Does this document contribute to the WORKSPACE scan?
'
' Membership is frmExplorer_CatIndexForDoc, not merely "is in pDocList": a document whose
' category is outside the displayed range (CATINDEX_FILES) is rendered nowhere, and "what
' the Explorer shows" is exactly what this scan is meant to cover. Unsaved documents are
' skipped - there is nothing on disk to include, and their text reaches the database
' through the buffer tier anyway.
private function ScanMgr_IsWorkspaceFile( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false
    if frmExplorer_CatIndexForDoc( pDoc ) < 0 then return false
    if instr( pDoc->DiskFilename, "\" ) = 0 then return false
    return ScanMgr_IsScannable( pDoc )
end function


' Would a workspace scan have anything to scan? Asked by frmUnusedSymbols before it commits
' to waiting on one: with nothing to include, RequestProjectScan files no request, and a
' caller that expected a scan would wait for a completion that never posts.
function ScanMgr_WorkspaceHasFiles() as boolean
    dim as clsDocument ptr pDoc = gApp.pDocList
    do until pDoc = 0
        if ScanMgr_IsWorkspaceFile( pDoc ) then return true
        pDoc = pDoc->pDocNext
    loop
    return false
end function


' Build the WORKSPACE scan's root text: '#include once' for every file the Explorer
' displays, so that a workspace with no Main still gets all of its files parsed instead of
' nothing at all. Returns "" when nothing qualifies (the caller then files no request).
'
' ONCE, not plain #include: a file already reached through another file's includes must not
' be parsed twice, and with several .bas files in one root that is the normal case.
'
' sOutDirs collects each contributing file's own directory as a ';' list for the request's
' include paths. Without it a loose file's own #include "sibling.bi" cannot resolve: the
' synthetic root lives in the exe folder, so the root-directory search that normally
' covers this finds nothing.
private function ScanMgr_BuildWorkspaceRootText( byref sOutDirs as DWSTRING ) as string
    dim as DWSTRING wszText, wszDirs

    dim as clsDocument ptr pDoc = gApp.pDocList
    do until pDoc = 0
        if ScanMgr_IsWorkspaceFile( pDoc ) then
            dim as DWSTRING wszFile = pDoc->DiskFilename
            wszText = wszText & "#include once """ & wszFile & """" & chr(13, 10)
            dim as DWSTRING wszDir = AfxStrPathname( "PATH", wszFile )
            if len( wszDir ) then
                if instr( ucase( wszDirs ), ucase( wszDir ) ) = 0 then
                    wszDirs = wszDirs & wszDir & ";"
                end if
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    if len( wszText ) = 0 then return ""

    sOutDirs = wszDirs

    ' To UTF-8: a path can hold any character, and the DLL takes bytes. bTextUtf8 on the
    ' request says which conversion the TODO scanner would apply - the workspace tier does
    ' not use it (its TODOs are swept from disk), but the flag must still be honest.
    dim as long cbUtf8 = WideCharToMultiByte( CP_UTF8, 0, cast( LPCWSTR, wszText.sptr ), len( wszText ), NULL, 0, NULL, NULL )
    if cbUtf8 <= 0 then return ""
    dim as string sUtf8 = space( cbUtf8 )
    WideCharToMultiByte( CP_UTF8, 0, cast( LPCWSTR, wszText.sptr ), len( wszText ), strptr( sUtf8 ), cbUtf8, NULL, NULL )
    function = sUtf8
end function


''
''
sub clsScanMgr.StartWorker()
    if m_bStarted then exit sub

    InitializeCriticalSection( @m_cs )
    m_hWakeEvent = CreateEvent( 0, FALSE, FALSE, 0 )    ' auto-reset
    m_hExitEvent = CreateEvent( 0, TRUE, FALSE, 0 )     ' manual-reset
    m_retire.Reserve( 16 )
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
        m_retire.Clear( )
    end if
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
        pReq->sIncPrefix = ucase( AfxStrPathname( "PATH", wszCompiler ) & "inc" )
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
sub clsScanMgr.RequestProjectScan( byref wszRootOverride as const wstring = "" )
    if m_bStarted = false then exit sub
    if m_bShutdown then exit sub

    dim as DWSTRING wszFile = wszRootOverride
    if len( wszFile ) = 0 then
        ' NO NAMED PROJECT: scan the WHOLE EXPLORER. A project has a main file whose
        ' includes define the compilation unit, and rooting there is right. A loose
        ' workspace has no such structure - its files are simply the ones the user has
        ' open, related or not - so the only honest root is all of them.
        '
        ' The test is IsProjectNamed, NOT "is there a Main". Every untitled workspace has a
        ' Main: ProjectSetFileType hands FILETYPE_MAIN to the first .bas opened
        ' (Workspace_DeriveFileType, clsApp.inc). Gating on the Main made this whole path
        ' unreachable in practice, and left the other Explorer files unparsed behind a
        ' scan rooted at whichever .bas happened to be opened first.
        if gApp.IsProjectNamed() = false then
            this.RequestWorkspaceScan()
            exit sub
        end if
        dim as clsDocument ptr pDoc = gApp.GetMainDocumentPtr()
        if pDoc = 0 then exit sub          ' named project, no Main - nothing to root at
        wszFile = pDoc->DiskFilename
    end if
    if instr( wszFile, "\" ) = 0 then exit sub    ' never saved - nothing on disk to scan

    this.EnqueueRequest( this.BuildRequest( ScanTierProject, wszFile ) )
end sub


' Project tier over a synthetic root that includes every file the Explorer displays - see
' ScanMgr_BuildWorkspaceRootText. Text-bearing, so the worker takes fbcparser_scan_text:
' the root is not a file and never will be. Everything downstream is unchanged - it is one
' ordinary project-tier PARSERESULTSET, installed in the one project slot.
private sub clsScanMgr.RequestWorkspaceScan()
    dim as DWSTRING wszDirs
    dim as string sText = ScanMgr_BuildWorkspaceRootText( wszDirs )
    if len( sText ) = 0 then exit sub

    dim as SCANREQUEST ptr pReq = this.BuildRequest( ScanTierProject, ScanMgr_WorkspaceRootName() )
    ' The contributing files' own directories, ahead of the configured includes: the
    ' synthetic root sits in the exe folder, so the root-directory search BuildRequest
    ' seeds resolves nothing for a loose file's own #include.
    if len( wszDirs ) then pReq->sIncludePaths = wszDirs & pReq->sIncludePaths

    pReq->bTextUtf8 = true
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
    m_retire.Push( pRSet )
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
            ' Still under the lock, and still one explicit release per entry: the
            ' container owns the pointer array, nothing else. Clear( ) keeps the capacity,
            ' so a steady stream of scans never reallocates this queue.
            for each pRetired in m_retire
                fbcparser_free( pRetired->pResult )
                delete pRetired
            next
            m_retire.Clear( )
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
            ' On pszText, not on the tier: the buffer tier always carries text, and the
            ' project tier does too for a workspace scan, whose root is synthetic and has
            ' no file behind it.
            if pReq->pszText then
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
            ' A text-bearing PROJECT scan is the workspace scan, and its diagnostics are
            ' collisions between files that were never meant to share a root.
            dim as boolean bProjTier = ( pReq->tier = ScanTierProject )
            dim as boolean bHasText  = ( pReq->pszText <> 0 )
            pRSet->bNoDiags = bProjTier andalso bHasText

            ' TODO line-scan (fbcParser itself never sees comments): the buffer
            ' tier scans the same text copy the symbol scan consumed; the
            ' project tier re-reads its user files from disk (project-wide
            ' TODO coverage - author requirement).
            if pReq->tier = ScanTierBuffer then
                ScanMgr_ScanTodoBytes( pRSet, cast( ubyte ptr, pReq->pszText ), _
                                       pReq->cchText, pReq->wszRootFile, pReq->bTextUtf8 )
            else
                ScanMgr_ScanProjectTodos( pRSet, pReq->sIncPrefix )
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
