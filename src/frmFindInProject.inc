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
' Which group's header the cursor is over, or -1. Hovering one offers "Open File" too --
' without the Alt+Enter hint, which only means anything while an excerpt has the caret.
dim shared gFipHotGroup   as long = -1
' Where that affordance was last painted, so a click can be hit-tested against it.
dim shared gFipOpenRect   as RECT
' Set while the subclass is post-processing a message, so that the SciExec it makes -- which
' is a SendMessage, and so re-enters this same subclass -- cannot recurse without bound.
dim shared gFipPinning    as boolean
' Where the caret was when an excerpt last lost focus. Remembered by BLOCK, not by slot or
' row: slots are recycled and rows are rebuilt, but a block is a stable identity for as long
' as the result set lives. -1 when the caret has never been in an excerpt.
dim shared gFipLastEditBlock as long = -1
dim shared gFipLastEditPos   as long = 0
' The Find bar's navigation state. Declared up here rather than beside the bar code because
' the window procedure below resets both when the tab is destroyed.
dim shared gFipCurrent   as long = -1     ' flat match index the user is sitting on
dim shared gFipLastQuery as DWSTRING      ' what the last search actually ran for


' ========================================================================================
' The excerpt pool.
' ========================================================================================

' Open the focused excerpt's file in a tab of its own, at the line the caret is on. The
' excerpt is a view onto the real document, so this is a plain tab switch -- there is nothing
' to load and nothing to save first.
private sub FipPool_OpenGroupFile( byval g as long )
    if (g < 0) orelse (g >= gFip.nGroupCount) then exit sub

    dim as clsDocument ptr pDoc = gFip.groups(g).pDoc
    if pDoc = 0 then exit sub

    ' Where the caret sits in whichever excerpt of this file has focus, so the file opens
    ' where the user was looking. -1 when the click came from the header instead, in which
    ' case the document keeps whatever position it already had.
    dim as long nPos = -1
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for
        if gFipRows( gFipPool(i).nRow ).nGroup <> g then continue for
        if GetFocus() = gFipPool(i).hWnd then
            nPos = SciExec( gFipPool(i).hWnd, SCI_GETCURRENTPOS, 0, 0 )
            exit for
        end if
    next

    ' A header click with no focused excerpt still deserves to land somewhere useful: the
    ' file's FIRST match, which is what the user was looking at in the list.
    if nPos = -1 then
        dim as long m0 = gFip.groups(g).nMatchFirst
        for m as long = m0 to m0 + gFip.groups(g).nMatchCount - 1
            if gFip.matches(m).bDead = false then nPos = gFip.matches(m).nStart: exit for
        next
    end if

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


' Alt+Enter's target: the group whose excerpt currently has the caret.
private sub FipPool_OpenFocusedFile()
    FipPool_OpenGroupFile( gFipFocusGroup )
end sub


' (FipPool_KeyWouldLeaveBlock is gone. It answered "would this keystroke take the caret out of
'  the excerpt's own lines?" so that Up on the first line and Down on the last could be
'  swallowed before Scintilla saw them. That is exactly the confinement this refactor removes:
'  an excerpt is a scrolling view onto the whole file now, so every navigation key is let
'  through and the view follows the caret. See FIP_VIEW_LINES.)


' Subclass on every slot, for one reason: Scintilla consumes WM_MOUSEWHEEL itself, so a
' gesture made with the pointer over an excerpt would scroll THAT VIEW off its pinned line
' instead of scrolling the results list. Routed into the bar, which owns the arithmetic.
private function FipPool_SciProc( byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case WM_MOUSEWHEEL
            ' STILL ROUTED TO THE LIST, even though the view itself can scroll now. A wheel
            ' notch inside a five-line window would move it by most of its own height, and the
            ' gesture the user made was over the results list -- keyboard navigation is what
            ' this refactor opened up, not the wheel.
            PsVScrollBar_HandleWheelDelta( HWND_FRMFINDINPROJECT_VSCROLL, cast(short, hiword(wParam)) )
            return 0

        case WM_SETFOCUS, WM_KILLFOCUS
            ' Which group owns the caret decides which header shows "Open File".
            dim as long g = -1
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).hWnd <> hwnd then continue for
                if gFipPool(i).nRow = -1 then exit for
                if uMsg = WM_SETFOCUS then
                    g = gFipRows( gFipPool(i).nRow ).nGroup
                else
                    ' Losing focus: take one last reading. The continuous record in the
                    ' post-processing below is the reliable one; this only narrows the gap
                    ' between the final keystroke and the focus actually going.
                    gFipLastEditBlock = gFipRows( gFipPool(i).nRow ).nBlock
                    gFipLastEditPos   = SciExec( hwnd, SCI_GETCURRENTPOS, 0, 0 )
                end if
                exit for
            next
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

            ' EVERY OTHER KEY IS LET THROUGH. Up, Down, PgUp, PgDn, Ctrl+Home and Ctrl+End
            ' used to be swallowed at the edges of the block; they are Scintilla's now, and
            ' the view scrolls with the caret like any other editor.

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

    ' RE-ENTRANCY GUARD, AND IT IS STILL NOT OPTIONAL EVEN THOUGH THE PINNING IT WAS WRITTEN
    ' FOR IS GONE. SciExec is a plain SendMessage (modDeclares.bi), so the SCI_GETCURRENTPOS
    ' below re-enters this very subclass, whose post-processing asks for the position again --
    ' unbounded recursion and a stack overflow, exactly as the pin's own SCI_SETFIRSTVISIBLELINE
    ' used to produce. Do not remove it along with the pinning.
    if gFipPinning then return lr
    gFipPinning = true

    ' REMEMBER WHERE THE CARET IS, on every message this view handles while it has focus.
    ' Recording it only on WM_KILLFOCUS was too fragile: a tab switch hides this window's
    ' ancestors, and whether that produces a kill-focus this subclass actually sees is not
    ' something to depend on. Updating it continuously costs one comparison and cannot miss.
    '
    ' This is ALL that is left here. The view is no longer re-scrolled to its block, the caret
    ' is no longer clamped to the block's lines, and the horizontal offset is no longer reset
    ' -- see FIP_VIEW_LINES.
    if GetFocus() = hwnd then
        for i as long = 0 to gFipPoolCount - 1
            if gFipPool(i).hWnd <> hwnd then continue for
            ' Bounds, not faith: this runs on EVERY message a slot receives, including ones
            ' that arrive while the row list is being rebuilt underneath it.
            if (gFipPool(i).nRow < 0) orelse (gFipPool(i).nRow >= gFipRowCount) then exit for
            dim as long b = gFipRows( gFipPool(i).nRow ).nBlock
            if (b < 0) orelse (b >= gFip.nBlockCount) then exit for
            gFipLastEditBlock = b
            gFipLastEditPos   = SciExec( hwnd, SCI_GETCURRENTPOS, 0, 0 )
            exit for
        next
    end if

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
        ' PHASE 7d: a tikoSciHost. FORCED INTO THE SAME STEP as clsDocument's two
        ' windows, and not by tidiness -- FindProject_BindSlot takes a Scintilla
        ' Document* out of the main editor and SCI_SETDOCPOINTERs it into one of
        ' these. A document from the vendored fork cannot be handed to the stock
        ' Scintilla DLL: different code, different vtables, different heap. It
        ' would not fail loudly, it would corrupt.
        dim as HWND h = SciHost_Create( HWND_FRMFINDINPROJECT_SURFACE, _
                                        IDC_SCINTILLA_EXCERPT + i, _
                                        WS_CHILD or WS_CLIPSIBLINGS )
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
        ' HORIZONTAL PANNING IS ON, and the scrollbar is still off. The caret can now be
        ' driven off the right edge of a long line, and with tracking suppressed Scintilla
        ' would have nowhere to pan to -- the caret would simply disappear. Tracking widens
        ' the scroll width to the longest line it has styled; End on a long line then pans.
        SciExec( h, SCI_SETSCROLLWIDTHTRACKING, 1, 0 )

        gFipOldSciProc = cast( WNDPROC, SetWindowLongPtr( h, GWLP_WNDPROC, _
                                                          cast(LONG_PTR, @FipPool_SciProc) ) )

        gFipPool(i).hWnd     = h
        gFipPool(i).pSciView = SciHost_DirectPointer( h )
        gFipPool(i).pSciDoc  = 0
        gFipPool(i).nRow     = -1
        gFipPoolCount = i + 1
    next
end sub


' Bind a slot to a block row and place it.
private sub FipPool_Bind( byval i as long, byval nRow as long, byref rcView as RECT )
    dim as boolean bNewRow = cbool( gFipPool(i).nRow <> nRow )
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
        '
        ' (Margin 1 used to be kept clear for a dirty-line stripe the surface painted beside
        '  this window. The dirty signal is a circle on the file's header now, so both these
        '  margins are suppressed for the same plain reason: an excerpt has no room for, and
        '  no use for, breakpoints or folding.)
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 1, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SETMARGINWIDTHN, 2, 0 )
        ' NO CURRENT-LINE HIGHLIGHT in an excerpt. A full-width band across a five-line window
        ' -- keep reading; the reasoning below is unchanged by the views becoming scrollable.
        ' is loud, and it is drawn from the editor's own current-line colour -- which appears
        ' when the view takes focus and stays afterwards. That is what the reported
        ' "repainting problem" looked like.
        '
        ' BOTH CALLS ARE NEEDED, AND THE SECOND IS THE ONE THAT WORKS. SCI_SETCARETLINEVISIBLE
        ' is deprecated in Scintilla 5 -- the caret line is driven by an element colour there,
        ' so passing 0 is silently ignored. Worse, ApplyPropertiesToView sets the colour ONLY
        ' when gConfig.HighlightCurrentLine is on, so with it off the element keeps Scintilla's
        ' own default of WHITE: a full-width white band across the block's first line, which is
        ' exactly where FipPool_SciProc parks the caret after SETDOCPOINTER. Painting the
        ' highlight in the editor's own background makes it invisible whatever the message
        ' semantics turn out to be, which is why the colour is set rather than relying on the
        ' flag. Do not drop either line.
        SciExec( gFipPool(i).hWnd, SCI_SETCARETLINEVISIBLE, 0, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SETCARETLINEBACK, theme.editor.text.backcolor, 0 )
        ' The match highlight. The RANGES live on the document (filled once per file by
        ' FindProject_ApplyIndicators), but the indicator's appearance is PER-VIEW, so it has
        ' to be described again on every view that should show it.
        SciExec( gFipPool(i).hWnd, SCI_INDICSETSTYLE, FINDPROJ_INDICATOR, INDIC_STRAIGHTBOX )
        SciExec( gFipPool(i).hWnd, SCI_INDICSETFORE, FINDPROJ_INDICATOR, theme.editor.occurrence )
        SciExec( gFipPool(i).hWnd, SCI_INDICSETALPHA, FINDPROJ_INDICATOR, 90 )
    end if

    ' WHERE THE VIEW STARTS -- and ONLY on a fresh binding. Re-asserting this on every sync is
    ' what "pinned" used to mean, and doing it now would undo the user's own scrolling on the
    ' next repaint: FipPool_Sync calls this for a slot that is merely being repositioned as
    ' the list scrolls, not just for one being bound to a new row.
    if bNewRow then
        SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, gFip.blocks(b).nFirstLine, 0 )
    end if

    ' SWP_NOCOPYBITS: repaint the moved window rather than blitting its old pixels to the new
    ' position. Without it a view that scrolls into a different place can arrive carrying
    ' stale rows -- the other half of the reported repainting artifact.
    SetWindowPos( gFipPool(i).hWnd, 0, rcView.left, rcView.top, _
                  rcView.right - rcView.left, rcView.bottom - rcView.top, _
                  SWP_NOZORDER or SWP_SHOWWINDOW or SWP_NOCOPYBITS )
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
        ' The same fixed window for every excerpt, and the same identity FipRows_BlockHeight
        ' builds the row from -- so a view can never be taller or shorter than its row.
        rcView.bottom = rcView.top + (FIP_VIEW_LINES * gFipMetrics.nLineHeight)

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
        ' ApplyPropertiesToView has just re-enabled the current-line highlight for a full
        ' editor, so it has to be suppressed again here exactly as FipPool_Bind does -- this
        ' path was missing it, which would have brought the white band back on a theme change
        ' even once the bind path was fixed.
        SciExec( gFipPool(i).hWnd, SCI_SETCARETLINEVISIBLE, 0, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SETCARETLINEBACK, theme.editor.text.backcolor, 0 )
        ' ...and horizontal panning, which ApplyPropertiesToView has just set for a full
        ' editor that owns a real scrollbar. Suppressing tracking here would strand the caret
        ' off the right edge of a long line -- see FipPool_Ensure.
        SciExec( gFipPool(i).hWnd, SCI_SETSCROLLWIDTHTRACKING, 1, 0 )
        ' (The view is NOT re-scrolled to its block here. A theme change must not throw away
        '  wherever the user has navigated to inside an excerpt.)
    next

    PsVScrollBar_SetColors( HWND_FRMFINDINPROJECT_VSCROLL, theme.editor.scrollbar.backcolor, _
                            theme.editor.scrollbar.forecolor, theme.editor.scrollbar.forecolor )

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

