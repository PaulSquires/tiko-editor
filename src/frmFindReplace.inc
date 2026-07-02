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

#include once "frmFindReplace.bi"

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
' frmFindReplace_PositionWindows
' ========================================================================================
function frmFindReplace_PositionWindows() as LRESULT
    if IsWindow( HWND_FRMFINDREPLACE ) = 0 then exit function

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMFINDREPLACE )
    if pWindow = 0 then exit function

    dim wszText as wstring * 100 = gFind.wszResults
    dim as long nResultsWidth = getTextWidth(HWND_FRMFINDREPLACE, wszText, GUIFONT_9) + 10

    pWindow->SetClientSize( 350 + max(70, nResultsWidth), iif(gFind.bExpanded, 66, 33) )
            
    dim as long frmWidth      = AfxGetWindowWidth( HWND_FRMFINDREPLACE )
    dim as long frmHeight     = AfxGetWindowHeight( HWND_FRMFINDREPLACE )
    dim as long iconWidth     = pWindow->ScaleX(24)
    dim as long iconHeight    = pWindow->ScaleY(24)
    dim as long textBoxHeight = pWindow->ScaleY(24)
    dim as long hmargin       = pWindow->ScaleX(2)
    dim as long vmargin       = pWindow->ScaleX(6)
    dim as long nLeft         = pWindow->ScaleY(2)    ' account for the LeftEdge border
    dim as long nTop          = 0

    ' Set the RECT values for each element that we draw in the form. We use these RECTs for
    ' hot hittests and for clicking hittests.
    ' Each panel (find panel and replace panel) total height of 66px

    ' calculate the expand/collapse button
    gFind.rcExpand.Left = nLeft
    gFind.rcExpand.top = 0
    gFind.rcExpand.right = gFind.rcExpand.Left + iconWidth
    gFind.rcExpand.bottom = gFind.rcExpand.top + frmHeight
    
    ' Display the Find textbox               
    nTop  = vmargin
    nLeft = gFind.rcExpand.Right + hmargin
    SetWindowPos( GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND), 0, _
                    nLeft, nTop, 0, 0, SWP_SHOWWINDOW or SWP_NOSIZE or SWP_NOZORDER )

    ' Match Case RECT
    gFind.rcMatchCase.Left = nLeft + _
        AfxGetWindowWidth( GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND) )
    gFind.rcMatchCase.top = nTop
    gFind.rcMatchCase.right = gFind.rcMatchCase.Left + iconWidth
    gFind.rcMatchCase.bottom = gFind.rcMatchCase.top + iconWidth

    ' Whole Word RECT
    gFind.rcWholeWord.Left = gFind.rcMatchCase.right
    gFind.rcWholeWord.top = nTop
    gFind.rcWholeWord.right = gFind.rcWholeWord.Left + iconWidth
    gFind.rcWholeWord.bottom = gFind.rcWholeWord.top + iconWidth

    ' Results from search eg. "1 of 10"
    gFind.rcResults.Left = gFind.rcWholeWord.right + hmargin + hmargin
    gFind.rcResults.top = nTop
    gFind.rcResults.right = gFind.rcResults.Left + pWindow->ScaleX(nResultsWidth)
    gFind.rcResults.bottom = gFind.rcResults.top + pWindow->ScaleY(24)

    ' UpArrow RECT
    gFind.rcUpArrow.Left = gFind.rcResults.right + hmargin
    gFind.rcUpArrow.top = nTop
    gFind.rcUpArrow.right = gFind.rcUpArrow.Left + iconWidth
    gFind.rcUpArrow.bottom = gFind.rcUpArrow.top + iconWidth

    ' DownArrow RECT
    gFind.rcDownArrow.Left = gFind.rcUpArrow.right + hmargin
    gFind.rcDownArrow.top = nTop
    gFind.rcDownArrow.right = gFind.rcDownArrow.Left + iconWidth
    gFind.rcDownArrow.bottom = gFind.rcDownArrow.top + iconWidth

    ' Selection RECT
    gFind.rcSelection.Left = gFind.rcDownArrow.right + hmargin
    gFind.rcSelection.top = nTop
    gFind.rcSelection.right = gFind.rcSelection.Left + iconWidth
    gFind.rcSelection.bottom = gFind.rcSelection.top + iconWidth

    ' Close RECT
    gFind.rcClose.Left = gFind.rcSelection.right + hmargin
    gFind.rcClose.top = nTop
    gFind.rcClose.right = gFind.rcClose.Left + iconWidth
    gFind.rcClose.bottom = gFind.rcClose.top + iconWidth

    ' Display the Replace textbox               
    nTop  = vmargin + textBoxHeight + vmargin
    nLeft = gFind.rcExpand.Right + hmargin
    SetWindowPos( GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTREPLACE), 0, _
                    nLeft, nTop, 0, 0, iif(gFind.bExpanded, SWP_SHOWWINDOW, SWP_HIDEWINDOW) or SWP_NOSIZE or SWP_NOZORDER )

    ' Preserve Case RECT
    gFind.rcPreserve.Left = nleft + _
        AfxGetWindowWidth(GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTREPLACE))
    gFind.rcPreserve.top = nTop
    gFind.rcPreserve.right = gFind.rcPreserve.Left + iconWidth
    gFind.rcPreserve.bottom = gFind.rcPreserve.top + iconWidth
    
    ' Replace RECT
    gFind.rcReplace.Left = gFind.rcPreserve.right + hmargin + hmargin
    gFind.rcReplace.top = nTop
    gFind.rcReplace.right = gFind.rcReplace.Left + iconWidth
    gFind.rcReplace.bottom = gFind.rcReplace.top + iconWidth

    ' ReplaceAll RECT
    gFind.rcReplaceAll.Left = gFind.rcReplace.right 
    gFind.rcReplaceAll.top = nTop
    gFind.rcReplaceAll.right = gFind.rcReplaceAll.Left + iconWidth
    gFind.rcReplaceAll.bottom = gFind.rcReplaceAll.top + iconWidth

    AfxRedrawWindow(HWND_FRMFINDREPLACE)
    
    function = 0
