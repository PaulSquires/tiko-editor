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
