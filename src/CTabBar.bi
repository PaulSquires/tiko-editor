
#pragma once

#include once "clsDoubleBuffer.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id.
#define IDT_CTABBAR_HOTTRACK   &hCB01
#define CTABBAR_HOTTRACK_MS    100


' Default horizontal inset applied to each side of a tab's text when measuring. The
' control DPI-scales this at Create; CTabBar_SetPadding takes raw pixels thereafter.
#define CTABBAR_DEFAULT_PADDING  8

' Default side of the two square icon rects (left icon, right close icon) reserved in
' EVERY tab, used or not -- always reserving them is what keeps the caption at the same
' offset in every tab, and stops the close "X" shifting the text when it appears on
' hover. DPI-scaled at Create; CTabBar_SetIconWidth takes raw pixels thereafter.
#define CTABBAR_DEFAULT_ICONWIDTH  20

' Cross-control navigation messages, for driving the bar from a SEPARATE control (the
' host's nav buttons) via SendMessage. Each is the message-shaped door to the plain
' function of the same name -- one implementation, two doors.
'   TCM_NAVIGATE:    wParam = -1 (left) / +1 (right). Returns the new first-visible index.
'   TCM_CANNAVIGATE: wParam = -1 / +1. Returns 1 if navigation that way would move, else 0
'                    (so a nav control can grey its buttons out).
#define TCM_NAVIGATE     (WM_USER + 100)
#define TCM_CANNAVIGATE  (WM_USER + 101)

' type and array to hold values related to the tab bar's tabs
' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
' that never name the real problem -- hence nMinWidth / nWidth throughout.
' NOTE: no isHot here. Hover is transient and single-valued, so the control's nLastHotIdx
' is the one source of truth; a per-tab copy would be a second one to keep in sync.
type CTABBAR_TABINFO
    Text      as DWSTRING
    itemData  as integer         ' id to invoke if clicked on
    nMinWidth as integer = 0     ' floor for this tab's width; 0 = auto-size to text
    ' --- Everything below is DERIVED by LayoutTabs, never set from outside. A tab
    '     scrolled off the left holds EMPTY rects, not stale ones: hit-testing loops
    '     every tab, so a leftover rect would still answer hover and clicks. ---
    nWidth    as integer = 0     ' measured natural width (the no-shrink formula)
    rc        as RECT            ' full tab rect, client coordinates
    rcPaint   as RECT            ' rc deflated by the border -- the host paints in THIS
    rcText    as RECT            ' caption area between the icon rects (symmetric height)
    rcIcon    as RECT            ' left icon rect (file-type glyph etc.)
    rcClose   as RECT            ' right icon rect (the close "X"; hit-test target)
end type

type CTABBAR_PAINTINFO
    hTabBar         as HWND                   ' the control, so the callback can query it
    itemID          as integer                ' tab index (model index)
    b               as clsDoubleBuffer ptr    ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutTabs. Never re-derive insets from rc. ---
    rc              as RECT                   ' the border-DEFLATED paint area: fill THIS.
                                              '   On the active tab it runs to the client
                                              '   bottom (your fill covers the gap where
                                              '   the control's bottom line breaks).
    rcText          as RECT                   ' caption area, rcIcon.right -> rcClose.left.
                                              '   Symmetric vertical extents, so DT_VCENTER
                                              '   text never moves when selection changes.
    rcIcon          as RECT                   ' left icon square (file-type glyph etc.)
    rcClose         as RECT                   ' right icon square (the close "X")
    ' --- State: the control decides these, you only render. ---
    isSelected      as boolean                ' this tab is the active tab
    isHot           as boolean                ' the mouse is over this tab
    isHotClose      as boolean                ' mouse is over the close icon. A tab can be
                                              '   both isHot and isHotClose: the "X" is
                                              '   inside the tab.
    isPressed       as boolean                ' a live left press is on this tab
    isDragging      as boolean                ' this tab is being drag-reordered (it is
                                              '   also isPressed; the tabs visibly moving
                                              '   ARE the drag feedback -- style this one
                                              '   so the eye can follow it)
    wszCaption      as DWSTRING               ' the tab's Text
end type

type CTABBAR_MESSAGEINFO
    hTabBar        as HWND
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
    idx             as integer    ' tab index under the mouse (-1 if none)
end type


