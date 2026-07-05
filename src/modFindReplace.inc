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


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Find controls.
' ========================================================================================
function FindControls_Show() as long
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as boolean isOriginalSelectionMultiline = _
                    cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
    if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
    
    dim as string sFindText = pDoc->GetSelText
    if isOriginalSelectionMultiline then
        sFindText = ""
        gFind.nSelection = true   
        pDoc->SetMarkerHighlight
    else
        gFind.txtFind = sFindtext
    end if   

    AfxSetWindowText( GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND ), sFindText )

    gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT_FIND
    gFind.bExpanded = false

    return 0
end function


' ========================================================================================
' Expand the frmTopTabsInfo area to allow for the Replace controls.
' ========================================================================================
function ReplaceControls_Show() as long
    ' Display the Replace controls
    AfxSetWindowText( GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE ), "" )
    gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT_REPLACE
    gFind.bExpanded = true
    return 0
end function


' ========================================================================================
' Hide the Find/Replace controls and collapse the frmTopTabsInfo area.
' ========================================================================================
function FindReplaceControls_Hide() as long
    gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT
    gFind.bExpanded = false

    ' Remove any Find Highlight selection indicators from open documents.
    for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs)
        if gTTabCtl.tabs(i).pDoc <> 0 then
            gTTabCtl.tabs(i).pDoc->RemoveSelectionAttributes()
        end if    
    next

    frmMain_PositionWindows()
    frmMain_SetFocusToCurrentCodeWindow()
    return 0
end function


' ========================================================================================
' Hide all parts of the frmTopTabsInfo panels.
' ========================================================================================
function TopTabsInfoPanel_Hide() as long
    gTTabCtl.InfoPanelHeight = 0

    ' Remove any Find Highlight selection indicators from open documents.
    for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs)
        if gTTabCtl.tabs(i).pDoc <> 0 then
            gTTabCtl.tabs(i).pDoc->RemoveSelectionAttributes()
        end if    
    next

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

    dim as long mainIdx, lenFind, endPos, nPos, iStart, iEnd
    
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
 
    ' Build an array holding all of the positions
    redim iPositions(100) as long
    
    gFind.foundCount = 0 
    while nPos <= endPos 
        iStart = SciExec( hEdit, SCI_INDICATORSTART, 8, nPos) 
        iEnd   = SciExec( hEdit, SCI_INDICATOREND, 8, iStart)
        if (iStart = 0) andalso (iEnd = 0) then exit while    ' no indicators found

        if SciExec( hEdit, SCI_INDICATORVALUEAT, 8, iStart-1) then
            gFind.foundCount = gFind.foundCount + 1
            if gFind.foundCount > ubound(iPositions) then
                redim preserve iPositions(gFind.foundCount) as long 
            end if
            iPositions(gFind.foundCount) = iStart - lenFind 
        end if
        nPos = iEnd + 1
    wend    
    
    ' Determine the next/prev selection that we should position to.
    if bGetNext then
        ' next selection
        mainIdx = 0
        for i as long = 1 to gFind.foundCount
            if iPositions(i) > startPos then 
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
            if iPositions(i) >= (startPos - lenFind) then exit for
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
            dim as long currentLine = SciExec( hEdit, SCI_LINEFROMPOSITION, iPositions(mainIdx), 0)
            SciExec( hEdit, SCI_ENSUREVISIBLE, currentLine, 0) 
            SciExec( hEdit, SCI_SETSEL, iPositions(mainIdx), iPositions(mainIdx) + lenFind)
        end if
    else    
        SciExec( hEdit, SCI_SETSEL, startPos, startPos)
        gFind.wszResults = "0/0"
    end if
    
    AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )
    
    return iif(mainIdx > 0, true, false)
    
end function


' ========================================================================================
' Highlight the found selections
' ========================================================================================
function FindReplace_HighlightSearches() as long
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    
    ' If Find is not active then don't do any highlighting
    if (gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT) then exit function

    dim as long r, startPos, endPos, findFlags, nLength

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    SciExec( hEdit, SCI_SETADDITIONALCARETSVISIBLE, false, 0) 
    SciExec( hEdit, SCI_INDICSETSTYLE, 8, INDIC_STRAIGHTBOX)
    SciExec( hEdit, SCI_SETINDICATORCURRENT, 8, 0)
    SciExec( hEdit, SCI_INDICSETFORE, 8, ghEditor.ForeColorOccurrence ) 
    SciExec( hEdit, SCI_INDICSETALPHA, 8, 90 )   
    
    dim txtFind as string = gFind.txtFind

    ' Remove all existing selection indicators
    pDoc->RemoveSelectionAttributes()
        
    ' if Find or Replace is open then use their flags also.
    if gTTabCtl.InfoPanelHeight <> TOPTABS_INFO_HEIGHT then
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
 
        gFind.wszResults = "0/" & gFind.foundCount 
        AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )
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

    dim as HWND   hWndFind    = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND)
    dim as HWND   hWndReplace = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
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
        
    ' Need to reparse because some of the function names may been replaced with new names.
    pDoc->bNeedsParsing = true
    pDoc->ParseDocument()

    function = 0
end function


' ========================================================================================
' Center text vertically in the RichEdit Find/Replace controls.
' ========================================================================================
function CenterSingleLineText( byval hRichEdit as HWND ) as long
    dim as RECT rc
    GetClientRect(hRichEdit, @rc)

    dim as HFONT hFont = ghFont(INFOFONT_11)  ' cast(HFONT, SendMessage(hRichEdit, WM_GETFONT, 0, 0))
    dim as HDC hdc = GetDC(hRichEdit)
    dim as HFONT hOldFont = SelectObject(hdc, hFont)

    dim as TEXTMETRIC tm
    GetTextMetrics(hdc, @tm)

    SelectObject(hdc, hOldFont)
    ReleaseDC(hRichEdit, hdc)

    dim as long topMargin = ((rc.bottom - rc.top) - tm.tmHeight) / 2
    if (topMargin < 0) then topMargin = 0

    rc.top = topMargin
    SendMessage( hRichEdit, EM_SETRECT, 0, cast(LPARAM, @rc) )
    
    return 0
end function

