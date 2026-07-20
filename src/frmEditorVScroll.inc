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
''  frmEditorVScroll.inc
''   

#include once "frmEditorVScroll.bi"

' ========================================================================================
' Calculate the RECT that holds the client coordinates of the scrollbar's vertical thumb
' Returns True if RECT is not empty
' ========================================================================================
function frmEditorVScroll_calcVThumbRect( byval pDoc as clsDocument ptr ) as boolean
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 Then exit function
    if pDoc = 0 then exit function
    dim as RECT rc

    dim as integer minThumb = pWindow->ScaleY(SCROLLBAR_MINTHUMBSIZE)
    
    ' calculate the vertical scrollbars in client coordinates
    for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
        rc = AfxGetWindowRect(pDoc->hWindow(i))
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 ) 
        dim as long clientHeight = rc.bottom - rc.top
        if clientHeight = 0 then continue for
        dim as long firstVisibleLine = SciExec( pDoc->hWindow(i), SCI_GETFIRSTVISIBLELINE, 0, 0 )
        dim as long iScrollbarWidth = AfxGetWindowWidth(HWND_FRMEDITOR_VSCROLLBAR(i)) 
        dim as long minThumbHeight = pWindow->ScaleY(SCROLLBAR_MINTHUMBSIZE)
        SetRectEmpty( @gEditorVScroll(i).rc )

        with gEditorVScroll(i)
            .linesPerPage = pDoc->linesPerPage(i)
            .numLines = pDoc->GetLineCount + 1

            dim as double ratio = .linesPerPage / .numLines
            .thumbHeight = int(ratio * clientHeight)
            if .thumbHeight < minThumb then .thumbHeight = minThumb

            .rc.left = 0
            .rc.right = .rc.left + iScrollbarWidth
            .rc.top = ((firstVisibleLine / .numLines) * clientHeight)
            .rc.bottom = .rc.top + .thumbHeight
        end with
    next

    function = 0
