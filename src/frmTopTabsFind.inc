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

#include once "frmPanelMenu.bi"


' This bar's tooltip. A PsTooltip rather than the comctl32 tip built by hand here before: it
' is themed, it wraps without the hand-sent TTM_SETMAXTIPWIDTH that used to follow the create,
' and it adds no pump obligation. One tip for the window; the hot rect is carried by the TOOL
' INDEX, set from WM_MOUSEMOVE, so the hit-test runs once on the message rather than a second
' time against a cursor that has since moved.
dim shared ghTopTabsFindTip as HWND

enum
    TTFTIP_NONE = -1
    TTFTIP_TOGGLE = 0
    TTFTIP_MATCHCASE
    TTFTIP_WHOLEWORD
    TTFTIP_UPARROW
    TTFTIP_DOWNARROW
    TTFTIP_SELECTION
    TTFTIP_CLOSE
end enum


' ========================================================================================
' Supply the tooltip text for the currently hot rect.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi). Returning "" means no tip for the rest of this hover.
private function frmTopTabsFind_TipText( byval hTip as HWND, _
                                         byval hAttached as HWND, _
                                         byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    select case idx
        case TTFTIP_TOGGLE:     return frmPanelMenu_FormatTooltip( IDM_TOGGLEREPLACE )
        case TTFTIP_MATCHCASE:  return L(163,"Match Case")
        case TTFTIP_WHOLEWORD:  return L(162,"Match Whole Words")
        case TTFTIP_UPARROW:    return frmPanelMenu_FormatTooltip( IDM_FINDPREVACCEL )
        case TTFTIP_DOWNARROW:  return frmPanelMenu_FormatTooltip( IDM_FINDNEXTACCEL )
        case TTFTIP_SELECTION:  return L(148,"Selection")
        case TTFTIP_CLOSE:      return L(161,"Close") & " (Esc)"
    end select
    return ""
end function


' ========================================================================================
' Which of the bar's rects is the cursor over? TTFTIP_NONE for none of them.
' ========================================================================================
private function frmTopTabsFind_HitTest( byval hwnd as HWND ) as long
    if isMouseOverRECT( hwnd, gFind.rcToggle )    then return TTFTIP_TOGGLE
    if isMouseOverRECT( hwnd, gFind.rcMatchCase ) then return TTFTIP_MATCHCASE
    if isMouseOverRECT( hwnd, gFind.rcWholeWord ) then return TTFTIP_WHOLEWORD
    if isMouseOverRECT( hwnd, gFind.rcUpArrow )   then return TTFTIP_UPARROW
    if isMouseOverRECT( hwnd, gFind.rcDownArrow ) then return TTFTIP_DOWNARROW
    if isMouseOverRECT( hwnd, gFind.rcSelection ) then return TTFTIP_SELECTION
    if isMouseOverRECT( hwnd, gFind.rcClose )     then return TTFTIP_CLOSE
    return TTFTIP_NONE
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmTopTabsFind_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSFIND)
    if pWindow = 0 then exit function

    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_TOPTABSFIND, @rc )

    dim as long iconWidth
    dim as long iconHeight 
    dim as long nPadding = 10   ' 5 left 5 right
    dim as long vmargin = pWindow->ScaleY(4)
    dim as long nLeft
    dim as long nTop
    dim as long nWidth 
    dim as long nHeight
    dim as boolean isProjectSearch = false
    
