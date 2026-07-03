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

#include once "frmStatusBar.bi"


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmStatusBar_PositionWindows() as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_STATUSBAR)
    if pWindow = 0 then exit function
    
    ' Get the entire client area
    dim as Rect rc
    GetClientRect(HWND_FRMMAIN_STATUSBAR, @rc)

    dim as long i = 0
    dim as long nLeft = rc.left
    dim as long nSpacer = pWindow->ScaleX(4)
    dim as long nPadding = 10   ' 5 left 5 right
    
    ' the text for each panel is set in frmMain_SetStatusbar which then calls this
    ' function to determine proper sizing and placement.
    for i as long = lbound(gSBPanels) to ubound(gSBPanels)
        SetRectEmpty( @gSBPanels(i).rc )
    next
    if gTTabCtl.GetActiveDocumentPtr() = 0 then
        AfxRedrawWindow( HWND_FRMMAIN_STATUSBAR )
        exit function
    end if

    '''' LEFT ALIGNED STATUSBAR PANELS ''''
    i = GOTO_PANEL  ' Ln 23, Col 20
    gSBPanels(i).rc.Left   = nLeft
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nLeft + _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_GOTO
    nLeft = gSBPanels(i).rc.right + nSpacer

    '''' RIGHT ALIGNED STATUSBAR PANELS ''''
    grcGripper = rc
    grcGripper.top = (rc.bottom-rc.top) / 2
    grcGripper.left = rc.right - (nSpacer * 4)
    
    i = CRLF_PANEL  ' CRLF 
    dim as long nRight = grcGripper.left
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_LINEENDINGS
    nRight = gSBPanels(i).rc.left - nSpacer

    i = UTF_PANEL  ' UTF-8
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight 
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_FILEENCODING
    nRight = gSBPanels(i).rc.left - nSpacer

    i = SPACES_PANEL  ' Spaces
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_SPACES
    nRight = gSBPanels(i).rc.left - nSpacer

    i = BUILD_POPUP_PANEL  ' Build Configuration
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_BUILDCONFIG_POPUP
    nRight = gSBPanels(i).rc.left - nSpacer

    i = BUILD_DIALOG_PANEL  ' Build Configuration dialog
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_BUILDCONFIG
    nRight = gSBPanels(i).rc.left - nSpacer

    i = THEMES_DIALOG_PANEL  ' Themes/Colors dialog
    gSBPanels(i).rc.Left   = nRight - _
        pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9) + nPadding )
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = nRight
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_THEMES
    nRight = gSBPanels(i).rc.left - nSpacer

    '''' THE MESSAGES PANEL FILLS THE REMAINING AVAILABLE SPACE ''''
    i = COMPILE_STATUS_PANEL  ' Compile status and error messages
    gSBPanels(i).rc.Left   = gSBPanels(0).rc.right
    gSBPanels(i).rc.top    = rc.top
    gSBPanels(i).rc.right  = gSBPanels(2).rc.Left
    gSBPanels(i).rc.bottom = rc.bottom
    gSBPanels(i).nID       = IDM_VIEWOUTPUT

    AfxRedrawWindow( HWND_FRMMAIN_STATUSBAR )
    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmStatusBar
' ========================================================================================
function frmStatusBar_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmStatusBar_PositionWindows()
    end if
    function = 0
end function


