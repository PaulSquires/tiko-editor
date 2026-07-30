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

#include once "frmFindInProject.bi"


' ========================================================================================
' Surface state. The scroll position is a PIXEL OFFSET into the virtual canvas; the row list
' is rebuilt whenever the model or the metrics change, never per paint.
' ========================================================================================
dim shared gFipRows(any)   as FIP_ROW
dim shared gFipRowCount    as long
dim shared gFipCanvasH     as long
dim shared gFipScrollTop   as long
dim shared gFipMetrics     as FIP_METRICS
dim shared as HWND HWND_FRMFINDINPROJECT_SURFACE, HWND_FRMFINDINPROJECT_VSCROLL

' ----------------------------------------------------------------------------------------
' THE EXCERPT POOL.
'
' One real Scintilla view per VISIBLE block, recycled as the list scrolls -- never one per
' result. At the ~2000 matches a project search can produce, a view per match is not a design
' choice, it is an out-of-resources bug.
'
' Each slot is bound to a document with SCI_SETDOCPOINTER and pinned to a first visible line,
' so it is a genuine second view onto the real buffer: real syntax colouring, real line
' numbers, and edits that land in the actual file. The refcount discipline that makes
' rebinding safe lives in modFindProject -- one reference per FILE, held by the model.
' ----------------------------------------------------------------------------------------
type FIP_SLOT
    hWnd     as HWND
    pSciView as any ptr
    pSciDoc  as any ptr   ' what it is currently bound to, or 0
    nRow     as long      ' the FIP_ROW it is showing, or -1 when parked
end type

dim shared gFipPool(0 to FIP_POOL_MAX - 1) as FIP_SLOT
dim shared gFipPoolCount as long
dim shared gFipOldSciProc as WNDPROC
' Which group currently holds keyboard focus, or -1. The header of that group grows an
' "Open File" affordance -- the excerpt is a view onto a real file, and this is how you get
' from it to the file's own tab.
dim shared gFipFocusGroup as long = -1
' Where that affordance was last painted, so a click can be hit-tested against it.
dim shared gFipOpenRect   as RECT
' Set while the pinned line is being re-asserted, so that the scroll it causes cannot come
' back through the subclass and re-assert it again forever.
dim shared gFipPinning    as boolean


' ========================================================================================
' The excerpt pool.
' ========================================================================================

' Open the focused excerpt's file in a tab of its own, at the line the caret is on. The
' excerpt is a view onto the real document, so this is a plain tab switch -- there is nothing
' to load and nothing to save first.
private sub FipPool_OpenFocusedFile()
    if gFipFocusGroup < 0 then exit sub
    if gFipFocusGroup >= gFip.nGroupCount then exit sub

    dim as clsDocument ptr pDoc = gFip.groups(gFipFocusGroup).pDoc
    if pDoc = 0 then exit sub

    ' Where the caret sits in the excerpt, so the file opens where the user was looking.
    dim as long nPos = -1
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for
        if gFipRows( gFipPool(i).nRow ).nGroup <> gFipFocusGroup then continue for
        if GetFocus() = gFipPool(i).hWnd then
            nPos = SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )
            exit for
        end if
    next

    dim as long idx = gTTabCtl.GetTabIndexByDocumentPtr( pDoc )
    if idx = -1 then idx = gTTabCtl.AddTab( pDoc )
    if idx = -1 then exit sub

    gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
    gTTabCtl.SetFocusTab( idx )

    if (nPos >= 0) andalso IsWindow( pDoc->hWindow(0) ) then
        SciExec( pDoc->hWindow(0), SCI_GOTOPOS, nPos, 0 )
        pDoc->CenterCurrentLine
    end if
end sub


' Would this keystroke take the caret out of the excerpt's own lines? Answered BEFORE the
' key reaches Scintilla, so the answer can be "do nothing" rather than "undo it afterwards".
private function FipPool_KeyWouldLeaveBlock( byval hwnd as HWND, byval nKey as WPARAM ) as boolean
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd <> hwnd then continue for
        if (gFipPool(i).nRow < 0) orelse (gFipPool(i).nRow >= gFipRowCount) then return false
        dim as long b = gFipRows( gFipPool(i).nRow ).nBlock
        if (b < 0) orelse (b >= gFip.nBlockCount) then return false

        dim as long nFirst = gFip.blocks(b).nFirstLine
        dim as long nLast  = nFirst + gFip.blocks(b).nLineCount - 1
        dim as long nLine  = SciExec( hwnd, SCI_LINEFROMPOSITION, _
                                      SciExec( hwnd, SCI_GETCURRENTPOS, 0, 0 ), 0 )
        dim as boolean bCtrl = ((GetKeyState(VK_CONTROL) and &H8000) <> 0)

        select case nKey
            case VK_UP, VK_PRIOR
                return (nLine <= nFirst)
            case VK_DOWN, VK_NEXT
                return (nLine >= nLast)
            case VK_HOME
                ' Plain Home is start-of-line and stays put; Ctrl+Home is start-of-DOCUMENT.
                if bCtrl then return (nLine <= nFirst)
            case VK_END
                if bCtrl then return (nLine >= nLast)
        end select
        return false
    next
    return false
end function


' Subclass on every slot, for one reason: Scintilla consumes WM_MOUSEWHEEL itself, so a
' gesture made with the pointer over an excerpt would scroll THAT VIEW off its pinned line
' instead of scrolling the results list. Routed into the bar, which owns the arithmetic.
private function FipPool_SciProc( byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case WM_MOUSEWHEEL
            PsVScrollBar_HandleWheelDelta( HWND_FRMFINDINPROJECT_VSCROLL, cast(short, hiword(wParam)) )
            return 0

        case WM_SETFOCUS, WM_KILLFOCUS
            ' Which group owns the caret decides which header shows "Open File".
            dim as long g = -1
            if uMsg = WM_SETFOCUS then
                for i as long = 0 to gFipPoolCount - 1
                    if (gFipPool(i).hWnd = hwnd) andalso (gFipPool(i).nRow <> -1) then
                        g = gFipRows( gFipPool(i).nRow ).nGroup
                        exit for
                    end if
                next
            end if
            if g <> gFipFocusGroup then
                gFipFocusGroup = g
                if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
                    AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
                end if
            end if

        case WM_SYSKEYDOWN, WM_KEYDOWN
            ' Alt+Enter opens the excerpt's file in a tab of its own. Alt+key arrives as
            ' WM_SYSKEYDOWN, not WM_KEYDOWN -- handling only the latter would have looked
            ' right and never fired.
            if (wParam = VK_RETURN) andalso ((GetKeyState(VK_MENU) and &H8000) <> 0) then
                FipPool_OpenFocusedFile()
                return 0
            end if

            ' LEAVING THE BLOCK IS A NO-OP, not a clamp. Letting the key through and pulling
            ' the caret back afterwards is what produced the reported behaviour: Up on the
            ' first line jumped the caret to column 0, Down on the last line to end of line.
            ' Swallowing the keystroke before Scintilla sees it means nothing moves at all,
            ' which is what "you cannot leave the five lines" should feel like.
            if uMsg = WM_KEYDOWN then
                if FipPool_KeyWouldLeaveBlock( hwnd, wParam ) then return 0
            end if

        case WM_SYSCHAR
            ' AND SWALLOW ITS CHARACTER, or the system beeps on every Alt+Enter. Claiming a
            ' key without consuming the character it manufactures is the family-wide trap
            ' written up in CLAUDE.md -- it shipped in five controls at once and was found by
            ' a user pressing Enter. Here the character is a SYSCHAR because the modifier is
            ' Alt; everything else Alt does must still reach DefWindowProc, so this is
            ' narrowed to the one keystroke that was claimed above.
            if wParam = VK_RETURN then return 0
    end select

    dim as LRESULT lr = CallWindowProc( gFipOldSciProc, hwnd, uMsg, wParam, lParam )

    ' AN EXCERPT NEVER SCROLLS. It is a fixed window onto a fixed run of lines, so typing,
    ' arrowing or clicking must not slide it off them -- you edit within the lines you can
    ' see. Scintilla scrolls to keep the caret visible on all sorts of messages, so rather
    ' than trying to enumerate them the pinned line is simply re-asserted after ANY message
    ' that moved it.
    ' RE-ENTRANCY GUARD, AND IT IS NOT OPTIONAL. SCI_SETFIRSTVISIBLELINE makes Scintilla
    ' scroll, which repaints and notifies, which comes straight back through this same
    ' subclass -- where the line still looks wrong mid-scroll, so it is set again. That
    ' recurses without bound and takes the process out with a stack overflow: measured, as a
    ' segfault immediately after the first search.
    if gFipPinning then return lr
    gFipPinning = true

    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd <> hwnd then continue for
        ' Bounds, not faith: this runs on EVERY message a slot receives, including ones that
        ' arrive while the row list is being rebuilt underneath it.
        if (gFipPool(i).nRow < 0) orelse (gFipPool(i).nRow >= gFipRowCount) then exit for
        dim as long b = gFipRows( gFipPool(i).nRow ).nBlock
        if (b < 0) orelse (b >= gFip.nBlockCount) then exit for

        dim as long nWant = gFip.blocks(b).nFirstLine
        dim as long nLast = nWant + gFip.blocks(b).nLineCount - 1

        ' CONFINE THE CARET TO THE VISIBLE LINES. Pinning the view alone is not enough: the
        ' caret would walk out of the block on Down or Ctrl+End and then be editing lines the
        ' user cannot see. Clamping it here means the arrow keys simply stop at the edges of
        ' the excerpt, which is what "you edit within the five lines" has to mean.
        dim as long nCaretLine = SciExec( hwnd, SCI_LINEFROMPOSITION, _
                                          SciExec( hwnd, SCI_GETCURRENTPOS, 0, 0 ), 0 )
        if nCaretLine < nWant then
            SciExec( hwnd, SCI_GOTOPOS, SciExec( hwnd, SCI_POSITIONFROMLINE, nWant, 0 ), 0 )
        elseif nCaretLine > nLast then
            SciExec( hwnd, SCI_GOTOPOS, SciExec( hwnd, SCI_GETLINEENDPOSITION, nLast, 0 ), 0 )
        end if

        if SciExec( hwnd, SCI_GETFIRSTVISIBLELINE, 0, 0 ) <> nWant then
            SciExec( hwnd, SCI_SETFIRSTVISIBLELINE, nWant, 0 )
        end if
        ' Horizontal too: a long line would otherwise pan out from under the block.
        if SciExec( hwnd, SCI_GETXOFFSET, 0, 0 ) <> 0 then
            SciExec( hwnd, SCI_SETXOFFSET, 0, 0 )
        end if
        exit for
    next

    gFipPinning = false
    return lr
end function


' Park a slot: hide it and let go of whatever document it held.
private sub FipPool_Park( byval i as long )
    if gFipPool(i).hWnd = 0 then exit sub
    ShowWindow( gFipPool(i).hWnd, SW_HIDE )
    if gFipPool(i).pSciDoc then
        ' A null document pointer makes Scintilla give the view a fresh empty buffer of its
        ' own, releasing the one it held.
        SciExec( gFipPool(i).hWnd, SCI_SETDOCPOINTER, 0, 0 )
        gFipPool(i).pSciDoc = 0
    end if
    gFipPool(i).nRow = -1
end sub


' Installed into the model as gFipUnbindDoc: called just before it releases a document, so no
' slot is left pointing at one whose last reference has gone.
private sub FipPool_UnbindDoc( byval pSciDoc as any ptr )
    if pSciDoc = 0 then exit sub
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).pSciDoc = pSciDoc then FipPool_Park( i )
    next
end sub


' Grow the pool to n slots (capped). Slots are created once and live until teardown.
private sub FipPool_Ensure( byval n as long )
    if n > FIP_POOL_MAX then n = FIP_POOL_MAX
    if n <= gFipPoolCount then exit sub
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) = 0 then exit sub

    for i as long = gFipPoolCount to n - 1
        dim as HWND h = CreateWindowEx( 0, "Scintilla", "", _
                            WS_CHILD or WS_CLIPSIBLINGS, 0, 0, 0, 0, _
                            HWND_FRMFINDINPROJECT_SURFACE, _
                            cast(HMENU, IDC_SCINTILLA_EXCERPT + i), _
                            GetModuleHandle(null), null )
        if h = 0 then exit for

        ' MANDATORY. The modification event mask is per-VIEW and defaults to every event, and
        ' every view bound to a document is one of that document's watchers -- so a pool left
        ' at the default fans a full-mask SCN_MODIFIED (and every restyle) out of each of its
        ' slots on every keystroke the user types in a file it happens to be showing.
        SciExec( h, SCI_SETMODEVENTMASK, 0, 0 )
        SciExec( h, SCI_SETVSCROLLBAR, 0, 0 )
        SciExec( h, SCI_SETHSCROLLBAR, 0, 0 )
        ' An excerpt has no room for, and no use for, breakpoints or folding: the symbol and
        ' fold margins are suppressed and only the line-number margin survives. This also
        ' removes the margin-click notifications entirely rather than having to ignore them.
        SciExec( h, SCI_SETMARGINWIDTHN, 1, 0 )
        SciExec( h, SCI_SETMARGINWIDTHN, 2, 0 )
        ' No horizontal scrolling inside a fixed 5-line window; long lines simply clip.
        SciExec( h, SCI_SETSCROLLWIDTHTRACKING, 0, 0 )

        gFipOldSciProc = cast( WNDPROC, SetWindowLongPtr( h, GWLP_WNDPROC, _
                                                          cast(LONG_PTR, @FipPool_SciProc) ) )

        gFipPool(i).hWnd     = h
        gFipPool(i).pSciView = cast( any ptr, SciExec( h, SCI_GETDIRECTPOINTER, 0, 0 ) )
        gFipPool(i).pSciDoc  = 0
        gFipPool(i).nRow     = -1
        gFipPoolCount = i + 1
    next
