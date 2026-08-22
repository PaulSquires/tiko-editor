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

'' ========================================================================================
'' The search itself. See app/modFindReplace.bi for what moved and why.
''
'' ---- THE THREE SHELL REACHES, AND WHERE EACH ONE WENT ---------------------------------
''
''   gTTabCtl.GetActiveDocumentPtr()   ->  the seam. gAppHost.TabDocAt(TabActiveIndex()),
''                                         which 7c step 13 added for exactly this: the app
''                                         layer asking which document is active without
''                                         knowing what a tab is.
''
''   pDoc->hWndActiveScintilla (HWND)  ->  pDoc->GetActiveScintillaPtr(), which has been an
''                                         `any ptr` since step 3 and says so in its own
''                                         header. SciMsg takes it directly.
''
''   AfxRedrawWindow(HWND_FRMMAIN_FIND)->  gAppNotify.RefreshFindBar(), a new notify field.
''                                         FIRE-AND-FORGET, like every other Notify: a
''                                         binary with no find bar leaves it empty and is
''                                         CORRECT, which is the whole distinction between
''                                         Notify and Services.
''
'' SetWindowRedraw around the bulk replace went to gAppHost.SetViewRedraw, which already
'' existed and had been waiting for a caller.
'' ========================================================================================

#pragma once

#include once "app/modFindReplace.bi"
#include once "app/modScintilla.bi"
#include once "app/modAppHost.bi"
#include once "app/clsApp.bi"


'' The document the search acts on, or NULL. One place, so the six functions below cannot
'' come to disagree about what "the active document" means.
private function FindReplace_ActiveDoc() as clsDocument ptr
    if gAppHost.TabActiveIndex = 0 then return 0
    if gAppHost.TabDocAt = 0 then return 0
    return gAppHost.TabDocAt( gAppHost.TabActiveIndex() )
end function


'' ========================================================================================
'' Is the whole string upper- / lowercase?
'' ========================================================================================
'' ---- ASCII, TESTED INLINE, WHERE THIS USED THE CRT'S isupper/islower -------------------
''
'' Those are not declared in the app layer -- tiko reaches them through AfxNova's headers,
'' which is precisely what this layer does not have. Two ways out: include crt/ctype.bi, or
'' ask the question directly. The second, for a reason worth the lines:
''
'' isupper AND islower ARE LOCALE-DEPENDENT. Their answer for anything above 0x7F changes
'' with whatever setlocale was last called with, and this layer already folds ASCII ONLY
'' everywhere else -- PsUCase does, and SymDb_NameEqW says so at length. A Preserve Case
'' replace that behaved differently depending on the C locale would be the odd one out, and
'' the difference would show up as "replace mangled my accented text" on one machine.
''
'' A NON-ASCII CHARACTER IS THEREFORE NEITHER UPPER NOR LOWER, so a string containing one
'' falls to the "mixed" arm and is replaced verbatim. That is the safe answer and it is what
'' the ASCII fold everywhere else in the layer already implies.
function isUpperCaseString( byval sText as string ) as boolean
    function = true
    for i as long = 0 to len(sText) - 1
        dim as ulong ch = sText[i]
        if (ch < 65) orelse (ch > 90) then return false
    next
end function

function isLowerCaseString( byval sText as string ) as boolean
    function = true
    for i as long = 0 to len(sText) - 1
        dim as ulong ch = sText[i]
        if (ch < 97) orelse (ch > 122) then return false
    next
end function


