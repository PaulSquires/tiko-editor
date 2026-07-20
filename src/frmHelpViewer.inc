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

#include once "frmHelpViewer.bi"


' ========================================================================================
' Push the topics listbox scroll state into the CVScrollBar, in ITEM units (total
' items, items per page, top index). SetRange re-derives the thumb and repaints; the
' control shows an empty track when everything fits.
' ========================================================================================
function frmHelpViewer_UpdateScrollBar() as boolean
    if IsWindow( HWND_FRMHELPVIEWER_VSCROLLBAR ) = 0 then exit function
    dim as RECT rc
    GetClientRect( HWND_FRMHELPVIEWER_LEFTPANEL, @rc )
    ' LB_GETITEMHEIGHT works with zero items on a fixed-height owner-draw listbox
    ' (see C:\dev\Learnings.md); guard the divide anyway.
    dim as long itemHeight = SendMessage( HWND_FRMHELPVIEWER_LEFTPANEL, LB_GETITEMHEIGHT, 0, 0 )
    dim as long itemsPerPage = 0
    if itemHeight > 0 then itemsPerPage = (rc.bottom - rc.top) \ itemHeight
    CVScrollBar_SetRange( HWND_FRMHELPVIEWER_VSCROLLBAR, _
                          ListBox_GetCount( HWND_FRMHELPVIEWER_LEFTPANEL ), _
                          itemsPerPage, _
                          SendMessage( HWND_FRMHELPVIEWER_LEFTPANEL, LB_GETTOPINDEX, 0, 0 ) )
    function = 0
end function


' ========================================================================================
' The user scrolled the CVScrollBar (thumb drag or track paging): set the listbox top
' index. Owner-drawn rows need the explicit redraw (same as the old code).
' ========================================================================================
sub frmHelpViewer_VScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    SendMessage( HWND_FRMHELPVIEWER_LEFTPANEL, LB_SETTOPINDEX, newPos, 0 )
    AfxRedrawWindow( HWND_FRMHELPVIEWER_LEFTPANEL )
end sub