end sub


' Bind a slot to a block row and place it.
private sub FipPool_Bind( byval i as long, byval nRow as long, byref rcView as RECT )
    dim as long g = gFipRows(nRow).nGroup
    dim as long b = gFipRows(nRow).nBlock
    dim as clsDocument ptr pDoc = gFip.groups(g).pDoc
    if pDoc = 0 then exit sub

    dim as any ptr pSciDoc = gFip.groups(g).pSciDoc
    if pSciDoc = 0 then exit sub

    if gFipPool(i).pSciDoc <> pSciDoc then
        ' SCI_SETDOCPOINTER releases the view's old document and adds a reference to the new
        ' one by itself, so there is nothing to balance here. The model's own per-file
        ' reference is what keeps the OLD document alive through this.
        SciExec( gFipPool(i).hWnd, SCI_SETDOCPOINTER, 0, cast(LPARAM, pSciDoc) )
        gFipPool(i).pSciDoc = pSciDoc
        ' STYLES ARE PER-VIEW. The lexer came with the document, but without this the view
        ' renders in Scintilla's default black-on-white.
        pDoc->ApplyPropertiesToView( gFipPool(i).pSciView )
        ' Re-suppress what ApplyPropertiesToView just set for a full editor.
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 1, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 2, 0 )
        ' The match highlight. The RANGES live on the document (filled once per file by
        ' FindProject_ApplyIndicators), but the indicator's appearance is PER-VIEW, so it has
        ' to be described again on every view that should show it.
        SciExec( gFipPool(i).hWnd, SCI_INDICSETSTYLE, FINDPROJ_INDICATOR, INDIC_STRAIGHTBOX )
        SciExec( gFipPool(i).hWnd, SCI_INDICSETFORE, FINDPROJ_INDICATOR, ghEditor.ForeColorOccurrence )
        SciExec( gFipPool(i).hWnd, SCI_INDICSETALPHA, FINDPROJ_INDICATOR, 90 )
    end if

    ' Pin the window onto the block's first line. Re-asserted on every sync, not just on a
    ' rebind: clicking into an excerpt moves its caret, and anything that then scrolls the
    ' caret into view would slide the block off the lines it is supposed to be showing.
    SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, gFip.blocks(b).nFirstLine, 0 )

    SetWindowPos( gFipPool(i).hWnd, 0, rcView.left, rcView.top, _
                  rcView.right - rcView.left, rcView.bottom - rcView.top, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )
    gFipPool(i).nRow = nRow
end sub


' Match the pool to what is on screen. Called after every scroll and every rebuild.
private sub FipPool_Sync()
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rcClient )

    ' Which block rows are on screen?
    dim as long nVisible(0 to FIP_POOL_MAX - 1)
    dim as long nVisCount = 0
    dim as long nFirst = FipRows_FirstVisible( gFipRows(), gFipRowCount, gFipScrollTop )
    if nFirst >= 0 then
        for r as long = nFirst to gFipRowCount - 1
            if (gFipRows(r).nTop - gFipScrollTop) > rcClient.bottom then exit for
            if gFipRows(r).nKind <> FIP_ROW_BLOCK then continue for
            if nVisCount >= FIP_POOL_MAX then exit for
            nVisible(nVisCount) = r
            nVisCount += 1
        next
    end if

    FipPool_Ensure( nVisCount )

    ' Park every slot showing a row that is no longer visible. Done as a separate pass so the
    ' reuse pass below can find them: a slot already on the right row must NOT be rebound, or
    ' every scroll tick would re-run SETDOCPOINTER and the styling with it.
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for
        dim as boolean bStillVisible = false
        for v as long = 0 to nVisCount - 1
            if nVisible(v) = gFipPool(i).nRow then bStillVisible = true: exit for
        next
        if bStillVisible = false then FipPool_Park( i )
    next

    for v as long = 0 to nVisCount - 1
        dim as long nRow = nVisible(v)

        dim as RECT rcView
        rcView.left   = gFipMetrics.nIndent
        rcView.right  = rcClient.right
        rcView.top    = gFipRows(nRow).nTop - gFipScrollTop + gFipMetrics.nBlockPad
        rcView.bottom = rcView.top + (gFip.blocks( gFipRows(nRow).nBlock ).nLineCount * _
                                      gFipMetrics.nLineHeight)

        ' Already showing this row: reposition and re-pin, nothing more.
        dim as long nSlot = -1
        for i as long = 0 to gFipPoolCount - 1
            if gFipPool(i).nRow = nRow then nSlot = i: exit for
        next
        if nSlot = -1 then
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = -1 then nSlot = i: exit for
            next
        end if
        if nSlot = -1 then continue for      ' pool exhausted; the row just shows empty

        FipPool_Bind( nSlot, nRow, rcView )
    next
end sub


sub frmFindInProject_ApplyTheme()
    if IsWindow( HWND_FRMFINDINPROJECT ) = 0 then exit sub

    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for
        dim as long g = gFipRows( gFipPool(i).nRow ).nGroup
        if gFip.groups(g).pDoc = 0 then continue for
        gFip.groups(g).pDoc->ApplyPropertiesToView( gFipPool(i).pSciView )
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 1, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 2, 0 )
        ' Re-styling can change the line height, so the pin has to be re-asserted.
        dim as long b = gFipRows( gFipPool(i).nRow ).nBlock
        SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, gFip.blocks(b).nFirstLine, 0 )
    next

    PsVScrollBar_SetColors( HWND_FRMFINDINPROJECT_VSCROLL, ghEditor.BackColorScrollBar, _
                            ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBar )

    ' A font or size change moves the line height, which moves every block height and the
    ' whole canvas with it -- so this is a full re-measure, not just a repaint.
    frmFindInProject_Refresh( false )
end sub


private sub FipPool_Destroy()
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd then
            ' Let go of the document BEFORE the window dies.
            SciExec( gFipPool(i).hWnd, SCI_SETDOCPOINTER, 0, 0 )
            DestroyWindow( gFipPool(i).hWnd )
        end if
        gFipPool(i).hWnd     = 0
        gFipPool(i).pSciView = 0
        gFipPool(i).pSciDoc  = 0
        gFipPool(i).nRow     = -1
    next
    gFipPoolCount = 0
end sub


' ========================================================================================
' PURE geometry. No window, no document.
' ========================================================================================

function FipRows_BlockHeight( byval nLineCount as long, byref m as FIP_METRICS ) as long
    dim as long n = nLineCount
    if n < 1 then n = 1
    function = (n * m.nLineHeight) + (m.nBlockPad * 2)
end function


function FipRows_Build( rows() as FIP_ROW, byref nRowCount as long, _
                        byref m as FIP_METRICS ) as long
    nRowCount = 0
    dim as long y = 0

    ' Worst case is one header per group plus every block, so size once rather than growing.
    dim as long nMax = gFip.nGroupCount + gFip.nBlockCount
    if nMax < 1 then
        erase rows
        return 0
    end if
    redim rows(0 to nMax - 1)

    for g as long = 0 to gFip.nGroupCount - 1
        ' A group whose document has been closed or reloaded is emptied rather than removed
        ' (indices have to stay stable), so it simply contributes no rows and the file drops
        ' out of the list.
        if gFip.groups(g).pDoc = 0 then continue for

        with rows(nRowCount)
            .nKind   = FIP_ROW_HEADER
            .nGroup  = g
            .nBlock  = -1
            .nTop    = y
            .nHeight = m.nHeaderHeight
        end with
        y += m.nHeaderHeight
        nRowCount += 1

        ' A collapsed group contributes its header and nothing else -- which is what makes
        ' collapsing cheap: the canvas simply gets shorter and every row below moves up.
        if gFip.groups(g).bCollapsed = false then
            dim as long b0 = gFip.groups(g).nBlockFirst
            for b as long = b0 to b0 + gFip.groups(g).nBlockCount - 1
                with rows(nRowCount)
                    .nKind   = FIP_ROW_BLOCK
                    .nGroup  = g
                    .nBlock  = b
                    .nTop    = y
                    .nHeight = FipRows_BlockHeight( gFip.blocks(b).nLineCount, m )
                end with
                y += rows(nRowCount).nHeight + m.nBlockGap
                nRowCount += 1
            next
        end if
    next

    function = y
end function


function FipRows_FirstVisible( rows() as FIP_ROW, byval nRowCount as long, _
                               byval nTop as long ) as long
    if nRowCount <= 0 then return -1
    ' Rows ascend by nTop and tile the canvas without gaps other than the inter-block gap, so
    ' the first row whose BOTTOM edge is past nTop is the first one on screen.
    ' (nMid, not mid -- MID is FreeBASIC's string function and cannot be a variable name.)
    dim as long lo = 0, hi = nRowCount - 1, nAns = -1
    do while lo <= hi
        dim as long nMid = lo + (hi - lo) \ 2
        if (rows(nMid).nTop + rows(nMid).nHeight) > nTop then
            nAns = nMid
            hi = nMid - 1
        else
            lo = nMid + 1
        end if
    loop
    function = nAns
end function


' ========================================================================================
' One document line's height, asked of Scintilla rather than guessed, so the excerpt views
' that replace this painted placeholder line up with the geometry already built for them.
' Falls back to a scaled default when nothing is realized yet.
' ========================================================================================
private function FipMetrics_LineHeight() as long
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if IsWindow( pDoc->hWindow(0) ) then
            dim as long n = SciExec( pDoc->hWindow(0), SCI_TEXTHEIGHT, 0, 0 )
            if n > 0 then return n
        end if
        pDoc = pDoc->pDocNext
    loop
    ' Nothing realized: a plausible scaled default. Any later Refresh picks up the real value.
    function = AfxScaleY(16)
end function


private sub FipMetrics_Measure( byref m as FIP_METRICS )
    m.nLineHeight   = FipMetrics_LineHeight()
    m.nHeaderHeight = AfxScaleY(FIP_HEADER_HEIGHT)
    m.nBlockPad     = AfxScaleY(FIP_BLOCK_PAD)
    m.nBlockGap     = AfxScaleY(FIP_BLOCK_GAP)
    m.nIndent       = AfxScaleX(FIP_INDENT)
end sub


' ========================================================================================
' Push the scrollbar's range from the canvas, and clamp the scroll position to it.
' ========================================================================================
private sub FipSurface_SyncScrollBar()
    if IsWindow( HWND_FRMFINDINPROJECT_VSCROLL ) = 0 then exit sub

    dim as RECT rc
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rc )
    dim as long nPage = rc.bottom - rc.top
    if nPage < 1 then nPage = 1

    dim as long nMax = gFipCanvasH - nPage
    if nMax < 0 then nMax = 0
    if gFipScrollTop > nMax then gFipScrollTop = nMax
    if gFipScrollTop < 0 then gFipScrollTop = 0

    PsVScrollBar_SetRange( HWND_FRMFINDINPROJECT_VSCROLL, gFipCanvasH, nPage, gFipScrollTop )
    ' One notch moves three document lines, which is what the editor's own wheel does.
    PsVScrollBar_SetWheelStep( HWND_FRMFINDINPROJECT_VSCROLL, gFipMetrics.nLineHeight * 3 )
end sub


' The twisty column. FipSurface_PaintHeader puts the chevron at the header's left padding,
' and the Find bar's fold icon is placed on this same X so the two read as one column.
function frmFindInProject_TwistyColumnX() as long
    ' THE CENTRE of the twisty column, not its left edge.
    '
    ' Both users of it centre a glyph on it, and they do so in cells of DIFFERENT widths --
    ' the header paints into a 16px box, the Find bar's fold icon into a PsIconPanel cell as
    ' tall as the find field. Matching left edges therefore did NOT line the two glyphs up;
    ' only a shared centre does.
    return AfxScaleX(6) + AfxScaleX(8)
end function