end function


' ========================================================================================
' Find next/prev selection based on current document position
' ========================================================================================
function frmFindReplace_NextSelection( _
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
        gFind.wszResults = mainIdx & " of " & gFind.foundCount 
        ' make sure target line is unfolded
        if bRepositionCaret then
            dim as long currentLine = SciExec( hEdit, SCI_LINEFROMPOSITION, iPositions(mainIdx), 0)
            SciExec( hEdit, SCI_ENSUREVISIBLE, currentLine, 0) 
            SciExec( hEdit, SCI_SETSEL, iPositions(mainIdx), iPositions(mainIdx) + lenFind)
        end if
    else    
        SciExec( hEdit, SCI_SETSEL, startPos, startPos)
        gFind.wszResults = L(269, "No results")
    end if
    frmFindReplace_PositionWindows
    
    return iif(mainIdx > 0, true, false)
    
end function


' ========================================================================================
' Highlight the found selections
' ========================================================================================
function frmFindReplace_HighlightSearches() as long
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as long r, startPos, endPos, findFlags, nLength

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    SciExec( hEdit, SCI_SETADDITIONALCARETSVISIBLE, false, 0) 
    SciExec( hEdit, SCI_INDICSETSTYLE, 8, INDIC_STRAIGHTBOX)
    SciExec( hEdit, SCI_SETINDICATORCURRENT, 8, 0)
    SciExec( hEdit, SCI_INDICSETFORE, 8, ghEditor.ForeColorOccurrence ) 
    SciExec( hEdit, SCI_INDICSETALPHA, 8, 60 )   
    
    dim txtFind as string = gFind.txtFind
        
    if IsWindowVisible(HWND_FRMFINDREPLACE) then
        if gFind.nMatchCase then findFlags = (findFlags or SCFIND_MATCHCASE)
        if gFind.nWholeWord then findFlags = (findFlags or SCFIND_WHOLEWORD)
        SciExec( hEdit, SCI_SETSEARCHFLAGS, findFlags, 0)
    end if
        
    ' Remove all existing selection indicators
    nLength = SendMessage( hEdit, SCI_GETTEXTLENGTH, 0, 0)
    SciExec( hEdit, SCI_INDICATORCLEARRANGE, 0, nLength)
    
    ' Are we searching a selection or the whole document
    if pDoc->HasMarkerHighlight then
        startPos = SciExec( hEdit, SCI_POSITIONFROMLINE, pDoc->FirstMarkerHighlight, 0)
        endPos = SciExec( hEdit, SCI_GETLINEENDPOSITION, pDoc->LastMarkerHighlight, 0)
    else
        startPos = 0
        endPos = SciExec( hEdit, SCI_GETLENGTH, 0, 0)
    end if

    if len(txtFind) then
        do
            SciExec( hEdit, SCI_SETTARGETSTART, startPos, 0)
            SciExec( hEdit, SCI_SETTARGETEND, endPos, 0)
            r = SciExec( hEdit, SCI_SEARCHINTARGET, len(txtFind), strptr(txtFind))
            if r = -1 then exit do
            
            SciExec( hEdit, SCI_SETINDICATORVALUE, 8, 0 )
            SciExec( hEdit, SCI_INDICATORFILLRANGE, r, len(txtFind))

            ' Move start position after the replaced text
            startPos = SendMessage(hEdit, SCI_GETTARGETEND, 0, 0)
        loop
    end if
        
    function = 0

end function


' ========================================================================================
' Replace current selection or all selections
' ========================================================================================
function frmFindReplace_DoReplace( _
            byval fReplaceAll as boolean = false, _
            byval fMovenext as boolean = true _
            ) as long

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as HWND   hWndFind    = GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND)
    dim as HWND   hWndReplace = GetDlgItem(HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTREPLACE)
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
        frmFindReplace_NextSelection(startPos, fMoveNext, true)
        AfxRedrawWindow( hEdit )

    else   
        ' Need to ensure that the current cursor/selection is actually the find
        ' phrase that we want to replace. if not, then move to the closest selection.
        if ucase(pDoc->GetSelText) <> ucase(txtFind) then
            startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0) 
            frmFindReplace_NextSelection(startPos, fMoveNext, true)
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
        frmFindReplace_NextSelection(startPos, fMoveNext, true)
    end if
        
    ' Need to reparse because some of the function names may been replaced with new names.
    pDoc->bNeedsParsing = true
    pDoc->ParseDocument()

    function = 0
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmFindReplace
' ========================================================================================
function frmFindReplace_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id

        case IDC_FRMFINDREPLACE_TXTFIND
            if (codeNotify = EN_CHANGE) then
                gFind.txtFind = AfxGetWindowText(hwndCtl)
                frmFindReplace_HighlightSearches()
                dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
                frmFindReplace_NextSelection(startPos, true, true)
            elseif (codeNotify = EN_SETFOCUS) then
                AfxRedrawWindow( hwnd )
            end if
                
        case IDC_FRMFINDREPLACE_TXTREPLACE
            if (codeNotify = EN_SETFOCUS) then
                AfxRedrawWindow( hwnd )
            end if

    end select
    
    function = 0
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmFindReplace
' ========================================================================================
function frmFindReplace_OnPaint( byval hwnd as HWND) as LRESULT
            
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            
    dim as clsDoubleBuffer b
    
    b.BeginDoubleBuffer(hwnd)

    ' Set default fore and back colors
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.BackColorHot )

    ' Clear the entire back client area
    b.PaintClientRect()

    ' Paint the left edge
    b.SetPenColor(ghFindReplace.leftedge)
    b.PaintLine( 2, 0, 0, 0, b.rcClient.bottom )

    
    ' Paint the Expand/Collapse button
    b.SetFont( SYMBOLFONT_10 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.IconBackColorHot )
    b.PaintRect( @gFind.rcExpand, true )
    dim as DWSTRING wszText = iif( gFind.bExpanded, wszIconChevronDown, wszIconChevronRight )
    b.PaintText( wszText, @gFind.rcExpand, DT_CENTER, true )


    ' Paint a rectangle around the textbox control that has focus.
    b.SetPenColor( BGR(0,0,255) )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.TextBoxBackColor, ghFindReplace.TextBoxBackColor )
    
    dim as RECT rcTextFind = AfxGetWindowRect(GetDlgItem(hwnd, IDC_FRMFINDREPLACE_TXTFIND))
    MapWindowPoints( HWND_DESKTOP, hwnd, cast(LPPOINT, @rcTextFind), 2)
    rcTextFind.left -= 1: rcTextFind.top -= 1: rcTextFind.right += pWindow->ScaleX(49): rcTextFind.bottom += 1
    b.PaintRect( @rcTextFind )
    
    dim as RECT rcTextReplace = AfxGetWindowRect(GetDlgItem(hwnd, IDC_FRMFINDREPLACE_TXTREPLACE))
    MapWindowPoints( HWND_DESKTOP, hwnd, cast(LPPOINT, @rcTextReplace), 2)
    rcTextReplace.left -= 1: rcTextReplace.top -= 1: rcTextReplace.right += pWindow->ScaleX(25): rcTextReplace.bottom += 1
    b.PaintRect( @rcTextReplace )

    if GetFocus() = GetDlgItem(hwnd, IDC_FRMFINDREPLACE_TXTFIND) then b.PaintBorderRect( @rcTextFind, false, 1 )
    if GetFocus() = GetDlgItem(hwnd, IDC_FRMFINDREPLACE_TXTREPLACE) then b.PaintBorderRect( @rcTextReplace, false, 1 )
    
    ' Match Case
    b.SetFont( GUIFONT_9 )
    if gFind.nMatchCase then 
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconSelectedBackColor, ghFindReplace.IconSelectedBackColor )
    elseif isMouseOverRECT( hwnd, gFind.rcMatchCase ) then
        b.SetForeColors( ghFindReplace.ForeColorHot, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconBackColorHot, ghFindReplace.IconBackColorHot )
    else
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.TextBoxBackColor, ghFindReplace.TextBoxBackColor )
    end if
    b.PaintRoundRect( @gFind.rcMatchCase )
    b.PaintText( wszIconMatchCase, @gFind.rcMatchCase, DT_CENTER )
    
    
    ' Whole Word
    b.SetFont( GUIFONT_9 )
    if gFind.nWholeWord then 
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconSelectedBackColor, ghFindReplace.BackColorHot )
    elseif isMouseOverRECT( hwnd, gFind.rcWholeWord ) then
        b.SetForeColors( ghFindReplace.ForeColorHot, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconBackColorHot, ghFindReplace.IconBackColorHot )
    else
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColor )
        b.SetBackColors( ghFindReplace.TextBoxBackColor, ghFindReplace.TextBoxBackColor )
    end if
    b.PaintRoundRect( @gFind.rcWholeWord )
    b.PaintText( wszIconWholeWord, @gFind.rcWholeWord, DT_CENTER )


    ' Paint the Results text
    b.SetFont( GUIFONT_9 )
    if gFind.foundCount = 0 then 
        b.SetForeColors( ghFindReplace.NotFoundForeColor, ghFindReplace.NotFoundForeColor )
    else
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColor )
    end if
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.BackColorHot )
    b.PaintText( gFind.wszResults, @gFind.rcResults, DT_CENTER )
    

    ' Paint the Up Arrow
    b.SetFont( SYMBOLFONT_9 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor,  ghFindReplace.IconBackColorHot )
    b.PaintRoundRect( @gFind.rcUpArrow, true )
    b.PaintText( wszIconUpArrow, @gFind.rcUpArrow, DT_CENTER )

    
    ' Paint the Down Arrow
    b.SetFont( SYMBOLFONT_9 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor,  ghFindReplace.IconBackColorHot )
    b.PaintRoundRect( @gFind.rcDownArrow, true )
    b.PaintText( wszIconDownArrow, @gFind.rcDownArrow, DT_CENTER )


    ' Paint the Selection icon
    b.SetFont( SYMBOLFONT_9 )
    if gFind.nSelection then 
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconSelectedBackColor, ghFindReplace.IconSelectedBackColor )
    elseif isMouseOverRECT( hwnd, gFind.rcSelection) then
        b.SetForeColors( ghFindReplace.ForeColorHot, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconBackColorHot, ghFindReplace.IconBackColorHot )
    else
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.BackColorHot )
    end if
    b.PaintRoundRect( @gFind.rcSelection )
    b.PaintText( wszIconSelection, @gFind.rcSelection, DT_CENTER )


    ' Paint the Close icon
    b.SetFont( SYMBOLFONT_9 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.IconBackColorHot  )
    b.PaintRoundRect( @gFind.rcClose, true )
    b.PaintText( wszIconClose, @gFind.rcClose, DT_CENTER )


    ' Preserve Case
    b.SetFont( GUIFONT_9 )
    if gFind.nPreserve then 
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconSelectedBackColor, ghFindReplace.BackColorHot )
    elseif isMouseOverRECT( hwnd, gFind.rcPreserve ) then
        b.SetForeColors( ghFindReplace.ForeColorHot, ghFindReplace.ForeColorHot )
        b.SetBackColors( ghFindReplace.IconBackColorHot, ghFindReplace.IconBackColorHot )
    else
        b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColor )
        b.SetBackColors( ghFindReplace.TextBoxBackColor, ghFindReplace.TextBoxBackColor )
    end if
    b.PaintRoundRect( @gFind.rcPreserve )
    b.PaintText( wszIconPreserveCase, @gFind.rcPreserve, DT_CENTER )

    
    ' Paint the Replace icon
    b.SetFont( SYMBOLFONT_9 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.IconBackColorHot  )
    b.PaintRoundRect( @gFind.rcReplace, true )
    b.PaintText( wszIconReplace, @gFind.rcReplace, DT_CENTER )


    ' Paint the ReplaceAll icon
    b.SetFont( SYMBOLFONT_9 )
    b.SetForeColors( ghFindReplace.ForeColor, ghFindReplace.ForeColorHot )
    b.SetBackColors( ghFindReplace.BackColor, ghFindReplace.IconBackColorHot  )
    b.PaintRoundRect( @gFind.rcReplaceAll, true )
    b.PaintText( wszIconReplaceAll, @gFind.rcReplaceAll, DT_CENTER )

    b.EndDoubleBuffer()
    
    function = true
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmFindReplace
' ========================================================================================
function frmFindReplace_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmFindReplace
' ========================================================================================
function frmFindReplace_OnDestroy( byval hwnd as HWND ) as LRESULT 
    
    ' Remove selected attributes (Attribute #8). We need to remove it from all
    ' open documents because the user may have tabbed between documents while
    ' the FindReplace dialog was active thereby causing selection highlights.
    gApp.RemoveAllSelectionAttributes
    
    ' Destroy the "shadow" form
    DestroyWindow( HWND_FRMFINDREPLACE_SHADOW )
    
    ' Remove any markers set in the document that highlights
    ' entire lines (used for Selected text searching).
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        pDoc->RemoveMarkerHighlight
        frmMain_SetFocusToCurrentCodeWindow
    end if
        
    ' Destroy the CueBannerFont. We need to keep this font alive throughout the
    ' the life of the popup otherwise the cuebanner text will not show. We can
    ' not create the font everytine in paint.
    if gFind.hCueBannerFont then 
        DeleteObject(gFind.hCueBannerFont)
        gFind.hCueBannerFont = 0
    end if

    DestroyWindow(ghFindTooltip)
    ghFindTooltip = 0

    ' ensure that the tabcontrol shadow is painted once the find dialog disappears
    frmMain_PositionWindows()
    
    function = 0
