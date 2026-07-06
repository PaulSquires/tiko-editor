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
    
    dim as HWND hCtlFind = GetDlgItem(HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND)

    ' FIND CONTROLS
    gTTabCtl.rcFindTextRect = rc
    gTTabCtl.rcFindTextRect.left      = pWindow->ScaleY(10)
    gTTabCtl.rcFindTextRect.top       = 0
    gTTabCtl.rcFindTextRect.right     = gTTabCtl.rcPrevTabs.left - pWindow->ScaleX(150)
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
    
    ' DiskFilename text
    SetWindowPos( hCtlFind, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )
    CenterSingleLineText( hCtlFind )    
    SendMessage( hCtlFind, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )

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

    ' Close RECT (calculate this one prior to the Results rect)
    GetClientRect( HWND_FRMMAIN_TOPTABSFIND, @rc )
    gFind.rcClose.right       = rc.right - (pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR) / 2)
    gFind.rcClose.left        = gFind.rcClose.right - iconWidth
    gFind.rcClose.top         = gFind.rcDownArrow.top + pWindow->ScaleY(4)
    gFind.rcClose.bottom      = gFind.rcDownArrow.bottom

    ' Results from search eg. "1 of 10"
    gFind.rcResults.left       = gFind.rcDownArrow.right + pWindow->ScaleX(4)
    gFind.rcResults.top        = gFind.rcDownArrow.top
    gFind.rcResults.right      = gFind.rcClose.Left
    gFind.rcResults.bottom     = gFind.rcDownArrow.bottom

    AfxRedrawWindow( HWND_FRMMAIN_TOPTABSFIND )

    function = 0
end function


