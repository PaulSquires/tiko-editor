' ========================================================================================
' clsSymbolDb implementation. See clsSymbolDb.bi for the model and threading rules.
' ========================================================================================

' ---- pure helpers (no globals; safe to call from the worker thread) --------------------

' FNV-1a over the ASCII-uppercased wchars of a pooled name. FB identifiers are
' ASCII, so ASCII-only case folding is exact for symbol names.
private function SymDb_HashName( byval pName as const wstring ptr ) as ulong
    dim as ulong h = 2166136261UL
    dim as long i = 0
    do
        dim as ulong c = pName[i]
        if c = 0 then exit do
        if (c >= 97) andalso (c <= 122) then c -= 32   ' a-z -> A-Z
        h = (h xor c) * 16777619UL
        i += 1
    loop
    function = h
end function

' ASCII case-insensitive equality of two null-terminated wide names.
private function SymDb_NameEqW( byval pName as const wstring ptr, byval pQuery as const wstring ptr ) as boolean
    dim as long i = 0
    do
        dim as ulong a = pName[i]
        dim as ulong b = pQuery[i]
        if (a >= 97) andalso (a <= 122) then a -= 32
        if (b >= 97) andalso (b <= 122) then b -= 32
        if a <> b then return false
        if a = 0 then return true
        i += 1
    loop
end function

' ASCII case-insensitive prefix test: does pName start with the first plen chars of pPrefix?
private function SymDb_NamePrefixW( byval pName as const wstring ptr, byval pPrefix as const wstring ptr, byval plen as long ) as boolean
    if plen <= 0 then return true
    for i as long = 0 to plen - 1
        dim as ulong a = pName[i]
        dim as ulong b = pPrefix[i]
        if a = 0 then return false          ' name shorter than prefix
        if (a >= 97) andalso (a <= 122) then a -= 32
        if (b >= 97) andalso (b <= 122) then b -= 32
        if a <> b then return false
    next
    return true
end function

' Case-insensitive path equality.
'
' WAS lstrcmpiW -- a Win32 call, and the only one in this file. SymDb_NameEqW is
' twelve lines above, does the same ASCII-insensitive comparison over the same
' kind of NUL-terminated wide data, allocates nothing, and is already trusted for
' symbol names. Using it here costs no conversion and removes the dependency.
'
' THE FOLD NARROWS, AND THAT IS A REAL BEHAVIOUR CHANGE. lstrcmpiW folds the full
' Unicode range; this folds ASCII. Two paths differing only in the case of a
' non-ASCII character now compare UNEQUAL on Windows where they used to match.
' Deliberate, for two reasons:
'
'   - PsCore's PsUCase folds ASCII only as well, so this is what the layer does
'     everywhere else once the port lands. Keeping lstrcmpiW would leave one
'     comparison in the program disagreeing with all the others.
'   - ON LINUX PATHS ARE CASE-SENSITIVE, so a portable symbol database cannot
'     lean on case-insensitive path equality at all. The question is only which
'     way to be wrong on the way there, and ASCII-folding is the smaller step.
'
' ---- AND SEPARATORS FOLD TOO, since 7c step 18 -----------------------------------------
'
' A PATH IS NOT A SYMBOL NAME, and this used SymDb_NameEqW unchanged, so "C:/dev/a.bas" and
' "C:\dev\a.bas" were different files. Nothing noticed while every path in the process had
' been through FilenameOriginalCase, which ends in PsPathToNative and hands back backslashes.
' The moment a caller built a path the PsPath house way -- forward slashes, which is what
' PsPath.bi tells shared logic to use -- the file table missed and EnumProcsInFile returned
' ZERO. That is a Functions pane with no rows and no error: the same shape as a suite that
' does not run.
'
' Folded in the COMPARISON rather than by normalising both sides, because SymDb_FileIndexOf
' calls this once per entry in the file table -- 134 entries for tiko.bas -- and a
' PsPathNormalise per call would allocate twice per entry per lookup. This allocates nothing.
private function SymDb_PathEqW( byval pName as const wstring ptr, byval pQuery as const wstring ptr ) as boolean
    dim as long i = 0
    do
        dim as ulong a = pName[i]
        dim as ulong b = pQuery[i]
        if (a >= 97) andalso (a <= 122) then a -= 32
        if (b >= 97) andalso (b <= 122) then b -= 32
        if a = 92 then a = 47                   ' '\' and '/' are the same separator here
        if b = 92 then b = 47
        if a <> b then return false
        if a = 0 then return true
        i += 1
    loop
end function

private function SymDb_FileNameEq( byval a as DWSTRING, byval b as DWSTRING ) as boolean
    return SymDb_PathEqW( a.Wz(), b.Wz() )
end function

' Index of wszFile in pRSet's file table, or -1.
private function SymDb_FileIndexOf( byval pRSet as PARSERESULTSET ptr, byval wszFile as DWSTRING ) as long
    if pRSet = 0 then return -1
    if pRSet->pResult = 0 then return -1
    dim as long fc = pRSet->pResult->fileCount
    for i as long = 0 to fc - 1
        dim as wstring ptr pfn = @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ]
        if SymDb_FileNameEq( *pfn, wszFile ) then return i
    next
    function = -1