end function

    
' ========================================================================================
' frmEditorVScroll Window procedure
' ========================================================================================
function frmEditorVScroll_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as point prev_pt   ' screen pt.y cursor position

    dim as long idxWindow = iif( hwnd = HWND_FRMEDITOR_VSCROLLBAR(0), 0, 1 )

    select case uMsg
        case WM_NCDESTROY
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            if hwnd = HWND_FRMEDITOR_VSCROLLBAR(0) then HWND_FRMEDITOR_VSCROLLBAR(0) = 0
            if hwnd = HWND_FRMEDITOR_VSCROLLBAR(1) then HWND_FRMEDITOR_VSCROLLBAR(1) = 0
            return 0
                
        case WM_LBUTTONDOWN
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                dim as point pt: GetCursorPos( @pt )
                frmEditorVScroll_calcVThumbRect(pDoc)   ' in client coordinates
                dim as RECT rc = gEditorVScroll(idxWindow).rc ' covert copy to screen coordinates
                MapWindowPoints( HWND_FRMEDITOR_VSCROLLBAR(idxWindow), HWND_DESKTOP, cast(point ptr, @rc), 2)
                if PtInRect( @rc, pt ) then
                    ' Suppress only while the drag is active; cleared in WM_LBUTTONUP
                    ' and WM_CAPTURECHANGED (capture can be stolen mid-drag).
                    gApp.SuppressNotify = true   ' prevent SCN_UPDATEUI
                    prev_pt = pt
                    gApp.bDragActive = true
                    SetCapture( hwnd )
                else
                    ' we have clicked on a PageUp or PageDn. No capture is taken here so
                    ' WM_LBUTTONUP is not guaranteed to arrive - suppress only around the
                    ' reposition itself, never across the click.
                    gApp.SuppressNotify = true   ' prevent SCN_UPDATEUI
                    dim as long nTopIndex = SendMessage( pDoc->hWindow(idxWindow), SCI_GETFIRSTVISIBLELINE, 0, 0 )
                    if pt.y < rc.top then
                        nTopIndex = max( nTopIndex - gEditorVScroll(idxWindow).linesPerPage, 0 )
                        SendMessage( pDoc->hWindow(idxWindow), SCI_SETFIRSTVISIBLELINE, nTopIndex, 0 )
                        frmEditorVScroll_calcVThumbRect(pDoc)   ' in client coordinates
                        AfxRedrawWindow( HWND_FRMEDITOR_VSCROLLBAR(idxWindow) )
                    elseif pt.y > rc.bottom then
                        dim as long nMaxTopIndex = gEditorVScroll(idxWindow).numLines - gEditorVScroll(idxWindow).linesPerPage
                        nTopIndex = min( nTopIndex + gEditorVScroll(idxWindow).linesPerPage, nMaxTopIndex )
                        SendMessage( pDoc->hWindow(idxWindow), SCI_SETFIRSTVISIBLELINE, nTopIndex, 0 )
                        frmEditorVScroll_calcVThumbRect(pDoc)   ' in client coordinates
                        AfxRedrawWindow( HWND_FRMEDITOR_VSCROLLBAR(idxWindow) )
                    end if
                    gApp.SuppressNotify = false   ' allow SCN_UPDATEUI
                end if
            end if
            
        case WM_MOUSEMOVE
            if gApp.bDragActive then
                dim as point pt: GetCursorPos( @pt )
                if pt.y <> prev_pt.y then 
                    dim as long delta = pt.y - prev_pt.y 

                    ' convert to client coordinates for ease of use
                    dim as RECT rc: GetClientRect( HWND_FRMEDITOR_VSCROLLBAR(idxWindow), @rc )
                    gEditorVScroll(idxWindow).rc.top = max(0, gEditorVScroll(idxWindow).rc.top + delta)
                    gEditorVScroll(idxWindow).rc.top = min(gEditorVScroll(idxWindow).rc.top, rc.bottom - gEditorVScroll(idxWindow).thumbHeight)
                    gEditorVScroll(idxWindow).rc.bottom = gEditorVScroll(idxWindow).rc.top + gEditorVScroll(idxWindow).thumbHeight

                    prev_pt = pt
                    
                    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
                    if pDoc then
                        dim as long nPrevTopLine = SendMessage( pDoc->hWindow(idxWindow), SCI_GETFIRSTVISIBLELINE, 0, 0 ) 
                        dim as rect rc = AfxGetWindowRect(pDoc->hWindow(idxWindow))
                        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 ) 
                        dim as long clientHeight = rc.bottom - rc.top
                        if clientHeight = 0 then exit function
                        dim as double scrollRatio = gEditorVScroll(idxWindow).rc.top / (clientHeight - gEditorVScroll(idxWindow).thumbHeight)
                        dim as long nTopLine = int(scrollRatio * (gEditorVScroll(idxWindow).numLines - gEditorVScroll(idxWindow).linesPerPage))
                        if nTopLine <> nPrevTopLine then
                            SendMessage( pDoc->hWindow(idxWindow), SCI_SETFIRSTVISIBLELINE, nTopLine, 0 ) 
                        end if   
                    end if
                    AfxRedrawWindow( HWND_FRMEDITOR_VSCROLLBAR(idxWindow) )

                end if
            end if
            
        case WM_LBUTTONUP
            gApp.SuppressNotify = false   ' allow SCN_UPDATEUI
            gApp.bDragActive = false
            prev_pt.x = 0
            prev_pt.y = 0
            ReleaseCapture

        case WM_CAPTURECHANGED
            ' Capture was lost or stolen (Alt+Tab, another SetCapture) so WM_LBUTTONUP
            ' may never arrive: cancel the drag here or SuppressNotify stays stuck true
            ' and every SCN_ notification (autoinsert, autoindent, UI updates) goes dead.
            ' Also fires from our own ReleaseCapture above; the reset is idempotent.
            gApp.SuppressNotify = false   ' allow SCN_UPDATEUI
            gApp.bDragActive = false

        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim as clsDoubleBuffer b

            b.BeginDoubleBuffer(hwnd)
            b.SetBackColor( ghEditor.BackColorScrollBar )
            b.PaintClientRect()
            b.SetPenColor( ghEditor.Divider )
            b.PaintLine( 1, b.rcClient.left, b.rcClient.top, b.rcClient.left, b.rcClient.bottom )
            b.SetBackColor( ghEditor.ForeColorScrollBar )
            b.PaintRect( @gEditorVScroll(idxWindow).rc )

            b.EndDoubleBuffer()
            
            return 0
    end Select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmEditorVScroll_Show
' ========================================================================================
function frmEditorVScroll_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr 

    for i as long = 0 to 1
        pWindow = new CPanelWindow
        HWND_FRMEDITOR_VSCROLLBAR(i) = pWindow->Create( hWndParent, _
            @frmEditorVScroll_WndProc, 0, 0, SCROLLBAR_WIDTH_EDITOR + 1, 0, _
            WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, WS_EX_NOACTIVATE )
    next

    function = 0
    
end function

