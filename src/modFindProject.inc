'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#include once "modFindProject.bi"


' ========================================================================================
' The hidden scratch view. EVERY search runs through this and never through one of the
' user's editor windows, because search flags and the target range are per-view state: the
' in-document Find bar has a path that reuses previously-set flags, so clobbering them from
' here would be a bug at a distance.
' ========================================================================================
dim shared as HWND gFipScratch

private function FindProject_Scratch() as HWND
    if IsWindow( gFipScratch ) then return gFipScratch

    ' IDC_FINDPROJ_SCRATCH deliberately is NOT IDC_SCINTILLA(+1): clsDocument.IsValidScintillaID
    ' is a control-ID test, and frmMain_OnNotify uses it to decide whether a notification
    ' belongs to a document. An unrecognised id is ignored, which is what we want.
    ' PHASE 7d: a tikoSciHost. It holds a REFERENCE to a document owned by the
    ' main editor -- see the SCI_ADDREFDOCUMENT discipline below -- so it has to
    ' be the same Scintilla implementation as the editor is, or the refcount is
    ' being kept by one library on another library's object.
    gFipScratch = SciHost_Create( HWND_FRMMAIN, IDC_FINDPROJ_SCRATCH, WS_CHILD )
    if gFipScratch then
        ' SILENCE IT. The modification event mask is per-view and defaults to every event,
        ' and every view bound to a document is one of that document's watchers -- so a
        ' chatty scratch would fan a full-mask SCN_MODIFIED out of every keystroke the user
        ' makes in a file it happens to be pointed at.
        SciExec( gFipScratch, SCI_SETMODEVENTMASK, 0, 0 )
        SciExec( gFipScratch, SCI_SETVSCROLLBAR, 0, 0 )
        SciExec( gFipScratch, SCI_SETHSCROLLBAR, 0, 0 )
    end if
    function = gFipScratch
end function


' Let go of whatever document the scratch is pointed at. Called after every document, so the
' scratch never holds the only reference to a document the user then closes.
private sub FindProject_UnbindScratch()
    if IsWindow( gFipScratch ) = 0 then exit sub
    ' A null document pointer makes Scintilla hand the view a fresh empty buffer of its own,
    ' releasing the one it held.
    SciExec( gFipScratch, SCI_SETDOCPOINTER, 0, 0 )
end sub


function FindProject_Messenger() as HWND
    return FindProject_Scratch()
end function


sub FindProject_Shutdown()
    ' ORDER IS LOAD-BEARING. Clear() gives back one document reference per result file, and
    ' SCI_RELEASEDOCUMENT is a MESSAGE -- it needs a live Scintilla window to send it to. Destroy
    ' the scratch first and every document the search ever touched leaks, permanently, with its
    ' lexer state attached.
    FindProject_Clear()
    FindProject_UnbindScratch()
    if IsWindow( gFipScratch ) then
        DestroyWindow( gFipScratch )
        gFipScratch = 0
    end if
end sub


' ========================================================================================
' Array growth. Capacity doubles, so a 2000-match result does not pay 2000 reallocations.
' ========================================================================================
private sub FindProject_EnsureMatchCap( byval n as long )
    if n <= ubound(gFip.matches) + 1 then exit sub
    dim as long nCap = ubound(gFip.matches) + 1
    if nCap < 64 then nCap = 64
    do while nCap < n
        nCap *= 2
    loop
    redim preserve gFip.matches(0 to nCap - 1)
end sub

private sub FindProject_EnsureBlockCap( byval n as long )
    if n <= ubound(gFip.blocks) + 1 then exit sub
    dim as long nCap = ubound(gFip.blocks) + 1
    if nCap < 32 then nCap = 32
    do while nCap < n
        nCap *= 2
    loop
    redim preserve gFip.blocks(0 to nCap - 1)
end sub

private sub FindProject_EnsureGroupCap( byval n as long )
    if n <= ubound(gFip.groups) + 1 then exit sub
    dim as long nCap = ubound(gFip.groups) + 1
    if nCap < 16 then nCap = 16
    do while nCap < n
        nCap *= 2
    loop
    redim preserve gFip.groups(0 to nCap - 1)
end sub


' ========================================================================================
' Drop everything.
' ========================================================================================
sub FindProject_Clear()
    ' Give back the ONE document reference the model holds per result file.
    '
    ' That single reference is the whole refcount discipline: SCI_SETDOCPOINTER does its own
    ' release-old / addref-new, so an excerpt view rebinding from file A to file B decrements
    ' A -- and this reference is what guarantees A survives that regardless of how many views
    ' happen to be pointed at it.
    '
    ' Unbind the views BEFORE releasing, or a slot is left pointing at a document whose last
    ' reference just went.
    dim as HWND hMsg = FindProject_Messenger()
    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pSciDoc = 0 then continue for
        ' Take the highlight off the document before letting go of it, or a closed search
        ' leaves its marks behind in the user's editor.
        FindProject_ClearIndicators( g )
        if gFipUnbindDoc <> 0 then gFipUnbindDoc( gFip.groups(g).pSciDoc )
        if hMsg then
            SciExec( hMsg, SCI_RELEASEDOCUMENT, 0, cast(LPARAM, gFip.groups(g).pSciDoc) )
        end if
        gFip.groups(g).pSciDoc = 0
    next

    erase gFip.groups
    erase gFip.matches
    erase gFip.blocks
    gFip.nGroupCount  = 0
    gFip.nMatchCount  = 0
    gFip.nBlockCount  = 0
    gFip.nLiveMatches = 0
    gFip.wszPhrase    = ""
    gFip.bMatchCase   = false
    gFip.bWholeWord   = false
    gFip.bValid       = false
    gFip.bCapped      = false