end function

' The proc whose body range brackets nCaretLine within file nFileIdx, or -1.
private function SymDb_EnclosingProc( byval pRSet as PARSERESULTSET ptr, byval nFileIdx as long, byval nCaretLine as long ) as long
    if nFileIdx < 0 then return -1
    dim as long s = pRSet->fileHead(nFileIdx)
    do while s >= 0
        dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[s]
        dim as boolean bProc = ((pSym->kind = FBCP_KIND_SUB) orelse (pSym->kind = FBCP_KIND_FUNCTION))
        if bProc andalso (pSym->bodyLine > 0) then
            if (nCaretLine >= pSym->bodyLine) andalso (nCaretLine <= pSym->bodyEndLine) then
                return s
            end if
        end if
        s = pRSet->fileNext(s)
    loop
    function = -1
end function

' First meaningful identifier of a type-text ("const Widget ptr" -> "Widget").
private function SymDb_ExtractBaseType( byval wszTypeText as DWSTRING ) as DWSTRING
    dim as DWSTRING wszT = PsTrim( wszTypeText )
    if PsLen( wszT ) = 0 then return ""
    dim as long cnt = PsStrParseCount( wszT, " " )
    for i as long = 1 to cnt
        dim as DWSTRING tok = PsTrim( PsStrParse( wszT, i, " " ) )
        if PsLen( tok ) = 0 then continue for
        if PsUCase( tok ) = "CONST" then continue for
        return tok
    next
    return ""
end function


' ---- PARSERESULTSET.BuildIndexes (worker thread) ---------------------------------------

sub PARSERESULTSET.BuildIndexes()
    dim as long n = 0
    dim as long fc = 0
    if this.pResult then
        n  = this.pResult->symbolCount
        fc = this.pResult->fileCount
    end if

    ' name hash sizing: power of two >= 2*n
    if n = 0 then
        this.hashSize = 0
    else
        dim as long cap = 16
        while cap < n * 2
            cap shl= 1
        wend
        this.hashSize = cap
        redim this.hashHead( 0 to cap - 1 )
        for i as long = 0 to cap - 1
            this.hashHead(i) = -1
        next
        redim this.hashNext( 0 to n - 1 )
    end if

    if fc > 0 then
        redim this.fileHead( 0 to fc - 1 )
        for i as long = 0 to fc - 1
            this.fileHead(i) = -1
        next
    end if

    if n > 0 then
        redim this.fileNext( 0 to n - 1 )
        redim this.childHead( 0 to n - 1 )
        redim this.childNext( 0 to n - 1 )
        for i as long = 0 to n - 1
            this.fileNext(i)  = -1
            this.childHead(i) = -1
            this.childNext(i) = -1
        next
    end if

    ' populate. iterate descending so prepend leaves each chain in ascending
    ' (declaration) order.
    dim as FBCP_SYMBOL ptr pSyms = 0
    if this.pResult then pSyms = this.pResult->symbols
    for i as long = n - 1 to 0 step -1
        dim as FBCP_SYMBOL ptr pSym = @pSyms[i]

        if this.hashSize > 0 then
            dim as ulong h = SymDb_HashName( @this.pResult->stringPool[ pSym->nameOffset ] )
            dim as long b = clng( h and culng( this.hashSize - 1 ) )
            this.hashNext(i) = this.hashHead(b)
            this.hashHead(b) = i
        end if

        if (pSym->fileIndex >= 0) andalso (pSym->fileIndex < fc) then
            this.fileNext(i) = this.fileHead( pSym->fileIndex )
            this.fileHead( pSym->fileIndex ) = i
        end if

        if (pSym->parentIndex >= 0) andalso (pSym->parentIndex < n) then
            this.childNext(i) = this.childHead( pSym->parentIndex )
            this.childHead( pSym->parentIndex ) = i
        end if
    next

    ' locate the root file
    this.rootFileIndex = -1
    for i as long = 0 to fc - 1
        dim as wstring ptr pfn = @this.pResult->stringPool[ this.pResult->fileOffsets[i] ]
        if SymDb_FileNameEq( *pfn, this.wszRootFile ) then
            this.rootFileIndex = i
            exit for
        end if
    next
end sub


' ---- SYMBOLREF -------------------------------------------------------------------------

function SYMBOLREF.IsValid() as boolean
    return (this.pRSet <> 0) andalso (this.idx >= 0)
end function


' ---- clsSymbolDb: set installation + merge --------------------------------------------

function clsSymbolDb.ProjectSet() as PARSERESULTSET ptr
    function = m_pProject
end function

function clsSymbolDb.BufferSet() as PARSERESULTSET ptr
    function = m_pBuffer
end function

function clsSymbolDb.InstallSet( byval nTier as long, byval pNew as PARSERESULTSET ptr ) as PARSERESULTSET ptr
    dim as PARSERESULTSET ptr pOld
    if nTier = ScanTierBuffer then
        pOld = m_pBuffer
        m_pBuffer = pNew
        ' the buffer scan's TODO line-scan replaces this file's stored entries
        ' (UI thread, per the store's threading rule)
        if pNew then TodoStore_ReplaceFile( pNew->wszRootFile, pNew->todoItems(), pNew->todoCount )
    else
        pOld = m_pProject
        m_pProject = pNew
        ' the project sweep re-line-scanned every user file from disk
        if pNew then this.InstallProjectTodos( pNew )
    end if
    this.RecomputeMerge()
    function = pOld
