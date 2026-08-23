'    Owner-drawn column header control
'
'    A self-contained, per-instance header band in the same vein as PsStatusBar and
'    PsVScrollBar: it owns its own window class and keeps all state per instance in the
'    CWindow UserData area, so any number of instances can coexist. It is generic -- it
'    knows nothing about listboxes. PsListBox embeds one above its rows (the way it embeds
'    a PsVScrollBar on its right edge), but the control works standalone.
'
'    The control owns the column model (text, widths) and the derived geometry; all
'    rendering is the host's, via a per-column paint callback. Column widths are stored
'    in PIXELS (drag-resize deltas and autosize answers are pixels; round-tripping
'    through unscaled units would accumulate rounding drift). The control-owned defaults
'    below (padding, divider gutter, minimum width) are DPI-scaled at use.

#pragma once

#include once "PsBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer ids are per-window, so every
' instance can share this id.
#define IDT_CCOLHDR_HOTTRACK    &hCB30
#define CCOLHDR_HOTTRACK_MS     100

' Default horizontal inset for a column's caption. The control DPI-scales this at
' Create; PsColumnHeader_SetPadding takes raw pixels thereafter. Padding is paint-side
' only here (unlike PsStatusBar, widths are stored, not measured from text).
#define CCOLHDR_DEFAULT_PADDING 8

' Half-width of the divider hit zone: a boundary is grabbable within +/- this many
' pixels (DPI-scaled at use) of a column's right edge.
#define CCOLHDR_DIVIDER_GUTTER  3

' Floor applied to any column whose own nMinWidth is 0 (DPI-scaled at use). A column
' can never be dragged or laid out narrower than its effective minimum.
#define CCOLHDR_DEFAULT_MINWIDTH 16

' Fill-column designations for PsColumnHeader_SetFillColumn (besides an explicit index).
' The fill column absorbs whatever client width the fixed columns leave over -- it
' shrinks and grows with the window (never below its minimum width). Default is
' FILL_LAST: with a single column the cell spans the full width, and there is no dead
' strip right of the last column.
#define CCOLHDR_FILL_LAST      -2
#define CCOLHDR_FILL_NONE      -1

' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces
' errors that never name the real problem -- hence nMinWidth / nWidth throughout.
' NOTE: no isHot here. Hover is transient and single-valued, so the control's
' nLastHotIdx / nHotDividerIdx are the one source of truth.
type PSCOLUMNHEADER_COLINFO
    Text      as DWSTRING
    itemData  as integer
    nWidth    as integer = 100   ' stored width in PIXELS; the fill column's is derived
    nMinWidth as integer = 0     ' floor in pixels; 0 = scaled CCOLHDR_DEFAULT_MINWIDTH
    rc        as RECT            ' computed client coordinates (see LayoutColumns)
end type

type PSCOLUMNHEADER_PAINTINFO
    hHeader     as HWND                   ' the control, so the callback can query it
    itemID      as integer                ' column index
    b           as PsBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    rc          as RECT                   ' this column's rect, in client coordinates
    isHot       as boolean                ' mouse is over this column's body
    isPressed   as boolean                ' live left press on this column's body
    isResizeHot as boolean                ' cursor is in this column's right-divider gutter
    isResizing  as boolean                ' this column's divider is being dragged
    isFill      as boolean                ' this is the (effective) fill column
    wszCaption  as DWSTRING               ' the column's Text
end type

type PSCOLUMNHEADER_MESSAGEINFO
    hHeader as HWND
    uMsg    as UINT
    wParam  as WPARAM
    lParam  as LPARAM
    idx     as integer    ' column index under the mouse (-1 if none / on a divider)
end type

