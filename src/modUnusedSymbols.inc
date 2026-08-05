' ========================================================================================
' modUnusedSymbols.inc   -- see modUnusedSymbols.bi for what this is and why it is
'                           separate from clsSymbolDb.
' ========================================================================================

' ----------------------------------------------------------------------------------------
' Map an FBCP_KIND onto the coarser group the filter toggles use. Returns -1 for kinds the
' report never shows (DEFINE, NAMESPACE).
' ----------------------------------------------------------------------------------------
private function UnusedSymbols_GroupOf( byval nFbcpKind as long ) as long
    select case nFbcpKind
        case FBCP_KIND_VAR                        : return UnusedKindVariable
        case FBCP_KIND_SUB, FBCP_KIND_FUNCTION    : return UnusedKindProcedure
        case FBCP_KIND_PARAM                      : return UnusedKindParameter
        case FBCP_KIND_TYPE, FBCP_KIND_UNION, _
             FBCP_KIND_ENUM, FBCP_KIND_TYPEDEF    : return UnusedKindType
        case FBCP_KIND_FIELD                      : return UnusedKindField
        case FBCP_KIND_CONST, FBCP_KIND_ENUMVAL   : return UnusedKindConstant
    end select
    function = -1
end function

function UnusedSymbols_KindText( byval nKind as long ) as DWSTRING
    select case nKind
        case UnusedKindVariable  : return L(400, "Variable")
        case UnusedKindProcedure : return L(401, "Procedure")
        case UnusedKindParameter : return L(402, "Parameter")
        case UnusedKindType      : return L(403, "Type")
        case UnusedKindField     : return L(404, "Field")
        case UnusedKindConstant  : return L(405, "Constant")
    end select
    return ""
end function

function UnusedSymbols_StatusText( byref r as UNUSEDROW ) as DWSTRING
    select case r.nStatus
        case UnusedStatusWriteOnly
            ' the count is the evidence - show it rather than only a word
            return L(406, "Never read") & " (" & r.nWrites & "w)"
        case UnusedStatusUnknown
            return L(407, "Unknown")
    end select
    return L(408, "Unused")
end function

' Case-insensitive text compare: -1 / 0 / +1.
private function UnusedSymbols_CmpText( byref a as const wstring, byref b as const wstring ) as long
    dim as DWSTRING ua = PsUCase( a ), ub = PsUCase( b )
    if ua < ub then return -1
    if ua > ub then return 1
    function = 0
end function

' ----------------------------------------------------------------------------------------
' Comparator. PURE.
'
' Line and reference counts compare NUMERICALLY - comparing them as text puts 10 before 9,
' which is the obvious bug in a report whose whole point is line numbers.
'
' Every column falls through to the same (file, line) tiebreak, which is what makes the
' sort stable in the sense that matters here: re-clicking a header does not shuffle rows
' that compare equal on the chosen column.
' ----------------------------------------------------------------------------------------
function UnusedSymbols_Compare( byref a as UNUSEDROW, byref b as UNUSEDROW, _
                                byval nCol as long ) as long
    dim as long n = 0

    select case nCol
        case UnusedColFile
            n = UnusedSymbols_CmpText( a.wszFile, b.wszFile )

        case UnusedColLine
            if a.nLine < b.nLine then
                n = -1
            elseif a.nLine > b.nLine then
                n = 1
            end if

        case UnusedColClass
            n = UnusedSymbols_CmpText( UnusedSymbols_KindText( a.nKind ), _
                                UnusedSymbols_KindText( b.nKind ) )

        case UnusedColName
            n = UnusedSymbols_CmpText( a.wszName, b.wszName )

        case UnusedColStatus
            ' group by status first, then by how many writes - a row written five times
            ' and never read is more interesting than one written once
            if a.nStatus < b.nStatus then
                n = -1
            elseif a.nStatus > b.nStatus then
                n = 1
            elseif a.nWrites < b.nWrites then
                n = -1
            elseif a.nWrites > b.nWrites then
                n = 1
            end if
    end select

    if n <> 0 then return n

    ' deterministic tiebreak
    n = UnusedSymbols_CmpText( a.wszFile, b.wszFile )
    if n <> 0 then return n
    if a.nLine < b.nLine then return -1
    if a.nLine > b.nLine then return 1
    function = UnusedSymbols_CmpText( a.wszName, b.wszName )
end function