end function


' Replace the stored TODO rows of every file a project scan visited. The active
' document's rows are left alone - its buffer scans are fresher than the disk.
' Files the sweep found no TODOs in get their stale rows cleared (ReplaceFile
' with zero items removes), so deletions on disk propagate too.
sub clsSymbolDb.InstallProjectTodos( byval pNew as PARSERESULTSET ptr )
    if pNew->pResult = 0 then exit sub

    dim as DWSTRING wszBufRoot
    if m_pBuffer then wszBufRoot = m_pBuffer->wszRootFile

    dim tmp(any) as TODOITEM
    dim as long fc = pNew->pResult->fileCount
    for i as long = 0 to fc - 1
        dim as wstring ptr pFile = @pNew->pResult->stringPool[ pNew->pResult->fileOffsets[i] ]
        if PsLen( wszBufRoot ) > 0 then
            if SymDb_FileNameEq( *pFile, wszBufRoot ) then continue for
        end if

        dim as long nSub = 0
        redim tmp( 0 to 7 )
        for t as long = 0 to pNew->todoCount - 1
            if SymDb_FileNameEq( pNew->todoItems(t).wszFile, *pFile ) then
                if nSub > ubound( tmp ) then redim preserve tmp( 0 to nSub * 2 + 7 )
                tmp(nSub) = pNew->todoItems(t)
                nSub += 1
            end if
        next
        if nSub > 0 then
            ' store/display under the on-disk casing (scan names are UPPERCASED;
            ' the store's keying is case-insensitive either way)
            TodoStore_ReplaceFile( FilenameOriginalCase( *pFile ), tmp(), nSub )
        else
            TodoStore_ReplaceFile( *pFile, tmp(), nSub )
        end if
    next
end sub

sub clsSymbolDb.RecomputeMerge()
    dim as boolean bHaveProject = false
    dim as boolean bHaveBuffer  = false
    if m_pProject then
        if m_pProject->pResult then bHaveProject = true
    end if
    if m_pBuffer then
        if m_pBuffer->pResult then bHaveBuffer = true
    end if

    ' project files: all contribute, except the copy of the buffer's root file.
    ' Suppress it ONLY when the buffer set actually contains its root
    ' (rootFileIndex >= 0): a buffer scan of a lone .inc can abort on fbc's
    ' max-error limit before its own body is ever reached, leaving the root out
    ' of its file table - suppressing by name alone would then hide the
    ' project's complete copy while the buffer offers nothing in its place,
    ' and the active file vanishes from the Functions panel.
    if bHaveProject then
        dim as long fc = m_pProject->pResult->fileCount
        if fc > 0 then
            redim m_projContrib( 0 to fc - 1 )
            for i as long = 0 to fc - 1
                m_projContrib(i) = true
            next
            if bHaveBuffer andalso (m_pBuffer->rootFileIndex >= 0) then
                for i as long = 0 to fc - 1
                    dim as wstring ptr pfn = @m_pProject->pResult->stringPool[ m_pProject->pResult->fileOffsets[i] ]
                    if SymDb_FileNameEq( *pfn, m_pBuffer->wszRootFile ) then m_projContrib(i) = false
                next
            end if
        end if
    end if

    ' buffer files: root always contributes (the override); other files only if
    ' the project set does not already cover them
    if bHaveBuffer then
        dim as long fc = m_pBuffer->pResult->fileCount
        if fc > 0 then
            redim m_bufContrib( 0 to fc - 1 )
            for i as long = 0 to fc - 1
                dim as boolean bIsRoot = (i = m_pBuffer->rootFileIndex)
                if bIsRoot then
                    m_bufContrib(i) = true
                elseif bHaveProject = false then
                    m_bufContrib(i) = true
                else
                    dim as wstring ptr pfn = @m_pBuffer->pResult->stringPool[ m_pBuffer->pResult->fileOffsets[i] ]
                    m_bufContrib(i) = (SymDb_FileIndexOf( m_pProject, *pfn ) < 0)
                end if
            next
        end if
    end if
end sub

function clsSymbolDb.FileContributes( byval pRSet as PARSERESULTSET ptr, byval nFileIdx as long ) as boolean
    if nFileIdx < 0 then return true            ' unknown file - include it
    if pRSet = m_pProject then
        if nFileIdx <= ubound( m_projContrib ) then return m_projContrib(nFileIdx)
        return true
    elseif pRSet = m_pBuffer then
        if nFileIdx <= ubound( m_bufContrib ) then return m_bufContrib(nFileIdx)
        return true
    end if
    function = true
end function


' ---- clsSymbolDb: lookups --------------------------------------------------------------