end sub


' ========================================================================================
' PURE helpers
' ========================================================================================

sub FindProject_LineWindow( byval nLine as long, byval nContext as long, _
                            byval nLineCount as long, _
                            byref nFirst as long, byref nLast as long )
    nFirst = nLine - nContext
    nLast  = nLine + nContext
    if nFirst < 0 then nFirst = 0
    ' nLineCount of 0 would otherwise produce an inverted window.
    dim as long nMax = nLineCount - 1
    if nMax < 0 then nMax = 0
    if nLast > nMax then nLast = nMax
    if nFirst > nLast then nFirst = nLast
end sub


function FindProject_ShouldMerge( byval nPrevLast as long, byval nNextFirst as long ) as boolean
    ' Overlapping and abutting windows merge, and so does a ONE-LINE GAP: leaving a single
    ' hidden line between two excerpts would put a separator over one line of context that is
    ' cheaper to just show. A two-line gap stays separate.
    '
    ' The "+ 2" is the whole policy, and it was "+ 1" first: the self-test case for the
    ' one-line gap disagreed with it, which is how the gap between the documented intent
    ' ("not worth a separator") and the code got noticed.
    function = (nNextFirst <= nPrevLast + 2)
end function


function FindProject_RemapMatch( byval nStart as long, byval nLen as long, _
                                 byval nModType as long, _
                                 byval nModPos as long, byval nModLen as long, _
                                 byref nNewStart as long, byref nNewLen as long ) as boolean
    nNewStart = nStart
    nNewLen   = nLen

    dim as long nEnd = nStart + nLen        ' exclusive

    if (nModType and SC_MOD_INSERTTEXT) <> 0 then
        if nModPos <= nStart then
            ' At or before the start: the whole match slides right.
            nNewStart = nStart + nModLen
        elseif nModPos >= nEnd then
            ' After the match: untouched.
        else
            ' STRICTLY INSIDE. The run of text is no longer the phrase that was searched for,
            ' so the match is dead. This is the case a Scintilla indicator gets wrong -- its
            ' tracking GROWS the range instead, which would leave the model reporting a
            ' "match" whose text does not match.
            return false
        end if
        return true
    end if

    if (nModType and SC_MOD_DELETETEXT) <> 0 then
        dim as long nDelEnd = nModPos + nModLen
        if nDelEnd <= nStart then
            ' Entirely before: slides left.
            nNewStart = nStart - nModLen
        elseif nModPos >= nEnd then
            ' Entirely after: untouched.
        else
            ' Overlaps the match in any way: dead.
            return false
        end if
        return true
    end if

    ' Anything that is neither an insert nor a delete moves no text.
    return true
end function


' ========================================================================================
' Env-gated self-test: TIKO_FINDPROJ_SELFTEST=1 (the PURE half -- no window, no document, no
' message pump). Runs alongside the wiring half in frmFindInProject.inc.
' ========================================================================================
dim shared as long gFipMPass, gFipMFail

private sub FindProject_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gFipMPass += 1
    else
        gFipMFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

' Shorthand: remap and report the outcome as one string, so a case reads as input -> output.
private function FindProject_RemapStr( byval nStart as long, byval nLen as long, _
                                       byval nModType as long, _
                                       byval nModPos as long, byval nModLen as long ) as string
    dim as long nNewStart, nNewLen
    if FindProject_RemapMatch( nStart, nLen, nModType, nModPos, nModLen, nNewStart, nNewLen ) = false then
        return "dead"
    end if
    return str(nNewStart) & "," & str(nNewLen)
end function