' ========================================================================================
' frmTopTabsFind_RichEdit_SubclassProc Window procedure
' ========================================================================================
Function frmTopTabsFind_RichEdit_SubclassProc ( _
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
                cueBannerText = L(158,"Find") & "..."
                SelectObject( hdc, gFind.hCueBannerFont )
                SetTextColor( hdc, ghTopTabs.ForeColor )
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
            dim hPopUpMenu As HMENU = CreatePopupMenu()
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
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMMAIN_TOPTABSFIND, 0 ) 
            select case nResult
                case IDM_CUT:   SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:  SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE: SendMessage( hWin, WM_PASTE, 0, 0 )
            end select
            DestroyMenu hPopUpMenu

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmTopTabsFind_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function


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

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit function
    
    select case id

        case IDC_FRMTOPTABSFIND_TXTFIND
            if (codeNotify = EN_CHANGE) then
                gFind.txtFind = AfxGetWindowText(hwndCtl)
                FindReplace_HighlightSearches()
                dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
                FindReplace_NextSelection(startPos, true, true)
                SetFocus( hwndCtl )
            elseif (codeNotify = EN_SETFOCUS) then
                AfxRedrawWindow( hwnd )
            end if
                
    end select
    
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

    static as DWSTRING wszPrevTooltip 
    static as HWND hTooltip 
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsFind_OnSize)
        HANDLE_MSG (hwnd, WM_COMMAND, frmTopTabsFind_OnCommand)
            
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMMAIN_TOPTABSFIND = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSFIND)
        if pWindow = 0 then exit function

        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
        TrackMouseEvent(@tme) 

        if IsWindow(hTooltip) = 0 then 
            hTooltip = AfxAddTooltip( hwnd, "", false, false )
            ' Set the tooltip max width in order to handle the multiline
            ' text needed for the split editor button.
            SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, 200 )
        end if
    
        AfxRedrawWindow( hwnd )
        
           
    case WM_MOUSELEAVE
        wszPrevTooltip = ""
        AfxRedrawWindow( hwnd )


    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip 
        if isMouseOverRECT( hwnd, gFind.rcToggle ) then
            wszTooltip  = frmPanelMenu_FormatTooltip( IDM_TOGGLEREPLACE )
        elseif isMouseOverRECT( hwnd, gFind.rcMatchCase ) then
            wszTooltip = L(163,"Match Case")
        elseif isMouseOverRECT( hwnd, gFind.rcWholeWord ) then
            wszTooltip = L(162,"Match Whole Words")
        elseif isMouseOverRECT(hwnd, gFind.rcUpArrow) then
            wszTooltip  = frmPanelMenu_FormatTooltip( IDM_FINDPREVACCEL )
        elseif isMouseOverRECT(hwnd, gFind.rcDownArrow) then
            wszTooltip  = frmPanelMenu_FormatTooltip( IDM_FINDNEXTACCEL )
        elseif isMouseOverRECT( hwnd, gFind.rcSelection ) then
            wszTooltip = L(148,"Selection")
        elseif isMouseOverRECT( hwnd, gFind.rcClose ) then
            wszTooltip = L(161,"Close") & " (Esc)"
        end if
        
        ' Display the tooltip
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip 
        end if    


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
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Paint the tab info area (top and bottom lines)
        b.SetPenColor( ghEditor.Divider )
        if gFind.bShowInfoPanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.top, b.rcClient.right, b.rcClient.top )
            ' Clear the bottom border of the active tab
            dim as long i = gTTabCtl.CurSel
            b.SetPenColor( ghTopTabs.BackColorHot )
            b.PaintLine( 1, gTTabCtl.tabs(i).rcTab.left, b.rcClient.top, _
                                gTTabCtl.tabs(i).rcTab.right, b.rcClient.top )
        end if                        
        ' Only paint the bottom line if Replace is not active.
        if gFind.bShowReplacePanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if     
        
        ' Draw the Find controls.
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )
        b.SetPenColor( ghEditor.Divider )
        
        b.PaintRoundBorderRect( @gTTabCtl.rcFindTextRect )

        b.SetFont( GUIFONT_9 )
        
        ' Match Case
        if gFind.nMatchCase then 
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.IconSelectedBackColor, ghTopTabs.IconSelectedBackColor )
        else
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )
        end if
        b.PaintIconButton( wszIconMatchCase, @gFind.rcMatchCase )

        ' Whole Word
        if gFind.nWholeWord then 
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.IconSelectedBackColor, ghTopTabs.IconSelectedBackColor )
        else
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )
        end if
        b.PaintIconButton( wszIconWholeWord, @gFind.rcWholeWord )

        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )

        b.PaintText( gFind.wszResults, @gFind.rcResults, DT_LEFT )
        
        b.SetFont( SYMBOLFONT_9 )
        b.PaintIconButton( wszIconToggleReplace, @gFind.rcToggle )
        
        if gFind.nSelection then 
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.IconSelectedBackColor, ghTopTabs.IconSelectedBackColor )
        else
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )
        end if
        b.PaintIconButton( wszIconSelection, @gFind.rcSelection )
            
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )

        b.PaintIconButton( wszIconChevronLeft, @gFind.rcUpArrow )
        b.PaintIconButton( wszIconChevronRight, @gFind.rcDownArrow )
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

    ' we use RichEdit controls because regular Edit controls can not easily have
    ' their text vertically centered.
    if gFind.hCueBannerFont = 0 then 
        gFind.hCueBannerFont = ghFont(INFOFONT_11)
    end if

    dim as RECT rc
    dim cf as CHARFORMATW 
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_FACE or CFM_SIZE or CFM_CHARSET or CFM_COLOR
    cf.crTextColor = ghTopTabs.ForeColorHot
    cf.yHeight = 11 * 20      ' in twips (1/20 pt)
    cf.bCharSet = DEFAULT_CHARSET
    cf.bPitchAndFamily = DEFAULT_PITCH or FF_DONTCARE
    cf.szFaceName = GUIFIXEDFONT
    
    dim as HWND hCtlFind
    
    hCtlFind = pWindow->AddControl("RICHEDIT", , IDC_FRMTOPTABSFIND_TXTFIND, "", _
                0, 0, 0, 0, _
                WS_CHILD or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
                WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtlFind, @frmTopTabsFind_RichEdit_SubclassProc, IDC_FRMTOPTABSFIND_TXTFIND, NULL)
    SendMessage( hCtlFind, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtlFind, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghTopTabs.BackColorHot) )
    
    function = 0
    
end function