function clsSymbolDb.FindInSet( byval pRSet as PARSERESULTSET ptr, byval wszName as DWSTRING, byval kindMask as long, byval bIgnoreSuppression as boolean = false ) as SYMBOLREF
    dim as SYMBOLREF res
    res.idx = -1
    if pRSet = 0 then return res
    if pRSet->pResult = 0 then return res
    if pRSet->hashSize = 0 then return res

    dim as ulong h = SymDb_HashName( wszName.Wz() )
    dim as long b = clng( h and culng( pRSet->hashSize - 1 ) )
    dim as long s = pRSet->hashHead(b)
    do while s >= 0
        dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[s]
        if (kindMask and (1 shl pSym->kind)) <> 0 then
            dim as wstring ptr pName = @pRSet->pResult->stringPool[ pSym->nameOffset ]
            if SymDb_NameEqW( pName, wszName.Wz() ) then
                dim as boolean bVisible = bIgnoreSuppression
                if bVisible = false then bVisible = this.FileContributes( pRSet, pSym->fileIndex )
                if bVisible then
                    res.pRSet = pRSet
                    res.idx = s
                    return res
                end if
            end if
        end if
        s = pRSet->hashNext(s)
    loop
    return res
end function

function clsSymbolDb.FindByKindMask( byval wszName as DWSTRING, byval kindMask as long ) as SYMBOLREF
    ' buffer first (freshest), then the project set honoring the merge masks,
    ' then the project set IGNORING suppression: a context-starved buffer scan
    ' of a lone .inc drops symbols fbc's error recovery could not parse, and
    ' falling back to the project's (possibly stale) complete copy beats
    ' reporting the symbol as nonexistent.
    dim as SYMBOLREF r = this.FindInSet( m_pBuffer, wszName, kindMask )
    if r.IsValid then return r
    r = this.FindInSet( m_pProject, wszName, kindMask )
    if r.IsValid then return r
    return this.FindInSet( m_pProject, wszName, kindMask, true )
end function

function clsSymbolDb.FindProc( byval wszName as DWSTRING ) as SYMBOLREF
    return this.FindByKindMask( wszName, SYMMASK_PROC )
end function

function clsSymbolDb.FindType( byval wszName as DWSTRING ) as SYMBOLREF
    dim as SYMBOLREF r = this.FindByKindMask( wszName, SYMMASK_TYPE )
    ' resolve TYPEDEF/ALIAS chains to the concrete TYPE/UNION/ENUM
    dim as long hops = 0
    do while r.IsValid andalso (this.SymKind(r) = FBCP_KIND_TYPEDEF) andalso (hops < 16)
        dim as wstring ptr pType = this.SymTypeText(r)
        if pType = 0 then exit do
        dim as DWSTRING wszBase = SymDb_ExtractBaseType( *pType )
        if PsLen( wszBase ) = 0 then exit do
        dim as SYMBOLREF r2 = this.FindByKindMask( wszBase, SYMMASK_TYPE )
        if r2.IsValid = false then exit do
        if (r2.idx = r.idx) andalso (r2.pRSet = r.pRSet) then exit do   ' self-alias
        r = r2
        hops += 1
    loop
    return r
end function

function clsSymbolDb.GetTypeBase( byval refType as SYMBOLREF ) as SYMBOLREF
    dim as SYMBOLREF miss
    miss.idx = -1
    if refType.IsValid = false then return miss
    dim as long k = this.SymKind( refType )
    if (k <> FBCP_KIND_TYPE) andalso (k <> FBCP_KIND_UNION) then return miss
    dim as wstring ptr pBase = this.SymTypeText( refType )   ' EXTENDS base name for a TYPE/UNION
    if pBase = 0 then return miss
    dim as DWSTRING wszBase = SymDb_ExtractBaseType( *pBase )
    if PsLen( wszBase ) = 0 then return miss
    return this.FindType( wszBase )
end function

function clsSymbolDb.ResolveTypeText( byval wszTypeText as DWSTRING ) as SYMBOLREF
    dim as DWSTRING wszBase = SymDb_ExtractBaseType( wszTypeText )
    if PsLen( wszBase ) = 0 then
        dim as SYMBOLREF miss
        miss.idx = -1
        return miss
    end if
    return this.FindType( wszBase )
end function

function clsSymbolDb.FindMemberOf( byval refType as SYMBOLREF, byval wszMember as DWSTRING ) as SYMBOLREF
    dim as SYMBOLREF miss
    miss.idx = -1
    if refType.IsValid = false then return miss

    dim as SYMBOLREF cur = refType
    dim as long hops = 0
    do while cur.IsValid andalso (hops < 32)
        dim as PARSERESULTSET ptr pRSet = cur.pRSet
        dim as long c = pRSet->childHead( cur.idx )
        do while c >= 0
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
            if (SYMMASK_MEMBER and (1 shl pSym->kind)) <> 0 then
                dim as wstring ptr pName = @pRSet->pResult->stringPool[ pSym->nameOffset ]
                if SymDb_NameEqW( pName, wszMember.Wz() ) then
                    dim as SYMBOLREF r
                    r.pRSet = pRSet
                    r.idx = c
                    return r
                end if
            end if
            c = pRSet->childNext(c)
        loop
        cur = this.GetTypeBase( cur )       ' walk up the EXTENDS chain
        hops += 1
    loop
    return miss
end function