private sub UnusedSymbols_QuickSort( rows() as UNUSEDROW, byval nLo as long, byval nHi as long, _
                                     byval nCol as long, byval bDesc as boolean )
    if nLo >= nHi then exit sub

    dim as long i = nLo, j = nHi
    dim as long nMid = nLo + (nHi - nLo) \ 2
    dim as UNUSEDROW pivot = rows(nMid)

    do while i <= j
        do while i <= nHi
            dim as long c = UnusedSymbols_Compare( rows(i), pivot, nCol )
            if bDesc then c = -c
            if c >= 0 then exit do
            i += 1
        loop
        do while j >= nLo
            dim as long c = UnusedSymbols_Compare( rows(j), pivot, nCol )
            if bDesc then c = -c
            if c <= 0 then exit do
            j -= 1
        loop
        if i <= j then
            swap rows(i), rows(j)
            i += 1
            j -= 1
        end if
    loop

    UnusedSymbols_QuickSort( rows(), nLo, j, nCol, bDesc )
    UnusedSymbols_QuickSort( rows(), i, nHi, nCol, bDesc )
end sub

sub UnusedSymbols_Sort( rows() as UNUSEDROW, byval nCount as long, _
                        byval nCol as long, byval bDesc as boolean )
    if nCount < 2 then exit sub
    if nCol < 0 orelse nCol >= UNUSED_COLUMN_COUNT then exit sub
    UnusedSymbols_QuickSort( rows(), 0, nCount - 1, nCol, bDesc )
end sub

' ----------------------------------------------------------------------------------------
' Filter test. PURE.
' ----------------------------------------------------------------------------------------
function UnusedSymbols_Passes( byref r as UNUSEDROW, byval nKindMask as long ) as boolean
    if (nKindMask and (1 shl r.nKind)) = 0 then return false
    ' The kind mask is the only test left now that the text filter is gone. This must be
    ' TRUE: a fall-through of FALSE rejects every row, and an empty list reads as "no
    ' unused symbols found" rather than as a broken filter.
    return true
end function

' ----------------------------------------------------------------------------------------
' NOISE POLICY. Should this symbol appear in the report at all?
'
' Every suppression here is a place the report can go SILENT rather than wrong, which is
' the failure mode to worry about: an over-firing filter produces an empty list, and an
' empty list reads as "no problems found". Each one is asserted with a near-miss
' neighbour in the self-test for exactly that reason.
' ----------------------------------------------------------------------------------------
private function UnusedSymbols_ShouldReport( byval refSym as SYMBOLREF, _
                                             byval nFlags as long, _
                                             byval nGroup as long ) as boolean
    ' the counts mean nothing for this symbol
    if (nFlags and FBCP_SYMBFLAG_REFTRACKED) = 0 then return false

    ' the implicit THIS of every method
    if (nFlags and FBCP_SYMBFLAG_INSTANCE) <> 0 then return false

    if nGroup = UnusedKindParameter then
        ' Params of a DECLARE-only prototype can never be referenced - there is no body
        ' to reference them from. Reporting them buries the real findings under every
        ' declaration in every header.
        dim as SYMBOLREF refParent = gSymDb.SymParent( refSym )
        if refParent.IsValid then
            if gSymDb.SymBodyLine( refParent ) = 0 then return false
        end if
    end if

    function = true
end function

' Which rows are only ever "unknown"? Kept apart from ShouldReport: these ARE shown, so
' the user can judge them, they are just never labelled dead.
private function UnusedSymbols_IsUnknown( byval refSym as SYMBOLREF, byval nFlags as long, _
                                          byref wszName as const wstring ) as boolean
    ' reachable from outside anything a scan can see
    if (nFlags and FBCP_SYMBFLAG_EXPORT) <> 0 then return true

    ' implicitly invoked, never called by name
    dim as DWSTRING wszUp = PsUCase( wszName )
    if wszUp = "CONSTRUCTOR" orelse wszUp = "DESTRUCTOR" then return true
    if PsLeft( wszUp, 8 ) = "OPERATOR" then return true

    function = false
end function

