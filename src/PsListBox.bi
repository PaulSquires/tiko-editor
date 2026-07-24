
#pragma once

#include once "PsBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id. Value is deliberately unusual to avoid colliding with
' any timer the standard listbox uses internally.
#define IDT_CLISTBOX_HOTTRACK   &hCB01
#define PSLISTBOX_HOTTRACK_MS    100

' Auto-scroll timer used ONLY while a drag-reorder is in progress: when the cursor is held
' near the top/bottom edge the list scrolls a row at a time so off-screen drop targets are
' reachable. Separate id from the hot-track timer so the two never step on each other.
#define IDT_CLISTBOX_DRAGSCROLL &hCB02
#define PSLISTBOX_DRAGSCROLL_MS   60
' How close to the top/bottom edge (in pixels) the cursor must be to trigger auto-scroll.
#define PSLISTBOX_DRAGSCROLL_ZONE 18

' One cell of a multi-column row, as handed to the paint callback. The rect is in the
' row buffer's coordinate space (y spans 0..row height; x comes from the header's
' column geometry, which maps 1:1 onto the row -- see PsListBox_PositionWindows).
type PSLISTBOX_CELLINFO
    iCol            as integer
    rc              as RECT
    wszText         as DWSTRING
end type

type PSLISTBOX_PAINTINFO
    itemID          as integer                ' MODEL row index (not the visible/listbox index)
    b               as PsBufferPaint ptr    ' points to the caller's buffer (no copy)
    rc              as RECT
    isHot           as boolean                ' mouse is hovering this row
    isSelected      as boolean                ' row is part of the selection
    isFocused       as boolean                ' row has the keyboard focus (caret)
    isHeader        as boolean                ' this row is a group header
    isCollapsed     as boolean                ' header only: its child items are hidden
    isSpanned       as boolean                ' this row spans all columns (see below)
    isSelectable    as boolean                ' FALSE = host made the row non-selectable; the
                                              ' control blocks selection/focus, the painter may
                                              ' grey it (cosmetic only -- not the enforcement)
    wszCaption      as DWSTRING
    ' --- Columns. 0/null whenever the row should paint as a single full-width cell:
    '     no columns are defined, or this row is a group header (group headers span).
    '     With columns: background-fill the FULL rc first (selection/hot spans the
    '     whole row, listview-style), then draw each cells[i].wszText inside
    '     cells[i].rc -- the control does not clip between cells (DT_END_ELLIPSIS is
    '     your friend). The array is control-owned scratch, valid ONLY during the
    '     callback -- copy anything you need to keep. ---
    '
    '     SPANNED rows (isSpanned = true, an ordinary selectable row flagged with
    '     PsListBox_SetRowSpanColumns) collapse to columnCount = 1 whenever columns are
    '     defined: cells[0].wszText is column 0's text and cells[0].rc runs the whole
    '     column band -- column 0's own left (the same left a normal column-0 cell gets,
    '     so the spanned text lines up under column 0) to the last column's right -- so
    '     the callback's existing `for c = 0 to columnCount-1` loop draws it as one wide
    '     cell with no new branch. Selection/hot still fill the full rc. When no
    '     columns are defined the row is already full-width (columnCount = 0) and the
    '     flag only advertises the intent. Group headers are NOT spanned rows -- they
    '     have their own isHeader path and carry isSpanned = false.
    columnCount     as integer
    cells           as PSLISTBOX_CELLINFO ptr
end type

type PSLISTBOX_MESSAGEINFO
    hList           as HWND
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
    idx             as integer    ' MODEL row index under the mouse (-1 if none)
    isCtrl          as boolean
    isShift         as boolean
end type


type PSLISTBOX_ROWINFO
    IsHeader        as boolean = false
    bCollapsed      as boolean = false
    bSpanColumns    as boolean = false    ' paint this (ordinary, selectable) row as one cell
                                          ' spanning every column; text stays in column 0
    bSelectable     as boolean = true     ' false = the row cannot be selected or focused and
                                          ' keyboard navigation skips over it (enforced on every
                                          ' path, programmatic setters included)
    selected        as boolean = false    ' selection is stored in the model, not the listbox
    Text            as DWSTRING           ' column 0's cell text (the pre-columns contract)
    ' Columns 1..N: cells(c-1) holds column c's text. SPARSE storage, independent of
    ' the column definitions -- grown lazily by SetCellText, and any cell past the
    ' stored count reads back "" (so populate-then-define and define-then-populate
    ' both work, and columns added late simply read empty until set).
    cells(any)      as DWSTRING
    itemData        as integer
    itemDataExtra   as integer

    declare sub EnsureCells( byval n as integer )
    declare sub TrimCells( byval n as integer )
end type

' fbc cannot parse `redim rows(i).cells(...)` (a member array reached through an array
' element); a member procedure redim'ing `this.cells` parses fine -- see Learnings.md.
sub PSLISTBOX_ROWINFO.EnsureCells( byval n as integer )
    if n <= 0 then exit sub
    if ubound(this.cells) < n - 1 then redim preserve this.cells( 0 to n - 1 )
end sub

' Shrink storage to exactly n cells; n <= 0 frees the array.
sub PSLISTBOX_ROWINFO.TrimCells( byval n as integer )
    if n <= 0 then
        erase this.cells
    elseif ubound(this.cells) >= n then
        redim preserve this.cells( 0 to n - 1 )
    end if
end sub

' Draw one row. Called for each visible row on every repaint; keep it cheap. Paint through
' p->b (a per-control buffer that is already clipped and offset to this row), using p->rc
' as the row rect -- do not touch the screen DC. Style from the state flags; the control
' decides them, you only render. Nothing is drawn if no paint callback is set.
type PaintCallbackSub as sub( byval p as PSLISTBOX_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the default listbox
' handling suppressed, FALSE to let it proceed.
' CAUTION: for WM_LBUTTONUP the result is IGNORED and the default always runs -- the
' listbox releases its mouse capture there, and swallowing it strands the capture.
type MessageCallbackFunc as function( byval m as PSLISTBOX_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for a MODEL row, on demand (only when a tip is about to show).
' Return "" for no tooltip. If unset, the row's own Text is used.
type TooltipCallbackFunc as function( byval hListControl as HWND, byval row as integer ) as DWSTRING

' The USER changed the selection -- by clicking a row, or by moving the focus with the
' keyboard (arrows, PageUp/Down, Home/End, Space in checklist mode). row is the MODEL index
' of the newly focused row, or -1 if there is no focus row.
'
' Why this exists: the control handles keyboard navigation itself and WM_KEYDOWN never
' reaches the message callback, so without this a host cannot tell that an arrow key moved
' the selection. Mouse-only hosts can keep using WM_LBUTTONUP in the message callback.
'
' NOT fired for PsListBox_SetCurSel / SetSel / SelectAll / Clear: programmatic setters are
' silent (the family rule), so a host may call them from inside this callback without
' re-entering itself. Nor is it fired when the user re-selects the row that is already
' current -- only an actual change notifies.
type SelChangeCallbackSub as sub( byval hListControl as HWND, byval row as integer )

' ----------------------------------------------------------------------------------------
' Drag-and-drop row reordering (opt-in via PsListBox_SetDragReorder). The user drags a row,
' or the whole current selection, to a new position; dropping ON a header inserts the block
' as that header's first children. The control moves its OWN rows() model and notifies.
' ----------------------------------------------------------------------------------------

' Handed to the CanDrop veto just before a drop commits. Everything here is a SNAPSHOT for
' the duration of the callback -- srcRows points at control-owned scratch and targetInfo at
' a live ROWINFO; copy anything you need to keep, and do not mutate the model from inside.
type PSLISTBOX_DROPINFO
    hList        as HWND
    srcRows      as integer ptr           ' the MODEL indices being dragged (srcCount of them)
    srcCount     as integer
    insertBefore as integer               ' MODEL index the block will land before (0..rowCount)
    onHeader     as boolean               ' TRUE = dropped on a header (block becomes its first children)
    targetRow    as integer               ' MODEL index of the drop-target row, or -1 at the list end
    targetInfo   as PSLISTBOX_ROWINFO ptr ' the target row's ROWINFO (read-only), or NULL at the end
end type

' Fired just BEFORE the move commits. Return FALSE to reject the drop (the model is left
' untouched). Unset = every drop is allowed.
type CanDropCallbackFunc as function( byval p as PSLISTBOX_DROPINFO ptr ) as boolean

' Fired just AFTER a user drag-drop moves rows. newFirstRow is the MODEL index the moved
' block now starts at; count is how many rows moved. Lets a host resync any parallel data
' (walk newFirstRow..newFirstRow+count-1 with the public getters / itemData). NOT fired for
' the programmatic PsListBox_MoveRows -- setters are silent (the family rule).
type DragDropCallbackSub as sub( byval hList as HWND, byval newFirstRow as integer, byval count as integer )

type PSLISTBOX
    hWin            as HWND
    hToolTip        as HWND
    wszTooltip      as DWSTRING
    ' --- Model: rows() is the backing store (capacity = ubound+1); rowCount is
    '     the number of logical rows and the single source of truth for "count". ---
    rows(any)       as PSLISTBOX_ROWINFO
    rowCount        as integer = 0
    ' --- View: visibleMap(v) -> model row index, for v = 0..visibleCount-1.
    '     Rebuilt on any change / collapse-expand. ---
    visibleMap(any) as integer
    visibleCount    as integer = 0
    updateDepth     as integer = 0        ' BeginUpdate/EndUpdate nesting (defers refresh)
    RowHeight       as integer = 22
    idc_ListBox     as integer = 1000
    accumDelta      as integer = 0        ' mousewheel
    HoverTime       as integer = 250
    nLastHotIdx     as integer = -1       ' last VISIBLE row the mouse was over (hover tracking)
    hotTimerOn      as boolean = false    ' is the hot-tracking safety-net timer running?
    focusRow        as integer = -1       ' MODEL row with the keyboard focus/caret (-1 = none)
    ' Last row handed to SelChangeCallback -- the host's idea of "current". Programmatic
    ' setters update it WITHOUT notifying, which is what keeps a later user click on that
    ' same row correctly silent.
    lastNotifiedRow as integer = -1
    anchorRow       as integer = -1       ' MODEL row anchoring a shift-range selection
    topRow          as integer = 0        ' MODEL row that should be first displayed; the scroll
                                          ' source of truth the VIEW is re-derived from
    ' --- Scroll and metrics, formerly held by the Win32 listbox child. ---
    ' The first VISIBLE row on screen. topRow (model) stays the source of truth that
    ' survives a rebuild -- a collapsed group can move a model row to a different visible
    ' slot -- and this is what the paint loop and hit-testing actually walk. The two are
    ' reconciled in PsListBox_SyncViewFromModel, exactly where LB_SETTOPINDEX used to be.
    nTopVis         as integer = 0
    ' Row height in PIXELS, already DPI-scaled. RowHeight above is in unscaled units, the
    ' way every host sets it; this is the scaled value the geometry uses, computed once at
    ' creation and whenever the font or row height changes. It was LB_GETITEMHEIGHT.
    nItemHeightPx   as integer = 0
    ExtendSel       as boolean = false
    MultipleSel     as boolean = false
    PreventDblClick as boolean = false    ' host opt-out of double-click: the dblclk becomes a
                                          ' plain click and its trailing second up is not forwarded
    skipNextLBtnUp  as boolean = false    ' armed by a suppressed dblclk; consumed by the next
                                          ' WM_LBUTTONUP, disarmed by any fresh WM_LBUTTONDOWN
    ' --- Drag-and-drop reordering (opt-in). The gesture is the ONLY thing in this control
    '     that takes mouse capture, and it does so only while a drag is actually active. ---
    bDragReorder    as boolean = false    ' host opt-in; nothing below runs when false
    bDragArmed      as boolean = false    ' a press landed on a draggable row, awaiting the threshold
    bDragActive     as boolean = false    ' past the threshold: capture held, indicator painting
    ptDragStart     as POINT              ' client-coord press point (drag threshold origin)
    dragAnchorRow   as integer = -1       ' MODEL row under the initial press (source of the drag)
    dragPendCollapse as integer = -1      ' MODEL row whose single-select collapse was deferred to
                                          ' the up (so pressing a selected row can start a group drag)
    dropInsertBefore as integer = -1      ' current target: MODEL index the block would land before
    dropOnHeader    as boolean = false    ' current target is a header (insert as its first children)
    dropTargetRow   as integer = -1       ' current target: MODEL index of the drop-on row (-1 = end)
    dragTimerOn     as boolean = false    ' is the auto-scroll timer running?
    dragIndicatorColor as COLORREF = &h00D77800  ' insertion line / header highlight (accent blue; SetDragIndicatorColor overrides)
    BackColor       as COLORREF
    hFont           as HFONT              ' caller-supplied font for row text (caller owns it)
    ' --- Owner-drawn vertical scrollbar, created and driven by this control. It is
    '     auto-hidden whenever the visible rows fit, and the listbox then reclaims the
    '     full client width. ---
    hScrollBar      as HWND
    ScrollBarWidth  as integer = CVSCROLL_DEFAULT_WIDTH   ' DPI-scaled when laid out
    scrollBarShown  as boolean = false
    ' --- Optional column header band (PsColumnHeader), created hidden and owned by this
    '     control. The embedded header instance is the SINGLE store for column
    '     definitions and geometry -- PsListBox keeps no column state; the PsListBox_*
    '     column wrappers delegate, and OnDrawItem reads cell x-coordinates from the
    '     header's rects. The control owns the header's WidthChanged slot (an internal
    '     chain hook); hosts use PsListBox_SetColumnResizeCallback. ---
    hHeader         as HWND
    headerShown     as boolean = false
    HeaderHeight    as integer = 24       ' unscaled units (like RowHeight); ScaleY at layout
    ColumnResizeCallback as HDR_WidthChangedCallbackSub   ' host-facing re-broadcast
    PaintCallback   as PaintCallbackSub
    MessageCallback as MessageCallbackFunc
    TooltipCallback as TooltipCallbackFunc    ' optional; defaults to the row's Text
    SelChangeCallback as SelChangeCallbackSub ' optional; user-driven selection changes only
    CanDropCallback   as CanDropCallbackFunc  ' optional; pre-drop veto (drag reorder)
    DragDropCallback  as DragDropCallbackSub  ' optional; post-drop notify (drag reorder)
    ' --- Persistent scratch for PAINTINFO.cells, re-dimensioned only when the column
    '     count changes (the paint loop is strictly sequential and single-threaded, so
    '     one array serves every row; the pointer handed out is valid only for the
    '     duration of each callback). ---
    paintCells(any) as PSLISTBOX_CELLINFO
    ' Single-cell scratch for a spanned row (isSpanned). Kept separate from paintCells so
    ' a spanned row never overwrites the shared column x-geometry that later normal rows
    ' in the same repaint still read. Valid only for the duration of one callback.
    spanCell        as PSLISTBOX_CELLINFO

    declare destructor()
    declare sub      EnsurePaintCells( byval n as integer )
    declare function GetCount() as integer                                  ' model row count
    declare function GetVisibleCount() as integer
    declare function AddRow() as PSLISTBOX_ROWINFO ptr                       ' append
    declare function InsertRowAt( byval modelRow as integer ) as PSLISTBOX_ROWINFO ptr
    declare function DeleteRowAt( byval modelRow as integer ) as boolean
    declare sub      Clear()
    declare function GetRow( byval row as integer ) as PSLISTBOX_ROWINFO ptr
    declare function IsValidRow( byval row as integer ) as boolean
    declare function ModelToVisible( byval modelRow as integer ) as integer ' -1 if hidden/invalid
    declare function VisibleToModel( byval visRow as integer ) as integer   ' -1 if invalid
    declare function IsRowSelected( byval modelRow as integer ) as boolean
    declare function IsRowSelectable( byval modelRow as integer ) as boolean
    declare sub      SetRowSelected( byval modelRow as integer, byval state as boolean )
    declare sub      ClearSelection()
    declare sub      SelectOnly( byval modelRow as integer )
    declare sub      SelectRange( byval a as integer, byval b as integer )
    declare function GetSelCount() as integer
    declare function MoveRows( srcRows() as integer, byval insertBefore as integer ) as integer
    declare sub      RebuildVisibleMap()
    declare sub      BeginUpdate()
    declare sub      EndUpdate()
    declare sub      NotifyChange()
    declare sub      Refresh()
end type

' Defined in PsListBox.inc, but PSLISTBOX.Refresh (below) has to call it -- push the
' scrollbar's range/visibility to match the current model + scroll position.
declare sub      PsListBox_SyncScrollBar( byval pList as PSLISTBOX ptr )
declare sub      PsListBox_SyncViewFromModel( byval pList as PSLISTBOX ptr )
declare sub      PsListBox_CaptureTopRow( byval pList as PSLISTBOX ptr )
declare function PsListBox_ItemsPerPage( byval pList as PSLISTBOX ptr ) as integer
declare function PsListBox_PositionWindows( byval hwnd as HWND ) as LRESULT

' The one-row back buffer (EnsureCache / FreeCache and its five fields) is GONE. It existed
' solely so WM_DRAWITEM would not create and destroy a compatible DC and bitmap for every
' row on every repaint. With the whole surface painted in a single buffer there is no
' per-row buffer to cache, and its disappearance is a good part of why the rewrite made
' the GDI and GDI+ backends faster too.
destructor PSLISTBOX()
end destructor

' Size the PAINTINFO.cells scratch to exactly n entries (only re-dims when the column
' count actually changed, so the per-row cost is a compare).
sub PSLISTBOX.EnsurePaintCells( byval n as integer )
    if n <= 0 then
        erase this.paintCells
    elseif ubound(this.paintCells) + 1 <> n then
        redim this.paintCells( 0 to n - 1 )
    end if
end sub

function PSLISTBOX.GetCount() as integer
    return this.rowCount
end function

function PSLISTBOX.GetVisibleCount() as integer
    return this.visibleCount
end function

function PSLISTBOX.IsValidRow( byval row as integer ) as boolean
    return (row >= 0) andalso (row < this.rowCount)
end function

function PSLISTBOX.GetRow( byval row as integer ) as PSLISTBOX_ROWINFO ptr
    if this.IsValidRow(row) = false then return null
    return @this.rows(row)
end function

' Insert a fresh (reset) row at modelRow, shifting later rows up. Grows the
' backing store by doubling so bulk inserts are amortized O(1), not O(n^2).
function PSLISTBOX.InsertRowAt( byval modelRow as integer ) as PSLISTBOX_ROWINFO ptr
    if modelRow < 0 then modelRow = 0
    if modelRow > this.rowCount then modelRow = this.rowCount

    dim as integer cap = ubound(this.rows) + 1
    if this.rowCount >= cap then
        dim as integer newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.rows( 0 to newcap - 1 )
    end if

    ' shift [modelRow .. rowCount-1] up by one (no-op when appending)
    for i as integer = this.rowCount to modelRow + 1 step -1
        this.rows(i) = this.rows(i - 1)
    next

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot;
    ' .selected included, else a Clear+repopulate resurrects the old contents'
    ' selection at whatever rows happen to land on the recycled indices)
    with this.rows(modelRow)
        .IsHeader      = false
        .bCollapsed    = false
        .bSpanColumns  = false
        .bSelectable   = true
        .selected      = false
        .Text          = ""
        .itemData      = 0
        .itemDataExtra = 0
    end with
    this.rows(modelRow).TrimCells( 0 )   ' recycled slot: stale cells must not resurrect

    this.rowCount += 1
    this.NotifyChange()
    return @this.rows(modelRow)
end function

function PSLISTBOX.AddRow() as PSLISTBOX_ROWINFO ptr
    return this.InsertRowAt( this.rowCount )
end function

function PSLISTBOX.DeleteRowAt( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    ' shift [modelRow+1 .. rowCount-1] down by one
    for i as integer = modelRow to this.rowCount - 2
        this.rows(i) = this.rows(i + 1)
    next
    this.rows(this.rowCount - 1).Text = ""       ' free the vacated last slot's strings
    this.rows(this.rowCount - 1).TrimCells( 0 )
    this.rowCount -= 1
    this.NotifyChange()
    return true
end function

sub PSLISTBOX.Clear()
    for i as integer = 0 to this.rowCount - 1
        this.rows(i).Text = ""
        this.rows(i).TrimCells( 0 )
    next
    this.rowCount = 0
    ' the focus/anchor rows died with the contents; left stale, GetCurSel would
    ' report a row of the OLD list against whatever is loaded next
    this.focusRow  = -1
    this.anchorRow = -1
    ' Clearing is programmatic, so it does NOT notify -- but the host's idea of the
    ' current row died with the contents too, and leaving it stale would swallow the
    ' first user selection if it happened to land on the same index.
    this.lastNotifiedRow = -1
    this.NotifyChange()
end sub

' Rebuild the visible map from the model (flat, one-level grouping: an item is
' hidden iff its nearest preceding header is collapsed). Pure model: pushing the
' new count into the Win32 listbox is PsListBox_SyncListboxFromModel's job, because
' LB_SETCOUNT resets the listbox's scroll and caret and the sync re-derives both.
sub PSLISTBOX.RebuildVisibleMap()
    this.visibleCount = 0
    if this.rowCount > 0 then
        redim this.visibleMap( 0 to this.rowCount - 1 )
        dim as integer vis = 0
        dim as boolean collapsed = false
        for i as integer = 0 to this.rowCount - 1
            if this.rows(i).IsHeader then
                collapsed = this.rows(i).bCollapsed
                this.visibleMap(vis) = i : vis += 1
            elseif collapsed = false then
                this.visibleMap(vis) = i : vis += 1
            end if
        next
        this.visibleCount = vis
    else
        erase this.visibleMap
    end if
end sub

function PSLISTBOX.ModelToVisible( byval modelRow as integer ) as integer
    for v as integer = 0 to this.visibleCount - 1
        if this.visibleMap(v) = modelRow then return v
    next
    return -1
end function

function PSLISTBOX.VisibleToModel( byval visRow as integer ) as integer
    if (visRow < 0) orelse (visRow >= this.visibleCount) then return -1
    return this.visibleMap(visRow)
end function

' --- Selection is stored per-row in the model, so it survives collapse/expand
'     index shifts and can include hidden rows and headers. ---
function PSLISTBOX.IsRowSelected( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    return this.rows(modelRow).selected
end function

' A row the host has NOT marked non-selectable. Invalid rows read false (they cannot be
' selected anyway). This is the single predicate every selection/focus path consults, so
' the "non-selectable = never selected, never focused" invariant lives in one place.
function PSLISTBOX.IsRowSelectable( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    return this.rows(modelRow).bSelectable
end function

' Setting selected = true is refused for a non-selectable row (the model-level guard, so
' every caller inherits it); clearing is always allowed.
sub PSLISTBOX.SetRowSelected( byval modelRow as integer, byval state as boolean )
    if this.IsValidRow(modelRow) = false then exit sub
    if state andalso (this.rows(modelRow).bSelectable = false) then exit sub
    this.rows(modelRow).selected = state
end sub

sub PSLISTBOX.ClearSelection()
    for i as integer = 0 to this.rowCount - 1
        this.rows(i).selected = false
    next
end sub

sub PSLISTBOX.SelectOnly( byval modelRow as integer )
    this.ClearSelection()
    if this.IsValidRow(modelRow) andalso this.rows(modelRow).bSelectable then this.rows(modelRow).selected = true
end sub

sub PSLISTBOX.SelectRange( byval a as integer, byval b as integer )
    if a > b then swap a, b
    if a < 0 then a = 0
    if b > this.rowCount - 1 then b = this.rowCount - 1
    ' A Shift-range selects every SELECTABLE row it spans and skips the rest.
    for i as integer = a to b
        if this.rows(i).bSelectable then this.rows(i).selected = true
    next
end sub

function PSLISTBOX.GetSelCount() as integer
    dim as integer n = 0
    for i as integer = 0 to this.rowCount - 1
        if this.rows(i).selected then n += 1
    next
    return n
end function

' Reorder rows() so the source rows land contiguously just before `insertBefore` (a MODEL
' index; rowCount = append at the end). Source indices may be non-contiguous and in any
' order; they are de-duplicated and kept in their original relative order. HEADERS in the
' source are ignored (they are structural, never dragged). Each row's whole ROWINFO --
' Text, cells(), itemData and .selected -- travels on the struct copy, so selection follows
' for free. The moved block becomes the new focus/anchor. Returns the block's new first
' MODEL index, or -1 if nothing valid was moved. Caller repaints; this does not notify.
function PSLISTBOX.MoveRows( srcRows() as integer, byval insertBefore as integer ) as integer
    dim as integer n = this.rowCount
    if n <= 0 then return -1

    ' Mark the movable source rows (valid, non-header, de-duplicated).
    dim as byte moving(0 to n - 1)
    dim as integer srcN = 0
    for k as integer = lbound(srcRows) to ubound(srcRows)
        dim as integer r = srcRows(k)
        if (r >= 0) andalso (r < n) then
            if (this.rows(r).IsHeader = false) andalso (moving(r) = 0) then
                moving(r) = 1
                srcN += 1
            end if
        end if
    next
    if srcN = 0 then return -1

    ' Clamp the insertion anchor to [0, n].
    dim as integer anchor = insertBefore
    if anchor < 0 then anchor = 0
    if anchor > n then anchor = n

    ' Permutation of 0..n-1: non-moving rows in original order, with the whole moving block
    ' (also in original order) spliced in just before the anchor row. A block dropped just
    ' before/after itself collapses to a no-op, which falls out of this naturally.
    dim as integer newOrder(0 to n - 1)
    dim as integer w = 0
    dim as boolean emitted = false
    for i as integer = 0 to n - 1
        if i = anchor then
            for j as integer = 0 to n - 1
                if moving(j) then newOrder(w) = j : w += 1
            next
            emitted = true
        end if
        if moving(i) = 0 then newOrder(w) = i : w += 1
    next
    if emitted = false then                      ' anchor = n: block goes to the very end
        for j as integer = 0 to n - 1
            if moving(j) then newOrder(w) = j : w += 1
        next
    end if

    ' Where does the moving block start in the new order?
    dim as integer newFirst = -1
    for p as integer = 0 to n - 1
        if moving( newOrder(p) ) then newFirst = p : exit for
    next

    ' Apply the permutation through a temp copy (deep-copies Text/cells/itemData/.selected).
    dim tmp(0 to n - 1) as PSLISTBOX_ROWINFO
    for p as integer = 0 to n - 1
        tmp(p) = this.rows( newOrder(p) )
    next
    for p as integer = 0 to n - 1
        this.rows(p) = tmp(p)
    next

    ' The moved block is now the current selection's anchor/caret. lastNotifiedRow tracks it
    ' too, so a later user re-select of the block's first row stays correctly silent.
    this.focusRow        = newFirst
    this.anchorRow       = newFirst
    this.lastNotifiedRow = newFirst

    this.NotifyChange()
    return newFirst
end function

sub PSLISTBOX.BeginUpdate()
    this.updateDepth += 1
end sub

sub PSLISTBOX.EndUpdate()
    if this.updateDepth > 0 then this.updateDepth -= 1
    if this.updateDepth = 0 then this.Refresh()
end sub

' Called by every model mutator. Coalesces into a single Refresh when a
' BeginUpdate/EndUpdate batch is active.
sub PSLISTBOX.NotifyChange()
    if this.updateDepth = 0 then this.Refresh()
end sub

sub PSLISTBOX.Refresh()
    ' Capture the listbox's actual scroll position back into the model BEFORE the
    ' rebuild: the sync below pushes LB_SETCOUNT (which resets the Win32 listbox's
    ' top and caret), and the OLD visibleMap still matches the listbox contents at
    ' this point, so the translation is valid. This is what lets a scroll made by
    ' the user (wheel / keyboard / scrollbar) survive any rebuild.
    PsListBox_CaptureTopRow( @this )
    this.RebuildVisibleMap()
    dim as HWND hList = GetDlgItem( this.hWin, this.idc_ListBox )
    if hList = 0 then exit sub
    ShowWindow( hList, SW_SHOW )
    ' Re-derive the Win32 listbox (count, caret, top row) and the scrollbar from
    ' the model, then repaint WITH background erase so the vacated region below the
    ' last row is cleared when the list shrinks (delete / collapse).
    PsListBox_SyncViewFromModel( @this )
    InvalidateRect( hList, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every PsListBox_* function takes the handle returned by PsListBox_Create(). That handle
'   is the container window, which hosts three children: the owner-drawn LISTBOX, the
'   vertical scrollbar, and the (optional, hidden by default) column header band. The
'   functions resolve those children internally. Never pass the child listbox handle --
'   results are undefined.
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need
'   to treat the control as a window, e.g. SetWindowPos() to place and size it. An opaque
'   wrapper would buy a little type-safety and break that, so it was rejected.
'
' ROW INDICES ARE *MODEL* INDICES
'   Rows are addressed by the order they were added, independent of what is on screen.
'   Collapsing a group does NOT renumber anything, and hidden rows keep working with every
'   row API (including selection). Internally the control maps model rows to the visible
'   listbox positions; that mapping never leaks into this API. Everything you receive --
'   PAINTINFO.itemID, MESSAGEINFO.idx, GetCurSel, GetSelItems -- is a model index too.
'
' GROUPS
'   A row is either a header or an item. Items belong to the nearest preceding header;
'   there is exactly one level of nesting (no nested headers). Collapsing a header hides
'   its items. Headers are selectable and are returned by GetSelItems, so use
'   PsListBox_IsHeader() to tell them apart.
'
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID is the child listbox's control id (the scrollbar takes CtrlID + 1 and the
'   column header CtrlID + 2). The control is created zero-sized: position it with
'   SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function PsListBox_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing rows.  Add* and Insert* return the new row's model index, or -1.
' Wrap bulk loads in BeginUpdate/EndUpdate: it collapses the per-row rebuild+repaint into
' one, turning an O(n^2) load into O(n). The pairs nest.
' ----------------------------------------------------------------------------------------
declare function PsListBox_AddString( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListBox_AddHeader( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListBox_InsertString( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListBox_DeleteString( byval hListControl as HWND, byval row as integer ) as boolean
declare sub      PsListBox_Clear( byval hListControl as HWND )
declare sub      PsListBox_BeginUpdate( byval hListControl as HWND )
declare sub      PsListBox_EndUpdate( byval hListControl as HWND )
declare sub      PsListBox_Refresh( byval hListControl as HWND )

' ----------------------------------------------------------------------------------------
' Counts.  GetCount = every row in the model. GetVisibleCount = rows currently on show
' (headers + items of expanded groups). They differ whenever anything is collapsed.
' ----------------------------------------------------------------------------------------
declare function PsListBox_GetCount( byval hListControl as HWND ) as integer
declare function PsListBox_GetVisibleCount( byval hListControl as HWND ) as integer

' ----------------------------------------------------------------------------------------
' Row contents.  Set* return FALSE for an invalid row index.
'   Cells: column 0 IS the row's Text (GetText/SetText and the cell APIs with col = 0
'   are interchangeable). Higher columns are stored sparsely per row -- any cell never
'   set reads back "", and cell text is independent of the column DEFINITIONS, so rows
'   can be populated before or after columns are added. col < 0 fails; col beyond the
'   defined columns is legal storage (it paints once a matching column exists).
' ----------------------------------------------------------------------------------------
declare function PsListBox_GetText( byval hListControl as HWND, byval row as integer ) as DWSTRING
declare function PsListBox_SetText( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING ) as boolean
declare function PsListBox_GetCellText( byval hListControl as HWND, byval row as integer, byval col as integer ) as DWSTRING
declare function PsListBox_SetCellText( byval hListControl as HWND, byval row as integer, byval col as integer, byval Text as DWSTRING ) as boolean
declare function PsListBox_GetItemData( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListBox_SetItemData( byval hListControl as HWND, byval row as integer, byval itemData as integer ) as boolean
declare function PsListBox_GetItemDataExtra( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListBox_SetItemDataExtra( byval hListControl as HWND, byval row as integer, byval itemDataExtra as integer ) as boolean

' Make an ordinary (selectable) row paint as a single cell spanning every column instead
' of one cell per column. The row's text stays in column 0 (GetText / GetCellText(row,0));
' the paint callback receives columnCount = 1 with cells[0].rc covering the whole column
' band and p->isSpanned = true. Silent (programmatic setter); repaints. Get returns the
' flag; both return FALSE for an invalid row.
declare function PsListBox_SetRowSpanColumns( byval hListControl as HWND, byval row as integer, byval bSpan as boolean = true ) as boolean
declare function PsListBox_GetRowSpanColumns( byval hListControl as HWND, byval row as integer ) as boolean

' Make a row non-selectable (bSelectable = false): it cannot be selected or focused by any
' path -- mouse, keyboard and the programmatic setters alike -- and keyboard navigation
' skips over it. Turning a currently selected/focused row non-selectable clears its
' selection and drops the caret. Silent (programmatic setter); repaints. Get returns the
' flag (default TRUE); both return FALSE for an invalid row.
declare function PsListBox_SetRowSelectable( byval hListControl as HWND, byval row as integer, byval bSelectable as boolean = true ) as boolean
declare function PsListBox_GetRowSelectable( byval hListControl as HWND, byval row as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Groups / collapsing.  Collapse/Expand/Toggle act only on header rows and return FALSE
' for items or invalid rows. A mouse click on a header toggles it without disturbing the
' selection; the keyboard uses Left/Right.
' ----------------------------------------------------------------------------------------
declare function PsListBox_IsHeader( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_IsCollapsed( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_CollapseRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_ExpandRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_ToggleRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_CollapseAll( byval hListControl as HWND ) as boolean
declare function PsListBox_ExpandAll( byval hListControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Selection.  Selection is stored on the rows, so it survives collapse/expand and can
' include hidden rows and headers.
'   Modes are mutually exclusive; both off = single-select:
'     SetExtendedSelect - Shift ranges / Ctrl toggles (explorer style)
'     SetMultiSelect    - every click toggles one row (checklist style)
'   GetCurSel/SetCurSel address the focused row. SetCurSel selects only that row.
'   GetSelItems reports ALL selected rows, hidden ones included, and redims selItems().
' ----------------------------------------------------------------------------------------
declare function PsListBox_GetCurSel( byval hListControl as HWND ) as integer
declare function PsListBox_SetCurSel( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListBox_GetSel( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListBox_SetSel( byval hListControl as HWND, byval row as integer, byval state as boolean ) as boolean
declare function PsListBox_GetSelCount( byval hListControl as HWND ) as integer
declare function PsListBox_GetSelItems( byval hListControl as HWND, selItems() as integer ) as integer
declare sub      PsListBox_SelectAll( byval hListControl as HWND, byval state as boolean )
declare function PsListBox_SetMultiSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function PsListBox_SetExtendedSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function PsListBox_PreventDoubleClick( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListBox_GetTopIndex( byval hListControl as HWND ) as integer
declare function PsListBox_SetTopIndex( byval hListControl as HWND, byval row as integer ) as integer

' ----------------------------------------------------------------------------------------
' Drag-and-drop row reordering.  Opt-in (default OFF). When on, the user drags a row -- or,
' if the pressed row is part of the selection, the whole selection -- to a new position;
' dropping ON a header inserts the block as that header's first children. Non-selectable
' rows and headers are not draggable. The control reorders its OWN model, firing CanDrop
' (veto) before and DragDrop (notify) after. MoveRows is the same reorder as a silent,
' programmatic call; SetDragIndicatorColor tunes the insertion line / header highlight.
' ----------------------------------------------------------------------------------------
declare function PsListBox_SetDragReorder( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListBox_GetDragReorder( byval hListControl as HWND ) as boolean
declare sub      PsListBox_SetCanDropCallback( byval hListControl as HWND, byval usersub as CanDropCallbackFunc )
declare sub      PsListBox_SetDragDropCallback( byval hListControl as HWND, byval usersub as DragDropCallbackSub )
declare function PsListBox_MoveRows( byval hListControl as HWND, srcRows() as integer, byval insertBefore as integer ) as integer
declare function PsListBox_SetDragIndicatorColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
declare function PsListBox_GetDragIndicatorColor( byval hListControl as HWND ) as COLORREF

' ----------------------------------------------------------------------------------------
' Appearance.  Row height is in unscaled units and is DPI-scaled internally. The font is
' borrowed, never owned: keep it alive and destroy it yourself.
' ----------------------------------------------------------------------------------------
declare function PsListBox_GetBackColor( byval hListControl as HWND ) as COLORREF
declare function PsListBox_SetBackColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
declare function PsListBox_GetRowHeight( byval hListControl as HWND ) as integer
declare function PsListBox_SetRowHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function PsListBox_GetFont( byval hListControl as HWND ) as HFONT
declare function PsListBox_SetFont( byval hListControl as HWND, byval hFont as HFONT ) as boolean
declare sub      PsListBox_SetHoverTime( byval hListControl as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Vertical scrollbar.  Created, positioned, ranged and auto-hidden by this control -- it
' appears only while the rows overflow, and the listbox reclaims the width otherwise.
' Nothing here is required; it is for theming. GetScrollBar exposes the child for direct
' PsVScrollBar_* calls.
' ----------------------------------------------------------------------------------------
declare function PsListBox_GetScrollBar( byval hListControl as HWND ) as HWND
declare sub      PsListBox_SetScrollBarWidth( byval hListControl as HWND, byval nWidth as integer )
declare sub      PsListBox_SetScrollBarColors( byval hListControl as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
declare sub      PsListBox_SetScrollBarPaintCallback( byval hListControl as HWND, byval usersub as VScrollPaintCallbackSub )

' ----------------------------------------------------------------------------------------
' Columns and the header band.  All optional: with no columns defined the control paints
' exactly as before. Column state lives in the embedded PsColumnHeader child (the single
' source of truth); these wrappers delegate to it. Widths are PIXELS (see PsColumnHeader.bi
' for the width/min-width/fill rules); HeaderHeight is unscaled units like RowHeight.
'
'   Columns can be defined with the header band hidden (ShowHeader false, the default):
'   rows still paint in columns, there is just no interactive header strip. The header
'   spans the full container width -- listview-style, over the scrollbar strip -- so
'   column geometry never shifts when the scrollbar auto-hides.
'
'   CALLBACK OWNERSHIP: on an embedded header the control owns the header's own
'   WidthChanged slot (it must repaint rows on every live resize). Hosts subscribe with
'   PsListBox_SetColumnResizeCallback -- never PsColumnHeader_SetWidthChangedCallback on
'   the child returned by PsListBox_GetHeader. The other header callbacks (paint, click,
'   autosize, tooltip) pass straight through.
'
'   Programmatic setters are silent (family rule): SetColumnWidth repaints but fires no
'   resize callback; only user drags/autosize notify.
' ----------------------------------------------------------------------------------------
declare function PsListBox_AddColumn( byval hListControl as HWND, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsListBox_InsertColumn( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsListBox_DeleteColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare sub      PsListBox_ClearColumns( byval hListControl as HWND )
declare function PsListBox_GetColumnCount( byval hListControl as HWND ) as integer
declare function PsListBox_GetColumnText( byval hListControl as HWND, byval idx as integer ) as DWSTRING
declare function PsListBox_SetColumnText( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function PsListBox_GetColumnWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function PsListBox_SetColumnWidth( byval hListControl as HWND, byval idx as integer, byval nWidth as integer ) as boolean
declare function PsListBox_GetColumnMinWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function PsListBox_SetColumnMinWidth( byval hListControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function PsListBox_GetFillColumn( byval hListControl as HWND ) as integer
declare function PsListBox_SetFillColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare function PsListBox_ShowHeader( byval hListControl as HWND, byval bShow as boolean = true ) as boolean
declare function PsListBox_IsHeaderVisible( byval hListControl as HWND ) as boolean
declare function PsListBox_GetHeaderHeight( byval hListControl as HWND ) as integer
declare function PsListBox_SetHeaderHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function PsListBox_GetHeader( byval hListControl as HWND ) as HWND
declare sub      PsListBox_SetColumnResizeCallback( byval hListControl as HWND, byval usersub as HDR_WidthChangedCallbackSub )
declare sub      PsListBox_SetColumnClickCallback( byval hListControl as HWND, byval usersub as HDR_ClickCallbackSub )
declare sub      PsListBox_SetColumnAutoSizeCallback( byval hListControl as HWND, byval userfunc as HDR_AutoSizeCallbackFunc )
declare sub      PsListBox_SetHeaderPaintCallback( byval hListControl as HWND, byval usersub as HDR_PaintCallbackSub )
declare sub      PsListBox_SetHeaderTooltipCallback( byval hListControl as HWND, byval userfunc as HDR_TooltipCallbackFunc )
declare sub      PsListBox_SetHeaderBackColor( byval hListControl as HWND, byval clr as COLORREF )
declare sub      PsListBox_SetHeaderFont( byval hListControl as HWND, byval hFont as HFONT )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw one row. Required if you want to see anything.
'   MessageCallback - observe mouse messages; return TRUE to suppress default handling.
'                     NOTE: the result is ignored for WM_LBUTTONUP (see PsListBox.inc).
'   TooltipCallback - supply per-row tooltip text on demand; "" for none.
'   SelChangeCallback - the USER selected a different row (mouse OR keyboard). Silent for
'                     the programmatic setters. This is the only way to see keyboard
'                     navigation: the control consumes WM_KEYDOWN itself.
' ----------------------------------------------------------------------------------------
declare sub      PsListBox_SetPaintCallback( byval hListControl as HWND, byval usersub as PaintCallbackSub )
declare sub      PsListBox_SetMessageCallback( byval hListControl as HWND, byval userfunc as MessageCallbackFunc )
declare sub      PsListBox_SetTooltipCallback( byval hListControl as HWND, byval userfunc as TooltipCallbackFunc )
declare sub      PsListBox_SetSelChangeCallback( byval hListControl as HWND, byval usersub as SelChangeCallbackSub )
