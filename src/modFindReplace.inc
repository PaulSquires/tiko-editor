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

#include once "frmTopTabs.bi"


' ========================================================================================
' THE ONE DOOR for the Find/Replace bars' visibility.
' ========================================================================================
' Sets the flags, repositions, and restores focus. Every mutator goes through here, so a
' caller can no longer set state that nothing acts on -- which used to present as "the
' menu item did nothing", because frmMain_PositionWindows is the only code that turns
' these flags into windows and it was the CALLER's job to remember to run it.
'
' REPLACE IMPLIES FIND. A visible Replace bar with no Find bar above it is not a state the
' layout has; enforcing it in one place beats trusting five call sites to agree.
'
' IDEMPOTENT, AND THAT IS THE POINT OF THE EARLY RETURN. OnCommand_FileClose and
' OnCommand_ProjectClose both call the hide path unconditionally, on paths where nothing is
' open. That used to walk every open document, run a full re-layout, and -- the part worth
' avoiding -- call frmMain_SetFocusToCurrentCodeWindow while that very window was being
' destroyed.
function FindReplace_SetVisible( byval bFind as boolean, byval bReplace as boolean ) as long
    ' The bars are created once at startup, but WM_NCDESTROY zeroes these globals. Without
    ' the guard every GetDlgItem below silently returns 0 and the work is done on nothing.
    if IsWindow(HWND_FRMMAIN_FIND) = 0 then return 0
    if IsWindow(HWND_FRMMAIN_REPLACE) = 0 then return 0

    if bReplace then bFind = true

    if (gFind.bShowFindPanel = bFind) andalso (gFind.bShowReplacePanel = bReplace) then return 0

    dim as boolean bClosing = (gFind.bShowFindPanel = true) andalso (bFind = false)

    gFind.bShowInfoPanel    = true
    gFind.bShowFindPanel    = bFind
    gFind.bShowReplacePanel = bReplace

    if bClosing then
        ' Drop the search highlights AND the Selection latch. Leaving nSelection set was
        ' how a closed-and-reopened Find bar came back with Selection lit against a marker
        ' range that no longer described anything -- visible now that the button paints
        ' from this model rather than from a control's own state.
        gFind.nSelection = false
        dim as clsDocument ptr pDoc
        for i as integer = 0 to gTTabCtl.GetItemCount() - 1
            pDoc = gTTabCtl.GetpDoc(i)
            if pDoc = null then continue for
            pDoc->RemoveMarkerHighlight()
            pDoc->RemoveSelectionAttributes()
        next
    end if

    frmMain_PositionWindows()

    ' Only on the closing edge: opening the bars puts focus in the find field (the caller's
    ' job), and a no-op call must not move focus at all.
    if bClosing then frmMain_SetFocusToCurrentCodeWindow()

    return 0
end function


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Find controls.
' ========================================================================================
' Leaves the Replace bar exactly as it found it -- this used to clear bExpanded while
' leaving bShowReplacePanel set, which is how Ctrl+F followed by Ctrl+H became a keypress
' that did nothing.
function FindControls_Show() as long
    if IsWindow(HWND_FRMMAIN_FIND) = 0 then return 0

    ' pDoc can be null when the Find panel is being shown for Project Search.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        dim as boolean isOriginalSelectionMultiline = _
                        cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
        if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
        
        ' THE FIELD ALWAYS SHOWS THE SELECTION, INCLUDING WHEN THERE ISN'T ONE. Opening Find
        ' with nothing selected clears the box rather than re-offering the last phrase, which
        ' is by request: the box is a picture of the selection, so an empty selection is an
        ' empty box and Ctrl+F is always ready to be typed into. There used to be a bSeedField
        ' flag here that skipped the write in exactly that case; it is gone, and reinstating
        ' it puts the old term back.
        '
        ' The multi-line case clears the field for a DIFFERENT reason and is kept separate:
        ' it means "search within this selection", which starts with no term by definition.
        dim as string sFindText = pDoc->GetSelText
        if isOriginalSelectionMultiline then
            sFindText = ""
            gFind.nSelection = true
            pDoc->SetMarkerHighlight
        end if
        ' One assignment for both paths, so the box and the model cannot disagree about what
        ' is being searched for -- the multi-line branch used to clear the box and leave
        ' gFind.txtFind holding the previous phrase.
        gFind.txtFind = sFindText

        dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
        if sFindText <> AfxGetWindowText(hCtl) then
            AfxSetWindowText( hCtl, sFindText )
            dim as long nLen = len(sFindText)
            Edit_SetSel( hCtl, nLen, nLen )
        end if
    end if
        
    return FindReplace_SetVisible( true, gFind.bShowReplacePanel )