function clsSymbolDb.FindVariable( byval wszName as DWSTRING, byval wszFile as DWSTRING, byval nCaretLine as long ) as SYMBOLREF
    dim as SYMBOLREF miss
    miss.idx = -1

    ' 1) locals/params of the enclosing proc, then 2) module-level vars of the file
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for
        dim as long fileIdx = SymDb_FileIndexOf( pRSet, wszFile )
        if fileIdx < 0 then continue for

        dim as long proc = SymDb_EnclosingProc( pRSet, fileIdx, nCaretLine )
        if proc >= 0 then
            dim as long c = pRSet->childHead( proc )
            do while c >= 0
                dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
                dim as long k = pSym->kind
                dim as boolean bVarKind = ((k = FBCP_KIND_VAR) orelse (k = FBCP_KIND_PARAM) orelse (k = FBCP_KIND_CONST))
                if bVarKind then
                    if SymDb_NameEqW( @pRSet->pResult->stringPool[pSym->nameOffset], wszName.Wz() ) then
                        dim as SYMBOLREF r
                        r.pRSet = pRSet
                        r.idx = c
                        return r
                    end if
                end if
                c = pRSet->childNext(c)
            loop
        end if

        dim as long s = pRSet->fileHead( fileIdx )
        do while s >= 0
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[s]
            dim as long k = pSym->kind
            dim as boolean bTop = (pSym->parentIndex = -1)
            dim as boolean bVarKind = ((k = FBCP_KIND_VAR) orelse (k = FBCP_KIND_CONST))
            if bTop andalso bVarKind then
                if SymDb_NameEqW( @pRSet->pResult->stringPool[pSym->nameOffset], wszName.Wz() ) then
                    dim as SYMBOLREF r
                    r.pRSet = pRSet
                    r.idx = s
                    return r
                end if
            end if
            s = pRSet->fileNext(s)
        loop
    next

    ' 3) any SHARED / module-level var
    dim as SYMBOLREF r2 = this.FindByKindMask( wszName, SYMMASK_VAR )
    if r2.IsValid then return r2
    return miss
end function


' ---- clsSymbolDb: enumerations ---------------------------------------------------------

function clsSymbolDb.EnumMembers( byval refType as SYMBOLREF, results() as SYMBOLREF, byval bWalkExtends as boolean = true ) as long
    dim as long cnt = 0
    redim results( 0 to 63 )
    if refType.IsValid = false then return 0

    dim as SYMBOLREF cur = refType
    dim as long hops = 0
    do while cur.IsValid andalso (hops < 32)
        dim as PARSERESULTSET ptr pRSet = cur.pRSet
        dim as wstring ptr pPrevName = 0
        dim as long c = pRSet->childHead( cur.idx )
        do while (c >= 0) andalso (cnt < SYMDB_MAX_ENUM)
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
            if (SYMMASK_MEMBER and (1 shl pSym->kind)) <> 0 then
                ' dynamic member arrays emit twin FIELD records (data + its
                ' descriptor) with the same name, adjacent in decl order - skip
                dim as wstring ptr pName = @pRSet->pResult->stringPool[ pSym->nameOffset ]
                dim as boolean bDup = false
                if pPrevName then bDup = SymDb_NameEqW( pName, pPrevName )
                if bDup = false then
                    if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
                    results(cnt).pRSet = pRSet
                    results(cnt).idx = c
                    cnt += 1
                    pPrevName = pName
                end if
            end if
            c = pRSet->childNext(c)
        loop
        if bWalkExtends = false then exit do
        cur = this.GetTypeBase( cur )
        hops += 1
    loop
    function = cnt
end function

' USER FILES FIRST, then the toolchain's. SYMDB_MAX_ENUM is a hard stop, and symbols
' are walked in DECLARATION order -- which for any real project means windows.bi, then
' the framework, then the project's own code dead last. A bare "dim p as " (no prefix
' typed) matches EVERY type in the scan: tiko's own project produces 14,457 of them, so
' the 8,192 stop was reached inside the toolchain headers and the project's own types
' were never returned at all. tiko's FINDREPLACE_TYPE sits at record 14,177 -- the popup
' could not show it because this function never handed it over.
'
' Splitting the walk by origin makes the stop truncate toolchain symbols instead of the
' user's. It also fixes the popup's own AUTOCOMPLETE_MAX_ITEMS cap for free, since
' results arrive in the order the caller appends them.
function clsSymbolDb.EnumPrefix( byval wszPrefix as DWSTRING, byval kindMask as long, results() as SYMBOLREF ) as long
    dim as long cnt = 0
    redim results( 0 to 127 )
    dim as long plen = PsLen( wszPrefix )
    dim as DWSTRING wszInc = this.ToolchainIncludeRoot()
    dim as long nIncLen = PsLen( wszInc )

    for origin as long = 0 to 1
        ' Without a derivable toolchain root nothing can be classified, so origin 0
        ' enumerates everything in scan order and there is no second round.
        if (origin = 1) andalso (nIncLen = 0) then exit for
        dim as boolean bWantToolchain = (origin = 1)

        for pass as long = 0 to 1
            dim as PARSERESULTSET ptr pRSet
            if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
            if pRSet = 0 then continue for
            if pRSet->pResult = 0 then continue for

            ' precompute per-file "is a toolchain file" once (same idiom as
            ' EnumAllProcsTypes) rather than re-testing the path per symbol
            dim as long fc = pRSet->pResult->fileCount
            redim as boolean bTool( 0 to iif(fc > 0, fc - 1, 0) )
            if nIncLen then
                for i as long = 0 to fc - 1
                    dim as DWSTRING wszF = *cast(wstring ptr, @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ])
                    bTool(i) = ( PsUCase( PsLeft( wszF, nIncLen ) ) = wszInc )
                next
            end if

            dim as long n = pRSet->pResult->symbolCount
            for i as long = 0 to n - 1
                if cnt >= SYMDB_MAX_ENUM then exit for
                dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[i]
                if (kindMask and (1 shl pSym->kind)) = 0 then continue for
                if this.FileContributes( pRSet, pSym->fileIndex ) = false then continue for
                if nIncLen then
                    ' A symbol with no file cannot be classified - treat it as the
                    ' user's, so it is offered rather than dropped by both rounds.
                    dim as boolean bIsToolchain = false
                    if pSym->fileIndex >= 0 then bIsToolchain = bTool( pSym->fileIndex )
                    if bIsToolchain <> bWantToolchain then continue for
                end if
                dim as wstring ptr pName = @pRSet->pResult->stringPool[ pSym->nameOffset ]
                if SymDb_NamePrefixW( pName, wszPrefix.Wz(), plen ) then
                    if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
                    results(cnt).pRSet = pRSet
                    results(cnt).idx = i
                    cnt += 1
                end if
            next
        next
    next
    function = cnt