' EVERY EXCERPT IS THE SAME HEIGHT. It used to be the block's own line count, which made the
' row heights vary from 5 up to FINDPROJ_MAX_BLOCK_LINES; the views scroll now, so a block is
' where the window starts rather than what it must contain. See FIP_VIEW_LINES.
function FipRows_BlockHeight( byref m as FIP_METRICS ) as long
    function = (FIP_VIEW_LINES * m.nLineHeight) + (m.nBlockPad * 2)
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
                    .nHeight = FipRows_BlockHeight( m )
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


' The excerpt that holds the caret, and the document behind it. This is what lets Edit
' commands work on the Find in Project tab at all: the tab has no document of its own, so
' every "pDoc = GetActiveDocumentPtr()" gate answers null and the whole Edit menu greys out.
function frmFindInProject_ActiveEdit( byref pDocOut as clsDocument ptr ) as HWND
    pDocOut = 0
    if frmFindInProject_IsActive() = false then return 0

    dim as HWND hFocus = GetFocus()
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd <> hFocus then continue for
        if (gFipPool(i).nRow < 0) orelse (gFipPool(i).nRow >= gFipRowCount) then return 0
        dim as long g = gFipRows( gFipPool(i).nRow ).nGroup
        if gFip.groups(g).pDoc = 0 then return 0
        pDocOut = gFip.groups(g).pDoc
        return hFocus
    next
    return 0
end function


' ========================================================================================
' The document Ctrl+S should save while this tab is up.
'
' ActiveEdit alone is not enough, and that gap IS the reported bug: it answers only while an
' excerpt holds the caret, so clicking the surface, the scrollbar or the find field -- or a
' Replace All, which rebinds every pooled view -- left Ctrl+S a SILENT no-op while
' Ctrl+Shift+S went on saving everything. The fallback is the excerpt that last had the
' caret, which is the file the user was editing and the only defensible answer.
'
' Deliberately NOT folded into ActiveEdit: that one routes the EDIT MENU, where "no excerpt
' focused" has to keep meaning "no edit window" or Ctrl+Z starts acting on a window the user
' cannot see.
' ========================================================================================
function frmFindInProject_SaveTarget( byref pDocOut as clsDocument ptr ) as boolean
    pDocOut = 0
    if frmFindInProject_IsActive() = false then return false
    if frmFindInProject_ActiveEdit( pDocOut ) then return true

    if gFipLastEditBlock < 0 then return false
    if gFipLastEditBlock >= gFip.nBlockCount then return false
    dim as long g = gFip.blocks(gFipLastEditBlock).nGroup
    if (g < 0) orelse (g >= gFip.nGroupCount) then return false
    if gFip.groups(g).pDoc = 0 then return false
    pDocOut = gFip.groups(g).pDoc
    return true
end function


' ========================================================================================
' The topmost excerpt view currently on screen -- the Tab target past the Find/Replace bars
' while this tab is up, standing in for the document Scintilla a normal tab would offer.
'
' ROW ORDER, NOT POOL ORDER. The slot pool is a recycling ring: FipPool_Sync parks and
' rebinds slots as the surface scrolls, so gFipPool(0) is whichever slot happened to be free
' when the first visible row was bound, not the first result. Tabbing into the middle of the
' list would look like the focus went nowhere.
' ========================================================================================
function frmFindInProject_FirstExcerptWindow() as HWND
    if frmFindInProject_IsActive() = false then return 0

    dim as long nBestRow = &h7FFFFFFF
    dim as HWND hBest
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for      ' parked: showing nothing
        if IsWindow( gFipPool(i).hWnd ) = 0 then continue for
        if gFipPool(i).nRow < nBestRow then
            nBestRow = gFipPool(i).nRow
            hBest = gFipPool(i).hWnd
        end if
    next

    function = hBest
end function


function frmFindInProject_RestoreFocus() as boolean
    if gFipLastEditBlock < 0 then return false
    if gFipLastEditBlock >= gFip.nBlockCount then return false

    ' The block may be off screen, or in a group that has since been collapsed. Bring it back
    ' into view before looking for the slot showing it -- otherwise there is nothing to focus.
    dim as long g = gFip.blocks(gFipLastEditBlock).nGroup
    if gFip.groups(g).pDoc = 0 then return false
    if gFip.groups(g).bCollapsed then return false

    ' Make sure the pool actually reflects what is on screen before looking for the slot: the
    ' tab has just been shown again, and a resize may not have fired.
    FipPool_Sync()

    ' If the block scrolled out of view while the user was elsewhere, bring it back -- there
    ' is no slot showing it otherwise, and the caret would have nowhere to go.
    dim as long nRowWanted = -1
    for r as long = 0 to gFipRowCount - 1
        if (gFipRows(r).nKind = FIP_ROW_BLOCK) andalso _
           (gFipRows(r).nBlock = gFipLastEditBlock) then nRowWanted = r: exit for
    next
    if nRowWanted <> -1 then
        dim as RECT rcCl
        GetClientRect( HWND_FRMFINDINPROJECT_SURFACE, @rcCl )
        dim as long nPage = rcCl.bottom - rcCl.top
        dim as long nTop  = gFipRows(nRowWanted).nTop
        dim as long nBot  = nTop + gFipRows(nRowWanted).nHeight
        if nTop < gFipScrollTop then
            gFipScrollTop = nTop
        elseif nBot > (gFipScrollTop + nPage) then
            gFipScrollTop = nBot - nPage
        end if
        FipSurface_SyncScrollBar()
        PsVScrollBar_SetPos( HWND_FRMFINDINPROJECT_VSCROLL, gFipScrollTop )
        FipPool_Sync()
    end if

    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).nRow = -1 then continue for
        if gFipRows( gFipPool(i).nRow ).nBlock <> gFipLastEditBlock then continue for
        SetFocus( gFipPool(i).hWnd )
        SciExec( gFipPool(i).hWnd, SCI_GOTOPOS, gFipLastEditPos, 0 )
        ' GOTOPOS scrolls the caret into view, which would slide the excerpt off its lines.
        SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, _
                 gFip.blocks(gFipLastEditBlock).nFirstLine, 0 )
        return true
    next
    return false
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


' Repaint the surface and nothing else. Deliberately does NOT rebuild or re-bind: this runs
' on every keystroke, and Refresh hides every pooled view on its way through -- which takes
' the caret out of the excerpt being typed into.
private sub frmFindInProject_OnRepaint()
    if IsWindow( HWND_FRMFINDINPROJECT_SURFACE ) then
        AfxRedrawWindow( HWND_FRMFINDINPROJECT_SURFACE )
    end if
end sub


sub frmFindInProject_Refresh( byval bResetScroll as boolean = false )
    FipMetrics_Measure( gFipMetrics )
    gFipCanvasH = FipRows_Build( gFipRows(), gFipRowCount, gFipMetrics )
    if bResetScroll then gFipScrollTop = 0
    FipSurface_SyncScrollBar()

    ' Hiding the focused window takes the focus with it, so remember where it was and put it
    ' back. The per-keystroke path avoids this function entirely (see gFipRepaint), but a
    ' rebuild can still happen while the caret is in an excerpt -- typing a newline, for one.
    dim as HWND hFocusWas = GetFocus()
    dim as boolean bFocusInPool = false
    for i as long = 0 to gFipPoolCount - 1
        if gFipPool(i).hWnd = hFocusWas then bFocusInPool = true: exit for
    next

    ' Park every slot first: the row list has just been rebuilt, so a slot's stored row index
    ' refers to the OLD list and would otherwise be matched against the new one by number.
    for i as long = 0 to gFipPoolCount - 1
        gFipPool(i).nRow = -1
        if gFipPool(i).hWnd then ShowWindow( gFipPool(i).hWnd, SW_HIDE )
    next
    FipPool_Sync()

    ' Hoisted to booleans: IsWindow/IsWindowVisible return BOOL, and mixing those into an
    ' andalso chain with a real boolean is warning 38.
    dim as boolean bStillThere = cbool( IsWindow( hFocusWas ) <> 0 )
    dim as boolean bStillShown = cbool( IsWindowVisible( hFocusWas ) <> 0 )
    if bFocusInPool andalso bStillThere andalso bStillShown then
        SetFocus( hFocusWas )
    end if
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

    ' The band BEHIND the card is the panel background, so a header row reads as chrome
    ' rather than as a gap in the code. The card itself keeps the editor's current-line
    ' colour -- two different jobs, two different palettes.
    b->SetBackColor( theme.panel.text.backcolor )
    b->PaintRect( @rc )

    ' A ROUNDED CARD, not a full-width band with a rule under it. The headers are the
    ' structure of this list, and a card reads as one object holding a filename rather than
    ' as a line drawn across the client area.
    dim as RECT rcCard = rc
    b->SetPenColor( theme.editor.divider )