end function


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Replace controls.
' ========================================================================================
function ReplaceControls_Show() as long
    if IsWindow(HWND_FRMMAIN_REPLACE) = 0 then return 0

    ' Display the Replace controls
    AfxSetWindowText( GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE ), "" )
    ' This is the ONLY writer of bProjectReplaceActive, which frmMain_PositionWindows reads
    ' to keep the Replace bar open on the Find in Project tab. Without it that read is a
    ' permanently-false disjunct.
    if gTTabCtl.IsFindTab( gTTabCtl.GetCurSel() ) then
        gFind.bProjectReplaceActive = true
    end if
    return FindReplace_SetVisible( true, true )
end function


' ========================================================================================
' Hide the Find/Replace controls and collapse the frmTopTabsInfo area.
' ========================================================================================
function FindReplaceControls_Hide() as long
    return FindReplace_SetVisible( false, false )
end function


' ========================================================================================
' Hide the frmTopTabsInfo panel.
' ========================================================================================
function TopTabsInfoPanel_Hide() as long
    gFind.bShowInfoPanel = false
    return 0
end function


' ========================================================================================
' Show the frmTopTabsInfo panel.
' ========================================================================================
function TopTabsInfoPanel_Show() as long
    gFind.bShowInfoPanel = true
    return 0
end function


' ========================================================================================
' Determine if the incoming string is uppercase
' ========================================================================================
function isUpperCaseString( byval sText as string ) as boolean
    ' Default that the string is uppercase
    function = true
    for i as long = 0 to len(sText) - 1
        if isupper( sText[i] ) = 0 then return false
    next
end function

' ========================================================================================
' Determine if the incoming string is lowercase
' ========================================================================================
function isLowerCaseString( byval sText as string ) as boolean
    ' Default that the string is lowercase
    function = true
    for i as long = 0 to len(sText) - 1
        if islower( sText[i] ) = 0 then return false
    next
end function


' The start position of every highlighted match in the active document, in document order,
' 1-based. Filled by FindReplace_CollectMatchPositions and valid only until the next call.
' Module-level rather than a byref array parameter because it is redim'd as it grows.
dim shared gFindMatchPos(any) as long


' ========================================================================================
' Collect the start position of every indicator-8 match up to endPos. Returns the count.
' ========================================================================================
' Lifted verbatim out of FindReplace_NextSelection so that the navigation path and the
' display path cannot come to disagree about what "a match" is or where it starts. The
' (iStart - lenFind) is that function's own convention and is preserved exactly: the walk
' reads the indicator range END and steps back over the search term.
private function FindReplace_CollectMatchPositions( _
            byval hEdit as HWND, _
            byval endPos as long, _
            byval lenFind as long _
            ) as long

    dim as long nPos, iStart, iEnd, nCount

    redim gFindMatchPos(100) as long

    while nPos <= endPos
        iStart = SciExec( hEdit, SCI_INDICATORSTART, 8, nPos)
        iEnd   = SciExec( hEdit, SCI_INDICATOREND, 8, iStart)
        if (iStart = 0) andalso (iEnd = 0) then exit while    ' no indicators found

        if SciExec( hEdit, SCI_INDICATORVALUEAT, 8, iStart-1) then
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