sub FindProject_RunSelfTest()
    if environ("TIKO_FINDPROJ_SELFTEST") <> "1" then exit sub

    gFipMPass = 0
    gFipMFail = 0
    print "--- TIKO_FINDPROJ_SELFTEST (model) ---"

    dim as long nF, nL

    ' ---- the context window --------------------------------------------------------------
    FindProject_LineWindow( 10, 2, 100, nF, nL )
    FindProject_Check( "window: a mid-file match gives 5 lines", _
        (nF = 8) andalso (nL = 12), str(nF) & ".." & str(nL) )
    FindProject_LineWindow( 0, 2, 100, nF, nL )
    FindProject_Check( "window: line 0 clamps at the top, not negative", _
        (nF = 0) andalso (nL = 2), str(nF) & ".." & str(nL) )
    FindProject_LineWindow( 99, 2, 100, nF, nL )
    FindProject_Check( "window: the last line clamps at the bottom", _
        (nF = 97) andalso (nL = 99), str(nF) & ".." & str(nL) )
    ' FILES SHORTER THAN THE 5-LINE WINDOW. The excerpt shows the whole file and nothing
    ' beyond it -- asking for lines that do not exist would draw blank rows past the end.
    FindProject_LineWindow( 1, 2, 3, nF, nL )
    FindProject_Check( "window: a 3-line file yields exactly its 3 lines", _
        (nF = 0) andalso (nL = 2), str(nF) & ".." & str(nL) )
    FindProject_LineWindow( 0, 2, 1, nF, nL )
    FindProject_Check( "window: a 1-line file yields exactly that line", _
        (nF = 0) andalso (nL = 0), str(nF) & ".." & str(nL) )
    FindProject_LineWindow( 1, 2, 2, nF, nL )
    FindProject_Check( "window: a 2-line file yields both lines", _
        (nF = 0) andalso (nL = 1), str(nF) & ".." & str(nL) )
    ' An empty document would otherwise produce nLast = -1 and a negative line count.
    FindProject_LineWindow( 0, 2, 0, nF, nL )
    FindProject_Check( "window: an empty document gives a single line, not -1", _
        (nF = 0) andalso (nL = 0), str(nF) & ".." & str(nL) )

    ' ---- the merge rule ------------------------------------------------------------------
    FindProject_Check( "merge: overlapping windows merge", _
        FindProject_ShouldMerge( 12, 10 ) = true )
    FindProject_Check( "merge: abutting windows merge", _
        FindProject_ShouldMerge( 12, 13 ) = true )
    ' The deliberate part: ONE unshown line between two excerpts is absorbed rather than
    ' separated, because a separator over a single hidden line reads as noise.
    FindProject_Check( "merge: a one-line gap is absorbed", _
        FindProject_ShouldMerge( 12, 14 ) = true )
    FindProject_Check( "merge: a two-line gap stays separate", _
        FindProject_ShouldMerge( 12, 15 ) = false )

    ' ---- remap: insertion ----------------------------------------------------------------
    ' A match of "foo" at 10..13 (exclusive end 13).
    FindProject_Check( "insert: before the match shifts it right", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 5, 2 ) = "12,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 5, 2 ) )
    FindProject_Check( "insert: exactly AT the start shifts it right", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 10, 1 ) = "11,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 10, 1 ) )
    FindProject_Check( "insert: exactly at the END leaves it alone", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 13, 1 ) = "10,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 13, 1 ) )
    FindProject_Check( "insert: after the match leaves it alone", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 40, 5 ) = "10,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 40, 5 ) )
    ' THE CASE A SCINTILLA INDICATOR GETS WRONG. Its own tracking would grow the range and
    ' leave the model reporting a match whose text is not the search phrase.
    FindProject_Check( "insert: STRICTLY INSIDE kills the match", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 11, 1 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 11, 1 ) )
    FindProject_Check( "insert: inside at the last byte also kills it", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 12, 1 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_INSERTTEXT, 12, 1 ) )

    ' ---- remap: deletion -----------------------------------------------------------------
    FindProject_Check( "delete: entirely before shifts it left", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 2, 4 ) = "6,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 2, 4 ) )
    FindProject_Check( "delete: ending exactly at the start shifts it left", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 8, 2 ) = "8,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 8, 2 ) )
    FindProject_Check( "delete: starting exactly at the end leaves it alone", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 13, 5 ) = "10,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 13, 5 ) )
    FindProject_Check( "delete: overlapping the head kills it", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 8, 4 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 8, 4 ) )
    FindProject_Check( "delete: overlapping the tail kills it", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 12, 4 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 12, 4 ) )
    FindProject_Check( "delete: containing the match kills it", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 5, 20 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 5, 20 ) )
    FindProject_Check( "delete: exactly the match kills it", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 10, 3 ) = "dead", _
        FindProject_RemapStr( 10, 3, SC_MOD_DELETETEXT, 10, 3 ) )

    ' ---- remap: neither ------------------------------------------------------------------
    ' A style or indicator change moves no text, so it must not move the match. Scintilla
    ' sends plenty of these.
    FindProject_Check( "other: a style change moves nothing", _
        FindProject_RemapStr( 10, 3, SC_MOD_CHANGESTYLE, 0, 100 ) = "10,3", _
        FindProject_RemapStr( 10, 3, SC_MOD_CHANGESTYLE, 0, 100 ) )

    print "--- TIKO_FINDPROJ_SELFTEST (model): " & str(gFipMPass) & " passed, " & _
          str(gFipMFail) & " failed ---"
end sub