'    b->PaintRoundOutline( @rcCard )
    b->PaintRect( @rcCard )

    dim as long nPad = AfxScaleX(6)

    ' The twisty. Down = expanded, right = collapsed, which is the direction every tree in
    ' tiko already uses.
    dim as RECT rcGlyph = rc
    rcGlyph.left  = frmFindInProject_TwistyColumnX() - AfxScaleX(8)
    rcGlyph.right = rcGlyph.left + AfxScaleX(16)
    b->SetFont( ghFont(SYMBOLFONT_9) )
    b->SetForeColor( theme.editor.text.forecolor )
    b->PaintText( iif( gFip.groups(r.nGroup).bCollapsed, wszIconChevronRight, wszIconChevronDown ), _
                  @rcGlyph, DT_CENTER )

    ' Name, then the match count, then the directory in the dimmer line-number colour. Drawn
    ' in that order because the count must not be pushed off the right by a long path.
    dim as DWSTRING wszName  = PsPathName(gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszDir   = PsPathDirWithSep(gFip.groups(r.nGroup).wszFile )
    dim as DWSTRING wszCount = "(" & str( gFip.groups(r.nGroup).nMatchCount ) & ")"

    dim as RECT rcText = rc
    rcText.left = rcGlyph.right + nPad

'    b->SetFont( ghFont(GUIFONTBOLD_10) )
    b->SetFont( ghFont(GUIFONT_10) )
    b->SetForeColor( theme.editor.text.forecolor )
    dim as long nNameW = FipSurface_TextW( b, wszName ) + nPad
    b->PaintText( wszName, @rcText, DT_LEFT )

    ' THE DIRTY CIRCLE, APPENDED TO THE FILENAME. This is where "one of these results has
    ' unsaved changes" is said now -- it used to be an amber stripe beside each changed line,
    ' which a scrolling excerpt can no longer point at (see FipSurface_Render). It is
    ' DELIBERATELY the same glyph, font and meaning the tab bar already uses for a modified
    ' document (frmTopTabs_PaintCallback), so the header and the tab cannot disagree; and it is
    ' asked of the DOCUMENT rather than of a snapshot, so saving clears it for free.
    dim as long nDirtyW = 0
    if FindProject_IsGroupDirty( r.nGroup ) then
        nDirtyW = AfxScaleX(14)
        dim as RECT rcDot = rc
        rcDot.left  = rcText.left + nNameW
        rcDot.right = rcDot.left + nDirtyW
        b->SetFont( ghFont(SYMBOLFONT_9) )
        b->SetForeColor( theme.editor.text.forecolor )
        b->PaintText( wszIconCompileResult, @rcDot, DT_CENTER )
    end if

    dim as RECT rcCount = rc
    rcCount.left = rcText.left + nNameW + nDirtyW
    b->SetFont( ghFont(GUIFONT_9) )
    b->SetForeColor( theme.editor.linenumbers.forecolor )
    dim as long nCountW = FipSurface_TextW( b, wszCount ) + nPad
    b->PaintText( wszCount, @rcCount, DT_LEFT )

    ' "Open File  Alt+Enter", but only on the group whose excerpt has the caret. It is an
    ' affordance for something you can only do while you are IN a file, so showing it on every
    ' header would be noise -- and it would also be ambiguous about which file it meant.
    dim as long nOpenLeft = rcCard.right
    if (r.nGroup = gFipFocusGroup) orelse (r.nGroup = gFipHotGroup) then
        ' Id 401, newly minted: there was no "Open File" string and no blank slot left, so
        ' MAXIMUM moved 400 -> 401 in all six .lang files, each with a real translation.
        ' "Alt+Enter" is deliberately NOT localized -- it is a keystroke, not a phrase.
        dim as DWSTRING wszOpen = L(390, "Open File")

        ' THE SHORTCUT IS SHOWN ONLY WHERE IT WOULD WORK. Alt+Enter acts on the excerpt
        ' holding the caret, so on a merely-hovered header it would advertise a keystroke
        ' that does nothing.
        dim as DWSTRING wszKey = ""
        if r.nGroup = gFipFocusGroup then wszKey = "Alt+Enter"

        b->SetFont( ghFont(GUIFONT_9) )
        dim as long nKeyW = 0
        if PsLen(wszKey) then nKeyW = FipSurface_TextW( b, wszKey ) + nPad*2
        dim as long nOpenW = FipSurface_TextW( b, wszOpen )
        nOpenLeft = rcCard.right - nPad - nKeyW - nOpenW - nPad

        dim as RECT rcOpen = rcCard
        rcOpen.left  = nOpenLeft
        rcOpen.right = rcCard.right - nPad
        rcOpen.top    += AfxScaleY(2)
        rcOpen.bottom -= AfxScaleY(2)
        b->SetBackColor( theme.editor.text.backcolor )
        b->PaintRoundRect( @rcOpen )

        dim as RECT rcLbl = rcOpen
        rcLbl.left += nPad
        b->SetForeColor( theme.editor.text.forecolor )
        b->SetBackColor( theme.editor.text.backcolor )
        b->PaintText( wszOpen, @rcLbl, DT_LEFT )

        if PsLen(wszKey) then
            dim as RECT rcKey = rcOpen
            rcKey.right -= nPad
            b->SetForeColor( theme.editor.linenumbers.forecolor )
            b->PaintText( wszKey, @rcKey, DT_RIGHT )
        end if

        ' Remembered so the click handler can tell the button from the rest of the bar, which
        ' folds. Only one header shows the button at a time, so one rect is enough.
        gFipOpenRect = rcOpen
    end if

    if PsLen(wszDir) then
        dim as RECT rcDir = rc
        rcDir.left = rcCount.left + nCountW
        ' Stop the path before the Open File affordance rather than letting it run under it.
        rcDir.right = nOpenLeft - nPad
        if rcDir.left < rcDir.right then
            b->SetForeColor( theme.editor.linenumbers.forecolor )
            b->SetBackColor( theme.editor.currentline )
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
    b->SetBackColor( theme.editor.text.backcolor )
    b->PaintRect( @rc )

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
                ' BOUNDS, NOT FAITH -- FipPool_SciProc's rule, and for the same reason. This
                ' is the one place a row is DEREFERENCED into the model (PaintHeader reads
                ' groups(nGroup)), and a WM_PAINT can arrive at any moment, including one
                ' where the model has been cleared and these rows have not yet been rebuilt.
                ' THE WM_NCDESTROY RESET IS THE FIX; this is only a cheap guard on the one
                ' dereference in the painter. Measured: with the reset removed and this guard
                ' in place the reopen STILL crashes, because FipPool_Sync/FipPool_Bind index
                ' groups() from the same rows. Do not read this line as making the reset
                ' optional.
                if gFipRows(i).nGroup < gFip.nGroupCount then
                    FipSurface_PaintHeader( b, gFipRows(i), yTop, rc )
                end if
            else
                ' (THE DIRTY-LINE STRIPE IS GONE. It was an amber bar in the strip left of the
                '  excerpt, one per line whose text differed from a snapshot taken when the
                '  search ran. A per-LINE signal only made sense while an excerpt was a fixed
                '  window onto a fixed run of lines: the views scroll now, so the lines under
                '  the stripe are not the lines on screen. The signal moved up to the file's
                '  own header -- FipSurface_PaintHeader draws the same dirty circle the tab
                '  bar uses -- and the whole snapshot subsystem behind it went with it.)

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
                    b->SetBackColor( theme.editor.divider )
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

            dim as long i = FipSurface_RowAtPoint( yClick )
            if i >= 0 then
                if gFipRows(i).nKind = FIP_ROW_HEADER then
                    ' THE WHOLE HEADER FOLDS; only the "Open File" button opens. The header is
                    ' a big target and folding is the thing you do to it constantly, so the
                    ' bar is the fold and opening gets the explicit button.
                    dim as long g = gFipRows(i).nGroup
                    dim as boolean bOnButton = false
                    if (gFipHotGroup = g) orelse (gFipFocusGroup = g) then
                        if (xClick >= gFipOpenRect.left) andalso (xClick < gFipOpenRect.right) andalso _
                           (yClick >= gFipOpenRect.top) andalso (yClick < gFipOpenRect.bottom) then
                            bOnButton = true
                        end if
                    end if

                    if bOnButton then
                        FipPool_OpenGroupFile( g )
                    else
                        gFip.groups(g).bCollapsed = not gFip.groups(g).bCollapsed
                        ' Keep the scroll where it is: the header the user clicked should not
                        ' jump out from under the cursor.
                        frmFindInProject_Refresh( false )
                    end if
                end if
            end if
            return 0

        case WM_MOUSEMOVE
            ' Hot-track the headers so a hovered one can offer "Open File".
            dim as long yMove = cast(short, hiword(lParam))
            dim as long nHot  = -1
            dim as long r     = FipSurface_RowAtPoint( yMove )
            if r >= 0 then
                if gFipRows(r).nKind = FIP_ROW_HEADER then nHot = gFipRows(r).nGroup
            end if
            if nHot <> gFipHotGroup then
                gFipHotGroup = nHot
                AfxRedrawWindow( hwnd )
            end if
            ' Ask to be told when the cursor leaves, or the last hovered header would keep
            ' its button after the mouse has gone.
            dim as TRACKMOUSEEVENT tme
            tme.cbSize    = sizeof(TRACKMOUSEEVENT)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent( @tme )
            return 0

        case WM_MOUSELEAVE
            if gFipHotGroup <> -1 then
                gFipHotGroup = -1
                AfxRedrawWindow( hwnd )
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
            b.SetBackColor( theme.editor.text.backcolor )
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
            gFipRepaint      = 0
            gFipUnbindDoc    = 0
            ' The next search starts from nothing, so the caption starts from nothing too.
            gFipLastQuery = ""
            gFipCurrent   = -1
            gFipFocusGroup = -1
            gFipHotGroup   = -1
            gFipLastEditBlock = -1
            ' AND THE DERIVED ROW LIST, WHICH IS THE ONE THAT CRASHED. The rows are built
            ' FROM the model and must never outlive it. OnCommand_FileClose calls
            ' FindProject_Clear on the way in here -- which erases groups(), blocks() and
            ' matches() and zeroes their counts -- but gFipRows is this file's own and
            ' survived, still describing the result set that has just been thrown away. The
            ' next Ctrl+Shift+F then created the window and SetFocusTab painted it BEFORE
            ' frmFindInProject_Refresh had rebuilt anything, so FipSurface_Render walked
            ' those stale rows and FipSurface_PaintHeader dereferenced groups(nGroup) in an
            ' erased array. Measured: 1785 rows against 0 groups, GPF on the second open.
            gFipRowCount  = 0
            erase gFipRows
            gFipCanvasH   = 0
            gFipScrollTop = 0
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
    PsVScrollBar_SetColors( HWND_FRMFINDINPROJECT_VSCROLL, theme.editor.scrollbar.backcolor, _
                            theme.editor.scrollbar.forecolor, theme.editor.scrollbar.forecolor )

    ' Hand the model its two callbacks. It is included long before this file and cannot name
    ' anything in it, so this is how it reaches the surface at all -- to say "I changed" and,
    ' critically, "let go of this document, I am about to release it".
    gFipModelChanged = @frmFindInProject_OnModelChanged
    gFipRepaint      = @frmFindInProject_OnRepaint
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

            ' EVERY BLOCK ROW IS THE SAME HEIGHT, and it is FIP_VIEW_LINES lines plus the
            ' padding -- the identity the excerpt views are sized against, so an error here
            ' would misplace every one of them. It used to be the block's own line count;
            ' asserting the constant rather than the formula is what makes a block that spans
            ' 40 lines still get a five-line window.
            dim as long nBadHeight = 0
            dim as long nWant = FipRows_BlockHeight( gFipMetrics )
            for i as long = 0 to gFipRowCount - 1
                if gFipRows(i).nKind <> FIP_ROW_BLOCK then continue for
                if gFipRows(i).nHeight <> nWant then nBadHeight += 1
            next
            frmFindInProject_Check( "canvas: every block row is the fixed excerpt height", _
                nBadHeight = 0, str(nBadHeight) & " wrong" )
            frmFindInProject_Check( "canvas: and that height is FIP_VIEW_LINES lines plus padding", _
                nWant = (FIP_VIEW_LINES * gFipMetrics.nLineHeight) + (gFipMetrics.nBlockPad * 2), _
                str(nWant) )

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

            ' ---- a fruitless search reddens the phrase -------------------------------------
            ' The state is asserted rather than the pixel: what the field is TOLD to paint is
            ' the part that can be wrong here.
            frmFindInProject_Check( "noresults: a search WITH hits leaves the field normal", _
                gFindTextIsError = false )

            ' A phrase that cannot occur, BUILT AT RUNTIME so it never exists as a literal in
            ' any file. The first version was a literal string of punctuation -- and it
            ' matched, once, because this suite searches tiko's own project and the needle was
            ' sitting right here in the source being searched. Control characters cannot
            ' appear in a text source, and concatenating keeps even the pieces apart.
            dim as long nHitsBefore = gFip.nMatchCount
            dim as DWSTRING wszNeedle = chr(1) & "nomatch" & chr(1)
            ' THE FIELD HAS TO HOLD THE PHRASE. The red is keyed on the field still showing
            ' what was searched -- that is what makes editing clear it -- so a test that
            ' searches without putting the phrase in the field is not reproducing anything
            ' the user can do. Real searches always come from this field.
            AfxSetWindowText( GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND ), wszNeedle )
            FindProject_Search( wszNeedle, true, false )
            frmFind_ApplyErrorState()
            frmFindInProject_Check( "noresults: a search with NO hits reddens the field", _
                gFindTextIsError = true, str(gFip.nLiveMatches) & " matches" )

            ' EDITING THE FAILED PHRASE CLEARS THE RED, without searching again. The red
            ' reports the outcome of a search, so once the text differs from what was
            ' searched it is describing something nobody has looked for yet.
            dim as HWND hFld = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
            AfxSetWindowText( hFld, gFip.wszPhrase & "x" )
            frmFind_ApplyErrorState()
            frmFindInProject_Check( "noresults: editing the phrase clears the red", _
                gFindTextIsError = false )

            ' ...and putting the failed phrase back makes it red again WITHOUT a new search,
            ' which is what proves the test is keyed on the text rather than on having just
            ' typed something.
            AfxSetWindowText( hFld, gFip.wszPhrase )
            frmFind_ApplyErrorState()
            frmFindInProject_Check( "noresults: restoring the failed phrase reddens it again", _
                gFindTextIsError = true )

            ' ...and finding something again clears it. Without this the assertion above
            ' would pass on a field that had simply been red since startup.
            FindProject_Search( sPhrase, true, false )
            frmFind_ApplyErrorState()
            frmFindInProject_Check( "noresults: finding hits again clears the red", _
                gFindTextIsError = false, str(gFip.nLiveMatches) & " matches" )
            frmFindInProject_Check( "noresults: and the results really did come back", _
                gFip.nMatchCount = nHitsBefore, _
                str(gFip.nMatchCount) & " vs " & str(nHitsBefore) )
            frmFindInProject_Check( "noresults: a normal search is not reported as capped", _
                gFip.bCapped = false )

            ' THE CAP. A one-character phrase over this project left the editor unresponsive
            ' for over three minutes before the two bounds went in -- from the outside,
            ' indistinguishable from a crash.
            FindProject_Search( "e", false, false )
            frmFindInProject_Check( "cap: a huge search stops at the match cap", _
                gFip.nMatchCount <= FINDPROJ_MAX_MATCHES, str(gFip.nMatchCount) )
            frmFindInProject_Check( "cap: and reports itself as capped", _
                gFip.bCapped = true )
            ' The other half: no block may swallow a whole file, or its per-line snapshot
            ' costs one GetLine for every line of every matching file.
            dim as long nFatBlocks = 0
            for b as long = 0 to gFip.nBlockCount - 1
                if gFip.blocks(b).nLineCount > FINDPROJ_MAX_BLOCK_LINES then nFatBlocks += 1
            next
            frmFindInProject_Check( "cap: no excerpt grew past the block cap", _
                nFatBlocks = 0, str(nFatBlocks) & " of " & str(gFip.nBlockCount) )

            FindProject_Search( sPhrase, true, false )
            frmFindInProject_Refresh( true )
            ' The group index is rebuilt by those searches.
            nGrp = -1
            for g as long = 0 to gFip.nGroupCount - 1
                if gFip.groups(g).pDoc = pTest then nGrp = g: exit for
            next

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
                ' ...and STARTING at the block's first line. This is no longer a pin -- the
                ' view scrolls freely from here -- but it is still where a freshly bound one
                ' has to open, and getting it wrong shows the wrong five lines, which looks
                ' like a search bug rather than a view bug. Nothing has navigated yet at this
                ' point in the run; the section that does is further down.
                if SciExec( gFipPool(i).hWnd, SCI_GETFIRSTVISIBLELINE, 0, 0 ) <> _
                   gFip.blocks(bb).nFirstLine then nBadLine += 1
            next

            frmFindInProject_Check( "pool: at least one excerpt view is bound", _
                nBound > 0, str(nBound) & " bound of " & str(gFipPoolCount) )
            frmFindInProject_Check( "pool: every bound view holds its group's document", _
                nBadDoc = 0, str(nBadDoc) & " wrong" )
            frmFindInProject_Check( "pool: every bound view opens on its block's first line", _
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
                ' Folding has no meaning in a five-line excerpt. Margin 1 (the symbol margin)
                ' is checked too: it was left out while it carried the dirty-line stripe, and
                ' that stripe is a header circle now.
                if SciExec( gFipPool(i).hWnd, SCI_GETMARGINWIDTHN, 1, 0 ) <> 0 then nMargins += 1
                if SciExec( gFipPool(i).hWnd, SCI_GETMARGINWIDTHN, 2, 0 ) <> 0 then nMargins += 1
            next
            frmFindInProject_Check( "pool: every view has its modification events silenced", _
                nNoisy = 0, str(nNoisy) & " noisy of " & str(gFipPoolCount) )
            frmFindInProject_Check( "pool: the fold margin is suppressed", _
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

            ' ---- the dirty circle on the file's header ---------------------------------------
            ' PER FILE, NOT PER LINE. This replaced a stripe drawn beside each line whose text
            ' differed from a snapshot taken when the search ran; the excerpts scroll now, so
            ' the lines that stripe pointed at are not the lines on screen. The signal is the
            ' document's own modified state, which is also what the tab bar reads.
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = -1 then continue for
                dim as long gg2 = gFipRows( gFipPool(i).nRow ).nGroup
                dim as long bb2 = gFipRows( gFipPool(i).nRow ).nBlock
                dim as clsDocument ptr pD2 = gFip.groups(gg2).pDoc
                if pD2 = 0 then continue for

                dim as long nLn = gFip.blocks(bb2).nFirstLine
                frmFindInProject_Check( "dirty: an untouched file shows no circle", _
                    FindProject_IsGroupDirty( gg2 ) = false )
                ' The tab's circle and the header's must agree -- one is built on the other,
                ' and this is what would catch them being re-separated.
                frmFindInProject_Check( "dirty: and neither does the tab", _
                    FindProject_AnyDirty() = false )

                ' Type into the file for real, then take it back. THE UNDO IS THE POINT: a
                ' flag set on edit would still be set here, which is exactly the bug the
                ' per-line comparison was written for. Asking the DOCUMENT keeps that property
                ' -- Scintilla clears SCI_GETMODIFY itself when the buffer returns to its save
                ' point -- without this module remembering anything.
                dim as HWND hD2 = pD2->hWindow(0)
                SciExec( hD2, SCI_BEGINUNDOACTION, 0, 0 )
                SciExec( hD2, SCI_INSERTTEXT, _
                         SciExec( hD2, SCI_POSITIONFROMLINE, nLn, 0 ), @"zz" )
                SciExec( hD2, SCI_ENDUNDOACTION, 0, 0 )
                frmFindInProject_Check( "dirty: an edited file DOES show the circle", _
                    FindProject_IsGroupDirty( gg2 ) = true )
                frmFindInProject_Check( "dirty: and so does the tab", _
                    FindProject_AnyDirty() = true )

                SciExec( hD2, SCI_UNDO, 0, 0 )
                frmFindInProject_Check( "dirty: undoing the edit clears it again", _
                    FindProject_IsGroupDirty( gg2 ) = false )
                SciExec( hD2, SCI_SETSAVEPOINT, 0, 0 )
                ' A group whose document has gone cannot be dirty, and an out-of-range one is
                ' answered rather than indexed -- this is called from a PAINT callback.
                frmFindInProject_Check( "dirty: an out-of-range group is not dirty", _
                    FindProject_IsGroupDirty( gFip.nGroupCount ) = false )

                ' ---- the Edit menu can reach an excerpt ------------------------------------
                ' This is what makes Ctrl+Z work in an excerpt: without it every Edit command
                ' asks GetActiveDocumentPtr, gets null on this tab, and greys itself out.
                dim as clsDocument ptr pEd
                frmFindInProject_Check( "edit: nothing focused means no edit window", _
                    frmFindInProject_ActiveEdit( pEd ) = 0 )

                SetFocus( gFipPool(i).hWnd )
                dim as HWND hEd = frmFindInProject_ActiveEdit( pEd )
                frmFindInProject_Check( "edit: a focused excerpt IS the edit window", _
                    hEd = gFipPool(i).hWnd )
                frmFindInProject_Check( "edit: and it reports the excerpt's own document", _
                    pEd = pD2 )
                ' The document's own active view must be left alone -- OnCommand_EditCommon
                ' borrows and restores it, and a leak there would send the user's next edit
                ' in the file's own tab to a window they cannot see.
                frmFindInProject_Check( "edit: the document's active view was not hijacked", _
                    pD2->hWndActiveScintilla <> gFipPool(i).hWnd )

                ' ---- focus survives leaving and returning to the tab -----------------------
                ' The reported failure: put the caret in an excerpt, switch to another tab,
                ' come back, and the caret was in the SEARCH BOX. The position is recorded on
                ' every message the focused view handles -- not only on WM_KILLFOCUS, which a
                ' tab switch does not reliably deliver to this subclass -- so by now it has
                ' been captured just by the SetFocus above.
                dim as long nPos2 = SciExec( gFipPool(i).hWnd, SCI_POSITIONFROMLINE, nLn, 0 ) + 1
                SciExec( gFipPool(i).hWnd, SCI_GOTOPOS, nPos2, 0 )
                ' One more message so the record is taken.
                SendMessage( gFipPool(i).hWnd, WM_KEYDOWN, VK_RIGHT, 0 )
                frmFindInProject_Check( "focus: the edit position was remembered", _
                    gFipLastEditBlock = bb2, str(gFipLastEditBlock) & " vs " & str(bb2) )

                ' Take focus away, exactly as switching tabs does, then ask for it back.
                SetFocus( HWND_FRMMAIN )
                frmFindInProject_Check( "focus: it is restored to the excerpt, not the box", _
                    frmFindInProject_RestoreFocus() = true )
                frmFindInProject_Check( "focus: and the caret really is in that excerpt", _
                    GetFocus() = gFipPool(i).hWnd )
                exit for
            next

            ' ---- the excerpt views SCROLL, and the caret can leave the block -----------------
            ' THIS SECTION IS THE INVERSE OF THE ONE IT REPLACED. An excerpt used to be a
            ' PINNED window onto exactly its block's lines: Up on the first line and Down on
            ' the last were swallowed before Scintilla saw them, and the view was re-scrolled
            ' back after any message that moved it -- so the assertions here asserted that
            ' nothing moved. That is the confinement this refactor removed, so an assertion
            ' that still passes on it would be asserting the bug.
            '
            ' PREFER A SLOT WHOSE BLOCK DOES NOT START AT LINE 0, and this is not cosmetic:
            ' taking the first bound slot picked one whose block began at the top of its file,
            ' where Up has nowhere to go and the whole leaves-the-block half of this section
            ' is skipped by its own guard. Measured -- the A/B that put the pin back turned
            ' two assertions red and could not reach that one at all.
            dim as long nScrollSlot = -1
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow = -1 then continue for
                if nScrollSlot = -1 then nScrollSlot = i
                if gFip.blocks( gFipRows( gFipPool(i).nRow ).nBlock ).nFirstLine > 0 then
                    nScrollSlot = i
                    exit for
                end if
            next

            for i as long = nScrollSlot to nScrollSlot
                if i < 0 then exit for
                if gFipPool(i).nRow = -1 then continue for
                dim as long bb = gFipRows( gFipPool(i).nRow ).nBlock
                dim as HWND hSlotView = gFipPool(i).hWnd
                dim as long nBlkFirst = gFip.blocks(bb).nFirstLine
                dim as long nBlkLast  = nBlkFirst + gFip.blocks(bb).nLineCount - 1
                dim as long nDocLast  = SciExec( hSlotView, SCI_GETLINECOUNT, 0, 0 ) - 1

                ' THE WINDOW IS FIVE LINES WHATEVER THE BLOCK SPANS -- which is the other half
                ' of the change, and the half no keystroke can demonstrate. Coalescing can run
                ' a block to FINDPROJ_MAX_BLOCK_LINES, and under the old height formula that
                ' was a 40-line window in the results list.
                dim as RECT rcEx
                GetClientRect( hSlotView, @rcEx )
                dim as long nLinesShown = (rcEx.bottom - rcEx.top) \ gFipMetrics.nLineHeight
                frmFindInProject_Check( "scroll: the excerpt window is FIP_VIEW_LINES tall", _
                    nLinesShown = FIP_VIEW_LINES, _
                    str(nLinesShown) & " vs " & str(FIP_VIEW_LINES) & _
                    " (block spans " & str(gFip.blocks(bb).nLineCount) & ")" )

                ' UP ON THE BLOCK'S FIRST LINE NOW MOVES. Still guarded: the slot chosen above
                ' prefers a block away from the top of its file, but a result set in which
                ' EVERY block starts at line 0 is legal. It says so out loud rather than
                ' passing quietly, because a silently skipped assertion is indistinguishable
                ' from a passing one in the totals.
                if nBlkFirst = 0 then
                    print "  NOTE: every visible block starts at line 0 -- " & _
                          "the leaves-the-block-upwards assertions did NOT run."
                end if
                if nBlkFirst > 0 then
                    SciExec( hSlotView, SCI_GOTOPOS, SciExec( hSlotView, SCI_POSITIONFROMLINE, nBlkFirst, 0 ), 0 )
                    SendMessage( hSlotView, WM_KEYDOWN, VK_UP, 0 )
                    dim as long nAfter = SciExec( hSlotView, SCI_LINEFROMPOSITION, _
                                                  SciExec( hSlotView, SCI_GETCURRENTPOS, 0, 0 ), 0 )
                    frmFindInProject_Check( "scroll: Up on the block's first line leaves the block", _
                        nAfter = nBlkFirst - 1, str(nAfter) & " vs " & str(nBlkFirst - 1) )
                    ' AND THE VIEW WENT WITH IT. Letting the caret move without scrolling
                    ' would park it off the top of a five-line window, which is worse than
                    ' not moving at all -- and is what dropping SCI_SETSCROLLWIDTHTRACKING's
                    ' vertical equivalent would look like.
                    frmFindInProject_Check( "scroll: and the view followed the caret up", _
                        SciExec( hSlotView, SCI_GETFIRSTVISIBLELINE, 0, 0 ) <= nAfter, _
                        str(SciExec( hSlotView, SCI_GETFIRSTVISIBLELINE, 0, 0 )) & " vs " & str(nAfter) )
                end if

                ' The far end of the file, which the caret clamp used to make unreachable.
                if nDocLast > nBlkLast then
                    SciExec( hSlotView, SCI_DOCUMENTEND, 0, 0 )
                    dim as long nEndLn = SciExec( hSlotView, SCI_LINEFROMPOSITION, _
                                                  SciExec( hSlotView, SCI_GETCURRENTPOS, 0, 0 ), 0 )
                    frmFindInProject_Check( "scroll: the caret reaches the end of the document", _
                        nEndLn = nDocLast, str(nEndLn) & " vs " & str(nDocLast) )
                    dim as long nTopLn = SciExec( hSlotView, SCI_GETFIRSTVISIBLELINE, 0, 0 )
                    frmFindInProject_Check( "scroll: and it is inside the visible window", _
                        (nEndLn >= nTopLn) andalso (nEndLn < (nTopLn + FIP_VIEW_LINES)), _
                        str(nEndLn) & " not in " & str(nTopLn) & ".." & _
                        str(nTopLn + FIP_VIEW_LINES - 1) )
                    ' ...which is only possible because the view is no longer held at the
                    ' block. Asserted separately: the two above would both pass on a document
                    ' whose end happens to fall inside the block's own window.
                    frmFindInProject_Check( "scroll: which means the view left the block", _
                        nTopLn <> nBlkFirst, str(nTopLn) & " vs " & str(nBlkFirst) )
                end if

                ' ...and an ordinary move still works. Without this, everything above would
                ' also pass if the subclass had gone back to eating arrow keys.
                if nDocLast > 0 then
                    SciExec( hSlotView, SCI_DOCUMENTSTART, 0, 0 )
                    dim as long nPosBefore = SciExec( hSlotView, SCI_GETCURRENTPOS, 0, 0 )
                    SendMessage( hSlotView, WM_KEYDOWN, VK_DOWN, 0 )
                    frmFindInProject_Check( "scroll: Down still moves the caret", _
                        SciExec( hSlotView, SCI_GETCURRENTPOS, 0, 0 ) <> nPosBefore, _
                        str(SciExec( hSlotView, SCI_GETCURRENTPOS, 0, 0 )) )
                end if

                ' Horizontal panning is what lets End on a long line stay visible. Tracking
                ' off is the state the pinned view shipped with, and the caret would simply
                ' vanish off the right edge.
                frmFindInProject_Check( "scroll: horizontal scroll-width tracking is on", _
                    SciExec( hSlotView, SCI_GETSCROLLWIDTHTRACKING, 0, 0 ) <> 0 )
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

            ' THE EMPTY STATE IS DELIBERATELY BLANK. It used to paint a "Find in Project"
            ' placeholder and this asserted that it drew ink; that message has since been
            ' commented out in FipSurface_Render, so one tone is now the CORRECT answer and
            ' the old assertion was testing a behaviour that no longer exists.
            '
            ' It still earns its place as the baseline for the comparison below: whatever the
            ' empty surface costs, a surface with results has to beat it.
            frmFindInProject_Check( "render: an empty surface is just the background", _
                nTonesEmpty <= 2, str(nTonesEmpty) )

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

    ' CLOSING DROPS THE RESULTS. It did not, and the symptom was silent: reopening with
    ' Ctrl+Shift+F showed the previous result set, stale against files that had since been
    ' edited and saved. It also leaked one document reference per result file, which nothing
    ' was giving back.
    frmFindInProject_Check( "close: the result model was cleared", _
        gFip.bValid = false, str(gFip.nGroupCount) & " groups left" )
    frmFindInProject_Check( "close: no group reads as dirty once the model is empty", _
        FindProject_AnyDirty() = false )
    frmFindInProject_Check( "close: no document reference is still held", _
        gFip.nBlockCount = 0, str(gFip.nBlockCount) & " blocks left" )
    frmFindInProject_Check( "close: the results window was destroyed", _
        IsWindow( HWND_FRMFINDINPROJECT ) = 0 )
    ' (THE ROW LIST DYING WITH THE MODEL is asserted in frmFindInProject_RunReopen, not here.
    '  The tone section above deliberately clears the model and refreshes, so gFipRowCount is
    '  ALREADY 0 by this point and the assertion would pass without the close having done
    '  anything at all -- measured, with the WM_NCDESTROY reset removed it stayed green.
    '  Re-searching here to populate it was tried and rejected: it puts the dirty document the
    '  Replace section leaves behind back into the model, and the close then raises a MODAL
    '  save prompt that hangs an unattended run.)
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
' Push the "n of m" the Find bar paints, and rename the tab to the phrase.
private sub FipBar_UpdateStatus()
    dim as long nOrdinal = 0
    if gFipCurrent >= 0 then
        for i as long = 0 to gFipCurrent
            if gFip.matches(i).bDead = false then nOrdinal += 1
        next
    end if
    gFind.wszResults = str(nOrdinal) & "/" & str(gFip.nLiveMatches)
    ' A capped search says so rather than quietly reporting a round number as if it were the
    ' whole answer.
    if gFip.bCapped then gFind.wszResults = gFind.wszResults & "+"
    gFind.foundCount = gFip.nLiveMatches
    ' A search that found nothing turns the phrase red.
    frmFind_ApplyErrorState()
    if IsWindowVisible( HWND_FRMMAIN_FIND ) then AfxRedrawWindow( HWND_FRMMAIN_FIND )

    ' The tab takes the phrase's name once there is one. SetTabText leaves a document-less
    ' tab alone, which is what lets this stick.
    dim as long idx = gTTabCtl.FindTabIndex()
    if idx <> -1 then
        ' Once there is a phrase the tab IS the phrase -- no prefix. The magnifier the tab
        ' painter puts to its left is what says this is a search.
        dim as DWSTRING wszCap = L(203, "Find in Project")
        if len(gFipLastQuery) then wszCap = gFipLastQuery
        ' A long phrase is truncated rather than allowed to take over the whole bar. Measured
        ' in the tab bar's own font, and the "..." is added here rather than left to a paint
        ' flag because PsTabBar sizes each tab from its caption -- an ellipsis applied only at
        ' paint time would leave the tab as wide as the untruncated text.
        dim as long nMaxChars = 32
        if PsLen(wszCap) > nMaxChars then wszCap = PsLeft(wszCap, nMaxChars - 1) & "..."

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
            GetTextExtentPoint32( hdcM, wszCap.sptr, PsLen(wszCap), @sz )
            SelectObject( hdcM, hOldFont )
            ReleaseDC( gTTabCtl.hWindow, hdcM )
            ' text + the magnifier + what the control reserves for its own close/padding. No
            ' extra room for the dirty circle: it takes the close button's PLACE rather than
            ' sitting beside the phrase, so it costs no width.
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
    ' scrolled to.
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
        ' PUT THE VIEW BACK ON ITS BLOCK, THEN LET SCINTILLA CORRECT IT. Setting the first
        ' visible line alone was enough while an excerpt was a five-line window onto exactly
        ' five lines; a block can span up to FINDPROJ_MAX_BLOCK_LINES, so the match being
        ' navigated to is often below the five that start at the block. SCI_SCROLLCARET
        ' scrolls only when it has to, so a match inside those five keeps the block's own
        ' framing and one further down brings the window to it.
        SciExec( gFipPool(i).hWnd, SCI_SETFIRSTVISIBLELINE, gFip.blocks(b).nFirstLine, 0 )
        SciExec( gFipPool(i).hWnd, SCI_SCROLLCARET, 0, 0 )
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

    if PsLen(wszPhrase) = 0 then
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


' ========================================================================================
' Re-run the search for whatever is in the find field, with the flags as they stand now.
'
' This is what the Match Case and Whole Word icons call. It is a full re-run rather than a
' re-highlight because the project results ARE the search: the flags decide which matches
' exist, not merely how they are drawn.
' ========================================================================================
sub frmFindInProject_RerunSearch()
    if frmFindInProject_IsActive() = false then exit sub
    if IsWindow( GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND ) ) = 0 then exit sub
    ' RunSearch unconditionally re-runs and re-stamps gFipLastQuery itself, so the "same
    ' phrase means navigate" shortcut in BarAction is left pointing at the new result set.
    FipBar_RunSearch()
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
' Env-gated repro: TIKO_FINDPROJ_REPRO=1
'
' Drives the REPORTED SEQUENCE through the real entry points -- a search that finds nothing,
' then a corrected phrase and Enter -- because that is the one that crashes and no assertion
' reaches it. BarAction is the function the Enter key actually funnels into, and the phrase
' is read from the field, so the field is written first exactly as typing would.
' ========================================================================================
' Drain the queue, so everything the real application would have processed between two
' keystrokes actually runs -- posted messages above all. MSG_USER_SETFOCUS is posted, not
' sent, so without this the repro skips the focus restore entirely.
private sub FipRepro_Pump()
    dim as MSG uMsg
    dim as long n = 0
    do while PeekMessage( @uMsg, null, 0, 0, PM_REMOVE )
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
        n += 1
        if n > 2000 then exit do
    loop
end sub


sub frmFindInProject_RunRepro()
    if environ("TIKO_FINDPROJ_REPRO") <> "1" then exit sub

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    if IsWindow( hCtl ) = 0 then
        print "REPRO: no find field -- open the tab first"
        exit sub
    end if

    print "REPRO: step 1 -- a phrase that matches nothing"
    AfxSetWindowText( hCtl, chr(1) & "nomatch" & chr(1) )
    frmFindInProject_BarAction( IDM_FINDNEXT )
    FipRepro_Pump()
    print "REPRO: step 1 searched, matches = " & str(gFip.nLiveMatches) & _
          ", current = " & str(gFipCurrent)
    ' RENDER. The real application paints between every keystroke, and the painter is the
    ' only thing that walks the blocks' line snapshots -- so a repro that never paints is
    ' skipping the most likely place for this to go wrong.
    print "REPRO: step 1 render, tones = " & str( FipSurface_CountRenderedTones() )

    print "REPRO: step 2 -- correct it to a phrase that DOES match, then Enter"
    AfxSetWindowText( hCtl, "function" )
    frmFindInProject_BarAction( IDM_FINDNEXT )
    FipRepro_Pump()
    print "REPRO: step 2 searched, matches = " & str(gFip.nLiveMatches) & _
          ", current = " & str(gFipCurrent)
    print "REPRO: step 2 render, tones = " & str( FipSurface_CountRenderedTones() )

    print "REPRO: step 3 -- Enter again on the same phrase (navigate)"
    frmFindInProject_BarAction( IDM_FINDNEXT )
    FipRepro_Pump()
    print "REPRO: step 3 searched, current = " & str(gFipCurrent)
    print "REPRO: step 3 render, tones = " & str( FipSurface_CountRenderedTones() )

    ' And scroll, which rebinds the pooled views against the new model.
    print "REPRO: step 4 -- scroll through the results"
    for n as long = 1 to 20
        PsVScrollBar_HandleWheelDelta( HWND_FRMFINDINPROJECT_VSCROLL, -120 )
        FipSurface_CountRenderedTones()
    next
    print "REPRO: step 4 survived"

    ' A very short phrase, which is a plausible "good search text" and produces an enormous
    ' result set -- the case that stresses every array's growth hardest.
    print "REPRO: step 5 -- a one-character phrase (huge result set)"
    AfxSetWindowText( hCtl, "e" )
    frmFindInProject_BarAction( IDM_FINDNEXT )
    FipRepro_Pump()
    print "REPRO: step 5 searched, matches = " & str(gFip.nLiveMatches) & _
          ", blocks = " & str(gFip.nBlockCount)
    ' NOTE on the tone count here: it is legitimately 1 when the viewport happens to sit
    ' wholly inside one excerpt, because that area is covered by a Scintilla child and the
    ' surface paints only its own background behind it. Checked, not assumed.
    print "REPRO: step 5 rows = " & str(gFipRowCount) & ", scroll = " & str(gFipScrollTop) & _
          ", capped = " & str(gFip.bCapped)
    print "REPRO: step 5 render, tones = " & str( FipSurface_CountRenderedTones() )

    print "REPRO: step 6 -- back to a normal phrase"
    AfxSetWindowText( hCtl, "function" )
    frmFindInProject_BarAction( IDM_FINDNEXT )
    FipRepro_Pump()
    print "REPRO: step 6 survived, matches = " & str(gFip.nLiveMatches)

    print "REPRO: completed without crashing"
end sub


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
        idx = PsTabBar_AddTab( gTTabCtl.hWindow, L(203, "Find in Project"), TAB_FINDINPROJECT )
        if idx = -1 then return 0
    end if

    gTTabCtl.TabFocusLost( gTTabCtl.GetCurSel() )
    gTTabCtl.SetFocusTab( idx )

    ' Adopt whatever the model holds. After SetFocusTab, so the surface has its real size and
    ' the scrollbar's page height is the one it will actually have.
    frmFindInProject_Refresh( true )

    ' Re-assert the caption and the tab's width. Reopening the tab on an existing search has
    ' to show the PHRASE, not "Find in Project" -- and without this the tab also kept the
    ' auto width it was given for the default caption, so the magnifier ate the first
    ' characters of it.
    FipBar_UpdateStatus()

    ' Env-gated crash repro. Last, so it runs against a fully built tab.
    frmFindInProject_RunRepro()

    function = 0
end function


' ========================================================================================
' Env-gated harness: TIKO_FINDPROJ_REPLACETEST=1
'
' Replace, Replace All, the Match Case / Whole Word toggles, and Ctrl+S -- driven through
' the REAL entry points (the icon panel's own mouse messages, frmFindInProject_BarAction,
' WM_COMMAND to frmMain), not by calling the model directly.
'
' IT WRITES ITS OWN TEMPORARY FILE and searches for a token that cannot occur anywhere
' else ("Wbeta"). Two reasons, and the second is not optional: the expected counts have
' to be KNOWN CONSTANTS rather than whatever happens to be open, and a Replace All is
' PROJECT-WIDE -- a phrase that also matched the user's own open documents would rewrite
' their buffers as a side effect of running the test.
'
' The four expected counts are all DIFFERENT (7 / 3 / 5 / 2), so no assertion here can pass
' by a flag being ignored in a way that happens to give the same answer.
' ========================================================================================
private function FipRT_Count( byval sHay as string, byval sNeedle as string ) as long
    dim as long n = 0, p = instr( sHay, sNeedle )
    do while p > 0
        n += 1
        p = instr( p + 1, sHay, sNeedle )
    loop
    function = n
end function

private function FipRT_GroupOf( byval pDoc as clsDocument ptr ) as long
    for g as long = 0 to gFip.nGroupCount - 1
        if gFip.groups(g).pDoc = pDoc then return g
    next
    function = -1
end function

' Live matches in ONE group. The project-wide gFip.nLiveMatches counts the user's other
' documents too, and this harness must not depend on what those contain.
private function FipRT_GroupLive( byval pDoc as clsDocument ptr ) as long
    dim as long g = FipRT_GroupOf( pDoc )
    if g = -1 then return 0
    dim as long n = 0
    for i as long = gFip.groups(g).nMatchFirst to _
                    gFip.groups(g).nMatchFirst + gFip.groups(g).nMatchCount - 1
        if gFip.matches(i).bDead = false then n += 1
    next
    function = n
end function

' A REAL press+release on one of the Find bar's icon buttons. PsIconPanel items are
' IP_KIND_COMMAND here, so the click callback fires on the UP of a matched pair -- which is
' why both messages are sent rather than one.
'
' THE CURSOR REALLY HAS TO MOVE. PsIconPanel resolves which item was hit through
' GetHotIconPanelItem, which reads GetCursorPos -- not the message's lParam. A synthetic
' click at a coordinate the mouse is not actually over presses nothing, silently: the first
' version of this harness sent both messages with the cursor elsewhere and reported that
' the toggle handler had never run, which would have been read as a bug in the handler.
private function FipRT_ClickIcon( byval hPanel as HWND, byval id as long ) as boolean
    if IsWindow( hPanel ) = 0 then return false
    dim as long idx = PsIconPanel_FindItemByID( hPanel, id )
    if idx = -1 then return false
    dim as RECT rc
    if PsIconPanel_GetItemRect( hPanel, idx, rc ) = false then return false
    dim as POINT pt
    pt.x = (rc.left + rc.right) \ 2
    pt.y = (rc.top + rc.bottom) \ 2
    dim as LPARAM lp = cast( LPARAM, ((pt.y and &hFFFF) shl 16) or (pt.x and &hFFFF) )
    ClientToScreen( hPanel, @pt )
    dim as POINT ptSave
    GetCursorPos( @ptSave )
    SetCursorPos( pt.x, pt.y )
    ' The hot item is tracked from WM_MOUSEMOVE (and a poll timer); give it the move first
    ' or the press lands with no hot item and does nothing.
    SendMessage( hPanel, WM_MOUSEMOVE, 0, lp )
    SendMessage( hPanel, WM_LBUTTONDOWN, 0, lp )
    SendMessage( hPanel, WM_LBUTTONUP,   0, lp )
    SetCursorPos( ptSave.x, ptSave.y )
    FipRepro_Pump()
    return true
end function


' A pump that also runs the ACCELERATOR step, which FipRepro_Pump deliberately does not.
'
' CAVEAT, and it bounds what the Ctrl+S assertions below can claim: frmMain's real loop
' runs eight FilterMessage calls AHEAD of TranslateAccelerator. They are not reproduced
' here, so this proves the accelerator table maps the keystroke and the command reaches the
' handler -- it cannot prove that no filter upstream is eating it first.
dim shared as long gFipRTKeysSeen, gFipRTKeysEaten

private sub FipRT_PumpWithAccel()
    dim as CWindow ptr pW = AfxCWindowPtr( HWND_FRMMAIN )
    dim as MSG m
    dim as long n = 0
    do while PeekMessage( @m, null, 0, 0, PM_REMOVE )
        dim as boolean bEaten = false
        dim as boolean bIsS = cbool( (m.message = WM_KEYDOWN) andalso (m.wParam = asc("S")) )
        if bIsS then gFipRTKeysSeen += 1
        if pW <> 0 then
            if pW->AccelHandle <> 0 then
                bEaten = cbool( TranslateAccelerator( pW->hWindow, pW->AccelHandle, @m ) <> 0 )
            end if
        end if
        if bIsS andalso bEaten then gFipRTKeysEaten += 1
        if bEaten = false then
            TranslateMessage( @m )
            DispatchMessage( @m )
        end if
        n += 1
        if n > 2000 then exit do
    loop
end sub

' A REAL Ctrl+S, synthesised at the keyboard driver, so it goes to whatever has the focus
' exactly as the user's own keystroke would.
' keybd_event is ASYNCHRONOUS -- it hands the events to the system, which posts them to the
' focus queue some time later. Pumping immediately drains a queue the keystroke has not
' reached yet, and the assertion then reports that Ctrl+S did nothing. That produced a
' result that flipped between runs with no code change, which is worse than no test: sleep
' and re-pump until the queue is genuinely empty of it.
' FALSE when the keystroke never arrived, which is an ENVIRONMENT failure and not a tiko
' one: injected input goes to the FOREGROUND window's queue, and this harness runs at
' startup, where another application can still own the foreground. Reported as a skip rather
' than as a failed assertion -- run unattended it produced a FAIL that flipped between runs
' with no code change, which is the most misleading result a suite can give.
private function FipRT_SendCtrlS() as boolean
    ' Drain first, so a stray keystroke left over from an earlier attempt cannot be counted
    ' as this one's.
    FipRT_PumpWithAccel()
    gFipRTKeysSeen = 0
    gFipRTKeysEaten = 0

    if GetForegroundWindow() <> HWND_FRMMAIN then
        print "  NOTE: frmMain is not the foreground window -- Ctrl+S keystroke SKIPPED."
        return false
    end if

    keybd_event( VK_CONTROL, 0, 0, 0 )
    keybd_event( asc("S"),   0, 0, 0 )
    keybd_event( asc("S"),   0, KEYEVENTF_KEYUP, 0 )
    keybd_event( VK_CONTROL, 0, KEYEVENTF_KEYUP, 0 )
    for i as long = 1 to 10
        sleep 30, 1
        FipRT_PumpWithAccel()
    next

    if gFipRTKeysSeen = 0 then
        print "  NOTE: the injected Ctrl+S never reached the queue -- SKIPPED."
        return false
    end if
    print "  KEY: 'S' keydowns pumped = " & str(gFipRTKeysSeen) & _
          ", claimed by an accelerator = " & str(gFipRTKeysEaten)
    return true
end function

' Make a buffer dirty the way a keystroke would -- through the document, not the model.
private sub FipRT_DirtyBuffer( byval hSci as HWND )
    dim sDirty as string = chr(13) & chr(10) & "x"
    SciExec( hSci, SCI_APPENDTEXT, len(sDirty), strptr(sDirty) )
end sub


sub frmFindInProject_RunReplaceTest()
    if environ("TIKO_FINDPROJ_REPLACETEST") <> "1" then exit sub

    gFipTestPass = 0
    gFipTestFail = 0
    print "--- TIKO_FINDPROJ_REPLACETEST ---"

    ' ---- the temporary file -------------------------------------------------------------
    dim as DWSTRING wszTemp = environ("TEMP")
    if PsLen(wszTemp) = 0 then wszTemp = "C:\"
    if PsRight(wszTemp, 1) <> "\" then wszTemp = wszTemp & "\"
    dim as DWSTRING wszFile = wszTemp & "tiko_fip_replacetest.bas"

    ' A DOCUMENT ALREADY OPEN ON THIS PATH WINS OVER THE BYTES ON DISK, so it has to go
    ' first. Killing a previous run mid-test leaves the temp file in the restored session,
    ' and OpenSelectedDocument then hands back that stale buffer -- which reported 0 matches
    ' and 7 already-replaced tokens, i.e. every assertion failing for a reason that had
    ' nothing to do with the code under test.
    dim pStale as clsDocument ptr = gApp.pDocList
    do until pStale = 0
        dim pNextStale as clsDocument ptr = pStale->pDocNext
        if ucase(str(pStale->DiskFilename)) = ucase(str(wszFile)) then
            ' Silence the save prompt: this buffer is about to be overwritten anyway.
            if IsWindow( pStale->hWindow(0) ) then
                SciExec( pStale->hWindow(0), SCI_SETSAVEPOINT, 0, 0 )
            end if
            pStale->UserModified = false
            dim as long idxStale = gTTabCtl.GetTabIndexByDocumentPtr( pStale )
            if idxStale <> -1 then gTTabCtl.CloseTab( idxStale )
        end if
        pStale = pNextStale
    loop
    FipRepro_Pump()

    dim as long f = freefile
    if open( str(wszFile) for output as #f ) <> 0 then
        print "  FAIL: could not create " & str(wszFile)
        exit sub
    end if
    print #f, "' Wbeta Wbeta Wbeta WbetaBeta Wbeta2"
    print #f, "dim Wbeta as long"
    print #f, "print ""Wbeta"""
    close #f

    dim as wstring * MAX_PATH wszOpen = wszFile
    dim as clsDocument ptr pT = OpenSelectedDocument( wszOpen )
    if pT = 0 then
        print "  FAIL: could not open the temporary file"
        exit sub
    end if
    FipRepro_Pump()

    dim as string sOrig = pT->GetText()
    frmFindInProject_Check( "setup: the file loaded with 7 occurrences of the token", _
        FipRT_Count( ucase(sOrig), "Wbeta" ) = 7, str(FipRT_Count( ucase(sOrig), "Wbeta" )) )

    ' ---- the four flag combinations, at the model level ---------------------------------
    ' Directly, so the toggle assertions below are comparing the click's answer against a
    ' number this harness has already established independently.
    FindProject_Search( "Wbeta", false, false )
    dim as long nCI = FipRT_GroupLive( pT )
    FindProject_Search( "Wbeta", true, false )
    dim as long nCS = FipRT_GroupLive( pT )
    FindProject_Search( "Wbeta", false, true )
    dim as long nWW = FipRT_GroupLive( pT )
    FindProject_Search( "Wbeta", true, true )
    dim as long nCSWW = FipRT_GroupLive( pT )

    frmFindInProject_Check( "flags: case-insensitive, any part of a word = 7", nCI = 7, str(nCI) )
    frmFindInProject_Check( "flags: match case = 3", nCS = 3, str(nCS) )
    frmFindInProject_Check( "flags: whole word = 5", nWW = 5, str(nWW) )
    frmFindInProject_Check( "flags: match case AND whole word = 2", nCSWW = 2, str(nCSWW) )

    ' ---- open the tab and search through the BAR ----------------------------------------
    gFind.nMatchCase = false
    gFind.nWholeWord = false
    frmFindInProject_Show()
    FipRepro_Pump()

    dim as HWND hFind = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    if IsWindow( hFind ) = 0 then
        print "  FAIL: the Find bar has no find field"
    else
        AfxSetWindowText( hFind, "Wbeta" )
        FipBar_RunSearch()
        FipRepro_Pump()
        frmFindInProject_Check( "bar: a search through the bar finds all 7", _
            FipRT_GroupLive( pT ) = 7, str(FipRT_GroupLive( pT )) )

        ' ---- THE TOGGLES ----------------------------------------------------------------
        ' The reported question: when Match Case or Whole Word changes, do the results in the
        ' buffer change with it? Driven by a real click on the icon, not by writing gFind.
        if FipRT_ClickIcon( ghFindIconsField, FINDTIP_MATCHCASE ) = false then
            print "  FAIL: no Match Case icon to click"
        else
            frmFindInProject_Check( "toggle: clicking Match Case sets the flag", _
                cbool(gFind.nMatchCase <> 0) = true )
            frmFindInProject_Check( "toggle: the MODEL was re-searched with Match Case on", _
                gFip.bMatchCase = true )
            frmFindInProject_Check( "toggle: Match Case narrows the results to 3", _
                FipRT_GroupLive( pT ) = 3, str(FipRT_GroupLive( pT )) )

            FipRT_ClickIcon( ghFindIconsField, FINDTIP_MATCHCASE )
            frmFindInProject_Check( "toggle: clicking it again widens them back to 7", _
                FipRT_GroupLive( pT ) = 7, str(FipRT_GroupLive( pT )) )
        end if

        if FipRT_ClickIcon( ghFindIconsField, FINDTIP_WHOLEWORD ) = false then
            print "  FAIL: no Whole Word icon to click"
        else
            frmFindInProject_Check( "toggle: clicking Whole Word sets the flag", _
                cbool(gFind.nWholeWord <> 0) = true )
            frmFindInProject_Check( "toggle: the MODEL was re-searched with Whole Word on", _
                gFip.bWholeWord = true )
            frmFindInProject_Check( "toggle: Whole Word narrows the results to 5", _
                FipRT_GroupLive( pT ) = 5, str(FipRT_GroupLive( pT )) )

            FipRT_ClickIcon( ghFindIconsField, FINDTIP_WHOLEWORD )
            frmFindInProject_Check( "toggle: clicking it again widens them back to 7", _
                FipRT_GroupLive( pT ) = 7, str(FipRT_GroupLive( pT )) )
        end if

        ' ---- REPLACE (one) --------------------------------------------------------------
        ReplaceControls_Show()
        FipRepro_Pump()
        dim as HWND hRep = GetDlgItem( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE )
        if IsWindow( hRep ) = 0 then
            print "  FAIL: the Replace bar has no replace field"
        else
            AfxSetWindowText( hRep, "Wbeta" )

            ' Re-run so gFipCurrent lands on the first match of a known result set.
            AfxSetWindowText( hFind, "Wbeta" )
            FipBar_RunSearch()
            FipRepro_Pump()
            dim as long nBefore = FipRT_GroupLive( pT )

            frmFindInProject_BarAction( IDM_REPLACENEXT )
            FipRepro_Pump()

            dim as string sAfter1 = pT->GetText()
            frmFindInProject_Check( "replace one: exactly one Wbeta is in the buffer", _
                FipRT_Count( sAfter1, "Wbeta" ) = 1, str(FipRT_Count( sAfter1, "Wbeta" )) )
            frmFindInProject_Check( "replace one: the live count dropped by exactly one", _
                FipRT_GroupLive( pT ) = nBefore - 1, _
                str(FipRT_GroupLive( pT )) & " vs " & str(nBefore - 1) )
            frmFindInProject_Check( "replace one: the document is DIRTY, not written to disk", _
                SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 )

            ' ---- REPLACE ALL ------------------------------------------------------------
            dim as long nLeft = FipRT_GroupLive( pT )
            frmFindInProject_BarAction( IDM_REPLACEALL )
            FipRepro_Pump()

            dim as string sAfter2 = pT->GetText()
            frmFindInProject_Check( "replace all: no live match is left in the group", _
                FipRT_GroupLive( pT ) = 0, str(FipRT_GroupLive( pT )) )
            frmFindInProject_Check( "replace all: every remaining match became Wbeta", _
                FipRT_Count( sAfter2, "Wbeta" ) = nLeft + 1, _
                str(FipRT_Count( sAfter2, "Wbeta" )) & " vs " & str(nLeft + 1) )
            ' THE ONE THAT CATCHES A CASE-BLIND REPLACE. Replace All ran with both flags OFF,
            ' so all 7 spellings had to go -- including the ones inside WbetaBeta and
            ' Wbeta2, which are matches only because Whole Word is off.
            frmFindInProject_Check( "replace all: not one spelling of the token survives", _
                FipRT_Count( ucase(sAfter2), "Wbeta" ) = 0, _
                str(FipRT_Count( ucase(sAfter2), "Wbeta" )) & " left" )

            ' ---- CTRL+S -----------------------------------------------------------------
            ' Two separate questions, and they fail differently.
            '
            ' (1) Is Ctrl+S in the accelerator table at all? Read back from the table Windows
            '     actually holds, not from gKeys -- a binding can be stored, displayed and
            '     saved while never reaching CreateAcceleratorTable.
            dim as CWindow ptr pW = AfxCWindowPtr( HWND_FRMMAIN )
            if pW = 0 then
                print "  FAIL: no CWindow for frmMain"
            else
                dim as long nEntries = CopyAcceleratorTable( pW->AccelHandle, 0, 0 )
                dim as boolean bFoundSave = false
                if nEntries > 0 then
                    redim acc( 0 to nEntries - 1 ) as ACCEL
                    CopyAcceleratorTable( pW->AccelHandle, @acc(0), nEntries )
                    for i as long = 0 to nEntries - 1
                        if acc(i).cmd <> IDM_FILESAVE then continue for
                        if acc(i).key <> asc("S") then continue for
                        if (acc(i).fVirt and FCONTROL) = 0 then continue for
                        if (acc(i).fVirt and FSHIFT) <> 0 then continue for
                        if (acc(i).fVirt and FALT) <> 0 then continue for
                        bFoundSave = true
                    next
                end if
                frmFindInProject_Check( "ctrl+s: Ctrl+S maps to IDM_FILESAVE in the real table", _
                    bFoundSave, str(nEntries) & " accelerators" )
            end if

            ' (2) Does the command actually save? Sent with the FOCUS INSIDE AN EXCERPT, which
            '     is the state the report describes -- on this tab GetActiveDocumentPtr is
            '     null, so IDM_FILESAVE has to resolve the document from the focused view.
            dim as HWND hExcerpt = 0
            for i as long = 0 to gFipPoolCount - 1
                if gFipPool(i).nRow < 0 then continue for
                if gFip.groups( gFipRows( gFipPool(i).nRow ).nGroup ).pDoc = pT then
                    hExcerpt = gFipPool(i).hWnd: exit for
                end if
            next
            frmFindInProject_Check( "ctrl+s: an excerpt view onto the test file is on screen", _
                hExcerpt <> 0 )

            if hExcerpt <> 0 then
                SetFocus( hExcerpt )
                FipRepro_Pump()
                dim as clsDocument ptr pFocused
                frmFindInProject_Check( "ctrl+s: the focused excerpt resolves to the test file", _
                    (frmFindInProject_ActiveEdit( pFocused ) = hExcerpt) andalso (pFocused = pT) )

                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FILESAVE, 0), 0 )
                FipRepro_Pump()

                frmFindInProject_Check( "ctrl+s: the buffer is no longer modified", _
                    SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) = 0, _
                    str(SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 )) )

                ' The only proof that matters: the bytes on disk.
                dim as long f2 = freefile
                dim as string sDisk
                if open( str(wszFile) for input as #f2 ) = 0 then
                    sDisk = space( lof(f2) )
                    if len(sDisk) then get #f2, , sDisk
                    close #f2
                end if
                frmFindInProject_Check( "ctrl+s: the replacement reached the file on disk", _
                    FipRT_Count( sDisk, "Wbeta" ) > 0, str(FipRT_Count( sDisk, "Wbeta" )) )
                frmFindInProject_Check( "ctrl+s: the old token is gone from the file on disk", _
                    FipRT_Count( ucase(sDisk), "Wbeta" ) = 0, _
                    str(FipRT_Count( ucase(sDisk), "Wbeta" )) & " left" )

                ' THE CIRCLE ON THE FILE'S HEADER. It was a per-line amber stripe compared
                ' against text captured WHEN THE SEARCH RAN, which is a different definition
                ' of "dirty" from the save point and had to be re-baselined by hand on every
                ' save. It is the document's own modified state now, so this asserts what was
                ' reported as the expected behaviour -- save and the mark goes -- rather than
                ' the re-baselining that used to have to make it true.
                frmFindInProject_Check( "ctrl+s: saving clears the file's dirty circle", _
                    FindProject_IsGroupDirty( FipRT_GroupOf( pT ) ) = false )

                ' (3) And the KEYSTROKE, not the command. Everything above went in as a
                '     WM_COMMAND, which skips the whole question the report is about.
                SetForegroundWindow( HWND_FRMMAIN )
                SetFocus( hExcerpt )
                FipRT_PumpWithAccel()
                FipRT_DirtyBuffer( pT->hWindow(0) )
                FipRT_PumpWithAccel()
                frmFindInProject_Check( "ctrl+s key: the buffer is dirty before the keystroke", _
                    SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 )
                frmFindInProject_Check( "ctrl+s key: the excerpt still has the focus", _
                    GetFocus() = hExcerpt )
                if FipRT_SendCtrlS() then
                    frmFindInProject_Check( "ctrl+s key: Ctrl+S in an EXCERPT saved the file", _
                        SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) = 0, _
                        str(SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 )) )
                end if

                ' (4) THE SAME KEYSTROKE WITH THE FOCUS ONE WINDOW OUT. On this tab
                '     IDM_FILESAVE resolves its document from the FOCUSED EXCERPT and from
                '     nothing else, so anywhere else on the tab -- the surface, the scrollbar,
                '     the find field -- Ctrl+S is a silent no-op while Ctrl+Shift+S still
                '     saves everything. That asymmetry is the reported symptom, so it is
                '     measured here rather than reasoned about.
                FipRT_DirtyBuffer( pT->hWindow(0) )
                FipRT_PumpWithAccel()
                SetFocus( HWND_FRMFINDINPROJECT_SURFACE )
                FipRT_PumpWithAccel()
                if FipRT_SendCtrlS() then
                    frmFindInProject_Check( "ctrl+s key: Ctrl+S saves from the SURFACE too", _
                        SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) = 0, _
                        str(SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 )) )
                end if

                ' ...and Save All, from the same focus, does save it.
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FILESAVEALL, 0), 0 )
                FipRT_PumpWithAccel()
                frmFindInProject_Check( "save all: Save All saves from anywhere on the tab", _
                    SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) = 0, _
                    str(SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 )) )
            end if
        end if
    end if

    ' ---- Ctrl+S in an ORDINARY editor window --------------------------------------------
    ' The other half of the report. Done with the Find tab CLOSED, so the document's own tab
    ' is current and this is the plain editor path with nothing special about it.
    FindProject_Clear()
    dim as long idxFip = gTTabCtl.FindTabIndex()
    if idxFip <> -1 then gTTabCtl.CloseTab( idxFip )
    FipRepro_Pump()

    dim as long idxDoc = gTTabCtl.GetTabIndexByDocumentPtr( pT )
    if idxDoc <> -1 then
        gTTabCtl.SetFocusTab( idxDoc )
        SetForegroundWindow( HWND_FRMMAIN )
        SetFocus( pT->hWndActiveScintilla )
        FipRT_PumpWithAccel()
        FipRT_DirtyBuffer( pT->hWndActiveScintilla )
        FipRT_PumpWithAccel()
        frmFindInProject_Check( "ctrl+s key: the editor buffer is dirty before the keystroke", _
            SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) <> 0 )
        frmFindInProject_Check( "ctrl+s key: the editor window has the focus", _
            GetFocus() = pT->hWndActiveScintilla, hex(cast(integer, GetFocus())) )
        if FipRT_SendCtrlS() then
            frmFindInProject_Check( "ctrl+s key: Ctrl+S in a plain EDITOR window saved the file", _
                SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 ) = 0, _
                str(SciExec( pT->hWindow(0), SCI_GETMODIFY, 0, 0 )) )
        end if
    end if

    ' ---- clean up -----------------------------------------------------------------------
    idxDoc = gTTabCtl.GetTabIndexByDocumentPtr( pT )
    if idxDoc <> -1 then gTTabCtl.CloseTab( idxDoc )
    FipRepro_Pump()
    kill( str(wszFile) )

    print "--- TIKO_FINDPROJ_REPLACETEST: " & str(gFipTestPass) & " passed, " & _
          str(gFipTestFail) & " failed ---"
