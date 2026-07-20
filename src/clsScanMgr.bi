' ========================================================================================
' clsScanMgr - background symbol-scan manager for fbcParser.dll
'
' Owns the single worker thread that performs ALL fbcParser calls (the DLL's
' threading contract: one global compiler instance, one scan at a time, every
' call from the same thread - including the frees). The UI thread only files
' requests and picks up completed results:
'
'   UI thread:  RequestProjectScan / RequestBufferScan   (latest-wins mailbox)
'               TakeCompleted / RetireResult             (from MSG_USER_PARSE_COMPLETE)
'   worker:     drains the retire queue (fbcparser_free), runs one scan,
'               stores the done slot, PostMessage(MSG_USER_PARSE_COMPLETE)
'
' Two scan tiers (see REFACTOR_PLAN.md): the PROJECT scan reads the project's
' main file from disk (refreshed on save / project open); the BUFFER scan
' parses a copy of the active document's live text (debounced while typing).
' ========================================================================================

' SCAN_TIER and PARSERESULTSET are defined in clsSymbolDb.bi (included first) -
' they are symbol-database concepts; the scan manager only produces and shuttles
' PARSERESULTSET instances.

' The root filename a buffer scan attributes symbols to (real files: the absolute
' path; unsaved documents: a synthetic per-document name). Also the key the TODO
' store and DereferenceLine use to identify the active document in the database.
declare function ScanMgr_GetRootName( byval pDoc as clsDocument ptr ) as DWSTRING

' A pending scan request (mailbox slot). Allocated with NEW on the UI thread;
' DELETEd by the worker after the scan, or by the UI thread when a newer
' request supersedes it in its slot. The destructor frees the text copy.
type SCANREQUEST
    tier          as long               ' SCAN_TIER
    pszText       as zstring ptr        ' buffer tier: heap copy of the document text
    cchText       as long
    sIncludePaths as DWSTRING           ' semicolon list snapshotted on the UI thread
    wszRootFile   as wstring * MAX_PATH ' absolute path; buffer tier: the virtual name
    declare destructor()
end type

type clsScanMgr
    private:
        m_cs          as CRITICAL_SECTION
        m_hWakeEvent  as HANDLE          ' auto-reset: work available / shutdown
        m_hExitEvent  as HANDLE          ' manual-reset: worker has exited its loop
        m_pThread     as any ptr         ' ThreadCreate handle
        m_bStarted    as boolean
        m_bShutdown   as boolean
        ' latest-wins mailbox: one pending slot per tier (the tiers coexist;
        ' the worker serves the buffer slot first - it is latency-sensitive)
        m_pProjReq    as SCANREQUEST ptr
        m_pBufReq     as SCANREQUEST ptr
        ' completed sets awaiting UI pickup, one slot per tier (the worker
        ' frees an unclaimed predecessor before overwriting a slot)
        m_pProjDone   as PARSERESULTSET ptr
        m_pBufDone    as PARSERESULTSET ptr
        ' sets the UI has retired, queued for same-thread freeing
        m_retire(any) as PARSERESULTSET ptr
        m_retireCount as long

        declare function BuildRequest( byval nTier as long, byref wszRootFile as wstring ) as SCANREQUEST ptr
        declare sub EnqueueRequest( byval pReq as SCANREQUEST ptr )
        declare sub RetireLocked( byval pRSet as PARSERESULTSET ptr )

    public:
        declare sub StartWorker()                                       ' once, at startup
        declare sub RequestProjectScan()                                ' UI thread only
        declare sub RequestBufferScan( byval pDoc as clsDocument ptr )  ' UI thread only
        declare function TakeCompleted( byval nTier as long ) as PARSERESULTSET ptr
        declare sub RetireResult( byval pRSet as PARSERESULTSET ptr )    ' UI thread only
        declare sub Shutdown()          ' bounded join; call before DestroyWindow
        declare sub WorkerLoop()        ' internal - runs on the worker thread only
end type

dim shared gScanMgr as clsScanMgr