' ========================================================================================
' Refresh the "current / total" count from where the caret actually is. Moves nothing.
' ========================================================================================
' WHY THIS EXISTS: FindReplace_HighlightSearches used to hardcode the numerator to zero
' ("0/" & foundCount), because scanning for matches and knowing which one you are ON are
' two different questions and only FindReplace_NextSelection ever answered the second. So
' every path that re-highlighted WITHOUT navigating displayed 0/N -- switching tabs (the
' visible one: a document you had already searched came up as 0/12), toggling Match Case,
' toggling Whole Word, toggling Selection.
'
' The rule is the one other editors use: the match CONTAINING the caret, else the next one
' after it, wrapping to the first when the caret is past the last. That last clause is not
' arbitrary -- it is what F3 will do from here, so the number and the next keypress agree.
' A caret sitting inside match 2 of 5 therefore reads "2/5" and F3 goes to 3, which is the
' behaviour to keep if this is ever revisited.
'
' DISPLAY ONLY. It never repositions the caret and never writes gFind.foundCount: the
' denominator stays whatever FindReplace_HighlightSearches counted (that is the number
' OnCommand_SearchReplaceActions gates on), so a disagreement between the target search and
' the indicator walk can only ever show a zero numerator, never a wrong total.
function FindReplace_UpdateResultsFromCaret() as long

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as long lenFind = len(gFind.txtFind)
    dim as long endPos

    ' Same "selection or whole document" question FindReplace_NextSelection asks.
    if (pDoc->CurrentSelection.isInitialized = true) andalso (gFind.nSelection = true) then
        endPos = SciExec( hEdit, SCI_GETLINEENDPOSITION, pDoc->CurrentSelection.endpos, 0)
    else
        endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)
    end if

    dim as long nCount = 0
    if (lenFind > 0) andalso (endPos > 0) then
        nCount = FindReplace_CollectMatchPositions( hEdit, endPos, lenFind )
    end if

    dim as long caretPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
    dim as long idx = 0
    for i as long = 1 to nCount
        ' The match END, so a caret anywhere inside a match -- or at either edge of it --
        ' reports that match rather than the following one.
        if (gFindMatchPos(i) + lenFind) >= caretPos then
            idx = i
            exit for
        end if
    next
    if (idx = 0) andalso (nCount > 0) then idx = 1     ' past the last match: wraps, as F3 does

    if idx > gFind.foundCount then idx = gFind.foundCount

    gFind.wszResults = idx & "/" & gFind.foundCount

    AfxRedrawWindow( HWND_FRMMAIN_FIND )

    function = 0
end function


' ========================================================================================
' Find next/prev selection based on current document position
' ========================================================================================
function FindReplace_NextSelection( _
            byval startPos as long, _
            byval bGetNext as boolean, _
            byval bRepositionCaret as boolean _
            ) as boolean

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as long mainIdx, lenFind, endPos

    dim as hwnd hEdit = pDoc->hWndActiveScintilla

    dim txtFind as string = gFind.txtFind
    lenFind = len(txtFind)

    ' Are we searching a selection or the whole document
    if (pDoc->CurrentSelection.isInitialized = true) andalso (gFind.nSelection = true) then   
        endPos = SciExec( hEdit, SCI_GETLINEENDPOSITION, pDoc->CurrentSelection.endpos, 0)
    else
        endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)
    end if

    if endPos = 0 then exit function
 
    ' Build an array holding all of the positions. Shared with the display path
    ' (FindReplace_UpdateResultsFromCaret) so the two cannot disagree about the matches.
    gFind.foundCount = FindReplace_CollectMatchPositions( hEdit, endPos, lenFind )

    ' Determine the next/prev selection that we should position to.
    if bGetNext then
        ' next selection
        mainIdx = 0
        for i as long = 1 to gFind.foundCount
            if gFindMatchPos(i) > startPos then 
                mainIdx = i:
                exit for
            end if
        next

        ' If searching past end of buffer without a match then wrap around
        ' to the first selection (mainIdx = 1)
        if mainIdx = 0 then mainIdx = 1
    else
        ' Previous selection 
        mainIdx = 0
        for i as long = 1 to gFind.foundCount
            if gFindMatchPos(i) >= (startPos - lenFind) then exit for
            mainIdx = i
        next
        ' If searching past start of buffer without a match then wrap around
        ' to the last selection.
        if mainIdx = 0 then mainIdx = gFind.foundCount 
    end if

    if (mainIdx > 0) andalso (gFind.foundCount > 0) then
        gFind.wszResults = mainIdx & "/" & gFind.foundCount 
        ' make sure target line is unfolded
        if bRepositionCaret then
            dim as long currentLine = SciExec( hEdit, SCI_LINEFROMPOSITION, gFindMatchPos(mainIdx), 0)
            SciExec( hEdit, SCI_ENSUREVISIBLE, currentLine, 0) 
            SciExec( hEdit, SCI_SETSEL, gFindMatchPos(mainIdx), gFindMatchPos(mainIdx) + lenFind)
        end if
    else    
        SciExec( hEdit, SCI_SETSEL, startPos, startPos)
        gFind.wszResults = "0/0"
    end if
    
    AfxRedrawWindow( HWND_FRMMAIN_FIND )
    
    return iif(mainIdx > 0, true, false)
    