end function


' ========================================================================================
' frmFindReplaceShadow_WndProc Window procedure
' ========================================================================================
function frmFindReplaceShadow_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT
        
    select case uMsg
        case WM_DESTROY
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDREPLACE_SHADOW = 0
    end select      

    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmFindReplace Window procedure
' ========================================================================================
function frmFindReplace_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT
    
    select case uMsg
        HANDLE_MSG (hwnd, WM_CLOSE,    frmFindReplace_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmFindReplace_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmFindReplace_OnCommand)
        HANDLE_MSG (hwnd, WM_PAINT,    frmFindReplace_OnPaint)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMFINDREPLACE = 0

        gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT
        frmMain_PositionWindows()

        return 0

    case WM_ACTIVATE
        AfxRedrawWindow( hwnd )
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 20    ' system default is 400ms
        TrackMouseEvent(@tme) 

        if IsWindow(ghFindTooltip) = 0 then ghFindTooltip = AfxAddTooltip( hwnd, "", false, false )
        AfxRedrawWindow(hwnd)

    case WM_MOUSELEAVE
        AfxRedrawWindow( hwnd )

    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip = ""
        if isMouseOverRECT( hwnd, gFind.rcExpand ) then
            wszTooltip = L(270, "Toggle Replace mode")
        elseif isMouseOverRECT( hwnd, gFind.rcMatchCase ) then
            wszTooltip = L(163,"Match Case")
        elseif isMouseOverRECT( hwnd, gFind.rcWholeWord ) then
            wszTooltip = L(162,"Match Whole Words")
        elseif isMouseOverRECT( hwnd, gFind.rcUpArrow ) then
            wszTooltip = L(45,"Find Previous") & " (" & getMenuAccelText(IDM_FINDPREVACCEL) & ")"
        elseif isMouseOverRECT( hwnd, gFind.rcDownArrow ) then
            wszTooltip = L(44,"Find next") & " (" & getMenuAccelText(IDM_FINDNEXTACCEL) & ")"
        elseif isMouseOverRECT( hwnd, gFind.rcSelection ) then
            wszTooltip = L(148,"Selection")
        elseif isMouseOverRECT( hwnd, gFind.rcPreserve ) then
            wszTooltip = L(424,"Preserve Case")
        elseif isMouseOverRECT( hwnd, gFind.rcReplace ) then
            wszTooltip = L(173,"Replace") & " (Enter)"
        elseif isMouseOverRECT( hwnd, gFind.rcReplaceAll ) then
            wszTooltip = L(174,"Replace All") & " (Ctrl+Alt+Enter)"
        elseif isMouseOverRECT( hwnd, gFind.rcClose ) then
            wszTooltip = L(161,"Close") & " (Esc)"
        end if

        ' Display the tooltip
        AfxSetTooltipText( ghFindTooltip, hwnd, wszTooltip )
        AfxRedrawWindow( hwnd )


    case WM_RBUTTONDOWN

    case WM_LBUTTONDOWN

    case WM_LBUTTONUP
        dim as HWND hEdit 
        dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
        if pDoc then hEdit = pDoc->hWndActiveScintilla
        
        if isMouseOverRECT( hwnd, gFind.rcExpand ) then
            gFind.bExpanded = not gFind.bExpanded
            frmFindReplace_Show( hwnd, gFind.bExpanded )

        elseif isMouseOverRECT( hwnd, gFind.rcMatchCase ) then
            gFind.nMatchCase = not gFind.nMatchCase
            frmFindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcWholeWord ) then
            gFind.nWholeWord = not gFind.nWholeWord
            frmFindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcUpArrow ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDPREV, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcDownArrow ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDNEXT, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcSelection ) then
            if pDoc = 0 then exit function 
            dim as boolean isOriginalSelectionMultiline = _
                (pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline) 
            if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
            if (isOriginalSelectionMultiline) orelse (pDoc->HasMarkerHighlight) then
                gFind.nSelection = not gFind.nSelection
            else
                gFind.nSelection = false
            end if
            if gFind.nSelection then
                pDoc->SetMarkerHighlight
            else   
                pDoc->RemoveMarkerHighlight
            end if
            frmFindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcPreserve ) then
            if pDoc = 0 then exit function   
            gFind.nPreserve = not gFind.nPreserve

        elseif isMouseOverRECT( hwnd, gFind.rcReplace ) then
            if pDoc = 0 then exit function   
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEnext, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcReplaceAll ) then
            if pDoc = 0 then exit function   
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEALL, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcClose ) then
            SendMessage( hwnd, WM_CLOSE, 0, 0 )
        end if
            
    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmFindReplace_RichEdit_SubclassProc Window procedure
