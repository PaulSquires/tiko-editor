
#pragma once

#include once "clsDoubleBuffer.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id. Value is deliberately unusual to avoid colliding with
' any timer the standard listbox uses internally.
#define IDT_CLISTBOX_HOTTRACK   &hCB01
#define CLISTBOX_HOTTRACK_MS    100

' One cell of a multi-column row, as handed to the paint callback. The rect is in the
' row buffer's coordinate space (y spans 0..row height; x comes from the header's
' column geometry, which maps 1:1 onto the row -- see CListBox_PositionWindows).
type CLISTBOX_CELLINFO
    iCol            as integer
    rc              as RECT
    wszText         as DWSTRING
end type

type CLISTBOX_PAINTINFO
    itemID          as integer                ' MODEL row index (not the visible/listbox index)
    b               as clsDoubleBuffer ptr    ' points to the caller's buffer (no copy)
    rc              as RECT
    isHot           as boolean                ' mouse is hovering this row
    isSelected      as boolean                ' row is part of the selection
    isFocused       as boolean                ' row has the keyboard focus (caret)
    isHeader        as boolean                ' this row is a group header
    isCollapsed     as boolean                ' header only: its child items are hidden
    wszCaption      as DWSTRING
    ' --- Columns. 0/null whenever the row should paint as a single full-width cell:
    '     no columns are defined, or this row is a group header (group headers span).
    '     With columns: background-fill the FULL rc first (selection/hot spans the
    '     whole row, listview-style), then draw each cells[i].wszText inside
    '     cells[i].rc -- the control does not clip between cells (DT_END_ELLIPSIS is
    '     your friend). The array is control-owned scratch, valid ONLY during the
    '     callback -- copy anything you need to keep. ---
    columnCount     as integer
    cells           as CLISTBOX_CELLINFO ptr
end type

type CLISTBOX_MESSAGEINFO
    hList           as HWND
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
    idx             as integer    ' MODEL row index under the mouse (-1 if none)
    isCtrl          as boolean
    isShift         as boolean
end type


type CLISTBOX_ROWINFO
    IsHeader        as boolean = false
    bCollapsed      as boolean = false
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
sub CLISTBOX_ROWINFO.EnsureCells( byval n as integer )
    if n <= 0 then exit sub
    if ubound(this.cells) < n - 1 then redim preserve this.cells( 0 to n - 1 )
end sub

' Shrink storage to exactly n cells; n <= 0 frees the array.
sub CLISTBOX_ROWINFO.TrimCells( byval n as integer )
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
type PaintCallbackSub as sub( byval p as CLISTBOX_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the default listbox
' handling suppressed, FALSE to let it proceed.
' CAUTION: for WM_LBUTTONUP the result is IGNORED and the default always runs -- the
' listbox releases its mouse capture there, and swallowing it strands the capture.
type MessageCallbackFunc as function( byval m as CLISTBOX_MESSAGEINFO ptr ) as boolean

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
' NOT fired for CListBox_SetCurSel / SetSel / SelectAll / Clear: programmatic setters are
' silent (the family rule), so a host may call them from inside this callback without
' re-entering itself. Nor is it fired when the user re-selects the row that is already
' current -- only an actual change notifies.
type SelChangeCallbackSub as sub( byval hListControl as HWND, byval row as integer )

type CLISTBOX
    hWin            as HWND
    hToolTip        as HWND
    wszTooltip      as DWSTRING
    ' --- Model: rows() is the backing store (capacity = ubound+1); rowCount is
    '     the number of logical rows and the single source of truth for "count". ---
    rows(any)       as CLISTBOX_ROWINFO
    rowCount        as integer = 0
    ' --- View: visibleMap(v) -> model row index, for v = 0..visibleCount-1.
    '     Rebuilt on any change / collapse-expand. The listbox (LBS_NODATA) count
    '     is always set to visibleCount. ---
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
                                          ' source of truth the Win32 listbox is re-derived from
    ExtendSel       as boolean = false
    MultipleSel     as boolean = false
    PreventDblClick as boolean = false    ' host opt-out of double-click: the dblclk becomes a
                                          ' plain click and its trailing second up is not forwarded
    skipNextLBtnUp  as boolean = false    ' armed by a suppressed dblclk; consumed by the next
                                          ' WM_LBUTTONUP, disarmed by any fresh WM_LBUTTONDOWN
    BackColor       as COLORREF
    hBackBrush      as HBRUSH             ' cached WM_CTLCOLORLISTBOX brush (control-owned;
    backBrushColor  as COLORREF           '   recreated when BackColor changes, freed at destroy)
    hFont           as HFONT              ' caller-supplied font for row text (caller owns it)
    ' --- Owner-drawn vertical scrollbar, created and driven by this control. It is
    '     auto-hidden whenever the visible rows fit, and the listbox then reclaims the
    '     full client width. ---
    hScrollBar      as HWND
    ScrollBarWidth  as integer = CVSCROLL_DEFAULT_WIDTH   ' DPI-scaled when laid out
    scrollBarShown  as boolean = false
    ' --- Optional column header band (CColumnHeader), created hidden and owned by this
    '     control. The embedded header instance is the SINGLE store for column
    '     definitions and geometry -- CListBox keeps no column state; the CListBox_*
    '     column wrappers delegate, and OnDrawItem reads cell x-coordinates from the
    '     header's rects. The control owns the header's WidthChanged slot (an internal
    '     chain hook); hosts use CListBox_SetColumnResizeCallback. ---
    hHeader         as HWND
    headerShown     as boolean = false
    HeaderHeight    as integer = 24       ' unscaled units (like RowHeight); ScaleY at layout
    ColumnResizeCallback as HDR_WidthChangedCallbackSub   ' host-facing re-broadcast
    PaintCallback   as PaintCallbackSub
    MessageCallback as MessageCallbackFunc
    TooltipCallback as TooltipCallbackFunc    ' optional; defaults to the row's Text
    SelChangeCallback as SelChangeCallbackSub ' optional; user-driven selection changes only
    ' --- Reusable one-row back buffer, so WM_DRAWITEM doesn't create/destroy a
    '     compatible DC + bitmap for every row on every repaint. ---
    cacheDC         as HDC
    cacheBmp        as HBITMAP
    cacheOldBmp     as HBITMAP
    cacheW          as integer = 0
    cacheH          as integer = 0
    ' --- Persistent scratch for PAINTINFO.cells, re-dimensioned only when the column
    '     count changes (WM_DRAWITEM is strictly sequential and single-threaded, so
    '     one array serves every row; the pointer handed out is valid only for the
    '     duration of each callback). ---
    paintCells(any) as CLISTBOX_CELLINFO

    declare destructor()
    declare function EnsureCache( byval refDC as HDC, byval w as integer, byval h as integer ) as HDC
    declare sub      FreeCache()
    declare sub      EnsurePaintCells( byval n as integer )
    declare function GetCount() as integer                                  ' model row count
    declare function GetVisibleCount() as integer
    declare function AddRow() as CLISTBOX_ROWINFO ptr                       ' append
    declare function InsertRowAt( byval modelRow as integer ) as CLISTBOX_ROWINFO ptr
    declare function DeleteRowAt( byval modelRow as integer ) as boolean
    declare sub      Clear()
    declare function GetRow( byval row as integer ) as CLISTBOX_ROWINFO ptr
    declare function IsValidRow( byval row as integer ) as boolean
    declare function ModelToVisible( byval modelRow as integer ) as integer ' -1 if hidden/invalid
    declare function VisibleToModel( byval visRow as integer ) as integer   ' -1 if invalid
    declare function IsRowSelected( byval modelRow as integer ) as boolean
    declare sub      SetRowSelected( byval modelRow as integer, byval state as boolean )
    declare sub      ClearSelection()
    declare sub      SelectOnly( byval modelRow as integer )
    declare sub      SelectRange( byval a as integer, byval b as integer )
    declare function GetSelCount() as integer
    declare sub      RebuildVisibleMap()
    declare sub      BeginUpdate()
    declare sub      EndUpdate()
    declare sub      NotifyChange()
    declare sub      Refresh()
end type

' Defined in CListBox.inc, but CLISTBOX.Refresh (below) has to call it -- push the
' scrollbar's range/visibility to match the current model + scroll position.
declare sub      CListBox_SyncScrollBar( byval pList as CLISTBOX ptr )
declare sub      CListBox_SyncListboxFromModel( byval pList as CLISTBOX ptr )
declare sub      CListBox_CaptureTopRow( byval pList as CLISTBOX ptr )
declare function CListBox_ItemsPerPage( byval pList as CLISTBOX ptr ) as integer
declare function CListBox_PositionWindows( byval hwnd as HWND ) as LRESULT

destructor CLISTBOX()
    this.FreeCache()
end destructor

' Return a cached memDC whose selected bitmap is at least w x h, (re)creating it
' only when the requested size changes. Bitmap is compatible with refDC.
function CLISTBOX.EnsureCache( byval refDC as HDC, byval w as integer, byval h as integer ) as HDC
    if (this.cacheDC <> 0) andalso (this.cacheW = w) andalso (this.cacheH = h) then
        return this.cacheDC
    end if
    this.FreeCache()
    this.cacheDC     = CreateCompatibleDC( refDC )
    this.cacheBmp    = CreateCompatibleBitmap( refDC, w, h )
    this.cacheOldBmp = SelectObject( this.cacheDC, this.cacheBmp )
    this.cacheW      = w
    this.cacheH      = h
    return this.cacheDC
end function

sub CLISTBOX.FreeCache()
    if this.cacheDC then
        if this.cacheOldBmp then SelectObject( this.cacheDC, this.cacheOldBmp )
        DeleteDC( this.cacheDC )
        this.cacheDC = 0
    end if
    if this.cacheBmp then
        DeleteObject( this.cacheBmp )
        this.cacheBmp = 0
    end if
    this.cacheOldBmp = 0
    this.cacheW = 0
    this.cacheH = 0
end sub

' Size the PAINTINFO.cells scratch to exactly n entries (only re-dims when the column
' count actually changed, so the per-row cost is a compare).
sub CLISTBOX.EnsurePaintCells( byval n as integer )
    if n <= 0 then
        erase this.paintCells
    elseif ubound(this.paintCells) + 1 <> n then
        redim this.paintCells( 0 to n - 1 )
    end if
end sub

function CLISTBOX.GetCount() as integer
    return this.rowCount
end function

function CLISTBOX.GetVisibleCount() as integer
    return this.visibleCount
end function

function CLISTBOX.IsValidRow( byval row as integer ) as boolean
    return (row >= 0) andalso (row < this.rowCount)
end function

function CLISTBOX.GetRow( byval row as integer ) as CLISTBOX_ROWINFO ptr
    if this.IsValidRow(row) = false then return null
    return @this.rows(row)
end function

' Insert a fresh (reset) row at modelRow, shifting later rows up. Grows the
' backing store by doubling so bulk inserts are amortized O(1), not O(n^2).
function CLISTBOX.InsertRowAt( byval modelRow as integer ) as CLISTBOX_ROWINFO ptr
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

function CLISTBOX.AddRow() as CLISTBOX_ROWINFO ptr
    return this.InsertRowAt( this.rowCount )
end function

function CLISTBOX.DeleteRowAt( byval modelRow as integer ) as boolean
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

sub CLISTBOX.Clear()
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
' new count into the Win32 listbox is CListBox_SyncListboxFromModel's job, because
' LB_SETCOUNT resets the listbox's scroll and caret and the sync re-derives both.
sub CLISTBOX.RebuildVisibleMap()
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

function CLISTBOX.ModelToVisible( byval modelRow as integer ) as integer
    for v as integer = 0 to this.visibleCount - 1
        if this.visibleMap(v) = modelRow then return v
    next
    return -1
end function

function CLISTBOX.VisibleToModel( byval visRow as integer ) as integer
    if (visRow < 0) orelse (visRow >= this.visibleCount) then return -1
    return this.visibleMap(visRow)
end function

' --- Selection is stored per-row in the model, so it survives collapse/expand
'     index shifts and can include hidden rows and headers. ---
function CLISTBOX.IsRowSelected( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    return this.rows(modelRow).selected
end function

sub CLISTBOX.SetRowSelected( byval modelRow as integer, byval state as boolean )
    if this.IsValidRow(modelRow) then this.rows(modelRow).selected = state
end sub

sub CLISTBOX.ClearSelection()
    for i as integer = 0 to this.rowCount - 1
        this.rows(i).selected = false
    next
end sub

sub CLISTBOX.SelectOnly( byval modelRow as integer )
    this.ClearSelection()
    if this.IsValidRow(modelRow) then this.rows(modelRow).selected = true
end sub

sub CLISTBOX.SelectRange( byval a as integer, byval b as integer )
    if a > b then swap a, b
    if a < 0 then a = 0
    if b > this.rowCount - 1 then b = this.rowCount - 1
    for i as integer = a to b
        this.rows(i).selected = true
    next
end sub

function CLISTBOX.GetSelCount() as integer
    dim as integer n = 0
    for i as integer = 0 to this.rowCount - 1
        if this.rows(i).selected then n += 1
    next
    return n
end function

sub CLISTBOX.BeginUpdate()
    this.updateDepth += 1
end sub

sub CLISTBOX.EndUpdate()
    if this.updateDepth > 0 then this.updateDepth -= 1
    if this.updateDepth = 0 then this.Refresh()
end sub

' Called by every model mutator. Coalesces into a single Refresh when a
' BeginUpdate/EndUpdate batch is active.
sub CLISTBOX.NotifyChange()
    if this.updateDepth = 0 then this.Refresh()
end sub

sub CLISTBOX.Refresh()
    ' Capture the listbox's actual scroll position back into the model BEFORE the
    ' rebuild: the sync below pushes LB_SETCOUNT (which resets the Win32 listbox's
    ' top and caret), and the OLD visibleMap still matches the listbox contents at
    ' this point, so the translation is valid. This is what lets a scroll made by
    ' the user (wheel / keyboard / scrollbar) survive any rebuild.
    CListBox_CaptureTopRow( @this )
    this.RebuildVisibleMap()
    dim as HWND hList = GetDlgItem( this.hWin, this.idc_ListBox )
    if hList = 0 then exit sub
    ShowWindow( hList, SW_SHOW )
    ' Re-derive the Win32 listbox (count, caret, top row) and the scrollbar from
    ' the model, then repaint WITH background erase so the vacated region below the
    ' last row is cleared when the list shrinks (delete / collapse).
    CListBox_SyncListboxFromModel( @this )
    InvalidateRect( hList, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every CListBox_* function takes the handle returned by CListBox_Create(). That handle
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
'   CListBox_IsHeader() to tell them apart.
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
declare function CListBox_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing rows.  Add* and Insert* return the new row's model index, or -1.
' Wrap bulk loads in BeginUpdate/EndUpdate: it collapses the per-row rebuild+repaint into
' one, turning an O(n^2) load into O(n). The pairs nest.
' ----------------------------------------------------------------------------------------
declare function CListBox_AddString( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function CListBox_AddHeader( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function CListBox_InsertString( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function CListBox_DeleteString( byval hListControl as HWND, byval row as integer ) as boolean
declare sub      CListBox_Clear( byval hListControl as HWND )
declare sub      CListBox_BeginUpdate( byval hListControl as HWND )
declare sub      CListBox_EndUpdate( byval hListControl as HWND )
declare sub      CListBox_Refresh( byval hListControl as HWND )

' ----------------------------------------------------------------------------------------
' Counts.  GetCount = every row in the model. GetVisibleCount = rows currently on show
' (headers + items of expanded groups). They differ whenever anything is collapsed.
' ----------------------------------------------------------------------------------------
declare function CListBox_GetCount( byval hListControl as HWND ) as integer
declare function CListBox_GetVisibleCount( byval hListControl as HWND ) as integer

' ----------------------------------------------------------------------------------------
' Row contents.  Set* return FALSE for an invalid row index.
'   Cells: column 0 IS the row's Text (GetText/SetText and the cell APIs with col = 0
'   are interchangeable). Higher columns are stored sparsely per row -- any cell never
'   set reads back "", and cell text is independent of the column DEFINITIONS, so rows
'   can be populated before or after columns are added. col < 0 fails; col beyond the
'   defined columns is legal storage (it paints once a matching column exists).
' ----------------------------------------------------------------------------------------
declare function CListBox_GetText( byval hListControl as HWND, byval row as integer ) as DWSTRING
declare function CListBox_SetText( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING ) as boolean
declare function CListBox_GetCellText( byval hListControl as HWND, byval row as integer, byval col as integer ) as DWSTRING
declare function CListBox_SetCellText( byval hListControl as HWND, byval row as integer, byval col as integer, byval Text as DWSTRING ) as boolean
declare function CListBox_GetItemData( byval hListControl as HWND, byval row as integer ) as integer
declare function CListBox_SetItemData( byval hListControl as HWND, byval row as integer, byval itemData as integer ) as boolean
declare function CListBox_GetItemDataExtra( byval hListControl as HWND, byval row as integer ) as integer
declare function CListBox_SetItemDataExtra( byval hListControl as HWND, byval row as integer, byval itemDataExtra as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Groups / collapsing.  Collapse/Expand/Toggle act only on header rows and return FALSE
' for items or invalid rows. A mouse click on a header toggles it without disturbing the
' selection; the keyboard uses Left/Right.
' ----------------------------------------------------------------------------------------
declare function CListBox_IsHeader( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_IsCollapsed( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_CollapseRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_ExpandRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_ToggleRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_CollapseAll( byval hListControl as HWND ) as boolean
declare function CListBox_ExpandAll( byval hListControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Selection.  Selection is stored on the rows, so it survives collapse/expand and can
' include hidden rows and headers.
'   Modes are mutually exclusive; both off = single-select:
'     SetExtendedSelect - Shift ranges / Ctrl toggles (explorer style)
'     SetMultiSelect    - every click toggles one row (checklist style)
'   GetCurSel/SetCurSel address the focused row. SetCurSel selects only that row.
'   GetSelItems reports ALL selected rows, hidden ones included, and redims selItems().
' ----------------------------------------------------------------------------------------
declare function CListBox_GetCurSel( byval hListControl as HWND ) as integer
declare function CListBox_SetCurSel( byval hListControl as HWND, byval row as integer ) as integer
declare function CListBox_GetSel( byval hListControl as HWND, byval row as integer ) as boolean
declare function CListBox_SetSel( byval hListControl as HWND, byval row as integer, byval state as boolean ) as boolean
declare function CListBox_GetSelCount( byval hListControl as HWND ) as integer
declare function CListBox_GetSelItems( byval hListControl as HWND, selItems() as integer ) as integer
declare sub      CListBox_SelectAll( byval hListControl as HWND, byval state as boolean )
declare function CListBox_SetMultiSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function CListBox_SetExtendedSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function CListBox_PreventDoubleClick( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function CListBox_GetTopIndex( byval hListControl as HWND ) as integer
declare function CListBox_SetTopIndex( byval hListControl as HWND, byval row as integer ) as integer

' ----------------------------------------------------------------------------------------
' Appearance.  Row height is in unscaled units and is DPI-scaled internally. The font is
' borrowed, never owned: keep it alive and destroy it yourself.
' ----------------------------------------------------------------------------------------
declare function CListBox_GetBackColor( byval hListControl as HWND ) as COLORREF
declare function CListBox_SetBackColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
declare function CListBox_GetRowHeight( byval hListControl as HWND ) as integer
declare function CListBox_SetRowHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function CListBox_GetFont( byval hListControl as HWND ) as HFONT
declare function CListBox_SetFont( byval hListControl as HWND, byval hFont as HFONT ) as boolean
declare sub      CListBox_SetHoverTime( byval hListControl as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Vertical scrollbar.  Created, positioned, ranged and auto-hidden by this control -- it
' appears only while the rows overflow, and the listbox reclaims the width otherwise.
' Nothing here is required; it is for theming. GetScrollBar exposes the child for direct
' CVScrollBar_* calls.
' ----------------------------------------------------------------------------------------
declare function CListBox_GetScrollBar( byval hListControl as HWND ) as HWND
declare sub      CListBox_SetScrollBarWidth( byval hListControl as HWND, byval nWidth as integer )
declare sub      CListBox_SetScrollBarColors( byval hListControl as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
declare sub      CListBox_SetScrollBarPaintCallback( byval hListControl as HWND, byval usersub as VScrollPaintCallbackSub )

' ----------------------------------------------------------------------------------------
' Columns and the header band.  All optional: with no columns defined the control paints
' exactly as before. Column state lives in the embedded CColumnHeader child (the single
' source of truth); these wrappers delegate to it. Widths are PIXELS (see CColumnHeader.bi
' for the width/min-width/fill rules); HeaderHeight is unscaled units like RowHeight.
'
'   Columns can be defined with the header band hidden (ShowHeader false, the default):
'   rows still paint in columns, there is just no interactive header strip. The header
'   spans the full container width -- listview-style, over the scrollbar strip -- so
'   column geometry never shifts when the scrollbar auto-hides.
'
'   CALLBACK OWNERSHIP: on an embedded header the control owns the header's own
'   WidthChanged slot (it must repaint rows on every live resize). Hosts subscribe with
'   CListBox_SetColumnResizeCallback -- never CColumnHeader_SetWidthChangedCallback on
'   the child returned by CListBox_GetHeader. The other header callbacks (paint, click,
'   autosize, tooltip) pass straight through.
'
'   Programmatic setters are silent (family rule): SetColumnWidth repaints but fires no
'   resize callback; only user drags/autosize notify.
' ----------------------------------------------------------------------------------------
declare function CListBox_AddColumn( byval hListControl as HWND, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function CListBox_InsertColumn( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function CListBox_DeleteColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare sub      CListBox_ClearColumns( byval hListControl as HWND )
declare function CListBox_GetColumnCount( byval hListControl as HWND ) as integer
declare function CListBox_GetColumnText( byval hListControl as HWND, byval idx as integer ) as DWSTRING
declare function CListBox_SetColumnText( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function CListBox_GetColumnWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function CListBox_SetColumnWidth( byval hListControl as HWND, byval idx as integer, byval nWidth as integer ) as boolean
declare function CListBox_GetColumnMinWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function CListBox_SetColumnMinWidth( byval hListControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function CListBox_GetFillColumn( byval hListControl as HWND ) as integer
declare function CListBox_SetFillColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare function CListBox_ShowHeader( byval hListControl as HWND, byval bShow as boolean = true ) as boolean
declare function CListBox_IsHeaderVisible( byval hListControl as HWND ) as boolean
declare function CListBox_GetHeaderHeight( byval hListControl as HWND ) as integer
declare function CListBox_SetHeaderHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function CListBox_GetHeader( byval hListControl as HWND ) as HWND
declare sub      CListBox_SetColumnResizeCallback( byval hListControl as HWND, byval usersub as HDR_WidthChangedCallbackSub )
declare sub      CListBox_SetColumnClickCallback( byval hListControl as HWND, byval usersub as HDR_ClickCallbackSub )
declare sub      CListBox_SetColumnAutoSizeCallback( byval hListControl as HWND, byval userfunc as HDR_AutoSizeCallbackFunc )
declare sub      CListBox_SetHeaderPaintCallback( byval hListControl as HWND, byval usersub as HDR_PaintCallbackSub )
declare sub      CListBox_SetHeaderTooltipCallback( byval hListControl as HWND, byval userfunc as HDR_TooltipCallbackFunc )
declare sub      CListBox_SetHeaderBackColor( byval hListControl as HWND, byval clr as COLORREF )
declare sub      CListBox_SetHeaderFont( byval hListControl as HWND, byval hFont as HFONT )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw one row. Required if you want to see anything.
'   MessageCallback - observe mouse messages; return TRUE to suppress default handling.
'                     NOTE: the result is ignored for WM_LBUTTONUP (see CListBox.inc).
'   TooltipCallback - supply per-row tooltip text on demand; "" for none.
'   SelChangeCallback - the USER selected a different row (mouse OR keyboard). Silent for
'                     the programmatic setters. This is the only way to see keyboard
'                     navigation: the control consumes WM_KEYDOWN itself.
' ----------------------------------------------------------------------------------------
declare sub      CListBox_SetPaintCallback( byval hListControl as HWND, byval usersub as PaintCallbackSub )
declare sub      CListBox_SetMessageCallback( byval hListControl as HWND, byval userfunc as MessageCallbackFunc )
declare sub      CListBox_SetTooltipCallback( byval hListControl as HWND, byval userfunc as TooltipCallbackFunc )
declare sub      CListBox_SetSelChangeCallback( byval hListControl as HWND, byval usersub as SelChangeCallbackSub )
