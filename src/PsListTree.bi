
#pragma once

#include once "PsBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id. Value is deliberately unusual to avoid colliding with
' any timer the standard listbox uses internally.
#define IDT_CLISTTREE_HOTTRACK   &hCB01
#define PSLISTTREE_HOTTRACK_MS    100

' Auto-scroll timer used ONLY while a drag-reorder is in progress: when the cursor is held
' near the top/bottom edge the list scrolls a row at a time so off-screen drop targets are
' reachable. Separate id from the hot-track timer so the two never step on each other.
#define IDT_CLISTTREE_DRAGSCROLL &hCB02
#define PSLISTTREE_DRAGSCROLL_MS   60
' How close to the top/bottom edge (in pixels) the cursor must be to trigger auto-scroll.
#define PSLISTTREE_DRAGSCROLL_ZONE 18

' One cell of a multi-column row, as handed to the paint callback. The rect is in the
' row buffer's coordinate space (y spans 0..row height; x comes from the header's
' column geometry, which maps 1:1 onto the row -- see PsListTree_PositionWindows).
type PSLISTTREE_CELLINFO
    iCol            as integer
    rc              as RECT
    wszText         as DWSTRING
end type

type PSLISTTREE_PAINTINFO
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
    ' --- Tree depth. Always populated; only MEANINGFUL once a host turns on tree visuals
    '     (PsListTree_SetTreeIndent / PsListTree_ShowTwisty). level = 0 is a top-level node.
    '     A legacy flat list is all level 0; a legacy header+item group is header level 0,
    '     its items level 1 -- so a host that never uses the tree API sees level 0 / indent 0
    '     / an empty rcTwisty and paints exactly as before. ---
    level           as integer                ' node depth (0 = top-level)
    hasChildren     as boolean                ' at least one deeper-level row follows this one
    isExpanded      as boolean                ' = NOT isCollapsed (both carried; isCollapsed kept
                                              ' for back-compat, isExpanded reads naturally for trees)
    indent          as integer                ' column-0 content x-offset in PIXELS (0 when tree
                                              ' indent is off); already includes the reserved
                                              ' twisty band when the twisty is shown
    rcTwisty        as RECT                   ' the expand/collapse glyph's hit/draw rect, in the
                                              ' SAME surface-client coords as rc; empty (all zero)
                                              ' for a row with no twisty (leaf, or twisty disabled)
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
    '     PsListTree_SetRowSpanColumns) collapse to columnCount = 1 whenever columns are
    '     defined: cells[0].wszText is column 0's text and cells[0].rc runs the whole
    '     column band -- column 0's own left (the same left a normal column-0 cell gets,
    '     so the spanned text lines up under column 0) to the last column's right -- so
    '     the callback's existing `for c = 0 to columnCount-1` loop draws it as one wide
    '     cell with no new branch. Selection/hot still fill the full rc. When no
    '     columns are defined the row is already full-width (columnCount = 0) and the
    '     flag only advertises the intent. Group headers are NOT spanned rows -- they
    '     have their own isHeader path and carry isSpanned = false.
    columnCount     as integer
    cells           as PSLISTTREE_CELLINFO ptr
end type

type PSLISTTREE_MESSAGEINFO
    hList           as HWND
    uMsg            as UINT
    wParam          as WPARAM
    lParam          as LPARAM
    idx             as integer    ' MODEL row index under the mouse (-1 if none)
    isCtrl          as boolean
    isShift         as boolean
end type


type PSLISTTREE_ROWINFO
    IsHeader        as boolean = false
    bCollapsed      as boolean = false
    bSpanColumns    as boolean = false    ' paint this (ordinary, selectable) row as one cell
                                          ' spanning every column; text stays in column 0
    bSelectable     as boolean = true     ' false = the row cannot be selected or focused and
                                          ' keyboard navigation skips over it (enforced on every
                                          ' path, programmatic setters included)
    level           as integer = 0        ' tree depth (0 = top-level). The tree is carried by
                                          ' pre-order/DFS array order: a node's children and its
                                          ' whole subtree are the CONTIGUOUS run of deeper-level
                                          ' rows immediately after it, so parent / child-count /
                                          ' has-children are all DERIVED (PSLISTTREE.ParentOf etc.)
                                          ' -- there is deliberately no stored parent index to
                                          ' fix up on every insert/delete/move. A legacy header
                                          ' is level 0 and its AddString items are level 1, which
                                          ' makes the old one-level grouping a depth-1 tree.
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
sub PSLISTTREE_ROWINFO.EnsureCells( byval n as integer )
    if n <= 0 then exit sub
    if ubound(this.cells) < n - 1 then redim preserve this.cells( 0 to n - 1 )
end sub

' Shrink storage to exactly n cells; n <= 0 frees the array.
sub PSLISTTREE_ROWINFO.TrimCells( byval n as integer )
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
type PaintCallbackSub as sub( byval p as PSLISTTREE_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the default listbox
' handling suppressed, FALSE to let it proceed.
' CAUTION: for WM_LBUTTONUP the result is IGNORED and the default always runs -- the
' listbox releases its mouse capture there, and swallowing it strands the capture.
type MessageCallbackFunc as function( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

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
' NOT fired for PsListTree_SetCurSel / SetSel / SelectAll / Clear: programmatic setters are
' silent (the family rule), so a host may call them from inside this callback without
' re-entering itself. Nor is it fired when the user re-selects the row that is already
' current -- only an actual change notifies.
type SelChangeCallbackSub as sub( byval hListControl as HWND, byval row as integer )

' ----------------------------------------------------------------------------------------
' Drag-and-drop row reordering (opt-in via PsListTree_SetDragReorder). The user drags a row,
' or the whole current selection, to a new position; dropping ON a header inserts the block
' as that header's first children. The control moves its OWN rows() model and notifies.
' ----------------------------------------------------------------------------------------

' Handed to the CanDrop veto just before a drop commits. Everything here is a SNAPSHOT for
' the duration of the callback -- srcRows points at control-owned scratch and targetInfo at
' a live ROWINFO; copy anything you need to keep, and do not mutate the model from inside.
type PSLISTTREE_DROPINFO
    hList        as HWND
    srcRows      as integer ptr           ' the MODEL indices being dragged (srcCount of them)
    srcCount     as integer
    insertBefore as integer               ' MODEL index the block will land before (0..rowCount)
    onHeader     as boolean               ' TRUE = dropped on a header (block becomes its first children)
    targetRow    as integer               ' MODEL index of the drop-target row, or -1 at the list end
    targetInfo   as PSLISTTREE_ROWINFO ptr ' the target row's ROWINFO (read-only), or NULL at the end
end type

' Fired just BEFORE the move commits. Return FALSE to reject the drop (the model is left
' untouched). Unset = every drop is allowed.
type CanDropCallbackFunc as function( byval p as PSLISTTREE_DROPINFO ptr ) as boolean

' Fired just AFTER a user drag-drop moves rows. newFirstRow is the MODEL index the moved
' block now starts at; count is how many rows moved. Lets a host resync any parallel data
' (walk newFirstRow..newFirstRow+count-1 with the public getters / itemData). NOT fired for
' the programmatic PsListTree_MoveRows -- setters are silent (the family rule).
type DragDropCallbackSub as sub( byval hList as HWND, byval newFirstRow as integer, byval count as integer )

' ----------------------------------------------------------------------------------------
' Tree label editing (opt-in via PsListTree_EnableLabelEdit) and node expand/collapse.
' These mirror the Win32 treeview TVN_BEGINLABELEDIT / TVN_ENDLABELEDIT contract.
' ----------------------------------------------------------------------------------------

' Fired just BEFORE an in-place edit begins. Return FALSE to veto (no editor appears).
' Unset = every edit is allowed.
type BeginLabelEditCallbackFunc as function( byval hListControl as HWND, byval row as integer ) as boolean

' Fired when the user COMMITS an edit (Enter / focus loss), with the edited text. Return
' FALSE to reject it (the row keeps its old caption); TRUE (or unset) writes newText into
' the row. Not fired on cancel (Esc). newText is byval -- never byref const (a copied
' const DWSTRING corrupts the heap; see Learnings.md).
type EndLabelEditCallbackFunc as function( byval hListControl as HWND, byval row as integer, byval newText as DWSTRING ) as boolean

' The USER expanded or collapsed a node -- by clicking its twisty, by Left/Right, or by
' clicking a legacy header. bExpanded is the NEW state. NOT fired for the programmatic
' CollapseRow / ExpandRow / ToggleRow / CollapseAll / ExpandAll / SetNodeCollapsed setters
' (the family "silent setters" rule, as SelChangeCallback already follows).
type ExpandCollapseCallbackSub as sub( byval hListControl as HWND, byval row as integer, byval bExpanded as boolean )

type PSLISTTREE
    hWin            as HWND
    hToolTip        as HWND
    wszTooltip      as DWSTRING
    ' --- Model: rows() is the backing store (capacity = ubound+1); rowCount is
    '     the number of logical rows and the single source of truth for "count". ---
    rows(any)       as PSLISTTREE_ROWINFO
    rowCount        as integer = 0
    ' --- View: visibleMap(v) -> model row index, for v = 0..visibleCount-1.
    '     Rebuilt on any change / collapse-expand. ---
    visibleMap(any) as integer
    visibleCount    as integer = 0
    updateDepth     as integer = 0        ' BeginUpdate/EndUpdate nesting (defers refresh)
    RowHeight       as integer = 22
    idc_ListTree     as integer = 1000
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
    ' reconciled in PsListTree_SyncViewFromModel, exactly where LB_SETTOPINDEX used to be.
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
    bSwallowNextChar as boolean = false   ' armed in WM_KEYDOWN for a claimed key that produces a
                                          ' WM_CHAR (Space); the WM_CHAR arm consumes it so the
                                          ' system does not beep (the CLAUDE.md WM_CHAR rule)
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
    ' Named hRowFont, NOT hFont: a field named hFont case-insensitively shadows the HFONT
    ' type within this TYPE body, so a SECOND `... as HFONT` field (hTwistyFont below) would
    ' fail to compile. Renaming the row-font field frees the type name (Learnings.md,
    ' PsDatePicker). Public surface is unchanged -- PsListTree_SetFont still sets this.
    hRowFont        as HFONT              ' caller-supplied font for row text (caller owns it)
    ' --- Tree appearance. Both switches default OFF and every field is zero-effect until a
    '     host opts in, which is what keeps existing header/flat hosts pixel-identical. ---
    bTreeIndent     as boolean = false    ' auto-indent column-0 content by depth (SetTreeIndent)
    IndentWidth     as integer = 16       ' per-level indent, UNSCALED units (like RowHeight)
    nIndentPx       as integer = 0        ' cached DPI-scaled indent; re-derived like nItemHeightPx
    bShowTwisty     as boolean = false    ' control draws the expand/collapse glyph + owns its hit-rect
    TwistyWidth     as integer = 16       ' reserved twisty band, UNSCALED units
    nTwistyPx       as integer = 0        ' cached DPI-scaled twisty band width
    TwistyColor     as COLORREF = &h00808080  ' glyph colour (mid-grey default; SetTwistyColor overrides)
    hTwistyFont     as HFONT              ' symbol font for the glyph (borrowed, like hRowFont)
    wszGlyphExpanded  as DWSTRING             ' shown on an EXPANDED parent (default set at Create)
    wszGlyphCollapsed as DWSTRING             ' shown on a COLLAPSED parent
    ' --- In-place label editor (opt-in). The PsTextBox child is created lazily on the first
    '     edit and destroyed on commit/cancel; hEdit = 0 whenever no edit is in progress.
    '     Turning bLabelEdit on is what establishes the PsListTree_FilterMessage pump
    '     obligation -- a host that never enables editing needs no pump call. ---
    bLabelEdit      as boolean = false    ' EnableLabelEdit: master switch for in-place editing
    bClickToEdit    as boolean = false    ' single click on the already-current row starts an edit (default OFF)
    hEdit           as HWND               ' the PsTextBox editor child (0 = not editing)
    editRow         as integer = -1       ' MODEL row being edited (-1 = none)
    editCol         as integer = 0        ' column being edited (0 = caption; >0 reserved for later)
    bEditTearingDown as boolean = false   ' guards the re-entrant commit that DestroyWindow's focus loss triggers
    BeginLabelEditCallback as BeginLabelEditCallbackFunc  ' optional pre-edit veto
    EndLabelEditCallback   as EndLabelEditCallbackFunc    ' optional commit accept/reject
    ExpandCollapseCallback as ExpandCollapseCallbackSub   ' optional user-toggle notify
    ' --- Owner-drawn vertical scrollbar, created and driven by this control. It is
    '     auto-hidden whenever the visible rows fit, and the listbox then reclaims the
    '     full client width. ---
    hScrollBar      as HWND
    ScrollBarWidth  as integer = CVSCROLL_DEFAULT_WIDTH   ' DPI-scaled when laid out
    scrollBarShown  as boolean = false
    ' --- Optional column header band (PsColumnHeader), created hidden and owned by this
    '     control. The embedded header instance is the SINGLE store for column
    '     definitions and geometry -- PsListTree keeps no column state; the PsListTree_*
    '     column wrappers delegate, and OnDrawItem reads cell x-coordinates from the
    '     header's rects. The control owns the header's WidthChanged slot (an internal
    '     chain hook); hosts use PsListTree_SetColumnResizeCallback. ---
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
    paintCells(any) as PSLISTTREE_CELLINFO
    ' Single-cell scratch for a spanned row (isSpanned). Kept separate from paintCells so
    ' a spanned row never overwrites the shared column x-geometry that later normal rows
    ' in the same repaint still read. Valid only for the duration of one callback.
    spanCell        as PSLISTTREE_CELLINFO

    declare destructor()
    declare sub      EnsurePaintCells( byval n as integer )
    declare function GetCount() as integer                                  ' model row count
    declare function GetVisibleCount() as integer
    declare function AddRow() as PSLISTTREE_ROWINFO ptr                       ' append
    declare function InsertRowAt( byval modelRow as integer ) as PSLISTTREE_ROWINFO ptr
    declare function DeleteRowAt( byval modelRow as integer ) as boolean
    declare sub      Clear()
    declare function GetRow( byval row as integer ) as PSLISTTREE_ROWINFO ptr
    declare function IsValidRow( byval row as integer ) as boolean
    ' --- Tree structure, all DERIVED from the flat rows() + level field (pre-order order). ---
    declare function GetNodeLevel( byval row as integer ) as integer      ' rows(row).level, -1 invalid
    declare function HasChildrenAt( byval row as integer ) as boolean     ' next row is deeper
    declare function ChildCountAt( byval row as integer ) as integer      ' # DIRECT children
    declare function ParentOf( byval row as integer ) as integer          ' first shallower row before, else -1
    declare function LastDescendantOf( byval row as integer ) as integer  ' end of this node's subtree (=row if leaf)
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

' Defined in PsListTree.inc, but PSLISTTREE.Refresh (below) has to call it -- push the
' scrollbar's range/visibility to match the current model + scroll position.
declare sub      PsListTree_SyncScrollBar( byval pList as PSLISTTREE ptr )
declare sub      PsListTree_SyncViewFromModel( byval pList as PSLISTTREE ptr )
declare sub      PsListTree_CaptureTopRow( byval pList as PSLISTTREE ptr )
declare function PsListTree_ItemsPerPage( byval pList as PSLISTTREE ptr ) as integer
declare function PsListTree_PositionWindows( byval hwnd as HWND ) as LRESULT

' The one-row back buffer (EnsureCache / FreeCache and its five fields) is GONE. It existed
' solely so WM_DRAWITEM would not create and destroy a compatible DC and bitmap for every
' row on every repaint. With the whole surface painted in a single buffer there is no
' per-row buffer to cache, and its disappearance is a good part of why the rewrite made
' the GDI and GDI+ backends faster too.
destructor PSLISTTREE()
end destructor

' Size the PAINTINFO.cells scratch to exactly n entries (only re-dims when the column
' count actually changed, so the per-row cost is a compare).
sub PSLISTTREE.EnsurePaintCells( byval n as integer )
    if n <= 0 then
        erase this.paintCells
    elseif ubound(this.paintCells) + 1 <> n then
        redim this.paintCells( 0 to n - 1 )
    end if
end sub

function PSLISTTREE.GetCount() as integer
    return this.rowCount
end function

function PSLISTTREE.GetVisibleCount() as integer
    return this.visibleCount
end function

function PSLISTTREE.IsValidRow( byval row as integer ) as boolean
    return (row >= 0) andalso (row < this.rowCount)
end function

function PSLISTTREE.GetRow( byval row as integer ) as PSLISTTREE_ROWINFO ptr
    if this.IsValidRow(row) = false then return null
    return @this.rows(row)
end function

' Insert a fresh (reset) row at modelRow, shifting later rows up. Grows the
' backing store by doubling so bulk inserts are amortized O(1), not O(n^2).
function PSLISTTREE.InsertRowAt( byval modelRow as integer ) as PSLISTTREE_ROWINFO ptr
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
        .level         = 0            ' recycled slot: a stale depth must not survive
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

function PSLISTTREE.AddRow() as PSLISTTREE_ROWINFO ptr
    return this.InsertRowAt( this.rowCount )
end function

function PSLISTTREE.DeleteRowAt( byval modelRow as integer ) as boolean
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

sub PSLISTTREE.Clear()
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

' Rebuild the visible map from the model. A row is hidden iff it lies inside the subtree of
' a collapsed ancestor. Tracked with ONE integer -- the level of the shallowest currently
' active collapse: everything DEEPER than it is hidden, and the moment a row at that level
' or shallower appears we have left the collapsed subtree. No stack is needed because a
' collapse deeper than an already-active one changes nothing about what is hidden.
'
' This GENERALIZES the old single-boolean header walk and reduces to it exactly for legacy
' data: a level-0 header with bCollapsed sets hideBelow = 0, hiding every following level>0
' row until the next level-0 row -- byte-identical to "nearest preceding header collapsed".
' A flat list (all level 0, never collapsed) leaves every row visible.
'
' Pure model: pushing the new count into the view is PsListTree_SyncViewFromModel's job.
sub PSLISTTREE.RebuildVisibleMap()
    this.visibleCount = 0
    if this.rowCount > 0 then
        redim this.visibleMap( 0 to this.rowCount - 1 )
        dim as integer vis = 0
        dim as integer hideBelow = &h7FFFFFFF          ' no collapse active (nothing is deeper than this)
        for i as integer = 0 to this.rowCount - 1
            if this.rows(i).level > hideBelow then continue for   ' inside a collapsed subtree -> hidden
            hideBelow = &h7FFFFFFF                       ' at/above the collapse level -> we have exited it
            this.visibleMap(vis) = i : vis += 1
            ' A collapsed node hides everything deeper than itself. (Unconditional on
            ' bCollapsed, matching the old code: an empty collapsed header still "collapses".)
            if this.rows(i).bCollapsed then hideBelow = this.rows(i).level
        next
        this.visibleCount = vis
    else
        erase this.visibleMap
    end if
end sub

' --- Derived tree structure. The tree lives in the pre-order rows() array + the per-row
'     level field; parent / children / subtree extent are all computed, never stored. ---

' rows(row).level, or -1 for an invalid row.
function PSLISTTREE.GetNodeLevel( byval row as integer ) as integer
    if this.IsValidRow(row) = false then return -1
    return this.rows(row).level
end function

' TRUE when the row immediately after `row` is deeper -- i.e. `row` has at least one child.
function PSLISTTREE.HasChildrenAt( byval row as integer ) as boolean
    if this.IsValidRow(row) = false then return false
    dim as integer nxt = row + 1
    if nxt >= this.rowCount then return false
    return (this.rows(nxt).level > this.rows(row).level)
end function

' Number of DIRECT children: rows in the contiguous deeper run whose level is exactly
' parent.level + 1 (grandchildren sit between them but are skipped by the level test).
function PSLISTTREE.ChildCountAt( byval row as integer ) as integer
    if this.IsValidRow(row) = false then return 0
    dim as integer lvl = this.rows(row).level
    dim as integer n = 0
    for i as integer = row + 1 to this.rowCount - 1
        if this.rows(i).level <= lvl then exit for       ' left the subtree
        if this.rows(i).level = lvl + 1 then n += 1       ' a direct child
    next
    return n
end function

' The nearest preceding row shallower than `row` -- its parent -- or -1 for a top-level node.
function PSLISTTREE.ParentOf( byval row as integer ) as integer
    if this.IsValidRow(row) = false then return -1
    dim as integer lvl = this.rows(row).level
    if lvl <= 0 then return -1
    for i as integer = row - 1 to 0 step -1
        if this.rows(i).level < lvl then return i
    next
    return -1
end function

' The last row of `row`'s subtree (the contiguous deeper run), or `row` itself for a leaf.
' This is the insertion point math for AddNode (subtree end + 1) and for deleting a subtree.
function PSLISTTREE.LastDescendantOf( byval row as integer ) as integer
    if this.IsValidRow(row) = false then return row
    dim as integer lvl = this.rows(row).level
    dim as integer last = row
    for i as integer = row + 1 to this.rowCount - 1
        if this.rows(i).level <= lvl then exit for
        last = i
    next
    return last
end function

function PSLISTTREE.ModelToVisible( byval modelRow as integer ) as integer
    for v as integer = 0 to this.visibleCount - 1
        if this.visibleMap(v) = modelRow then return v
    next
    return -1
end function

function PSLISTTREE.VisibleToModel( byval visRow as integer ) as integer
    if (visRow < 0) orelse (visRow >= this.visibleCount) then return -1
    return this.visibleMap(visRow)
end function

' --- Selection is stored per-row in the model, so it survives collapse/expand
'     index shifts and can include hidden rows and headers. ---
function PSLISTTREE.IsRowSelected( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    return this.rows(modelRow).selected
end function

' A row the host has NOT marked non-selectable. Invalid rows read false (they cannot be
' selected anyway). This is the single predicate every selection/focus path consults, so
' the "non-selectable = never selected, never focused" invariant lives in one place.
function PSLISTTREE.IsRowSelectable( byval modelRow as integer ) as boolean
    if this.IsValidRow(modelRow) = false then return false
    return this.rows(modelRow).bSelectable
end function

' Setting selected = true is refused for a non-selectable row (the model-level guard, so
' every caller inherits it); clearing is always allowed.
sub PSLISTTREE.SetRowSelected( byval modelRow as integer, byval state as boolean )
    if this.IsValidRow(modelRow) = false then exit sub
    if state andalso (this.rows(modelRow).bSelectable = false) then exit sub
    this.rows(modelRow).selected = state
end sub

sub PSLISTTREE.ClearSelection()
    for i as integer = 0 to this.rowCount - 1
        this.rows(i).selected = false
    next
end sub

sub PSLISTTREE.SelectOnly( byval modelRow as integer )
    this.ClearSelection()
    if this.IsValidRow(modelRow) andalso this.rows(modelRow).bSelectable then this.rows(modelRow).selected = true
end sub

sub PSLISTTREE.SelectRange( byval a as integer, byval b as integer )
    if a > b then swap a, b
    if a < 0 then a = 0
    if b > this.rowCount - 1 then b = this.rowCount - 1
    ' A Shift-range selects every SELECTABLE row it spans and skips the rest.
    for i as integer = a to b
        if this.rows(i).bSelectable then this.rows(i).selected = true
    next
end sub

function PSLISTTREE.GetSelCount() as integer
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
function PSLISTTREE.MoveRows( srcRows() as integer, byval insertBefore as integer ) as integer
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
    dim tmp(0 to n - 1) as PSLISTTREE_ROWINFO
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

sub PSLISTTREE.BeginUpdate()
    this.updateDepth += 1
end sub

sub PSLISTTREE.EndUpdate()
    if this.updateDepth > 0 then this.updateDepth -= 1
    if this.updateDepth = 0 then this.Refresh()
end sub

' Called by every model mutator. Coalesces into a single Refresh when a
' BeginUpdate/EndUpdate batch is active.
sub PSLISTTREE.NotifyChange()
    if this.updateDepth = 0 then this.Refresh()
end sub

sub PSLISTTREE.Refresh()
    ' Capture the listbox's actual scroll position back into the model BEFORE the
    ' rebuild: the sync below pushes LB_SETCOUNT (which resets the Win32 listbox's
    ' top and caret), and the OLD visibleMap still matches the listbox contents at
    ' this point, so the translation is valid. This is what lets a scroll made by
    ' the user (wheel / keyboard / scrollbar) survive any rebuild.
    PsListTree_CaptureTopRow( @this )
    this.RebuildVisibleMap()
    dim as HWND hList = GetDlgItem( this.hWin, this.idc_ListTree )
    if hList = 0 then exit sub
    ShowWindow( hList, SW_SHOW )
    ' Re-derive the Win32 listbox (count, caret, top row) and the scrollbar from
    ' the model, then repaint WITH background erase so the vacated region below the
    ' last row is cleared when the list shrinks (delete / collapse).
    PsListTree_SyncViewFromModel( @this )
    InvalidateRect( hList, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every PsListTree_* function takes the handle returned by PsListTree_Create(). That handle
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
' GROUPS AND TREES
'   Every row carries a tree LEVEL (0 = top-level). The tree is held by pre-order array
'   order: a node's whole subtree is the contiguous run of deeper-level rows right after it.
'   Collapsing a node hides its entire subtree. Build depth with AddNode/InsertNode, or use
'   the legacy AddHeader + AddString to build a depth-1 group (a header is a level-0 parent,
'   its items its level-1 children) -- the two produce the same structure. "Header" is now
'   just a STYLING flag (PsListTree_IsHeader): headers are selectable, are returned by
'   GetSelItems, and clicking one toggles the whole row; a plain AddNode parent toggles only
'   via its twisty. See "Tree appearance" for the indent/twisty visuals (off by default).
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
declare function PsListTree_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

' ----------------------------------------------------------------------------------------
' Adding / removing rows.  Add* and Insert* return the new row's model index, or -1.
' Wrap bulk loads in BeginUpdate/EndUpdate: it collapses the per-row rebuild+repaint into
' one, turning an O(n^2) load into O(n). The pairs nest.
' ----------------------------------------------------------------------------------------
declare function PsListTree_AddString( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListTree_AddHeader( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListTree_InsertString( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
' Tree insertion. AddNode appends a child under parentRow (parentRow = -1 appends a new
' top-level node); the child lands after parentRow's whole existing subtree, at
' level = parent.level + 1. InsertNode inserts among parentRow's DIRECT children at
' childIndex (clamped; childIndex >= child count appends). Both return the new row's MODEL
' index or -1. AddString/AddHeader/InsertString are unchanged and still build legacy
' one-level groups; use these for real depth.
declare function PsListTree_AddNode( byval hListControl as HWND, byval parentRow as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListTree_InsertNode( byval hListControl as HWND, byval parentRow as integer, byval childIndex as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
declare function PsListTree_DeleteString( byval hListControl as HWND, byval row as integer ) as boolean
declare sub      PsListTree_Clear( byval hListControl as HWND )
declare sub      PsListTree_BeginUpdate( byval hListControl as HWND )
declare sub      PsListTree_EndUpdate( byval hListControl as HWND )
declare sub      PsListTree_Refresh( byval hListControl as HWND )

' ----------------------------------------------------------------------------------------
' Counts.  GetCount = every row in the model. GetVisibleCount = rows currently on show
' (headers + items of expanded groups). They differ whenever anything is collapsed.
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetCount( byval hListControl as HWND ) as integer
declare function PsListTree_GetVisibleCount( byval hListControl as HWND ) as integer

' ----------------------------------------------------------------------------------------
' Row contents.  Set* return FALSE for an invalid row index.
'   Cells: column 0 IS the row's Text (GetText/SetText and the cell APIs with col = 0
'   are interchangeable). Higher columns are stored sparsely per row -- any cell never
'   set reads back "", and cell text is independent of the column DEFINITIONS, so rows
'   can be populated before or after columns are added. col < 0 fails; col beyond the
'   defined columns is legal storage (it paints once a matching column exists).
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetText( byval hListControl as HWND, byval row as integer ) as DWSTRING
declare function PsListTree_SetText( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING ) as boolean
declare function PsListTree_GetCellText( byval hListControl as HWND, byval row as integer, byval col as integer ) as DWSTRING
declare function PsListTree_SetCellText( byval hListControl as HWND, byval row as integer, byval col as integer, byval Text as DWSTRING ) as boolean
declare function PsListTree_GetItemData( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_SetItemData( byval hListControl as HWND, byval row as integer, byval itemData as integer ) as boolean
declare function PsListTree_GetItemDataExtra( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_SetItemDataExtra( byval hListControl as HWND, byval row as integer, byval itemDataExtra as integer ) as boolean

' Make an ordinary (selectable) row paint as a single cell spanning every column instead
' of one cell per column. The row's text stays in column 0 (GetText / GetCellText(row,0));
' the paint callback receives columnCount = 1 with cells[0].rc covering the whole column
' band and p->isSpanned = true. Silent (programmatic setter); repaints. Get returns the
' flag; both return FALSE for an invalid row.
declare function PsListTree_SetRowSpanColumns( byval hListControl as HWND, byval row as integer, byval bSpan as boolean = true ) as boolean
declare function PsListTree_GetRowSpanColumns( byval hListControl as HWND, byval row as integer ) as boolean

' Make a row non-selectable (bSelectable = false): it cannot be selected or focused by any
' path -- mouse, keyboard and the programmatic setters alike -- and keyboard navigation
' skips over it. Turning a currently selected/focused row non-selectable clears its
' selection and drops the caret. Silent (programmatic setter); repaints. Get returns the
' flag (default TRUE); both return FALSE for an invalid row.
declare function PsListTree_SetRowSelectable( byval hListControl as HWND, byval row as integer, byval bSelectable as boolean = true ) as boolean
declare function PsListTree_GetRowSelectable( byval hListControl as HWND, byval row as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Groups / collapsing.  Collapse/Expand/Toggle act on any node that CAN collapse -- a legacy
' header OR a tree node with children -- and return FALSE for a leaf item or an invalid row.
' A mouse click on a legacy header toggles the whole row; on a plain tree parent only the
' twisty toggles (the label selects). The keyboard uses Left/Right at any depth.
' ----------------------------------------------------------------------------------------
declare function PsListTree_IsHeader( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_IsCollapsed( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_CollapseRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_ExpandRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_ToggleRow( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_SetNodeCollapsed( byval hListControl as HWND, byval row as integer, byval bCollapsed as boolean ) as boolean
declare function PsListTree_CollapseAll( byval hListControl as HWND ) as boolean
declare function PsListTree_ExpandAll( byval hListControl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Tree structure queries (MODEL indices). Level 0 = top-level. Parent = -1 for a top-level
' node. These read the derived tree structure; they work whether the nodes were built with
' AddNode (real depth) or AddHeader/AddString (a depth-1 group).
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetLevel( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_GetParent( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_GetChildCount( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_HasChildren( byval hListControl as HWND, byval row as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Selection.  Selection is stored on the rows, so it survives collapse/expand and can
' include hidden rows and headers.
'   Modes are mutually exclusive; both off = single-select:
'     SetExtendedSelect - Shift ranges / Ctrl toggles (explorer style)
'     SetMultiSelect    - every click toggles one row (checklist style)
'   GetCurSel/SetCurSel address the focused row. SetCurSel selects only that row.
'   GetSelItems reports ALL selected rows, hidden ones included, and redims selItems().
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetCurSel( byval hListControl as HWND ) as integer
declare function PsListTree_SetCurSel( byval hListControl as HWND, byval row as integer ) as integer
declare function PsListTree_GetSel( byval hListControl as HWND, byval row as integer ) as boolean
declare function PsListTree_SetSel( byval hListControl as HWND, byval row as integer, byval state as boolean ) as boolean
declare function PsListTree_GetSelCount( byval hListControl as HWND ) as integer
declare function PsListTree_GetSelItems( byval hListControl as HWND, selItems() as integer ) as integer
declare sub      PsListTree_SelectAll( byval hListControl as HWND, byval state as boolean )
declare function PsListTree_SetMultiSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function PsListTree_SetExtendedSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
declare function PsListTree_PreventDoubleClick( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_GetTopIndex( byval hListControl as HWND ) as integer
declare function PsListTree_SetTopIndex( byval hListControl as HWND, byval row as integer ) as integer

' ----------------------------------------------------------------------------------------
' Drag-and-drop row reordering.  Opt-in (default OFF). When on, the user drags a row -- or,
' if the pressed row is part of the selection, the whole selection -- to a new position;
' dropping ON a header inserts the block as that header's first children. Non-selectable
' rows and headers are not draggable. The control reorders its OWN model, firing CanDrop
' (veto) before and DragDrop (notify) after. MoveRows is the same reorder as a silent,
' programmatic call; SetDragIndicatorColor tunes the insertion line / header highlight.
' ----------------------------------------------------------------------------------------
declare function PsListTree_SetDragReorder( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_GetDragReorder( byval hListControl as HWND ) as boolean
declare sub      PsListTree_SetCanDropCallback( byval hListControl as HWND, byval usersub as CanDropCallbackFunc )
declare sub      PsListTree_SetDragDropCallback( byval hListControl as HWND, byval usersub as DragDropCallbackSub )
declare function PsListTree_MoveRows( byval hListControl as HWND, srcRows() as integer, byval insertBefore as integer ) as integer
declare function PsListTree_SetDragIndicatorColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
declare function PsListTree_GetDragIndicatorColor( byval hListControl as HWND ) as COLORREF

' ----------------------------------------------------------------------------------------
' Appearance.  Row height is in unscaled units and is DPI-scaled internally. The font is
' borrowed, never owned: keep it alive and destroy it yourself.
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetBackColor( byval hListControl as HWND ) as COLORREF
declare function PsListTree_SetBackColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
declare function PsListTree_GetRowHeight( byval hListControl as HWND ) as integer
declare function PsListTree_SetRowHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function PsListTree_GetFont( byval hListControl as HWND ) as HFONT
declare function PsListTree_SetFont( byval hListControl as HWND, byval hFont as HFONT ) as boolean
declare sub      PsListTree_SetHoverTime( byval hListControl as HWND, byval milliseconds as integer )

' ----------------------------------------------------------------------------------------
' Tree appearance.  All OFF / zero-effect by default, so a control that only builds flat
' lists or legacy groups looks and behaves exactly as before. Turn on SetTreeIndent to
' indent column-0 content by depth, and ShowTwisty to have the control draw the
' expand/collapse glyph and own its hit-rect (clicking it toggles WITHOUT selecting). The
' paint callback still receives level / hasChildren / isExpanded / rcTwisty, so a host may
' custom-paint the glyph and set ShowTwisty false to suppress the control's own draw.
' Indent/twisty widths are UNSCALED units (DPI-scaled internally, like RowHeight). Glyphs
' are symbol-font code points (default a Segoe Fluent chevron pair); the symbol font is a
' separate borrowed HFONT set with SetTwistyFont.
' ----------------------------------------------------------------------------------------
declare function PsListTree_SetTreeIndent( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_GetTreeIndent( byval hListControl as HWND ) as boolean
declare function PsListTree_SetIndentWidth( byval hListControl as HWND, byval nWidth as integer ) as integer
declare function PsListTree_GetIndentWidth( byval hListControl as HWND ) as integer
declare function PsListTree_ShowTwisty( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_IsTwistyShown( byval hListControl as HWND ) as boolean
declare sub      PsListTree_SetTwistyWidth( byval hListControl as HWND, byval nWidth as integer )
declare sub      PsListTree_SetTwistyColor( byval hListControl as HWND, byval clr as COLORREF )
declare sub      PsListTree_SetTwistyFont( byval hListControl as HWND, byval hFont as HFONT )
declare sub      PsListTree_SetTwistyGlyphs( byval hListControl as HWND, byval wszExpanded as DWSTRING, byval wszCollapsed as DWSTRING )

' ----------------------------------------------------------------------------------------
' In-place label editing.  Opt-in: EnableLabelEdit turns it on AND establishes the pump
' obligation -- a host that enables it MUST call PsListTree_FilterMessage( @msg ) in its
' message loop (it forwards to the editor's PsTextBox, whose right-click menu needs it).
' A host that never enables editing needs no pump call. An edit starts on F2 (on the
' focused row), on the programmatic BeginEdit, or -- when SetClickToEdit is on -- on a
' single click of the already-current row (explorer rename). It commits on Enter or focus
' loss and cancels on Esc. BeginLabelEdit can veto; EndLabelEdit can reject the new text.
' Only column 0 (the caption) is editable for now. All programmatic; EndEdit is silent.
' ----------------------------------------------------------------------------------------
declare function PsListTree_EnableLabelEdit( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_IsLabelEditEnabled( byval hListControl as HWND ) as boolean
declare function PsListTree_SetClickToEdit( byval hListControl as HWND, byval enable as boolean = true ) as boolean
declare function PsListTree_BeginEdit( byval hListControl as HWND, byval row as integer, byval col as integer = 0 ) as boolean
declare function PsListTree_EndEdit( byval hListControl as HWND, byval bCommit as boolean = true ) as boolean
declare function PsListTree_IsEditing( byval hListControl as HWND ) as boolean
declare function PsListTree_GetEditRow( byval hListControl as HWND ) as integer
' Call from the host message pump when label editing is enabled (see above). Returns TRUE
' if the message was consumed by the editor's context menu. Safe to call always.
declare function PsListTree_FilterMessage( byval pMsg as MSG ptr ) as boolean

' ----------------------------------------------------------------------------------------
' Vertical scrollbar.  Created, positioned, ranged and auto-hidden by this control -- it
' appears only while the rows overflow, and the listbox reclaims the width otherwise.
' Nothing here is required; it is for theming. GetScrollBar exposes the child for direct
' PsVScrollBar_* calls.
' ----------------------------------------------------------------------------------------
declare function PsListTree_GetScrollBar( byval hListControl as HWND ) as HWND
declare sub      PsListTree_SetScrollBarWidth( byval hListControl as HWND, byval nWidth as integer )
declare sub      PsListTree_SetScrollBarColors( byval hListControl as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
declare sub      PsListTree_SetScrollBarPaintCallback( byval hListControl as HWND, byval usersub as VScrollPaintCallbackSub )

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
'   PsListTree_SetColumnResizeCallback -- never PsColumnHeader_SetWidthChangedCallback on
'   the child returned by PsListTree_GetHeader. The other header callbacks (paint, click,
'   autosize, tooltip) pass straight through.
'
'   Programmatic setters are silent (family rule): SetColumnWidth repaints but fires no
'   resize callback; only user drags/autosize notify.
' ----------------------------------------------------------------------------------------
declare function PsListTree_AddColumn( byval hListControl as HWND, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsListTree_InsertColumn( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
declare function PsListTree_DeleteColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare sub      PsListTree_ClearColumns( byval hListControl as HWND )
declare function PsListTree_GetColumnCount( byval hListControl as HWND ) as integer
declare function PsListTree_GetColumnText( byval hListControl as HWND, byval idx as integer ) as DWSTRING
declare function PsListTree_SetColumnText( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
declare function PsListTree_GetColumnWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function PsListTree_SetColumnWidth( byval hListControl as HWND, byval idx as integer, byval nWidth as integer ) as boolean
declare function PsListTree_GetColumnMinWidth( byval hListControl as HWND, byval idx as integer ) as integer
declare function PsListTree_SetColumnMinWidth( byval hListControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
declare function PsListTree_GetFillColumn( byval hListControl as HWND ) as integer
declare function PsListTree_SetFillColumn( byval hListControl as HWND, byval idx as integer ) as boolean
declare function PsListTree_ShowHeader( byval hListControl as HWND, byval bShow as boolean = true ) as boolean
declare function PsListTree_IsHeaderVisible( byval hListControl as HWND ) as boolean
declare function PsListTree_GetHeaderHeight( byval hListControl as HWND ) as integer
declare function PsListTree_SetHeaderHeight( byval hListControl as HWND, byval height as integer ) as integer
declare function PsListTree_GetHeader( byval hListControl as HWND ) as HWND
declare sub      PsListTree_SetColumnResizeCallback( byval hListControl as HWND, byval usersub as HDR_WidthChangedCallbackSub )
declare sub      PsListTree_SetColumnClickCallback( byval hListControl as HWND, byval usersub as HDR_ClickCallbackSub )
declare sub      PsListTree_SetColumnAutoSizeCallback( byval hListControl as HWND, byval userfunc as HDR_AutoSizeCallbackFunc )
declare sub      PsListTree_SetHeaderPaintCallback( byval hListControl as HWND, byval usersub as HDR_PaintCallbackSub )
declare sub      PsListTree_SetHeaderTooltipCallback( byval hListControl as HWND, byval userfunc as HDR_TooltipCallbackFunc )
declare sub      PsListTree_SetHeaderBackColor( byval hListControl as HWND, byval clr as COLORREF )
declare sub      PsListTree_SetHeaderFont( byval hListControl as HWND, byval hFont as HFONT )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw one row. Required if you want to see anything.
'   MessageCallback - observe mouse messages; return TRUE to suppress default handling.
'                     NOTE: the result is ignored for WM_LBUTTONUP (see PsListTree.inc).
'   TooltipCallback - supply per-row tooltip text on demand; "" for none.
'   SelChangeCallback - the USER selected a different row (mouse OR keyboard). Silent for
'                     the programmatic setters. This is the only way to see keyboard
'                     navigation: the control consumes WM_KEYDOWN itself.
' ----------------------------------------------------------------------------------------
declare sub      PsListTree_SetPaintCallback( byval hListControl as HWND, byval usersub as PaintCallbackSub )
declare sub      PsListTree_SetMessageCallback( byval hListControl as HWND, byval userfunc as MessageCallbackFunc )
declare sub      PsListTree_SetTooltipCallback( byval hListControl as HWND, byval userfunc as TooltipCallbackFunc )
declare sub      PsListTree_SetSelChangeCallback( byval hListControl as HWND, byval usersub as SelChangeCallbackSub )
' Tree callbacks. BeginLabelEdit/EndLabelEdit gate in-place editing; ExpandCollapse reports
' a USER expand/collapse (silent for the programmatic collapse/expand setters).
declare sub      PsListTree_SetBeginLabelEditCallback( byval hListControl as HWND, byval userfunc as BeginLabelEditCallbackFunc )
declare sub      PsListTree_SetEndLabelEditCallback( byval hListControl as HWND, byval userfunc as EndLabelEditCallbackFunc )
declare sub      PsListTree_SetExpandCollapseCallback( byval hListControl as HWND, byval usersub as ExpandCollapseCallbackSub )
