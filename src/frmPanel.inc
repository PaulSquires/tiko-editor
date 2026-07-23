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
    

''
''  frmPanel.inc
''   

#include once "frmPanel.bi"
#include once "frmPanelMenu.bi"
#include once "frmExplorer.bi"
#include once "frmBookmarks.bi"
#include once "frmFunctions.bi"


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmPanel_PositionWindows() as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL)
    if pWindow = 0 then exit function
    
    ' Get the entire client area
    dim as RECT rc: GetClientRect( HWND_FRMPANEL, @rc )
    
    dim as long nPanelMenuHeight = pWindow->ScaleY(TOPTABS_HEIGHT)
    dim as long nPanelHeaderHeight = pWindow->ScaleY(45)   ' EXPLORER, FUNCTIONS, BOOKMARKS

    ' show/hide of the child window is done separately in order to prevent blank client area from 
    ' painting before the lists are loaded. Allow a 3 pixel right border between listbox and
    ' form edge in order to allow for panel resizing. 
    dim as long nLeft = 0
    
    dim as long nTop = 0
    dim as long nWidth = rc.right - rc.Left
    dim as long nHeight = nPanelMenuHeight 

    nTop = rc.top + nPanelMenuHeight + nPanelHeaderHeight

    nHeight = rc.bottom - nTop

    ' Position the panel menu
    ' Give it some left margin 
    dim as long nMargin = pWindow->ScaleX(10)
    SetWindowPos( HWND_FRMPANEL_MENU, 0, _
            nLeft + nMargin, 0, nWidth - (nMargin * 2), _
            nPanelMenuHeight - pWindow->ScaleY(1), SWP_SHOWWINDOW or SWP_NOZORDER )
    
    SetWindowPos( HWND_FRMEXPLORER, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER )
    SetWindowPos( HWND_FRMBOOKMARKS, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER )
    SetWindowPos( HWND_FRMFUNCTIONS, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER )

    select case gPanel.hActiveChild
        case HWND_FRMEXPLORER
            ShowWindow( HWND_FRMBOOKMARKS, SW_HIDE )
            ShowWindow( HWND_FRMFUNCTIONS, SW_HIDE )

            gPanel.wszHeader = L(247,"EXPLORER")
            ShowWindow( HWND_FRMEXPLORER, SW_SHOW )

        case HWND_FRMBOOKMARKS
            ShowWindow( HWND_FRMEXPLORER, SW_HIDE )
            ShowWindow( HWND_FRMFUNCTIONS, SW_HIDE )

            gPanel.wszHeader = ucase( L(188,"Bookmarks") )
            ShowWindow( HWND_FRMBOOKMARKS, SW_SHOW )
        
        case HWND_FRMFUNCTIONS
            ShowWindow( HWND_FRMEXPLORER, SW_HIDE )
            ShowWindow( HWND_FRMBOOKMARKS, SW_HIDE )

            gPanel.wszHeader = ucase( L(223,"Function List") )
            ShowWindow( HWND_FRMFUNCTIONS, SW_SHOW )

    end select

    ' Calculate the divider line
    nWidth = rc.right - rc.Left
    gPanel.rcDividerLine = type( 0, nPanelMenuHeight, nWidth, nPanelMenuHeight )
    
    ' calculate the rect for the panel label (EXPLORER, FUNCTION LIST, BOOKMARKS)
    with gPanel.rcHeader 
        .left = pWindow->ScaleX( 10 )
        .top = nPanelMenuHeight + pWindow->ScaleY( 10 )
        .bottom = .top + pWindow->ScaleY( 20 )
        .right = .left + _
                pWindow->ScaleX(getTextWidth(HWND_FRMPANEL, gPanel.wszHeader, GUIFONT_9) + 10) 
    end with

    ' calculate the actual more actions "..." button itself
    dim as long ActionButtonWidth = 24
    dim as long ActionButtonHeight = 22
    with gPanel.rcActionMenu
        .left = rc.right - pWindow->ScaleX(ActionButtonWidth + 10)
        .top = nPanelMenuHeight + pWindow->ScaleY(7)
        .right = .left + pWindow->ScaleX(ActionButtonWidth)
        .bottom = .top + pWindow->ScaleY(ActionButtonHeight)
    end with
        
    AfxRedrawWindow( HWND_FRMPANEL )

    ' Ensure that the panel menu selected item is in sync with the pane
    AfxRedrawWindow(HWND_FRMPANEL_MENU)
    
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmPanel
' ========================================================================================
function frmPanel_OnDestroy( byval hwnd as HWND ) as LRESULT
    function = 0
