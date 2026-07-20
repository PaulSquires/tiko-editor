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
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmMenuBar_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    If pWindow = 0 Then Exit function
    
    dim as long nLeft = pWindow->ScaleX(6)
    dim as long nTop = 0
    dim as long nWidth = 0
    dim as long nHeight = AfxGetWindowHeight(HWND_FRMMAIN_MENUBAR)
    dim as long nPadding = 12   ' 6 left 6 right
    
    dim id( 0 to ... ) as long = _
        { _
        IDC_MENUBAR_FILE, IDC_MENUBAR_EDIT, IDC_MENUBAR_SEARCH, IDC_MENUBAR_VIEW, _
        IDC_MENUBAR_PROJECT, IDC_MENUBAR_COMPILE, IDC_MENUBAR_DEBUG, IDC_MENUBAR_HELP _
        }

    redim gMenuBar(ubound(id)) as MENUBAR_ITEM
        
    for i as integer = lbound(id) to ubound(id)
        gMenuBar(i).id = id(i)
        gMenuBar(i).wszText = getMenuText(gMenuBar(i).id)
        dim as long nWidth = pWindow->ScaleX(getTextWidth( HWND_FRMMAIN, gMenuBar(i).wszText, GUIFONT_10) + nPadding)
        gMenuBar(i).rcItem = type(nLeft, nTop, nLeft + nWidth, nTop + nHeight)
        nLeft += nWidth
    next
    
    ' Position the Tiko Update Available message
    gMenuUpdateMessage.id = IDC_MENUBAR_UPDATE
    gMenuUpdateMessage.wszText = getMenuText(gMenuUpdateMessage.id)
    nWidth = pWindow->ScaleX(getTextWidth( HWND_FRMMAIN, gMenuUpdateMessage.wszText, GUIFONT_10) + nPadding)
    nLeft = AfxGetWindowClientWidth( HWND_FRMMAIN ) - nWidth - nPadding
    gMenuUpdateMessage.rcItem = type<RECT>(nLeft, nTop, nLeft + nWidth, nTop + nHeight)
    
    AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )

    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmMenuBar
' ========================================================================================
function frmMenuBar_OnDestroy( byval hwnd as HWND ) as LRESULT
    function = 0