' Draw one column's header cell. Called for each column touched by the repaint; keep it
' cheap. Paint through p->b using p->rc as the cell rect -- do not touch the screen DC.
' Style from the state flags; the control decides them, you only render. Inset your
' caption by PsColumnHeader_GetPadding( p->hHeader ). Nothing but the control's flat
' BackColor is drawn if no paint callback is set.
type HDR_PaintCallbackSub as sub( byval p as PSCOLUMNHEADER_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed. The result is IGNORED for WM_LBUTTONUP:
' the control holds mouse capture across a press, and the up-message is the capture's
' exit -- a callback that suppressed it would strand the capture (see Learnings.md).
type HDR_MessageCallbackFunc as function( byval m as PSCOLUMNHEADER_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for a column, on demand (only when a tip is about to show).
' Return "" for no tooltip. If unset, the column's own Text is used.
type HDR_TooltipCallbackFunc as function( byval hHeader as HWND, byval idx as integer ) as DWSTRING

' A completed click on a column's BODY (matched press+release on the same column, not on
' a divider). This is the host's sorting hook: the control never sorts -- repaint a sort
' glyph from your paint callback and reorder your own data.
type HDR_ClickCallbackSub as sub( byval hHeader as HWND, byval idx as integer )

' A column's width changed through USER interaction. Fired per WM_MOUSEMOVE during a
' live divider drag with bLive = true, and once more with bLive = false when the drag
' commits (release), cancels (ESC / capture loss -- carrying the restored width), or an
' autosize applies. Programmatic PsColumnHeader_SetColumnWidth does NOT fire this (the
' family rule: programmatic setters are silent; only user interaction notifies).
type HDR_WidthChangedCallbackSub as sub( byval hHeader as HWND, byval idx as integer, byval nWidth as integer, byval bLive as boolean )

' Divider double-click: return the best-fit width in pixels for the column (the host
' owns the cell data and fonts, so only it can measure). Return <= 0 for "no change".
type HDR_AutoSizeCallbackFunc as function( byval hHeader as HWND, byval idx as integer ) as integer

type PSCOLUMNHEADER
    hWin              as HWND
    hToolTip          as HWND
    wszTooltip        as DWSTRING     ' instance-lifetime buffer TTN_GETDISPINFOW points at
    cols(any)         as PSCOLUMNHEADER_COLINFO
    colCount          as integer = 0
    HoverTime         as integer = 250
    nLastHotIdx       as integer = -1     ' column body the mouse was last over
    nHotDividerIdx    as integer = -1     ' column whose right-divider gutter is hovered
    hotTimerOn        as boolean = false  ' is the hot-tracking safety-net timer running?
    BackColor         as COLORREF
    ' Caller-supplied font (caller owns it). NOT named hFont: a member called hFont would
    ' shadow the TYPE name HFONT inside every member procedure (see Learnings.md).
    hHeaderFont       as HFONT
    ' Fill designation: CCOLHDR_FILL_LAST (default) / CCOLHDR_FILL_NONE / explicit index.
    ' A stored index gets the same insert/delete fixups as PsStatusBar's spring panel.
    fillCol           as integer = CCOLHDR_FILL_LAST
    ' Future horizontal scroll: every rect is computed at (runningX - xOffset). Always 0
    ' in v1; a PsHScrollBar can drive it later without any contract change.
    xOffset           as integer = 0
    nPadding          as integer = CCOLHDR_DEFAULT_PADDING   ' DPI-scaled at Create
    bLayoutDirty      as boolean = true
    ' --- Press / divider-drag state (mouse capture; PsTabBar discipline). The control
    '     takes capture on WM_LBUTTONDOWN because both gestures here consume the
    '     guaranteed down->up pairing: the drag obviously, and the body click's
    '     "press, slide off, release -> nothing" cancel semantics. ---
    nPressedIdx       as integer = -1     ' column body under a live left press
    nDragDividerIdx   as integer = -1     ' column whose divider is being dragged
    nDragOrigWidth    as integer = 0      ' stored width at drag start; ESC restores this
    nDragAnchorX      as integer = 0      ' press x; drag delta = cursor x - anchor
    bDragCancelled    as boolean = false  ' ESC consumed the drag; ignore the trailing up
    PaintCallback        as HDR_PaintCallbackSub
    MessageCallback      as HDR_MessageCallbackFunc
    TooltipCallback      as HDR_TooltipCallbackFunc      ' optional; defaults to Text
    ClickCallback        as HDR_ClickCallbackSub
    WidthChangedCallback as HDR_WidthChangedCallbackSub
    AutoSizeCallback     as HDR_AutoSizeCallbackFunc

    declare sub      LayoutColumns()
    declare function EffectiveFillCol() as integer
    declare function EffectiveMinWidth( byval idx as integer ) as integer
    declare function GetCount() as integer
    declare function InsertColumnAt( byval idx as integer ) as PSCOLUMNHEADER_COLINFO ptr
    declare function AddColumn() as PSCOLUMNHEADER_COLINFO ptr
    declare function GetColumn( byval idx as integer ) as PSCOLUMNHEADER_COLINFO ptr
    declare function DeleteColumnAt( byval idx as integer ) as boolean
    declare function IsValidColumn( byval idx as integer ) as boolean
    declare function HitTestColumn( byval x as integer, byval y as integer ) as integer
    declare function HitTestDivider( byval x as integer, byval y as integer ) as integer
    declare sub      CancelPress()
    declare sub      Clear()
    declare sub      Refresh()
end type

' Which column is the (effective) fill column right now? Resolves the FILL_LAST
' designation against the current count. -1 = none.
function PSCOLUMNHEADER.EffectiveFillCol() as integer
    if this.fillCol = CCOLHDR_FILL_NONE then return -1
    if this.fillCol = CCOLHDR_FILL_LAST then
        if this.colCount > 0 then return this.colCount - 1
        return -1
    end if
    if this.IsValidColumn( this.fillCol ) then return this.fillCol
    return -1
end function

' The floor a column can never be laid out or dragged below: its own nMinWidth, or the
' DPI-scaled control default when that is 0.
function PSCOLUMNHEADER.EffectiveMinWidth( byval idx as integer ) as integer
    dim as integer minW = 0
    if this.IsValidColumn( idx ) then minW = this.cols(idx).nMinWidth
    if minW <= 0 then
        minW = CCOLHDR_DEFAULT_MINWIDTH
        dim pWindow as CWindow ptr = AfxCWindowPtr( this.hWin )
        if pWindow then minW = pWindow->ScaleX( CCOLHDR_DEFAULT_MINWIDTH )
    end if
    return minW
end function

' Assign every column's rect. This is the only place column geometry is produced;
' everything else (hit-testing, painting, invalidation, an embedding host's row cells)
' consumes it. Widths are stored, not measured: each fixed column gets
' max( nWidth, EffectiveMinWidth ), and the fill column absorbs the leftover client
' width -- shrinking as well as growing (unlike PsStatusBar's grow-only spring), because
' a fill column must track the window both ways -- but never below its own minimum.
' On overflow the rects run honestly past the right edge; clipping is the paint pass's
' job, and the cursor cannot reach past the edge, so hit-testing stays correct for free.
sub PSCOLUMNHEADER.LayoutColumns()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub
    if this.colCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as integer clientW = rcClient.right - rcClient.left
    dim as integer clientH = rcClient.bottom - rcClient.top
    ' No geometry yet (created 0x0, sized only once the host positions us). Nothing to
    ' lay out; stay dirty so the next query after a real WM_SIZE recomputes.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    dim as integer effFill = this.EffectiveFillCol()

    dim colW( 0 to this.colCount - 1 ) as integer
    dim as integer sumOthers = 0
    for i as integer = 0 to this.colCount - 1
        if i = effFill then continue for
        dim as integer nWidth = this.cols(i).nWidth
        dim as integer minW = this.EffectiveMinWidth(i)
        if nWidth < minW then nWidth = minW
        colW(i) = nWidth
        sumOthers += nWidth
    next
    if effFill <> -1 then
        dim as integer nWidth = clientW - sumOthers
        dim as integer minW = this.EffectiveMinWidth(effFill)
        if nWidth < minW then nWidth = minW
        colW(effFill) = nWidth
    end if

    ' Position left-to-right; every column spans the full client height. xOffset shifts
    ' the whole run left once horizontal scrolling exists (always 0 in v1).
    dim as integer x = rcClient.left - this.xOffset
    for i as integer = 0 to this.colCount - 1
        SetRect( @this.cols(i).rc, x, rcClient.top, x + colW(i), rcClient.bottom )
        x += colW(i)
    next
end sub

function PSCOLUMNHEADER.GetCount() as integer
    return this.colCount
end function

function PSCOLUMNHEADER.IsValidColumn( byval idx as integer ) as boolean
    return (idx >= 0) andalso (idx < this.colCount)
end function

function PSCOLUMNHEADER.GetColumn( byval idx as integer ) as PSCOLUMNHEADER_COLINFO ptr
    if this.IsValidColumn(idx) = false then return 0
    return @this.cols(idx)
end function

' Insert a fresh (reset) column at idx, shifting later columns up. Grows the backing
' store by doubling so bulk inserts are amortized O(1), not O(n^2).
function PSCOLUMNHEADER.InsertColumnAt( byval idx as integer ) as PSCOLUMNHEADER_COLINFO ptr
    if idx < 0 then idx = 0
    if idx > this.colCount then idx = this.colCount

    dim as integer cap = ubound(this.cols) + 1
    if this.colCount >= cap then
        dim as integer newcap = iif( cap = 0, 8, cap * 2 )
        redim preserve this.cols( 0 to newcap - 1 )
    end if

    ' shift [idx .. colCount-1] up by one (no-op when appending)
    for i as integer = this.colCount to idx + 1 step -1
        this.cols(i) = this.cols(i - 1)
    next

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.cols(idx)
        .Text      = ""
        .itemData  = 0
        .nWidth    = 100
        .nMinWidth = 0
        SetRectEmpty( @.rc )
    end with

    this.colCount += 1
    ' A stored fill index shifts up when inserting at or before it.
    if (this.fillCol >= 0) andalso (idx <= this.fillCol) then
        this.fillCol += 1
    end if
    ' Hover/press indices now point at shifted columns; a live gesture's target is
    ' ambiguous, so cancel rather than guess.
    this.CancelPress()
    this.nLastHotIdx = -1
    this.nHotDividerIdx = -1
    this.Refresh()
    return @this.cols(idx)
end function

function PSCOLUMNHEADER.AddColumn() as PSCOLUMNHEADER_COLINFO ptr
    return this.InsertColumnAt( this.colCount )
end function

function PSCOLUMNHEADER.DeleteColumnAt( byval idx as integer ) as boolean
    if this.IsValidColumn(idx) = false then return false
    ' shift [idx+1 .. colCount-1] down by one
    for i as integer = idx to this.colCount - 2
        this.cols(i) = this.cols(i + 1)
    next
    this.cols(this.colCount - 1).Text = ""   ' free the vacated last slot's string
    this.colCount -= 1
    ' Keep a stored fill index meaningful: deleting the fill column itself falls back to
    ' the default (last column); deleting anything before it shifts it down.
    if this.fillCol >= 0 then
        if idx = this.fillCol then
            this.fillCol = CCOLHDR_FILL_LAST
        elseif idx < this.fillCol then
            this.fillCol -= 1
        end if
    end if
    this.CancelPress()
    this.nLastHotIdx = -1
    this.nHotDividerIdx = -1
    this.Refresh()
    return true
end function

sub PSCOLUMNHEADER.Clear()
    for i as integer = 0 to this.colCount - 1
        this.cols(i).Text = ""
    next
    this.colCount = 0
    this.fillCol  = CCOLHDR_FILL_LAST
    this.nLastHotIdx = -1
    this.nHotDividerIdx = -1
    this.CancelPress()
    this.Refresh()
end sub

' Which column's right-divider gutter is (x, y) in? Returns the column being resized by
' that divider, or -1. Iterates right-to-left so that when a column sits at its minimum
' width and two boundaries nearly coincide, the later divider wins (matches listview
' feel). The effective fill column's own divider is NOT grabbable: its width is derived,
' and with no horizontal scroll there is nothing meaningful to drag it to.
function PSCOLUMNHEADER.HitTestDivider( byval x as integer, byval y as integer ) as integer
    if this.colCount = 0 then return -1
    if this.bLayoutDirty then this.LayoutColumns()
    dim as integer gutter = CCOLHDR_DIVIDER_GUTTER
    dim pWindow as CWindow ptr = AfxCWindowPtr( this.hWin )
    if pWindow then gutter = pWindow->ScaleX( CCOLHDR_DIVIDER_GUTTER )
    dim as integer effFill = this.EffectiveFillCol()
    for i as integer = this.colCount - 1 to 0 step -1
        if i = effFill then continue for
        dim as integer bx = this.cols(i).rc.right
        if abs(x - bx) <= gutter then return i
    next
    return -1
end function

' Which column's BODY is (x, y) over? Divider gutters take priority: on a gutter this
' returns -1 (the gesture there is a resize, not a column press).
function PSCOLUMNHEADER.HitTestColumn( byval x as integer, byval y as integer ) as integer
    if this.colCount = 0 then return -1
    if this.bLayoutDirty then this.LayoutColumns()
    if this.HitTestDivider( x, y ) <> -1 then return -1
    dim as POINT pt
    pt.x = x : pt.y = y
    for i as integer = 0 to this.colCount - 1
        if PtInRect( @this.cols(i).rc, pt ) then return i
    next
    return -1
end function

' Abandon any live press or divider drag WITHOUT firing callbacks or restoring widths --
' this is the "state is gone" path (column mutated under a gesture, window destroyed).
' Interactive cancel/commit paths (up, ESC, WM_CAPTURECHANGED) have their own handlers.
sub PSCOLUMNHEADER.CancelPress()
    this.nPressedIdx = -1
    this.nDragDividerIdx = -1
    this.bDragCancelled = false
    if (this.hWin <> 0) andalso (GetCapture() = this.hWin) then ReleaseCapture()
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which
' is what makes layout lazy: a burst of width changes costs one layout pass, not N.
sub PSCOLUMNHEADER.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking column is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
'
' THE CONTROL HANDLE
'   Every PsColumnHeader_* function takes the handle returned by PsColumnHeader_Create().
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need
'   to treat the control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized: position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function PsColumnHeader_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing columns. Add returns the new column's index, Insert the actual index
' used (clamped to [0, count]); both -1 on failure. Widths are pixels. nMinWidth 0 means
' the scaled CCOLHDR_DEFAULT_MINWIDTH floor applies.
' ----------------------------------------------------------------------------------------
declare function PsColumnHeader_AddColumn( byval hHeader as HWND, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsColumnHeader_InsertColumn( byval hHeader as HWND, byval idx as integer, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsColumnHeader_DeleteColumn( byval hHeader as HWND, byval idx as integer ) as boolean
declare sub      PsColumnHeader_Clear( byval hHeader as HWND )
declare function PsColumnHeader_GetCount( byval hHeader as HWND ) as integer
declare sub      PsColumnHeader_Refresh( byval hHeader as HWND )

' ----------------------------------------------------------------------------------------
' Column contents. Set* return FALSE for an invalid column index.
' ----------------------------------------------------------------------------------------
declare function PsColumnHeader_GetColumnText( byval hHeader as HWND, byval idx as integer ) as DWSTRING
declare function PsColumnHeader_SetColumnText( byval hHeader as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function PsColumnHeader_GetColumnItemData( byval hHeader as HWND, byval idx as integer ) as integer
declare function PsColumnHeader_SetColumnItemData( byval hHeader as HWND, byval idx as integer, byval itemData as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Layout.
'
'   Fixed columns render at max( nWidth, effective minimum ); the single fill column
'   absorbs the leftover client width, shrinking and growing with the window but never
'   below its minimum. SetFillColumn takes an index, CCOLHDR_FILL_LAST (default), or
'   CCOLHDR_FILL_NONE; GetFillColumn returns the resolved index (-1 = none).
'
'   SetColumnWidth is the programmatic door: it stores the width silently (no
'   WidthChanged callback -- that is reserved for user drags). Setting the fill
'   column's width stores it but has no visual effect until it stops being the fill.
'
'   Rects are computed lazily. GetColumnRect forces a pending layout, so it is always
'   current -- an embedding host reads row-cell x-coordinates from it.
'
'   HitTestDivider (client coords) is public mainly for self-tests: it returns the
'   column whose right-edge divider gutter contains the point, -1 otherwise.
' ----------------------------------------------------------------------------------------
declare function PsColumnHeader_GetColumnWidth( byval hHeader as HWND, byval idx as integer ) as integer
declare function PsColumnHeader_SetColumnWidth( byval hHeader as HWND, byval idx as integer, byval nWidth as integer ) as boolean
declare function PsColumnHeader_GetColumnMinWidth( byval hHeader as HWND, byval idx as integer ) as integer
declare function PsColumnHeader_SetColumnMinWidth( byval hHeader as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function PsColumnHeader_GetFillColumn( byval hHeader as HWND ) as integer
declare function PsColumnHeader_SetFillColumn( byval hHeader as HWND, byval idx as integer ) as boolean
declare function PsColumnHeader_GetColumnRect( byval hHeader as HWND, byval idx as integer, byref rc as RECT ) as boolean
declare function PsColumnHeader_HitTestDivider( byval hHeader as HWND, byval x as integer, byval y as integer ) as integer
declare function PsColumnHeader_GetPadding( byval hHeader as HWND ) as integer
declare sub      PsColumnHeader_SetPadding( byval hHeader as HWND, byval nPadding as integer )
declare sub      PsColumnHeader_SetXOffset( byval hHeader as HWND, byval xOffset as integer )

' ----------------------------------------------------------------------------------------
' Appearance. The font is borrowed, never owned: keep it alive and destroy it yourself.
' ----------------------------------------------------------------------------------------
declare function PsColumnHeader_GetBackColor( byval hHeader as HWND ) as COLORREF
declare function PsColumnHeader_SetBackColor( byval hHeader as HWND, byval clr as COLORREF ) as COLORREF
declare function PsColumnHeader_GetFont( byval hHeader as HWND ) as HFONT
declare function PsColumnHeader_SetFont( byval hHeader as HWND, byval hFont as HFONT ) as boolean
declare function PsColumnHeader_GetTooltipHandle( byval hHeader as HWND ) as HWND
declare sub      PsColumnHeader_SetHoverTime( byval hHeader as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Callbacks. See the type declarations above for each signature and contract.
'   PaintCallback        - draw one column's header cell. Required to see anything.
'   MessageCallback      - observe mouse messages; TRUE suppresses default handling
'                          (result ignored for WM_LBUTTONUP -- capture discipline).
'   TooltipCallback      - per-column tooltip text on demand; "" for none.
'   ClickCallback        - completed click on a column body (the sorting hook).
'   WidthChangedCallback - user resize: live per move, final on commit/cancel/autosize.
'   AutoSizeCallback     - divider double-click: return best-fit pixels, <= 0 = ignore.
' ----------------------------------------------------------------------------------------
declare sub      PsColumnHeader_SetPaintCallback( byval hHeader as HWND, byval usersub as HDR_PaintCallbackSub )
declare sub      PsColumnHeader_SetMessageCallback( byval hHeader as HWND, byval userfunc as HDR_MessageCallbackFunc )
declare sub      PsColumnHeader_SetTooltipCallback( byval hHeader as HWND, byval userfunc as HDR_TooltipCallbackFunc )
declare sub      PsColumnHeader_SetClickCallback( byval hHeader as HWND, byval usersub as HDR_ClickCallbackSub )
declare sub      PsColumnHeader_SetWidthChangedCallback( byval hHeader as HWND, byval usersub as HDR_WidthChangedCallbackSub )
declare sub      PsColumnHeader_SetAutoSizeCallback( byval hHeader as HWND, byval userfunc as HDR_AutoSizeCallbackFunc )
