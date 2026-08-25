' ========================================================================================
' clsSymbolDb - in-memory symbol database over fbcParser results
'
' Replaces the flat clsDB2 array + linear dbSeek. Each completed scan is a
' PARSERESULTSET: the DLL's flat FBCP_RESULT plus thin indexes built over it
' (a case-insensitive name hash, per-file symbol chains, parent->child chains).
' The result memory is never copied - the indexes are just arrays of longs
' pointing into result->symbols[].
'
' gSymDb holds two live sets (see REFACTOR_PLAN.md two-tier design): the PROJECT
' set (main file scanned from disk) and the BUFFER set (active document's live
' text). A query walks the buffer set first, then the project set, honoring the
' merge masks so the active file's fresh symbols shadow the project's stale copy
' and no file is reported twice.
'
' Threading: index build happens on the worker thread (BuildIndexes, right after
' the scan). All READS happen on the UI thread, and the atomic Swap that installs
' a new set also happens on the UI thread - so no reader ever sees a set mutate.
' ========================================================================================

enum SCAN_TIER
    ScanTierProject = 1
    ScanTierBuffer  = 2
end enum

' Kind masks for EnumPrefix (1 shl FBCP_KIND_*). FBCP_KIND max is 13, fits a long.
const SYMMASK_PROC  = (1 shl FBCP_KIND_SUB) or (1 shl FBCP_KIND_FUNCTION)
const SYMMASK_TYPE  = (1 shl FBCP_KIND_TYPE) or (1 shl FBCP_KIND_UNION) or _
                      (1 shl FBCP_KIND_ENUM) or (1 shl FBCP_KIND_TYPEDEF)
const SYMMASK_VAR   = (1 shl FBCP_KIND_VAR) or (1 shl FBCP_KIND_CONST)
const SYMMASK_FIELD = (1 shl FBCP_KIND_FIELD)
const SYMMASK_MEMBER = SYMMASK_FIELD or SYMMASK_PROC   ' members reachable via '.'

' Defensive cap: fbc's error recovery on a mid-edit unterminated TYPE can parent
' a runaway number of declarations into it. Enumerations stop here.
const SYMDB_MAX_ENUM = 8192

' A single 'TODO: comment line. Produced by the scan worker's line scan of each
' buffer-scan text copy (fbcParser itself never sees comments). The flat store
' gTodoItems() below keys entries by file and is mutated only on the UI thread:
' entries are replaced when a buffer scan installs (clsSymbolDb.InstallSet) and
' removed when their document closes (clsApp.RemoveDocument).
type TODOITEM
    wszFile  as DWSTRING          ' owning file, as passed to the scan
    nLineNum as long              ' 1-based source line
    wszText  as DWSTRING          ' text after the TODO: marker
end type

' Aliased because a generic spelled directly in a TYPE field does not instantiate -- see
' PARSERESULTSET.todoItems below and RFC-0004 s1. The alias is declared here so both the
' global store and the per-set field can use it.
type TODOLIST as FB.Array( of TODOITEM )

dim shared gTodoItems as TODOLIST

declare sub TodoStore_ReplaceFile( byref wszFile as const wstring, byref items as TODOLIST )
declare sub TodoStore_RemoveFile( byref wszFile as const wstring )

' One scan's result plus its indexes. Built on the worker, immutable afterward.
' The FBCP_RESULT ptr is owned elsewhere (freed via fbcparser_free on the retire
' queue); the FB dynamic arrays free themselves when the set is deleted.
type PARSERESULTSET
    pResult        as FBCP_RESULT ptr
    tier           as long                 ' SCAN_TIER
    scanRc         as long                 ' fbcparser_scan return code
    scanMs         as long                 ' elapsed scan time
    wszRootFile    as wstring * MAX_PATH   ' root file as passed to the scan
    rootFileIndex  as long                 ' index into fileOffsets[] of the root; -1 if absent

    ' case-insensitive name hash: bucket -> first symbol, symbol -> next in bucket
    hashSize       as long                 ' power of two (mask = hashSize-1); 0 if no symbols
    hashHead(any)  as long
    hashNext(any)  as long
    ' per-file symbol chains: file index -> first symbol, symbol -> next in file
    fileHead(any)  as long
    fileNext(any)  as long
    ' parent -> child chains (declaration order): symbol -> first child, child -> next sibling
    childHead(any) as long
    childNext(any) as long

    ' 'TODO: lines found by the worker's line scan of the buffer-scan text copy
    ' (buffer tier only; empty for project scans - their text is never in memory)
    ' TODOLIST, not FB.Array( of TODOITEM ) written out: a generic spelled directly in a
    ' TYPE field is silently never defined, and the only symptom is "error 18: Element not
    ' defined" at every use site, naming the member and never this line.
    todoItems      as TODOLIST

    declare sub BuildIndexes()
end type

' A symbol identified by (owning set, index). Never stored across a Swap -
' acquired, used and dropped within one UI callback.
type SYMBOLREF
    pRSet as PARSERESULTSET ptr
    idx   as long                          ' -1 = not found
    declare function IsValid() as boolean
end type

' A diagnostic identified by (owning set, index).
type DIAGREF
    pRSet as PARSERESULTSET ptr
    idx   as long
end type

' The enumeration result lists. Aliased for the same reason TODOLIST and RETIREQUEUE are:
' a generic spelled out in a TYPE field never instantiates. None of these is a field today,
' but they are one edit away from becoming one, and the alias costs nothing.
type SYMBOLREFLIST as FB.Array( of SYMBOLREF )
type DIAGREFLIST   as FB.Array( of DIAGREF )
type FILENAMELIST  as FB.Array( of DWSTRING )

