
''
''  PsListBox.inc
''   

#include once "PsListBox.bi"


function GetListBoxPointer( byval hListControl as HWND ) as PSLISTBOX ptr
    ' hListBox is actually the overall parent form (ListBox is the child)
    dim pWindow as CWindow ptr = AfxCWindowPtr(hListControl)
    dim as PSLISTBOX ptr pList = null
    if pWindow then pList = cast( PSLISTBOX ptr, pWindow->UserData(0) )
    return pList
end function


' ========================================================================================
' ROW GEOMETRY -- the arithmetic that replaced the Win32 listbox.
'
' The listbox child was, in the end, a holder for three integers (top index, item height,
' item count) plus a WM_DRAWITEM dispatch loop. Everything it was actually asked for
' reduces to the four functions below. They are pure functions of the model, which is why
' the change removes far more code than it adds.
'
' All indices here are VISIBLE indices, the same space LB_GETTOPINDEX / LB_SETTOPINDEX and
' Listbox_ItemFromPoint worked in. The model/visible translation is unchanged and still
' happens at the control's public boundary.
' ========================================================================================

' Row height in pixels, DPI-scaled once. Was LB_GETITEMHEIGHT.
private function PsListBox_ItemHeightPx( byval pList as PSLISTBOX ptr ) as integer
    if pList = 0 then return 0
    if pList->nItemHeightPx > 0 then return pList->nItemHeightPx
    dim as integer h = pList->RowHeight
    dim pWindow as CWindow ptr = AfxCWindowPtr( pList->hWin )
    if pWindow then h = pWindow->ScaleY( pList->RowHeight )
    if h < 1 then h = 1
    pList->nItemHeightPx = h
    return h
end function

' How many rows fit in the surface. Was a client-height / LB_GETITEMHEIGHT division.
private function PsListBox_VisPerPage( byval pList as PSLISTBOX ptr ) as integer
    if pList = 0 then return 0
    dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hSurf = 0 then return 0
    dim as RECT rc : GetClientRect( hSurf, @rc )
    dim as integer ih = PsListBox_ItemHeightPx( pList )
    if ih < 1 then return 0
    return (rc.bottom - rc.top) \ ih
end function

' Clamp a visible top index the way LB_SETTOPINDEX did: never past the point where the
' last row sits at the bottom of the viewport, and never negative.
private function PsListBox_ClampTopVis( byval pList as PSLISTBOX ptr, byval vis as integer ) as integer
    if pList = 0 then return 0
    dim as integer maxTop = pList->visibleCount - PsListBox_VisPerPage( pList )
    if maxTop < 0 then maxTop = 0
    if vis > maxTop then vis = maxTop
    if vis < 0 then vis = 0
    return vis
end function

' Scroll to a visible top index, clamping. Returns the value actually adopted, which is
' what the old code read back out of the listbox with LB_GETTOPINDEX after setting it.
' Repaints only when something moved.
private function PsListBox_SetTopVis( byval pList as PSLISTBOX ptr, byval vis as integer ) as integer
    if pList = 0 then return 0
    dim as integer newTop = PsListBox_ClampTopVis( pList, vis )
    if newTop <> pList->nTopVis then
        pList->nTopVis = newTop
        dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
        ' Whole surface, not a scrolled band: the buffer redraws the whole client each paint.
        if hSurf then InvalidateRect( hSurf, NULL, FALSE )
    end if
    return pList->nTopVis
end function