end Function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmPanel
' ========================================================================================
function frmPanel_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmPanel_PositionWindows
    end if
    function = 0
end function
        
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: frmPanel
' ========================================================================================
function frmPanel_OnPaint( byval hwnd as HWND ) as LRESULT
            
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL)
    if pWindow = 0 then exit function

    dim as CBufferPaint b
    
    b.BeginDoubleBuffer(hwnd)

    ' Set default fore and back colors
    b.SetColors( ghPanel.ForeColor, ghPanel.BackColor )

    ' Clear the entire back client area
    b.PaintClientRect()

    ' Draw the panel Label
    b.SetFont( ghFont(GUIFONT_9) )
    b.PaintText( gPanel.wszHeader, @gPanel.rcHeader, DT_LEFT )

    ' Draw the "..." menu item (hot when the mouse is over it)
    b.SetFont( ghFont(SYMBOLFONT_10) )
    if isMouseOverRECT( hwnd, gPanel.rcActionMenu ) then
        b.SetColors( ghPanel.ForeColorHot, ghPanel.BackColorHot )
    else
        b.SetColors( ghPanel.ForeColor, ghPanel.BackColor )
    end if
    b.PaintRoundRect( @gPanel.rcActionMenu )
    b.PaintText( wszIconMoreActions, @gPanel.rcActionMenu, DT_CENTER )

    ' Paint a separator line at the top of the window
    b.SetPenColor( ghEditor.Divider )
    b.PaintLine( 1, gPanel.rcDividerLine.left, gPanel.rcDividerLine.top, _
                    gPanel.rcDividerLine.right, gPanel.rcDividerLine.bottom )

    b.EndDoubleBuffer()

    function = 0
end function