' FB intrinsic calltip (LEN, MID, ...). Kept as a separate sorted lookup - these
' never appear in any include file, so they are not part of the scanned symbols.
type FBINTRINSIC_TIP
    wszName    as DWSTRING                  ' original case, for display
    wszNameUp  as DWSTRING                  ' uppercased, for binary search
    wszCalltip as DWSTRING
end type

type clsSymbolDb
    private:
        m_pProject     as PARSERESULTSET ptr
        m_pBuffer      as PARSERESULTSET ptr
        ' merge masks recomputed on every Swap (UI thread), one bool per file:
        m_projContrib(any) as boolean       ' project file i's symbols are visible
        m_bufContrib(any)  as boolean       ' buffer  file i's symbols are visible

        declare sub RecomputeMerge()
        declare sub InstallProjectTodos( byval pNew as PARSERESULTSET ptr )
        declare function FileContributes( byval pRSet as PARSERESULTSET ptr, byval nFileIdx as long ) as boolean
        declare function FindByKindMask( byref wszName as const wstring, byval kindMask as long ) as SYMBOLREF
        declare function FindInSet( byval pRSet as PARSERESULTSET ptr, byref wszName as const wstring, byval kindMask as long, byval bIgnoreSuppression as boolean = false ) as SYMBOLREF

    public:
        ' installs a new set for its tier, returns the displaced set (to retire); UI thread
        declare function InstallSet( byval nTier as long, byval pNew as PARSERESULTSET ptr ) as PARSERESULTSET ptr

        ' -- lookups (case-insensitive; buffer-first with the merge masks) --
        declare function FindProc( byref wszName as const wstring ) as SYMBOLREF
        declare function FindType( byref wszName as const wstring ) as SYMBOLREF
        declare function FindMemberOf( byval refType as SYMBOLREF, byref wszMember as const wstring ) as SYMBOLREF
        declare function GetTypeBase( byval refType as SYMBOLREF ) as SYMBOLREF
        declare function ResolveTypeText( byref wszTypeText as const wstring ) as SYMBOLREF
        declare function FindVariable( byref wszName as const wstring, byref wszFile as const wstring, byval nCaretLine as long ) as SYMBOLREF

        ' -- enumerations (results() redim'd by callee; returns count) --
        declare sub EnumMembers( byval refType as SYMBOLREF, byref results as SYMBOLREFLIST, byval bWalkExtends as boolean = true )
        declare sub EnumPrefix( byref wszPrefix as const wstring, byval kindMask as long, byref results as SYMBOLREFLIST )
        declare sub EnumLocalsInScope( byref wszFile as const wstring, byval nCaretLine as long, byref results as SYMBOLREFLIST )
        declare sub EnumProcsInFile( byref wszFile as const wstring, byref results as SYMBOLREFLIST )
        declare sub EnumUserFiles( byref results as FILENAMELIST )
        declare sub EnumAllProcsTypes( byref results as SYMBOLREFLIST, byval bUserFilesOnly as boolean )
        declare sub EnumDiags( byref results as DIAGREFLIST )

        ' -- accessors on a SYMBOLREF (thin reads over pResult) --
        declare function SymName( byval r as SYMBOLREF ) as wstring ptr
        declare function SymTypeText( byval r as SYMBOLREF ) as wstring ptr   ' 0 if typeOffset = -1
        declare function SymFile( byval r as SYMBOLREF ) as wstring ptr
        declare function SymKind( byval r as SYMBOLREF ) as long
        declare function SymFlags( byval r as SYMBOLREF ) as long
        declare function SymLine( byval r as SYMBOLREF ) as long              ' DECLARE line for procs
        declare function SymCol( byval r as SYMBOLREF ) as long
        declare function SymBodyLine( byval r as SYMBOLREF ) as long          ' 0 if declare-only
        declare function SymParent( byval r as SYMBOLREF ) as SYMBOLREF

        ' -- reference counts (see FBCP_SYMBFLAG_REFTRACKED in fbcParser.bi) --
        ' Only meaningful when SymFlags() has FBCP_SYMBFLAG_REFTRACKED; without it
        ' a zero means UNKNOWN, not unused.
        declare function SymReadCount( byval r as SYMBOLREF ) as long
        declare function SymWriteCount( byval r as SYMBOLREF ) as long
        declare function SymRefFile( byval r as SYMBOLREF ) as wstring ptr    ' 0 if no reference
        declare function SymRefLine( byval r as SYMBOLREF ) as long           ' 0 if no reference

        ' Root of the toolchain include tree ("" when not derivable), for the
        ' user-files-only test EnumAllProcsTypes also applies. Already uppercased.
        declare function ToolchainIncludeRoot() as DWSTRING

        ' -- calltip synthesis (replaces stored CallTip strings) --
        declare function BuildCalltip( byval refProc as SYMBOLREF ) as DWSTRING
        ' Does this proc take any caller-supplied parameter? Stops at the first one
        ' instead of formatting them all, for callers that only need the yes/no.
        declare function ProcHasParams( byval refProc as SYMBOLREF ) as boolean

        ' -- display name: members prefixed with their owning TYPE ("clsDoc.GetLine") --
        declare function QualifiedName( byval r as SYMBOLREF ) as DWSTRING

        ' -- misc --
        declare function ProjectSet() as PARSERESULTSET ptr
        declare function BufferSet() as PARSERESULTSET ptr
end type

dim shared gSymDb as clsSymbolDb

' FB intrinsics table, sorted ascending by wszNameUp (binary search).
dim shared gFBIntrinsics( any ) as FBINTRINSIC_TIP
declare function FindIntrinsicCalltip( byref wszName as const wstring ) as DWSTRING
