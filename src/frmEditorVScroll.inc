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
''  The editor's vertical scrollbars as CVScrollBar instances. Replaces the old
''  hand-rolled CPanelWindow scrollbar (WndProc + calcVThumbRect + gEditorVScroll
''  state): the control owns the thumb geometry, dragging, track paging with
''  auto-repeat, hover tracking and capture-loss cancellation; this file only
''  translates between Scintilla's line numbers and the control's range model.
''

#include once "frmEditorVScroll.bi"


' ========================================================================================
' Push the current Scintilla scroll state of BOTH views into the scrollbars, in LINE
' units (total, page, first) -- called from the Scintilla notification paths
' (SCN_UPDATEUI via frmMain_SetStatusbar, the split branch of Scintilla_OnNotify) and
' from frmMain_PositionWindows. SetRange re-derives the thumb and repaints; visibility
' stays host-owned (frmMain's positioning and SCN_MODIFIED auto-show logic).
' numLines keeps the old +1 headroom so the thumb proportions match the previous
' implementation.
' ========================================================================================
function frmEditorVScroll_UpdateScrollBars( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then exit function
    dim as long numLines = pDoc->GetLineCount + 1
    for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
        dim as long firstVisibleLine = SciExec( pDoc->hWindow(i), SCI_GETFIRSTVISIBLELINE, 0, 0 )
        CVScrollBar_SetRange( HWND_FRMEDITOR_VSCROLLBAR(i), _
                              numLines, pDoc->LinesPerPage(i), firstVisibleLine )
    next
    function = 0
end function


' ========================================================================================
' The user scrolled a scrollbar (thumb drag or track paging): scroll the matching
' Scintilla view. SuppressNotify brackets the reposition so the synchronous
' SCN_UPDATEUI it raises is dropped (the old drag code suppressed for the same
' reason); the scrollbar itself already sits at newPos, so no echo is needed.
' ========================================================================================
sub frmEditorVScroll_OnScroll( byval hScrollBar as HWND, byval newPos as integer )
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub
    dim as long idxWindow = iif( hScrollBar = HWND_FRMEDITOR_VSCROLLBAR(0), 0, 1 )
    gApp.SuppressNotify = true
    SendMessage( pDoc->hWindow(idxWindow), SCI_SETFIRSTVISIBLELINE, newPos, 0 )
    gApp.SuppressNotify = false
end sub


' ========================================================================================
' Custom paint keeping the old editor scrollbar visuals: flat track, an always-drawn
' divider line at the left edge, flat thumb with no hover brightening. Reads the
' ghEditor theme colors live, so a theme switch only needs a repaint.
' ========================================================================================
sub frmEditorVScroll_PaintCallback( byval p as CVSCROLLBAR_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b
    b->SetBackColor( ghEditor.BackColorScrollBar )
    b->PaintClientRect()
    b->SetPenColor( ghEditor.Divider )
    b->PaintLine( 1, p->rcClient.left, p->rcClient.top, p->rcClient.left, p->rcClient.bottom )
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetBackColor( ghEditor.ForeColorScrollBar )
        b->PaintRect( @p->rcThumb )
    end if
end sub


' ========================================================================================
' frmEditorVScroll_Show
' ========================================================================================
function frmEditorVScroll_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr( hWndParent )

    for i as long = 0 to 1
        HWND_FRMEDITOR_VSCROLLBAR(i) = CVScrollBar_Create( hWndParent, _
                iif( i = 0, IDC_FRMEDITOR_VSCROLLBAR0, IDC_FRMEDITOR_VSCROLLBAR1 ) )
        CVScrollBar_SetScrollCallback( HWND_FRMEDITOR_VSCROLLBAR(i), @frmEditorVScroll_OnScroll )
        CVScrollBar_SetPaintCallback( HWND_FRMEDITOR_VSCROLLBAR(i), @frmEditorVScroll_PaintCallback )
        ' CVScrollBar is created 0x0, but frmMain's layout code reads the bar width
        ' back with AfxGetWindowWidth before the first positioning -- give each bar
        ' its real width immediately (same 13px design width as the old CPanelWindow
        ' scrollbar, which CPanelWindow.Create DPI-scaled the same way).
        if pWindow then
            SetWindowPos( HWND_FRMEDITOR_VSCROLLBAR(i), 0, 0, 0, _
                          pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR + 1), 0, _
                          SWP_NOMOVE or SWP_NOZORDER or SWP_NOACTIVATE )
        end if
    next

    function = 0

end function