'' ========================================================================================
'' Collect the start position of every indicator-8 match up to endPos. Returns the count.
'' ========================================================================================
'' Lifted verbatim out of FindReplace_NextSelection so that the navigation path and the
'' display path cannot come to disagree about what "a match" is or where it starts. The
'' (iStart - lenFind) is that function's own convention and is preserved exactly: the walk
'' reads the indicator range END and steps back over the search term.
private function FindReplace_CollectMatchPositions( _
            byval pEdit as any ptr, _
            byval endPos as long, _
            byval lenFind as long _
            ) as long

    dim as long nPos, iStart, iEnd, nCount

    redim gFindMatchPos(100) as long

    while nPos <= endPos
        iStart = SciMsg( pEdit, SCI_INDICATORSTART, 8, nPos)
        iEnd   = SciMsg( pEdit, SCI_INDICATOREND, 8, iStart)
        if (iStart = 0) andalso (iEnd = 0) then exit while    '' no indicators found

        if SciMsg( pEdit, SCI_INDICATORVALUEAT, 8, iStart-1) then
            nCount = nCount + 1
            if nCount > ubound(gFindMatchPos) then
                redim preserve gFindMatchPos(nCount) as long
            end if
            gFindMatchPos(nCount) = iStart - lenFind
        end if
        nPos = iEnd + 1
    wend

    return nCount
end function


'' ========================================================================================
'' Refresh the "current / total" count from where the caret actually is. Moves nothing.
'' ========================================================================================
'' WHY THIS EXISTS: FindReplace_HighlightSearches used to hardcode the numerator to zero
'' ("0/" & foundCount), because scanning for matches and knowing which one you are ON are
'' two different questions and only FindReplace_NextSelection ever answered the second. So
'' every path that re-highlighted WITHOUT navigating displayed 0/N -- switching tabs (the
'' visible one: a document you had already searched came up as 0/12), toggling Match Case,
'' toggling Whole Word, toggling Selection.
''
'' The rule is the one other editors use: the match CONTAINING the caret, else the next one
'' after it, wrapping to the first when the caret is past the last. That last clause is not
'' arbitrary -- it is what F3 will do from here, so the number and the next keypress agree.
'' A caret sitting inside match 2 of 5 therefore reads "2/5" and F3 goes to 3, which is the
'' behaviour to keep if this is ever revisited.
''
'' DISPLAY ONLY. It never repositions the caret and never writes gFind.foundCount: the
'' denominator stays whatever FindReplace_HighlightSearches counted, so a disagreement
'' between the target search and the indicator walk can only ever show a zero numerator,
'' never a wrong total.
function FindReplace_UpdateResultsFromCaret() as long

    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit function

    dim as any ptr pEdit = pDoc->GetActiveScintillaPtr()
    if pEdit = 0 then exit function
    dim as long lenFind = PsLen(gFind.txtFind)
    dim as long endPos

    '' Same "selection or whole document" question FindReplace_NextSelection asks.
    if (pDoc->CurrentSelection.isInitialized = true) andalso (gFind.nSelection = true) then
        endPos = SciMsg( pEdit, SCI_GETLINEENDPOSITION, pDoc->CurrentSelection.endpos, 0)
    else
        endPos = SciMsg( pEdit, SCI_GETLENGTH, 0, 0)
    end if
    if endPos = 0 then exit function
    if lenFind = 0 then exit function

    dim as long nCount = FindReplace_CollectMatchPositions( pEdit, endPos, lenFind )
    if nCount = 0 then exit function

    dim as long caretPos = SciMsg( pEdit, SCI_GETCURRENTPOS, 0, 0)

    dim as long mainIdx = 0
    for i as long = 1 to nCount
        if (caretPos >= gFindMatchPos(i)) andalso (caretPos <= gFindMatchPos(i) + lenFind) then
            mainIdx = i : exit for
        end if
        if gFindMatchPos(i) > caretPos then mainIdx = i : exit for
    next
    if mainIdx = 0 then mainIdx = 1

    gFind.wszResults = mainIdx & "/" & gFind.foundCount
    if gAppNotify.RefreshFindBar then gAppNotify.RefreshFindBar()

    function = 0
end function