end Function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmMenuBar
' ========================================================================================
function frmMenuBar_OnSize( _
            byval hwnd as HWND,_
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    If state <> SIZE_MINIMIZED Then
        ' Position all of the child windows
        frmMenuBar_PositionWindows
    end If
    function = 0
end function


' ========================================================================================
' frmMenuBar Window procedure
' ========================================================================================
function frmMenuBar_GetHotIndex( byval hwnd as HWND ) as long
    dim as integer idx = -1
    for i as integer = lbound(gMenuBar) to ubound(gMenuBar)
        if isMouseOverRECT( hwnd, gMenuBar(i).rcItem ) then
            idx = i: exit for
        end if
    next
    function = idx
end function


' ========================================================================================
' frmMenuBar Window procedure
' ========================================================================================
function frmMenuBar_WndProc( _
            byval hwnd   as HWND,_
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as integer PrevHotIndex

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,    frmMenuBar_OnSize)
        HANDLE_MSG (hwnd, WM_DESTROY, frmMenuBar_OnDestroy)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_MENUBAR = 0
        return 0

    case WM_ERASEBKGND
        return true

    case WM_PAINT
        dim as clsDoubleBuffer b

        b.BeginDoubleBuffer(hwnd)
        b.SetBackColor( ghMenuBar.BackColor )
        b.PaintClientRect()

        b.SetFont( ghFont(GUIFONT_10) )
        for i as integer = lbound(gMenuBar) to ubound(gMenuBar)
            if i = gActiveMenuBarIndex then
                b.SetColors( ghMenuBar.ForeColorHot, ghMenuBar.BackColorHot )
            else
                ' back stays normal; only the TEXT goes hot on hover
                dim as boolean isHot = isMouseOverRECT( hwnd, gMenuBar(i).rcItem )
                b.SetColors( iif(isHot, ghMenuBar.ForeColorHot, ghMenuBar.ForeColor), _
                             ghMenuBar.BackColor )
            end if
            b.PaintRect( @gMenuBar(i).rcItem )
            b.PaintText( gMenuBar(i).wszText, @gMenuBar(i).rcItem, DT_CENTER )
        next

        ' Paint the Update Available message
        if len(gMenuUpdateMessage.wszText) then
            if isMouseOverRECT( hwnd, gMenuUpdateMessage.rcItem ) then
                b.SetColors( ghMenuBar.ForeColorHot, ghMenuBar.BackColorHot )
            else
                b.SetColors( ghMenuBar.ForeColor, ghMenuBar.BackColor )
            end if
            b.PaintRect( @gMenuUpdateMessage.rcItem )
            b.PaintText( gMenuUpdateMessage.wszText, @gMenuUpdateMessage.rcItem, DT_CENTER )
        end if
            
        b.EndDoubleBuffer()

        return 0


    case WM_MOUSEMOVE
        ' Get the button index of the current hot state button
        dim as integer CurHotIndex = frmMenuBar_GetHotIndex(hwnd)
        if CurHotIndex = -1 then PrevHotIndex = -1
        
        if CurHotIndex <> PrevHotIndex  then   
            ' A new menubar button has been moused over. If a previous popup
            ' menu exists then we will automatically continue to show a popup menu
            ' for the newly selected menubar button.
            gActiveMenuBarIndex = CurHotIndex 
            PrevHotIndex = CurHotIndex
            if isPopupMenuOpen() then
                frmPopupMenu_Show( ID_POPUP, 0, hwnd, gMenuBar(gActiveMenuBarIndex).rcItem )
            end if   
        end if   

        ' Track that we are over the control in order to catch the 
        ' eventual WM_MOUSELEAVE event
        dim tme as TrackMouseEvent
        tme.cbSize = Sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)
        AfxRedrawWindow( hwnd )
        
    case WM_MOUSELEAVE
        if isPopupMenuOpen() = false then
            gActiveMenuBarIndex = -1
            PrevHotIndex = -1
            AfxRedrawWindow(hwnd)
        end if   
        
    case WM_LBUTTONDOWN
        ' Check if the Tiko Update Available label has been pressed
        if isMouseOverRECT( hwnd, gMenuUpdateMessage.rcItem ) then
            ShellExecute( NULL, "open", "https://github.com/PaulSquires/tiko/releases", Null, Null, SW_SHOWNORMAL )
        end if

        ' Get the button index of the current hot state button or the wParam
        ' if this button press was sent by an ENTER keypress originating from
        ' handleKeysTopMenu() in modMsgPump.
        if (cint(wParam) <> -1) then   ' ENTER will send wParam = -1
            gActiveMenuBarIndex = frmMenuBar_GetHotIndex(hwnd)
        end if    
        
        ' activate the popup menu or toggle an existing one off 
        if isPopupMenuOpen() then
            killAllPopupMenus()
        else
            if gActiveMenuBarIndex <> -1 then
                frmPopupMenu_Show( ID_POPUP, 0, hwnd, gMenuBar(gActiveMenuBarIndex).rcItem )
            end if    
        end if
        AfxRedrawWindow(hwnd)
        
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function

        
' ========================================================================================
' frmMenuBar_Show
' ========================================================================================
function frmMenuBar_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    dim as long nHeight = iif(gConfig.CompactMenus, MENUBAR_HEIGHT_COMPACT, MENUBAR_HEIGHT)

    HWND_FRMMAIN_MENUBAR = pWindow->Create( hWndParent, @frmMenuBar_WndProc, 0, 0, 0, nHeight )
    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS
    
    frmMenuBar_PositionWindows()
    buildTopMenuDefinitions()
    
    function = 0
    
end function

