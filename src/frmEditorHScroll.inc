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
''  frmEditorHScroll.inc
''
''  The editor's horizontal scrollbars as CHScrollBar instances. Replaces the old
''  hand-rolled CPanelWindow scrollbar (WndProc + gEditorHScroll ratio state +
''  gApp.bHorizDragActive): the control owns the thumb geometry, dragging, track
''  paging with auto-repeat, hover tracking and capture-loss cancellation; this file
''  only translates between Scintilla's xOffset pixels and the control's range model.
''

#include once "frmEditorHScroll.bi"


' ========================================================================================
' Measure one view's horizontal extent and push it into its scrollbar, in PIXEL units.
' total = widest visible line in document space (POINTXFROMPOSITION is view-relative,
' so xOffset is added back), page = visible text width, pos = xOffset. The total is
' floored at page+xOffset so a scrolled-right view can always be dragged back to 0
' even when its visible lines happen to fit (the old code's "xOffset <> 0" show test).
' Returns the SW_ visibility the bar deserves; showing/hiding stays the CALLER's call.
' ========================================================================================
function frmEditorHScroll_NeedScrollBar( _
            byval pDoc as clsDocument ptr, _
            byval idxWindow as long _
            ) as long

    dim as RECT rc = AfxGetWindowRect( pDoc->hWindow(idxWindow) )
    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 )
    dim as long ClientWidth = rc.right - rc.left

    ' Visible TEXT area width (client minus the margin columns)
    dim as long totalMarginWidth = 0
    for i as long = 0 to SC_MAX_MARGIN - 1
        totalMarginWidth += SciExec( pDoc->hWindow(idxWindow), SCI_GETMARGINWIDTHN, i, 0 )
    next
    dim as long visibleTextWidth = ClientWidth - totalMarginWidth
    if visibleTextWidth < 0 then visibleTextWidth = 0

    dim as long firstVisibleLine = SciExec( pDoc->hWindow(idxWindow), SCI_GETFIRSTVISIBLELINE, 0, 0 )
    dim as long linesOnScreen    = SciExec( pDoc->hWindow(idxWindow), SCI_LINESONSCREEN, 0, 0 )
    dim as long lineCount        = SciExec( pDoc->hWindow(idxWindow), SCI_GETLINECOUNT, 0, 0 )
    dim as long xOffset          = SciExec( pDoc->hWindow(idxWindow), SCI_GETXOFFSET, 0, 0 )

    dim as long maxWidth = 0
    for i as long = 0 to linesOnScreen - 1
        dim as long docLine = firstVisibleLine + i
        if docLine >= lineCount then exit for
        dim as long endPos = SciExec( pDoc->hWindow(idxWindow), SCI_GETLINEENDPOSITION, docLine, 0 )
        dim as long x = xOffset + SciExec( pDoc->hWindow(idxWindow), SCI_POINTXFROMPOSITION, 0, endPos )
        if x > maxWidth then maxWidth = x
    next

    dim as long nTotal = maxWidth
    if visibleTextWidth + xOffset > nTotal then nTotal = visibleTextWidth + xOffset

    CHScrollBar_SetRange( HWND_FRMEDITOR_HSCROLLBAR(idxWindow), _
                          nTotal, visibleTextWidth, xOffset )

    ' --- Wheel step: convert the system's CHARACTERS into our PIXELS ------------------
    ' CHScrollBar is unit-agnostic, so its default wheel step follows
    ' SPI_GETWHEELSCROLLCHARS -- which is a count of CHARACTERS (typically 3). Our range
    ' is in pixels, so left at the default a trackpad swipe moves the view three PIXELS
    ' per notch: roughly an eighth of a character, which reads as the gesture barely
    ' working. (The vertical bars need no such conversion: they are already in LINE units,
    ' which is exactly what SPI_GETWHEELSCROLLLINES counts.)
    '
    ' Set here rather than once at creation deliberately: Ctrl+wheel zoom changes the
    ' character width, and this function already re-runs on every layout and content
    ' change, so the step tracks the zoom for free.
    dim as long charWidth = SciExec( pDoc->hWindow(idxWindow), SCI_TEXTWIDTH, 0, cast(LPARAM, @"n") )
    if charWidth < 1 then charWidth = 1        ' Scintilla not ready / degenerate font
    dim as ulong wheelChars = 3
    SystemParametersInfo( SPI_GETWHEELSCROLLCHARS, 0, @wheelChars, 0 )
    ' WHEEL_PAGESCROLL (= UINT_MAX) means "a page per notch": hand the control 0 so it
    ' applies its own page-per-notch handling rather than a nonsense pixel count.
    dim as long wheelStep = 0
    if wheelChars <> cast(ulong, &hFFFFFFFF) then
        wheelStep = cast(long, wheelChars) * charWidth
        if wheelStep < 1 then wheelStep = 1
    end if
    CHScrollBar_SetWheelStep( HWND_FRMEDITOR_HSCROLLBAR(idxWindow), wheelStep )

    dim as boolean needHSB = CHScrollBar_NeedsThumb( HWND_FRMEDITOR_HSCROLLBAR(idxWindow) )
    function = iif( needHSB, SW_SHOWNOACTIVATE, SW_HIDE )
