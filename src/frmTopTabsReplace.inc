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
dim shared ghTopTabsReplaceTip as HWND

enum
    TTRTIP_NONE = -1
    TTRTIP_PRESERVE = 0
    TTRTIP_REPLACE
    TTRTIP_REPLACEALL
end enum


' ========================================================================================
' Supply the tooltip text for the currently hot rect.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi). Returning "" means no tip for the rest of this hover.
private function frmTopTabsReplace_TipText( byval hTip as HWND, _
                                            byval hAttached as HWND, _
                                            byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    select case idx
        case TTRTIP_PRESERVE:    return L(424,"Preserve Case")
        case TTRTIP_REPLACE:     return L(173,"Replace") & " (Enter)"
        case TTRTIP_REPLACEALL:  return L(174,"Replace All") & " (Ctrl+Alt+Enter)"
    end select
    return ""
end function


' ========================================================================================
' Which of the bar's rects is the cursor over? TTRTIP_NONE for none of them.
' ========================================================================================
private function frmTopTabsReplace_HitTest( byval hwnd as HWND ) as long
    if isMouseOverRECT( hwnd, gFind.rcPreserve )   then return TTRTIP_PRESERVE
    if isMouseOverRECT( hwnd, gFind.rcReplace )    then return TTRTIP_REPLACE
    if isMouseOverRECT( hwnd, gFind.rcReplaceAll ) then return TTRTIP_REPLACEALL
    return TTRTIP_NONE
end function


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

    ' Vertical centering and colors are the PsTextBox control's job.
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
' PsTextBox callback for the Replace textbox. The control owns the cue banner, colors,
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

    ' The Replace textbox notifies through its PsTextBox focus callback, not through
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

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsReplace_OnSize)
        HANDLE_MSG (hwnd, WM_COMMAND, frmTopTabsReplace_OnCommand)

    case WM_DESTROY
        ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with the
        ' parent -- destroying it here is also what removes it from the shared clock's registry.
        if IsWindow(ghTopTabsReplaceTip) then DestroyWindow( ghTopTabsReplaceTip )
        ghTopTabsReplaceTip = 0

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_TOPTABSREPLACE = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSREPLACE)
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
        PsTooltip_SetToolIndex( ghTopTabsReplaceTip, frmTopTabsReplace_HitTest( hwnd ) )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghTopTabsReplaceTip, TTRTIP_NONE )
        AfxRedrawWindow( hwnd )


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
        dim as PsBufferPaint b
        
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

    ' The Replace textbox is the reusable PsTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as HWND hCtlReplace = PsTextBox_Create( HWND_FRMMAIN_TOPTABSREPLACE, IDC_FRMTOPTABSREPLACE_TXTREPLACE )
    PsTextBox_SetFont( hCtlReplace, ghFont(INFOFONT_11) )
    PsTextBox_SetForeColor( hCtlReplace, ghTopTabs.ForeColorHot )
    PsTextBox_SetBackColor( hCtlReplace, ghTopTabs.BackColorHot )
    PsTextBox_SetBorderWidth( hCtlReplace, 0 )                    ' flat, as before
    PsTextBox_SetCueBannerText( hCtlReplace, L(173,"Replace") & "..." )
    PsTextBox_SetCueBannerColor( hCtlReplace, ghTopTabs.ForeColor )
    PsTextBox_SetFocusCallback( hCtlReplace, @frmTopTabsReplace_TextBoxFocus )
    PsTextBox_SetText( hCtlReplace, gFind.txtReplace )            ' carried-over replace term
    ThemeTextBoxContextMenu( hCtlReplace )

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so a long tip still
    ' wraps at the same place; colours and fonts are pushed per hover from
    ' frmTopTabsReplace_TipText, so nothing set here can go stale on a theme change.
    ghTopTabsReplaceTip = PsTooltip_Create( HWND_FRMMAIN_TOPTABSREPLACE )
    PsTooltip_SetToolIndex( ghTopTabsReplaceTip, TTRTIP_NONE )
    PsTooltip_SetMaxWidth( ghTopTabsReplaceTip, 200 )
    PsTooltip_SetTooltipCallback( ghTopTabsReplaceTip, @frmTopTabsReplace_TipText )

    function = 0
    
end function

