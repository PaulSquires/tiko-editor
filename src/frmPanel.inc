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
    dim as long nPanelHeaderHeight = pWindow->ScaleY(46)   ' EXPLORER, FUNCTIONS, BOOKMARKS

    ' show/hide of the child window is done separately in order to prevent blank client area from 
    ' painting before the lists are loaded. Allow a 3 pixel right border between listbox and
    ' form edge in order to allow for panel resizing. 
    dim as long nLeft = 0
    if gConfig.ExplorerPositionRight then nLeft = pWindow->ScaleX(3)  ' allow for grab
    
    dim as long nTop = 0
    dim as long nWidth = rc.right - rc.Left
    dim as long nHeight = nPanelMenuHeight

    nTop = rc.top + nPanelMenuHeight + nPanelHeaderHeight
    nWidth = nWidth - pWindow->ScaleX(3)   
    nHeight = rc.bottom - nTop

    ' Position the panel menu
    ' Give it some left margin 
    dim as long nMargin = pWindow->ScaleX(10)
    SetWindowPos( HWND_FRMPANEL_MENU, 0, _
            nLeft + nMargin, 0, nWidth - (nMargin * 2), nPanelMenuHeight, SWP_SHOWWINDOW or SWP_NOZORDER )
    
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

    dim as clsDoubleBuffer b
    
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
    b.SetPenColor( ghPanel.BackColorHot )
    b.PaintLine( 2, gPanel.rcDividerLine.left, gPanel.rcDividerLine.top, _
                    gPanel.rcDividerLine.right, gPanel.rcDividerLine.bottom )

    ' Right side vertical border line
    b.SetPenColor( ghEditor.Divider )
    b.PaintLine( 1, b.rcClient.right, b.rcClient.top, _
                    b.rcClient.right, b.rcClient.bottom )

    b.EndDoubleBuffer()

    function = 0
end function



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
            
        ' PANELS LEFT/RIGHT SPLITTER
        dim as point pt
        dim as Rect rc
        GetWindowRect HWND_FRMPANEL, @rc

        if gConfig.ExplorerPositionRight then
            rc.right = rc.left + pWindow->ScaleX(3)
        else
            rc.left = rc.right - pWindow->ScaleX(3)
        end if

        GetCursorPos(@pt)
        if PtInRect( @rc, pt ) then
            if WindowFrompoint(pt) = HWND_FRMPANEL then
                SetCursor( ghCursorSizeWE )
            end if
        end if

        if (gApp.bDragActive = true) andalso (gApp.hwndPanel = HWND_FRMPANEL) then
            ' Get the current rect of the panel and compare right side to the current cursor position
            ' and then move the right side to equal the cursor position.
            dim as long nDiff = pt.x - rc.right
            if gConfig.ExplorerPositionRight then nDiff = rc.left - pt.x
            GetClientRect HWND_FRMPANEL, @rc
            dim as long nWidth = (rc.right - rc.left + nDiff)
            ' make sure width does not go below a specific size otherwise the user will not
            ' be able to grab the panel edge to resize and the panel menu will squish together
            nWidth = max( nWidth, pWindow->ScaleX(236) ) 
            SetWindowPos( HWND_FRMPANEL, 0, 0, 0, nWidth, rc.Bottom - rc.Top, SWP_NOMOVE or SWP_NOZORDER )
            RedrawWindow( HWND_FRMPANEL, null, null, RDW_INVALIDATE or RDW_UPDATENOW or RDW_ALLCHILDREN)                
            frmMain_PositionWindows()
        end if
        
    case WM_LBUTTONDOWN
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL)
        if pWindow = 0 then exit function

        ' PANELS LEFT/RIGHT SPLITTER
        dim as Rect rc
        dim as point pt 
        gApp.bDragActive = false 
        GetWindowRect HWND_FRMPANEL, @rc

        if gConfig.ExplorerPositionRight then
            rc.right = rc.left + pWindow->ScaleX(3)
        else
            rc.left = rc.right - pWindow->ScaleX(3)
        end if

        GetCursorPos(@pt)
        if PtInRect( @rc, pt ) then
            if WindowFrompoint(pt) = HWND_FRMPANEL then
                SetCursor( ghCursorSizeWE )
                gApp.bDragActive = true 
                gApp.hwndPanel   = HWND_FRMPANEL
                SetCapture( HWND_FRMPANEL )
            end if
        end if

        
    case WM_LBUTTONUP
        if gApp.bDragActive then
            gApp.bDragActive = false 
            gApp.hwndPanel = 0
            ReleaseCapture()
        end if
        
        if isMouseOverRECT( hwnd, gPanel.rcActionMenu ) then
            dim as HMENU hPopupMenu 
            select case gPanel.hActiveChild
            case HWND_FRMEXPLORER
                hPopupMenu = CreateExplorerActionButtonContextMenu()
            case HWND_FRMFUNCTIONS
                hPopupMenu = CreateFunctionsActionButtonContextMenu()
            case HWND_FRMBOOKMARKS
                hPopupMenu = CreateBookmarksActionButtonContextMenu()
            end select
            ' Popup the menu to the bottom of the Action Button (right aligned)
            dim as RECT rc = gPanel.rcActionMenu   ' work with a copy
            MapWindowpoints( HWND_FRMPANEL, HWND_DESKTOP, cast(point ptr, @rc), 2 )
            TrackPopupMenu( hPopUpMenu, TPM_RIGHTALIGN, _
                                rc.right, rc.bottom, 0, HWND_FRMMAIN, byval null)
            DestroyMenu( hPopUpMenu )
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