end function


' ========================================================================================
' Push the current Scintilla horizontal state of BOTH views into the scrollbars --
' called from the message pump when SCI_GETXOFFSET changes (typing/caret movement can
' scroll the view sideways without any mouse interaction). SetRange re-derives the
' thumb and repaints; visibility stays with frmEditorHScroll_DoNeedScrollBar.
' ========================================================================================
function frmEditorHScroll_UpdateScrollBars( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then exit function
    for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
        frmEditorHScroll_NeedScrollBar( pDoc, i )
    next
    function = 0
end function


' ========================================================================================
' The user scrolled a scrollbar (thumb drag or track paging): scroll the matching
' Scintilla view. SuppressNotify brackets the reposition so the synchronous
' SCN_UPDATEUI it raises is dropped; the scrollbar itself already sits at newPos, so
' no echo is needed. lastXOffsetPos is synced so the pump's change detector does not
' immediately re-push the range the control just reported.
' ========================================================================================
sub frmEditorHScroll_OnScroll( byval hScrollBar as HWND, byval newPos as integer )
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit sub
    dim as long idxWindow = iif( hScrollBar = HWND_FRMEDITOR_HSCROLLBAR(0), 0, 1 )
    gApp.SuppressNotify = true
    SendMessage( pDoc->hWindow(idxWindow), SCI_SETXOFFSET, newPos, 0 )
    gApp.SuppressNotify = false
    pDoc->lastXOffsetPos = newPos
end sub


' ========================================================================================
' Custom paint keeping the old editor scrollbar visuals: flat track, flat thumb, no
' hover brightening. Reads the ghEditor theme colors live, so a theme switch only
' needs a repaint.
' ========================================================================================
sub frmEditorHScroll_PaintCallback( byval p as CHSCROLLBAR_PAINTINFO ptr )
    dim as CBufferPaint ptr b = p->b
    b->SetBackColor( ghEditor.BackColorScrollBar )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetBackColor( ghEditor.ForeColorScrollBar )
        b->PaintRect( @p->rcThumb )
    end if
end sub


' ========================================================================================
' frmEditorHScroll_Show
' ========================================================================================
function frmEditorHScroll_Show( byval hWndParent as HWND ) as LRESULT

    ' No pre-sizing needed (unlike the vertical bars): frmMain's positioning always
    ' sets the horizontal bars' full rect from ScaleY(SCROLLBAR_HEIGHT) and nothing
    ' reads the bar height back with AfxGetWindowHeight.
    for i as long = 0 to 1
        HWND_FRMEDITOR_HSCROLLBAR(i) = CHScrollBar_Create( hWndParent, _
                iif( i = 0, IDC_FRMEDITOR_HSCROLLBAR0, IDC_FRMEDITOR_HSCROLLBAR1 ) )
        CHScrollBar_SetScrollCallback( HWND_FRMEDITOR_HSCROLLBAR(i), @frmEditorHScroll_OnScroll )
        CHScrollBar_SetPaintCallback( HWND_FRMEDITOR_HSCROLLBAR(i), @frmEditorHScroll_PaintCallback )
    next

    function = 0

end function


' ========================================================================================
' Auto-show/auto-hide: the bars appear while the mouse is over an editor view (or the
' bar itself) and something is scrollable, and disappear otherwise -- except that a
' bar being actively worked (thumb drag / track auto-repeat, mouse captured) is never
' hidden mid-gesture: with capture held the cursor legitimately roams outside the bar.
' ========================================================================================
function frmEditorHScroll_DoNeedScrollBar( byval pDoc as clsDocument ptr ) as long
    for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
        dim as boolean isVisible = IsWindowVisible( HWND_FRMEDITOR_HSCROLLBAR(i) )
        dim as long curState = iif(isVisible, SW_SHOWNOACTIVATE, SW_HIDE)
        dim as long newState = SW_HIDE
        if isMouseOverWindow( pDoc->hWindow(i) ) orelse _
            isMouseOverWindow( HWND_FRMEDITOR_HSCROLLBAR(i) ) then
            newState = frmEditorHScroll_NeedScrollBar( pDoc, i )
        end if
        if CHScrollBar_IsDragging( HWND_FRMEDITOR_HSCROLLBAR(i) ) then
            newState = SW_SHOWNOACTIVATE
        end if
        if newState <> curState then
            ShowWindow( HWND_FRMEDITOR_HSCROLLBAR(i), newState )
            frmMain_PositionWindows()
        end if
    next
    function = 0
end function