'' ========================================================================================
'' Move to the next or previous match.
'' ========================================================================================
function FindReplace_NextSelection( _
            byval startPos as long, _
            byval bGetNext as boolean, _
            byval bRepositionCaret as boolean _
            ) as boolean

    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit function

    dim as long mainIdx, lenFind, endPos

    dim as any ptr pEdit = pDoc->GetActiveScintillaPtr()
    if pEdit = 0 then exit function

    dim txtFind as string = gFind.txtFind.Utf8
    lenFind = PsLen(txtFind)

    '' Are we searching a selection or the whole document
    if (pDoc->CurrentSelection.isInitialized = true) andalso (gFind.nSelection = true) then
        endPos = SciMsg( pEdit, SCI_GETLINEENDPOSITION, pDoc->CurrentSelection.endpos, 0)
    else
        endPos = SciMsg( pEdit, SCI_GETLENGTH, 0, 0)
    end if

    if endPos = 0 then exit function

    '' Build an array holding all of the positions. Shared with the display path
    '' (FindReplace_UpdateResultsFromCaret) so the two cannot disagree about the matches.
    gFind.foundCount = FindReplace_CollectMatchPositions( pEdit, endPos, lenFind )

    '' Determine the next/prev selection that we should position to.
    if bGetNext then
        '' next selection
        mainIdx = 0
        for i as long = 1 to gFind.foundCount
            if gFindMatchPos(i) > startPos then
                mainIdx = i:
                exit for
            end if
        next

        '' If searching past end of buffer without a match then wrap around
        '' to the first selection (mainIdx = 1)
        if mainIdx = 0 then mainIdx = 1
    else
        '' Previous selection
        mainIdx = 0
        for i as long = 1 to gFind.foundCount
            if gFindMatchPos(i) >= (startPos - lenFind) then exit for
            mainIdx = i
        next
        '' If searching past start of buffer without a match then wrap around
        '' to the last selection.
        if mainIdx = 0 then mainIdx = gFind.foundCount
    end if

    if (mainIdx > 0) andalso (gFind.foundCount > 0) then
        gFind.wszResults = mainIdx & "/" & gFind.foundCount
        '' make sure target line is unfolded
        if bRepositionCaret then
            dim as long currentLine = SciMsg( pEdit, SCI_LINEFROMPOSITION, gFindMatchPos(mainIdx), 0)
            SciMsg( pEdit, SCI_ENSUREVISIBLE, currentLine, 0)
            SciMsg( pEdit, SCI_SETSEL, gFindMatchPos(mainIdx), gFindMatchPos(mainIdx) + lenFind)
        end if
    else
        SciMsg( pEdit, SCI_SETSEL, startPos, startPos)
        gFind.wszResults = "0/0"
    end if

    if gAppNotify.RefreshFindBar then gAppNotify.RefreshFindBar()

    return iif(mainIdx > 0, true, false)

end function