' ========================================================================================
' Push the help-text CTextBox scroll state into its CVScrollBar, in LINE units --
' the CVScrollBar-shaped scroll API the control was built with (GetVScrollInfo ->
' SetRange). Called from the textbox's ScrollChangedCallback (typing can't happen --
' read-only -- but wheel scrolling and programmatic SetText both fire it) and from
' PositionWindows (a resize changes the lines-per-page).
' ========================================================================================
function frmHelpViewer_UpdateTextScrollBar() as boolean
    if IsWindow( HWND_FRMHELPVIEWER_VSCROLLBAR2 ) = 0 then exit function
    dim as integer nTotalLines, nLinesPerPage, nFirstVisible
    CTextBox_GetVScrollInfo( HWND_FRMHELPVIEWER_RIGHTPANEL, nTotalLines, nLinesPerPage, nFirstVisible )
    CVScrollBar_SetRange( HWND_FRMHELPVIEWER_VSCROLLBAR2, nTotalLines, nLinesPerPage, nFirstVisible )
    function = 0
end function


' Vertical scroll state of the help text may have changed: push the fresh range.
sub frmHelpViewer_TextScrollChanged( byval hTextBox as HWND )
    frmHelpViewer_UpdateTextScrollBar()
end sub


' The user scrolled the text CVScrollBar (thumb drag or track paging): scroll the
' help text to that first line. The ScrollToLine -> EN_UPDATE round trip echoes the
' same position back through TextScrollChanged -- consistent by construction.
sub frmHelpViewer_TextVScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    CTextBox_ScrollToLine( HWND_FRMHELPVIEWER_RIGHTPANEL, newPos )
end sub


' ========================================================================================
' Custom paint keeping the old visuals: flat track + flat thumb in the ghPanel colors,
' no hover brightening. Colors are read live, so a theme switch only needs a repaint.
' Shared by both bars (topics list and help text).
' ========================================================================================
sub frmHelpViewer_VScrollPaint( byval p as CVSCROLLBAR_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b
    b->SetBackColor( ghPanel.BackColorScrollBar )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetBackColor( ghPanel.ForeColorScrollBar )
        b->PaintRect( @p->rcThumb )
    end if
end sub


' ========================================================================================
' Calculate RECT area of the splitter bar
' ========================================================================================
function frmHelpViewer_CalcSplitRect() as RECT
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit function

    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_PANEL)
    dim as long nSplitterWidth = pWindow->ScaleX(SPLITSIZE)
    dim as RECT rc
    GetClientRect( HWND_FRMHELPVIEWER, @rc )
    rc.left = AfxGetWindowWidth(HWND_FRMHELPVIEWER_LEFTPANEL) + nVScrollWidth
    rc.right = rc.left + nSplitterWidth

    function = rc
end function


' ========================================================================================
' Load a specific Help file richtext file into the CTextBox.
' The help files on disk are RTF; the CTextBox is plain text by design (one themed
' fore/back color for all text). A hidden RichEdit does the RTF -> plain text
' conversion: stream the file in, read the plain text back out, hand it to the
' textbox. SetText is programmatic (silent for ChangeCallback) but still fires the
' ScrollChangedCallback, so the scrollbar re-ranges automatically.
' ========================================================================================
function frmHelpViewer_LoadHelpFile( byval nIndex as long ) as long
    if (nIndex >= lbound(gHelpViewer.Filenames)) andalso _
        (nIndex <= ubound(gHelpViewer.Filenames)) then
        dim as DWSTRING wszPath = AfxStrPathname("PATH", gConfig.HelpTableOfContents)
        dim as HWND hConvert = GetDlgItem( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_RTFCONVERT )
        RichEdit_LoadRtfFromFile( hConvert, wszPath & gHelpViewer.Filenames(nIndex) )
        CTextBox_SetText( HWND_FRMHELPVIEWER_RIGHTPANEL, AfxGetWindowText(hConvert) )
        SetWindowText( hConvert, wstr("") )   ' don't hold the document twice
        ' Ensure the view starts at the top of the file
        CTextBox_SetSel( HWND_FRMHELPVIEWER_RIGHTPANEL, 0, 0 )
        CTextBox_ScrollToLine( HWND_FRMHELPVIEWER_RIGHTPANEL, 0 )
    end if
    function = 0
end function



' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmHelpViewer_PositionWindows() as LRESULT
    
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMHELPVIEWER)
    if pWindow = 0 then exit function

    dim as long nSplitterWidth = pWindow->ScaleX(SPLITSIZE)

    dim as RECT rcClient: GetClientRect(HWND_FRMHELPVIEWER, @rcClient)
    dim as long nClientHeight = rcClient.bottom - rcClient.top
    dim as long nClientWidth = rcClient.right - rcClient.left
    
    dim as long xDelta = 0
    if gApp.bDragActive = true then xDelta = gHelpViewer.xDeltaSplitter
        
    dim as long nLeftPanelWidth = AfxGetWindowWidth(HWND_FRMHELPVIEWER_LEFTPANEL) + xDelta
    if nLeftPanelWidth < nSplitterWidth then nLeftPanelWidth = nSplitterWidth
    if nLeftPanelWidth >= (nClientWidth - nSplitterWidth * 2) then _
        nLeftPanelWidth = (nClientWidth - nSplitterWidth * 2)

    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_PANEL)

    SetWindowPos( HWND_FRMHELPVIEWER_LEFTPANEL, 0, _
        0, 0, nLeftPanelWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    SetWindowPos( HWND_FRMHELPVIEWER_VSCROLLBAR, 0, _
        nLeftPanelWidth, 0, nVScrollWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Help text CTextBox, with its own CVScrollBar hugging the window's right edge
    SetWindowPos( HWND_FRMHELPVIEWER_RIGHTPANEL, 0, _
        nLeftPanelWidth + nVScrollWidth + nSplitterWidth, 0, _
        nClientWidth - nSplitterWidth - nVScrollWidth * 2 - nLeftPanelWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    SetWindowPos( HWND_FRMHELPVIEWER_VSCROLLBAR2, 0, _
        nClientWidth - nVScrollWidth, 0, nVScrollWidth, nClientHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Push the ranges AFTER the resize so items/lines-per-page reflect the new heights
    ' (the old code computed the listbox one before repositioning, going stale by one
    ' layout pass).
    frmHelpViewer_UpdateScrollBar()
    frmHelpViewer_UpdateTextScrollBar()
    AfxRedrawWindow( HWND_FRMHELPVIEWER_LEFTPANEL )
    
    function = 0
end function


' ========================================================================================
' Process WM_MOUSEMOVE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnMouseMove( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    ' HITTEST (SPLITTER)
    dim as RECT rcSplitter = frmHelpViewer_CalcSplitRect()
    if (isMouseOverRECT(hwnd, rcSplitter) = true) orelse (gApp.bDragActive = true) then
        SetCursor( ghCursorSizeWE )
    else    
        SetCursor( LoadCursor( null, IDC_ARROW ))
    end if

    if (gApp.bDragActive = true) andalso (x <> gHelpViewer.ptSplitPrev.x) then
        gHelpViewer.xDeltaSplitter = (x - gHelpViewer.ptSplitPrev.x)
        frmHelpViewer_PositionWindows
    end if

    gHelpViewer.ptSplitPrev = type(x, y)
            
    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONDOWN message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnLButtonDown( _
            byval hwnd as HWND, _
            byval fDoubleClick as boolean, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long
                                        
    dim as RECT rcSplitter = frmHelpViewer_CalcSplitRect()
    if isMouseOverRECT( hwnd, rcSplitter ) then
        gApp.bDragActive = true
        gHelpViewer.ptSplitPrev = type(x, y)
        SetCursor(ghCursorSizeWE)
        SetCapture( HWND_FRMHELPVIEWER )
    end if

    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONUP message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnLButtonUp( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    if gApp.bDragActive then
       gApp.bDragActive = false
       frmHelpViewer_PositionWindows
       ReleaseCapture
    end if   
    SetCursor( LoadCursor( null, IDC_ARROW ))

    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmHelpViewer_PositionWindows
    end if
    function = 0
end Function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    '  Message cracker macro expects a True to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case codeNotify
        case LBN_SELCHANGE
            ' update the highlighting of the current line
            AfxRedrawWindow(hwndCtl)
            ' Load the help document into the richedit control
            dim as long nCurSel = Listbox_GetCurSel(hwndCtl)
            frmHelpViewer_LoadHelpFile( nCurSel )
            ' update the scrollbar position if necessary
            frmHelpViewer_PositionWindows()
    end select      
        
    select case id
        case IDCANCEL
            if codeNotify = BN_CLICKED then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
                exit function
            end if
    
    end select

    function = 0
end function


' ========================================================================================
' Process WM_NOTIFY message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnNotify( _
            byval hwnd as HWND, _
            byval id as long, _
            byval pNMHDR as NMHDR ptr _
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    
    function = 0
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( ghMain.BackColor )
    b.PaintClientRect()

    ' Draw any splitter between the two panels
    dim as RECT rcSplitter = frmHelpViewer_CalcSplitRect()
    b.SetBackColor( ghPanel.BackColor )
    b.PaintRect( @rcSplitter )
    
    b.EndDoubleBuffer()
    
    function = 0
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnDestroy( byval hwnd as HWND ) as LRESULT
    ' Save the current screen coordinates of the Help Viewer
    gConfig.SaveConfigFile    

    function = 0
end function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long
    ' Set the height of the list box items.
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    lpmis->itemHeight = pWindow->ScaleY(EXPLORERITEM_HEIGHT)

    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmHelpViewer
' ========================================================================================
function frmHelpViewer_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function

    if lpdis = 0 then exit function

    if ( lpdis->itemAction = ODA_DRAWENTIRE ) orelse _
        ( lpdis->itemAction = ODA_SELECT ) orelse _
        ( lpdis->itemAction = ODA_FOCUS ) then

        dim as clsDoubleBuffer b

        b.BeginDoubleBuffer(lpdis->hwndItem, lpdis->hDC, lpdis->rcItem)

        dim as boolean IsHot = false
        dim as boolean isNodeHeader = false
        dim as boolean isIconDown = false

        ' if mouse is over then reset hot
        if isMouseOverRECT(lpdis->hwndItem, b.rcClient) then IsHot = true   
        if ListBox_GetCurSel(lpdis->hwndItem) = lpdis->itemID then IsHot = true

        dim as COLORREF backclr = iif( IsHot, ghPanel.BackColorHot,  ghPanel.BackColor)
        dim as COLORREF foreclr = iif( IsHot, ghPanel.ForeColorHot,  ghPanel.ForeColor)
        b.SetForeColor( foreclr )
        b.SetBackColor( backclr )

        dim as DWSTRING wszCaption
        if (lpdis->ItemID >= lbound(gHelpViewer.Filenames)) andalso _
            (lpdis->ItemID <= ubound(gHelpViewer.Filenames)) then
            wszCaption = gHelpViewer.Topics(lpdis->ItemID)
        end if

        ' Create our rect that works with the entire line
        dim as RECT rc
        SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
        
        ' Paint the entire background
        b.PaintRect( @rc )

        dim as long nLeftMargin = pWindow->ScaleX(20)
        rc.left = nLeftMargin
        dim as RECT rcText = rc 
        dim as RECT rcBitmap = rc 

        if left(ltrim(wszCaption), 1) = "-" then
            rcBitmap.left = rcText.left + pWindow->ScaleX(10)
            rcBitmap.right = rcBitmap.left + pWindow->ScaleX(10) 
            b.SetFont( ghFont(GUIFONT_10) )  ' use GUI rather than SYMBOL b/c of dot icon
            b.PaintText( wszIconDocument, @rcBitmap, DT_CENTER )
            rcText.left = rcBitmap.right
        end if    

        wszCaption = ltrim(wszCaption, any " -")
        
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( wszCaption, @rcText, DT_LEFT )

        b.EndDoubleBuffer()
            
    end if

    function = true

end function


' ========================================================================================
' frmOptions Window procedure
' ========================================================================================
function frmHelpViewer_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,           frmHelpViewer_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,          frmHelpViewer_OnPaint)
        HANDLE_MSG (hwnd, WM_CREATE,         frmHelpViewer_OnCreate)
        HANDLE_MSG (hwnd, WM_CLOSE,          frmHelpViewer_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,        frmHelpViewer_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,        frmHelpViewer_OnCommand)
        HANDLE_MSG (hwnd, WM_NOTIFY,         frmHelpViewer_OnNotify)
        HANDLE_MSG (hwnd, WM_LBUTTONDOWN,    frmHelpViewer_OnLButtonDown)
        HANDLE_MSG (hwnd, WM_LBUTTONUP,      frmHelpViewer_OnLButtonUp)
        HANDLE_MSG (hwnd, WM_MOUSEMOVE,      frmHelpViewer_OnMouseMove)
        HANDLE_MSG (hwnd, WM_MEASUREITEM,    frmHelpViewer_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,       frmHelpViewer_OnDrawItem)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMHELPVIEWER = 0
        ' The CVScrollBar/CTextBox children destroyed themselves in their own
        ' WM_NCDESTROY; reset the handles here (the Help window can be closed and
        ' reopened).
        HWND_FRMHELPVIEWER_VSCROLLBAR = 0
        HWND_FRMHELPVIEWER_VSCROLLBAR2 = 0
        HWND_FRMHELPVIEWER_RIGHTPANEL = 0
        return 0

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmHelpViewerListBox_SubclassProc 
' ========================================================================================
function frmHelpViewerListBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    static as long accumDelta
    
    ' keep track of last index we were over so that we only issue a 
    ' repaint if the cursor has moved off of the line
    static as long nLastIdx = -1
        
    select case uMsg
        case WM_MOUSEWHEEL
            ' accumulate delta until scroll one line (up +120, down -120). 
            ' 120 is the Microsoft default delta
            dim as long zDelta = GET_WHEEL_DELTA_WPARAM( _wParam )
            dim as long nTopIndex = SendMessage( hWin, LB_GETTOPINDEX, 0, 0 ) 
            accumDelta = accumDelta + zDelta
            if accumDelta >= 120 then       ' scroll up 3 lines
                nTopIndex = nTopIndex - 3
                nTopIndex = max( 0, nTopIndex )
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
                frmHelpViewer_PositionWindows
            elseif accumDelta <= -120 then  ' scroll down 3 lines
                nTopIndex = nTopIndex + 3
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
                frmHelpViewer_PositionWindows
            end if

        case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' eventual WM_MOUSEHOVER and WM_MOUSELEAVE events
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 1
            TrackMouseEvent(@tme) 

            ' get the item rect that the mouse is over and only invalidate
            ' that instead of the entire listbox
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                if idx <> nLastIdx then
                    nLastIdx = idx
                    AfxRedrawWindow(hWin)   
                end if
            end if
                            
        case WM_MOUSELEAVE
            nLastIdx = -1
            AfxRedrawWindow(hWin)   
            
        case WM_LBUTTONDOWN
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                AfxRedrawWindow(hWin)   
            end if

                                    
        case WM_ERASEBKGND
            ' if the number of lines in the listbox maybe less than the number per page then 
            ' calculate from last item to bottom of listbox, otherwise calculate based on
            ' the mod of the lineheight to listbox height so we can color the partial line
            ' that won't be displayed at the bottom of the list.
            dim as RECT rc: GetClientRect( hWin, @rc )

            dim as RECT rcItem  
            SendMessage( hWin, LB_GETITEMRECT, 0, cast(LPARAM, @rcItem) )
            dim as long itemHeight = rcItem.bottom - rcItem.top
            dim as long NumItems = ListBox_GetCount(hWin)
            dim as long nTopIndex = SendMessage( hWin, LB_GETTOPINDEX, 0, 0 ) 
            dim as long visible_rows = 0
            dim as long ItemsPerPage = 0
            dim as long bottom_index = 0
                        
            if NumItems > 0 then
                ItemsPerPage = (rc.bottom - rc.top) / itemHeight
                bottom_index = (nTopIndex + ItemsPerPage) 
                if bottom_index >= NumItems then bottom_index = NumItems - 1
                visible_rows = (bottom_index - nTopIndex) + 1
                rc.top = visible_rows * itemHeight 
            end if

            if rc.top < rc.bottom then
                dim as HDC _hDC = cast(HDC, _wParam)
                PaintRect( _hDC, @rc, ghPanel.BackColor )
            end if

            ValidateRect( hWin, @rc )
            return true
        
        Case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmHelpViewerListBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' frmHelpViewer_Show
' ========================================================================================
function frmHelpViewer_Show( byval hWndParent as HWND ) as LRESULT

    ' Display the HelpViewer if it already exists
    if IsWindow( HWND_FRMHELPVIEWER ) then
        if IsIconic(HWND_FRMHELPVIEWER) then 
            ShowWindow( HWND_FRMHELPVIEWER, SW_NORMAL )
        end if
        AllowSetForegroundWindow(ASFW_ANY)
        SetForegroundWindow( HWND_FRMHELPVIEWER )
        exit function
    end if
    
    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    dim as DWSTRING wszText = APPNAME & " " & L(73,"Help")
    HWND_FRMHELPVIEWER = pWindow->Create( _
        hWndParent, wszText, @frmHelpViewer_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_OVERLAPPEDWINDOW or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->ClassStyle = CS_DBLCLKS

    ' Set the small and large icon for the main window (must be set after main window is created)
    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    dim as long nvalue = false
    if instr(gConfig.ThemeShortFilename, "_dark") then nvalue = true
    ' Attempt to apply the standard Windows dark theme to the non-client areas of the main form.
    DwmSetWindowAttribute(HWND_FRMHELPVIEWER, DWMWA_USE_IMMERSIVE_DARK_MODE, @nvalue, sizeof(nvalue))

    if (gConfig.HelpStartupRight = 0) orelse (gConfig.HelpStartupBottom = 0) then
        gConfig.HelpStartupRight  = pWindow->ScaleX(800)
        gConfig.HelpStartupBottom = pWindow->ScaleX(600)
        gConfig.HelpLeftPanelWidth = pWindow->ScaleX(250)
    end if
    
    dim WinPla as WINDOWPLACEMENT
    with WinPla
        .Length = sizeof(WinPla)
        .rcNormalPosition.Left   = gConfig.HelpStartupLeft
        .rcNormalPosition.Top    = gConfig.HelpStartupTop
        .rcNormalPosition.Right  = gConfig.HelpStartupRight
        .rcNormalPosition.Bottom = gConfig.HelpStartupBottom
        .showCmd = iif( gConfig.HelpStartupMaximized, SW_MAXIMIZE, SW_SHOWNORMAL )
    end with
    SetWindowPlacement( pWindow->hWindow, @WinPla )

    dim as long nWidth = pWindow->UnScaleX(gConfig.HelpLeftPanelWidth)

    HWND_FRMHELPVIEWER_LEFTPANEL = _ 
        pWindow->AddControl("LISTBOX", , IDC_FRMHELPVIEWER_LEFTPANEL, "", 0, 0, nWidth, 0, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or WS_TABSTOP or _
        LBS_NOINTEGRALHEIGHT or LBS_OWNERDRAWFIXED or LBS_NOTIFY, _
        WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(HWND_FRMHELPVIEWER_LEFTPANEL, @frmHelpViewerListBox_SubclassProc, IDC_FRMHELPVIEWER_LEFTPANEL, @pWindow)

    ' Topics-list scrollbar: the reusable CVScrollBar control, driven in ITEM units
    ' (frmHelpViewer_UpdateScrollBar pushes the range; the ScrollCallback sets the
    ' listbox top index). PositionWindows sizes it, so no initial size is needed here.
    HWND_FRMHELPVIEWER_VSCROLLBAR = CVScrollBar_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_VSCROLLBAR )
    CVScrollBar_SetScrollCallback( HWND_FRMHELPVIEWER_VSCROLLBAR, @frmHelpViewer_VScrollCallback )
    CVScrollBar_SetPaintCallback( HWND_FRMHELPVIEWER_VSCROLLBAR, @frmHelpViewer_VScrollPaint )

    ' Help text panel: a multiline CTextBox (word-wrapped, control-implemented wheel
    ' scrolling, no native scrollbar -- an external CVScrollBar is wired through the
    ' control's scroll API below). Themed with the editor text colors -- the thing the
    ' old RichEdit never managed (its dead SetBkgndColor/EM_SETCHARFORMAT attempts sat
    ' commented out here). The fixed-pitch font keeps the help files' column-aligned
    ' shortcut tables lined up.
    HWND_FRMHELPVIEWER_RIGHTPANEL = _
        CTextBox_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_RIGHTPANEL, true )
    CTextBox_SetFont( HWND_FRMHELPVIEWER_RIGHTPANEL, ghFont(INFOFONT_11) )
    CTextBox_SetBorderWidth( HWND_FRMHELPVIEWER_RIGHTPANEL, 0 )
    CTextBox_SetMargins( HWND_FRMHELPVIEWER_RIGHTPANEL, pWindow->ScaleX(40), pWindow->ScaleX(40) )
    CTextBox_SetReadOnly( HWND_FRMHELPVIEWER_RIGHTPANEL, true )
    CTextBox_SetForeColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.ForeColorText )
    CTextBox_SetBackColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.BackColorText )
    CTextBox_SetOuterBackColor( HWND_FRMHELPVIEWER_RIGHTPANEL, ghEditor.BackColorText )
    CTextBox_SetScrollChangedCallback( HWND_FRMHELPVIEWER_RIGHTPANEL, @frmHelpViewer_TextScrollChanged )
    CTextBox_SetMenuText( HWND_FRMHELPVIEWER_RIGHTPANEL, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )

    ' Help-text scrollbar, driven in LINE units through the CTextBox scroll API
    ' (ScrollChangedCallback -> GetVScrollInfo -> SetRange; ScrollCallback -> ScrollToLine).
    HWND_FRMHELPVIEWER_VSCROLLBAR2 = CVScrollBar_Create( HWND_FRMHELPVIEWER, IDC_FRMHELPVIEWER_VSCROLLBAR2 )
    CVScrollBar_SetScrollCallback( HWND_FRMHELPVIEWER_VSCROLLBAR2, @frmHelpViewer_TextVScrollCallback )
    CVScrollBar_SetPaintCallback( HWND_FRMHELPVIEWER_VSCROLLBAR2, @frmHelpViewer_VScrollPaint )

    ' Hidden RichEdit used only to convert the RTF help files to plain text for the
    ' CTextBox (see frmHelpViewer_LoadHelpFile). Never shown, never positioned.
    pWindow->AddControl("RICHEDIT", , IDC_FRMHELPVIEWER_RTFCONVERT, "", 0, 0, 0, 0, _
        WS_CHILD or ES_LEFT or ES_MULTILINE, _
        WS_EX_LEFT)

    ' Load the Help table of contents
    dim pStream as CTextStream
    if pStream.Open(gConfig.HelpTableOfContents) = S_OK then
        dim as DWSTRING wst
        dim as long idx
        do until pStream.EOS
            wst = pStream.ReadLine
            wst = trim(wst)
            if len(wst) = 0 then continue do
            
            if left(wst, 7) = "TOPIC: " then
                idx = Listbox_AddString( HWND_FRMHELPVIEWER_LEFTPANEL, @wstr("") )
                redim preserve gHelpViewer.Topics(idx)
                redim preserve gHelpViewer.Filenames(idx)
                gHelpViewer.Topics(idx) = mid(wst, 8)
                continue do
            end if

            if left(wst, 10) = "FILENAME: " then
                if (idx >= lbound(gHelpViewer.Filenames)) andalso _
                    (idx <= ubound(gHelpViewer.Filenames)) then
                    gHelpViewer.Filenames(idx) = mid(wst, 11)
                end if
                continue do
            end if
        loop
        pStream.Close
    end if
    
    Listbox_SetCurSel( HWND_FRMHELPVIEWER_LEFTPANEL, 0 )
    frmHelpViewer_LoadHelpFile(0)

    frmHelpViewer_PositionWindows()
    
    ShowWindow( HWND_FRMHELPVIEWER, SW_SHOW )
    SetFocus(HWND_FRMHELPVIEWER_LEFTPANEL)
        
    return 0
end function