' ----------------------------------------------------------------------------------------
' Build the report from ONE tier.
' ----------------------------------------------------------------------------------------
function UnusedSymbols_Build( byval nTier as long, rows() as UNUSEDROW, _
                              byref wszDirtyFiles as DWSTRING ) as long
    dim as long cnt = 0
    redim rows( 0 to 255 )
    wszDirtyFiles = ""

    ' ONE tier, never the merged view (see the .bi header): reference counts are
    ' whole-scan facts and do not merge per-file the way declarations do.
    dim as PARSERESULTSET ptr pRSet
    if nTier = ScanTierBuffer then
        pRSet = gSymDb.BufferSet()
    else
        pRSet = gSymDb.ProjectSet()
    end if
    if pRSet = 0 then return 0
    if pRSet->pResult = 0 then return 0

    ' precompute "is this one of the project's own files" per file index, the same test
    ' EnumAllProcsTypes applies. Without it the report is ~99% Win32 and AfxNova headers.
    dim as DWSTRING wszInc = gSymDb.ToolchainIncludeRoot()
    dim as long fc = pRSet->pResult->fileCount
    redim as boolean bUserFile( 0 to iif( fc > 0, fc - 1, 0 ) )
    for i as long = 0 to fc - 1
        bUserFile(i) = true
        if PsLen( wszInc ) > 0 then
            dim as DWSTRING wszF = *cast( wstring ptr, _
                @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[i] ] )
            if ucase( PsLeft( wszF, PsLen( wszInc ) ) ) = wszInc then bUserFile(i) = false
        end if
    next

    dim as long n = pRSet->pResult->symbolCount
    for i as long = 0 to n - 1
        dim as FBCP_SYMBOL ptr pSym = @pRSet->pResult->symbols[i]

        ' a row is only ever built for something with no reads
        if pSym->readCount > 0 then continue for
        if pSym->fileIndex < 0 then continue for
        if bUserFile( pSym->fileIndex ) = false then continue for

        dim as long nGroup = UnusedSymbols_GroupOf( pSym->kind )
        if nGroup < 0 then continue for

        dim as SYMBOLREF refSym
        refSym.pRSet = pRSet
        refSym.idx = i

        if UnusedSymbols_ShouldReport( refSym, pSym->flags, nGroup ) = false then continue for

        if cnt > ubound( rows ) then redim preserve rows( 0 to cnt * 2 + 1 )
        with rows(cnt)
            ' fbc interns filenames UPPERCASED (they are its include-once keys), so a scan
            ' reports C:\DEV\TIKO\SRC\FRMMAIN.INC. Restore the real casing for display --
            ' and do it HERE rather than at paint time, so the File column sorts and filters
            ' on the same text the user is reading.
            .wszFile = FilenameOriginalCase( *cast( wstring ptr, _
                @pRSet->pResult->stringPool[ pRSet->pResult->fileOffsets[pSym->fileIndex] ] ) )
            .nLine   = pSym->lineNum
            .nKind   = nGroup
            .wszName = gSymDb.QualifiedName( refSym )
            .nReads  = pSym->readCount
            .nWrites = pSym->writeCount

            if UnusedSymbols_IsUnknown( refSym, pSym->flags, .wszName ) then
                .nStatus = UnusedStatusUnknown
            elseif pSym->writeCount > 0 then
                .nStatus = UnusedStatusWriteOnly
            else
                .nStatus = UnusedStatusDead
            end if
        end with
        cnt += 1
    next

    ' Which open documents have unsaved edits AND contributed to this scan? The scan read
    ' from disk, so their line numbers may be stale and a row click can land on the wrong
    ' line. Reported, not blocked on.
    dim as clsDocument ptr pDoc = gApp.pDocList
    do until pDoc = 0
        if pDoc->hWindow(0) then
            if SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 then
                if PsLen( wszDirtyFiles ) > 0 then wszDirtyFiles = wszDirtyFiles & ", "
                wszDirtyFiles = wszDirtyFiles & PsPathName(pDoc->DiskFilename )
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    function = cnt
end function

' ========================================================================================
' Self-test.  TIKO_UNUSED_SELFTEST=1
'
' Covers the two routines that can be confidently WRONG rather than merely incomplete: the
' comparator and the filter test. Both are pure, so all of this runs with no window open
' and no scan.
'
' What this CANNOT reach, stated rather than left to be assumed: UnusedSymbols_Build (it
' needs a real scan), and everything about the window - that the sort glyph renders in the
' right column, that a row click lands on the right line, that the list is readable in
' both themes. See frmUnusedSymbols.inc's layout half and the interactive pass.
' ========================================================================================
private sub hUAssert( byval bCond as boolean, byref wszWhat as const wstring, _
                      byref nPass as long, byref nFail as long )
    if bCond then
        nPass += 1
    else
        nFail += 1
        dim as string s = "  FAIL " & wszWhat
        print s
    end if