' ========================================================================================
' Coalesce one group's matches into excerpt blocks.
' ========================================================================================
private sub FindProject_BuildBlocks( byval nGroup as long, byval nLineCount as long )
    dim as long m0 = gFip.groups(nGroup).nMatchFirst
    dim as long mN = m0 + gFip.groups(nGroup).nMatchCount - 1

    gFip.groups(nGroup).nBlockFirst = gFip.nBlockCount
    gFip.groups(nGroup).nBlockCount = 0

    dim as long i = m0
    do while i <= mN
        dim as long nFirst, nLast
        FindProject_LineWindow( gFip.matches(i).nLine, FINDPROJ_CONTEXT_LINES, _
                                nLineCount, nFirst, nLast )
        dim as long nMatchFirst = i
        dim as long nMatchCount = 1

        ' Absorb every following match whose own window touches or overlaps this one. Matches
        ' arrive in ascending position order, so one forward pass is enough.
        do while (i + 1) <= mN
            dim as long nF2, nL2
            FindProject_LineWindow( gFip.matches(i+1).nLine, FINDPROJ_CONTEXT_LINES, _
                                    nLineCount, nF2, nL2 )
            if FindProject_ShouldMerge( nLast, nF2 ) = false then exit do
            ' AN EXCERPT STAYS AN EXCERPT. Without this, a file in which nearly every line
            ' matches collapses into a single block spanning the whole file -- which is not an
            ' excerpt, and whose per-line snapshot then costs one GetLine for every line of
            ' every file. Stopping here starts a new block instead.
            if (nL2 - nFirst + 1) > FINDPROJ_MAX_BLOCK_LINES then exit do
            if nL2 > nLast then nLast = nL2
            nMatchCount += 1
            i += 1
        loop

        FindProject_EnsureBlockCap( gFip.nBlockCount + 1 )
        with gFip.blocks( gFip.nBlockCount )
            .nGroup      = nGroup
            .nFirstLine  = nFirst
            .nLineCount  = nLast - nFirst + 1
            .nMatchFirst = nMatchFirst
            .nMatchCount = nMatchCount
        end with

        ' (NO PER-LINE SNAPSHOT. Every block used to capture its lines as found, one GetLine
        '  each, to be the baseline a per-line dirty stripe compared against. The stripe is a
        '  per-FILE circle on the header now -- FindProject_IsGroupDirty -- so the snapshot has
        '  no consumer, and a search no longer walks the text of every block it builds.)

        gFip.nBlockCount += 1
        gFip.groups(nGroup).nBlockCount += 1

        i += 1
    loop
end sub


' ========================================================================================
' Search one document. Records a group only if it actually has matches -- which is what keeps
' "realize the document" off the files that turn out to have none.
' ========================================================================================
private sub FindProject_SearchOneDocument( byval pDoc as clsDocument ptr, _
                                           byval sPhrase as string, _
                                           byval nFlags as long )
    if pDoc = 0 then exit sub
    if len(sPhrase) = 0 then exit sub

    dim as HWND hScratch = FindProject_Scratch()
    if hScratch = 0 then exit sub

    dim as boolean bRealized = cbool( IsWindow( DocView(pDoc, 0) ) )

    if bRealized then
        ' Point the scratch at the document itself: no copy, and the buffer includes unsaved
        ' edits, which is the whole reason results come from documents rather than from disk.
        dim as any ptr pSciDoc = cast( any ptr, SciExec( DocView(pDoc, 0), SCI_GETDOCPOINTER, 0, 0 ) )
        if pSciDoc = 0 then exit sub
        SciExec( hScratch, SCI_SETDOCPOINTER, 0, cast(LPARAM, pSciDoc) )
    else
        ' Never realized: the bytes are still sitting in TextBuffer.
        if len( pDoc->TextBuffer ) = 0 then exit sub
        SciExec( hScratch, SCI_SETDOCPOINTER, 0, 0 )
        dim as long nCP = iif( pDoc->FileEncoding = FILE_ENCODING_ANSI, 0, SC_CP_UTF8 )
        SciExec( hScratch, SCI_SETCODEPAGE, nCP, 0 )
        SciExec( hScratch, SCI_SETTEXT, 0, strptr( pDoc->TextBuffer ) )
    end if

    dim as long nLength    = SciExec( hScratch, SCI_GETLENGTH, 0, 0 )
    dim as long nLineCount = SciExec( hScratch, SCI_GETLINECOUNT, 0, 0 )
    SciExec( hScratch, SCI_SETSEARCHFLAGS, nFlags, 0 )

    dim as long nFirstMatch = gFip.nMatchCount
    dim as long nFound = 0
    dim as long startPos = 0

    do
        SciExec( hScratch, SCI_SETTARGETSTART, startPos, 0 )
        SciExec( hScratch, SCI_SETTARGETEND, nLength, 0 )
        dim as long r = SciExec( hScratch, SCI_SEARCHINTARGET, len(sPhrase), strptr(sPhrase) )
        if r = -1 then exit do

        FindProject_EnsureMatchCap( gFip.nMatchCount + 1 )
        with gFip.matches( gFip.nMatchCount )
            .nStart = r
            .nLen   = len(sPhrase)
            .nLine  = SciExec( hScratch, SCI_LINEFROMPOSITION, r, 0 )
            .bDead  = false
        end with
        gFip.nMatchCount += 1
        nFound += 1

        startPos = SciExec( hScratch, SCI_GETTARGETEND, 0, 0 )
        ' Belt: a target that did not advance would spin forever.
        if startPos <= r then startPos = r + 1
        if startPos >= nLength then exit do
        ' The cap. Reported, not silent -- see FindProject_Search.
        if gFip.nMatchCount >= FINDPROJ_MAX_MATCHES then
            gFip.bCapped = true
            exit do
        end if
    loop

    if nFound = 0 then
        FindProject_UnbindScratch()
        exit sub
    end if

    ' There ARE hits, so this file needs a real Scintilla document for its excerpt views to
    ' bind to. AssignTextBuffer is idempotent and is the same path a normal file-open takes;
    ' the byte offsets collected above stay valid because it inserts those same bytes.
    '
    ' Deliberately NOT SCI_CREATEDOCUMENT: that document would be unknown to the clsDocument,
    ' so opening the file later would build a SECOND one and the excerpts would be editing a
    ' phantom copy.
    if bRealized = false then
        FindProject_UnbindScratch()
        pDoc->AssignTextBuffer()
    end if

    ' ONE reference per FILE, taken here and given back in FindProject_Clear. Never one per
    ' excerpt view: SCI_SETDOCPOINTER already does release-old/addref-new for the view itself,
    ' so per-slot bookkeeping would be both redundant and easy to get wrong. This single
    ' reference is what makes a rebind -- and a document the user closes -- safe.
    dim as any ptr pSciDocNew = 0
    if IsWindow( DocView(pDoc, 0) ) then
        pSciDocNew = cast( any ptr, SciExec( DocView(pDoc, 0), SCI_GETDOCPOINTER, 0, 0 ) )
        if pSciDocNew then
            SciExec( DocView(pDoc, 0), SCI_ADDREFDOCUMENT, 0, cast(LPARAM, pSciDocNew) )
        end if
    end if

    FindProject_EnsureGroupCap( gFip.nGroupCount + 1 )
    with gFip.groups( gFip.nGroupCount )
        .pDoc        = pDoc
        .pSciDoc     = pSciDocNew
        .wszFile     = pDoc->DiskFilename
        .bCollapsed  = false
        .nMatchFirst = nFirstMatch
        .nMatchCount = nFound
        .nBlockFirst = 0
        .nBlockCount = 0
    end with
    FindProject_BuildBlocks( gFip.nGroupCount, nLineCount )
    gFip.nGroupCount += 1

    FindProject_UnbindScratch()