' Are all the groups folded? Drives which way round the Find bar's fold icon points. With no
' results at all the answer is FALSE, so the icon offers "collapse" rather than sitting in the
' expand state over an empty list.
function frmFindInProject_AllCollapsed() as boolean
    dim as long nLive = 0
    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc = 0 then continue for
        nLive += 1
        if gFip.groups(g).bCollapsed = false then return false
    next
    return (nLive > 0)
end function


sub frmFindInProject_SetAllCollapsed( byval bCollapsed as boolean )
    for g as long = 0 to gFip.nGroupCount - 1
        gFip.groups(g).bCollapsed = bCollapsed
    next
    ' Back to the top: after folding everything the old scroll position points into a canvas
    ' that no longer exists.
    frmFindInProject_Refresh( true )
end sub


' Installed into the model as gFipModelChanged. Keeps the scroll where it is: a file dropping
' out because its document closed should not also throw the user back to the top.
private sub frmFindInProject_OnModelChanged()
    frmFindInProject_Refresh( false )
end sub


sub frmFindInProject_Refresh( byval bResetScroll as boolean = false )
    FipMetrics_Measure( gFipMetrics )
    gFipCanvasH = FipRows_Build( gFipRows(), gFipRowCount, gFipMetrics )
    if bResetScroll then gFipScrollTop = 0
    FipSurface_SyncScrollBar()
    ' Park every slot first: the row list has just been rebuilt, so a slot's stored row index
    ' refers to the OLD list and would otherwise be matched against the new one by number.
    for i as long = 0 to gFipPoolCount - 1
        gFipPool(i).nRow = -1
        if gFipPool(i).hWnd then ShowWindow( gFipPool(i).hWnd, SW_HIDE )
    next
    FipPool_Sync()
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
        AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
    end if
end sub


' ========================================================================================
' True when the Find in Project tab is the tab currently showing.
' ========================================================================================
function frmFindInProject_IsActive() as boolean
    return gTTabCtl.IsFindTab( gTTabCtl.GetCurSel() )
end function


' Width of a run of text in the font currently set on the buffer. DT_CALCRECT through
' PaintTextEx, because PaintText forces DT_VCENTER or DT_SINGLELINE and cannot measure --
' the same route frmAbout takes.
private function FipSurface_TextW( byval b as PsBufferPaint ptr, byval wszText as DWSTRING ) as long
    dim as RECT rc
    rc.left   = 0
    rc.top    = 0
    rc.right  = 32000
    rc.bottom = 32000
    b->PaintTextEx( wszText, @rc, DT_LEFT or DT_SINGLELINE or DT_CALCRECT )
    function = rc.right - rc.left
end function


' ========================================================================================
' One file's separator bar: chevron, name, path, match count.
' ========================================================================================
private sub FipSurface_PaintHeader( byval b as PsBufferPaint ptr, byref r as FIP_ROW, _
                                    byval yTop as long, byref rcClient as RECT )
    dim as RECT rc
    rc.left   = 0
    rc.right  = rcClient.right
    rc.top    = yTop
    rc.bottom = yTop + r.nHeight

    ' A ROUNDED CARD, not a full-width band with a rule under it. The headers are the
    ' structure of this list, and a card reads as one object holding a filename rather than
    ' as a line drawn across the client area. Highlighted rather than a slightly different
    ' grey, so it stands out from what is otherwise wall-to-wall code.
    dim as RECT rcCard = rc
    rcCard.left   += AfxScaleX(4)
    rcCard.right  -= AfxScaleX(4)
    rcCard.top    += AfxScaleY(2)
    rcCard.bottom -= AfxScaleY(2)
    b->SetBackColor( ghEditor.BackColorCurrentLine )
    b->PaintRoundRect( @rcCard )

    dim as long nPad = AfxScaleX(6)

    ' The twisty. Down = expanded, right = collapsed, which is the direction every tree in
    ' tiko already uses.
    dim as RECT rcGlyph = rc
    rcGlyph.left  = frmFindInProject_TwistyColumnX() - AfxScaleX(8)
    rcGlyph.right = rcGlyph.left + AfxScaleX(16)
    b->SetFont( ghFont(SYMBOLFONT_9) )
    b->SetForeColor( ghEditor.ForeColorText )
    b->PaintText( iif( gFip.groups(r.nGroup).bCollapsed, wszIconChevronRight, wszIconChevronDown ), _
                  @rcGlyph, DT_CENTER )

    ' Name, then the match count, then the directory in the dimmer line-number colour. Drawn
    ' in that order because the count must not be pushed off the right by a long path.
    dim as DWSTRING wszName  = AfxStrPathname( "NAMEX", gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszDir   = AfxStrPathname( "PATH",  gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszCount = "(" & str( gFip.groups(r.nGroup).nMatchCount ) & ")"

    dim as RECT rcText = rc
    rcText.left = rcGlyph.right + nPad

    b->SetFont( ghFont(GUIFONTBOLD_10) )
    b->SetForeColor( ghEditor.ForeColorText )
    dim as long nNameW = FipSurface_TextW( b, wszName ) + nPad
    b->PaintText( wszName, @rcText, DT_LEFT )

    dim as RECT rcCount = rc
    rcCount.left = rcText.left + nNameW
    b->SetFont( ghFont(GUIFONT_9) )
    b->SetForeColor( ghEditor.ForeColorLinenumbers )
    dim as long nCountW = FipSurface_TextW( b, wszCount ) + nPad
    b->PaintText( wszCount, @rcCount, DT_LEFT )

    ' "Open File  Alt+Enter", but only on the group whose excerpt has the caret. It is an
    ' affordance for something you can only do while you are IN a file, so showing it on every
    ' header would be noise -- and it would also be ambiguous about which file it meant.
    dim as long nOpenLeft = rcCard.right
    if r.nGroup = gFipFocusGroup then
        ' Id 401, newly minted: there was no "Open File" string and no blank slot left, so
        ' MAXIMUM moved 400 -> 401 in all six .lang files, each with a real translation.
        ' "Alt+Enter" is deliberately NOT localized -- it is a keystroke, not a phrase.
        dim as DWSTRING wszOpen = L(401, "Open File")
        dim as DWSTRING wszKey  = "Alt+Enter"

        b->SetFont( ghFont(GUIFONT_9) )
        dim as long nKeyW  = FipSurface_TextW( b, wszKey )
        dim as long nOpenW = FipSurface_TextW( b, wszOpen )
        nOpenLeft = rcCard.right - nPad - nKeyW - nPad*2 - nOpenW - nPad

        dim as RECT rcOpen = rcCard
        rcOpen.left  = nOpenLeft
        rcOpen.right = rcCard.right - nPad
        rcOpen.top    += AfxScaleY(2)
        rcOpen.bottom -= AfxScaleY(2)
        b->SetBackColor( ghEditor.BackColorText )
        b->PaintRoundRect( @rcOpen )

        dim as RECT rcLbl = rcOpen
        rcLbl.left += nPad
        b->SetForeColor( ghEditor.ForeColorText )
        b->SetBackColor( ghEditor.BackColorText )
        b->PaintText( wszOpen, @rcLbl, DT_LEFT )

        dim as RECT rcKey = rcOpen
        rcKey.right -= nPad
        b->SetForeColor( ghEditor.ForeColorLinenumbers )
        b->PaintText( wszKey, @rcKey, DT_RIGHT )

        ' Remembered so a click can be hit-tested. Only ever one is on screen at a time.
        gFipOpenRect = rcOpen
    end if

    if len(wszDir) then
        dim as RECT rcDir = rc
        rcDir.left = rcCount.left + nCountW
        ' Stop the path before the Open File affordance rather than letting it run under it.
        rcDir.right = nOpenLeft - nPad
        if rcDir.left < rcDir.right then
            b->SetForeColor( ghEditor.ForeColorLinenumbers )
            b->SetBackColor( ghEditor.BackColorCurrentLine )
            b->PaintTextEx( wszDir, @rcDir, DT_LEFT or DT_SINGLELINE or DT_VCENTER or DT_END_ELLIPSIS )
        end if
    end if

    ' (No hairline under the header any more -- the card's own edge is the separation, and a
    ' rule as well would read as two boundaries for one thing. The hairlines BETWEEN excerpts
    ' inside a group stay: those genuinely divide sibling windows.)
end sub


' (FipSurface_PaintBlock is gone. Excerpt blocks are REAL SCINTILLA VIEWS now -- see
'  FipPool_Sync -- so the surface paints only the background behind them and the separator
'  bars. The painted placeholder it replaced drew line numbers and raw line text in a
'  proportional font; it existed so the geometry could be asserted before any Scintilla was
'  involved, and the block heights it was measured against are unchanged.)



' ========================================================================================
' Paint the surface: the visible slice of the canvas and nothing else.
'
' Colours come from the EDITOR palette, not the panel one: this window sits in the space a
' document's Scintilla windows occupy, and it is about to hold real Scintilla excerpt views
' of those same documents. Chrome drawn against a different background from the code inside
' it would read as broken.
' ========================================================================================
' The whole frame, taking the buffer rather than owning it. Split out from WM_PAINT SO THAT A
' PROBE CAN DRIVE THE EXACT PATH WM_PAINT USES -- PsDatePicker's lesson: a probe that renders
' through some other route can report a healthy number while the real window draws nothing.
private sub FipSurface_Render( byval b as PsBufferPaint ptr, byref rc as RECT )
    b->SetBackColor( ghEditor.BackColorText )
    b->PaintRect( @rc )

    if (gFip.bValid = false) orelse (gFipRowCount = 0) then
        ' Empty state. Reuses the tab's own caption rather than minting a localized id that
        ' would be retired the moment this gets a real message.
        dim as RECT rcText = rc
        rcText.top  += AfxScaleY(20)
        rcText.left += AfxScaleX(20)
        b->SetFont( ghFont(GUIFONT_10) )
        b->SetForeColor( ghEditor.ForeColorLinenumbers )
        b->SetBackColor( ghEditor.BackColorText )
        b->PaintText( L(206, "Find in Project"), @rcText, DT_LEFT )
        exit sub
    end if

    dim as long nFirst = FipRows_FirstVisible( gFipRows(), gFipRowCount, gFipScrollTop )
    if nFirst >= 0 then
        for i as long = nFirst to gFipRowCount - 1
            dim as long yTop = gFipRows(i).nTop - gFipScrollTop
            if yTop > rc.bottom then exit for
            ' Only the separator bars and the rules between excerpts are painted. A block row
            ' is covered by a real Scintilla view, so the background fill above is all it
            ' needs from here -- and a row whose view could not be bound (the pool is capped)
            ' simply shows that background.
            if gFipRows(i).nKind = FIP_ROW_HEADER then
                FipSurface_PaintHeader( b, gFipRows(i), yTop, rc )
            else
                ' A hairline in the gap BELOW each excerpt, so several excerpts from one file
                ' read as separate windows onto it rather than as one long run of code. Not
                ' drawn after the last excerpt in a group -- the next header's own top edge
                ' already separates those.
                dim as boolean bLastInGroup = true
                if (i + 1) < gFipRowCount then
                    if (gFipRows(i+1).nKind = FIP_ROW_BLOCK) andalso _
                       (gFipRows(i+1).nGroup = gFipRows(i).nGroup) then bLastInGroup = false
                end if
                if bLastInGroup = false then
                    dim as RECT rcRule
                    rcRule.left   = gFipMetrics.nIndent
                    rcRule.right  = rc.right
                    rcRule.top    = yTop + gFipRows(i).nHeight + (gFipMetrics.nBlockGap \ 2)
                    rcRule.bottom = rcRule.top + AfxScaleY(1)
                    b->SetBackColor( ghEditor.Divider )
                    b->PaintRect( @rcRule )
                end if
            end if
        next
    end if
end sub


' Thin WM_PAINT wrapper: owns the buffer, delegates the drawing.
private sub FipSurface_OnPaint( byval hwnd as HWND )
    dim as PsBufferPaint b
    ' NOTE: BeginDoubleBuffer calls BeginPaint itself. Do not wrap this in another
    ' BeginPaint/EndPaint pair -- the outer one validates the update region and the buffer
    ' then paints into an empty rect, giving a blank window with a clean build.
    b.BeginDoubleBuffer( hwnd )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    FipSurface_Render( @b, rc )
    b.EndDoubleBuffer()
end sub


' ========================================================================================
' How many distinct colours does the surface actually put on screen?
'
' Renders offscreen THROUGH THE REAL FipSurface_Render and counts distinct pixels over a
' sampled grid. This exists because every geometry assertion in this file is blind to whether
' anything is drawn at all: a painter that renders nothing, or floods the whole surface with
' one colour, satisfies all of them. Both have happened in this codebase.
'
' Counting stops at the cap -- the question is "is there ink", not "how much".
' ========================================================================================
private function FipSurface_CountRenderedTones() as long
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) = 0 then return -1

    dim as RECT rcClient
    GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return -1

    dim as HDC hRef = GetDC( HWND_FRMFINDINPROJECT_SURFACE )
    if hRef = 0 then return -1
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( HWND_FRMFINDINPROJECT_SURFACE, hRef )
        return -1
    end if

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels read back
    ' are laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( HWND_FRMFINDINPROJECT_SURFACE, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return -1
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( HWND_FRMFINDINPROJECT_SURFACE, hMem, rcClient, hMem )
        FipSurface_Render( @b, rcClient )
        b.EndDoubleBuffer()
    end scope

    const FIP_TONE_CAP = 64
    dim as ulong seen(0 to FIP_TONE_CAP - 1)
    dim as long nSeen = 0
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = 0 to ch - 1 step 2
        for x as long = 0 to cw - 1 step 2
            dim as ulong c = px[(y * cw) + x]
            dim as boolean bFound = false
            for i as long = 0 to nSeen - 1
                if seen(i) = c then bFound = true: exit for
            next
            if bFound = false then
                if nSeen >= FIP_TONE_CAP then exit for, for
                seen(nSeen) = c
                nSeen += 1
            end if
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )
    function = nSeen
