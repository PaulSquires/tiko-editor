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
''  frmPanelMenu.inc
''   

#include once "frmPanel.bi"
#include once "frmPanelMenu.bi"


' ========================================================================================
' Format the tooltip based on menu id
' ========================================================================================
function frmPanelMenu_FormatTooltip( byval id as integer ) as DWSTRING
    dim as DWSTRING wszTooltip = getMenuText(id)
    dim as DWSTRING wszCaption = rtrim(AfxStrParse(wszTooltip, 1, wchr(9)), "...")
    dim as DWSTRING wszShortcut = AfxStrParse(wszTooltip, 2, wchr(9))
    
    if len(wszShortcut) then wszShortcut = " (" & wszShortcut & ")"
    
    return wszCaption & wszShortcut
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmPanelMenu_PositionWindows() as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL_MENU)
    if pWindow = 0 then exit function
    
    ' Get the entire client area
    dim as Rect rc
    GetClientRect( HWND_FRMPANEL_MENU, @rc )

    dim as long nButtonWidth = pWindow->ScaleX( 24 )
    dim as long nButtonHeight = pWindow->ScaleY( 24 )
    dim as long nLeft = pWindow->ScaleX( 10 )
    dim as long nTop = pWindow->ScaleY( 8 )
    dim as long nRight = rc.right - pWindow->ScaleX( 6 )

    gPanelMenu(0).wszCaption = wszIconExplorer
    gPanelMenu(0).id = IDM_VIEWEXPLORER
    gPanelMenu(0).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(0).wszTooltip = frmPanelMenu_FormatTooltip(IDM_VIEWEXPLORER)
    
    nLeft += nButtonWidth
    gPanelMenu(1).wszCaption = wszIconFunctions
    gPanelMenu(1).id = IDM_FUNCTIONLIST
    gPanelMenu(1).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(1).wszTooltip = frmPanelMenu_FormatTooltip(IDM_FUNCTIONLIST)
    
    ' Bookmarks
    nLeft += nButtonWidth
    gPanelMenu(2).wszCaption = wszIconBookmarks
    gPanelMenu(2).id = IDM_BOOKMARKSLIST
    gPanelMenu(2).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(2).wszTooltip = frmPanelMenu_FormatTooltip(IDM_BOOKMARKSLIST)

    ' Settings (Environment Options)
    nLeft += nButtonWidth
    gPanelMenu(3).wszCaption = wszIconSettings
    gPanelMenu(3).id = IDM_OPTIONSDIALOG
    gPanelMenu(3).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(3).wszTooltip = frmPanelMenu_FormatTooltip(IDM_OPTIONSDIALOG)
    
    '(Right to left orientation)
    
    ' Build & Execute
    nLeft = nRight - nButtonWidth
    gPanelMenu(8).wszCaption = wszIconBuildExecute
    gPanelMenu(8).id = IDM_BUILDEXECUTE
    gPanelMenu(8).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(8).wszTooltip = frmPanelMenu_FormatTooltip(IDM_BUILDEXECUTE)

    ' Compile
    nLeft -= nButtonWidth
    gPanelMenu(7).wszCaption = wszIconCompile
    gPanelMenu(7).id = IDM_COMPILE
    gPanelMenu(7).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(7).wszTooltip = frmPanelMenu_FormatTooltip(IDM_COMPILE)

    ' Debug
    nLeft -= nButtonWidth
    gPanelMenu(6).wszCaption = wszIconDebug
    gPanelMenu(6).id = IDM_DEBUG_STARTDEBUGGING
    gPanelMenu(6).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(6).wszTooltip = frmPanelMenu_FormatTooltip(IDM_DEBUG_STARTDEBUGGING)

    ' Save
    nLeft -= nButtonWidth
    gPanelMenu(5).wszCaption = wszIconSave
    gPanelMenu(5).id = IDM_FILESAVEALL
    gPanelMenu(5).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(5).wszTooltip = frmPanelMenu_FormatTooltip(IDM_FILESAVEALL)

    ' Find in Project
    nLeft -= nButtonWidth
    gPanelMenu(4).wszCaption = wszIconFind
    gPanelMenu(4).id = IDM_FINDINPROJECT
    gPanelMenu(4).rc = type(nLeft, nTop, nLeft + nButtonWidth, nTop + nButtonHeight)
    gPanelMenu(4).wszTooltip = frmPanelMenu_FormatTooltip(IDM_FINDINPROJECT)

    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmPanelMenu
' ========================================================================================
function frmPanelMenu_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmPanelMenu_PositionWindows
    end if
    function = 0
end function
        