end function


' ========================================================================================
' Highlight the found selections
' ========================================================================================
function FindReplace_HighlightSearches() as long
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    
    dim as long r, startPos, endPos, findFlags, nLength

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    SciExec( hEdit, SCI_SETADDITIONALCARETSVISIBLE, false, 0) 
    SciExec( hEdit, SCI_INDICSETSTYLE, 8, INDIC_STRAIGHTBOX)
    SciExec( hEdit, SCI_SETINDICATORCURRENT, 8, 0)
    SciExec( hEdit, SCI_INDICSETFORE, 8, theme.editor.occurrence ) 
    SciExec( hEdit, SCI_INDICSETALPHA, 8, 90 )   
    
    dim txtFind as string = gFind.txtFind

    ' Remove all existing selection indicators
    pDoc->RemoveSelectionAttributes()
        
    ' if Find or Replace is open then use their flags also.
    if (gFind.bShowFindPanel = true) orelse (gFind.bShowReplacePanel = true) then
        if gFind.nMatchCase then findFlags = (findFlags or SCFIND_MATCHCASE)
        if gFind.nWholeWord then findFlags = (findFlags or SCFIND_WHOLEWORD)
        SciExec( hEdit, SCI_SETSEARCHFLAGS, findFlags, 0)
    end if
            
    ' Are we searching a selection or the whole document
    if pDoc->HasMarkerHighlight then
        startPos = SciExec( hEdit, SCI_POSITIONFROMLINE, pDoc->FirstMarkerHighlight, 0)
        endPos = SciExec( hEdit, SCI_GETLINEENDPOSITION, pDoc->LastMarkerHighlight, 0)
    else
        startPos = 0
        endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)
    end if

    if len(txtFind) then
        gFind.foundCount = 0
        do
            SciExec( hEdit, SCI_SETTARGETSTART, startPos, 0)
            SciExec( hEdit, SCI_SETTARGETEND, endPos, 0)
            r = SciExec( hEdit, SCI_SEARCHINTARGET, len(txtFind), strptr(txtFind))

            if r = -1 then exit do
            
            SciExec( hEdit, SCI_SETINDICATORVALUE, 8, 0 )
            SciExec( hEdit, SCI_INDICATORFILLRANGE, r, len(txtFind))
            
            gFind.foundCount += 1

            ' Move start position after the text
            startPos = SendMessage(hEdit, SCI_GETTARGETEND, 0, 0)
        loop

        ' The numerator used to be hardcoded to 0 here, so every path that re-highlighted
        ' without navigating displayed 0/N -- most visibly a tab switch, which lands in
        ' this function (via clsTopTabCtl.SetFocusTab) and nowhere near the navigation
        ' code that knows which match is current.
        FindReplace_UpdateResultsFromCaret()
    else
        ' An empty search term is not "the previous term's count, still on screen".
        gFind.foundCount = 0
        gFind.wszResults = "0/0"
        AfxRedrawWindow( HWND_FRMMAIN_FIND )
    end if

    function = 0

end function