end sub


' ========================================================================================
' Search every document in the editor.
' ========================================================================================
function FindProject_Search( byval wszPhrase as DWSTRING, _
                             byval bMatchCase as boolean, _
                             byval bWholeWord as boolean ) as long
    FindProject_Clear()
    if PsLen(wszPhrase) = 0 then return 0

    gFip.wszPhrase  = wszPhrase
    gFip.bMatchCase = bMatchCase
    gFip.bWholeWord = bWholeWord
    gFip.bCapped    = false

    dim as long nFlags = 0
    if bMatchCase then nFlags = (nFlags or SCFIND_MATCHCASE)
    if bWholeWord then nFlags = (nFlags or SCFIND_WHOLEWORD)

    ' The same wide->narrow conversion the in-document find uses (modFindReplace's
    ' "dim txtFind as string = gFind.txtFind"). Kept identical ON PURPOSE: it means a
    ' non-ASCII search term behaves the same way in both, rather than the two quietly
    ' disagreeing about what matches. Improving it is a change to BOTH or neither.
    dim as string sPhrase = wszPhrase

    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        FindProject_SearchOneDocument( pDoc, sPhrase, nFlags )
        ' Stop walking documents once the cap is reached: continuing would realize every
        ' remaining file for results that are already being truncated.
        if gFip.nMatchCount >= FINDPROJ_MAX_MATCHES then
            gFip.bCapped = true
            exit do
        end if
        pDoc = pDoc->pDocNext
    loop

    gFip.nLiveMatches = gFip.nMatchCount
    gFip.bValid = true

    for g as long = 0 to gFip.nGroupCount - 1
        FindProject_ApplyIndicators( g )
    next

    function = gFip.nMatchCount
end function


' ========================================================================================
' Match highlighting.
'
' Indicator RANGES belong to the Document, so they are filled once per file and show up in
' every view of it -- the excerpt views AND the file's own editor tab, which is the same way
' the Find bar's own highlight behaves. The indicator's COLOUR is per-view; FipPool_Bind sets
' that on each excerpt.
' ========================================================================================
sub FindProject_ApplyIndicators( byval nGroup as long )
    if (nGroup < 0) orelse (nGroup >= gFip.nGroupCount) then exit sub
    dim as clsDocument ptr pDoc = gFip.groups(nGroup).pDoc
    if pDoc = 0 then exit sub
    if IsWindow( DocView(pDoc, 0) ) = 0 then exit sub

    dim as HWND h = DocView(pDoc, 0)
    SciExec( h, SCI_SETINDICATORCURRENT, FINDPROJ_INDICATOR, 0 )
    SciExec( h, SCI_INDICATORCLEARRANGE, 0, SciExec( h, SCI_GETLENGTH, 0, 0 ) )

    dim as long m0 = gFip.groups(nGroup).nMatchFirst
    for i as long = m0 to m0 + gFip.groups(nGroup).nMatchCount - 1
        if gFip.matches(i).bDead then continue for
        SciExec( h, SCI_INDICATORFILLRANGE, gFip.matches(i).nStart, gFip.matches(i).nLen )
    next
end sub