end function

function clsSymbolDb.EnumLocalsInScope( byval wszFile as DWSTRING, byval nCaretLine as long, results() as SYMBOLREF ) as long
    dim as long cnt = 0
    redim results( 0 to 63 )
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for
        dim as long fileIdx = SymDb_FileIndexOf( pRSet, wszFile )
        if fileIdx < 0 then continue for
        dim as long proc = SymDb_EnclosingProc( pRSet, fileIdx, nCaretLine )
        if proc < 0 then continue for
        dim as long c = pRSet->childHead( proc )
        do while (c >= 0) andalso (cnt < SYMDB_MAX_ENUM)
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
            dim as long k = pSym->kind
            dim as boolean bWant = ((k = FBCP_KIND_VAR) orelse (k = FBCP_KIND_PARAM) orelse (k = FBCP_KIND_CONST))
            if bWant then
                if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
                results(cnt).pRSet = pRSet
                results(cnt).idx = c
                cnt += 1
            end if
            c = pRSet->childNext(c)
        loop
        if cnt > 0 then return cnt          ' the active file lives in one set
    next
    function = cnt
end function

function clsSymbolDb.EnumProcsInFile( byval wszFile as DWSTRING, results() as SYMBOLREF ) as long
    dim as long cnt = 0
    redim results( 0 to 63 )
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for
        dim as long fileIdx = SymDb_FileIndexOf( pRSet, wszFile )
        if fileIdx < 0 then continue for
        if this.FileContributes( pRSet, fileIdx ) = false then continue for
        dim as long s = pRSet->fileHead( fileIdx )
        do while (s >= 0) andalso (cnt < SYMDB_MAX_ENUM)
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[s]
            dim as boolean bProc = ((pSym->kind = FBCP_KIND_SUB) orelse (pSym->kind = FBCP_KIND_FUNCTION))
            if bProc then
                if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
                results(cnt).pRSet = pRSet
                results(cnt).idx = s
                cnt += 1
            end if
            s = pRSet->fileNext(s)
        loop
        return cnt                          ' the file lives in one set; first hit wins
    next
    function = cnt
end function

function clsSymbolDb.EnumUserFiles( results() as DWSTRING ) as long
    dim as DWSTRING wszInc
    if PsLen( gConfig.FBWINCompiler64 ) then
        wszInc = PsUCase( PsPathDirWithSep(ProcessFromCurdriveApp( gConfig.FBWINCompiler64 ) ) & "inc" )
    end if
    dim as long cnt = 0
    redim results( 0 to 63 )
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for
        dim as long fc = pRSet->pResult->fileCount
        for i as long = 0 to fc - 1
            if this.FileContributes( pRSet, i ) = false then continue for
            dim as DWSTRING wszF = *cast(wstring ptr, @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ])
            if PsLen( wszInc ) then
                if PsUCase( PsLeft( wszF, PsLen( wszInc ) ) ) = wszInc then continue for
            end if
            dim as boolean bDup = false
            for j as long = 0 to cnt - 1
                if SymDb_FileNameEq( results(j), wszF ) then
                    bDup = true
                    exit for
                end if
            next
            if bDup = false then
                if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
                results(cnt) = wszF
                cnt += 1
            end if
        next
    next
    function = cnt
end function