' ========================================================================================
Function frmFindReplace_RichEdit_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)

    select case uMsg
                            
        case WM_PRINTCLIENT, WM_PAINT
            dim as long textLength = AfxGetWindowTextLength(hWin)
            if (textLength = 0 ) then 
                ' Get the needed DC with DCX_INTERSECTUPDATE before the EDIT
                ' control's WM_PAINT handler calls BeginPaint/EndPaint, which
                ' validates the update rect and would otherwise lead to drawing
                ' nothing later because the region is empty. Also, grab it from
                ' the cache so we don't mess with the EDIT's DC.
                dim as HDC hdc = _
                    iif(uMsg = WM_PRINTCLIENT, GetDC(hWin), _
                        GetDCEx(hWin, 0, DCX_INTERSECTUPDATE or DCX_CACHE or DCX_CLIPCHILDREN or DCX_CLIPSIBLINGS) )

                ' Call the EDIT control so that the caret is properly handled,
                ' no caret litter left on the screen after tabbing away.
                dim as LRESULT result = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

                SaveDC(hdc)
                
                ' Get the font and margin so the cue banner text has a
                ' consistent appearance and placement with existing text.
                dim as RECT editRect: SendMessage( hWin, EM_GETRECT, 0, cast(LPARAM, @editRect) )

                ' Ideally we would call Edit_GetCueBannerText, but since that message
                ' returns nothing when ES_MULTILINE, use a window property instead.
                dim cueBannerText as wstring * 250
                if hWin = GetDlgItem( HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND ) then
                    cueBannerText = L(158,"Find")
                elseif hWin = GetDlgItem( HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTREPLACE ) then
                    cueBannerText = L(173,"Replace")
                end if
                SelectObject( hdc, gFind.hCueBannerFont )
                SetTextColor( hdc, ghFindReplace.CueBannerForeColor )
                SetBkMode(hdc, TRANSPARENT)
                dim as long wsStyle = DT_TOP or DT_LEFT or DT_NOPREFIX or DT_NOCLIP or DT_SINGLELINE 
                DrawText(hdc, cueBannerText, -1, @editRect, wsStyle)
                RestoreDC( hdc, -1 )
                ReleaseDC( hWin, hdc )

                ' Return the EDIT's result (could probably safely just return zero here,
                ' but seems safer to relay whatever value came from the edit).
                return result
            end if
        
        case WM_CONTEXTMENU 
            ' Create the right click popup menu
            dim as DWSTRING wszText = RichEdit_GetSelText( hWin )
            Dim hPopUpMenu As HMENU = CreatePopupMenu()
            if len(wszText) then 
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_CUT, wstr("Cut") )
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_COPY, wstr("Copy") )
            end if
            if RichEdit_CanPaste( hWin, 0 ) then
                if len(wszText) then 
                    AppendMenu( hPopUpMenu, MF_SEPARATOR, 0, "" )
                end if   
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_PASTE, wstr("Paste") )
            end if   

            dim as long nResult
            nResult = TrackPopupMenu( hPopUpMenu, TPM_RETURNCMD or TPM_NONOTIFY, _
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMFINDREPLACE, 0 ) 
            select case nResult
                case IDM_CUT:   SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:  SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE: SendMessage( hWin, WM_PASTE, 0, 0 )
            end select
            DestroyMenu hPopUpMenu

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmFindReplace_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function