'' ========================================================================================
'' Highlight the found selections
'' ========================================================================================
function FindReplace_HighlightSearches( byval clrOccurrence as ulong ) as long

    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit function

    dim as long r, startPos, endPos, findFlags

    dim as any ptr pEdit = pDoc->GetActiveScintillaPtr()
    if pEdit = 0 then exit function
    SciMsg( pEdit, SCI_SETADDITIONALCARETSVISIBLE, false, 0)
    SciMsg( pEdit, SCI_INDICSETSTYLE, 8, INDIC_STRAIGHTBOX)
    SciMsg( pEdit, SCI_SETINDICATORCURRENT, 8, 0)
    SciMsg( pEdit, SCI_INDICSETFORE, 8, clrOccurrence )
    SciMsg( pEdit, SCI_INDICSETALPHA, 8, 90 )

    dim txtFind as string = gFind.txtFind.Utf8

    '' Remove all existing selection indicators
    pDoc->RemoveSelectionAttributes()

    '' if Find or Replace is open then use their flags also.
    if (gFind.bShowFindPanel = true) orelse (gFind.bShowReplacePanel = true) then
        if gFind.nMatchCase then findFlags = (findFlags or SCFIND_MATCHCASE)
        if gFind.nWholeWord then findFlags = (findFlags or SCFIND_WHOLEWORD)
        SciMsg( pEdit, SCI_SETSEARCHFLAGS, findFlags, 0)
    end if

    '' Are we searching a selection or the whole document
    if pDoc->HasMarkerHighlight then
        startPos = SciMsg( pEdit, SCI_POSITIONFROMLINE, pDoc->FirstMarkerHighlight, 0)
        endPos = SciMsg( pEdit, SCI_GETLINEENDPOSITION, pDoc->LastMarkerHighlight, 0)
    else
        startPos = 0
        endPos = SciMsg( pEdit, SCI_GETLENGTH, 0, 0)
    end if

    if PsLen(txtFind) then
        gFind.foundCount = 0
        do
            SciMsg( pEdit, SCI_SETTARGETSTART, startPos, 0)
            SciMsg( pEdit, SCI_SETTARGETEND, endPos, 0)
            r = SciMsg( pEdit, SCI_SEARCHINTARGET, PsLen(txtFind), cast(integer, strptr(txtFind)))

            if r = -1 then exit do

            SciMsg( pEdit, SCI_SETINDICATORVALUE, 8, 0 )
            SciMsg( pEdit, SCI_INDICATORFILLRANGE, r, PsLen(txtFind))

            gFind.foundCount += 1

            '' Move start position after the text
            startPos = SciMsg( pEdit, SCI_GETTARGETEND, 0, 0)
        loop

        '' The numerator used to be hardcoded to 0 here, so every path that re-highlighted
        '' without navigating displayed 0/N -- most visibly a tab switch, which lands in
        '' this function and nowhere near the navigation code that knows which match is
        '' current.
        FindReplace_UpdateResultsFromCaret()
    else
        '' An empty search term is not "the previous term's count, still on screen".
        gFind.foundCount = 0
        gFind.wszResults = "0/0"
        if gAppNotify.RefreshFindBar then gAppNotify.RefreshFindBar()
    end if

    function = 0

end function