'    dim as long idx = gTTabCtl.GetCurSel()
'    if gTTabCtl.IsValidTab(idx) then
'        if gTTabCtl.tabs(idx).docType = DocumentType.ProjectSearch then isProjectSearch = true
'    end if    

    dim as HWND hCtlFind = GetDlgItem(HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND)

    ' FIND CONTROLS
    gTTabCtl.rcFindTextRect = rc
    gTTabCtl.rcFindTextRect.left      = pWindow->ScaleY(10)
    gTTabCtl.rcFindTextRect.top       = 0
    gTTabCtl.rcFindTextRect.right     = rc.right - pWindow->ScaleX(300)
    gTTabCtl.rcFindTextRect.bottom    = pWindow->ScaleY(TOPTABS_FIND_HEIGHT) - pWindow->ScaleY(8)
     
    rc      = gTTabCtl.rcFindTextRect
    nLeft   = rc.left + vmargin + pWindow->ScaleX(10)
    nTop    = rc.top + vmargin
    nWidth  = (rc.right - rc.left - vmargin*2 - pWindow->ScaleX(10))
    nHeight = (rc.bottom - rc.top - vmargin*2)

    ' Make the width of the textbox smaller because we want to visually fit the
    ' Match Case and Whole Word icons into the Find rect also. nHeight
    ' will be our icon height and width as well.
    iconWidth = nHeight
    iconHeight = nHeight
    nWidth = nWidth - (iconWidth * 2) - pWindow->ScaleX(4)
    
    ' DiskFilename text. Vertical centering re-derives inside the control on WM_SIZE.
    SetWindowPos( hCtlFind, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Match Case RECT
    gFind.rcMatchCase.left   = nLeft + nWidth
    gFind.rcMatchCase.top    = nTop
    gFind.rcMatchCase.right  = gFind.rcMatchCase.left + iconWidth
    gFind.rcMatchCase.bottom = gFind.rcMatchCase.top + nHeight

    ' Whole Word RECT
    gFind.rcWholeWord.left   = gFind.rcMatchCase.right
    gFind.rcWholeWord.top    = gFind.rcMatchCase.top
    gFind.rcWholeWord.right  = gFind.rcWholeWord.left + iconWidth
    gFind.rcWholeWord.bottom = gFind.rcMatchCase.bottom

    ' Toggle Replace RECT
    gFind.rcToggle.Left     = rc.right + pWindow->ScaleX(8)
    gFind.rcToggle.top      = gFind.rcWholeWord.top
    gFind.rcToggle.right    = gFind.rcToggle.left + iconWidth
    gFind.rcToggle.bottom   = gFind.rcWholeWord.bottom

    ' Selection RECT
    gFind.rcSelection.left   = gFind.rcToggle.right
    gFind.rcSelection.top    = gFind.rcToggle.top
    gFind.rcSelection.right  = gFind.rcSelection.left + iconWidth
    gFind.rcSelection.bottom = gFind.rcToggle.bottom
    ' Project Search does not allow for Selection
    if isProjectSearch then gFind.rcSelection.right = gFind.rcSelection.left

    ' Vertical Divider line
    gFind.rcDividerLine.left   = gFind.rcSelection.right
    gFind.rcDividerLine.top    = gFind.rcSelection.top + pWindow->ScaleY(4)
    gFind.rcDividerLine.right  = gFind.rcDividerLine.left + (iconWidth / 2)
    gFind.rcDividerLine.bottom = gFind.rcSelection.bottom - pWindow->ScaleY(4)
    
    ' UpArrow RECT
    gFind.rcUpArrow.left       = gFind.rcDividerLine.right + (iconWidth / 2)
    gFind.rcUpArrow.top        = gFind.rcSelection.top
    gFind.rcUpArrow.right      = gFind.rcUpArrow.left + iconWidth
    gFind.rcUpArrow.bottom     = gFind.rcSelection.bottom

    ' DownArrow RECT
    gFind.rcDownArrow.left     = gFind.rcUpArrow.right
    gFind.rcDownArrow.top      = gFind.rcUpArrow.top
    gFind.rcDownArrow.right    = gFind.rcDownArrow.left + iconWidth
    gFind.rcDownArrow.bottom   = gFind.rcUpArrow.bottom

    ' Close RECT (calculate this one prior to the Results rect). If this is a Project Search
    ' document then we hide the Close button. We do need to set the rcClose left coordinate
    ' because rcResults needs it.
    GetClientRect( HWND_FRMMAIN_TOPTABSFIND, @rc )
    gFind.rcClose.right       = rc.right - (pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR) / 2)
    gFind.rcClose.left        = gFind.rcClose.right - iconWidth
    gFind.rcClose.top         = gFind.rcDownArrow.top + pWindow->ScaleY(4)
    gFind.rcClose.bottom      = gFind.rcDownArrow.bottom
    if isProjectSearch then SetRect( @gFind.rcClose, rc.right, 0, rc.right, 0 )

    ' Results from search eg. "1 of 10"
    gFind.rcResults.left       = gFind.rcDownArrow.right + pWindow->ScaleX(4)
    gFind.rcResults.top        = gFind.rcDownArrow.top
    gFind.rcResults.right      = gFind.rcClose.left
    gFind.rcResults.bottom     = gFind.rcDownArrow.bottom

    AfxRedrawWindow( HWND_FRMMAIN_TOPTABSFIND )

    function = 0