end function


' ========================================================================================
' Which row is at this client y? -1 for none.
' ========================================================================================
private function FipSurface_RowAtPoint( byval y as long ) as long
    dim as long nCanvasY = y + gFipScrollTop
    dim as long i = FipRows_FirstVisible( gFipRows(), gFipRowCount, nCanvasY )
    if i < 0 then return -1
    ' FirstVisible answers the row whose bottom is past nCanvasY, which may be the row AFTER a
    ' point that fell in an inter-block gap. Reject that rather than reporting a near miss.
    if nCanvasY < gFipRows(i).nTop then return -1
    function = i
end function


' ========================================================================================
' Surface window procedure.
' ========================================================================================
private function FipSurface_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            return true

        case WM_PAINT
            FipSurface_OnPaint( hwnd )
            return 0

        case WM_SIZE
            ' The page size changed, so the scrollbar range and the clamp both move -- and the
            ' excerpt views have to be re-placed and re-widened to match.
            FipSurface_SyncScrollBar()
            FipPool_Sync()
            return 0

        case WM_MOUSEWHEEL
            ' Route the gesture into the bar: the surface owns no scroll state of its own
            ' beyond gFipScrollTop, and the bar owns the arithmetic (and the sub-notch
            ' accumulation that makes a precision touchpad scroll smoothly).
            PsVScrollBar_HandleWheelDelta( HWND_FRMFINDINPROJECT_VSCROLL, _
                                           cast(short, hiword(wParam)) )
            return 0

        case WM_LBUTTONDOWN
            ' Signed: a click can be dragged above the client area.
            dim as long yClick = cast(short, hiword(lParam))
            dim as long xClick = cast(short, loword(lParam))

            ' The "Open File" affordance, when one is showing. Tested before the header
            ' collapse below, or clicking it would fold the group instead.
            if gFipFocusGroup >= 0 then
                if (xClick >= gFipOpenRect.left) andalso (xClick < gFipOpenRect.right) andalso _
                   (yClick >= gFipOpenRect.top) andalso (yClick < gFipOpenRect.bottom) then
                    FipPool_OpenFocusedFile()
                    return 0
                end if
            end if

            dim as long i = FipSurface_RowAtPoint( yClick )
            if i >= 0 then
                if gFipRows(i).nKind = FIP_ROW_HEADER then
                    dim as long g = gFipRows(i).nGroup
                    gFip.groups(g).bCollapsed = not gFip.groups(g).bCollapsed
                    ' Keep the scroll where it is: the header the user clicked should not
                    ' jump out from under the cursor.
                    frmFindInProject_Refresh( false )
                end if
            end if
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDINPROJECT_SURFACE = 0
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' The scrollbar moved.
' ========================================================================================
private sub FipSurface_ScrollCallback( byval hSB as HWND, byval nPos as integer )
    gFipScrollTop = nPos
    ' The excerpt views move with the content: they are positioned in VIEWPORT coordinates,
    ' so a scroll repositions them rather than moving one big page. Only the handful that are
    ' on screen exist, which is what makes that cheap.
    FipPool_Sync()
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
        AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
    end if
end sub


' ========================================================================================
' frmFindInProject window procedure
' ========================================================================================
private function frmFindInProject_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            ' Fully painted in WM_PAINT.
            return true

        case WM_PAINT
            ' The surface and the scrollbar cover almost all of this window; only the strip
            ' the scrollbar vacates when it hides is left, so a plain fill is enough.
            dim as PsBufferPaint b
            b.BeginDoubleBuffer( hwnd )
            b.SetBackColor( ghEditor.BackColorText )
            b.PaintClientRect()
            b.EndDoubleBuffer()
            return 0

        case WM_DESTROY
            ' BEFORE the windows go: every excerpt view has to let go of its document while it
            ' is still alive to be told. Handled here rather than in WM_NCDESTROY because by
            ' then the children are already gone and there is nothing left to send to.
            '
            ' The model's own per-file references are NOT released here -- they survive the
            ' tab being closed and are given back by FindProject_Clear, whose messenger is the
            ' scratch view in modFindProject, destroyed last of all at shutdown.
            FipPool_Destroy()
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMFINDINPROJECT = 0
            ' The children have been destroyed with this window. The surface zeroes its own
            ' handle in its WM_NCDESTROY; the scrollbar's is a plain global, so it is cleared
            ' here rather than left dangling.
            HWND_FRMFINDINPROJECT_VSCROLL = 0
            ' The model must not keep calling into a surface that no longer exists.
            gFipModelChanged = 0
            gFipUnbindDoc    = 0
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Create the results window. Idempotent -- returns the existing handle if it already exists.
' ========================================================================================
function frmFindInProject_CreateWindow() as HWND
    if IsWindow( HWND_FRMFINDINPROJECT ) then return HWND_FRMFINDINPROJECT

    dim pWindow as CWindow ptr = new CWindow
    ' A child of frmMain, created hidden and sized by frmMain_PositionWindows -- exactly how
    ' a document's Scintilla windows are handled.
    '
    ' WS_EX_CONTROLPARENT is correct here and is NOT the mistake PsButton had to fix: this
    ' window really will hold child controls the dialog manager should recurse into (the
    ' pooled Scintilla excerpt views), and it never takes a tab stop of its own.
    HWND_FRMFINDINPROJECT = pWindow->Create( HWND_FRMMAIN, "", @frmFindInProject_WndProc, _
                    0, 0, 0, 0, _
                    WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                    WS_EX_CONTROLPARENT )
    if HWND_FRMFINDINPROJECT = 0 then return 0

    ' ------------------------------------------------------------------------------------
    ' The surface: the viewport onto the virtual canvas. It is a separate window from the
    ' container so that its CLIENT AREA IS the viewport -- the scrollbar sits outside it, so
    ' nothing has to subtract a scrollbar width when hit-testing or when placing the excerpt
    ' views that will live on it.
    ' ------------------------------------------------------------------------------------
    dim pSurface as CWindow ptr = new CWindow
    HWND_FRMFINDINPROJECT_SURFACE = pSurface->Create( HWND_FRMFINDINPROJECT, "", _
                    @FipSurface_WndProc, 0, 0, 0, 0, _
                    WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                    WS_EX_CONTROLPARENT )
    ' CWindow's default class style carries CS_HREDRAW or CS_VREDRAW, which flicker badly on
    ' a window that is resized and repainted as often as this one. Read-modify-write: the
    ' setter REPLACES, and it needs the window to exist. (PsScrollPanel's precedent.)
    if HWND_FRMFINDINPROJECT_SURFACE then
        pSurface->ClassStyle = pSurface->ClassStyle and (not (CS_HREDRAW or CS_VREDRAW))
    end if

    HWND_FRMFINDINPROJECT_VSCROLL = PsVScrollBar_Create( HWND_FRMFINDINPROJECT, _
                                                         IDC_FRMFINDINPROJECT_VSCROLL )
    PsVScrollBar_SetScrollCallback( HWND_FRMFINDINPROJECT_VSCROLL, @FipSurface_ScrollCallback )
    PsVScrollBar_SetColors( HWND_FRMFINDINPROJECT_VSCROLL, ghEditor.BackColorScrollBar, _
                            ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBar )

    ' Hand the model its two callbacks. It is included long before this file and cannot name
    ' anything in it, so this is how it reaches the surface at all -- to say "I changed" and,
    ' critically, "let go of this document, I am about to release it".
    gFipModelChanged = @frmFindInProject_OnModelChanged
    gFipUnbindDoc    = @FipPool_UnbindDoc

    return HWND_FRMFINDINPROJECT
end function