' ========================================================================================
' The Explorer panel's splitter bar. It is a CSplitter owned by frmMain (the bar sits
' BETWEEN the panel and the content area, and frmMain owns that layout), so only the two
' callbacks live here.
'
' Gone with the migration: the 3px edge hit-test in WM_MOUSEMOVE, the whole WM_LBUTTONDOWN
' handler, the WM_LBUTTONUP drag teardown, and the gApp.bDragActive / gApp.hwndPanel
' globals they drove. The panel docks on either side (gConfig.ExplorerPositionRight) and
' one vertical bar covers both -- the position is an absolute coordinate, so only the
' conversion to and from the panel's width mirrors.
' ========================================================================================

' Paint: the Explorer panel's background across the whole grab area, with a single 1 pixel
' vertical divider line centered in it (the bar is vertical, so this is the transposed twin
' of frmOutput_SplitterPaint). The line stays 1 physical pixel at every DPI -- it is a rule,
' not a band, so it is deliberately not scaled.
sub frmPanel_SplitterPaint( byval p as CSPLITTER_PAINTINFO ptr )
    dim as CBufferPaint ptr b = p->b

    b->SetBackColor( ghPanel.BackColor )
    b->PaintClientRect()

    dim as RECT rc = p->rcClient
    dim as long nMid = rc.left + ((rc.right - rc.left) \ 2)
    rc.left = nMid
    rc.right = nMid + 1

    ' Constant color -- deliberately no isHot/isDragging highlight. The cursor change
    ' from the control is the only hover feedback this bar gets.
    b->SetBackColor( ghEditor.Divider )
    b->PaintRect( @rc )
end sub


' The user dragged the bar. The control has already moved itself; we convert its new LEFT
' EDGE back into the panel's width -- mirrored by dock side -- and re-lay out.
sub frmPanel_SplitterMoved( _
            byval hSplitter as HWND, _
            byval newPos    as integer, _
            byval nPhase    as integer _
            )

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit sub
    if IsWindow( HWND_FRMPANEL ) = 0 then exit sub

    select case nPhase
        case CSPLITTER_PHASE_MOVE
            dim as RECT rcMain : GetClientRect( HWND_FRMMAIN, @rcMain )
            dim as long nGrab = pWindow->ScaleX(SPLITTER_GRAB)

            ' Docked left the bar's left edge IS the panel width; docked right the panel
            ' runs from the bar's right edge to the client edge.
            dim as long nWidth = newPos
            if gConfig.ExplorerPositionRight then nWidth = rcMain.right - newPos - nGrab
            if nWidth < 0 then nWidth = 0

            dim as RECT rc : GetClientRect( HWND_FRMPANEL, @rc )
            SetWindowPos( HWND_FRMPANEL, 0, 0, 0, nWidth, rc.Bottom - rc.Top, _
                          SWP_NOMOVE or SWP_NOZORDER )
            RedrawWindow( HWND_FRMPANEL, null, null, _
                          RDW_INVALIDATE or RDW_UPDATENOW or RDW_ALLCHILDREN )
            frmMain_PositionWindows()

        case CSPLITTER_PHASE_END
            frmMain_PositionWindows()
    end select
end sub


' ========================================================================================
' frmPanel Window procedure
' ========================================================================================
function frmPanel_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as boolean isPrevHotAction
    
    static as DWSTRING wszPrevTooltip
    static as HWND hTooltip 
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,    frmPanel_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,   frmPanel_OnPaint)
        HANDLE_MSG (hwnd, WM_DESTROY, frmPanel_OnDestroy)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMPANEL = 0
        return 0

    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL)
        if pWindow = 0 then exit function

        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
        TrackMouseEvent(@tme) 

        if IsWindow(hTooltip) = 0 then hTooltip = AfxAddTooltip( hwnd, "", false, false )

        ' Repaint the gPanelMenuRect to ensure Hot highlighting
        dim as boolean isHotAction, isHotButton0, isHotButton1 
        isHotAction = isMouseOverRECT( hwnd, gPanel.rcActionMenu )

        if isHotAction <> isPrevHotAction then   
            AfxRedrawWindow( hwnd )
            isPrevHotAction = isHotAction
        end if   
            
        ' The left/right splitter hit-test and drag that used to follow are the CSplitter
        ' control's job now (frmPanel_SplitterMoved), as is the WM_LBUTTONDOWN handler that
        ' started the drag.

    case WM_LBUTTONUP
        if isMouseOverRECT( hwnd, gPanel.rcActionMenu ) then
            ' Popup the menu to the bottom of the Action Button (right aligned). Which
            ' of the three pane menus appears follows gPanel.hActiveChild, decided inside
            ' ShowPanelActionContextMenu (modContextMenus.inc).
            dim as RECT rc = gPanel.rcActionMenu   ' work with a copy
            MapWindowpoints( HWND_FRMPANEL, HWND_DESKTOP, cast(point ptr, @rc), 2 )
            ShowPanelActionContextMenu( rc )
            Return true   ' prevent further processing that leads to WM_CONTEXTMENU
        end if
        

     case WM_MOUSELEAVE
        wszPrevTooltip = ""
        isPrevHotAction = false
        AfxRedrawWindow(hwnd)


    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip 
        if isMouseOverRECT( hwnd, gPanel.rcActionMenu ) then
            wszTooltip  = L(440, "More Actions") & "..."
        end if
        ' Display the tooltip
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip 
        end if    
        
    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmPanel_Show
' ========================================================================================
function frmPanel_Show( byval hwndParent as hwnd ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    ' Only make the Panel initially visible if it was already visible
    ' when the most previous instance of the program closed. Also, set the width of
    ' the window to the last used visible width.
    dim as long nWidth = iif(gConfig.ShowPanel, gConfig.ShowPanelWidth, 250)
    
    HWND_FRMPANEL = pWindow->Create( hwndParent, @frmPanel_WndProc, 0, 0, nWidth, 0 )
    ShowWindow( HWND_FRMPANEL, iif(gConfig.ShowPanel, SW_SHOW, SW_HIDE) )

    ' Create the child windows
    frmPanelMenu_Show( HWND_FRMPANEL )
    frmExplorer_Show( HWND_FRMPANEL )
    frmBookmarks_Show( HWND_FRMPANEL )
    frmFunctions_Show( HWND_FRMPANEL )
    gPanel.hActiveChild = HWND_FRMEXPLORER
    
    frmPanel_PositionWindows()

    function = 0
    
end function