end function


' ========================================================================================
' PsTextBox callbacks for the Find textbox. The control owns the cue banner, colors,
' vertical centering, single-line enforcement and the Cut/Copy/Paste context menu that
' the old RichEdit subclass hand-rolled here. ENTER/TAB handling stays in
' handleKeysFindReplace (modMsgPump), which intercepts those keys before dispatch.
' ========================================================================================
sub frmTopTabsFind_TextBoxChange( byval hTextBox as HWND )
    ' User edits only (was the EN_CHANGE path): re-run the incremental search.
    gFind.txtFind = PsTextBox_GetText( hTextBox )
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        FindReplace_HighlightSearches()
        dim as hwnd hEdit = pDoc->hWndActiveScintilla
        dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
        FindReplace_NextSelection(startPos, true, true)
    end if
    SetFocus( hTextBox )   ' the search may have moved focus to Scintilla; take it back
end sub

sub frmTopTabsFind_TextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    ' Was the EN_SETFOCUS path: repaint the panel chrome around the textbox.
    if bGotFocus then AfxRedrawWindow( HWND_FRMMAIN_TOPTABSFIND )
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmTopTabsFind
' ========================================================================================
function frmTopTabsFind_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmTopTabsFind_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmTopTabsFind
' ========================================================================================
function frmTopTabsFind_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    ' The Find textbox notifies through its PsTextBox callbacks (change/focus), not
    ' through WM_COMMAND EN_ codes anymore.
    function = 0
end function


