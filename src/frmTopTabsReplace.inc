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
function frmTopTabsReplace_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSREPLACE)
    if pWindow = 0 then exit function

    dim as HWND hCtlReplace = GetDlgItem(HWND_FRMMAIN_TOPTABSREPLACE, IDC_FRMTOPTABSREPLACE_TXTREPLACE)
    
    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_TOPTABSREPLACE, @rc )
            
    ' Calculate the RECT positions for all of the top tabs
    dim as long iconWidth
    dim as long iconHeight 
    dim as long nPadding = 10   ' 5 left 5 right
    dim as long vmargin = pWindow->ScaleY(4)
    dim as long nLeft
    dim as long nTop
    dim as long nWidth 
    dim as long nHeight
    
    gTTabCtl.rcReplaceTextRect = rc
    gTTabCtl.rcReplaceTextRect.left   = gTTabCtl.rcFindTextRect.left
    gTTabCtl.rcReplaceTextRect.top    = 0
    gTTabCtl.rcReplaceTextRect.right  = gTTabCtl.rcFindTextRect.right
    gTTabCtl.rcReplaceTextRect.bottom = pWindow->ScaleY(TOPTABS_REPLACE_HEIGHT) - pWindow->ScaleY(8)

    rc      = gTTabCtl.rcReplaceTextRect
    nLeft   = rc.left + vmargin + pWindow->ScaleX(10)
    nTop    = rc.top + vmargin
    nWidth  = (rc.right - rc.left - vmargin*2 - pWindow->ScaleX(10))
    nHeight = (rc.bottom - rc.top - vmargin*2)

    ' Make the width of the textbox smaller because we want to visually fit the
    ' Preserve Case icon into the Replace text rect. nHeight will be our icon 
    ' height and width as well.
    iconWidth = nHeight
    iconHeight = nHeight
    nWidth = nWidth - iconWidth - pWindow->ScaleX(4)

    SetWindowPos( hCtlReplace, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )
    SendMessage( hCtlReplace, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghTopTabs.BackColorHot) )
    CenterSingleLineText( hCtlReplace )    
    SendMessage( hCtlReplace, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )

    ' Preserve Case RECT
    gFind.rcPreserve.left   = nLeft + nWidth
    gFind.rcPreserve.top    = nTop
    gFind.rcPreserve.right  = gFind.rcPreserve.Left + iconWidth
    gFind.rcPreserve.bottom = gFind.rcPreserve.top + iconHeight
    
    ' Replace RECT
    gFind.rcReplace.Left    = rc.right + pWindow->ScaleX(8)
    gFind.rcReplace.top     = gFind.rcPreserve.top
    gFind.rcReplace.right   = gFind.rcReplace.Left + iconWidth
    gFind.rcReplace.bottom  = gFind.rcPreserve.bottom

    ' ReplaceAll RECT
    gFind.rcReplaceAll.Left   = gFind.rcReplace.right 
    gFind.rcReplaceAll.top    = gFind.rcReplace.top
    gFind.rcReplaceAll.right  = gFind.rcReplaceAll.Left + iconWidth
    gFind.rcReplaceAll.bottom = gFind.rcReplace.bottom

    AfxRedrawWindow( HWND_FRMMAIN_TOPTABSREPLACE )

    function = 0
end function


' ========================================================================================
' frmTopTabsReplace_RichEdit_SubclassProc Window procedure
' ========================================================================================
Function frmTopTabsReplace_RichEdit_SubclassProc ( _
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
                hWin = GetDlgItem( HWND_FRMMAIN_TOPTABSREPLACE, IDC_FRMTOPTABSREPLACE_TXTREPLACE )
                cueBannerText = L(173,"Replace") & "..."
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
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMMAIN_TOPTABSREPLACE, 0 ) 
            select case nResult
                case IDM_CUT:   SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:  SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE: SendMessage( hWin, WM_PASTE, 0, 0 )
            end select
            DestroyMenu hPopUpMenu

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmTopTabsReplace_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmTopTabsReplace
' ========================================================================================
function frmTopTabsReplace_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmTopTabsReplace_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmTopTabsReplace
' ========================================================================================
function frmTopTabsReplace_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        case IDC_FRMTOPTABSREPLACE_TXTREPLACE
            if (codeNotify = EN_SETFOCUS) then
                AfxRedrawWindow( hwnd )
            end if

    end select
    
    function = 0
end function


' ========================================================================================
' frmTopTabsReplace Window procedure
' ========================================================================================
function frmTopTabsReplace_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as DWSTRING wszPrevTooltip 
    static as HWND hTooltip 
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsReplace_OnSize)
        HANDLE_MSG (hwnd, WM_COMMAND, frmTopTabsReplace_OnCommand)
            
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMMAIN_TOPTABSREPLACE = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSREPLACE)
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
        if isMouseOverRECT( hwnd, gFind.rcPreserve ) then
            wszTooltip = L(424,"Preserve Case")
        elseif isMouseOverRECT( hwnd, gFind.rcReplace ) then
            wszTooltip = L(173,"Replace") & " (Enter)"
        elseif isMouseOverRECT( hwnd, gFind.rcReplaceAll ) then
            wszTooltip = L(174,"Replace All") & " (Ctrl+Alt+Enter)"
        end if
        
        ' Display the tooltip
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip 
        end if    


    case WM_LBUTTONUP
        if isMouseOverRECT( hwnd, gFind.rcPreserve ) then
            gFind.nPreserve = not gFind.nPreserve
            AfxRedrawWindow(hwnd)

        elseif isMouseOverRECT( hwnd, gFind.rcReplace ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACENEXT, 0), 0 )

        elseif isMouseOverRECT( hwnd, gFind.rcReplaceAll ) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEALL, 0), 0 )
        end if
            

    case WM_PAINT
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Paint the bottom line that separates from the code editing window.
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )

        ' Draw the Replace controls.
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )
        b.SetPenColor( ghEditor.Divider )
        
        b.PaintRoundBorderRect( @gTTabCtl.rcReplaceTextRect )

        b.SetFont( GUIFONT_9 )
        if gFind.nPreserve then 
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.IconSelectedBackColor, ghTopTabs.IconSelectedBackColor )
        else
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )
        end if
        b.PaintIconButton( wszIconPreserveCase, @gFind.rcPreserve )
       
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )

        b.SetFont( SYMBOLFONT_9 )
        b.PaintIconButton( wszIconReplace, @gFind.rcReplace )
        b.PaintIconButton( wszIconReplaceAll, @gFind.rcReplaceAll )
                
        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmTopTabsReplace_Show
' ========================================================================================
function frmTopTabsReplace_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_TOPTABSREPLACE = pWindow->Create( hwndParent, @frmTopTabsReplace_WndProc, _
        0, 0, 0, TOPTABS_REPLACE_HEIGHT, _
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
    
    dim as HWND hCtlReplace
    
    hCtlReplace = pWindow->AddControl("RICHEDIT", , IDC_FRMTOPTABSREPLACE_TXTREPLACE, gFind.txtReplace, _
                0, 0, 0, 0, _
                WS_CHILD or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
                WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtlReplace, @frmTopTabsReplace_RichEdit_SubclassProc, IDC_FRMTOPTABSREPLACE_TXTREPLACE, NULL)
    SendMessage( hCtlReplace, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtlReplace, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghTopTabs.BackColorHot) )

    function = 0
    
end function