end sub


' ========================================================================================
' Env-gated repro: TIKO_FINDPROJ_REOPEN=1
'
' The REPORTED SEQUENCE for the second crash, driven through the real entry points: select a
' phrase in the active document, Ctrl+Shift+F, click the tab's X, Ctrl+Shift+F again.
'
' IT EXISTS BECAUSE THE SELF-TEST CANNOT REACH IT. That suite opens the tab once and closes
' it, and never reopens -- and the crash was in the WM_PAINT that SetFocusTab triggers on the
' SECOND open, before Refresh has rebuilt anything. Nothing that does not run a real message
' pump between the two opens can see it, which is what FipRepro_Pump is for here.
' ========================================================================================
sub frmFindInProject_RunReopen()
    if environ("TIKO_FINDPROJ_REOPEN") <> "1" then exit sub

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then
        print "REOPEN: no active document -- nothing to select"
        exit sub
    end if

    ' Select a phrase, exactly as the user's drag would: the seeded path is the one reported,
    ' and it is what makes the reopen run a full search rather than showing an empty list.
    dim as HWND hSci = pDoc->hWndActiveScintilla
    dim as string sNeedle = "function"
    SciExec( hSci, SCI_SETTARGETSTART, 0, 0 )
    SciExec( hSci, SCI_SETTARGETEND, SciExec( hSci, SCI_GETLENGTH, 0, 0 ), 0 )
    SciExec( hSci, SCI_SETSEARCHFLAGS, 0, 0 )
    dim as long r = SciExec( hSci, SCI_SEARCHINTARGET, len(sNeedle), strptr(sNeedle) )
    if r <> -1 then SciExec( hSci, SCI_SETSEL, r, r + len(sNeedle) )
    print "REOPEN: step 0 selection = [" & str(pDoc->GetSelText) & "]"

    print "REOPEN: step 1 -- Ctrl+Shift+F"
    OnCommand_SearchFindInProject()
    FipRepro_Pump()
    print "REOPEN: step 1 done, tab = " & str(gTTabCtl.FindTabIndex()) & _
          ", matches = " & str(gFip.nLiveMatches) & ", rows = " & str(gFipRowCount)

    print "REOPEN: step 2 -- click the tab's X"
    dim as long idx = gTTabCtl.FindTabIndex()
    dim as long nRowsIn = gFipRowCount
    if idx <> -1 then gTTabCtl.CloseTab( idx )
    FipRepro_Pump()
    ' THE NUMBER THIS HARNESS EXISTS FOR. Groups 0 with rows non-zero is the crash: the
    ' second open paints those rows against the erased model. Asserted here rather than in the
    ' self-test, which has already cleared and refreshed by the time it closes and so cannot
    ' tell the two implementations apart.
    print "REOPEN: step 2 done, tab = " & str(gTTabCtl.FindTabIndex()) & _
          ", groups = " & str(gFip.nGroupCount) & ", rows = " & str(gFipRowCount)
    if nRowsIn > 0 then
        frmFindInProject_Check( "reopen: the derived row list died with the model", _
            gFipRowCount = 0, str(nRowsIn) & " in, " & str(gFipRowCount) & " left" )
    else
        print "  NOTE: the search produced no rows -- that assertion did NOT run."
    end if

    print "REOPEN: step 3 -- Ctrl+Shift+F again"
    OnCommand_SearchFindInProject()
    FipRepro_Pump()
    print "REOPEN: step 3 done, tab = " & str(gTTabCtl.FindTabIndex()) & _
          ", matches = " & str(gFip.nLiveMatches) & ", rows = " & str(gFipRowCount)

    ' And close it again, so a run of this harness leaves nothing behind.
    idx = gTTabCtl.FindTabIndex()
    if idx <> -1 then gTTabCtl.CloseTab( idx )
    FipRepro_Pump()

    print "--- TIKO_FINDPROJ_REOPEN: " & str(gFipTestPass) & " passed, " & _
          str(gFipTestFail) & " failed ---"
    print "REOPEN: completed without crashing"
end sub