' Draw one tab. Called for each tab touched by the repaint; keep it cheap. Paint through
' p->b (the control's double buffer for this repaint) -- do not touch the screen DC.
' Fill p->rc, caption in p->rcText, glyphs in p->rcIcon / p->rcClose. Style from the
' state flags; the control decides them, you only render. No callback = no tabs drawn
' (the control still paints its background and chrome).
'
' Two contracts worth honoring, or the layout will not match what you draw:
'   - Draw with the SAME font you handed to CTabBar_SetFont. The control measured the
'     tab widths with it; a different font means the widths lie.
'   - Use the rects as handed to you instead of re-deriving insets from p->rc -- they
'     are exactly the space the measured width accounts for, and their vertical extents
'     encode the never-moves-on-selection rule.
'
' The control draws its own chrome AFTER the callbacks (the bottom line, broken under
' the active tab, and the active tab's 3-sided border), so chrome is authoritative no
' matter what a callback paints.
type TB_PaintCallbackSub as sub( byval p as CTABBAR_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed.
'
' CAUTION: the result is honored for every message EXCEPT WM_LBUTTONUP and WM_MBUTTONUP,
' where it is IGNORED. The control holds mouse capture across a press (unlike CStatusBar,
' and deliberately -- the close-X cancel semantics and Phase 5's drag both consume the
' guaranteed down->up pairing), and the up-message is what releases it: a callback that
' suppressed it would strand capture and route every subsequent click to this control
' (the CListBox bug recorded in Learnings.md). Suppressing WM_LBUTTONDOWN suppresses
' selection and the press, never the capture bookkeeping.
type TB_MessageCallbackFunc as function( byval m as CTABBAR_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for a tab, on demand (only when a tip is about to show).
' Return "" for no tooltip. If unset, the tab's own Text is used.
type TB_TooltipCallbackFunc as function( byval hTabBar as HWND, byval idx as integer ) as DWSTRING

' The active tab changed through USER interaction (click, middle of a gesture the
' control owns). Fired AFTER the control's state is updated, so GetCurSel() = idxNew.
' Programmatic SetCurSel does NOT fire this -- only the user does (Win32's
' TCM_SETCURSEL / TCN_SELCHANGE precedent), which also makes it safe to call SetCurSel
' from inside the handler without recursing.
type TB_SelChangeCallbackSub as sub( byval hTabBar as HWND, byval idxOld as integer, byval idxNew as integer )

' The user asked to close a tab: a matched press+release on its close "X", or a matched
' middle-click anywhere on it. NOTIFY-ONLY -- the control deletes nothing. The host
' decides: prompt to save, veto by doing nothing, or call CTabBar_DeleteTab itself.
type TB_CloseCallbackSub as sub( byval hTabBar as HWND, byval idx as integer )

' A drag reordered a tab: fired once PER MOVE, after the control's array and stored
' indices are already updated. Reordering is LIVE (each midpoint crossing moves the tab
' immediately), so one drag can fire several of these -- mirror each (idxFrom, idxTo)
' move into your model as it arrives and the two stay in sync. Unlike close there is
' nothing to veto: a move destroys nothing, so the control reorders itself and tells
' you after. Programmatic CTabBar_MoveTab does NOT fire this (the SetCurSel rule:
' programmatic is silent, only the user speaks).
type TB_ReorderCallbackSub as sub( byval hTabBar as HWND, byval idxFrom as integer, byval idxTo as integer )

type CTABBAR
    hWin            as HWND
    hToolTip        as HWND
    wszTooltip      as DWSTRING
    tabs(any)       as CTABBAR_TABINFO
    tabCount        as integer = 0
    idc_TabBar     as integer = 2000
    HoverTime       as integer = 250
    nLastHotIdx     as integer = -1       ' tab the mouse was last over (hover tracking)
    ' Second single-valued hover fact, kept beside nLastHotIdx for the same reason isHot
    ' is not a TABINFO field: one source of truth. A tab is both isHot and isHotClose
    ' when the cursor is over its "X" -- the X is inside the tab.
    bLastHotClose   as boolean = false    ' hover is over the hot tab's close rect
    hotTimerOn      as boolean = false    ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press -- unlike
    '     CStatusBar, and deliberately: the close-X cancel semantics (press the X, slide
    '     off, release, nothing happens) and Phase 5's drag both consume the guaranteed
    '     down->up pairing. It pays CVScrollBar's full price: release on the up-message
    '     unconditionally, WM_CAPTURECHANGED cancels the press, WM_DESTROY releases,
    '     and no callback may suppress an up-message. Any tab mutation cancels a live
    '     press -- the gesture's target is gone. ---
    nPressedIdx     as integer = -1       ' tab under a live left press, or -1
    bPressedClose   as boolean = false    ' that press started on the close "X"
    nPressedMidIdx  as integer = -1       ' tab under a live middle press, or -1
    ' --- Drag-to-reorder. A left press (not on the "X") that travels past the system
    '     drag threshold becomes a drag; thereafter the tabs EXCHANGE the moment the
    '     pointer crosses into an adjacent tab, gated on the direction of travel
    '     (live reordering -- the moving tabs are the drag visual).
    '     While dragging, HOVER IS PINNED to the dragged tab (tracking suspends, the
    '     clear paths stand down): the dragged tab keeps isHot -- and so its "X" --
    '     for the whole gesture, and a tab being dragged over can never light up or
    '     grow an X of its own. Normal tracking resumes on the first move after drop.
    '     No ESC cancel: the control never takes focus, so it never sees keyboard
    '     messages, and with live reordering every intermediate order is already a
    '     legitimate, notified state. Capture loss simply ends the drag the same way. ---
    bDragging       as boolean = false    ' the live press became a drag
    ptPress         as POINT              ' client coords of the press (threshold test)
    nLastDragX      as integer = 0        ' previous drag x: the direction-of-travel gate
    BackColor       as COLORREF
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member
    ' procedure of this type, and "dim as HFONT x" there fails with a misleading
    ' "error 14: Expected identifier, found 'HFONT'". See C:\dev\Learnings.md.
    hTabFont      as HFONT
    PaintCallback     as TB_PaintCallbackSub
    MessageCallback   as TB_MessageCallbackFunc
    TooltipCallback   as TB_TooltipCallbackFunc    ' optional; defaults to the tab's Text
    SelChangeCallback as TB_SelChangeCallbackSub   ' optional; user-driven changes only
    CloseCallback     as TB_CloseCallbackSub       ' optional; notify-only, host deletes
    ReorderCallback   as TB_ReorderCallbackSub     ' optional; one per live move
    ' --- Layout. tab rects are derived, never set from outside; LayoutTabs() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs it,
    '     which coalesces a burst of SetText calls into one measuring pass. ---
    nPadding        as integer = CTABBAR_DEFAULT_PADDING     ' DPI-scaled at Create
    nIconWidth      as integer = CTABBAR_DEFAULT_ICONWIDTH   ' DPI-scaled at Create
    nBorderWidth    as integer = 1                           ' DPI-scaled at Create
    BorderColor     as COLORREF                              ' bottom line + active tab border
    bLayoutDirty    as boolean = true
    ' --- Selection and scroll position. Stored indices: every insert/delete must fix
    '     them up, and layout clamps nFirstVisible (you cannot scroll into empty space). ---
    nCurSel         as integer = -1       ' the active tab, or -1 for none
    nFirstVisible   as integer = 0        ' leftmost drawn tab (listbox-style top index)
    bEnsureVisible  as boolean = false    ' scroll nCurSel into view on the next layout

    declare sub      LayoutTabs()
    declare sub      MoveTab( byval idxFrom as integer, byval idxTo as integer )
    declare function GetCount() as integer                                  ' tab count
    declare function InsertTabAt( byval idx as integer ) as CTABBAR_TABINFO ptr
    declare function AddTab() as CTABBAR_TABINFO ptr                       ' append
    declare function GetTab( byval idx as integer ) as CTABBAR_TABINFO ptr
    declare function DeleteTabAt( byval idx as integer ) as boolean
    declare function IsValidTab( byval idx as integer ) as boolean
    declare sub      Clear()
    declare sub      Refresh()
end type

' Measure every tab and assign its rects. This is the only place tab geometry is
' produced; everything else (hit-testing, painting, invalidation) consumes it.
'
'   width = max( text width + 2*icon width + 2*padding + 2*border, nMinWidth )
'
' Tabs NEVER shrink: a tab is always at least as wide as its own text plus its two
' always-reserved icon rects. When the tabs no longer fit, the rightmost simply clips at
' the client edge -- navigation (nFirstVisible, driven by a separate host control) is the
' escape valve, not squeezing. Rects that run past the edge are computed honestly rather
' than squeezed: clipping is the paint pass's job, and the cursor can't reach past the
' edge anyway, so hit-testing stays correct for free.
'
' Tabs scrolled off the left (i < nFirstVisible) get EMPTY rects, not stale ones:
' hit-testing loops every tab, so a leftover rect would still answer hover and clicks.
'
' rcPaint is rc deflated by the border the control draws (Phase 3), and the deflation is
' deliberately ASYMMETRIC:
'   - left/top/right on EVERY tab: reserving space a border MAY occupy is what stops the
'     caption jumping sideways the moment a tab is selected (same argument as the
'     always-reserved icon rects).
'   - bottom on INACTIVE tabs only: the active tab's rcPaint runs to the client bottom,
'     because its own fill is what covers the gap where the full-width bottom line
'     breaks. Deflate it and a 1px background strip destroys the merge-with-content look.
' The icon rects are v-centred on the SYMMETRICALLY deflated height, so they (and any
' text aligned to them) sit at identical offsets whether or not the tab is selected.
sub CTABBAR.LayoutTabs()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub
    if this.tabCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as integer clientW = rcClient.right - rcClient.left
    dim as integer clientH = rcClient.bottom - rcClient.top
    ' No geometry yet (created 0x0, sized only once the host shows us). Nothing to lay out,
    ' and bLayoutDirty stays consumed only because WM_SIZE will mark it again.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTabFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    for i as integer = 0 to this.tabCount - 1
        dim as integer textW = 0
        dim as integer nChars = len( this.tabs(i).Text )
        if nChars > 0 then
            dim as SIZE sz
            if GetTextExtentPoint32W( hDC, this.tabs(i).Text.vptr, nChars, @sz ) then
                textW = sz.cx
            end if
        end if
        ' The border is part of the spend: rcPaint is deflated by it on both sides, so a
        ' width that omits it clips the last 2*bw pixels of every caption (found by
        ' screenshot in Phase 3 -- the geometry trace matched the formula, but the
        ' formula was short).
        dim as integer nW = textW + (this.nIconWidth * 2) + (this.nPadding * 2) + (this.nBorderWidth * 2)
        if nW < this.tabs(i).nMinWidth then nW = this.tabs(i).nMinWidth
        this.tabs(i).nWidth = nW
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' Clamp the scroll position: stored index, so deletes can leave it past the end,
    ' and you cannot scroll into empty space.
    if this.nFirstVisible < 0 then this.nFirstVisible = 0
    if this.nFirstVisible > this.tabCount - 1 then this.nFirstVisible = this.tabCount - 1

    ' Scroll-into-view, requested by SetCurSel and consumed HERE, because this is where
    ' measured widths exist (SetCurSel may run before the control is even sized).
    if this.bEnsureVisible then
        this.bEnsureVisible = false
        if this.IsValidTab( this.nCurSel ) then
            if this.nCurSel < this.nFirstVisible then
                this.nFirstVisible = this.nCurSel
            else
                ' Find the smallest first-visible >= current such that the selected
                ' tab's right edge fits. If the tab alone is wider than the client,
                ' show it from its own left edge -- its right will clip, honestly.
                dim as integer totalW = 0
                for i as integer = this.nCurSel to this.nFirstVisible step -1
                    totalW += this.tabs(i).nWidth
                    if totalW > clientW then
                        this.nFirstVisible = i + 1
                        if this.nFirstVisible > this.nCurSel then this.nFirstVisible = this.nCurSel
                        exit for
                    end if
                next
            end if
        end if
    end if

    ' Back-fill: when the whole tail [nFirstVisible..end] fits the client with enough
    ' slack to also hold the previous tab, pull earlier tabs into view. Without this a
    ' delete (or a widening resize) leaves dead space on the right while tabs sit
    ' hidden to the left -- a listbox clamps its top index the same way. Runs after
    ' ensure-visible: it only ever reduces nFirstVisible while keeping the full tail
    ' (nCurSel included) visible, so the two never fight.
    dim as integer tailW = 0
    for i as integer = this.nFirstVisible to this.tabCount - 1
        tailW += this.tabs(i).nWidth
    next
    while (this.nFirstVisible > 0) andalso _
          (tailW + this.tabs(this.nFirstVisible - 1).nWidth <= clientW)
        this.nFirstVisible -= 1
        tailW += this.tabs(this.nFirstVisible).nWidth
    wend

    ' Position left-to-right from nFirstVisible; every tab spans the full client height.
    dim as integer x = rcClient.left
    for i as integer = 0 to this.tabCount - 1
        if i < this.nFirstVisible then
            SetRectEmpty( @this.tabs(i).rc )
            SetRectEmpty( @this.tabs(i).rcPaint )
            SetRectEmpty( @this.tabs(i).rcText )
            SetRectEmpty( @this.tabs(i).rcIcon )
            SetRectEmpty( @this.tabs(i).rcClose )
            continue for
        end if

        SetRect( @this.tabs(i).rc, x, rcClient.top, x + this.tabs(i).nWidth, rcClient.bottom )

        ' rcPaint: the asymmetric deflation (see the header comment).
        dim as RECT rp = this.tabs(i).rc
        rp.left   += this.nBorderWidth
        rp.top    += this.nBorderWidth
        rp.right  -= this.nBorderWidth
        if i <> this.nCurSel then rp.bottom -= this.nBorderWidth
        this.tabs(i).rcPaint = rp

        ' Icon rects: v-centred on the symmetric height so they never move with selection.
        dim as integer cyMid   = ( (this.tabs(i).rc.top + this.nBorderWidth) + _
                                   (this.tabs(i).rc.bottom - this.nBorderWidth) ) \ 2
        dim as integer iconTop = cyMid - (this.nIconWidth \ 2)
        SetRect( @this.tabs(i).rcIcon, _
                 rp.left + this.nPadding, iconTop, _
                 rp.left + this.nPadding + this.nIconWidth, iconTop + this.nIconWidth )
        SetRect( @this.tabs(i).rcClose, _
                 rp.right - this.nPadding - this.nIconWidth, iconTop, _
                 rp.right - this.nPadding, iconTop + this.nIconWidth )

        ' Caption area: between the icon rects, on the same symmetric vertical extents,
        ' so DT_VCENTER text obeys the same never-moves-on-selection rule as the icons.
        SetRect( @this.tabs(i).rcText, _
                 this.tabs(i).rcIcon.right, this.tabs(i).rc.top + this.nBorderWidth, _
                 this.tabs(i).rcClose.left, this.tabs(i).rc.bottom - this.nBorderWidth )

        x += this.tabs(i).nWidth
    next
end sub

function CTABBAR.GetCount() as integer
    return this.tabCount
end function

function CTABBAR.IsValidTab( byval idx as integer ) as boolean
    return (idx >= 0) andalso (idx < this.tabCount)
end function

function CTABBAR.GetTab( byval idx as integer ) as CTABBAR_TABINFO ptr
    if this.IsValidTab(idx) = false then return null
    return @this.tabs(idx)
end function

' Insert a fresh (reset) tab at idx, shifting later tabs right. Grows the
' backing store by doubling so bulk inserts are amortized O(1), not O(n^2).
function CTABBAR.InsertTabAt( byval idx as integer ) as CTABBAR_TABINFO ptr
    if idx < 0 then idx = 0
    if idx > this.tabCount then idx = this.tabCount

    dim as integer cap = ubound(this.tabs) + 1
    if this.tabCount >= cap then
        dim as integer newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.tabs( 0 to newcap - 1 )
    end if

    ' shift [idx .. tabCount-1] right by one (no-op when appending)
    for i as integer = this.tabCount to idx + 1 step -1
        this.tabs(i) = this.tabs(i - 1)
    next

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.tabs(idx)
        .Text       = ""
        .itemData   = 0
        .nMinWidth  = 0
        .nWidth     = 0
        SetRectEmpty( @.rc )
        SetRectEmpty( @.rcPaint )
        SetRectEmpty( @.rcText )
        SetRectEmpty( @.rcIcon )
        SetRectEmpty( @.rcClose )
    end with

    this.tabCount += 1

    ' Stored indices follow the shift. The tabCount > 1 guard keeps the very first tab
    ' from bumping nFirstVisible off it (0 <= 0 would fire). nCurSel = -1 can never
    ' fire its guard, since idx >= 0. Note: adding a tab does NOT select it -- the host
    ' decides what is current, always.
    if (this.tabCount > 1) andalso (idx <= this.nFirstVisible) then this.nFirstVisible += 1
    if idx <= this.nCurSel then this.nCurSel += 1

    ' A mutation mid-press invalidates the gesture: the pressed slot no longer means
    ' what the finger meant.
    this.nPressedIdx = -1
    this.bPressedClose = false
    this.nPressedMidIdx = -1
    this.bDragging = false

    this.Refresh()
    return @this.tabs(idx)
end function

function CTABBAR.AddTab() as CTABBAR_TABINFO ptr
    return this.InsertTabAt( this.tabCount )
end function

function CTABBAR.DeleteTabAt( byval idx as integer ) as boolean
    if this.IsValidTab(idx) = false then return false
    ' shift [idx+1 .. tabCount-1] down by one
    for i as integer = idx to this.tabCount - 2
        this.tabs(i) = this.tabs(i + 1)
    next
    this.tabs(this.tabCount - 1).Text = ""   ' free the vacated last slot's string
    this.tabCount -= 1
    ' A hot index can likewise be left dangling past the end.
    if this.nLastHotIdx >= this.tabCount then this.nLastHotIdx = -1
    ' Stored indices follow the shift; layout re-clamps nFirstVisible.
    if idx < this.nFirstVisible then this.nFirstVisible -= 1
    ' Deleting the selected tab leaves NOTHING selected: the control never guesses
    ' which document the host wants next (the notify-only philosophy, applied here).
    if idx = this.nCurSel then
        this.nCurSel = -1
    elseif idx < this.nCurSel then
        this.nCurSel -= 1
    end if
    ' A mutation mid-press invalidates the gesture (see InsertTabAt).
    this.nPressedIdx = -1
    this.bPressedClose = false
    this.nPressedMidIdx = -1
    this.bDragging = false
    this.Refresh()
    return true
end function

' Move one tab to another slot, remapping every stored index the way the slots moved.
' This is the THIRD index-fixup site (after insert and delete) -- the same bug class,
' worth the same care:
'   i = idxFrom          -> idxTo
'   idxFrom < i <= idxTo -> i - 1   (move right)
'   idxTo <= i < idxFrom -> i + 1   (move left)
' nCurSel and nLastHotIdx follow the DOCUMENT (the selection/hover is on a tab, and the
' tab moved). nFirstVisible deliberately does NOT: scroll position is a viewport concept,
' and a drag only ever touches visible tabs (hidden ones hold empty rects the cursor
' cannot hit), so every index the move remaps is >= nFirstVisible already -- following
' the dragged tab would instead scroll the bar as a side effect of dragging its first
' visible tab, hiding the tabs that just slid into its place.
sub CTABBAR.MoveTab( byval idxFrom as integer, byval idxTo as integer )
    if this.IsValidTab(idxFrom) = false then exit sub
    if this.IsValidTab(idxTo) = false then exit sub
    if idxFrom = idxTo then exit sub

    dim as CTABBAR_TABINFO tmp = this.tabs(idxFrom)
    if idxTo > idxFrom then
        for i as integer = idxFrom to idxTo - 1
            this.tabs(i) = this.tabs(i + 1)
        next
    else
        for i as integer = idxFrom to idxTo + 1 step -1
            this.tabs(i) = this.tabs(i - 1)
        next
    end if
    this.tabs(idxTo) = tmp

    dim as integer lo = iif( idxFrom < idxTo, idxFrom, idxTo )
    dim as integer hi = iif( idxFrom < idxTo, idxTo, idxFrom )
    dim as integer nShift = iif( idxTo > idxFrom, -1, 1 )

    if this.nCurSel = idxFrom then
        this.nCurSel = idxTo
    elseif (this.nCurSel >= lo) andalso (this.nCurSel <= hi) then
        this.nCurSel += nShift
    end if

    if this.nLastHotIdx = idxFrom then
        this.nLastHotIdx = idxTo
    elseif (this.nLastHotIdx >= lo) andalso (this.nLastHotIdx <= hi) then
        this.nLastHotIdx += nShift
    end if

    this.Refresh()
end sub

sub CTABBAR.Clear()
    for i as integer = 0 to this.tabCount - 1
        this.tabs(i).Text = ""
    next
    this.tabCount  = 0
    this.nLastHotIdx = -1
    this.bLastHotClose = false
    this.nCurSel = -1
    this.nFirstVisible = 0
    this.bEnsureVisible = false
    this.nPressedIdx = -1
    this.bPressedClose = false
    this.nPressedMidIdx = -1
    this.bDragging = false
    this.Refresh()
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of SetText calls costs one measuring pass, not N.
sub CTABBAR.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking tab is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A TAB BAR, NOT A TAB CONTAINER
'   The control never shows, hides, creates or owns tab pages. It reports what was
'   selected (SelChangeCallback), what the user wants closed (CloseCallback -- notify
'   only, nothing is deleted), and how tabs were dragged (ReorderCallback). The host
'   owns everything below the bar.
'
' THE CONTROL HANDLE
'   Every CTabBar_* function takes the handle returned by CTabBar_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need
'   to treat the control as a window, e.g. SetWindowPos() to place and size it. An opaque
'   wrapper would buy a little type-safety and break that, so it was rejected.
'
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized: position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function CTabBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing / moving tabs.  Add* and Insert* return the new tab's index, or -1.
'   MoveTab reorders programmatically with the same index remapping a drag performs
'   (selection and hover follow the moved tab; the scroll position stays put). Like
'   SetCurSel it is SILENT -- the reorder callback fires only for user drags.
' ----------------------------------------------------------------------------------------
declare function CTabBar_AddTab( byval hTabBar as HWND, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
declare function CTabBar_IsValidTab( byval hTabBar as HWND, byval idx as integer ) as boolean
declare function CTabBar_InsertTab( byval hTabBar as HWND, byval idx as integer, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
declare function CTabBar_DeleteTab( byval hTabBar as HWND, byval idx as integer ) as boolean
declare function CTabBar_MoveTab( byval hTabBar as HWND, byval idxFrom as integer, byval idxTo as integer ) as boolean
declare sub      CTabBar_Clear( byval hTabBar as HWND )
declare sub      CTabBar_Refresh( byval hTabBar as HWND )

' ----------------------------------------------------------------------------------------
' Counts.  GetCount = every tab in the control.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetCount( byval hTabBar as HWND ) as integer

' ----------------------------------------------------------------------------------------
' tab contents.  Set* return FALSE for an invalid tab index.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetText( byval hTabBar as HWND, byval idx as integer ) as DWSTRING
declare function CTabBar_SetText( byval hTabBar as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function CTabBar_GetItemData( byval hTabBar as HWND, byval idx as integer ) as integer
declare function CTabBar_SetItemData( byval hTabBar as HWND, byval idx as integer, byval itemData as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Layout.
'
'   A tab is always at least as wide as its text plus its two icon rects and its border,
'   and NEVER shrinks:
'       width = max( text width + 2*icon width + 2*padding + 2*border, nMinWidth )
'
'   nMinWidth defaults to 0, so tabs auto-size. Set a floor on a tab that would otherwise
'   be uncomfortably narrow for its own text (a one-character filename), or on one that
'   must not resize as its text changes and shove its neighbours along.
'
'   When the tabs stop fitting, the rightmost clips at the client edge -- nothing shrinks.
'   Rects that run past the edge are computed honestly; clipping is the paint pass's job.
'   (A consequence worth knowing: a partially visible rightmost tab is clickable, but its
'   close rect can sit past the client edge and be unreachable. Documented, not fixed.)
'
'   Both icon rects (left icon, right close "X") are ALWAYS reserved, used or not, so the
'   caption sits at the same offset in every tab and never shifts when the "X" appears on
'   hover. Padding and icon width are LAYOUT inputs: draw your text between rcIcon.right
'   and rcClose.left (see GetTabParts) rather than re-deriving insets.
'
'   Rects are computed lazily. GetTabRect / GetTabParts force a layout if one is pending,
'   so they are always current. GetTabRect returns the FULL tab rect; GetTabParts returns
'   the derived interior rects (rcPaint pre-deflated by the border, rcText between the
'   icon rects, rcIcon, rcClose) -- the same rects the paint callback receives.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetTabMinWidth( byval hTabBar as HWND, byval idx as integer ) as integer
declare function CTabBar_SetTabMinWidth( byval hTabBar as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function CTabBar_GetPadding( byval hTabBar as HWND ) as integer
declare sub      CTabBar_SetPadding( byval hTabBar as HWND, byval nPadding as integer )
declare function CTabBar_GetIconWidth( byval hTabBar as HWND ) as integer
declare sub      CTabBar_SetIconWidth( byval hTabBar as HWND, byval nIconWidth as integer )
declare function CTabBar_GetTabRect( byval hTabBar as HWND, byval idx as integer, byref rc as RECT ) as boolean
declare function CTabBar_GetTabParts( byval hTabBar as HWND, byval idx as integer, byref rcPaint as RECT, byref rcText as RECT, byref rcIcon as RECT, byref rcClose as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Selection.  The control owns the current tab (a tab bar has one; a statusbar had none).
'
'   SetCurSel accepts -1 to clear. A valid index is scrolled into view on the next layout
'   (so tiko opening a file never selects a tab the user cannot see). Programmatic
'   SetCurSel does NOT fire the selection-change notification (Phase 4) -- only user
'   interaction does, following Win32's TCM_SETCURSEL / TCN_SELCHANGE precedent; this also
'   stops a host calling SetCurSel from inside its own handler from recursing.
'
'   Deleting the selected tab clears the selection: the control never guesses which
'   document the host wants next. Adding a tab does not select it, for the same reason.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetCurSel( byval hTabBar as HWND ) as integer
declare function CTabBar_SetCurSel( byval hTabBar as HWND, byval idx as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Navigation.  nFirstVisible is a listbox-style top index: whole tabs, never half-drawn
' at the left edge. Navigate steps it by one tab; CanNavigate says whether a step that
' way would move (left: not already at 0; right: the last tab still clips), so a nav
' control can grey its buttons out. A no-op Navigate is harmless and returns the current
' index. TCM_NAVIGATE / TCM_CANNAVIGATE (top of file) are the SendMessage doors to these
' same functions, for driving the bar from a separate control.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetFirstVisible( byval hTabBar as HWND ) as integer
declare function CTabBar_SetFirstVisible( byval hTabBar as HWND, byval idx as integer ) as boolean
declare function CTabBar_Navigate( byval hTabBar as HWND, byval nDirection as integer ) as integer
declare function CTabBar_CanNavigate( byval hTabBar as HWND, byval nDirection as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  The font is borrowed, never owned: keep it alive and destroy it yourself.
'
'   BorderColor/BorderWidth style the chrome the CONTROL draws (Phase 3): the full-width
'   bottom line, broken under the active tab, and the active tab's 3-sided border. The
'   border width is also a LAYOUT input -- it is what rcPaint is deflated by.
' ----------------------------------------------------------------------------------------
declare function CTabBar_GetBackColor( byval hTabBar as HWND ) as COLORREF
declare function CTabBar_SetBackColor( byval hTabBar as HWND, byval clr as COLORREF ) as COLORREF
declare function CTabBar_GetBorderColor( byval hTabBar as HWND ) as COLORREF
declare function CTabBar_SetBorderColor( byval hTabBar as HWND, byval clr as COLORREF ) as COLORREF
declare function CTabBar_GetBorderWidth( byval hTabBar as HWND ) as integer
declare sub      CTabBar_SetBorderWidth( byval hTabBar as HWND, byval nBorderWidth as integer )
declare function CTabBar_GetFont( byval hTabBar as HWND ) as HFONT
declare function CTabBar_SetFont( byval hTabBar as HWND, byval hFont as HFONT ) as boolean
declare sub      CTabBar_SetHoverTime( byval hTabBar as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback     - draw one tab. Required if you want to see any tabs (the control
'                       still paints its own background and chrome without one).
'   MessageCallback   - observe mouse messages; return TRUE to suppress default handling
'                       (IGNORED for WM_LBUTTONUP / WM_MBUTTONUP -- capture-critical).
'   TooltipCallback   - supply per-tab tooltip text on demand; "" for none.
'   SelChangeCallback - the USER changed the active tab. Programmatic SetCurSel is silent.
'   CloseCallback     - the user asked to close a tab (its "X", or a middle-click on it).
'                       Notify-only: the control never deletes. Prompt, veto, or call
'                       CTabBar_DeleteTab yourself.
'   ReorderCallback   - a drag moved a tab; one callback per live move, state already
'                       updated. Programmatic MoveTab is silent.
' ----------------------------------------------------------------------------------------
declare sub      CTabBar_SetPaintCallback( byval hTabBar as HWND, byval usersub as TB_PaintCallbackSub )
declare sub      CTabBar_SetMessageCallback( byval hTabBar as HWND, byval userfunc as TB_MessageCallbackFunc )
declare sub      CTabBar_SetTooltipCallback( byval hTabBar as HWND, byval userfunc as TB_TooltipCallbackFunc )
declare sub      CTabBar_SetSelChangeCallback( byval hTabBar as HWND, byval usersub as TB_SelChangeCallbackSub )
declare sub      CTabBar_SetCloseCallback( byval hTabBar as HWND, byval usersub as TB_CloseCallbackSub )
declare sub      CTabBar_SetReorderCallback( byval hTabBar as HWND, byval usersub as TB_ReorderCallbackSub )