sub FindProject_ClearIndicators( byval nGroup as long )
    if (nGroup < 0) orelse (nGroup >= gFip.nGroupCount) then exit sub
    dim as clsDocument ptr pDoc = gFip.groups(nGroup).pDoc
    if pDoc = 0 then exit sub
    if IsWindow( DocView(pDoc, 0) ) = 0 then exit sub

    dim as HWND h = DocView(pDoc, 0)
    SciExec( h, SCI_SETINDICATORCURRENT, FINDPROJ_INDICATOR, 0 )
    SciExec( h, SCI_INDICATORCLEARRANGE, 0, SciExec( h, SCI_GETLENGTH, 0, 0 ) )
end sub


' ========================================================================================
' Is one result file modified? The DOCUMENT's own answer, asked twice the way the tab bar
' asks it: SCI_GETMODIFY covers the buffer, UserModified covers the changes clsDocument makes
' outside Scintilla's own modified flag.
'
' A file whose document has been closed, or which was never realized, cannot be dirty -- there
' is nothing to have edited.
' ========================================================================================
function FindProject_IsGroupDirty( byval nGroup as long ) as boolean
    if (nGroup < 0) orelse (nGroup >= gFip.nGroupCount) then return false
    dim as clsDocument ptr pDoc = gFip.groups(nGroup).pDoc
    if pDoc = 0 then return false
    if IsWindow( DocView(pDoc, 0) ) = 0 then return false
    if SciExec( DocView(pDoc, 0), SCI_GETMODIFY, 0, 0 ) <> 0 then return true
    return pDoc->UserModified
end function


' ========================================================================================
' A result file has just been written to disk: repaint, so its header drops the dirty circle.
'
' THERE IS NOTHING TO RE-BASELINE ANY MORE, and that is the point. This used to re-snapshot
' every one of the file's blocks, because the dirty signal was a per-line comparison against
' text captured when the search ran -- so without a fresh baseline the stripe stayed lit after
' a save, for the rest of the result set's life, which is indistinguishable from the save not
' happening and is exactly how it was reported. The signal is FindProject_IsGroupDirty now,
' which asks the document, so a save clears it by construction and an undo after a save brings
' it back for the same reason.
'
' A REPAINT, NOT A REBUILD -- the same rule the modify hook follows. Rebuilding hides and
' re-binds every pooled view, which destroys the focus of whichever excerpt the caret is in,
' and a save made from inside an excerpt is the common case here.
' ========================================================================================
sub FindProject_OnDocumentSaved( byval pDocSaved as clsDocument ptr )
    if gFip.bValid = false then exit sub
    if pDocSaved = 0 then exit sub

    dim as boolean bTouched = false
    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc = pDocSaved then bTouched = true: exit for
    next

    if bTouched then
        if gFipRepaint <> 0 then gFipRepaint()
        if IsWindow( gTTabCtl.hWindow ) then AfxRedrawWindow( gTTabCtl.hWindow )
    end if
end sub


' Save every result file with unsaved changes. FALSE if the user cancelled one of them, which
' the caller takes as "abandon the close".
function FindProject_SaveAllDirty() as boolean
    for g as long = 0 to gFip.nGroupCount - 1
        dim as clsDocument ptr pDoc = gFip.groups(g).pDoc
        if pDoc = 0 then continue for
        if IsWindow( DocView(pDoc, 0) ) = 0 then continue for
        if SciExec( DocView(pDoc, 0), SCI_GETMODIFY, 0, 0 ) = 0 then
            if pDoc->UserModified = false then continue for
        end if
        ' SaveFile returns TRUE when the user cancelled (its own inverted convention).
        if pDoc->SaveFile() then return false
    next
    return true
end function


' Any result file with unsaved changes? The Find tab shows a dot when so. Built on the
' per-group test the headers use, so the tab's circle and a header's circle cannot disagree.
function FindProject_AnyDirty() as boolean
    for g as long = 0 to gFip.nGroupCount - 1
        if FindProject_IsGroupDirty( g ) then return true
    next
    return false
end function


' ========================================================================================
' Navigation helpers.
' ========================================================================================
function FindProject_NextLive( byval nFrom as long, byval bForward as boolean ) as long
    if gFip.nMatchCount <= 0 then return -1
    dim as long nStep = iif( bForward, 1, -1 )
    dim as long i = nFrom
    for n as long = 1 to gFip.nMatchCount
        i += nStep
        if i >= gFip.nMatchCount then i = 0
        if i < 0 then i = gFip.nMatchCount - 1
        if gFip.matches(i).bDead then continue for
        ' A match whose file has been closed is still in the array but has nowhere to go.
        dim as long g = -1
        for gg as long = 0 to gFip.nGroupCount - 1
            if (i >= gFip.groups(gg).nMatchFirst) andalso _
               (i < gFip.groups(gg).nMatchFirst + gFip.groups(gg).nMatchCount) then g = gg: exit for
        next
        if g = -1 then continue for
        if gFip.groups(g).pDoc = 0 then continue for
        return i
    next
    function = -1
end function


function FindProject_BlockOfMatch( byval nMatch as long ) as long
    for b as long = 0 to gFip.nBlockCount - 1
        if (nMatch >= gFip.blocks(b).nMatchFirst) andalso _
           (nMatch < gFip.blocks(b).nMatchFirst + gFip.blocks(b).nMatchCount) then return b
    next
    function = -1