' ========================================================================================
' Size the surface into the document client area. rcDoc arrives in frmMain client
' coordinates -- the same rect frmMain_PositionWindows hands a document's editors.
' ========================================================================================
sub frmFindInProject_PositionWindows( byval rcDoc as RECT )
    if IsWindow( HWND_FRMFINDINPROJECT ) = 0 then exit sub

    dim as long nW = rcDoc.right - rcDoc.left
    dim as long nH = rcDoc.bottom - rcDoc.top

    SetWindowPos( HWND_FRMFINDINPROJECT, 0, _
                  rcDoc.left, rcDoc.top, nW, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' The scrollbar strip stays RESERVED whether or not the thumb is showing -- the same call
    ' PsScrollPanel makes, and for the same reason: reclaiming the width would reflow every
    ' excerpt line the moment the results got short enough not to need scrolling.
    dim as long nBar = AfxScaleX(SCROLLBAR_WIDTH_EDITOR)
    dim as long nSurfaceW = nW - nBar
    if nSurfaceW < 0 then nSurfaceW = 0

    SetWindowPos( HWND_FRMFINDINPROJECT_SURFACE, 0, 0, 0, nSurfaceW, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )
    SetWindowPos( HWND_FRMFINDINPROJECT_VSCROLL, 0, nSurfaceW, 0, nBar, nH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' A width change does not alter the canvas height (excerpts do not wrap), but the page
    ' size did change, so the range and the clamp have to be re-pushed.
    FipSurface_SyncScrollBar()
end sub


' ========================================================================================
' Env-gated self-test: TIKO_FINDPROJ_SELFTEST=1
'
' Drives the real Show and the real close path, because the thing under test is WIRING --
' the sentinel surviving GetpDoc, the outgoing editors actually hiding, the surface being
' sized, the menus' document count, and the session index. None of that is reachable from a
' pure function, and the build was clean before any of it worked.
' ========================================================================================
dim shared as long gFipTestPass, gFipTestFail

private sub frmFindInProject_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gFipTestPass += 1
    else
        gFipTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

' How many documents currently have a VISIBLE Scintilla window.
private function frmFindInProject_CountVisibleEditors() as long
    dim as long n = 0
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        for i as long = lbound(pDoc->hWindow) to ubound(pDoc->hWindow)
            if IsWindowVisible( pDoc->hWindow(i) ) then n += 1
        next
        pDoc = pDoc->pDocNext
    loop
    function = n
end function

sub frmFindInProject_RunSelfTest()
    if environ("TIKO_FINDPROJ_SELFTEST") <> "1" then exit sub

    gFipTestPass = 0
    gFipTestFail = 0
    print "--- TIKO_FINDPROJ_SELFTEST ---"

    ' ---- the visible-row search, on a SYNTHETIC canvas ------------------------------------
    ' Driven by a hand-built row array rather than by a real search, so it is tested
    ' independently of the model and of whatever files happen to be open. Four rows, 100px
    ' each, tiling 0..399.
    dim synth(0 to 3) as FIP_ROW
    for i as long = 0 to 3
        synth(i).nTop    = i * 100
        synth(i).nHeight = 100
    next
    frmFindInProject_Check( "rowsearch: the top of the canvas finds row 0", _
        FipRows_FirstVisible( synth(), 4, 0 ) = 0, str(FipRows_FirstVisible( synth(), 4, 0 )) )
    frmFindInProject_Check( "rowsearch: one pixel inside row 0 still finds row 0", _
        FipRows_FirstVisible( synth(), 4, 99 ) = 0, str(FipRows_FirstVisible( synth(), 4, 99 )) )
    ' The boundary: at y=100 row 0 is entirely above the fold, so row 1 is first.
    frmFindInProject_Check( "rowsearch: exactly on a boundary finds the LOWER row", _
        FipRows_FirstVisible( synth(), 4, 100 ) = 1, str(FipRows_FirstVisible( synth(), 4, 100 )) )
    frmFindInProject_Check( "rowsearch: mid-canvas finds the containing row", _
        FipRows_FirstVisible( synth(), 4, 250 ) = 2, str(FipRows_FirstVisible( synth(), 4, 250 )) )
    frmFindInProject_Check( "rowsearch: the last pixel finds the last row", _
        FipRows_FirstVisible( synth(), 4, 399 ) = 3, str(FipRows_FirstVisible( synth(), 4, 399 )) )
    ' Past the end there is nothing to draw -- and -1 rather than a clamped 3, so the painter
    ' does not redraw the last row forever when scrolled past it.
    frmFindInProject_Check( "rowsearch: past the end answers -1, not a clamp", _
        FipRows_FirstVisible( synth(), 4, 400 ) = -1, str(FipRows_FirstVisible( synth(), 4, 400 )) )
    frmFindInProject_Check( "rowsearch: an empty canvas answers -1", _
        FipRows_FirstVisible( synth(), 0, 0 ) = -1, str(FipRows_FirstVisible( synth(), 0, 0 )) )

    dim as long nTabsBefore    = gTTabCtl.GetItemCount()
    dim as long nDocTabsBefore = gTTabCtl.GetDocumentTabCount()
    dim as long nSelBefore     = gTTabCtl.GetCurSel()
    dim as long nVisBefore     = frmFindInProject_CountVisibleEditors()

    ' With no documents open, the "editors get hidden" and "document count is unchanged"
    ' assertions cannot distinguish a working fix from a broken one. Said out loud rather
    ' than left as a flattering green.
    if nDocTabsBefore = 0 then
        print "  NOTE: no document tabs open -- the hide-the-editors assertions are VACUOUS."
    end if

    frmFindInProject_Check( "baseline: no Find tab is open", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )

    ' ---- open it ------------------------------------------------------------------------
    frmFindInProject_Show()

    dim as long idx = gTTabCtl.FindTabIndex()
    frmFindInProject_Check( "Show: the Find tab exists", idx <> -1, str(idx) )

    if idx <> -1 then
        ' THE SENTINEL INVARIANT. Every one of the ~70 "if pDoc then <dereference>" call
        ' sites in the app depends on these three answers being exactly this combination:
        ' a VALID tab, that IS the Find tab, whose pDoc reads back NULL.
        frmFindInProject_Check( "sentinel: the tab is a valid tab", _
            gTTabCtl.IsValidTab(idx) = true )
        frmFindInProject_Check( "sentinel: IsFindTab sees it", _
            gTTabCtl.IsFindTab(idx) = true )
        frmFindInProject_Check( "sentinel: GetpDoc answers NULL, not 1", _
            gTTabCtl.GetpDoc(idx) = 0 )
        frmFindInProject_Check( "sentinel: GetActiveDocumentPtr answers NULL", _
            gTTabCtl.GetActiveDocumentPtr() = 0 )

        frmFindInProject_Check( "Show: the Find tab became the current tab", _
            gTTabCtl.GetCurSel() = idx, str(gTTabCtl.GetCurSel()) & " vs " & str(idx) )

        ' The count split that keeps the File / Compile / Debug menus honest.
        frmFindInProject_Check( "counts: the tab count went UP by one", _
            gTTabCtl.GetItemCount() = nTabsBefore + 1, str(gTTabCtl.GetItemCount()) )
        frmFindInProject_Check( "counts: the DOCUMENT tab count did not move", _
            gTTabCtl.GetDocumentTabCount() = nDocTabsBefore, _
            str(gTTabCtl.GetDocumentTabCount()) & " vs " & str(nDocTabsBefore) )

        ' The session writers skip this tab, so the persisted index must name a tab that will
        ' actually be recreated -- i.e. it must land inside the DOCUMENT tab count.
        '
        ' The obvious form of this assertion, "nSave <> idx", is WRONG and was written that
        ' way first: with the Find tab as the only tab it sits at index 0 and the fallback is
        ' also 0, so it failed while nothing was broken. It could not tell "points at the
        ' Find tab" from "there is nothing else to point at". This form can.
        dim as long nSave = gTTabCtl.SaveActiveTabIndex()
        if nDocTabsBefore = 0 then
            frmFindInProject_Check( "session: with no documents, ActiveTab is 0", _
                nSave = 0, str(nSave) )
        else
            frmFindInProject_Check( "session: ActiveTab names a tab that will be restored", _
                (nSave >= 0) andalso (nSave < nDocTabsBefore), _
                str(nSave) & " of " & str(nDocTabsBefore) )
        end if

        frmFindInProject_Check( "layout: every document editor is hidden", _
            frmFindInProject_CountVisibleEditors() = 0, _
            str(frmFindInProject_CountVisibleEditors()) & " still visible" )

        ' THE DisplayScintilla FIX, and it needs a BARE SetFocusTab to be visible at all.
        '
        ' The assertion above does NOT test it: frmFindInProject_Show pairs TabFocusLost with
        ' SetFocusTab, and TabFocusLost hides the outgoing document through a VALID document
        ' index -- so pDocShow is non-null there and the hide-all loop runs even with the old
        ' early return in place. Measured: restoring that early return left this suite fully
        ' green until the sequence below was added.
        '
        ' Seven paths call SetFocusTab bare, and two of them can land on the Find tab: the
        ' tabs-list dropdown (modContextMenus) and the tab right-click. This reproduces that
        ' exact sequence -- select a document tab, then select the Find tab with no
        ' TabFocusLost -- which is where a null pDocShow reaches DisplayScintilla and the
        ' outgoing editors used to stay painted over the results.
        if nDocTabsBefore > 0 then
            dim as long nDocTab = -1
            for i as long = 0 to gTTabCtl.GetItemCount() - 1
                if gTTabCtl.GetpDoc(i) then nDocTab = i: exit for
            next
            if nDocTab <> -1 then
                gTTabCtl.SetFocusTab( nDocTab )
                frmFindInProject_Check( "bare switch: the document's editor is showing first", _
                    frmFindInProject_CountVisibleEditors() > 0, _
                    str(frmFindInProject_CountVisibleEditors()) )
                ' Deliberately NO TabFocusLost here -- that is the whole point.
                gTTabCtl.SetFocusTab( idx )
                frmFindInProject_Check( "bare switch: editors hide without TabFocusLost", _
                    frmFindInProject_CountVisibleEditors() = 0, _
                    str(frmFindInProject_CountVisibleEditors()) & " still visible" )
            end if
        else
            print "  NOTE: no document tabs -- the bare-SetFocusTab assertions are SKIPPED."
        end if

        ' The surface itself, sized by frmMain_PositionWindows off the hoisted rcDoc. Before
        ' that hoist the rect was computed only on the pDoc <> 0 path, so this was 0x0.
        frmFindInProject_Check( "layout: the results window exists", _
            IsWindow( HWND_FRMFINDINPROJECT ) <> 0 )
        frmFindInProject_Check( "layout: the results window is visible", _
            IsWindowVisible( HWND_FRMFINDINPROJECT ) <> 0 )

        dim as RECT rcSurf = AfxGetWindowRect( HWND_FRMFINDINPROJECT )
        MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rcSurf), 2 )
        dim as RECT rcMain: GetClientRect( HWND_FRMMAIN, @rcMain )
        frmFindInProject_Check( "layout: the results window has a real size", _
            (rcSurf.right - rcSurf.left > 0) andalso (rcSurf.bottom - rcSurf.top > 0), _
            str(rcSurf.right - rcSurf.left) & " x " & str(rcSurf.bottom - rcSurf.top) )
        frmFindInProject_Check( "layout: it fits inside frmMain's client area", _
            (rcSurf.right <= rcMain.right) andalso (rcSurf.bottom <= rcMain.bottom), _
            str(rcSurf.right) & "," & str(rcSurf.bottom) & " in " & _
            str(rcMain.right) & "," & str(rcMain.bottom) )

        ' The bar is forced open, and the file-info strip stood down for it.
        frmFindInProject_Check( "bars: the Find bar was forced open", _
            gFind.bShowFindPanel = true )
        frmFindInProject_Check( "bars: the file-info strip stood down", _
            gFind.bShowInfoPanel = false )
    end if

    ' ---- the search engine, against an independent oracle ---------------------------------
    ' NOTE THE ORDER: everything from here on runs with the tab still OPEN, because the
    ' surface has to be alive for the canvas and render assertions to mean anything. Closing
    ' first left the tone probe answering -1 and the scrollbar sync never running, while every
    ' canvas assertion still passed -- they only need the row list, which is pure.
    ' THE PURE HALF IS BLIND TO WHETHER THE ENGINE FINDS ANYTHING. It only ever reaches the
    ' remap and window arithmetic, so stubbing the search out entirely leaves it fully green.
    ' What follows checks the real thing: it takes a phrase out of a real document's own text,
    ' counts the occurrences itself with plain string operations, and requires the engine to
    ' agree -- both on how many there are and on where each one is.
    dim as clsDocument ptr pTest = 0
    dim pWalk as clsDocument ptr = gApp.pDocList
    do until pWalk = 0
        if IsWindow( pWalk->hWindow(0) ) then pTest = pWalk: exit do
        pWalk = pWalk->pDocNext
    loop

    if pTest = 0 then
        print "  NOTE: no realized document -- the search-engine assertions are SKIPPED."
    else
        dim as string sText = pTest->GetText()

        ' Take the first run of >= 5 letters as the phrase, so this works against whatever
        ' file happens to be open rather than against a hardcoded needle.
        dim as string sPhrase
        dim as long nRun = 0, nRunStart = 0
        for i as long = 1 to len(sText)
            dim as long c = asc( mid(sText, i, 1) )
            dim as boolean bAlpha = ((c >= 65) andalso (c <= 90)) orelse ((c >= 97) andalso (c <= 122))
            if bAlpha then
                if nRun = 0 then nRunStart = i
                nRun += 1
                if nRun >= 5 then sPhrase = mid(sText, nRunStart, 5): exit for
            else
                nRun = 0
            end if
        next

        if len(sPhrase) = 0 then
            print "  NOTE: no usable phrase in the test document -- search assertions SKIPPED."
        else
            ' The oracle: count case-sensitively with instr, which shares no code with the
            ' engine (that goes through Scintilla's SCI_SEARCHINTARGET).
            dim as long nOracle = 0
            dim as long p = instr( sText, sPhrase )
            do while p > 0
                nOracle += 1
                p = instr( p + 1, sText, sPhrase )
            loop

            ' THE SEARCH IS SYNCHRONOUS ON THE UI THREAD. Timed and reported rather than
            ' assumed to be fast enough: this is the number that decides whether it has to
            ' move to a worker thread.
            dim as double t0 = timer
            FindProject_Search( sPhrase, true, false )
            dim as double nSearchMs = (timer - t0) * 1000
            print "  SEARCH: " & str(gApp.GetDocumentCount()) & " documents scanned, " & _
                  str(gFip.nMatchCount) & " matches in " & str(gFip.nGroupCount) & _
                  " files, " & str(cint(nSearchMs)) & " ms"

            ' Find this document's group.
            dim as long nGrp = -1
            for g as long = 0 to gFip.nGroupCount - 1
                if gFip.groups(g).pDoc = pTest then nGrp = g: exit for
            next

            frmFindInProject_Check( "engine: the test document produced a group", _
                nGrp <> -1, "phrase [" & sPhrase & "] oracle " & str(nOracle) )

            if nGrp <> -1 then
                frmFindInProject_Check( "engine: match count agrees with the oracle", _
                    gFip.groups(nGrp).nMatchCount = nOracle, _
                    str(gFip.groups(nGrp).nMatchCount) & " vs " & str(nOracle) )

                ' EVERY recorded offset must actually point at the phrase in the raw bytes.
                ' This is the assertion that would catch a position off by a BOM, by a line
                ' ending, or by a UTF-8 unit -- none of which a count can see.
                dim as long nBadPos = 0
                dim as long m0 = gFip.groups(nGrp).nMatchFirst
                for i as long = m0 to m0 + gFip.groups(nGrp).nMatchCount - 1
                    if mid( sText, gFip.matches(i).nStart + 1, len(sPhrase) ) <> sPhrase then
                        nBadPos += 1
                    end if
                next
                frmFindInProject_Check( "engine: every offset points at the phrase", _
                    nBadPos = 0, str(nBadPos) & " wrong of " & str(gFip.groups(nGrp).nMatchCount) )

                ' Structural contract of the coalescing: every match belongs to exactly one
                ' block, and blocks run in ascending line order without overlapping.
                dim as long nSumMatches = 0
                dim as long nOutOfOrder  = 0
                dim as long nPrevLast    = -1
                dim as long b0 = gFip.groups(nGrp).nBlockFirst
                for b as long = b0 to b0 + gFip.groups(nGrp).nBlockCount - 1
                    nSumMatches += gFip.blocks(b).nMatchCount
                    if gFip.blocks(b).nFirstLine <= nPrevLast then nOutOfOrder += 1
                    nPrevLast = gFip.blocks(b).nFirstLine + gFip.blocks(b).nLineCount - 1
                next
                frmFindInProject_Check( "engine: every match belongs to exactly one block", _
                    nSumMatches = gFip.groups(nGrp).nMatchCount, _
                    str(nSumMatches) & " vs " & str(gFip.groups(nGrp).nMatchCount) )
                frmFindInProject_Check( "engine: blocks ascend and never overlap", _
                    nOutOfOrder = 0, str(nOutOfOrder) & " out of order" )
                frmFindInProject_Check( "engine: every block shows at least the context window", _
                    gFip.blocks(b0).nLineCount >= 1, str(gFip.blocks(b0).nLineCount) )

                ' A FILE SHORTER THAN THE 5-LINE WINDOW must not produce a block asking for
                ' lines it does not have -- the excerpt view would show blank rows below the
                ' end of the document and the block would be taller than its content.
                dim as long nDocLines = pTest->GetLineCount()
                dim as long nOverrun = 0
                for b as long = 0 to gFip.nBlockCount - 1
                    if gFip.blocks(b).nGroup <> nGrp then continue for
                    if (gFip.blocks(b).nFirstLine + gFip.blocks(b).nLineCount) > nDocLines then
                        nOverrun += 1
                    end if
                    if gFip.blocks(b).nFirstLine < 0 then nOverrun += 1
                next
                frmFindInProject_Check( "shortfile: no block runs past the end of its document", _
                    nOverrun = 0, str(nOverrun) & " of " & str(gFip.groups(nGrp).nBlockCount) & _
                    " blocks, doc has " & str(nDocLines) & " lines" )
                if nDocLines < 5 then
                    print "  NOTE: the test document has " & str(nDocLines) & _
                          " lines -- the short-file path IS being exercised."
                end if

                ' The remap, driven through the REAL hook rather than the pure function: an
                ' insertion at the very start of the file must shift every match in it.
                dim as long nFirstStart = gFip.matches(m0).nStart
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                frmFindInProject_Check( "remap: the hook shifted a match by the insert length", _
                    gFip.matches(m0).nStart = nFirstStart + 7, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nFirstStart + 7) )

                ' ...and the suppression flag the replace loop will rely on really suppresses.
                dim as long nNow = gFip.matches(m0).nStart
                gFipSuppressRemap = true
                FindProject_OnDocumentModified( pTest, SC_MOD_INSERTTEXT, 0, 7, 0 )
                gFipSuppressRemap = false
                frmFindInProject_Check( "remap: gFipSuppressRemap blocks the hook", _
                    gFip.matches(m0).nStart = nNow, _
                    str(gFip.matches(m0).nStart) & " vs " & str(nNow) )
            end if

            ' ---- the virtual canvas ------------------------------------------------------
            ' Geometry is asserted by READING THE NUMBERS, never by eye.
            frmFindInProject_Refresh( true )

            frmFindInProject_Check( "canvas: one row per header plus one per block", _
                gFipRowCount = gFip.nGroupCount + gFip.nBlockCount, _
                str(gFipRowCount) & " vs " & str(gFip.nGroupCount + gFip.nBlockCount) )

            ' Rows must TILE the canvas in ascending order with no row starting before the
            ' previous one ended. This is what the binary search depends on.
            dim as long nBadOrder = 0
            dim as long nPrevEnd = 0
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nTop < nPrevEnd then nBadOrder += 1
                if gFipRows(i).nHeight <= 0 then nBadOrder += 1
                nPrevEnd = gFipRows(i).nTop + gFipRows(i).nHeight
            next
            frmFindInProject_Check( "canvas: rows ascend and every height is positive", _
                nBadOrder = 0, str(nBadOrder) & " bad of " & str(gFipRowCount) )

            frmFindInProject_Check( "canvas: the canvas is at least as tall as the last row", _
                gFipCanvasH >= nPrevEnd, str(gFipCanvasH) & " vs " & str(nPrevEnd) )

            ' A block's height must be exactly its line count times the line height plus the
            ' padding -- the identity the excerpt views will be sized against, so an error
            ' here would misplace every one of them.
            dim as long nBadHeight = 0
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nKind <> FIP_ROW_BLOCK then continue for
                dim as long nWant = FipRows_BlockHeight( gFip.blocks( gFipRows(i).nBlock ).nLineCount, _
                                                         gFipMetrics )
                if gFipRows(i).nHeight <> nWant then nBadHeight += 1
            next
            frmFindInProject_Check( "canvas: every block height matches the line-count formula", _
                nBadHeight = 0, str(nBadHeight) & " wrong" )

            ' The line height came from Scintilla, not from a guess -- so the painted
            ' placeholder and the real excerpt views agree about how tall a line is.
            frmFindInProject_Check( "canvas: the line height came from Scintilla", _
                gFipMetrics.nLineHeight > 0, str(gFipMetrics.nLineHeight) )

            ' COLLAPSING drops a group's blocks and shortens the canvas; expanding restores
            ' it exactly. An off-by-one in the rebuild would show up as a canvas that grows a
            ' little on every toggle.
            dim as long nRowsOpen = gFipRowCount
            dim as long nCanvasOpen = gFipCanvasH
            gFip.groups(nGrp).bCollapsed = true
            frmFindInProject_Refresh( false )
            frmFindInProject_Check( "collapse: the row count drops by that group's blocks", _
                gFipRowCount = nRowsOpen - gFip.groups(nGrp).nBlockCount, _
                str(gFipRowCount) & " vs " & str(nRowsOpen - gFip.groups(nGrp).nBlockCount) )
            frmFindInProject_Check( "collapse: the canvas got shorter", _
                gFipCanvasH < nCanvasOpen, str(gFipCanvasH) & " vs " & str(nCanvasOpen) )
            gFip.groups(nGrp).bCollapsed = false
            frmFindInProject_Refresh( false )
            frmFindInProject_Check( "collapse: expanding restores the canvas EXACTLY", _
                (gFipRowCount = nRowsOpen) andalso (gFipCanvasH = nCanvasOpen), _
                str(gFipRowCount) & "/" & str(gFipCanvasH) & " vs " & _
                str(nRowsOpen) & "/" & str(nCanvasOpen) )

            ' ---- the excerpt pool ----------------------------------------------------------
            dim as long nBound = 0
            dim as long nBadDoc = 0
            dim as long nBadLine = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = -1 then continue for
                nBound += 1
                dim as long gg = gFipRows( gFipPool(i).nRow ).nGroup
                dim as long bb = gFipRows( gFipPool(i).nRow ).nBlock
                ' Bound to the RIGHT document...
                if gFipPool(i).pSciDoc <> gFip.groups(gg).pSciDoc then nBadDoc += 1
                ' ...and pinned to the block's first line. Getting this wrong shows the wrong
                ' five lines, which looks like a search bug rather than a view bug.
                if SciExec( gFipPool(i).hWnd, SCI_GETFIRSTVISIBLELINE, 0, 0 ) <> _
                   gFip.blocks(bb).nFirstLine then nBadLine += 1
            next

            frmFindInProject_Check( "pool: at least one excerpt view is bound", _
                nBound > 0, str(nBound) & " bound of " & str(gFipPoolCount) )
            frmFindInProject_Check( "pool: every bound view holds its group's document", _
                nBadDoc = 0, str(nBadDoc) & " wrong" )
            frmFindInProject_Check( "pool: every bound view is pinned to its block's line", _
                nBadLine = 0, str(nBadLine) & " wrong" )
            frmFindInProject_Check( "pool: no more views exist than blocks on screen", _
                gFipPoolCount <= FIP_POOL_MAX, str(gFipPoolCount) )

            ' EVERY SLOT MUST BE SILENT. The modification event mask is per-view and defaults
            ' to every event, and each view bound to a document is one of its watchers -- so a
            ' pool left at the default fans a full-mask SCN_MODIFIED, plus every restyle, out
            ' of each slot on every keystroke typed in a file it happens to be showing. That
            ' is invisible except as "the editor got slow after I searched", which is exactly
            ' the kind of thing no other assertion here would ever catch.
            dim as long nNoisy = 0
            dim as long nMargins = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).hWnd = 0 then continue for
                if SciExec( gFipPool(i).hWnd, SCI_GETMODEVENTMASK, 0, 0 ) <> 0 then nNoisy += 1
                ' Breakpoints and folding have no meaning in a five-line excerpt, and
                ' suppressing the margins removes their click notifications outright.
                if SciExec( gFipPool(i).hWnd, SCI_GETMARGINWIDTHN, 1, 0 ) <> 0 then nMargins += 1
                if SciExec( gFipPool(i).hWnd, SCI_GETMARGINWIDTHN, 2, 0 ) <> 0 then nMargins += 1
            next
            frmFindInProject_Check( "pool: every view has its modification events silenced", _
                nNoisy = 0, str(nNoisy) & " noisy of " & str(gFipPoolCount) )
            frmFindInProject_Check( "pool: the symbol and fold margins are suppressed", _
                nMargins = 0, str(nMargins) & " margins still open" )

            ' A pooled view is a SECOND VIEW ONTO THE REAL DOCUMENT, not a copy. Proved by
            ' asking it for its document pointer and comparing against the document's own
            ' window -- if these ever differ, edits in an excerpt are going somewhere else.
            if nBound > 0 then
                dim as long nSlot = -1
                for i as long = 0 to gFipPoolCount - 1
                    if gFipPool(i).nRow <> -1 then nSlot = i: exit for
                next
                dim as long gg = gFipRows( gFipPool(nSlot).nRow ).nGroup
                dim as any ptr pViewDoc = cast( any ptr, _
                    SciExec( gFipPool(nSlot).hWnd, SCI_GETDOCPOINTER, 0, 0 ) )
                dim as any ptr pRealDoc = cast( any ptr, _
                    SciExec( gFip.groups(gg).pDoc->hWindow(0), SCI_GETDOCPOINTER, 0, 0 ) )
                frmFindInProject_Check( "pool: the view shares the document, it is not a copy", _
                    (pViewDoc <> 0) andalso (pViewDoc = pRealDoc) )

                ' The text the view reports must be the file's own text, which is the end-to-end
                ' proof that the binding worked.
                frmFindInProject_Check( "pool: the view sees the document's real length", _
                    SciExec( gFipPool(nSlot).hWnd, SCI_GETLENGTH, 0, 0 ) = _
                    SciExec( gFip.groups(gg).pDoc->hWindow(0), SCI_GETLENGTH, 0, 0 ) )
            end if

            ' SCROLLING MUST NOT REBIND A VIEW THAT IS STILL ON SCREEN. If it did, every tick
            ' would re-run SETDOCPOINTER and the full style push with it.
            dim as long nRowBefore = -1
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow <> -1 then nRowBefore = gFipPool(i).nRow: exit for
            next
            dim as any ptr pDocBefore = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = nRowBefore then pDocBefore = gFipPool(i).pSciDoc: exit for
            next
            FipPool_Sync()
            dim as boolean bStillThere = false
            for i as long = 0 to gFipPoolCount - 1
                if (gFipPool(i).nRow = nRowBefore) andalso (gFipPool(i).pSciDoc = pDocBefore) then
                    bStillThere = true: exit for
                end if
            next
            frmFindInProject_Check( "pool: a re-sync keeps an on-screen view on its row", _
                bStillThere orelse (nRowBefore = -1) )

            ' ---- does it actually DRAW? ----------------------------------------------------
            dim as long nTonesResults = FipSurface_CountRenderedTones()

            ' ---- collapse all / expand all --------------------------------------------------
            dim as long nRowsAllOpen = gFipRowCount
            frmFindInProject_SetAllCollapsed( true )
            frmFindInProject_Check( "foldall: everything collapsed leaves only headers", _
                gFipRowCount = gFip.nGroupCount, _
                str(gFipRowCount) & " vs " & str(gFip.nGroupCount) )
            frmFindInProject_Check( "foldall: the state reads back as all-collapsed", _
                frmFindInProject_AllCollapsed() = true )
            ' Every excerpt view must be parked when there is nothing to show -- a slot left
            ' visible over a collapsed group would float above the headers.
            dim as long nStillShowing = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow <> -1 then nStillShowing += 1
            next
            frmFindInProject_Check( "foldall: no excerpt view is left showing", _
                nStillShowing = 0, str(nStillShowing) )

            frmFindInProject_SetAllCollapsed( false )
            frmFindInProject_Check( "foldall: expanding restores every row", _
                gFipRowCount = nRowsAllOpen, _
                str(gFipRowCount) & " vs " & str(nRowsAllOpen) )
            frmFindInProject_Check( "foldall: the state reads back as not-all-collapsed", _
                frmFindInProject_AllCollapsed() = false )

            ' ---- the excerpt views do not scroll --------------------------------------------
            ' Moving the caret to the end of the document must NOT slide the view off its
            ' block: an excerpt is a fixed window onto a fixed run of lines, and you edit
            ' within the lines you can see.
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = -1 then continue for
                dim as long bb = gFipRows( gFipPool(i).nRow ).nBlock
                dim as long nPinned = gFip.blocks(bb).nFirstLine
                dim as long nLastLine = nPinned + gFip.blocks(bb).nLineCount - 1
                SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_END, 0 )
                SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_NEXT, 0 )
                frmFindInProject_Check( "noscroll: the view stayed pinned after caret keys", _
                    SciExec( gFipPool(i).hWnd, SCI_GETFIRSTVISIBLELINE, 0, 0 ) = nPinned, _
                    str(SciExec( gFipPool(i).hWnd, SCI_GETFIRSTVISIBLELINE, 0, 0 )) & _
                    " vs " & str(nPinned) )

                ' LEAVING THE BLOCK IS A NO-OP. Park the caret on the first line, press Up,
                ' and NOTHING may move -- not the line, and not the column either. Clamping
                ' after the fact (which is what this replaced) left the caret at column 0,
                ' so the position, not just the line, is what has to be compared.
                SciExec( gFipPool(i).hWnd, SCI_GOTOPOS, _
                         SciExec( gFipPool(i).hWnd, SCI_GETLINEENDPOSITION, nPinned, 0 ), 0 )
                dim as long nPosBefore = SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )
                SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_UP, 0 )
                frmFindInProject_Check( "noscroll: Up on the first line does nothing at all", _
                    SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 ) = nPosBefore, _
                    str(SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )) & _
                    " vs " & str(nPosBefore) )

                SciExec( gFipPool(i).hWnd, SCI_GOTOPOS, _
                         SciExec( gFipPool(i).hWnd, SCI_POSITIONFROMLINE, nLastLine, 0 ), 0 )
                nPosBefore = SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )
                SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_DOWN, 0 )
                frmFindInProject_Check( "noscroll: Down on the last line does nothing at all", _
                    SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 ) = nPosBefore, _
                    str(SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )) & _
                    " vs " & str(nPosBefore) )

                ' ...but a move INSIDE the block is untouched. Without this, "does nothing"
                ' would also pass if the subclass ate every arrow key.
                if gFip.blocks(bb).nLineCount > 1 then
                    SciExec( gFipPool(i).hWnd, SCI_GOTOPOS, _
                             SciExec( gFipPool(i).hWnd, SCI_POSITIONFROMLINE, nPinned, 0 ), 0 )
                    nPosBefore = SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )
                    SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_DOWN, 0 )
                    frmFindInProject_Check( "noscroll: Down INSIDE the block still moves", _
                        SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 ) <> nPosBefore, _
                        str(SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )) )
                end if
                exit for
            next

            ' ---- navigation and replace ----------------------------------------------------
            ' Next/prev walk LIVE matches and wrap. Enter in the find box funnels to
            ' IDM_FINDNEXT, so this is the same path typing takes.
            dim as long nFirstLive = FindProject_NextLive( -1, true )
            frmFindInProject_Check( "nav: there is a first live match", _
                nFirstLive >= 0, str(nFirstLive) )

            if nFirstLive >= 0 then
                dim as long nSecond = FindProject_NextLive( nFirstLive, true )
                if gFip.nLiveMatches > 1 then
                    frmFindInProject_Check( "nav: forward moves to a different match", _
                        (nSecond >= 0) andalso (nSecond <> nFirstLive), str(nSecond) )
                else
                    ' WITH A SINGLE MATCH, "next" WRAPS TO ITSELF -- which is right, and which
                    ' the assertion above cannot express. It was written as an unconditional
                    ' "moves to a different match" and only passed because every test run so
                    ' far happened to have several; a 3-line file with one hit failed it.
                    frmFindInProject_Check( "nav: a lone match wraps to itself", _
                        nSecond = nFirstLive, str(nSecond) & " vs " & str(nFirstLive) )
                end if
                ' Going back from the second must land on the first again.
                frmFindInProject_Check( "nav: backward is the inverse of forward", _
                    FindProject_NextLive( nSecond, false ) = nFirstLive, _
                    str(FindProject_NextLive( nSecond, false )) )
                ' Every match belongs to a block, or it could never be revealed.
                frmFindInProject_Check( "nav: the match maps to a block", _
                    FindProject_BlockOfMatch( nFirstLive ) >= 0, _
                    str(FindProject_BlockOfMatch( nFirstLive )) )
            end if

            ' A dead match is skipped rather than returned.
            if (nFirstLive >= 0) andalso (gFip.nLiveMatches > 1) then
                gFip.matches(nFirstLive).bDead = true
                frmFindInProject_Check( "nav: a dead match is skipped", _
                    FindProject_NextLive( -1, true ) <> nFirstLive )
                gFip.matches(nFirstLive).bDead = false
            end if

            ' REPLACE. Done last of the model tests because it edits the real buffer -- the
            ' document is left dirty and is NEVER saved, which is the whole contract.
            ' RE-SEARCH FIRST. The remap section above fired two SYNTHETIC SCN_MODIFIED
            ' notifications at the model describing insertions that never happened to the real
            ' buffer, so every stored offset is now several bytes adrift. Replacing against
            ' that model writes to the wrong places -- which is precisely what the
            ' "no occurrence survives" assertion below reported (22 left, on a real project)
            ' and what the byte-count assertion could not see.
            FindProject_Search( sPhrase, true, false )
            nGrp = -1
            for g as long = 0 to gFip.nGroupCount - 1
                if gFip.groups(g).pDoc = pTest then nGrp = g: exit for
            next

            dim as long nLiveBefore = gFip.nLiveMatches
            dim as long nLenBefore  = SciExec( pTest->hWindow(0), SCI_GETLENGTH, 0, 0 )
            ' THIS FILE's share of the replacements. Not the project-wide total: the length
            ' check below measures ONE document, and comparing it against every file's matches
            ' passed only because a single-file test run made the two numbers identical. A run
            ' against a real 123-document project reported 94458 vs 95426.
            dim as long nGroupLive = 0
            for i as long = gFip.groups(nGrp).nMatchFirst to _
                            gFip.groups(nGrp).nMatchFirst + gFip.groups(nGrp).nMatchCount - 1
                if gFip.matches(i).bDead = false then nGroupLive += 1
            next
            ' All punctuation, and two bytes longer than the phrase. Both properties matter:
            ' the length change makes later offsets drift if the replacement order is wrong,
            ' and a replacement that cannot CONTAIN the phrase is what lets the count below
            ' detect that drift.
            dim as string sRepl = string( len(sPhrase) + 2, asc("~") )
            dim as long nReplaced = FindProject_ReplaceAll( sRepl )

            frmFindInProject_Check( "replace: every live match was replaced", _
                nReplaced = nLiveBefore, str(nReplaced) & " vs " & str(nLiveBefore) )
            frmFindInProject_Check( "replace: nothing is left matching the old phrase", _
                gFip.nLiveMatches = 0, str(gFip.nLiveMatches) )
            frmFindInProject_Check( "replace: this document grew by 2 bytes per match in it", _
                SciExec( pTest->hWindow(0), SCI_GETLENGTH, 0, 0 ) = nLenBefore + (nGroupLive * 2), _
                str(SciExec( pTest->hWindow(0), SCI_GETLENGTH, 0, 0 )) & " vs " & _
                str(nLenBefore + (nGroupLive * 2)) )

            ' THE ONE THAT CATCHES MISPLACEMENT. The length check above cannot: each
            ' REPLACETARGET writes the same number of bytes wherever it lands, so the total
            ' delta is identical whether the offsets were right or drifting. Measured -- with
            ' the replace loop running forwards instead of backwards, every assertion above
            ' stayed green. Counting the text is what actually proves the right bytes moved.
            dim as string sAfter = pTest->GetText()
            dim as long nOldLeft = 0, nNewFound = 0, p2 = instr( sAfter, sPhrase )
            do while p2 > 0
                nOldLeft += 1
                p2 = instr( p2 + 1, sAfter, sPhrase )
            loop
            p2 = instr( sAfter, sRepl )
            do while p2 > 0
                nNewFound += 1
                p2 = instr( p2 + 1, sAfter, sRepl )
            loop
            frmFindInProject_Check( "replace: no occurrence of the old phrase survives", _
                nOldLeft = 0, str(nOldLeft) & " left" )
            frmFindInProject_Check( "replace: the replacement appears once per match", _
                nNewFound = nGroupLive, str(nNewFound) & " vs " & str(nGroupLive) )
            frmFindInProject_Check( "replace: the document is left DIRTY, not saved", _
                SciExec( pTest->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 )

            ' ...and one undo puts the whole file back, because the replacements were wrapped
            ' in ONE undo action per file rather than one per match.
            SciExec( pTest->hWindow(0), SCI_UNDO, 0, 0 )
            frmFindInProject_Check( "replace: ONE undo reverses the whole file", _
                SciExec( pTest->hWindow(0), SCI_GETLENGTH, 0, 0 ) = nLenBefore, _
                str(SciExec( pTest->hWindow(0), SCI_GETLENGTH, 0, 0 )) & " vs " & str(nLenBefore) )
            ' Leave the buffer exactly as it was found.
            SciExec( pTest->hWindow(0), SCI_SETSAVEPOINT, 0, 0 )

            ' ---- the close funnel ----------------------------------------------------------
            ' A document whose Scintilla windows are going away takes its results with it, and
            ' every excerpt view bound to it must let go BEFORE the model releases its
            ' reference. Driven through the real notifier, which is what
            ' clsDocument.DestroyScintillaWindows calls.
            dim as any ptr pDocSci = gFip.groups(nGrp).pSciDoc
            dim as long nBoundToIt = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).pSciDoc = pDocSci then nBoundToIt += 1
            next

            FindProject_OnDocumentClosing( pTest )

            frmFindInProject_Check( "closing: the group was emptied", _
                (gFip.groups(nGrp).pDoc = 0) andalso (gFip.groups(nGrp).pSciDoc = 0) )
            dim as long nStillBound = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).pSciDoc = pDocSci then nStillBound += 1
            next
            frmFindInProject_Check( "closing: no excerpt view is left on the released document", _
                nStillBound = 0, str(nStillBound) & " of " & str(nBoundToIt) & " still bound" )
            ' The file drops out of the list entirely -- its excerpts had nothing left to show.
            dim as long nHeadersLeft = 0
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nKind = FIP_ROW_HEADER then
                    if gFipRows(i).nGroup = nGrp then nHeadersLeft += 1
                end if
            next
            frmFindInProject_Check( "closing: the file is gone from the results", _
                nHeadersLeft = 0, str(nHeadersLeft) )

            ' Leave no results behind: this suite runs at startup, not on the user's command.
            FindProject_Clear()
            frmFindInProject_Refresh( true )

            dim as long nTonesEmpty = FipSurface_CountRenderedTones()
            print "  TONES: with results = " & str(nTonesResults) & _
                  ", empty state = " & str(nTonesEmpty) & " (cap 64)"

            ' Ink reached the surface at all. A render that draws nothing, or floods the whole
            ' client with one colour, reports 1 here -- and satisfies every geometry
            ' assertion above.
            frmFindInProject_Check( "render: the empty state draws something", _
                nTonesEmpty > 1, str(nTonesEmpty) )

            ' THE DISCRIMINATING ONE, and it is RELATIVE rather than an absolute floor
            ' inherited from a sibling: how many tones a healthy render produces depends on
            ' the theme, the DPI and how much of the canvas is on screen, but a surface
            ' showing headers and excerpts must always be busier than one showing a single
            ' line of grey text. Measured here at 64 (capped) against 36.
            frmFindInProject_Check( "render: results draw strictly more than the empty state", _
                nTonesResults > nTonesEmpty, _
                str(nTonesResults) & " vs " & str(nTonesEmpty) )
        end if
    end if

    ' ---- close it -------------------------------------------------------------------------
    ' The real user path: the tab's X routes through CloseTab -> OnCommand_FileClose.
    if idx <> -1 then gTTabCtl.CloseTab( idx )

    frmFindInProject_Check( "close: the Find tab is gone", _
        gTTabCtl.FindTabIndex() = -1, str(gTTabCtl.FindTabIndex()) )
    frmFindInProject_Check( "close: the results window was destroyed", _
        IsWindow( HWND_FRMFINDINPROJECT ) = 0 )
    frmFindInProject_Check( "close: the tab count is back to baseline", _
        gTTabCtl.GetItemCount() = nTabsBefore, _
        str(gTTabCtl.GetItemCount()) & " vs " & str(nTabsBefore) )
    frmFindInProject_Check( "close: the file-info strip came back", _
        gFind.bShowInfoPanel = true )
    frmFindInProject_Check( "close: the Find bar was closed again", _
        gFind.bShowFindPanel = false )

    ' The editors have to come BACK. Closing the tab that was covering them is the half of
    ' the DisplayScintilla fix that a "did they hide" assertion cannot see.
    frmFindInProject_Check( "close: the document editors are visible again", _
        frmFindInProject_CountVisibleEditors() = nVisBefore, _
        str(frmFindInProject_CountVisibleEditors()) & " vs " & str(nVisBefore) )
    ' The invariant that actually matters: whatever is current now is a REAL DOCUMENT TAB.
    '
    ' This used to compare against the tab that was current before the Find tab opened, which
    ' was only ever true by accident -- the bare-SetFocusTab section above deliberately moves
    ' the selection, so with a single file open it happened to land back on 0 and with a real
    ' project it did not. An assertion that passes because the test has one document is worse
    ' than no assertion.
    if nTabsBefore > 0 then
        frmFindInProject_Check( "close: a real document tab is current again", _
            (gTTabCtl.GetCurSel() >= 0) andalso (gTTabCtl.GetActiveDocumentPtr() <> 0), _
            str(gTTabCtl.GetCurSel()) )
    end if

    print "--- TIKO_FINDPROJ_SELFTEST: " & str(gFipTestPass) & " passed, " & _
          str(gFipTestFail) & " failed ---"