' The rect a visible row occupies in the surface's client area. Was ListBox_GetItemRect.
' Returns FALSE for a row that is not on screen, which the caller must respect -- the old
' API returned LB_ERR for the same case.
private function PsListBox_VisIndexRect( _
            byval pList as PSLISTBOX ptr, _
            byval vis as integer, _
            byref rc as RECT _
            ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    if pList = 0 then return false
    if (vis < 0) orelse (vis >= pList->visibleCount) then return false
    dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hSurf = 0 then return false
    dim as RECT rcClient : GetClientRect( hSurf, @rcClient )
    dim as integer ih = PsListBox_ItemHeightPx( pList )
    dim as integer y  = (vis - pList->nTopVis) * ih
    if y >= rcClient.bottom then return false
    if (y + ih) <= 0 then return false
    SetRect( @rc, rcClient.left, y, rcClient.right, y + ih )
    return true
end function

' The visible row under a client point, or -1. Was Listbox_ItemFromPoint, whose HIWORD
' flagged "outside the client area" -- callers tested `hiword(idx) <> 1`. Returning -1 for
' the same cases makes that test a plain `>= 0`, which is why every call site reads more
' simply now. A point below the last row is NOT a row: the old call reported the nearest
' one, and every caller then had to discard it.
private function PsListBox_VisIndexFromPoint( _
            byval pList as PSLISTBOX ptr, _
            byval x as integer, _
            byval y as integer _
            ) as integer
    if pList = 0 then return -1
    dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hSurf = 0 then return -1
    dim as RECT rcClient : GetClientRect( hSurf, @rcClient )
    if (x < rcClient.left) orelse (x >= rcClient.right) then return -1
    if (y < rcClient.top)  orelse (y >= rcClient.bottom) then return -1
    dim as integer ih = PsListBox_ItemHeightPx( pList )
    if ih < 1 then return -1
    dim as integer vis = pList->nTopVis + (y \ ih)
    if (vis < 0) orelse (vis >= pList->visibleCount) then return -1
    return vis
end function


' Resolve a drag cursor point to a drop target. Fills:
'   insertBefore -- the MODEL index the dragged block would land before (0..rowCount)
'   onHeader     -- TRUE when the point is over a header row (drop as its first children)
'   targetRow    -- MODEL index of the row under the point, or -1 past the last row
' Rules: over a header -> insert as its first child (header+1); over an item -> its top half
' inserts before it, its bottom half before the next visible row; below the last row -> end.
private sub PsListBox_DropTargetFromPoint( _
            byval pList as PSLISTBOX ptr, _
            byval x as integer, _
            byval y as integer, _
            byref insertBefore as integer, _
            byref onHeader as boolean, _
            byref targetRow as integer )
    insertBefore = 0
    onHeader     = false
    targetRow    = -1
    if pList = 0 then exit sub
    if pList->rowCount <= 0 then exit sub

    dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hSurf = 0 then exit sub
    dim as RECT rcClient : GetClientRect( hSurf, @rcClient )
    dim as integer ih = PsListBox_ItemHeightPx( pList )
    if ih < 1 then exit sub

    ' Which visible row does y fall in? Clamp above the top to the first visible row; below
    ' the last populated row means "end of list".
    if pList->visibleCount <= 0 then
        insertBefore = pList->rowCount        ' no visible rows (all collapsed away) -> append
        exit sub
    end if

    dim as integer relRow = y \ ih
    if y < 0 then relRow = 0
    dim as integer vis = pList->nTopVis + relRow

    if vis >= pList->visibleCount then
        ' Past the last visible row: drop at the very end of the model.
        insertBefore = pList->rowCount
        targetRow    = -1
        exit sub
    end if
    if vis < pList->nTopVis then vis = pList->nTopVis

    dim as integer modelRow = pList->VisibleToModel( vis )
    if modelRow < 0 then
        insertBefore = pList->rowCount
        exit sub
    end if
    targetRow = modelRow

    if pList->rows(modelRow).IsHeader then
        ' On a header: the block becomes its first children, regardless of cursor half.
        onHeader     = true
        insertBefore = modelRow + 1
        exit sub
    end if

    ' On an item: top half inserts before it, bottom half before the next VISIBLE row.
    dim as integer rowTopY = (vis - pList->nTopVis) * ih
    dim as boolean lowerHalf = (y - rowTopY) >= (ih \ 2)
    if lowerHalf = false then
        insertBefore = modelRow
    else
        dim as integer nextVis = vis + 1
        if nextVis >= pList->visibleCount then
            insertBefore = pList->rowCount            ' after the last visible row
        else
            insertBefore = pList->VisibleToModel( nextVis )
        end if
    end if
end sub


' ========================================================================================
' Add a string (and optional itemData) to the end of the custom listbox control.
' ========================================================================================
function PsListBox_AddString( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->AddRow()
    if pRow = null then return -1
    pRow->Text = Text
    pRow->itemData = itemData
    pRow->itemDataExtra = itemDataExtra
    return pList->GetCount() - 1  ' model index of the appended row
end function


' ========================================================================================
' Append a header (group) row. Child items added after it belong to this group
' until the next header. Returns the header's model index.
' ========================================================================================
function PsListBox_AddHeader( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->AddRow()
    if pRow = null then return -1
    pRow->IsHeader = true
    pRow->Text = Text
    pRow->itemData = itemData
    pRow->itemDataExtra = itemDataExtra
    return pList->GetCount() - 1
end function


' ========================================================================================
' Insert a row at the given MODEL index (existing rows shift down). Returns the
' actual index used (clamped to [0, count]), or -1 on failure.
' ========================================================================================
function PsListBox_InsertString( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as integer at = row
    if at < 0 then at = 0
    if at > pList->GetCount() then at = pList->GetCount()
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->InsertRowAt( at )
    if pRow = null then return -1
    pRow->Text = Text
    pRow->itemData = itemData
    pRow->itemDataExtra = itemDataExtra
    return at
end function


' ========================================================================================
' Delete the row at the given MODEL index. Returns TRUE on success.
' ========================================================================================
function PsListBox_DeleteString( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->DeleteRowAt( row )
end function


' ========================================================================================
' Remove all rows from the control.
' ========================================================================================
sub PsListBox_Clear( byval hListControl as HWND )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->Clear()
end sub


' ========================================================================================
' Row type / collapse queries and operations. Collapse/Expand/Toggle only act on
' header rows (return FALSE for non-headers or invalid rows).
' ========================================================================================
function PsListBox_IsHeader( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if pRow = null then return false
    return pRow->IsHeader
end function

function PsListBox_IsCollapsed( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if pRow = null then return false
    return pRow->bCollapsed
end function

function PsListBox_CollapseRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    if pRow->bCollapsed = false then
        pRow->bCollapsed = true
        pList->NotifyChange()
    end if
    return true
end function

function PsListBox_ExpandRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    if pRow->bCollapsed then
        pRow->bCollapsed = false
        pList->NotifyChange()
    end if
    return true
end function

function PsListBox_ExpandAll( byval hListControl as HWND ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow
    for i as integer = 0 to pList->rowCount - 1
        pRow = pList->GetRow(i)
        if (pRow = null) orelse (pRow->IsHeader = false) then continue for
        if pRow->bCollapsed then pRow->bCollapsed = false
    next
    pList->NotifyChange()
    return true
end function

function PsListBox_CollapseAll( byval hListControl as HWND ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow
    for i as integer = 0 to pList->rowCount - 1
        pRow = pList->GetRow(i)
        if (pRow = null) orelse (pRow->IsHeader = false) then continue for
        if pRow->bCollapsed = false then pRow->bCollapsed = true
    next
    pList->NotifyChange()
    return true
end function


function PsListBox_ToggleRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    pRow->bCollapsed = not pRow->bCollapsed
    pList->NotifyChange()
    return true
end function


' ========================================================================================
' Batch multiple model changes into a single visible-map rebuild + repaint. Wrap
' bulk inserts in BeginUpdate/EndUpdate to keep loading O(n) instead of O(n^2).
' ========================================================================================
sub PsListBox_BeginUpdate( byval hListControl as HWND )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->BeginUpdate()
end sub

sub PsListBox_EndUpdate( byval hListControl as HWND )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->EndUpdate()
end sub


' ========================================================================================
' Number of currently visible rows (headers + non-collapsed items).
' ========================================================================================
function PsListBox_GetVisibleCount( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetVisibleCount()
end function


' ========================================================================================
' Set specified row text in the custom listbox control. Returns TRUE on success,
' FALSE if the row index is invalid.
' ========================================================================================
function PsListBox_SetText( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) then
        pList->GetRow(row)->Text = Text
        return true     ' success
    end if
    return false  ' invalid row
end function


' ========================================================================================
' Retrieve specified row text from the custom listbox control.
' ========================================================================================
function PsListBox_GetText( byval hListControl as HWND, byval row as integer ) as DWSTRING
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return ""
    if pList->IsValidRow(row) = false then return ""
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow(row)
    dim as DWSTRING wszText = pRow->Text
    return wszText
end function


' ========================================================================================
' Per-cell text. Column 0 aliases the row's Text (the pre-columns API); higher columns
' live in the row's sparse cells() storage -- see the .bi contract. Storage is grown by
' the ROWINFO member sub because fbc cannot parse `redim rows(i).cells(...)`.
' ========================================================================================
function PsListBox_SetCellText( byval hListControl as HWND, byval row as integer, byval col as integer, byval Text as DWSTRING ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    if col < 0 then return false
    if col = 0 then
        pList->GetRow(row)->Text = Text
    else
        pList->rows(row).EnsureCells( col )
        pList->rows(row).cells(col - 1) = Text
    end if
    return true
end function


function PsListBox_GetCellText( byval hListControl as HWND, byval row as integer, byval col as integer ) as DWSTRING
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return ""
    if pList->IsValidRow(row) = false then return ""
    if col < 0 then return ""
    if col = 0 then return PsListBox_GetText( hListControl, row )
    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow(row)
    if col - 1 > ubound(pRow->cells) then return ""   ' sparse: never-set cells read ""
    dim as DWSTRING wszText = pRow->cells(col - 1)
    return wszText
end function


' ========================================================================================
' Row column-span. A spanned row is an ordinary, selectable row that paints as ONE cell
' across the whole column band (its text stays in column 0). See the PAINTINFO contract in
' the .bi. Programmatic (silent); the change repaints so a visible row reflows immediately.
' ========================================================================================
function PsListBox_SetRowSpanColumns( byval hListControl as HWND, byval row as integer, byval bSpan as boolean = true ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    if pList->rows(row).bSpanColumns = bSpan then return true   ' no change, no repaint
    pList->rows(row).bSpanColumns = bSpan
    ' Repaint (no erase: the row callback fills every pixel). No-op inside a BeginUpdate
    ' batch, whose EndUpdate covers it. Inlined rather than calling the private
    ' PsListBox_InvalidateRowsForColumns, which is defined further down this file.
    if pList->updateDepth = 0 then
        dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
        if hList then InvalidateRect( hList, NULL, FALSE )
    end if
    return true
end function

function PsListBox_GetRowSpanColumns( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    return pList->rows(row).bSpanColumns
end function


' ========================================================================================
' Row selectability. A non-selectable row cannot be selected or focused by ANY path (the
' invariant is enforced in SetRowSelected / SelectOnly / SelectRange and the four public
' setters) and keyboard navigation skips it. See the .bi contract.
' ========================================================================================
function PsListBox_SetRowSelectable( byval hListControl as HWND, byval row as integer, byval bSelectable as boolean = true ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    if pList->rows(row).bSelectable = bSelectable then return true   ' no change, no repaint
    pList->rows(row).bSelectable = bSelectable
    if bSelectable = false then
        ' Enforce the invariant now: a row that can no longer be selected must not stay
        ' selected or keep the caret/anchor. Silent (programmatic), but it repaints below.
        pList->rows(row).selected = false
        if pList->focusRow = row then
            pList->focusRow        = -1
            pList->lastNotifiedRow = -1
        end if
        if pList->anchorRow = row then pList->anchorRow = -1
    end if
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    return true
end function

function PsListBox_GetRowSelectable( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    return pList->rows(row).bSelectable
end function


' ========================================================================================
' Drag-and-drop row reordering -- public surface. The gesture lives in the surface WndProc;
' these are the opt-in switch, the two callbacks, the colour, and the programmatic reorder.
' ========================================================================================
function PsListBox_SetDragReorder( byval hListControl as HWND, byval enable as boolean = true ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->bDragReorder = enable
    if enable = false then
        ' Turning it off mid-life must not strand an armed/active gesture or its capture.
        pList->bDragArmed  = false
        pList->bDragActive = false
        pList->dragPendCollapse = -1
    end if
    return true
end function

function PsListBox_GetDragReorder( byval hListControl as HWND ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->bDragReorder
end function

sub PsListBox_SetCanDropCallback( byval hListControl as HWND, byval usersub as CanDropCallbackFunc )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->CanDropCallback = usersub
end sub

sub PsListBox_SetDragDropCallback( byval hListControl as HWND, byval usersub as DragDropCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->DragDropCallback = usersub
end sub

' Programmatic reorder: move srcRows() so they land contiguously before insertBefore (a MODEL
' index; GetCount = append). Silent (the family rule -- no DragDrop callback). Repaints.
' Returns the block's new first MODEL index, or -1 if nothing valid moved.
function PsListBox_MoveRows( byval hListControl as HWND, srcRows() as integer, byval insertBefore as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    ' The model method calls NotifyChange (-> Refresh) itself, like every other mutator.
    return pList->MoveRows( srcRows(), insertBefore )
end function

function PsListBox_SetDragIndicatorColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    dim as COLORREF prev = pList->dragIndicatorColor
    pList->dragIndicatorColor = clr
    return prev
end function

function PsListBox_GetDragIndicatorColor( byval hListControl as HWND ) as COLORREF
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->dragIndicatorColor
end function


' ========================================================================================
' Set the user defined row painting callback for the custom listbox control.
' ========================================================================================
sub PsListBox_SetPaintCallback( byval hListControl as HWND, byval usersub as PaintCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 
    pList->PaintCallback = usersub
end sub


' ========================================================================================
' Set the user defined message callback for the custom listbox control.
' ========================================================================================
sub PsListBox_SetMessageCallback( byval hListControl as HWND, byval userfunc as MessageCallbackFunc )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->MessageCallback = userfunc
end sub


' ========================================================================================
' Set the on-demand tooltip text callback. If none is set, the tooltip shows the
' hovered row's Text. Return "" from the callback to suppress the tooltip.
' ========================================================================================
sub PsListBox_SetTooltipCallback( byval hListControl as HWND, byval userfunc as TooltipCallbackFunc )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->TooltipCallback = userfunc
end sub


' ========================================================================================
' Set the selection-changed callback. Fired only when the USER moves the selection -- with
' the mouse or the keyboard -- never by PsListBox_SetCurSel/SetSel/SelectAll/Clear.
'
' This is the only way to observe keyboard navigation: the control consumes WM_KEYDOWN
' itself, so arrow/Page/Home/End moves never reach the message callback.
' ========================================================================================
sub PsListBox_SetSelChangeCallback( byval hListControl as HWND, byval usersub as SelChangeCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->SelChangeCallback = usersub
end sub


' Resolve the tooltip text for a model row: host callback if set, else row Text.
function PsListBox_ResolveTooltipText( byval pList as PSLISTBOX ptr, byval modelRow as integer ) as DWSTRING
    if (pList = null) orelse (pList->IsValidRow(modelRow) = false) then return ""
    if pList->TooltipCallback then return pList->TooltipCallback( pList->hWin, modelRow )
    return pList->rows(modelRow).Text
end function


' ========================================================================================
' Set the tooltip hovertime for the custom listbox control.
' ========================================================================================
sub PsListBox_SetHoverTime( byval hListControl as HWND, byval milliseconds as integer )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 
    pList->HoverTime = milliseconds
end sub


' ========================================================================================
' Set specified row itemData for the custom listbox control.
' Returns TRUE on success, FALSE if the row index is invalid.
' ========================================================================================
function PsListBox_SetItemData( byval hListControl as HWND, byval row as integer, byval itemData as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) then
        pList->GetRow(row)->itemData = itemData
        return true     ' success
    end if
    return false  ' invalid row
end function

function PsListBox_SetItemDataExtra( byval hListControl as HWND, byval row as integer, byval itemDataExtra as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) then
        pList->GetRow(row)->itemDataExtra = itemDataExtra
        return true     ' success
    end if
    return false  ' invalid row
end function


' ========================================================================================
' Retrieve specified row itemData from the custom listbox control.
' ========================================================================================
function PsListBox_GetItemData( byval hListControl as HWND, byval row as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    if pList->IsValidRow(row) = false then return 0
    return pList->GetRow(row)->itemData
end function

function PsListBox_GetItemDataExtra( byval hListControl as HWND, byval row as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    if pList->IsValidRow(row) = false then return 0
    return pList->GetRow(row)->itemDataExtra
end function


' ========================================================================================
' Retrieve the focused/current row (MODEL index), or -1 if none.
' ========================================================================================
function PsListBox_GetCurSel( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    return pList->focusRow
end function


' ========================================================================================
' Focus and select a single row by MODEL index (clears any other selection).
' Returns the row, or -1 if the index is invalid.
' ========================================================================================
function PsListBox_SetCurSel( byval hListControl as HWND, byval row as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if pList->IsValidRow(row) = false then
        pList->ClearSelection()
        pList->focusRow  = -1
        pList->anchorRow = -1
        ' Silent (this is the host setting the selection, not the user) -- but track it,
        ' or the next genuine user selection of this same row would be swallowed as a
        ' no-change. Same reasoning below.
        pList->lastNotifiedRow = -1
        PsListBox_SyncViewFromModel( pList )
        if hList then InvalidateRect( hList, NULL, FALSE )
        return -1
    end if
    ' A non-selectable row cannot become the current row -- leave the selection untouched
    ' and report failure (the host asked for something the invariant forbids).
    if pList->IsRowSelectable(row) = false then return -1
    pList->SelectOnly( row )
    pList->focusRow  = row
    pList->anchorRow = row
    pList->lastNotifiedRow = row
    ' Ensure-visible, decided in the MODEL: if the new caret is outside the current
    ' page, move topRow so the caret lands on-page (above -> caret at top; below ->
    ' caret at bottom). The view is then re-derived, never asked to scroll on its own.
    ' A row hidden under a collapsed header stays where it is.
    dim as integer visRow = pList->ModelToVisible( row )
    if (hList <> 0) andalso (visRow >= 0) then
        dim as integer visTop  = pList->nTopVis
        dim as integer perPage = PsListBox_ItemsPerPage( pList )
        if perPage < 1 then perPage = 1
        if visRow < visTop then
            pList->topRow = row
        elseif visRow >= visTop + perPage then
            pList->topRow = pList->VisibleToModel( visRow - perPage + 1 )
        end if
    end if
    PsListBox_SyncViewFromModel( pList )
    if hList then InvalidateRect( hList, NULL, FALSE )
    return row
end function


' ========================================================================================
' Retrieve the top (first displayed) row as a MODEL index, or -1 if the list is empty
' or the control is invalid.
' Scrolling happens in VISIBLE index space, so nTopVis is translated back through the
' visible map before being returned.
' ========================================================================================
function PsListBox_GetTopIndex( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    if pList->visibleCount = 0 then return -1
    return pList->VisibleToModel( pList->nTopVis )
end function


' ========================================================================================
' Scroll the listbox so the given MODEL row is the top row, by setting the model's
' topRow and re-deriving the Win32 listbox from it. A row hidden under a collapsed
' header resolves to its group header (inside the sync), so callers may pass any valid
' model row without checking collapse state. The listbox clamps the scroll for itself
' and the clamp is mirrored back, so the returned value is the MODEL row actually at
' the top. Returns -1 if the row or control is invalid.
' ========================================================================================
function PsListBox_SetTopIndex( byval hListControl as HWND, byval row as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    if pList->IsValidRow(row) = false then return -1
    pList->topRow = row
    PsListBox_SyncViewFromModel( pList )
    return pList->topRow
end function


' ========================================================================================
' Model-based selection API. Selection lives on the rows (per-row flag), so it
' survives collapse/expand and can include headers and hidden rows.
' ========================================================================================
function PsListBox_GetSel( byval hListControl as HWND, byval row as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->IsRowSelected( row )
end function

function PsListBox_SetSel( byval hListControl as HWND, byval row as integer, byval state as boolean ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    ' Selecting a non-selectable row is refused; clearing it (state = false) is fine.
    if state andalso (pList->IsRowSelectable(row) = false) then return false
    pList->SetRowSelected( row, state )
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    return true
end function

function PsListBox_GetSelCount( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetSelCount()
end function

' Fill selItems() with the MODEL indices of every selected row (including hidden
' rows under a collapsed header). Returns the count.
function PsListBox_GetSelItems( byval hListControl as HWND, selItems() as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    dim as integer n = pList->GetSelCount()
    if n = 0 then
        erase selItems
        return 0
    end if
    redim selItems( 0 to n - 1 )
    dim as integer k = 0
    for i as integer = 0 to pList->rowCount - 1
        if pList->rows(i).selected then
            selItems(k) = i
            k += 1
        end if
    next
    return n
end function

sub PsListBox_SelectAll( byval hListControl as HWND, byval state as boolean )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    for i as integer = 0 to pList->rowCount - 1
        ' Select-all covers every SELECTABLE row; a non-selectable row is always left clear.
        if state andalso (pList->rows(i).bSelectable = false) then
            pList->rows(i).selected = false
        else
            pList->rows(i).selected = state
        end if
    next
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
end sub

' Selection modes are mutually exclusive: extended (Shift/Ctrl ranges) vs.
' multiple (each click toggles). Off = single-select.
function PsListBox_SetMultiSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->MultipleSel = enable
    if enable then pList->ExtendSel = false
    return true
end function

function PsListBox_SetExtendedSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->ExtendSel = enable
    if enable then pList->MultipleSel = false
    return true
end function

' ========================================================================================
' Opt out of double-click behavior. With CS_DBLCLKS a double-click delivers
' down, up, DBLCLK, up -- the dblclk *substitutes* for the second down, so the host
' callback sees two WM_LBUTTONUPs and acts twice. When enabled here, the dblclk gets
' the plain-click treatment in the MODEL (selection / header toggle still apply) but
' is never surfaced to the host, and the trailing second WM_LBUTTONUP is not
' forwarded either: the host observes exactly one down/up pair per double-click.
' The default listbox handling still runs for every message (it owns the mouse
' capture), so the down->up capture pairing is untouched.
' ========================================================================================
function PsListBox_PreventDoubleClick( byval hListControl as HWND, byval enable as boolean = true ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->PreventDblClick = enable
    if enable = false then pList->skipNextLBtnUp = false
    return true
end function


' ========================================================================================
' Retrieve the row height from the custom listbox control.
' ========================================================================================
function PsListBox_GetRowHeight( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->RowHeight    
end function


' ========================================================================================
' Set the row height for the custom listbox control.
' ========================================================================================
function PsListBox_SetRowHeight( byval hListControl as HWND, byval height as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    pList->RowHeight = height
    ' Recompute the scaled pixel height and repaint. This used to be an LB_SETITEMHEIGHT
    ' plus a note that WM_MEASUREITEM only fires at creation; owning the metric outright
    ' makes that whole caveat disappear.
    pList->nItemHeightPx = 0                 ' force PsListBox_ItemHeightPx to re-derive
    PsListBox_ItemHeightPx( pList )
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then AfxRedrawWindow( hList )
    ' Row height changes items-per-page, which changes the scrollbar range (and can
    ' flip whether it is needed at all).
    PsListBox_SyncScrollBar( pList )
    return height
end function


' ========================================================================================
' Retrieve the font used to paint row text.
' ========================================================================================
function PsListBox_GetFont( byval hListControl as HWND ) as HFONT
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->hFont
end function


' ========================================================================================
' Set the font used to paint row text. The caller owns the HFONT and is responsible for
' destroying it; the control only stores the handle.
' ========================================================================================
function PsListBox_SetFont( byval hListControl as HWND, byval hFont as HFONT ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->hFont = hFont
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then AfxRedrawWindow( hList )
    return true
end function


' ========================================================================================
' Owner-drawn vertical scrollbar. The control creates, positions, auto-hides and drives
' it; these just expose it for appearance tweaks.
' ========================================================================================
function PsListBox_GetScrollBar( byval hListControl as HWND ) as HWND
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->hScrollBar
end function

' Width in unscaled units; it is DPI-scaled when the control is laid out.
sub PsListBox_SetScrollBarWidth( byval hListControl as HWND, byval nWidth as integer )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if nWidth < 1 then nWidth = 1
    pList->ScrollBarWidth = nWidth
    PsListBox_PositionWindows( pList->hWin )
end sub

sub PsListBox_SetScrollBarColors( byval hListControl as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hScrollBar then PsVScrollBar_SetColors( pList->hScrollBar, backclr, foreclr, foreclrhot )
end sub

sub PsListBox_SetScrollBarPaintCallback( byval hListControl as HWND, byval usersub as VScrollPaintCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hScrollBar then PsVScrollBar_SetPaintCallback( pList->hScrollBar, usersub )
end sub


' ========================================================================================
' Columns / header band. The embedded PsColumnHeader child is the single store for column
' definitions and geometry; these wrappers delegate to it (the scrollbar-wrapper
' precedent). Column geometry mutations repaint the listbox -- every visible row's cells
' moved -- unless a BeginUpdate batch is open, whose EndUpdate repaint covers it.
' ========================================================================================

' Keep every row's cell storage aligned when a column is inserted (nDir = +1) or
' deleted (nDir = -1) at column index idx. Handles the column-0 alias boundary: column
' 0 lives in ROWINFO.Text, columns 1..N in cells(0..N-1). Storage is sparse, so a row
' storing fewer cells than the mutation point needs no shift (its later columns were
' already ""). O(rows x cols); the caller wraps it in a single repaint.
private sub PsListBox_ShiftCellColumns( byval pList as PSLISTBOX ptr, byval idx as integer, byval nDir as integer )
    if pList = null then exit sub
    if idx < 0 then exit sub
    for r as integer = 0 to pList->rowCount - 1
        ' Spanned rows keep their text in column 0 regardless of column mutations -- their
        ' caption must not be pushed into cells(0) (insert-at-0) or clobbered (delete-at-0),
        ' which would silently blank the row. Its higher cells are unused while spanned.
        if pList->rows(r).bSpanColumns then continue for
        dim as integer nCells = ubound(pList->rows(r).cells) + 1   ' stored cells = columns 1..nCells
        if nDir > 0 then
            if idx = 0 then
                ' New first column: old column 0 (Text) becomes column 1. A row storing
                ' nothing at all needs no gap -- every cell reads "" either way.
                dim as boolean hasData = (nCells > 0) orelse (len(pList->rows(r).Text) > 0)
                if hasData then
                    pList->rows(r).EnsureCells( nCells + 1 )
                    for c as integer = nCells to 1 step -1
                        pList->rows(r).cells(c) = pList->rows(r).cells(c - 1)
                    next
                    pList->rows(r).cells(0) = pList->rows(r).Text
                    pList->rows(r).Text = ""
                end if
            elseif nCells >= idx then
                ' Open a gap at cells index idx-1.
                pList->rows(r).EnsureCells( nCells + 1 )
                for c as integer = nCells to idx step -1
                    pList->rows(r).cells(c) = pList->rows(r).cells(c - 1)
                next
                pList->rows(r).cells(idx - 1) = ""
            end if
        else
            if idx = 0 then
                ' Column 0 deleted: column 1 (cells(0)) becomes the new Text.
                if nCells > 0 then
                    pList->rows(r).Text = pList->rows(r).cells(0)
                    for c as integer = 0 to nCells - 2
                        pList->rows(r).cells(c) = pList->rows(r).cells(c + 1)
                    next
                    pList->rows(r).TrimCells( nCells - 1 )
                else
                    pList->rows(r).Text = ""
                end if
            elseif nCells >= idx then
                ' Close the gap at cells index idx-1.
                for c as integer = idx - 1 to nCells - 2
                    pList->rows(r).cells(c) = pList->rows(r).cells(c + 1)
                next
                pList->rows(r).TrimCells( nCells - 1 )
            end if
        end if
    next
end sub

' Repaint the child listbox after a column-geometry change (no background erase: the row
' paint callback fills every pixel). No-op inside a BeginUpdate batch.
private sub PsListBox_InvalidateRowsForColumns( byval pList as PSLISTBOX ptr )
    if pList = null then exit sub
    if pList->updateDepth > 0 then exit sub
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
end sub

function PsListBox_AddColumn( byval hListControl as HWND, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as integer idx = PsColumnHeader_AddColumn( pList->hHeader, Text, nWidth, nMinWidth, itemData )
    if idx <> -1 then PsListBox_InvalidateRowsForColumns( pList )
    return idx
end function

function PsListBox_InsertColumn( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING, byval nWidth as integer = 100, byval nMinWidth as integer = 0, byval itemData as integer = 0 ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as integer at = PsColumnHeader_InsertColumn( pList->hHeader, idx, Text, nWidth, nMinWidth, itemData )
    if at <> -1 then
        ' Keep every row's cell storage aligned with the shifted column indices.
        PsListBox_ShiftCellColumns( pList, at, 1 )
        PsListBox_InvalidateRowsForColumns( pList )
    end if
    return at
end function

function PsListBox_DeleteColumn( byval hListControl as HWND, byval idx as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if PsColumnHeader_DeleteColumn( pList->hHeader, idx ) = false then return false
    PsListBox_ShiftCellColumns( pList, idx, -1 )
    PsListBox_InvalidateRowsForColumns( pList )
    return true
end function

sub PsListBox_ClearColumns( byval hListControl as HWND )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    PsColumnHeader_Clear( pList->hHeader )
    ' Cell TEXT is row data, not column geometry: it survives a column-definition
    ' clear the same way it predates a late column definition (sparse storage).
    PsListBox_InvalidateRowsForColumns( pList )
end sub

function PsListBox_GetColumnCount( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return PsColumnHeader_GetCount( pList->hHeader )
end function

function PsListBox_GetColumnText( byval hListControl as HWND, byval idx as integer ) as DWSTRING
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return ""
    return PsColumnHeader_GetColumnText( pList->hHeader, idx )
end function

function PsListBox_SetColumnText( byval hListControl as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    ' Header caption only; row cells are untouched, so no listbox repaint needed.
    return PsColumnHeader_SetColumnText( pList->hHeader, idx, Text )
end function

function PsListBox_GetColumnWidth( byval hListControl as HWND, byval idx as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return PsColumnHeader_GetColumnWidth( pList->hHeader, idx )
end function

function PsListBox_SetColumnWidth( byval hListControl as HWND, byval idx as integer, byval nWidth as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if PsColumnHeader_SetColumnWidth( pList->hHeader, idx, nWidth ) = false then return false
    PsListBox_InvalidateRowsForColumns( pList )
    return true
end function

function PsListBox_GetColumnMinWidth( byval hListControl as HWND, byval idx as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return PsColumnHeader_GetColumnMinWidth( pList->hHeader, idx )
end function

function PsListBox_SetColumnMinWidth( byval hListControl as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if PsColumnHeader_SetColumnMinWidth( pList->hHeader, idx, nMinWidth ) = false then return false
    PsListBox_InvalidateRowsForColumns( pList )
    return true
end function

function PsListBox_GetFillColumn( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    return PsColumnHeader_GetFillColumn( pList->hHeader )
end function

function PsListBox_SetFillColumn( byval hListControl as HWND, byval idx as integer ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if PsColumnHeader_SetFillColumn( pList->hHeader, idx ) = false then return false
    PsListBox_InvalidateRowsForColumns( pList )
    return true
end function

' Show or hide the header band. Changes the listbox's height (items-per-page), so it
' re-lays out AND re-syncs the scrollbar -- the same pairing as SetRowHeight.
function PsListBox_ShowHeader( byval hListControl as HWND, byval bShow as boolean = true ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->hHeader = 0 then return false
    if bShow <> pList->headerShown then
        pList->headerShown = bShow
        ShowWindow( pList->hHeader, iif( bShow, SW_SHOW, SW_HIDE ) )
        PsListBox_PositionWindows( pList->hWin )
        PsListBox_SyncScrollBar( pList )
    end if
    return true
end function

function PsListBox_IsHeaderVisible( byval hListControl as HWND ) as boolean
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->headerShown
end function

function PsListBox_GetHeaderHeight( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->HeaderHeight
end function

' Height in unscaled units (like RowHeight); DPI-scaled when the control is laid out.
function PsListBox_SetHeaderHeight( byval hListControl as HWND, byval height as integer ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    if height < 1 then height = 1
    pList->HeaderHeight = height
    PsListBox_PositionWindows( pList->hWin )
    PsListBox_SyncScrollBar( pList )
    return height
end function

' Escape hatch for direct PsColumnHeader_* calls (theming, padding, hit-tests). Do NOT
' set the WidthChanged callback on it -- that slot is this control's chain hook; use
' PsListBox_SetColumnResizeCallback.
function PsListBox_GetHeader( byval hListControl as HWND ) as HWND
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->hHeader
end function

sub PsListBox_SetColumnResizeCallback( byval hListControl as HWND, byval usersub as HDR_WidthChangedCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->ColumnResizeCallback = usersub
end sub

sub PsListBox_SetColumnClickCallback( byval hListControl as HWND, byval usersub as HDR_ClickCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetClickCallback( pList->hHeader, usersub )
end sub

sub PsListBox_SetColumnAutoSizeCallback( byval hListControl as HWND, byval userfunc as HDR_AutoSizeCallbackFunc )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetAutoSizeCallback( pList->hHeader, userfunc )
end sub

sub PsListBox_SetHeaderPaintCallback( byval hListControl as HWND, byval usersub as HDR_PaintCallbackSub )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetPaintCallback( pList->hHeader, usersub )
end sub

sub PsListBox_SetHeaderTooltipCallback( byval hListControl as HWND, byval userfunc as HDR_TooltipCallbackFunc )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetTooltipCallback( pList->hHeader, userfunc )
end sub

sub PsListBox_SetHeaderBackColor( byval hListControl as HWND, byval clr as COLORREF )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetBackColor( pList->hHeader, clr )
end sub

sub PsListBox_SetHeaderFont( byval hListControl as HWND, byval hFont as HFONT )
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hHeader then PsColumnHeader_SetFont( pList->hHeader, hFont )
end sub


' ========================================================================================
' The embedded header reports a USER column resize: every visible row's cells moved, so
' repaint the listbox rows, then re-broadcast to the host's ColumnResizeCallback. This
' chain hook OWNS the header's WidthChanged slot on an embedded header (same rule as the
' scrollbar's scroll callback) -- hosts subscribe via PsListBox_SetColumnResizeCallback.
' ========================================================================================
sub PsListBox_OnHeaderWidthChanged( byval hHeader as HWND, byval idx as integer, byval nWidth as integer, byval bLive as boolean )
    ' the header is a child of the PsListBox container window
    dim as PSLISTBOX ptr pList = GetListBoxPointer( GetParent(hHeader) )
    if pList = 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    if pList->ColumnResizeCallback then pList->ColumnResizeCallback( hHeader, idx, nWidth, bLive )
end sub


' ========================================================================================
' Retrieve the background color from the custom listbox control.
' ========================================================================================
function PsListBox_GetBackColor( byval hListControl as HWND ) as COLORREF
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->BackColor
end function


' ========================================================================================
' Set the background color for the custom listbox control.
' ========================================================================================
function PsListBox_SetBackColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF 
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    dim as COLORREF oldClr = pList->BackColor
    pList->BackColor = clr
    return oldClr
end function


' ========================================================================================
' Retrieve the current row count in the custom listbox control.
' ========================================================================================
function PsListBox_GetCount( byval hListControl as HWND ) as integer
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetCount()
end function


' ========================================================================================
' Refresh the custom listbox control.
' ========================================================================================
sub PsListBox_Refresh( byval hListControl as HWND ) 
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->Refresh()
end sub



' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function PsListBox_PositionWindows( byval hwnd as HWND ) as LRESULT
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList = null then return 0

    ' Get the entire client area
    dim as Rect rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )

    ' Reserve a strip on the right edge for the scrollbar, but only while it is shown
    ' (auto-hide gives the width back to the listbox).
    dim as integer sbW = 0
    if (pList->hScrollBar <> 0) andalso pList->scrollBarShown then
        sbW = pList->ScrollBarWidth
        if pWindow then sbW = pWindow->ScaleX( pList->ScrollBarWidth )
    end if

    ' Reserve a strip along the top for the column header band while it is shown. The
    ' header spans the FULL container width -- over the scrollbar strip, listview-style
    ' -- so column geometry never shifts when the scrollbar auto-hides/shows. It is
    ' SIZED even while hidden: columns can be defined and painted with the band hidden,
    ' and the header's LayoutColumns derives its rects from ITS window's client width
    ' (a 0x0 window would leave them stale). Header (x=0) and listbox (x=0) share the
    ' container's x origin and neither has a client-shifting border style, so header
    ' column rects are directly valid as row-cell x coordinates.
    dim as integer hdrH = 0
    if pList->hHeader <> 0 then
        dim as integer hdrWinH = pList->HeaderHeight
        if pWindow then hdrWinH = pWindow->ScaleY( pList->HeaderHeight )
        SetWindowPos( pList->hHeader, 0, _
                        rc.left, rc.top, rc.right - rc.left, hdrWinH, _
                        SWP_NOZORDER )
        if pList->headerShown then hdrH = hdrWinH
    end if

    dim as HWND hListbox = GetDlgItem( hwnd, pList->idc_ListBox )
    SetWindowPos( hListbox, 0, _
                    rc.left, rc.top + hdrH, (rc.right - rc.left) - sbW, (rc.bottom - rc.top) - hdrH, _
                    SWP_NOZORDER )

    if (pList->hScrollBar <> 0) andalso (sbW > 0) then
        SetWindowPos( pList->hScrollBar, 0, _
                        rc.right - sbW, rc.top + hdrH, sbW, (rc.bottom - rc.top) - hdrH, _
                        SWP_NOZORDER )
    end if

    function = 0
end function


' ========================================================================================
' The scrollbar reports that the user scrolled: move the listbox to that top row.
' ========================================================================================
sub PsListBox_OnScrollBarScroll( byval hSB as HWND, byval newPos as integer )
    ' the scrollbar is a child of the PsListBox container window
    dim as PSLISTBOX ptr pList = GetListBoxPointer( GetParent(hSB) )
    if pList = 0 then return
    ' The control clamps for itself now, where the listbox used to; mirror whatever it
    ' actually did so the thumb can never show a position the list isn't at. SetPos does
    ' not re-enter the callback.
    dim as integer actual = PsListBox_SetTopVis( pList, newPos )
    if actual <> newPos then PsVScrollBar_SetPos( hSB, actual )
    ' Keep the model's scroll source-of-truth in step with the user's scroll, so a
    ' later rebuild or show re-derives to where the user actually left the list.
    dim as integer modelTop = pList->VisibleToModel( actual )
    if modelTop >= 0 then pList->topRow = modelTop
end sub


' ========================================================================================
' Push the current model + scroll position into the scrollbar and apply the auto-hide
' decision. Called from every path that can change the row count or the top row, which
' is what stops the thumb ever going stale.
' ========================================================================================
sub PsListBox_SyncScrollBar( byval pList as PSLISTBOX ptr )
    if pList = 0 then return
    if pList->hScrollBar = 0 then return

    PsVScrollBar_SetRange( pList->hScrollBar, _
                          pList->visibleCount, _
                          PsListBox_ItemsPerPage( pList ), _
                          pList->nTopVis )

    ' Auto-hide. This only changes the listbox WIDTH, while items-per-page depends on
    ' HEIGHT, so the decision cannot feed back on itself and oscillate.
    dim as boolean needed = PsVScrollBar_NeedsThumb( pList->hScrollBar )
    if needed <> pList->scrollBarShown then
        pList->scrollBarShown = needed
        ShowWindow( pList->hScrollBar, iif( needed, SW_SHOW, SW_HIDE ) )
        PsListBox_PositionWindows( pList->hWin )   ' listbox reclaims / gives up the strip
    end if
end sub


' ========================================================================================
' Pull the Win32 listbox's ACTUAL top row back into the model (topRow). Only valid
' while visibleMap still matches the listbox contents -- i.e. BEFORE a rebuild. Called
' at the top of Refresh (so user scrolls survive rebuilds) and when the control is
' shown (so scrolls made through paths that bypass the model are absorbed).
' ========================================================================================
sub PsListBox_CaptureTopRow( byval pList as PSLISTBOX ptr )
    if pList = 0 then return
    if pList->visibleCount = 0 then return
    dim as integer modelRow = pList->VisibleToModel( pList->nTopVis )
    if modelRow >= 0 then pList->topRow = modelRow
end sub


' ========================================================================================
' Re-derive the VIEW (top row) and the scrollbar from the MODEL. After any model change --
' and whenever the control is shown -- this one routine re-converges them.
'
' Formerly PsListBox_SyncListboxFromModel, and it lost most of its body with the listbox
' child. LB_SETCOUNT is gone because visibleCount was always the real count; the caret push
' is gone because the caret was only ever a listbox artefact (selection is painted from the
' model's own per-row flags, which is why LB_SETCURSEL was already restricted to
' single-select styles). What remains is the part that was always this control's own work:
' translate topRow (model) into a visible slot, clamp it, and tell the scrollbar.
' ========================================================================================
sub PsListBox_SyncViewFromModel( byval pList as PSLISTBOX ptr )
    if pList = 0 then return
    dim as HWND hSurf = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hSurf = 0 then return

    if pList->visibleCount = 0 then
        pList->topRow  = 0
        pList->nTopVis = 0
        InvalidateRect( hSurf, NULL, TRUE )
        PsListBox_SyncScrollBar( pList )
        return
    end if

    ' Top row: largest visible slot whose model row is <= topRow (visibleMap is
    ' strictly ascending, so binary search). A topRow hidden inside a collapsed
    ' group lands on its group header.
    dim as integer row = pList->topRow
    if row < 0 then row = 0
    if row > pList->rowCount - 1 then row = pList->rowCount - 1
    dim as integer lo = 0
    dim as integer hi = pList->visibleCount - 1
    dim as integer visRow = 0
    do while lo <= hi
        dim as integer midRow = (lo + hi) \ 2
        if pList->visibleMap(midRow) <= row then
            visRow = midRow
            lo = midRow + 1
        else
            hi = midRow - 1
        end if
    loop

    ' Clamped by the control now, where the listbox used to do it. Mirror the clamped
    ' result back into topRow so the model always reflects what is actually on screen.
    dim as integer actualVis = PsListBox_SetTopVis( pList, visRow )
    dim as integer actualTop = pList->VisibleToModel( actualVis )
    if actualTop >= 0 then pList->topRow = actualTop

    InvalidateRect( hSurf, NULL, FALSE )
    PsListBox_SyncScrollBar( pList )
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: PsListBox
' ========================================================================================
function PsListBox_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        PsListBox_PositionWindows( hwnd )
        ' Height changed -> items-per-page changed -> scrollbar range changed
        PsListBox_SyncScrollBar( GetListBoxPointer(hwnd) )
    end if
    function = 0
end function
 
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: PsListBox
' ========================================================================================
function PsListBox_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList = null then return 0

    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pList->BackColor )
    b.PaintClientRect()
    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Paint the list surface: background, then every visible row, in ONE buffer.
'
' This replaces WM_MEASUREITEM + WM_DRAWITEM and is the point of the whole rewrite. The
' system listbox turned one WM_PAINT into N WM_DRAWITEM messages, each with its own buffer
' and its own blit, so a 34-row repaint paid 34 buffer create/blit cycles. Here the whole
' client is one cycle.
'
' ONE CONTRACT CHANGE, and it is the only one hosts can see: p->rc and the cell rects are
' now in the SURFACE's CLIENT coordinates rather than in a row-sized buffer starting at
' y = 0. A callback that derives everything from p->rc -- which is what the family's row
' painters do, since they must already honour the row's left edge and width -- needs no
' change at all. One that hardcoded 0 for the top would.
'
' Keeping p->rc.top = 0 with a per-row coordinate transform was considered and rejected: a
' plain client-relative rect is simpler and needs no transform machinery.
' ========================================================================================
function PsListBox_OnPaintSurface( byval hSurf as HWND ) as LRESULT
    dim as PSLISTBOX ptr pList = GetListBoxPointer( GetParent( hSurf ) )
    if pList = null then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hSurf )

    ' The control's own background, over the whole client. This also covers the empty
    ' strip below the last row, which used to need a separate WM_ERASEBKGND fill through
    ' GetListBoxEmptyClientArea.
    b.SetBackColor( pList->BackColor )
    b.PaintClientRect()

    dim as RECT rcClient
    GetClientRect( hSurf, @rcClient )
    dim as integer ih = PsListBox_ItemHeightPx( pList )

    ' --- Column geometry, resolved ONCE for the repaint rather than once per row. The
    ' old per-row loop asked the header for every column's rect on every row; columns
    ' cannot move mid-paint, so that was pure repetition. Only the y values vary per row.
    dim as integer nCols = PsColumnHeader_GetCount( pList->hHeader )
    ' Column band bounds, resolved once for the repaint: a spanned row's single merged cell
    ' runs from column 0's own left to the last column's right (same left a normal column-0
    ' cell gets). The per-column loop below never rewrites .rc.left/.rc.right, so these stay
    ' valid for every row.
    dim as integer bandLeft = 0, bandRight = 0
    if nCols > 0 then
        pList->EnsurePaintCells( nCols )
        dim as RECT rcCol
        for c as integer = 0 to nCols - 1
            PsColumnHeader_GetColumnRect( pList->hHeader, c, rcCol )   ' forces layout if dirty
            with pList->paintCells(c)
                .iCol = c
                .rc.left  = rcCol.left
                .rc.right = rcCol.right
            end with
        next
        bandLeft  = pList->paintCells(0).rc.left
        bandRight = pList->paintCells(nCols - 1).rc.right
    end if

    if (ih > 0) andalso (pList->PaintCallback <> 0) then
        for vis as integer = pList->nTopVis to pList->visibleCount - 1
            dim as integer y = (vis - pList->nTopVis) * ih
            if y >= rcClient.bottom then exit for

            dim as RECT rc
            SetRect( @rc, rcClient.left, y, rcClient.right, y + ih )

            ' Hot = the single row the mouse is currently over. nLastHotIdx is a VISIBLE
            ' index and is the ONE source of truth, so exactly one row is ever hot and it
            ' clears deterministically on WM_MOUSELEAVE. Do NOT hit-test the cursor here
            ' (paint-time race).
            dim as boolean isHot = (vis = pList->nLastHotIdx)

            dim as integer modelRow = pList->VisibleToModel( vis )
            dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )

            dim as PSLISTBOX_PAINTINFO p
            with p
                .itemID      = modelRow
                .b           = @b
                .rc          = rc
                .isHot       = isHot
                .isSelected  = false
                .isFocused   = (modelRow = pList->focusRow)
                .isHeader    = false
                .isCollapsed = false
                .isSpanned   = false
                .isSelectable = true
                .columnCount = 0
                .cells       = 0
                if pRow then
                    .isSelected  = pRow->selected
                    .isHeader    = pRow->IsHeader
                    .isCollapsed = pRow->bCollapsed
                    .isSpanned   = pRow->bSpanColumns
                    .isSelectable = pRow->bSelectable
                    .wszCaption  = pRow->Text
                end if
            end with

            ' --- Columns: hand the host per-column cell rects + text. columnCount = 0
            ' (no columns defined, or a group-header row -- those span) leaves the
            ' contract exactly as it always was. Cell x came from the header above; only
            ' y is per-row. The scratch array persists on the instance, so the paint loop
            ' does no heap churn; the pointer is valid only during the callback.
            dim as boolean wantCells = (pRow <> 0)
            if wantCells then wantCells = (pRow->IsHeader = false)
            if nCols <= 0 then wantCells = false
            if wantCells then
                if pRow->bSpanColumns then
                    ' Spanned row: one merged cell over the whole band. Uses its own scratch
                    ' (spanCell) so the shared paintCells x-geometry is left intact for the
                    ' normal rows that follow in this same repaint.
                    with pList->spanCell
                        .iCol      = 0
                        .rc.left   = bandLeft
                        .rc.right  = bandRight
                        .rc.top    = y
                        .rc.bottom = y + ih
                        .wszText   = pRow->Text
                    end with
                    p.columnCount = 1
                    p.cells = @pList->spanCell
                else
                    for c as integer = 0 to nCols - 1
                        with pList->paintCells(c)
                            .rc.top    = y
                            .rc.bottom = y + ih
                            if c = 0 then
                                .wszText = pRow->Text
                            elseif c - 1 <= ubound(pRow->cells) then
                                .wszText = pRow->cells(c - 1)
                            else
                                .wszText = ""
                            end if
                        end with
                    next
                    p.columnCount = nCols
                    p.cells = @pList->paintCells(0)
                end if
            end if

            b.SetFont( pList->hFont )   ' default row font; the callback may override
            pList->PaintCallback( @p )
        next
    end if

    ' --- Drag-reorder drop indicator, drawn ON TOP of the rows. Either a frame around the
    ' target header (drop-as-first-child) or a horizontal insertion line at the target gap. ---
    if (pList->bDragActive = true) andalso (ih > 0) then
        b.SetBackColor( pList->dragIndicatorColor )
        if (pList->dropOnHeader = true) andalso (pList->dropTargetRow >= 0) then
            dim as RECT rh
            if PsListBox_VisIndexRect( pList, pList->ModelToVisible( pList->dropTargetRow ), rh ) then
                dim as RECT seg
                seg = rh : seg.bottom = seg.top + 2               : b.PaintRect( @seg )   ' top edge
                seg = rh : seg.top    = seg.bottom - 2            : b.PaintRect( @seg )   ' bottom edge
                seg = rh : seg.right  = seg.left + 2              : b.PaintRect( @seg )   ' left edge
                seg = rh : seg.left   = seg.right - 2             : b.PaintRect( @seg )   ' right edge
            end if
        else
            ' The gap's y: top of the row dropInsertBefore maps to, or just past the last
            ' visible row when the drop is at the end of the list.
            dim as integer lineY
            if pList->dropInsertBefore >= pList->rowCount then
                lineY = (pList->visibleCount - pList->nTopVis) * ih
                if lineY > rcClient.bottom - 3 then lineY = rcClient.bottom - 3
            else
                dim as integer gv = pList->ModelToVisible( pList->dropInsertBefore )
                if gv < 0 then gv = pList->visibleCount
                lineY = (gv - pList->nTopVis) * ih
            end if
            if lineY < 0 then lineY = 0
            dim as RECT rl
            SetRect( @rl, rcClient.left, lineY, rcClient.right, lineY + 3 )
            b.PaintRect( @rl )
        end if
    end if

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' PsListBox Window procedure
' ========================================================================================
function PsListBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        PsListBox_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       PsListBox_OnPaint)
        ' WM_MEASUREITEM and WM_DRAWITEM are gone with the listbox child. The container
        ' never painted rows itself -- it forwarded them -- and the surface now paints
        ' every visible row in one pass in PsListBox_OnPaintSurface.


    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot row's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if pnmh andalso (pnmh->code = TTN_GETDISPINFOW) then
            dim as PSLISTBOX ptr pList = GetListBoxPointer(hwnd)
            if pList then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                dim as integer modelRow = pList->VisibleToModel( pList->nLastHotIdx )
                ' store in an instance-lifetime buffer and point lpszText at it
                pList->wszTooltip = PsListBox_ResolveTooltipText( pList, modelRow )
                pdi->lpszText = cast( LPWSTR, pList->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_SHOWWINDOW
        ' Becoming visible: re-converge the derived Win32 listbox state from the
        ' model. Anything pushed while the control was hidden never reached the
        ' screen, and a scroll made through a path that bypasses the model (wheel,
        ' keyboard) lives only in the listbox -- capture it first so the sync
        ' converges on it instead of snapping back to a stale topRow.
        if wParam then
            dim as PSLISTBOX ptr pList = GetListBoxPointer(hwnd)
            PsListBox_CaptureTopRow( pList )
            PsListBox_SyncViewFromModel( pList )
        end if

    ' WM_CTLCOLORLISTBOX is gone with the listbox child. Only a system listbox sends it,
    ' and it existed here to stop an EMPTY list rendering as a white rectangle -- the
    ' listbox filled its own client with DefWindowProc's white brush during its WM_PAINT,
    ' painting over the themed erase. The surface fills its whole client with BackColor
    ' now, empty or not, so the problem cannot arise. pList->hBackBrush went with it.

    case WM_NCDESTROY
        ' Delete the stored listbox data pointer
        dim as PSLISTBOX ptr pList = GetListBoxPointer(hwnd)
        if pList then
            if IsWindow(pList->hToolTip) then DestroyWindow(pList->hToolTip)
            delete(pList)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    case WM_ERASEBKGND
        return true

    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function

' ========================================================================================
' Tell the host the USER moved the selection. The single choke point for the notification:
' every user-driven path (click, arrow keys, Left-to-header, Space) sets focusRow and then
' calls this.
'
' Keyed on focusRow against lastNotifiedRow, so re-selecting the row that is already
' current is silent, and so is every programmatic setter -- those update lastNotifiedRow
' without calling the callback.
' ========================================================================================
sub PsListBox_NotifySelChange( byval pList as PSLISTBOX ptr )
    if pList = null then return
    if pList->focusRow = pList->lastNotifiedRow then return
    pList->lastNotifiedRow = pList->focusRow
    if pList->SelChangeCallback then
        pList->SelChangeCallback( pList->hWin, pList->focusRow )
    end if
end sub


' ========================================================================================
' Apply a mouse-click selection to the model, honoring the selection mode and the
' Ctrl/Shift modifiers, then repaint. modelRow must be a valid model row.
' ========================================================================================
sub PsListBox_ApplyClickSelection( byval pList as PSLISTBOX ptr, byval modelRow as integer, byval isCtrl as boolean, byval isShift as boolean )
    if (pList = null) orelse (pList->IsValidRow(modelRow) = false) then return
    ' A non-selectable row swallows the click for selection purposes: no focus move, no
    ' selection change, no anchor update. The host still sees the raw click through the
    ' message callback (surfaced by the WndProc, not here).
    if pList->IsRowSelectable(modelRow) = false then return

    if pList->MultipleSel then
        ' checklist style: every click toggles that one row
        pList->SetRowSelected( modelRow, not pList->IsRowSelected(modelRow) )
        pList->anchorRow = modelRow
    elseif pList->ExtendSel then
        if isShift then
            dim as integer anchor = pList->anchorRow
            if anchor < 0 then anchor = modelRow
            if isCtrl = false then pList->ClearSelection()
            pList->SelectRange( anchor, modelRow )
        elseif isCtrl then
            pList->SetRowSelected( modelRow, not pList->IsRowSelected(modelRow) )
            pList->anchorRow = modelRow
        else
            pList->SelectOnly( modelRow )
            pList->anchorRow = modelRow
        end if
    else
        ' single-select
        pList->SelectOnly( modelRow )
        pList->anchorRow = modelRow
    end if

    pList->focusRow = modelRow
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    PsListBox_NotifySelChange( pList )
end sub


' ========================================================================================
' Clear any hot-tracked row and stop the safety-net timer. Repaints only if a row was
' actually hot, so this is cheap to call repeatedly.
' ========================================================================================
sub PsListBox_ClearHotTracking( byval pList as PSLISTBOX ptr, byval hList as HWND )
    if pList = null then return
    if pList->hotTimerOn then
        KillTimer( hList, IDT_CLISTBOX_HOTTRACK )
        pList->hotTimerOn = false
    end if
    if pList->nLastHotIdx <> -1 then
        pList->nLastHotIdx = -1
        AfxRedrawWindow( hList )
    end if
end sub


' ========================================================================================
' Keyboard-navigation helpers (all operate in MODEL space; movement is over the
' currently VISIBLE rows).
' ========================================================================================
function PsListBox_ItemsPerPage( byval pList as PSLISTBOX ptr ) as integer
    dim as integer n = PsListBox_VisPerPage( pList )
    if n < 1 then n = 1
    return n
end function

sub PsListBox_EnsureVisible( byval pList as PSLISTBOX ptr, byval modelRow as integer )
    dim as integer vis = pList->ModelToVisible( modelRow )
    if vis < 0 then return
    dim as integer top = pList->nTopVis
    dim as integer perPage = PsListBox_ItemsPerPage( pList )
    if vis < top then
        PsListBox_SetTopVis( pList, vis )
        PsListBox_SyncScrollBar( pList )
    elseif vis >= top + perPage then
        PsListBox_SetTopVis( pList, vis - perPage + 1 )
        PsListBox_SyncScrollBar( pList )
    end if
end sub

' Nearest header at or before modelRow (the group a row belongs to), or -1.
function PsListBox_ParentHeader( byval pList as PSLISTBOX ptr, byval modelRow as integer ) as integer
    for i as integer = modelRow to 0 step -1
        if pList->rows(i).IsHeader then return i
    next
    return -1
end function

' First VISIBLE index whose MODEL row is selectable, scanning from startVis INCLUSIVE in
' step dir (+1 down / -1 up). -1 if the scan runs off the end without finding one.
function PsListBox_FindSelectableVis( byval pList as PSLISTBOX ptr, byval startVis as integer, byval nDir as integer ) as integer
    dim as integer v = startVis
    while (v >= 0) andalso (v < pList->visibleCount)
        dim as integer mr = pList->VisibleToModel( v )
        if (mr >= 0) andalso pList->IsRowSelectable( mr ) then return v
        v += nDir
    wend
    return -1
end function

' Move the focus to a VISIBLE position (clamped) and apply selection per the control's mode
' and the Shift/Ctrl modifiers, then ensure-visible and repaint. Non-selectable rows are
' skipped: the caret lands on the first selectable row from the target travelling in `dir`.
' bAllowReverse lets the paging/extreme keys search back the other way when nothing lies
' ahead, so they always land somewhere; the single-step arrows pass FALSE, so a step that
' would only reach non-selectable rows simply leaves the caret where it is.
sub PsListBox_MoveFocusVis( byval pList as PSLISTBOX ptr, byval newVis as integer, byval isShift as boolean, byval isCtrl as boolean, byval nDir as integer = 1, byval bAllowReverse as boolean = false )
    if pList->visibleCount <= 0 then return
    if newVis < 0 then newVis = 0
    if newVis > pList->visibleCount - 1 then newVis = pList->visibleCount - 1

    dim as integer resolved = PsListBox_FindSelectableVis( pList, newVis, nDir )
    if (resolved < 0) andalso bAllowReverse then resolved = PsListBox_FindSelectableVis( pList, newVis, -nDir )
    if resolved < 0 then return          ' no selectable row reachable -> leave focus put
    newVis = resolved

    dim as integer newModel = pList->VisibleToModel( newVis )
    if newModel < 0 then return

    pList->focusRow = newModel
    if pList->MultipleSel then
        ' checklist mode: arrows move focus only; Space toggles selection
    elseif pList->ExtendSel then
        if isShift then
            dim as integer anchor = pList->anchorRow
            if anchor < 0 then anchor = newModel
            pList->ClearSelection()
            pList->SelectRange( anchor, newModel )
        elseif isCtrl then
            ' Ctrl: move focus only
        else
            pList->SelectOnly( newModel )
            pList->anchorRow = newModel
        end if
    else
        pList->SelectOnly( newModel )
        pList->anchorRow = newModel
    end if

    PsListBox_EnsureVisible( pList, newModel )
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    ' Keyboard navigation is the ONLY way a host can see this: WM_KEYDOWN is consumed
    ' here and never reaches the message callback.
    PsListBox_NotifySelChange( pList )
end sub

' Left arrow: collapse an expanded header, or (from an item) jump to its header.
sub PsListBox_KeyLeft( byval pList as PSLISTBOX ptr )
    dim as integer f = pList->focusRow
    if pList->IsValidRow(f) = false then return
    if pList->rows(f).IsHeader then
        if pList->rows(f).bCollapsed = false then
            pList->rows(f).bCollapsed = true
            pList->NotifyChange()
            PsListBox_EnsureVisible( pList, f )
        end if
    else
        dim as integer h = PsListBox_ParentHeader( pList, f )
        if (h >= 0) andalso pList->IsRowSelectable( h ) then
            pList->focusRow  = h
            pList->SelectOnly( h )
            pList->anchorRow = h
            PsListBox_EnsureVisible( pList, h )
            dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
            if hList then InvalidateRect( hList, NULL, FALSE )
            PsListBox_NotifySelChange( pList )
        end if
    end if
end sub

' Right arrow: expand a collapsed header, or (from an expanded header) step in.
sub PsListBox_KeyRight( byval pList as PSLISTBOX ptr )
    dim as integer f = pList->focusRow
    if pList->IsValidRow(f) = false then return
    if pList->rows(f).IsHeader = false then return
    if pList->rows(f).bCollapsed then
        pList->rows(f).bCollapsed = false
        pList->NotifyChange()
        PsListBox_EnsureVisible( pList, f )
    else
        dim as integer curVis = pList->ModelToVisible( f )
        PsListBox_MoveFocusVis( pList, curVis + 1, false, false, +1, false )
    end if
end sub

' Space: toggle the focus row's selection (multi/extended) or select it (single).
sub PsListBox_KeySpace( byval pList as PSLISTBOX ptr, byval isCtrl as boolean )
    dim as integer f = pList->focusRow
    if pList->IsValidRow(f) = false then return
    if pList->MultipleSel orelse pList->ExtendSel then
        pList->SetRowSelected( f, not pList->IsRowSelected(f) )
    else
        pList->SelectOnly( f )
    end if
    pList->anchorRow = f
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    PsListBox_NotifySelChange( pList )
end sub


' ========================================================================================
' Drag-and-drop reordering helpers (used by the surface WndProc below).
' ========================================================================================

' The rows a drag will move: if the pressed row is part of a multi-selection, every selected
' item row; otherwise just the pressed row (headers/invalid excluded). Redims srcRows() and
' returns the count.
private function PsListBox_BuildDragSource( byval pList as PSLISTBOX ptr, byval pressedRow as integer, srcRows() as integer ) as integer
    dim as integer n = 0
    dim as boolean pressedSelected = pList->IsRowSelected( pressedRow )
    if pressedSelected andalso (pList->GetSelCount() > 1) then
        redim srcRows( 0 to pList->rowCount - 1 )
        for i as integer = 0 to pList->rowCount - 1
            if pList->rows(i).selected andalso (pList->rows(i).IsHeader = false) then
                srcRows(n) = i : n += 1
            end if
        next
        if n > 0 then redim preserve srcRows( 0 to n - 1 )
    end if
    if n = 0 then
        if pList->IsValidRow(pressedRow) andalso (pList->rows(pressedRow).IsHeader = false) then
            redim srcRows( 0 to 0 )
            srcRows(0) = pressedRow
            n = 1
        end if
    end if
    return n
end function

' Recompute the current drop target from a client point and repaint the indicator if it moved.
private sub PsListBox_UpdateDragTarget( byval pList as PSLISTBOX ptr, byval hWin as HWND, byval x as integer, byval y as integer )
    dim as integer ib, tr
    dim as boolean oh
    PsListBox_DropTargetFromPoint( pList, x, y, ib, oh, tr )
    if (ib <> pList->dropInsertBefore) orelse (oh <> pList->dropOnHeader) orelse (tr <> pList->dropTargetRow) then
        pList->dropInsertBefore = ib
        pList->dropOnHeader     = oh
        pList->dropTargetRow    = tr
        InvalidateRect( hWin, NULL, FALSE )
    end if
end sub

' Tear a drag down: clear state, kill the auto-scroll timer, release capture, and clear the
' indicator if one was showing. Safe to call whether armed, active, or neither.
private sub PsListBox_CancelDrag( byval pList as PSLISTBOX ptr, byval hWin as HWND )
    dim as boolean wasActive = pList->bDragActive
    pList->bDragArmed       = false
    pList->bDragActive      = false
    pList->dragPendCollapse = -1
    pList->dragAnchorRow    = -1
    pList->dropInsertBefore = -1
    pList->dropOnHeader     = false
    pList->dropTargetRow    = -1
    if pList->dragTimerOn then
        KillTimer( hWin, IDT_CLISTBOX_DRAGSCROLL )
        pList->dragTimerOn = false
    end if
    if GetCapture() = hWin then ReleaseCapture()
    if wasActive then InvalidateRect( hWin, NULL, FALSE )
end sub

' Commit the current drop: build the source set, run the CanDrop veto, move the rows, and
' fire DragDrop. Reads the target tracked in drop* (recompute from the up point first).
private sub PsListBox_CommitDrop( byval pList as PSLISTBOX ptr, byval hWin as HWND )
    dim srcRows() as integer
    dim as integer n = PsListBox_BuildDragSource( pList, pList->dragAnchorRow, srcRows() )
    if n <= 0 then return
    dim as integer insertBefore = pList->dropInsertBefore
    if insertBefore < 0 then return

    ' Pre-drop veto. The host gets the source set and the drop-target ROWINFO (read-only).
    if pList->CanDropCallback then
        dim info as PSLISTBOX_DROPINFO
        info.hList        = pList->hWin
        info.srcRows      = @srcRows(0)
        info.srcCount     = n
        info.insertBefore = insertBefore
        info.onHeader     = pList->dropOnHeader
        info.targetRow    = pList->dropTargetRow
        if pList->IsValidRow( pList->dropTargetRow ) then info.targetInfo = pList->GetRow( pList->dropTargetRow )
        if pList->CanDropCallback( @info ) = false then return   ' rejected: leave the model alone
    end if

    ' Dropping onto a collapsed header expands it, so the moved rows are actually visible.
    if pList->dropOnHeader andalso pList->IsValidRow(pList->dropTargetRow) then
        if pList->rows(pList->dropTargetRow).bCollapsed then pList->rows(pList->dropTargetRow).bCollapsed = false
    end if

    dim as integer newFirst = pList->MoveRows( srcRows(), insertBefore )   ' NotifyChange->Refresh
    if (newFirst >= 0) andalso pList->DragDropCallback then pList->DragDropCallback( pList->hWin, newFirst, n )
end sub


' ========================================================================================
' PsListBoxSurface_WndProc -- the window procedure for the row surface.
'
' Was PsListBoxListBox_SubclassProc, a subclass hung off a system LISTBOX. The body is
' largely unchanged, because almost none of it ever depended on the listbox: selection,
' focus, keyboard navigation, hot tracking, group collapse and the wheel accumulator were
' already this control's own code. What changed is the frame around it -- every
' DefSubclassProc tail is now DefWindowProcW, the two things the listbox used to do for
' free (taking focus on a click, claiming the arrow keys) are done explicitly, and
' WM_PAINT is handled here rather than fanned out into WM_DRAWITEM.
' ========================================================================================
function PsListBoxSurface_WndProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM _               ' // Second message parameter
            ) as LRESULT

    dim as PSLISTBOX ptr pList = GetListBoxPointer( GetParent(hWin) )

    ' Per-message scratch info passed to the user MessageCallback. A plain local
    ' (NOT static) so that multiple control instances never share it. The hover
    ' tracking index likewise lives per-instance in pList->nLastHotIdx.
    dim m as PSLISTBOX_MESSAGEINFO

    select case uMsg
        ' Prepare the Callback MessageInfo structure for only the messages that we handle
        case WM_MOUSEWHEEL, WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK
            with m
                .hList   = hWin
                .uMsg    = uMsg
                .wParam  = _wParam
                .lParam  = _lParam
                .idx     = -1
                .isCtrl  = (GetAsyncKeyState(VK_CONTROL) and &H8000)
                .isShift = (GetAsyncKeyState(VK_SHIFT) and &H8000) 
            end with
    end select
   
    select case uMsg
        case WM_PAINT
            ' ONE repaint of the visible rows, in a single pass over the whole client.
            ' This used to hand off to DefSubclassProc, which fanned out into one
            ' WM_DRAWITEM per row -- and collapsing those N paint cycles into one is the
            ' whole of the speed win this rewrite bought. It is backend-independent: it
            ' made GDI 25% faster and GDI+ 38% faster.
            return PsListBox_OnPaintSurface( hWin )

        case WM_ERASEBKGND
            ' The paint fills the entire client, including the strip below the last row,
            ' so there is nothing to erase and erasing would only flicker.
            return true

        case WM_GETDLGCODE
            ' Claim the navigation keys. As a system listbox these arrived automatically;
            ' a plain child window has to ask, or IsDialogMessage in the host's pump eats
            ' the arrows for tab-navigation before WM_KEYDOWN is ever sent.
            return DLGC_WANTARROWS or DLGC_WANTCHARS

        case WM_SETFOCUS, WM_KILLFOCUS
            ' The focus row is painted with an accent bar, so the surface has to redraw
            ' when focus arrives or leaves. The listbox did this for itself.
            InvalidateRect( hWin, NULL, FALSE )
            return 0

        case WM_TIMER
            if pList andalso (_wParam = IDT_CLISTBOX_HOTTRACK) then
                ' The cursor may have left without us getting a WM_MOUSELEAVE. If it
                ' is no longer over our client area, clear the hot row ourselves.
                dim as RECT rcClient : GetClientRect( hWin, @rcClient )
                if isMouseOverRECT( hWin, rcClient ) = false then
                    PsListBox_ClearHotTracking( pList, hWin )
                end if
                return 0
            end if
            if pList andalso (_wParam = IDT_CLISTBOX_DRAGSCROLL) then
                ' Auto-scroll while dragging near an edge. Read the cursor live (capture
                ' means moves may stop arriving once it is held still against the edge).
                if pList->bDragActive then
                    dim as POINT pt : GetCursorPos( @pt ) : ScreenToClient( hWin, @pt )
                    dim as RECT rcC : GetClientRect( hWin, @rcC )
                    dim as integer newTop = pList->nTopVis
                    if pt.y < (rcC.top + PSLISTBOX_DRAGSCROLL_ZONE) then
                        newTop -= 1
                    elseif pt.y > (rcC.bottom - PSLISTBOX_DRAGSCROLL_ZONE) then
                        newTop += 1
                    end if
                    if newTop <> pList->nTopVis then
                        PsListBox_SetTopVis( pList, newTop )
                        PsListBox_CaptureTopRow( pList )
                        PsListBox_SyncScrollBar( pList )
                        PsListBox_UpdateDragTarget( pList, hWin, pt.x, pt.y )
                        InvalidateRect( hWin, NULL, FALSE )
                    end if
                end if
                return 0
            end if
            ' any other timer id falls through to the default listbox handler

        case WM_KEYDOWN
            ' Esc during an active drag cancels it with no move (capture-loss's sibling).
            if (pList <> 0) andalso (pList->bDragActive = true) andalso (_wParam = VK_ESCAPE) then
                PsListBox_CancelDrag( pList, hWin )
                return 0
            end if
            if pList then
                dim as boolean isShift = (GetAsyncKeyState(VK_SHIFT) and &H8000)
                dim as boolean isCtrl  = (GetAsyncKeyState(VK_CONTROL) and &H8000)
                dim as integer curVis  = pList->ModelToVisible( pList->focusRow )
                if curVis < 0 then curVis = 0        ' no/hidden focus -> start at top
                dim as integer perPage = PsListBox_ItemsPerPage( pList )
                if perPage < 1 then perPage = 1

                select case _wParam
                    case VK_UP        ' step up; a blocked step leaves the caret put
                        PsListBox_MoveFocusVis( pList, curVis - 1, isShift, isCtrl, -1, false ) : return 0
                    case VK_DOWN      ' step down; likewise
                        PsListBox_MoveFocusVis( pList, curVis + 1, isShift, isCtrl, +1, false ) : return 0
                    case VK_PRIOR    ' PageUp: search up from the target, else back down toward here
                        PsListBox_MoveFocusVis( pList, curVis - perPage, isShift, isCtrl, -1, true ) : return 0
                    case VK_NEXT     ' PageDown: search down from the target, else back up
                        PsListBox_MoveFocusVis( pList, curVis + perPage, isShift, isCtrl, +1, true ) : return 0
                    case VK_HOME     ' first selectable row from the top
                        PsListBox_MoveFocusVis( pList, 0, isShift, isCtrl, +1, true ) : return 0
                    case VK_END      ' last selectable row from the bottom
                        PsListBox_MoveFocusVis( pList, pList->visibleCount - 1, isShift, isCtrl, -1, true ) : return 0
                    case VK_LEFT
                        PsListBox_KeyLeft( pList ) : return 0
                    case VK_RIGHT
                        PsListBox_KeyRight( pList ) : return 0
                    case VK_SPACE
                        PsListBox_KeySpace( pList, isCtrl ) : return 0
                end select
            end if
            ' unhandled keys fall through to the default listbox handler

        Case WM_MOUSEWHEEL
            if pList then
                ' Accumulate the wheel delta and scroll one "notch" per WHEEL_DELTA
                ' (120), by the user's configured lines-per-notch. The remainder is
                ' kept so slow/precision wheels still scroll smoothly.
                dim as long linesPerNotch = 3
                SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, @linesPerNotch, 0 )
                dim as long page = PsListBox_ItemsPerPage( pList )
                if (linesPerNotch < 1) orelse (linesPerNotch > page) then linesPerNotch = page  ' WHEEL_PAGESCROLL / bogus
                if linesPerNotch < 1 then linesPerNotch = 1

                pList->accumDelta += GET_WHEEL_DELTA_WPARAM( _wParam )
                dim as long nTopIndex = pList->nTopVis
                dim as boolean moved = false
                while pList->accumDelta >= WHEEL_DELTA          ' wheel forward -> scroll up
                    nTopIndex -= linesPerNotch
                    pList->accumDelta -= WHEEL_DELTA
                    moved = true
                wend
                while pList->accumDelta <= -WHEEL_DELTA         ' wheel back -> scroll down
                    nTopIndex += linesPerNotch
                    pList->accumDelta += WHEEL_DELTA
                    moved = true
                wend
                if moved then
                    ' Clamped by PsListBox_SetTopVis, which is where the listbox's own
                    ' clamping went. It repaints only if the position actually changed,
                    ' so wheeling against the end of the list costs nothing.
                    PsListBox_SetTopVis( pList, nTopIndex )
                    ' The model's scroll source of truth follows the user's wheel, so a
                    ' later rebuild re-derives to where they left it.
                    PsListBox_CaptureTopRow( pList )
                    PsListBox_SyncScrollBar( pList )   ' keep the thumb with the wheel
                    if (pList->MessageCallback <> 0) andalso pList->MessageCallback( @m ) then return 0
                end if
            end if


        Case WM_MOUSEMOVE
            ' --- Drag-reorder gesture. While a drag is active, this owns the message: the
            ' indicator follows the cursor and hover/selection are suppressed. While merely
            ' armed, wait for the system drag threshold before taking capture. ---
            if (pList <> 0) andalso (pList->bDragReorder = true) then
                dim as integer mx = GET_X_LPARAM(_lParam)
                dim as integer my = GET_Y_LPARAM(_lParam)
                if pList->bDragActive then
                    PsListBox_UpdateDragTarget( pList, hWin, mx, my )
                    return 0
                elseif (pList->bDragArmed = true) andalso ((_wParam and MK_LBUTTON) <> 0) then
                    dim as integer dx = abs( mx - pList->ptDragStart.x )
                    dim as integer dy = abs( my - pList->ptDragStart.y )
                    if (dx >= GetSystemMetrics(SM_CXDRAG)) orelse (dy >= GetSystemMetrics(SM_CYDRAG)) then
                        pList->bDragActive = true
                        SetCapture( hWin )
                        PsListBox_ClearHotTracking( pList, hWin )          ' no hot row during a drag
                        SetTimer( hWin, IDT_CLISTBOX_DRAGSCROLL, PSLISTBOX_DRAGSCROLL_MS, NULL )
                        pList->dragTimerOn = true
                        PsListBox_UpdateDragTarget( pList, hWin, mx, my )
                        return 0
                    end if
                end if
            end if

            ' Track that we are over the control in order to catch the
            ' eventual WM_MOUSEHOVER and WM_MOUSELEAVE events
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 250    ' HOVER_DEFAULT is 500
            if pList then tme.dwHoverTime = pList->HoverTime
            TrackMouseEvent(@tme)

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse
            ' exits quickly (or onto another control), which would strand the hot row
            ' painted. Poll the cursor while we are over the control.
            if (pList <> 0) andalso (pList->hotTimerOn = false) then
                SetTimer( hWin, IDT_CLISTBOX_HOTTRACK, PSLISTBOX_HOTTRACK_MS, NULL )
                pList->hotTimerOn = true
            end if

            ' Invalidate only the rows whose hot state actually changed, sparing the buffer
            ' a full-client repaint per mouse move.
            dim as RECT rc
            dim as long idx = PsListBox_VisIndexFromPoint( pList, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
            if (pList <> 0) andalso (idx >= 0) then
                if idx <> pList->nLastHotIdx then
                    ' Repaint the newly-hot row and the previously-hot row. FALSE =
                    ' no background erase; the double buffer fully paints each row.
                    if PsListBox_VisIndexRect( pList, idx, rc ) then InvalidateRect( hWin, @rc, FALSE )
                    if pList->nLastHotIdx <> -1 then
                        if PsListBox_VisIndexRect( pList, pList->nLastHotIdx, rc ) then _
                            InvalidateRect( hWin, @rc, FALSE )
                    end if
                    pList->nLastHotIdx = idx
                    ' Row changed: pop the current tip so it re-queries
                    ' TTN_GETDISPINFOW for the new row on the next hover.
                    if pList->hTooltip then SendMessage( pList->hTooltip, TTM_POP, 0, 0 )
                end if
            end if
            if pList andalso pList->MessageCallback then
                if pList->MessageCallback( @m ) then return 0
            end if


        case WM_MOUSEHOVER
            dim as long idx = PsListBox_VisIndexFromPoint( pList, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
            if idx >= 0 then
                ' idx is a VISIBLE index; the callback sees MODEL indices.
                if pList then m.idx = pList->VisibleToModel( idx )
                if pList andalso pList->MessageCallback then
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if


        case WM_MOUSELEAVE
            if pList then
                ' Fast path for clearing the hot row (the WM_TIMER poll is the
                ' fallback for when this message never arrives). The tooltip hides
                ' itself when the mouse leaves the tool, so nothing to push.
                PsListBox_ClearHotTracking( pList, hWin )
                if (pList->MessageCallback <> 0) andalso pList->MessageCallback( @m ) then return 0
            end if
            

        case WM_LBUTTONDOWN
            ' A fresh press means any armed double-click up-skip lost its pairing
            ' partner (capture change, focus loss) -- disarm it so it cannot eat
            ' this press's own release.
            if pList then pList->skipNextLBtnUp = false
            if pList then
                pList->bDragArmed       = false     ' a fresh press re-arms from scratch
                pList->dragPendCollapse = -1
            end if
            ' Keyboard focus. The system listbox took it in its own default handling; a
            ' plain child window has to ask, or the arrow keys keep going to whatever had
            ' focus before the click.
            SetFocus( hWin )
            dim as long idx = PsListBox_VisIndexFromPoint( pList, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
            if (pList <> 0) andalso (idx >= 0) then
                dim as integer modelRow = pList->VisibleToModel( idx )
                m.idx = modelRow
                dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )
                if (pRow <> 0) andalso pRow->IsHeader then
                    ' Header: clicking toggles the group collapse and deliberately
                    ' leaves the selection untouched (a fold gesture, not a select).
                    pRow->bCollapsed = not pRow->bCollapsed
                    pList->NotifyChange()
                else
                    ' Item selection. If drag-reorder is on and this press lands on a row
                    ' that is ALREADY part of a multi-selection, DEFER the collapse-to-one:
                    ' the user may be starting to drag the whole selection, and collapsing it
                    ' now would drag a single row. A plain click (no drag) applies the
                    ' collapse on the up instead.
                    ' A draggable row is a selectable, non-header item while the feature is on
                    ' (hoisted to one boolean -- the 3-way andalso chain trips fbc warning 38).
                    dim as boolean canDrag = false
                    if pRow <> 0 then canDrag = (pList->bDragReorder = true) andalso (pRow->bSelectable = true)
                    dim as boolean deferSel = false
                    if canDrag then
                        if pList->IsRowSelected(modelRow) andalso (pList->GetSelCount() > 1) then deferSel = true
                    end if
                    if deferSel then
                        pList->dragPendCollapse = modelRow
                    else
                        PsListBox_ApplyClickSelection( pList, modelRow, m.isCtrl, m.isShift )
                    end if
                    ' Arm a potential drag from a draggable (selectable, non-header) item.
                    if canDrag then
                        pList->bDragArmed    = true
                        pList->bDragActive   = false
                        pList->dragAnchorRow = modelRow
                        pList->ptDragStart.x = GET_X_LPARAM(_lParam)
                        pList->ptDragStart.y = GET_Y_LPARAM(_lParam)
                    end if
                end if
                if pList->MessageCallback then
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if
            return 0


        case WM_LBUTTONUP
            ' --- Drag-reorder: an active drag ends here. Recompute the target from the up
            ' point, commit the move (with the CanDrop veto), then tear down capture/timer.
            ' The host's up callback is intentionally NOT fired for a completed drag. ---
            if (pList <> 0) andalso (pList->bDragActive = true) then
                PsListBox_UpdateDragTarget( pList, hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
                PsListBox_CommitDrop( pList, hWin )
                PsListBox_CancelDrag( pList, hWin )
                return 0
            end if
            ' Armed but never dragged: this was a plain click. Honor the deferred selection
            ' collapse now (pressing a selected row in a multi-selection selects just it),
            ' then fall through to the normal up handling so the host still sees the click.
            if (pList <> 0) andalso (pList->bDragArmed = true) then
                pList->bDragArmed = false
                if pList->dragPendCollapse >= 0 then
                    PsListBox_ApplyClickSelection( pList, pList->dragPendCollapse, false, false )
                    pList->dragPendCollapse = -1
                end if
                pList->dragAnchorRow = -1
            end if

            ' If a suppressed double-click armed the skip, this up is that press's
            ' release: consume the flag and do not surface it to the host callback.
            ' (Consume regardless of where the release lands, or a later legitimate
            ' up would be eaten instead.)
            dim as boolean skipNotify = false
            if (pList <> 0) andalso (pList->skipNextLBtnUp = true) then
                pList->skipNextLBtnUp = false
                skipNotify = true
            end if
            dim as long idx = PsListBox_VisIndexFromPoint( pList, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
            if (idx >= 0) andalso (skipNotify = false) then
                ' idx is a VISIBLE index; the callback sees MODEL indices.
                if pList then m.idx = pList->VisibleToModel( idx )
                if pList andalso pList->MessageCallback then
                    ' THE INHERITED COMMENT HERE WAS NOW FALSE AND HAS BEEN DELETED. It
                    ' said the "handled" result must be ignored because swallowing this
                    ' message would strand the mouse capture the listbox took in its own
                    ' WM_LBUTTONDOWN. There is no listbox and no capture: this control
                    ' implements no drag gesture, so per the family's capture rule it
                    ' takes none and pays none of the price. The result is therefore
                    ' honoured here like any other message.
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if
            return 0

        case WM_RBUTTONDOWN, _
             WM_LBUTTONDBLCLK
            dim as long idx = PsListBox_VisIndexFromPoint( pList, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam) )
            dim as boolean suppressDblClk = false
            if (uMsg = WM_LBUTTONDBLCLK) andalso (pList <> 0) then
                suppressDblClk = pList->PreventDblClick
            end if
            if suppressDblClk then
                ' Double-click opt-out: this message SUBSTITUTES for the pair's second
                ' WM_LBUTTONDOWN, so give it the plain-click treatment in the model
                ' (header toggle / selection) but never surface it to the host, and
                ' arm the skip so the trailing second WM_LBUTTONUP is not forwarded
                ' either -- the host sees exactly one down/up pair per double-click.
                ' Armed even for an outside-items press: the second up arrives
                ' regardless of where the press landed.
                pList->skipNextLBtnUp = true
                if idx >= 0 then
                    dim as integer modelRow = pList->VisibleToModel( idx )
                    dim as PSLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )
                    if pRow <> 0 then
                        if pRow->IsHeader then
                            pRow->bCollapsed = not pRow->bCollapsed
                            pList->NotifyChange()
                        else
                            PsListBox_ApplyClickSelection( pList, modelRow, m.isCtrl, m.isShift )
                        end if
                    end if
                end if
            elseif idx >= 0 then
                ' idx is a VISIBLE index; the callback sees MODEL indices.
                if pList then m.idx = pList->VisibleToModel( idx )
                if pList andalso pList->MessageCallback then
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if
            return 0


        case WM_CAPTURECHANGED
            ' Capture yanked away mid-drag (Alt+Tab, a menu, another window's SetCapture):
            ' cancel the drag with NO move. New obligation for this control -- it is the
            ' only gesture here that ever takes capture. CancelDrag won't re-enter us: it
            ' only calls ReleaseCapture when we still hold capture, which we no longer do.
            if (pList <> 0) andalso ((pList->bDragActive = true) orelse (pList->bDragArmed = true)) then
                PsListBox_CancelDrag( pList, hWin )
            end if
            return 0

        Case WM_DESTROY
            ' Don't let the hot-tracking timer outlive the window
            if (pList <> 0) andalso pList->hotTimerOn then
                KillTimer( hWin, IDT_CLISTBOX_HOTTRACK )
                pList->hotTimerOn = false
            end if
            ' Same for the drag auto-scroll timer, and release capture if a drag held it.
            if (pList <> 0) andalso pList->dragTimerOn then
                KillTimer( hWin, IDT_CLISTBOX_DRAGSCROLL )
                pList->dragTimerOn = false
            end if
            if GetCapture() = hWin then ReleaseCapture()
            ' No RemoveWindowSubclass: this is the window's own procedure now, not a
            ' subclass hung off a system listbox.
    end select

    ' For messages that we don't deal with
    function = DefWindowProcW( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' Create a tooltip that pulls its text on demand (LPSTR_TEXTCALLBACK) for the whole
' listbox. Notifications (TTN_GETDISPINFOW) go to the tool's owner = the parent form,
' which answers them in PsListBox_WndProc using the current hot row.
' ========================================================================================
function PsListBox_AddTooltip( byval hCtrl as HWND ) as HWND
    if IsWindow(hCtrl) = 0 then return 0
    dim as HWND hTooltip = CreateWindowExW( 0, "tooltips_class32", "", _
                                            WS_POPUP or TTS_ALWAYSTIP, _
                                            0, 0, 0, 0, NULL, NULL, NULL, NULL )
    if hTooltip = 0 then return 0
    SetWindowPos( hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_NOACTIVATE )

    dim tti as TOOLINFOW
    ' cbSize must match the running comctl32 version (see AfxNova AfxAddTooltip).
    #ifdef __FB_64BIT__
        if AfxComCtlVersion < 600 then tti.cbSize = 64 else tti.cbSize = 72
    #else
        if AfxComCtlVersion < 600 then tti.cbSize = 44 else tti.cbSize = 48
    #endif
    ' hCtrl is a WS_CHILD (the listbox): register the whole control as one tool
    ' owned by the parent form, and let the tooltip auto-relay via TTF_SUBCLASS.
    tti.uFlags   = TTF_IDISHWND or TTF_SUBCLASS
    tti.hwnd     = GetParent(hCtrl)
    tti.uId      = cast(UINT_PTR, hCtrl)
    tti.hinst    = GetModuleHandleW(NULL)
    tti.lpszText = LPSTR_TEXTCALLBACK
    SendMessageW( hTooltip, TTM_ADDTOOLW, 0, cast(LPARAM, @tti) )
    return hTooltip
end function


' ========================================================================================
' PsListBox_Create
' ========================================================================================
function PsListBox_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hForm = _
            pWindow->Create( hWndParent, "", @PsListBox_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' CtrlID becomes the control window's id (GWLP_ID) so hosts can locate the
    ' control with GetDlgItem, same contract as PsVScrollBar/PsStatusBar/PsTabBar.
    SetWindowLongPtr( hForm, GWLP_ID, CtrlID )
    ' Enable double-click messages (WM_LBUTTONDBLCLK) on the container's window
    ' class, preserving the class's existing styles rather than replacing them.
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    ' Define the TYPE that holds all the information about this custom listbox.
    dim as PSLISTBOX ptr pList = new PSLISTBOX
    pList->hWin = hForm
    pList->idc_ListBox = CtrlID
    
    ' Store the new pointer into the user data area of the listbox window
    pWindow->UserData(0) = cast(LONG_PTR, pList)
    

    ' ------------------------------------------------------------------------------------
    ' The row surface. Was a system LISTBOX with LBS_OWNERDRAWFIXED / LBS_NODATA plus a
    ' subclass; it is now a plain registered window class with its own procedure.
    '
    ' The listbox styles are gone rather than translated, because none of them meant
    ' anything to this control any more:
    '   LBS_OWNERDRAWFIXED / LBS_NODATA  the whole point was to get WM_DRAWITEM; the
    '                                    surface paints itself now.
    '   LBS_EXTENDEDSEL / LBS_MULTIPLESEL  selection has always lived in the model's own
    '                                    per-row flags, never in the listbox's selection
    '                                    bits (that is what let it survive a collapse).
    '                                    pList->ExtendSel / MultipleSel still drive it.
    '   LBS_NOTIFY / LBS_NOINTEGRALHEIGHT  notifications were never consumed, and partial
    '                                    bottom rows are simply painted.
    '
    ' CS_DBLCLKS is on the class: the control forwards WM_LBUTTONDBLCLK to the host and
    ' implements the PreventDblClick opt-out around it.
    ' ------------------------------------------------------------------------------------
    ' A WSTRING, not a bare literal: fbc string literals are ASCII, so @"..." yields a
    ' zstring ptr where lpszClassName wants an LPCWSTR -- which compiles with only a
    ' "suspicious pointer assignment" warning and then registers a garbage class name.
    static as wstring * 32 wszSurfClass = "PsListBoxSurface_Class"

    static as boolean bClassRegistered = false
    if bClassRegistered = false then
        dim as WNDCLASSEXW wc
        wc.cbSize        = sizeof(WNDCLASSEXW)
        wc.style         = CS_DBLCLKS
        wc.lpfnWndProc   = @PsListBoxSurface_WndProc
        wc.hInstance     = GetModuleHandleW( NULL )
        wc.hCursor       = LoadCursorW( NULL, cast(LPCWSTR, IDC_ARROW) )
        ' No background brush: the surface paints its entire client in WM_PAINT, and a
        ' class brush would only produce a flash of the wrong colour before it.
        wc.hbrBackground = cast( HBRUSH, NULL )
        wc.lpszClassName = @wszSurfClass
        RegisterClassExW( @wc )
        bClassRegistered = true
    end if

    dim as HWND hList = CreateWindowExW( 0, @wszSurfClass, "", _
            WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_TABSTOP, _
            0, 0, 0, 0, hForm, cast(HMENU, cast(LONG_PTR, pList->idc_ListBox)), _
            GetModuleHandleW( NULL ), NULL )
    pList->hTooltip = PsListBox_AddTooltip( hList )

    ' Owner-drawn vertical scrollbar, owned and driven by this control. It starts
    ' hidden; the first Refresh/Sync decides whether there is anything to scroll.
    pList->hScrollBar = PsVScrollBar_Create( hForm, CtrlID + 1 )
    PsVScrollBar_SetScrollCallback( pList->hScrollBar, @PsListBox_OnScrollBarScroll )

    ' Optional column header band, owned by this control. Starts hidden
    ' (PsListBox_ShowHeader turns it on); columns can be defined regardless --
    ' PositionWindows keeps the window sized even while hidden so its column layout
    ' is always derivable. The WidthChanged slot is this control's chain hook; hosts
    ' subscribe through PsListBox_SetColumnResizeCallback.
    pList->hHeader = PsColumnHeader_Create( hForm, CtrlID + 2 )
    PsColumnHeader_SetWidthChangedCallback( pList->hHeader, @PsListBox_OnHeaderWidthChanged )

    function = hForm

end function