' ========================================================================================
' frmFindReplace_PositionMainWindow
' ========================================================================================
function frmFindReplace_PositionMainWindow() as LRESULT
    ' find/replace is a popup window so we need to use screen coordinates. Place the 
    ' window just below the toptabs or the menubar if the toptabs are not showing.
    ' Place the window the width of 2 scrollbars from the right of the main window.
    dim as HWND hCtrl = iif( IsWindowVisible(HWND_FRMMAIN_TOPTABS), _
                                HWND_FRMMAIN_TOPTABSINFO, HWND_FRMMAIN_MENUBAR )  

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMFINDREPLACE )

    dim as RECT rc = AfxGetWindowRect( hCtrl )
    SetWindowPos( HWND_FRMFINDREPLACE_SHADOW, 0, _
                    rc.right - AfxGetWindowWidth(HWND_FRMFINDREPLACE) - _
                        pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR*3) - pWindow->ScaleX(3), _
                    rc.bottom, _
                    AfxGetWindowWidth(HWND_FRMFINDREPLACE) + pWindow->ScaleY(6), _
                    AfxGetWindowHeight(HWND_FRMFINDREPLACE) + pWindow->ScaleY(3), _
                    SWP_SHOWWINDOW or SWP_NOZORDER)
    SetWindowPos( HWND_FRMFINDREPLACE, 0, _
                    rc.right - AfxGetWindowWidth(HWND_FRMFINDREPLACE) - pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR * 3), _
                    rc.bottom, 0, 0, SWP_SHOWWINDOW or SWP_NOSIZE or SWP_NOZORDER)

    AfxRedrawWindow(HWND_FRMFINDREPLACE)

    function = 0