end sub

private function hMakeRow( byref wszFile as const wstring, byval nLine as long, _
                           byval nKind as long, byref wszName as const wstring, _
                           byval nStatus as long, byval nWrites as long ) as UNUSEDROW
    dim as UNUSEDROW r
    r.wszFile = wszFile
    r.nLine = nLine
    r.nKind = nKind
    r.wszName = wszName
    r.nStatus = nStatus
    r.nWrites = nWrites
    function = r
end function

sub UnusedSymbols_RunSelfTest()
    if environ( "TIKO_UNUSED_SELFTEST" ) <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    print "--- TIKO_UNUSED_SELFTEST (model: comparator + filter) ---"

    ' ---- comparator: LINE MUST BE NUMERIC ------------------------------------------
    ' 9 vs 10 is the case a text compare gets wrong, and it is the whole point of a
    ' report keyed on line numbers
    dim as UNUSEDROW r9  = hMakeRow( "a.bas", 9,  UnusedKindVariable, "x", UnusedStatusDead, 0 )
    dim as UNUSEDROW r10 = hMakeRow( "a.bas", 10, UnusedKindVariable, "y", UnusedStatusDead, 0 )
    hUAssert( UnusedSymbols_Compare( r9, r10, UnusedColLine ) < 0, _
              "line 9 sorts before line 10 (numeric, not lexicographic)", nPass, nFail )
    hUAssert( UnusedSymbols_Compare( r10, r9, UnusedColLine ) > 0, _
              "line 10 sorts after line 9", nPass, nFail )

    ' ---- comparator: every column, both directions -----------------------------------
    dim as UNUSEDROW ra = hMakeRow( "aaa.bas", 5, UnusedKindVariable,  "alpha", UnusedStatusDead, 0 )
    dim as UNUSEDROW rb = hMakeRow( "bbb.bas", 5, UnusedKindProcedure, "beta",  UnusedStatusWriteOnly, 2 )
    hUAssert( UnusedSymbols_Compare( ra, rb, UnusedColFile ) < 0, "file column orders a<b", nPass, nFail )
    hUAssert( UnusedSymbols_Compare( ra, rb, UnusedColName ) < 0, "name column orders alpha<beta", nPass, nFail )
    hUAssert( UnusedSymbols_Compare( ra, rb, UnusedColStatus ) < 0, "status column orders Unused<WriteOnly", nPass, nFail )
    hUAssert( UnusedSymbols_Compare( ra, ra, UnusedColFile ) = 0, "a row compares equal to itself", nPass, nFail )

    ' case-insensitivity: a mixed-case name must not sort away from its neighbours
    dim as UNUSEDROW rUp = hMakeRow( "AAA.BAS", 5, UnusedKindVariable, "ALPHA", UnusedStatusDead, 0 )
    hUAssert( UnusedSymbols_Compare( ra, rUp, UnusedColName ) = 0, _
              "name compare is case-insensitive", nPass, nFail )

    ' ---- comparator: deterministic tiebreak ------------------------------------------
    ' two rows equal on the sort column must still order consistently, or re-clicking a
    ' header visibly shuffles them
    dim as UNUSEDROW t1 = hMakeRow( "a.bas", 1, UnusedKindVariable, "aaa", UnusedStatusDead, 0 )
    dim as UNUSEDROW t2 = hMakeRow( "a.bas", 1, UnusedKindVariable, "bbb", UnusedStatusDead, 0 )
    hUAssert( UnusedSymbols_Compare( t1, t2, UnusedColClass ) < 0, _
              "rows equal on the sort column fall back to a stable tiebreak", nPass, nFail )
    hUAssert( UnusedSymbols_Compare( t1, t2, UnusedColClass ) = _
              -UnusedSymbols_Compare( t2, t1, UnusedColClass ), _
              "the comparator is antisymmetric", nPass, nFail )

    ' ---- the sort itself, ascending AND descending ----------------------------------
    redim as UNUSEDROW rows( 0 to 4 )
    rows(0) = hMakeRow( "c.bas", 30, UnusedKindVariable, "c", UnusedStatusDead, 0 )
    rows(1) = hMakeRow( "a.bas", 9,  UnusedKindVariable, "a", UnusedStatusDead, 0 )
    rows(2) = hMakeRow( "b.bas", 10, UnusedKindVariable, "b", UnusedStatusDead, 0 )
    rows(3) = hMakeRow( "a.bas", 100, UnusedKindVariable, "d", UnusedStatusDead, 0 )
    rows(4) = hMakeRow( "a.bas", 2,  UnusedKindVariable, "e", UnusedStatusDead, 0 )

    UnusedSymbols_Sort( rows(), 5, UnusedColLine, false )
    hUAssert( rows(0).nLine = 2 andalso rows(1).nLine = 9 andalso rows(2).nLine = 10 _
              andalso rows(3).nLine = 30 andalso rows(4).nLine = 100, _
              "ascending sort by line: 2,9,10,30,100", nPass, nFail )

    UnusedSymbols_Sort( rows(), 5, UnusedColLine, true )
    hUAssert( rows(0).nLine = 100 andalso rows(1).nLine = 30 andalso rows(2).nLine = 10 _
              andalso rows(3).nLine = 9 andalso rows(4).nLine = 2, _
              "descending sort by line: 100,30,10,9,2", nPass, nFail )

    UnusedSymbols_Sort( rows(), 5, UnusedColFile, false )
    hUAssert( rows(0).wszFile = "a.bas" andalso rows(4).wszFile = "c.bas", _
              "ascending sort by file", nPass, nFail )
    hUAssert( rows(0).nLine = 2 andalso rows(1).nLine = 9 andalso rows(2).nLine = 100, _
              "rows equal on file stay ordered by the line tiebreak", nPass, nFail )

    UnusedSymbols_Sort( rows(), 5, UnusedColFile, true )
    hUAssert( rows(0).wszFile = "c.bas" andalso rows(4).wszFile = "a.bas", _
              "descending sort by file", nPass, nFail )

    ' a single row and an empty list must not fault
    UnusedSymbols_Sort( rows(), 1, UnusedColName, false )
    UnusedSymbols_Sort( rows(), 0, UnusedColName, false )
    hUAssert( true, "sorting 0 and 1 rows is a no-op, not a fault", nPass, nFail )

    ' an out-of-range column must leave the array alone rather than scramble it
    dim as DWSTRING wszFirst = rows(0).wszFile
    UnusedSymbols_Sort( rows(), 5, 99, false )
    hUAssert( rows(0).wszFile = wszFirst, "an out-of-range sort column is ignored", nPass, nFail )

    ' ---- filter: the kind mask -------------------------------------------------------
    dim as UNUSEDROW rv = hMakeRow( "a.bas", 1, UnusedKindVariable,  "thing", UnusedStatusDead, 0 )
    dim as UNUSEDROW rp = hMakeRow( "a.bas", 1, UnusedKindProcedure, "thing", UnusedStatusDead, 0 )
    dim as long maskVar = (1 shl UnusedKindVariable)
    dim as long maskAll = (1 shl UNUSED_KIND_COUNT) - 1

    hUAssert( UnusedSymbols_Passes( rv, maskVar ), "variable passes a variables-only mask", nPass, nFail )
    hUAssert( UnusedSymbols_Passes( rp, maskVar ) = false, "procedure fails a variables-only mask", nPass, nFail )
    hUAssert( UnusedSymbols_Passes( rp, maskAll ), "procedure passes an all-kinds mask", nPass, nFail )
    hUAssert( UnusedSymbols_Passes( rv, 0 ) = false, "an empty mask passes nothing", nPass, nFail )

    ' ---- localization: every id this module owns must resolve -------------------------
    ' L() DISCARDS its default, so a missing .lang entry renders BLANK with nothing in the
    ' build to say so
    for k as long = 0 to UNUSED_KIND_COUNT - 1
        hUAssert( len( UnusedSymbols_KindText( k ) ) > 0, _
                  "kind " & k & " has a non-empty localized name", nPass, nFail )
    next
    ' print them back: len()>0 cannot tell a real translation from the English fallback
    dim as string sLang = "  langcheck 400=" & UnusedSymbols_KindText( UnusedKindVariable ) & _
                          " | 401=" & UnusedSymbols_KindText( UnusedKindProcedure ) & _
                          " | 408=" & UnusedSymbols_StatusText( rv )
    print sLang

    dim as string sOut = "--- TIKO_UNUSED_SELFTEST: " & nPass & " passed, " & nFail & " failed ---"
    print sOut
end sub