' ========================================================================================
' Replace current selection or all selections
' ========================================================================================
function FindReplace_DoReplace( _
            byval fReplaceAll as boolean = false, _
            byval fMovenext as boolean = true _
            ) as long

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as HWND   hWndFind    = GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
    dim as HWND   hWndReplace = GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE)
    dim as string txtFind     = str(AfxGetWindowText(hWndFind))
    dim as string txtReplace  = str(AfxGetWindowText(hWndReplace))
    dim as long   lenFind     = len(txtFind)

    if lenFind = 0 then exit function
    
    dim as long nPos, startPos, endPos, findFlags, r
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if fReplaceAll = true then 
        if gFind.nMatchCase then findFlags = (findFlags or SCFIND_MATCHCASE)
        if gFind.nWholeWord then findFlags = (findFlags or SCFIND_WHOLEWORD)
        SendMessage( hEdit, SCI_SETSEARCHFLAGS, findFlags, 0)
        
        if pDoc->HasMarkerHighlight then
            startPos = SciExec( hEdit, SCI_POSITIONFROMLINE, pDoc->FirstMarkerHighlight, 0)
        end if
        SciExec( hEdit, SCI_INDICATORCLEARRANGE, startPos, endPos)
        
        gApp.SuppressNotify = true
        SetWindowRedraw( hEdit, false )
        SendMessage( hEdit, SCI_BEGINUNDOACTION, 0, 0)
        do 
            if pDoc->HasMarkerHighlight then
                endPos = SciExec( hEdit, SCI_GETLINEENDPOSITION, pDoc->LastMarkerHighlight, 0)
            else    
                ' Search the whole document. Get new length in case changed due to replacements.
                endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)
            end if    
            
            SciExec( hEdit, SCI_SETTARGETSTART, startPos, 0)
            SciExec( hEdit, SCI_SETTARGETEND, endPos, 0)
            r = SciExec( hEdit, SCI_SEARCHINTARGET, len(txtFind), strptr(txtFind))
            if r = -1 then exit do
            dim as zstring * MAX_PATH sSelText
            SciExec( hEdit, SCI_GETTARGETTEXT, 0, @sSelText )
            ' If Preserve Case is active then check the selection to see if the entire
            ' selection is all UPPERCASE or all lowercase.
            if gFind.nPreserve then
                dim as string sReplaceText
                if isUpperCaseString( sSelText ) then
                    sReplaceText = ucase(txtReplace)
                elseif isLowerCaseString( sSelText ) then
                    sReplaceText = lcase(txtReplace)
                else
                    sReplaceText = txtReplace
                end if
                SciExec( hEdit, SCI_REPLACETARGET, len(sReplaceText), strptr(sReplaceText) )
            else            
                SciExec( hEdit, SCI_REPLACETARGET, len(txtReplace), strptr(txtReplace) )
            end if
            
            ' Move start position after the replaced text
            startPos = SendMessage(hEdit, SCI_GETTARGETEND, 0, 0)
        loop
        SendMessage( hEdit, SCI_ENDUNDOACTION, 0, 0)
        SetWindowRedraw( hEdit, true )
        gApp.SuppressNotify = false
        startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0) 
        FindReplace_NextSelection(startPos, fMoveNext, true)
        AfxRedrawWindow( hEdit )

    else   
        ' Need to ensure that the current cursor/selection is actually the find
        ' phrase that we want to replace. if not, then move to the closest selection.
        if ucase(pDoc->GetSelText) <> ucase(txtFind) then
            startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0) 
            FindReplace_NextSelection(startPos, fMoveNext, true)
            exit function
        end if
        SciExec( hEdit, SCI_TARGETFROMSELECTION, 0, 0)
        ' If Preserve Case is active then check the selection to see if the entire
        ' selection is all UPPERCASE or all lowercase.
        if gFind.nPreserve then
            dim as string sSelText = pDoc->GetSelText
            dim as string sReplaceText
            if isUpperCaseString( sSelText ) then
                sReplaceText = ucase(txtReplace)
            elseif isLowerCaseString( sSelText ) then
                sReplaceText = lcase(txtReplace)
            else
                sReplaceText = txtReplace
            end if
            SciExec( hEdit, SCI_REPLACETARGET, len(sReplaceText), strptr(sReplaceText) )
        else            
            SciExec( hEdit, SCI_REPLACETARGET, len(txtReplace), strptr(txtReplace) )
        end if
        startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0) 
        FindReplace_NextSelection(startPos, fMoveNext, true)
    end if
        
    ' Need to rescan because some of the function names may have been replaced
    ' with new names.
    gScanMgr.RequestBufferScan( pDoc )

    function = 0
end function