end sub


' ========================================================================================
' The Find / Replace bar, retargeted.
' ========================================================================================
dim shared gFipCurrent   as long = -1     ' flat match index the user is sitting on
dim shared gFipLastQuery as DWSTRING      ' what the last search actually ran for


' Push the "n of m" the Find bar paints, and rename the tab to the phrase.
private sub FipBar_UpdateStatus()
    dim as long nOrdinal = 0
    if gFipCurrent >= 0 then
        for i as long = 0 to gFipCurrent
            if gFip.matches(i).bDead = false then nOrdinal += 1
        next
    end if
    gFind.wszResults = str(nOrdinal) & "/" & str(gFip.nLiveMatches)
    gFind.foundCount = gFip.nLiveMatches
    if IsWindowVisible( HWND_FRMMAIN_FIND ) then AfxRedrawWindow( HWND_FRMMAIN_FIND )

    ' The tab takes the phrase's name once there is one. SetTabText leaves a document-less
    ' tab alone, which is what lets this stick.
    dim as long idx = gTTabCtl.FindTabIndex()
    if idx <> -1 then
        ' Once there is a phrase the tab IS the phrase -- no prefix. The magnifier the tab
        ' painter puts to its left is what says this is a search.
        dim as DWSTRING wszCap = L(206, "Find in Project")
        if len(gFipLastQuery) then wszCap = gFipLastQuery
        ' A long phrase is truncated rather than allowed to take over the whole bar. Measured
        ' in the tab bar's own font, and the "..." is added here rather than left to a paint
        ' flag because PsTabBar sizes each tab from its caption -- an ellipsis applied only at
        ' paint time would leave the tab as wide as the untruncated text.
        dim as long nMaxChars = 32
        if len(wszCap) > nMaxChars then wszCap = left(wszCap, nMaxChars - 1) & "..."

        if PsTabBar_GetText( gTTabCtl.hWindow, idx ) <> wszCap then
            PsTabBar_SetText( gTTabCtl.hWindow, idx, wszCap )
        end if

        ' The tab has to be WIDE ENOUGH FOR THE PHRASE PLUS THE MAGNIFIER. PsTabBar sizes a
        ' tab from its caption alone, and the icon is drawn inside the caption rect -- so
        ' without this floor the icon eats the text's room and a phrase like "include" comes
        ' out as "iclud". The floor is what the control would have chosen, plus the glyph.
        ' Measured on a plain DC, NOT through PsBufferPaint: BeginDoubleBuffer calls
        ' BeginPaint, and doing that outside a WM_PAINT would validate the tab bar's update
        ' region behind its back.
        dim as long nWant = 0
        dim as HDC hdcM = GetDC( gTTabCtl.hWindow )
        if hdcM then
            dim as HGDIOBJ hOldFont = SelectObject( hdcM, ghFont(GUIFONT_10) )
            dim as SIZE sz
            GetTextExtentPoint32( hdcM, wszCap.sptr, len(wszCap), @sz )
            SelectObject( hdcM, hOldFont )
            ReleaseDC( gTTabCtl.hWindow, hdcM )
            ' text + the magnifier + what the control reserves for its own close/padding.
            nWant = sz.cx + AfxScaleX(16) + AfxScaleX(56)
        end if
        if nWant > 0 then
            if PsTabBar_GetTabMinWidth( gTTabCtl.hWindow, idx ) <> nWant then
                PsTabBar_SetTabMinWidth( gTTabCtl.hWindow, idx, nWant )
            end if
        end if
    end if