'' ========================================================================================
'' Replace current selection or all selections
'' ========================================================================================
'' THE TEXT COMES FROM gFind, NOT FROM THE CONTROLS, and that is this move's one behaviour
'' change. It used to read the two text boxes through GetDlgItem + AfxGetWindowText, which
'' the app layer cannot do and should not want to: gFind.txtFind is written by the field's
'' own change handler, so the two agree by construction -- and where they ever did NOT, the
'' stored value is the one every other function in this file already used.
function FindReplace_DoReplace( _
            byval clrOccurrence as ulong, _
            byval fReplaceAll as boolean = false, _
            byval fMovenext as boolean = true _
            ) as long

    dim pDoc as clsDocument ptr = FindReplace_ActiveDoc()
    if pDoc = 0 then exit function

    dim as string txtFind    = gFind.txtFind.Utf8
    dim as string txtReplace = gFind.txtReplace.Utf8
    dim as long   lenFind    = PsLen(txtFind)

    if lenFind = 0 then exit function

    dim as long startPos, endPos, findFlags, r

    dim as any ptr pEdit = pDoc->GetActiveScintillaPtr()
    if pEdit = 0 then exit function

    if fReplaceAll = true then
        if gFind.nMatchCase then findFlags = (findFlags or SCFIND_MATCHCASE)
        if gFind.nWholeWord then findFlags = (findFlags or SCFIND_WHOLEWORD)
        SciMsg( pEdit, SCI_SETSEARCHFLAGS, findFlags, 0)

        if pDoc->HasMarkerHighlight then
            startPos = SciMsg( pEdit, SCI_POSITIONFROMLINE, pDoc->FirstMarkerHighlight, 0)
        end if
        SciMsg( pEdit, SCI_INDICATORCLEARRANGE, startPos, endPos)

        gApp.SuppressNotify = true
        if gAppHost.SetViewRedraw then gAppHost.SetViewRedraw( pEdit, false )
        SciMsg( pEdit, SCI_BEGINUNDOACTION, 0, 0)
        do
            if pDoc->HasMarkerHighlight then
                endPos = SciMsg( pEdit, SCI_GETLINEENDPOSITION, pDoc->LastMarkerHighlight, 0)
            else
                '' Search the whole document. Get new length in case changed due to replacements.
                endPos = SciMsg( pEdit, SCI_GETLENGTH, 0, 0)
            end if

            SciMsg( pEdit, SCI_SETTARGETSTART, startPos, 0)
            SciMsg( pEdit, SCI_SETTARGETEND, endPos, 0)
            r = SciMsg( pEdit, SCI_SEARCHINTARGET, PsLen(txtFind), cast(integer, strptr(txtFind)))
            if r = -1 then exit do
            dim as zstring * MAX_PATH sSelText
            SciMsg( pEdit, SCI_GETTARGETTEXT, 0, cast(integer, @sSelText) )
            '' If Preserve Case is active then check the selection to see if the entire
            '' selection is all UPPERCASE or all lowercase.
            if gFind.nPreserve then
                dim as string sReplaceText
                if isUpperCaseString( sSelText ) then
                    sReplaceText = PsUCase(txtReplace).Utf8
                elseif isLowerCaseString( sSelText ) then
                    sReplaceText = PsLCase(txtReplace).Utf8
                else
                    sReplaceText = txtReplace
                end if
                SciMsg( pEdit, SCI_REPLACETARGET, len(sReplaceText), cast(integer, strptr(sReplaceText)) )
            else
                SciMsg( pEdit, SCI_REPLACETARGET, PsLen(txtReplace), cast(integer, strptr(txtReplace)) )
            end if

            '' Move start position after the replaced text
            startPos = SciMsg( pEdit, SCI_GETTARGETEND, 0, 0)
        loop
        SciMsg( pEdit, SCI_ENDUNDOACTION, 0, 0)
        if gAppHost.SetViewRedraw then gAppHost.SetViewRedraw( pEdit, true )
        gApp.SuppressNotify = false
        startPos = SciMsg( pEdit, SCI_GETCURRENTPOS, 0, 0)
        FindReplace_NextSelection(startPos, fMoveNext, true)
        if gAppHost.InvalidateView then gAppHost.InvalidateView( pEdit )

    else
        '' Need to ensure that the current cursor/selection is actually the find
        '' phrase that we want to replace. if not, then move to the closest selection.
        if PsUCase(pDoc->GetSelText) <> PsUCase(txtFind) then
            startPos = SciMsg( pEdit, SCI_GETCURRENTPOS, 0, 0)
            FindReplace_NextSelection(startPos, fMoveNext, true)
            exit function
        end if
        SciMsg( pEdit, SCI_TARGETFROMSELECTION, 0, 0)
        '' If Preserve Case is active then check the selection to see if the entire
        '' selection is all UPPERCASE or all lowercase.
        if gFind.nPreserve then
            dim as string sSelText = pDoc->GetSelText
            dim as string sReplaceText
            if isUpperCaseString( sSelText ) then
                sReplaceText = PsUCase(txtReplace).Utf8
            elseif isLowerCaseString( sSelText ) then
                sReplaceText = PsLCase(txtReplace).Utf8
            else
                sReplaceText = txtReplace
            end if
            SciMsg( pEdit, SCI_REPLACETARGET, len(sReplaceText), cast(integer, strptr(sReplaceText)) )
        else
            SciMsg( pEdit, SCI_REPLACETARGET, PsLen(txtReplace), cast(integer, strptr(txtReplace)) )
        end if
        startPos = SciMsg( pEdit, SCI_GETCURRENTPOS, 0, 0)
        FindReplace_NextSelection(startPos, fMoveNext, true)
    end if

    '' Need to rescan because some of the function names may have been replaced
    '' with new names.
    if gAppNotify.RequestBufferScan then gAppNotify.RequestBufferScan( pDoc )

    function = 0
end function