' ========================================================================================
' frmStatusBar Window procedure
' ========================================================================================
function frmStatusBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as long prev_idxHot = -1
    static as long curr_idxHot = -1
    static as boolean resizing
    static as point prev_pt

    static as HWND hTooltip
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)
    
    Select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmStatusBar_OnSize)
    
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMMAIN_STATUSBAR = 0
        
    case WM_ERASEBKGND
        return true

    case WM_LBUTTONDOWN
        SetCapture( hwnd )
        
    case WM_MOUSEMOVE
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
        TrackMouseEvent(@tme) 

        if IsWindow(hTooltip) = 0 then 
            hTooltip = AfxAddTooltip( hwnd, "", false, false )
        end if
        
        dim as point pt: GetCursorPos( @pt )
        
        ' Move the tooltip into place
        dim as DWSTRING wszTooltip
        if IsMouseOverRECT(hwnd,  gSBPanels(BUILD_DIALOG_PANEL).rc ) then 
            wszTooltip = L(277,"Build Configurations") & "..."
        end if
        if IsMouseOverRECT(hwnd,  gSBPanels(THEMES_DIALOG_PANEL).rc ) then 
            wszTooltip = L(111,"Themes and Fonts")
        end if
        if IsMouseOverRECT(hwnd,  gSBPanels(COMPILE_STATUS_PANEL).rc ) then 
            wszTooltip = L(251,"View Output Window")
        end if
        if (wszTooltip = "") andalso (IsWindow(hToolTip)) then 
            DestroyWindow(hToolTip)
        else
            dim as DWSTRING wszCurTooltip = AfxGetWindowText(hTooltip)
            if wszTooltip <> wszCurTooltip then
                AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            end if    
        end if
        
        SetWindowPos( hTooltip, 0, _
                        pt.x + AfxScaleX(12), pt.y + AfxScaleY(16), _
                        0, 0, SWP_NOZORDER or SWP_NOSIZE or SWP_NOACTIVATE )

        ' Are we over one of the status bar panels
        MapWindowPoints( HWND_DESKTOP, hwnd, cast( point ptr, @pt ), 1 )
        curr_idxHot = -1
        for i as long = lbound(gSBPanels) to ubound(gSBPanels)
            if PtInRect( @gSBPanels(i).rc, pt ) then
                ' Only make hot panels that have actual text
                if len(gSBPanels(i).wszText) then
                    curr_idxHot = i
                    gSBPanels(i).isHot = true
                end if   
            else   
                gSBPanels(i).isHot = false
            end if
        next
        if curr_idxHot <> prev_idxHot then
            AfxRedrawWindow( hwnd )
            prev_idxHot = curr_idxHot 
        end if

            
    case WM_MOUSELEAVE
        for i as long = lbound(gSBPanels) to ubound(gSBPanels)
            gSBPanels(i).isHot = false
        next
        prev_idxHot = -1
        curr_idxHot = -1
        AfxRedrawWindow( hwnd )


    case WM_LBUTTONUP
        dim as point pt: GetCursorPos( @pt )
        MapWindowPoints( HWND_DESKTOP, hwnd, cast( point ptr, @pt ), 1 )
        SetCursor( LoadCursor( null, IDC_ARROW ))
        ReleaseCapture
        resizing = false
        ' Are we over one of the status bar panels
        for i as long = lbound(gSBPanels) to ubound(gSBPanels)
            if PtInRect( @gSBPanels(i).rc, pt ) then
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(gSBPanels(i).nID, 0), 0 )
                exit for
            end if
        next   


    case WM_PAINT
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_STATUSBAR)
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set default fore and back colors
        b.SetForeColors( ghStatusBar.ForeColor, ghStatusBar.ForeColorHot )
        b.SetBackColors( ghStatusBar.BackColor, ghStatusBar.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()
      
        ' paint the panels
        for i as long = lbound(gSBPanels) to ubound(gSBPanels)
            if (i = BUILD_DIALOG_PANEL) orelse (i = THEMES_DIALOG_PANEL) then
                b.SetFont( SYMBOLFONT_9 )
            else
                b.SetFont( GUIFONT_9 )
            end if

            b.SetForeColors( ghStatusBar.ForeColor, ghStatusBar.ForeColorHot )
            b.SetBackColors( ghStatusBar.BackColor, ghStatusBar.BackColorHot )

            dim as boolean bHitTest = iif( i = COMPILE_STATUS_PANEL, false, true )
            b.PaintRect(@gSBPanels(i).rc, bHitTest)
            b.PaintText( gSBPanels(i).wszText, @gSBPanels(i).rc, DT_CENTER, bHitTest )
            
            ' if this is the compile result panel then display the green/success or red/failure "icon"
            ' also check if Spinner is active. 
            if i = COMPILE_STATUS_PANEL then
                if (gApp.IsProjectLoading = false) andalso (gApp.IsFileLoading = false) then
                    if pWindow then
                        if len(gSBPanels(i).wszText) = 0 then gApp.hIconPanel = 0
                        dim as COLORREF clr = ghStatusBar.BackColor
                        if gApp.hIconPanel = ghIconGood then clr = ghGeneral.iconsuccess
                        if gApp.hIconPanel = ghIconBad then clr = ghGeneral.iconfail
                        b.SetForeColors( clr, ghStatusBar.ForeColorHot )
                        b.SetBackColors( ghStatusBar.BackColor, ghStatusBar.BackColorHot )
                        dim as long txtWidth = getTextWidth( HWND_FRMMAIN_STATUSBAR, gSBPanels(i).wszText, GUIFONT_9 ) 
                        dim as RECT rcBitmap = gSBPanels(i).rc
                        dim as long halfWidth = (rcBitmap.right-rcBitmap.left) / 2
                        rcBitmap.right = rcBitmap.left + halfWidth - pWindow->ScaleX((txtWidth / 2))
                        rcBitmap.left = rcBitmap.right - pWindow->ScaleX(16)
                        dim as DWSTRING wszText = wszIconCompileResult
                        
                        ' If a Spinner is active then that will take precedence over the good/bad compile icon.
                        if gApp.bShowSpinner then
                            b.SetForeColors( ghStatusBar.ForeColor, ghStatusBar.ForeColorHot )
                            b.SetBackColors( ghStatusBar.BackColor, ghStatusBar.BackColorHot )
                            wszText = @spinner(gApp.SpinnerCurrentFrame)
                            rcBitmap.top = rcBitmap.top - pWindow->ScaleY(4)
                            rcBitmap.left = rcBitmap.left - pWindow->ScaleY(2)
                        end if
                    
                        b.SetFont( SYMBOLFONT_9 )
                        b.PaintText( wszText, @rcBitmap, DT_CENTER )
                    end if
                end if
            end if   
        next
                                
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 1, b.rcClient.left, b.rcClient.top, b.rcClient.right, b.rcClient.top )

        b.EndDoubleBuffer()

        return 0
        
    end Select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmStatusBar_Show
' ========================================================================================
function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = New CPanelWindow

    HWND_FRMMAIN_STATUSBAR = pWindow->Create( hwndParent, @frmStatusBar_WndProc, _
        0, 0, 0, STATUSBAR_HEIGHT, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS
    
    frmStatusBar_PositionWindows()
    function = 0
    
end function