end sub


' Bring a match on screen and select it in whichever excerpt view is showing it.
private sub FipBar_RevealCurrent()
    if gFipCurrent < 0 then exit sub
    dim as long b = FindProject_BlockOfMatch( gFipCurrent )
    if b = -1 then exit sub

    ' A collapsed group has no row for the block, so open it first.
    dim as long g = gFip.blocks(b).nGroup
    if gFip.groups(g).bCollapsed then
        gFip.groups(g).bCollapsed = false
        frmFindInProject_Refresh( false )
    end if

    dim as long nRow = -1
    for r as long = 0 to gFipRowCount - 1
        if (gFipRows(r).nKind = FIP_ROW_BLOCK) andalso (gFipRows(r).nBlock = b) then nRow = r: exit for
    next
    if nRow = -1 then exit sub

    ' Scroll only as far as needed: if the block is already fully on screen, leave the view
    ' where it is rather than yanking it to the top on every Enter.
    dim as RECT rcClient
    GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rcClient )
    dim as long nTop = gFipRows(nRow).nTop
    dim as long nBot = nTop + gFipRows(nRow).nHeight
    dim as long nPage = rcClient.bottom - rcClient.top
    if nTop < gFipScrollTop then
        gFipScrollTop = nTop
    elseif nBot > (gFipScrollTop + nPage) then
        gFipScrollTop = nBot - nPage
    end if
    FipSurface_SyncScrollBar()
    PsVScrollBar_SetPos( HWND_FRMFINDINPROJECT_VSCROLL, gFipScrollTop )
    FipPool_Sync()
    AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )

    ' Select the match inside its excerpt view, so "next" is visible rather than merely
    ' scrolled to. The pin is re-asserted afterwards because setting a selection scrolls the
    ' caret into view and would otherwise slide the block off its lines.
    ' THE EDITING CARET MOVES WITH THE NAVIGATION, not just the scroll position: after F3 the
    ' match is where you are, so typing continues there.
    '
    ' Focus follows only when it was already in an excerpt. Pressing Enter repeatedly in the
    ' find box has to keep working, and stealing focus out of the box on every Enter would
    ' break that -- so a keystroke made from the box moves the caret without taking the box's
    ' focus away, and one made from an excerpt keeps you in the excerpts.
    dim as boolean bFocusInPool = false
    dim as HWND hFocus = GetFocus()
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd = hFocus then bFocusInPool = true: exit for
    next

    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow <> nRow then continue for
        SciExec( gFipPool(i).hWnd, SCI_SETSEL, gFip.matches(gFipCurrent).nStart, _
                 gFip.matches(gFipCurrent).nStart + gFip.matches(gFipCurrent).nLen )
        SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, gFip.blocks(b).nFirstLine, 0 )
        if bFocusInPool then SetFocus( gFipPool(i).hWnd )
        exit for
    next