end function


' ========================================================================================
' Replace.
'
' Files are left DIRTY -- nothing is written to disk. A project-wide replace that saved as it
' went would be unrecoverable outside per-file undo.
' ========================================================================================
function FindProject_ReplaceOne( byval nMatch as long, byval wszReplace as DWSTRING ) as boolean
    if (nMatch < 0) orelse (nMatch >= gFip.nMatchCount) then return false
    if gFip.matches(nMatch).bDead then return false

    dim as long g = -1
    for gg as long = 0 to gFip.nGroupCount - 1
        if (nMatch >= gFip.groups(gg).nMatchFirst) andalso _
           (nMatch < gFip.groups(gg).nMatchFirst + gFip.groups(gg).nMatchCount) then g = gg: exit for
    next
    if g = -1 then return false
    if gFip.groups(g).pDoc = 0 then return false
    if IsWindow( DocView(gFip.groups(g).pDoc, 0) ) = 0 then return false

    ' Deliberately NOT suppressed: one replace is one edit, and letting it go through the
    ' ordinary SCN_MODIFIED path means every other match in the file is remapped by the same
    ' code that handles the user's own typing. The replaced match is killed by that remap (a
    ' delete overlapping it), which is exactly right.
    dim as string sRep = wszReplace
    dim as HWND h = DocView(gFip.groups(g).pDoc, 0)
    SciExec( h, SCI_BEGINUNDOACTION, 0, 0 )
    SciExec( h, SCI_SETTARGETSTART, gFip.matches(nMatch).nStart, 0 )
    SciExec( h, SCI_SETTARGETEND, gFip.matches(nMatch).nStart + gFip.matches(nMatch).nLen, 0 )
    SciExec( h, SCI_REPLACETARGET, len(sRep), strptr(sRep) )
    SciExec( h, SCI_ENDUNDOACTION, 0, 0 )

    FindProject_ApplyIndicators( g )
    function = true
end function


function FindProject_ReplaceAll( byval wszReplace as DWSTRING ) as long
    if gFip.bValid = false then return 0
    dim as string sRep = wszReplace
    dim as long nDone = 0

    ' The hook is suppressed and the model updated by hand. Driving N replacements through
    ' SCN_MODIFIED would remap every match in the file N times over -- O(N^2) on a file with
    ' hundreds of hits.
    gFipSuppressRemap = true

    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc = 0 then continue for
        if IsWindow( DocView(gFip.groups(g).pDoc, 0) ) = 0 then continue for

        dim as HWND h = DocView(gFip.groups(g).pDoc, 0)
        dim as long m0 = gFip.groups(g).nMatchFirst
        dim as long mN = m0 + gFip.groups(g).nMatchCount - 1

        ' ONE undo action per FILE. Scintilla has no cross-document undo, so a project-wide
        ' replace is N separate undo entries by nature -- one per file, not one per match.
        SciExec( h, SCI_BEGINUNDOACTION, 0, 0 )

        ' BACKWARDS. Replacing the last match first leaves every earlier offset untouched, so
        ' no remapping is needed inside the loop at all.
        for i as long = mN to m0 step -1
            if gFip.matches(i).bDead then continue for
            SciExec( h, SCI_SETTARGETSTART, gFip.matches(i).nStart, 0 )
            SciExec( h, SCI_SETTARGETEND, gFip.matches(i).nStart + gFip.matches(i).nLen, 0 )
            SciExec( h, SCI_REPLACETARGET, len(sRep), strptr(sRep) )
            gFip.matches(i).bDead = true
            gFip.nLiveMatches -= 1
            nDone += 1
        next

        SciExec( h, SCI_ENDUNDOACTION, 0, 0 )
        FindProject_ClearIndicators( g )
    next

    gFipSuppressRemap = false

    ' The blocks stay: the point of showing them after a replace is to see what the files now
    ' say. Nothing matches the old phrase any more, which is why every match is dead.
    if gFipModelChanged <> 0 then gFipModelChanged()
    function = nDone
end function


' ========================================================================================
' A document is losing its Scintilla windows. Drop its results.
'
' Called from clsDocument.DestroyScintillaWindows -- the only choke point that covers all
' three routes. clsApp.RemoveAllDocuments does NOT call RemoveDocument (project close goes
' that way), and ReloadDocument destroys the Scintilla document while the clsDocument ptr
' SURVIVES, which the file watcher triggers with no user action at all. Hooking the two
' clsApp functions would miss both.
'
' The group is emptied rather than removed from the array: blocks and rows address groups by
' index, so compacting here would invalidate every one of them at once. FipRows_Build skips a
' group whose pDoc is null, so the file simply disappears from the results -- which is the
' right answer, since its excerpts had nothing left to show.
' ========================================================================================
sub FindProject_OnDocumentClosing( byval pDocGone as clsDocument ptr )
    if pDocGone = 0 then exit sub
    if gFip.bValid = false then exit sub

    dim as boolean bChanged = false
    dim as HWND hMsg = FindProject_Messenger()

    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc <> pDocGone then continue for

        if gFip.groups(g).pSciDoc then
            ' Views first, then the reference -- the same order Clear uses, and for the same
            ' reason: a slot must never be left pointing at a released document.
            if gFipUnbindDoc <> 0 then gFipUnbindDoc( gFip.groups(g).pSciDoc )
            if hMsg then
                SciExec( hMsg, SCI_RELEASEDOCUMENT, 0, cast(LPARAM, gFip.groups(g).pSciDoc) )
            end if
        end if

        gFip.groups(g).pDoc    = 0
        gFip.groups(g).pSciDoc = 0
        bChanged = true
    next

    ' Tell the surface to rebuild. It cannot be called directly from here -- this file is
    ' included long before it -- so it hands us this callback when it is created.
    if bChanged andalso (gFipModelChanged <> 0) then gFipModelChanged()
end sub


' ========================================================================================
' Move the model across one document edit.
'
' Called from the TOP of frmMain_OnNotify. It has to be above that function's
' active-document gate: with the Find in Project tab selected GetActiveDocumentPtr answers
' null, so anything inside the gate never runs -- which is exactly the state an edit made in
' an excerpt happens in.
' ========================================================================================
sub FindProject_OnDocumentModified( byval pDocMod as clsDocument ptr, _
                                    byval nModType as long, _
                                    byval nModPos as long, byval nModLen as long, _
                                    byval nLinesAdded as long )
    if gFipSuppressRemap then exit sub
    if gFip.bValid = false then exit sub
    if pDocMod = 0 then exit sub
    if (nModType and (SC_MOD_INSERTTEXT or SC_MOD_DELETETEXT)) = 0 then exit sub

    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc <> pDocMod then continue for

        ' ONE message per edit, not one per match: the line the edit happened on is needed to
        ' decide which blocks move and which grow, and asking per match would put a message
        ' storm on every keystroke in a file with hundreds of hits.
        dim as long nEditLine = 0
        if IsWindow( DocView(pDocMod, 0) ) then
            nEditLine = SciExec( DocView(pDocMod, 0), SCI_LINEFROMPOSITION, nModPos, 0 )

            ' (Nothing is marked here. Whether a file is dirty is DERIVED, by asking the
            '  document -- see FindProject_IsGroupDirty -- so an edit that is undone stops
            '  being dirty by itself. A flag set here could only ever say "this was typed in
            '  at some point", which stays true after an undo.)

            ' The tab's dirty circle and the header's are painted from live state, so both
            ' need a repaint -- nothing else invalidates them on a keystroke made inside an
            ' excerpt (the excerpt repaints itself, but its file's header is drawn OUTSIDE it,
            ' on the surface).
            '
            ' A REPAINT, NOT A REBUILD. Calling the model-changed callback here rebuilt the
            ' row list, which hides and re-binds every pooled view -- and hiding the focused
            ' window destroys the focus, so the excerpt lost the caret after every single
            ' character typed into it. Measured.
            if IsWindow( gTTabCtl.hWindow ) then AfxRedrawWindow( gTTabCtl.hWindow )
            if gFipRepaint <> 0 then gFipRepaint()
        end if

        dim as long m0 = gFip.groups(g).nMatchFirst
        dim as long mN = m0 + gFip.groups(g).nMatchCount - 1

        for i as long = m0 to mN
            if gFip.matches(i).bDead then continue for

            dim as long nOrigStart = gFip.matches(i).nStart
            dim as long nNewStart, nNewLen
            if FindProject_RemapMatch( gFip.matches(i).nStart, gFip.matches(i).nLen, _
                                       nModType, nModPos, nModLen, nNewStart, nNewLen ) then
                gFip.matches(i).nStart = nNewStart
                gFip.matches(i).nLen   = nNewLen
                ' Lines inserted or removed above a match move it down or up. Decided on the
                ' ORIGINAL position, since nStart has already been rewritten.
                if (nLinesAdded <> 0) andalso (nOrigStart >= nModPos) then
                    gFip.matches(i).nLine += nLinesAdded
                    if gFip.matches(i).nLine < 0 then gFip.matches(i).nLine = 0
                end if
            else
                gFip.matches(i).bDead = true
                gFip.nLiveMatches -= 1
            end if
        next

        ' Blocks: one that CONTAINS the edited line grows or shrinks by the line delta -- a
        ' newline typed inside an excerpt genuinely makes it one line taller. One that sits
        ' entirely BELOW simply moves. Anything above is untouched.
        if nLinesAdded <> 0 then
            dim as long b0 = gFip.groups(g).nBlockFirst
            dim as long bN = b0 + gFip.groups(g).nBlockCount - 1
            for b as long = b0 to bN
                dim as long nBFirst = gFip.blocks(b).nFirstLine
                dim as long nBLast  = nBFirst + gFip.blocks(b).nLineCount - 1
                if nBFirst > nEditLine then
                    gFip.blocks(b).nFirstLine += nLinesAdded
                    if gFip.blocks(b).nFirstLine < 0 then gFip.blocks(b).nFirstLine = 0
                elseif nBLast >= nEditLine then
                    gFip.blocks(b).nLineCount += nLinesAdded
                    if gFip.blocks(b).nLineCount < 1 then gFip.blocks(b).nLineCount = 1
                end if
            next
        end if
    next
end sub