' ========================================================================================
' frmTopTabsFind Window procedure
' ========================================================================================
function frmTopTabsFind_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsFind_OnSize)
        HANDLE_MSG (hwnd, WM_COMMAND, frmTopTabsFind_OnCommand)

    case WM_DESTROY
        ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with the
        ' parent -- destroying it here is also what removes it from the shared clock's registry.
        if IsWindow(ghTopTabsFindTip) then DestroyWindow( ghTopTabsFindTip )
        ghTopTabsFindTip = 0

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_TOPTABSFIND = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSFIND)
        if pWindow = 0 then exit function

        ' TME_LEAVE only. TME_HOVER existed solely to drive the old WM_MOUSEHOVER tooltip arm;
        ' PsTooltip does its own dwell timing off the shared clock.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)

        ' Silent when the index has not changed; on a change it hides any visible tip without
        ' a fade and clears a suppressed hover, so the new rect gets a fresh offer.
        PsTooltip_SetToolIndex( ghTopTabsFindTip, frmTopTabsFind_HitTest( hwnd ) )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghTopTabsFindTip, TTFTIP_NONE )
        AfxRedrawWindow( hwnd )


    case WM_LBUTTONUP
        if isMouseOverRECT( hwnd, gFind.rcToggle ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACE, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcMatchCase ) then
            gFind.nMatchCase = not gFind.nMatchCase
            FindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcWholeWord ) then
            gFind.nWholeWord = not gFind.nWholeWord
            FindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcUpArrow ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDPREV, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcDownArrow ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDNEXT, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcSelection ) then
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
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
            FindReplace_HighlightSearches()
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcClose ) then
            FindReplaceControls_Hide()
        end if
            

    case WM_PAINT
        dim as PsBufferPaint b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors. The strip's field color is BackColorHot, so a
        ' hovered (unselected) button inverts to the tab area's normal BackColor.
        b.SetColors( ghTopTabs.ForeColorHot, ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Only paint the bottom line if Replace is not active.
        b.SetPenColor( ghEditor.Divider )
        if gFind.bShowReplacePanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if

        ' Draw the Find controls.
        b.SetPenColor( ghEditor.Divider )

        b.PaintRoundBorderRect( @gTTabCtl.rcFindTextRect )

        b.SetFont( ghFont(GUIFONT_9) )

        ' Match Case
        if gFind.nMatchCase then
            b.SetBackColor( ghTopTabs.IconSelectedBackColor )
        else
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcMatchCase), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        end if
        b.PaintIconButton( wszIconMatchCase, @gFind.rcMatchCase )

        ' Whole Word
        if gFind.nWholeWord then
            b.SetBackColor( ghTopTabs.IconSelectedBackColor )
        else
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcWholeWord), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        end if
        b.PaintIconButton( wszIconWholeWord, @gFind.rcWholeWord )

        b.SetBackColor( ghTopTabs.BackColorHot )
        b.PaintText( gFind.wszResults, @gFind.rcResults, DT_LEFT )

        b.SetFont( ghFont(SYMBOLFONT_9) )
        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcToggle), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        b.PaintIconButton( wszIconToggleReplace, @gFind.rcToggle )

        if gFind.nSelection then
            b.SetBackColor( ghTopTabs.IconSelectedBackColor )
        else
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcSelection), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        end if
        b.PaintIconButton( wszIconSelection, @gFind.rcSelection )

        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcUpArrow), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        b.PaintIconButton( wszIconChevronLeft, @gFind.rcUpArrow )
        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcDownArrow), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        b.PaintIconButton( wszIconChevronRight, @gFind.rcDownArrow )
        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcClose), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        b.PaintIconButton( wszIconClose, @gFind.rcClose )
        
        ' Vertical Divider Line
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 1, gFind.rcDividerLine.right, gFind.rcDividerLine.top, _
                            gFind.rcDividerLine.right, gFind.rcDividerLine.bottom )
                
        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmTopTabsFind_Show
' ========================================================================================
function frmTopTabsFind_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_TOPTABSFIND = pWindow->Create( hwndParent, @frmTopTabsFind_WndProc, _
        0, 0, 0, TOPTABS_FIND_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    ' The Find textbox is the reusable PsTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as HWND hCtlFind = PsTextBox_Create( HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND )
    PsTextBox_SetFont( hCtlFind, ghFont(INFOFONT_11) )
    PsTextBox_SetForeColor( hCtlFind, ghTopTabs.ForeColorHot )
    PsTextBox_SetBackColor( hCtlFind, ghTopTabs.BackColorHot )
    PsTextBox_SetBorderWidth( hCtlFind, 0 )                       ' flat, as before
    PsTextBox_SetCueBannerText( hCtlFind, L(158,"Find") & "..." )
    PsTextBox_SetCueBannerColor( hCtlFind, ghTopTabs.ForeColor )
    PsTextBox_SetChangeCallback( hCtlFind, @frmTopTabsFind_TextBoxChange )
    PsTextBox_SetFocusCallback( hCtlFind, @frmTopTabsFind_TextBoxFocus )
    ThemeTextBoxContextMenu( hCtlFind )

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so a long tip still
    ' wraps at the same place; colours and fonts are pushed per hover from
    ' frmTopTabsFind_TipText, so nothing set here can go stale on a theme change.
    ghTopTabsFindTip = PsTooltip_Create( HWND_FRMMAIN_TOPTABSFIND )
    PsTooltip_SetToolIndex( ghTopTabsFindTip, TTFTIP_NONE )
    PsTooltip_SetMaxWidth( ghTopTabsFindTip, 200 )
    PsTooltip_SetTooltipCallback( ghTopTabsFindTip, @frmTopTabsFind_TipText )

    function = 0
    
end function