end sub


' If the caret is sitting in an excerpt, move gFipCurrent to the match nearest ahead of it in
' that file. Without this, clicking into a result and pressing F3 would resume from wherever
' the previous Enter had got to, which is somewhere else entirely.
private sub FipBar_SyncCurrentToCaret()
    dim as HWND hFocus = GetFocus()
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd <> hFocus then continue for
        if gFipPool(i).nRow = -1 then exit for
        if (gFipPool(i).nRow < 0) orelse (gFipPool(i).nRow >= gFipRowCount) then exit for

        dim as long g = gFipRows( gFipPool(i).nRow ).nGroup
        dim as long nPos = SciExec( hFocus, SCI_GETCURRENTPOS, 0, 0 )

        ' The last match at or before the caret, so the NEXT step lands on the one after it.
        dim as long m0 = gFip.groups(g).nMatchFirst
        dim as long nBest = -1
        for m as long = m0 to m0 + gFip.groups(g).nMatchCount - 1
            if gFip.matches(m).bDead then continue for
            if gFip.matches(m).nStart <= nPos then nBest = m
        next
        ' Caret before every match in the file: step from just before the first one.
        if nBest = -1 then nBest = m0 - 1
        gFipCurrent = nBest
        exit for
    next
end sub


' Run the search for whatever is in the find box.
private sub FipBar_RunSearch()
    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    dim as DWSTRING wszPhrase = AfxGetWindowText( hCtl )
    gFind.txtFind = wszPhrase

    if len(wszPhrase) = 0 then
        FindProject_Clear()
        gFipLastQuery = ""
        gFipCurrent = -1
        frmFindInProject_Refresh( true )
        FipBar_UpdateStatus()
        exit sub
    end if

    dim as double t0 = timer
    FindProject_Search( wszPhrase, cbool(gFind.nMatchCase), cbool(gFind.nWholeWord) )
    dim as double nMs = (timer - t0) * 1000

    gFipLastQuery = wszPhrase
    gFipCurrent = FindProject_NextLive( -1, true )
    frmFindInProject_Refresh( true )
    FipBar_UpdateStatus()
    FipBar_RevealCurrent()

    ' THE SEARCH IS SYNCHRONOUS ON THE UI THREAD. This is the number that decides whether it
    ' has to move to a worker -- reported rather than guessed at.
    if environ("TIKO_FINDPROJ_TRACE") = "1" then
        dim as string sMsg = "FINDPROJ: " & str(gFip.nMatchCount) & " matches in " & _
                             str(gFip.nGroupCount) & " files, " & _
                             str(gApp.GetDocumentCount()) & " documents scanned, " & _
                             str(cint(nMs)) & " ms"
        print sMsg
    end if
end sub


function frmFindInProject_BarAction( byval id as long ) as boolean
    if frmFindInProject_IsActive() = false then return false

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    dim as DWSTRING wszPhrase = AfxGetWindowText( hCtl )

    select case id
        case IDM_FINDNEXT, IDM_FINDNEXTACCEL, IDM_FINDPREV, IDM_FINDPREVACCEL
            dim as boolean bForward = (id = IDM_FINDNEXT) orelse (id = IDM_FINDNEXTACCEL)
            ' A changed phrase means a new search; the same phrase means step through what is
            ' already there. That is what makes Enter both "search" and "next".
            if (wszPhrase <> gFipLastQuery) orelse (gFip.bValid = false) then
                FipBar_RunSearch()
            else
                ' START FROM THE CARET, not from wherever the last Enter left off. If the
                ' user has clicked into an excerpt, that is where they are, and F3 has to
                ' continue from there rather than jumping back to the stored position.
                FipBar_SyncCurrentToCaret()
                dim as long n = FindProject_NextLive( gFipCurrent, bForward )
                if n <> -1 then
                    gFipCurrent = n
                    FipBar_UpdateStatus()
                    FipBar_RevealCurrent()
                end if
            end if
            return true

        case IDM_REPLACENEXT, IDM_REPLACEPREV
            dim as DWSTRING wszRep = AfxGetWindowText( _
                    GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE ) )
            if gFipCurrent >= 0 then
                FindProject_ReplaceOne( gFipCurrent, wszRep )
                ' The replaced match is dead now; move to the next live one.
                gFipCurrent = FindProject_NextLive( gFipCurrent, true )
                frmFindInProject_Refresh( false )
                FipBar_UpdateStatus()
                FipBar_RevealCurrent()
            end if
            return true

        case IDM_REPLACEALL
            dim as DWSTRING wszRep = AfxGetWindowText( _
                    GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE ) )
            FindProject_ReplaceAll( wszRep )
            gFipCurrent = -1
            frmFindInProject_Refresh( false )
            FipBar_UpdateStatus()
            return true
    end select

    return false
end function


' ========================================================================================
' frmFindInProject_Show
'
' Reached from the Search menu, the Ctrl+Shift+F accelerator and the Explorer panel's
' magnifier icon. There is at most ONE Find in Project tab: a second invocation re-selects
' the existing one rather than stacking another.
' ========================================================================================
function frmFindInProject_Show() as LRESULT
    frmFindInProject_CreateWindow()

    dim as long idx = gTTabCtl.FindTabIndex()

    if idx = -1 then
        ' TAB_FINDINPROJECT goes into the itemData slot that normally holds a clsDocument
        ' ptr. AddTab calls SetTabText, which leaves the caption alone for a tab with no
        ' document -- which is what lets the caption below stick.
        idx = PsTabBar_AddTab( gTTabCtl.hWindow, L(206, "Find in Project"), TAB_FINDINPROJECT )
        if idx = -1 then return 0
    end if

    gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
    gTTabCtl.SetFocusTab( idx )

    ' Adopt whatever the model holds. After SetFocusTab, so the surface has its real size and
    ' the scrollbar's page height is the one it will actually have.
    frmFindInProject_Refresh( true )

    function = 0
end function