function clsSymbolDb.EnumAllProcsTypes( results() as SYMBOLREF, byval bUserFilesOnly as boolean ) as long
    dim as long cnt = 0
    redim results( 0 to 255 )
    dim as long wantMask = SYMMASK_PROC or SYMMASK_TYPE
    dim as DWSTRING wszInc
    if bUserFilesOnly andalso (PsLen( gConfig.FBWINCompiler64 ) > 0) then
        wszInc = PsUCase( PsPathDirWithSep(ProcessFromCurdriveApp( gConfig.FBWINCompiler64 ) ) & "inc" )
    end if

    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for

        ' precompute per-file "is a user file" once
        dim as long fc = pRSet->pResult->fileCount
        redim as boolean bUserFile( 0 to iif(fc > 0, fc - 1, 0) )
        for i as long = 0 to fc - 1
            bUserFile(i) = true
            if bUserFilesOnly andalso (PsLen( wszInc ) > 0) then
                dim as DWSTRING wszF = *cast(wstring ptr, @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ])
                if PsUCase( PsLeft( wszF, PsLen( wszInc ) ) ) = wszInc then bUserFile(i) = false
            end if
        next

        dim as long n = pRSet->pResult->symbolCount
        for i as long = 0 to n - 1
            if cnt >= SYMDB_MAX_ENUM then exit for
            dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[i]
            if (wantMask and (1 shl pSym->kind)) = 0 then continue for
            if this.FileContributes( pRSet, pSym->fileIndex ) = false then continue for
            if bUserFilesOnly then
                if pSym->fileIndex < 0 then continue for
                if bUserFile( pSym->fileIndex ) = false then continue for
            end if
            if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
            results(cnt).pRSet = pRSet
            results(cnt).idx = i
            cnt += 1
        next
    next
    function = cnt
end function

function clsSymbolDb.EnumDiags( results() as DIAGREF ) as long
    dim as long cnt = 0
    redim results( 0 to 63 )
    for pass as long = 0 to 1
        dim as PARSERESULTSET ptr pRSet
        if pass = 0 then pRSet = m_pBuffer else pRSet = m_pProject
        if pRSet = 0 then continue for
        if pRSet->pResult = 0 then continue for
        dim as long dc = pRSet->pResult->diagCount
        for i as long = 0 to dc - 1
            if cnt > ubound(results) then redim preserve results( 0 to cnt * 2 + 1 )
            results(cnt).pRSet = pRSet
            results(cnt).idx = i
            cnt += 1
        next
    next
    function = cnt
end function


' ---- clsSymbolDb: accessors ------------------------------------------------------------

function clsSymbolDb.SymName( byval r as SYMBOLREF ) as wstring ptr
    if r.IsValid = false then return 0
    function = @r.pRSet->pResult->stringPool[ r.pRSet->pResult->symbols[r.idx].nameOffset ]
end function

function clsSymbolDb.SymTypeText( byval r as SYMBOLREF ) as wstring ptr
    if r.IsValid = false then return 0
    dim as long ofs = r.pRSet->pResult->symbols[r.idx].typeOffset
    if ofs < 0 then return 0
    function = @r.pRSet->pResult->stringPool[ ofs ]
end function

function clsSymbolDb.SymFile( byval r as SYMBOLREF ) as wstring ptr
    if r.IsValid = false then return 0
    dim as long fi = r.pRSet->pResult->symbols[r.idx].fileIndex
    if fi < 0 then return 0
    function = @r.pRSet->pResult->stringPool[ r.pRSet->pResult->fileOffsets[fi] ]
end function

function clsSymbolDb.SymKind( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].kind
end function

function clsSymbolDb.SymFlags( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].flags
end function

function clsSymbolDb.SymReadCount( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].readCount
end function

function clsSymbolDb.SymWriteCount( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].writeCount
end function

function clsSymbolDb.SymRefFile( byval r as SYMBOLREF ) as wstring ptr
    if r.IsValid = false then return 0
    dim as long fi = r.pRSet->pResult->symbols[r.idx].refFileIndex
    if fi < 0 then return 0
    if fi >= r.pRSet->pResult->fileCount then return 0
    function = cast( wstring ptr, @r.pRSet->pResult->stringPool[ r.pRSet->pResult->fileOffsets[fi] ] )
end function

function clsSymbolDb.SymRefLine( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].refLine
end function

function clsSymbolDb.ToolchainIncludeRoot() as DWSTRING
    if PsLen( gConfig.FBWINCompiler64 ) = 0 then return ""
    return PsUCase( PsPathDirWithSep(ProcessFromCurdriveApp( gConfig.FBWINCompiler64 ) ) & "inc" )
end function

function clsSymbolDb.SymLine( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].lineNum
end function

function clsSymbolDb.SymCol( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].colNum
end function

function clsSymbolDb.SymBodyLine( byval r as SYMBOLREF ) as long
    if r.IsValid = false then return 0
    function = r.pRSet->pResult->symbols[r.idx].bodyLine
end function

function clsSymbolDb.SymParent( byval r as SYMBOLREF ) as SYMBOLREF
    dim as SYMBOLREF res
    res.idx = -1
    if r.IsValid = false then return res
    dim as long p = r.pRSet->pResult->symbols[r.idx].parentIndex
    if p < 0 then return res
    res.pRSet = r.pRSet
    res.idx = p
    return res
end function


' ---- clsSymbolDb: calltip synthesis ----------------------------------------------------