end function


' ========================================================================================
' frmFindReplace_Show
' ========================================================================================
function frmFindReplace_Show( _
            byval hWndParent as HWND, _
            byval fShowReplace as boolean _
            ) as LRESULT

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
        
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMFINDREPLACE )

    dim as HWND hCtl
    dim as String sFindText
    dim as HWND hCtlFind, hCtlReplace 


    ' Expand the size of frmTopTabsInfo to accomodate the Find and/or Replace controls.
    gTTabCtl.InfoPanelHeight = iif( fShowReplace, TOPTABS_INFO_HEIGHT_REPLACE, TOPTABS_INFO_HEIGHT_FIND )
    frmMain_PositionWindows()
    

    
    gFind.bExpanded = fShowReplace

    if IsWindow(ghFindTooltip) then DestroyWindow(ghFindTooltip)
    ghFindTooltip = 0
    
    if IsWindow( HWND_FRMFINDREPLACE ) = 0 then
        gFind.nMatchCase = false
        gFind.nWholeWord = false
        gFind.nSelection = false

        '  Create the main window and child controls
        pWindow = new CWindow

        HWND_FRMFINDREPLACE = _
        pWindow->Create(hWndParent, "FINDREPLACE", @frmFindReplace_WndProc, 0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _   
            WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR )
        pWindow->ClassStyle = CS_DBLCLKS
        
        
        ' we use RichEdit controls because regular Edit controls can not easily have
        ' their text vertically centered.
        if gFind.hCueBannerFont = 0 then 
            gFind.hCueBannerFont = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL, , , , DEFAULT_CHARSET)
        end if

        dim as RECT rc
        dim cf as CHARFORMATW 
        cf.cbSize = sizeof(cf)
        cf.dwMask = CFM_COLOR
        cf.crTextColor = ghFindReplace.TextBoxForeColor

        hCtlFind = pWindow->AddControl("RICHEDIT", , IDC_FRMFINDREPLACE_TXTFIND, "", _
                    0, 0, 166, 24, _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
                    WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(hCtlFind, @frmFindReplace_RichEdit_SubclassProc, IDC_FRMFINDREPLACE_TXTFIND, NULL)

        SendMessage( hCtlFind, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
        SendMessage( hCtlFind, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghFindReplace.TextBoxBackColor) )
        GetClientRect( hCtlFind, @rc )
        rc.top = rc.top + pWindow->ScaleY(2): rc.left = rc.left + pWindow->ScaleX(4)
        SendMessage( hCtlFind, EM_SETRECT, 0, cast(LPARAM, @rc) )
        SendMessage( hCtlFind, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )
        
        hCtlReplace = pWindow->AddControl("RICHEDIT", , IDC_FRMFINDREPLACE_TXTREPLACE, gFind.txtReplace, _
                    0, 0, 190, 24, _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
                    WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(hCtlReplace, @frmFindReplace_RichEdit_SubclassProc, IDC_FRMFINDREPLACE_TXTREPLACE, NULL)
        SendMessage( hCtlReplace, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
        SendMessage( hCtlReplace, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghFindReplace.TextBoxBackColor) )
        GetClientRect( hCtlReplace, @rc )
        rc.top = rc.top + pWindow->ScaleY(2): rc.left = rc.left + pWindow->ScaleX(4)
        SendMessage( hCtlReplace, EM_SETRECT, 0, cast(LPARAM, @rc) )
        SendMessage( hCtlReplace, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )
        
        ' create semi-transparent window slightly offset under our popup menu in order to simulate a shadow.
        pWindow = New CWindow
        HWND_FRMFINDREPLACE_SHADOW = pWindow->Create( HWND_FRMMAIN, "", _
            @frmFindReplaceShadow_WndProc, 0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, WS_EX_LAYERED )
        pWindow->ClassStyle = CS_DBLCLKS
        pWindow->Brush = GetSysColorBrush(COLOR_WINDOWTEXT + 1)   ' black background
        SetLayeredWindowAttributes( HWND_FRMFINDREPLACE_SHADOW, GetSysColor(COLOR_WINDOWTEXT + 1) , 80, LWA_ALPHA ) 

    end if


    gFind.wszResults = L(269, "No results")

    ' position the child windows of the find/replace dialog
    frmFindReplace_PositionWindows()
    
    ' Position this Find/Replace popup window separately because this
    ' function is also used during WM_MOVE of frmMain.
    frmFindReplace_PositionMainWindow()
    

    dim as boolean isOriginalSelectionMultiline = _
                    cbool(pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
    if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
    
    sFindText = pDoc->GetSelText
    if isOriginalSelectionMultiline then
        sFindText = ""
        gFind.nSelection = true   
        pDoc->SetMarkerHighlight
    else
        gFind.txtFind = sFindtext
    end if   
    hCtl = GetDlgItem( HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND )
    AfxSetWindowText( hCtl, sFindText )

    if gFind.bExpanded then
        hCtl = GetDlgItem( HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTREPLACE )
    else   
        hCtl = GetDlgItem( HWND_FRMFINDREPLACE, IDC_FRMFINDREPLACE_TXTFIND )
    end if   

    frmFindReplace_HighlightSearches()
    if len(AfxGetWindowText(hCtl)) then Edit_SetSel(hCtl, 0, -1)

    SetFocus hCtl 
    AfxRedrawWindow(HWND_FRMFINDREPLACE)

    function = 0
end function

