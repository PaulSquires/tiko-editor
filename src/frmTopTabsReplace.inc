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

    ' Vertical centering and colors are the CTextBox control's job.
    SetWindowPos( hCtlReplace, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

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
' CTextBox callback for the Replace textbox. The control owns the cue banner, colors,
' vertical centering, single-line enforcement and the Cut/Copy/Paste context menu that
' the old RichEdit subclass hand-rolled here. ENTER/TAB handling stays in
' handleKeysFindReplace (modMsgPump). No change callback: the replace text is read on
' demand by FindReplace_DoReplace.
' ========================================================================================
sub frmTopTabsReplace_TextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    ' Was the EN_SETFOCUS path: repaint the panel chrome around the textbox.
    if bGotFocus then AfxRedrawWindow( HWND_FRMMAIN_TOPTABSREPLACE )
end sub


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

    ' The Replace textbox notifies through its CTextBox focus callback, not through
    ' WM_COMMAND EN_ codes anymore.
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
        b.SetForeColor( ghTopTabs.ForeColorHot )
        b.SetBackColor( ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Paint the bottom line that separates from the code editing window.
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )

        ' Draw the Replace controls. The strip's field color is BackColorHot, so
        ' a hovered (unselected) button inverts to the tab area's normal BackColor.
        b.SetColors( ghTopTabs.ForeColorHot, ghTopTabs.BackColorHot )
        b.SetPenColor( ghEditor.Divider )

        b.PaintRoundBorderRect( @gTTabCtl.rcReplaceTextRect )

        b.SetFont( ghFont(GUIFONT_9) )
        if gFind.nPreserve then
            b.SetBackColor( ghTopTabs.IconSelectedBackColor )
        else
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcPreserve), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        end if
        b.PaintIconButton( wszIconPreserveCase, @gFind.rcPreserve )

        b.SetFont( ghFont(SYMBOLFONT_9) )
        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcReplace), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
        b.PaintIconButton( wszIconReplace, @gFind.rcReplace )
        b.SetBackColor( iif( isMouseOverRECT(hwnd, gFind.rcReplaceAll), _
                             ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
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

    ' The Replace textbox is the reusable CTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as HWND hCtlReplace = CTextBox_Create( HWND_FRMMAIN_TOPTABSREPLACE, IDC_FRMTOPTABSREPLACE_TXTREPLACE )
    CTextBox_SetFont( hCtlReplace, ghFont(INFOFONT_11) )
    CTextBox_SetForeColor( hCtlReplace, ghTopTabs.ForeColorHot )
    CTextBox_SetBackColor( hCtlReplace, ghTopTabs.BackColorHot )
    CTextBox_SetBorderWidth( hCtlReplace, 0 )                    ' flat, as before
    CTextBox_SetCueBannerText( hCtlReplace, L(173,"Replace") & "..." )
    CTextBox_SetCueBannerColor( hCtlReplace, ghTopTabs.ForeColor )
    CTextBox_SetFocusCallback( hCtlReplace, @frmTopTabsReplace_TextBoxFocus )
    CTextBox_SetText( hCtlReplace, gFind.txtReplace )            ' carried-over replace term

    function = 0
    
end function