function clsSymbolDb.BuildCalltip( byval refProc as SYMBOLREF ) as DWSTRING
    if refProc.IsValid = false then return ""
    dim as PARSERESULTSET ptr pRSet = refProc.pRSet
    dim as FBCP_SYMBOL ptr pProc = @pRSet->pResult->symbols[ refProc.idx ]

    dim as DWSTRING wszTip = *cast(wstring ptr, @pRSet->pResult->stringPool[ pProc->nameOffset ]) & "("

    dim as long nParam = 0
    dim as long c = pRSet->childHead( refProc.idx )
    do while c >= 0
        dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
        if pSym->kind = FBCP_KIND_PARAM then
            if (pSym->flags and FBCP_SYMBFLAG_INSTANCE) = 0 then
                dim as DWSTRING wszP
                if (pSym->flags and FBCP_SYMBFLAG_BYREF) then wszP = "byref "
                wszP = wszP & *cast(wstring ptr, @pRSet->pResult->stringPool[ pSym->nameOffset ])
                if (pSym->flags and FBCP_SYMBFLAG_ARRAY) then wszP = wszP & "()"
                if pSym->typeOffset >= 0 then
                    wszP = wszP & " as " & *cast(wstring ptr, @pRSet->pResult->stringPool[ pSym->typeOffset ])
                end if
                if (pSym->flags and FBCP_SYMBFLAG_OPTIONAL) then wszP = "[" & wszP & "]"
                if nParam > 0 then wszTip = wszTip & ", "
                wszTip = wszTip & wszP
                nParam += 1
            end if
        end if
        c = pRSet->childNext(c)
    loop

    wszTip = wszTip & ")"
    if (pProc->kind = FBCP_KIND_FUNCTION) andalso (pProc->typeOffset >= 0) then
        wszTip = wszTip & " as " & *cast(wstring ptr, @pRSet->pResult->stringPool[ pProc->typeOffset ])
    end if
    return wszTip
end function


' ========================================================================================
' Does this proc take a caller-supplied parameter?
'
' The same child walk BuildCalltip does, stopping at the first hit and formatting
' nothing. The INSTANCE flag skips the implicit THIS of a member proc, which the caller
' never supplies -- without that, every member function would look parameterised.
' ========================================================================================
function clsSymbolDb.ProcHasParams( byval refProc as SYMBOLREF ) as boolean
    if refProc.IsValid = false then return false
    dim as PARSERESULTSET ptr pRSet = refProc.pRSet

    dim as long c = pRSet->childHead( refProc.idx )
    do while c >= 0
        dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[c]
        if pSym->kind = FBCP_KIND_PARAM then
            if (pSym->flags and FBCP_SYMBFLAG_INSTANCE) = 0 then return true
        end if
        c = pRSet->childNext(c)
    loop

    return false
end function


function clsSymbolDb.QualifiedName( byval r as SYMBOLREF ) as DWSTRING
    if r.IsValid = false then return ""
    dim as DWSTRING wszName = *this.SymName( r )
    dim as SYMBOLREF rp = this.SymParent( r )
    if rp.IsValid then
        select case this.SymKind( rp )
            case FBCP_KIND_TYPE, FBCP_KIND_UNION
                wszName = *this.SymName( rp ) & "." & wszName
        end select
    end if
    return wszName
end function


' ---- TODO store (flat, keyed by file; UI thread only) ----------------------------------

sub TodoStore_RemoveFile( byval wszFile as DWSTRING )
    dim as long nOut = 0
    for i as long = 0 to gTodoCount - 1
        if SymDb_FileNameEq( gTodoItems(i).wszFile, wszFile ) = false then
            if nOut <> i then gTodoItems(nOut) = gTodoItems(i)
            nOut += 1
        end if
    next
    ' clear the dropped tail entries so their strings free now
    for i as long = nOut to gTodoCount - 1
        gTodoItems(i).wszFile = ""
        gTodoItems(i).wszText = ""
        gTodoItems(i).nLineNum = 0
    next
    gTodoCount = nOut
end sub

sub TodoStore_ReplaceFile( byval wszFile as DWSTRING, items() as TODOITEM, byval itemCount as long )
    TodoStore_RemoveFile( wszFile )
    if itemCount <= 0 then exit sub
    if gTodoCount + itemCount - 1 > ubound( gTodoItems ) then
        redim preserve gTodoItems( 0 to (gTodoCount + itemCount) * 2 + 7 )
    end if
    for i as long = 0 to itemCount - 1
        gTodoItems( gTodoCount ).wszFile = wszFile
        gTodoItems( gTodoCount ).nLineNum = items(i).nLineNum
        gTodoItems( gTodoCount ).wszText = items(i).wszText
        gTodoCount += 1
    next
end sub


' ---- FB intrinsics lookup --------------------------------------------------------------

function FindIntrinsicCalltip( byval wszName as DWSTRING ) as DWSTRING
    dim as long hi = ubound( gFBIntrinsics )
    if hi < 0 then return ""
    dim as DWSTRING wszUp = PsUCase( wszName )
    dim as long lo = 0
    do while lo <= hi
        dim as long mid_ = (lo + hi) \ 2
        if wszUp = gFBIntrinsics(mid_).wszNameUp then
            return gFBIntrinsics(mid_).wszCalltip
        elseif (wszUp < gFBIntrinsics(mid_).wszNameUp) then
            hi = mid_ - 1
        else
            lo = mid_ + 1
        end if
    loop
    return ""
end function