' ========================================================================================
' Process WM_PAINT message for window/dialog: frmPanelMenu
' ========================================================================================
function frmPanelMenu_OnPaint( byval hwnd as HWND ) as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            
    dim as clsDoubleBuffer b
    
    b.BeginDoubleBuffer(hwnd)

    ' Set default fore and back colors
    b.SetForeColors( ghPanel.ForeColor, ghPanel.ForeColorHot )
    b.SetBackColors( ghPanel.BackColor, ghPanel.BackColorHot )

    ' Clear the entire back client area
    b.PaintClientRect()
    
    b.SetFont( ghFont(SYMBOLFONT_10) )

    for i as integer = lbound(gPanelMenu) to ubound(gPanelMenu)
        dim as DWSTRING wszCaption = gPanelMenu(i).wszCaption
        dim as integer id       = gPanelMenu(i).id
        dim as RECT rc          = gPanelMenu(i).rc

        b.SetForeColors( ghPanel.ForeColorHot, ghPanel.ForeColorHot )
        b.SetBackColors( ghPanel.BackColor, ghPanel.BackColorHot )
        
        ' Draw the icon
        b.PaintIconButton( wszCaption, @rc )

        ' Should we underline this icon as actively selected    
        dim as boolean bDrawUnderline = false
        if (id = IDM_VIEWEXPLORER) andalso _
            (gPanel.hActiveChild = HWND_FRMEXPLORER) then bDrawUnderline = true
        if (id = IDM_FUNCTIONLIST) andalso _
            (gPanel.hActiveChild = HWND_FRMFUNCTIONS) then bDrawUnderline = true
        if (id = IDM_BOOKMARKSLIST) andalso _
            (gPanel.hActiveChild = HWND_FRMBOOKMARKS) then bDrawUnderline = true
        
        if (bDrawUnderline = true) andalso (pWindow <> 0) then
            rc.left += pWindow->ScaleX(2)
            rc.right -= pWindow->ScaleX(2)
            b.SetPenColor( ghOutput.ForeColor )
            b.PaintLine( 1, rc.left, rc.bottom, rc.right, rc.bottom )
        end if
        
    next
    
    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmPanelMenu
' ========================================================================================
function frmPanelMenu_OnDestroy( byval hwnd as HWND ) as LRESULT 
    function = 0
end function


' ========================================================================================
' frmPanelMenu Window procedure
' ========================================================================================
function frmPanelMenu_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as DWSTRING wszPrevTooltip 
    static as HWND hTooltip
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,  frmPanelMenu_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT, frmPanelMenu_OnPaint)
        HANDLE_MSG (hwnd, WM_DESTROY, frmPanelMenu_OnDestroy)
    
    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMPANEL_MENU = 0
        return 0

    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL_MENU)
        if pWindow = 0 then exit function

        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme) 
        
        if IsWindow(hTooltip) = 0 then hTooltip = AfxAddTooltip( hwnd, "", false, false )

        for i as integer = lbound(gPanelMenu) to ubound(gPanelMenu)
            dim as boolean isHot = isMouseOverRECT(hwnd, gPanelMenu(i).rc)
            if isHot <> gPanelMenu(i).isPrevHot then
                gPanelMenu(i).isPrevHot = isHot
                AfxRedrawWindow( hwnd )
            end if
        next

    case WM_LBUTTONDOWN
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMPANEL_MENU)
        if pWindow = 0 then exit function

        
    case WM_LBUTTONUP
        dim as integer id = -1
        for i as integer = lbound(gPanelMenu) to ubound(gPanelMenu)
            dim as boolean isHot = isMouseOverRECT(hwnd, gPanelMenu(i).rc)
            if isHot then
                id = gPanelMenu(i).id
                exit for
            end if
        next
        if id <> -1 then
            frmMain_OnCommand( HWND_FRMMAIN, id, 0, 0 )
            AfxRedrawWindow(hwnd)
        end if


    case WM_MOUSELEAVE
        wszPrevTooltip = ""
        for i as integer = lbound(gPanelMenu) to ubound(gPanelMenu)
            gPanelMenu(i).isPrevHot = false
        next
        AfxRedrawWindow(hwnd)


    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip 
        for i as integer = lbound(gPanelMenu) to ubound(gPanelMenu)
            dim as boolean isHot = isMouseOverRECT(hwnd, gPanelMenu(i).rc)
            if isHot then
                wszTooltip = gPanelMenu(i).wszTooltip
                exit for
            end if
        next
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip
        end if
        
    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmPanelMenu_Show
' ========================================================================================
function frmPanelMenu_Show( byval hwndParent as hwnd ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMPANEL_MENU = pWindow->Create( hwndParent, @frmPanelMenu_WndProc, 0, 0, 0, 0)
    pWindow->ClassStyle = CS_DBLCLKS

    function = 0
    
end function
