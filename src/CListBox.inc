
''
''  CListBox.inc
''   

#include once "CListBox.bi"


function GetListBoxPointer( byval hListControl as HWND ) as CLISTBOX ptr
    ' hListBox is actually the overall parent form (ListBox is the child)
    dim pWindow as CWindow ptr = AfxCWindowPtr(hListControl)
    dim as CLISTBOX ptr pList = null
    if pWindow then pList = cast( CLISTBOX ptr, pWindow->UserData(0) )
    return pList
end function


' ========================================================================================
' Calculate the client area at the bottom of the listbox not covered by any row, so it
' can be painted manually in WM_ERASEBKGND (avoids flicker). Only non-empty when the list
' is shorter than the viewport; LBS_NOINTEGRALHEIGHT owner-draws any partial bottom row,
' so there is no partial-line gap to special-case.
' ========================================================================================
function GetListBoxEmptyClientArea( byval hListBox as HWND ) as RECT
    dim as RECT rc: GetClientRect( hListBox, @rc )
    ' Row height from LB_GETITEMHEIGHT is valid even with zero items (fixed-height
    ' owner-draw), unlike LB_GETITEMRECT(0) which fails when item 0 doesn't exist.
    dim as long itemHeight = SendMessage( hListBox, LB_GETITEMHEIGHT, 0, 0 )
    dim as long NumItems   = ListBox_GetCount(hListBox)
    dim as long nTopIndex  = SendMessage( hListBox, LB_GETTOPINDEX, 0, 0 )
    dim as long visible_rows = 0

    if (NumItems > 0) andalso (itemHeight > 0) then
        dim as long ItemsPerPage = (rc.bottom - rc.top) \ itemHeight
        dim as long bottom_index = nTopIndex + ItemsPerPage
        if bottom_index >= NumItems then bottom_index = NumItems - 1
        visible_rows = (bottom_index - nTopIndex) + 1
    end if

    rc.top = visible_rows * itemHeight
    if rc.top > rc.bottom then rc.top = rc.bottom

    return rc
end function


' ========================================================================================
' Add a string (and optional itemData) to the end of the custom listbox control.
' ========================================================================================
function CListBox_AddString( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as CLISTBOX_ROWINFO ptr pRow = pList->AddRow()
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
function CListBox_AddHeader( byval hListControl as HWND, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as CLISTBOX_ROWINFO ptr pRow = pList->AddRow()
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
function CListBox_InsertString( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING, byval itemData as integer = 0, byval itemDataExtra as integer = 0 ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as integer at = row
    if at < 0 then at = 0
    if at > pList->GetCount() then at = pList->GetCount()
    dim as CLISTBOX_ROWINFO ptr pRow = pList->InsertRowAt( at )
    if pRow = null then return -1
    pRow->Text = Text
    pRow->itemData = itemData
    pRow->itemDataExtra = itemDataExtra
    return at
end function


' ========================================================================================
' Delete the row at the given MODEL index. Returns TRUE on success.
' ========================================================================================
function CListBox_DeleteString( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->DeleteRowAt( row )
end function


' ========================================================================================
' Remove all rows from the control.
' ========================================================================================
sub CListBox_Clear( byval hListControl as HWND )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->Clear()
end sub


' ========================================================================================
' Row type / collapse queries and operations. Collapse/Expand/Toggle only act on
' header rows (return FALSE for non-headers or invalid rows).
' ========================================================================================
function CListBox_IsHeader( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if pRow = null then return false
    return pRow->IsHeader
end function

function CListBox_IsCollapsed( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if pRow = null then return false
    return pRow->bCollapsed
end function

function CListBox_CollapseRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    if pRow->bCollapsed = false then
        pRow->bCollapsed = true
        pList->NotifyChange()
    end if
    return true
end function

function CListBox_ExpandRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    if pRow->bCollapsed then
        pRow->bCollapsed = false
        pList->NotifyChange()
    end if
    return true
end function

function CListBox_ExpandAll( byval hListControl as HWND ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow
    for i as integer = 0 to pList->rowCount - 1
        pRow = pList->GetRow(i)
        if (pRow = null) orelse (pRow->IsHeader = false) then continue for
        if pRow->bCollapsed then pRow->bCollapsed = false
    next
    pList->NotifyChange()
    return true
end function

function CListBox_CollapseAll( byval hListControl as HWND ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow
    for i as integer = 0 to pList->rowCount - 1
        pRow = pList->GetRow(i)
        if (pRow = null) orelse (pRow->IsHeader = false) then continue for
        if pRow->bCollapsed = false then pRow->bCollapsed = true
    next
    pList->NotifyChange()
    return true
end function


function CListBox_ToggleRow( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( row )
    if (pRow = null) orelse (pRow->IsHeader = false) then return false
    pRow->bCollapsed = not pRow->bCollapsed
    pList->NotifyChange()
    return true
end function


' ========================================================================================
' Batch multiple model changes into a single visible-map rebuild + repaint. Wrap
' bulk inserts in BeginUpdate/EndUpdate to keep loading O(n) instead of O(n^2).
' ========================================================================================
sub CListBox_BeginUpdate( byval hListControl as HWND )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->BeginUpdate()
end sub

sub CListBox_EndUpdate( byval hListControl as HWND )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->EndUpdate()
end sub


' ========================================================================================
' Number of currently visible rows (headers + non-collapsed items).
' ========================================================================================
function CListBox_GetVisibleCount( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetVisibleCount()
end function


' ========================================================================================
' Set specified row text in the custom listbox control. Returns TRUE on success,
' FALSE if the row index is invalid.
' ========================================================================================
function CListBox_SetText( byval hListControl as HWND, byval row as integer, byval Text as DWSTRING ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
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
function CListBox_GetText( byval hListControl as HWND, byval row as integer ) as DWSTRING
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return ""
    if pList->IsValidRow(row) = false then return ""
    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow(row)
    dim as DWSTRING wszText = pRow->Text
    return wszText
end function


' ========================================================================================
' Set the user defined row painting callback for the custom listbox control.
' ========================================================================================
sub CListBox_SetPaintCallback( byval hListControl as HWND, byval usersub as PaintCallbackSub )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 
    pList->PaintCallback = usersub
end sub


' ========================================================================================
' Set the user defined message callback for the custom listbox control.
' ========================================================================================
sub CListBox_SetMessageCallback( byval hListControl as HWND, byval userfunc as MessageCallbackFunc )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->MessageCallback = userfunc
end sub


' ========================================================================================
' Set the on-demand tooltip text callback. If none is set, the tooltip shows the
' hovered row's Text. Return "" from the callback to suppress the tooltip.
' ========================================================================================
sub CListBox_SetTooltipCallback( byval hListControl as HWND, byval userfunc as TooltipCallbackFunc )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    pList->TooltipCallback = userfunc
end sub


' Resolve the tooltip text for a model row: host callback if set, else row Text.
function CListBox_ResolveTooltipText( byval pList as CLISTBOX ptr, byval modelRow as integer ) as DWSTRING
    if (pList = null) orelse (pList->IsValidRow(modelRow) = false) then return ""
    if pList->TooltipCallback then return pList->TooltipCallback( pList->hWin, modelRow )
    return pList->rows(modelRow).Text
end function


' ========================================================================================
' Set the tooltip hovertime for the custom listbox control.
' ========================================================================================
sub CListBox_SetHoverTime( byval hListControl as HWND, byval milliseconds as integer )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 
    pList->HoverTime = milliseconds
end sub


' ========================================================================================
' Set specified row itemData for the custom listbox control.
' Returns TRUE on success, FALSE if the row index is invalid.
' ========================================================================================
function CListBox_SetItemData( byval hListControl as HWND, byval row as integer, byval itemData as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) then
        pList->GetRow(row)->itemData = itemData
        return true     ' success
    end if
    return false  ' invalid row
end function

function CListBox_SetItemDataExtra( byval hListControl as HWND, byval row as integer, byval itemDataExtra as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
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
function CListBox_GetItemData( byval hListControl as HWND, byval row as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    if pList->IsValidRow(row) = false then return 0
    return pList->GetRow(row)->itemData
end function

function CListBox_GetItemDataExtra( byval hListControl as HWND, byval row as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    if pList->IsValidRow(row) = false then return 0
    return pList->GetRow(row)->itemDataExtra
end function


' ========================================================================================
' Retrieve the focused/current row (MODEL index), or -1 if none.
' ========================================================================================
function CListBox_GetCurSel( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    return pList->focusRow
end function


' ========================================================================================
' Focus and select a single row by MODEL index (clears any other selection).
' Returns the row, or -1 if the index is invalid.
' ========================================================================================
function CListBox_SetCurSel( byval hListControl as HWND, byval row as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if pList->IsValidRow(row) = false then
        pList->ClearSelection()
        pList->focusRow  = -1
        pList->anchorRow = -1
        CListBox_SyncListboxFromModel( pList )
        if hList then InvalidateRect( hList, NULL, FALSE )
        return -1
    end if
    pList->SelectOnly( row )
    pList->focusRow  = row
    pList->anchorRow = row
    ' Ensure-visible, decided in the MODEL: if the new caret is outside the current
    ' page, move topRow so the caret lands on-page (above -> caret at top; below ->
    ' caret at bottom). The Win32 listbox is then re-derived, never asked to scroll
    ' on its own. A row hidden under a collapsed header stays where it is.
    dim as integer visRow = pList->ModelToVisible( row )
    if (hList <> 0) andalso (visRow >= 0) then
        dim as integer visTop  = SendMessage( hList, LB_GETTOPINDEX, 0, 0 )
        dim as integer perPage = CListBox_ItemsPerPage( pList )
        if perPage < 1 then perPage = 1
        if visRow < visTop then
            pList->topRow = row
        elseif visRow >= visTop + perPage then
            pList->topRow = pList->VisibleToModel( visRow - perPage + 1 )
        end if
    end if
    CListBox_SyncListboxFromModel( pList )
    if hList then InvalidateRect( hList, NULL, FALSE )
    return row
end function


' ========================================================================================
' Retrieve the top (first displayed) row as a MODEL index, or -1 if the list is empty
' or the control is invalid.
' The Win32 listbox scrolls in VISIBLE index space, so the raw LB_GETTOPINDEX value is
' translated back through the visible map before being returned.
' ========================================================================================
function CListBox_GetTopIndex( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    if pList->visibleCount = 0 then return -1
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return -1
    dim as integer visTop = SendMessage( hList, LB_GETTOPINDEX, 0, 0 )
    return pList->VisibleToModel( visTop )
end function


' ========================================================================================
' Scroll the listbox so the given MODEL row is the top row, by setting the model's
' topRow and re-deriving the Win32 listbox from it. A row hidden under a collapsed
' header resolves to its group header (inside the sync), so callers may pass any valid
' model row without checking collapse state. The listbox clamps the scroll for itself
' and the clamp is mirrored back, so the returned value is the MODEL row actually at
' the top. Returns -1 if the row or control is invalid.
' ========================================================================================
function CListBox_SetTopIndex( byval hListControl as HWND, byval row as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    if pList->IsValidRow(row) = false then return -1
    pList->topRow = row
    CListBox_SyncListboxFromModel( pList )
    return pList->topRow
end function


' ========================================================================================
' Model-based selection API. Selection lives on the rows (per-row flag), so it
' survives collapse/expand and can include headers and hidden rows.
' ========================================================================================
function CListBox_GetSel( byval hListControl as HWND, byval row as integer ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    return pList->IsRowSelected( row )
end function

function CListBox_SetSel( byval hListControl as HWND, byval row as integer, byval state as boolean ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    if pList->IsValidRow(row) = false then return false
    pList->SetRowSelected( row, state )
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
    return true
end function

function CListBox_GetSelCount( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetSelCount()
end function

' Fill selItems() with the MODEL indices of every selected row (including hidden
' rows under a collapsed header). Returns the count.
function CListBox_GetSelItems( byval hListControl as HWND, selItems() as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
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

sub CListBox_SelectAll( byval hListControl as HWND, byval state as boolean )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    for i as integer = 0 to pList->rowCount - 1
        pList->rows(i).selected = state
    next
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
end sub

' Selection modes are mutually exclusive: extended (Shift/Ctrl ranges) vs.
' multiple (each click toggles). Off = single-select.
function CListBox_SetMultiSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->MultipleSel = enable
    if enable then pList->ExtendSel = false
    return true
end function

function CListBox_SetExtendedSelect( byval hListControl as HWND, byval enable as boolean ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
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
function CListBox_PreventDoubleClick( byval hListControl as HWND, byval enable as boolean = true ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return false
    pList->PreventDblClick = enable
    if enable = false then pList->skipNextLBtnUp = false
    return true
end function


' ========================================================================================
' Retrieve the row height from the custom listbox control.
' ========================================================================================
function CListBox_GetRowHeight( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->RowHeight    
end function


' ========================================================================================
' Set the row height for the custom listbox control.
' ========================================================================================
function CListBox_SetRowHeight( byval hListControl as HWND, byval height as integer ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return -1
    pList->RowHeight = height
    ' Apply immediately -- WM_MEASUREITEM only fires at creation, so a later change
    ' would otherwise have no visible effect until the listbox is recreated.
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then
        dim as long h = height
        dim pWindow as CWindow ptr = AfxCWindowPtr( pList->hWin )
        if pWindow then h = pWindow->ScaleY( height )
        SendMessage( hList, LB_SETITEMHEIGHT, 0, cast( LPARAM, h ) )
        AfxRedrawWindow( hList )
    end if
    ' Row height changes items-per-page, which changes the scrollbar range (and can
    ' flip whether it is needed at all).
    CListBox_SyncScrollBar( pList )
    return height
end function


' ========================================================================================
' Retrieve the font used to paint row text.
' ========================================================================================
function CListBox_GetFont( byval hListControl as HWND ) as HFONT
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->hFont
end function


' ========================================================================================
' Set the font used to paint row text. The caller owns the HFONT and is responsible for
' destroying it; the control only stores the handle.
' ========================================================================================
function CListBox_SetFont( byval hListControl as HWND, byval hFont as HFONT ) as boolean
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
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
function CListBox_GetScrollBar( byval hListControl as HWND ) as HWND
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->hScrollBar
end function

' Width in unscaled units; it is DPI-scaled when the control is laid out.
sub CListBox_SetScrollBarWidth( byval hListControl as HWND, byval nWidth as integer )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if nWidth < 1 then nWidth = 1
    pList->ScrollBarWidth = nWidth
    CListBox_PositionWindows( pList->hWin )
end sub

sub CListBox_SetScrollBarColors( byval hListControl as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hScrollBar then CVScrollBar_SetColors( pList->hScrollBar, backclr, foreclr, foreclrhot )
end sub

sub CListBox_SetScrollBarPaintCallback( byval hListControl as HWND, byval usersub as VScrollPaintCallbackSub )
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return
    if pList->hScrollBar then CVScrollBar_SetPaintCallback( pList->hScrollBar, usersub )
end sub


' ========================================================================================
' Retrieve the background color from the custom listbox control.
' ========================================================================================
function CListBox_GetBackColor( byval hListControl as HWND ) as COLORREF
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->BackColor
end function


' ========================================================================================
' Set the background color for the custom listbox control.
' ========================================================================================
function CListBox_SetBackColor( byval hListControl as HWND, byval clr as COLORREF ) as COLORREF 
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    dim as COLORREF oldClr = pList->BackColor
    pList->BackColor = clr
    return oldClr
end function


' ========================================================================================
' Retrieve the current row count in the custom listbox control.
' ========================================================================================
function CListBox_GetCount( byval hListControl as HWND ) as integer
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList = null then return 0
    return pList->GetCount()
end function


' ========================================================================================
' Refresh the custom listbox control.
' ========================================================================================
sub CListBox_Refresh( byval hListControl as HWND ) 
    dim as CLISTBOX ptr pList = GetListBoxPointer(hListControl)
    if pList then pList->Refresh()
end sub



' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function CListBox_PositionWindows( byval hwnd as HWND ) as LRESULT
    dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList = null then return 0

    ' Get the entire client area
    dim as Rect rc
    GetClientRect( hwnd, @rc )

    ' Reserve a strip on the right edge for the scrollbar, but only while it is shown
    ' (auto-hide gives the width back to the listbox).
    dim as integer sbW = 0
    if (pList->hScrollBar <> 0) andalso pList->scrollBarShown then
        sbW = pList->ScrollBarWidth
        dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
        if pWindow then sbW = pWindow->ScaleX( pList->ScrollBarWidth )
    end if

    dim as HWND hListbox = GetDlgItem( hwnd, pList->idc_ListBox )
    SetWindowPos( hListbox, 0, _
                    rc.left, rc.top, (rc.right - rc.left) - sbW, rc.bottom - rc.top, _
                    SWP_NOZORDER )

    if (pList->hScrollBar <> 0) andalso (sbW > 0) then
        SetWindowPos( pList->hScrollBar, 0, _
                        rc.right - sbW, rc.top, sbW, rc.bottom - rc.top, _
                        SWP_NOZORDER )
    end if

    function = 0
end function


' ========================================================================================
' The scrollbar reports that the user scrolled: move the listbox to that top row.
' ========================================================================================
sub CListBox_OnScrollBarScroll( byval hSB as HWND, byval newPos as integer )
    ' the scrollbar is a child of the CListBox container window
    dim as CLISTBOX ptr pList = GetListBoxPointer( GetParent(hSB) )
    if pList = 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return
    SendMessage( hList, LB_SETTOPINDEX, newPos, 0 )
    ' The listbox clamps for itself; mirror whatever it actually did so the thumb can
    ' never show a position the list isn't at. SetPos does not re-enter the callback.
    dim as integer actual = SendMessage( hList, LB_GETTOPINDEX, 0, 0 )
    if actual <> newPos then CVScrollBar_SetPos( hSB, actual )
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
sub CListBox_SyncScrollBar( byval pList as CLISTBOX ptr )
    if pList = 0 then return
    if pList->hScrollBar = 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return

    CVScrollBar_SetRange( pList->hScrollBar, _
                          pList->visibleCount, _
                          CListBox_ItemsPerPage( pList ), _
                          SendMessage( hList, LB_GETTOPINDEX, 0, 0 ) )

    ' Auto-hide. This only changes the listbox WIDTH, while items-per-page depends on
    ' HEIGHT, so the decision cannot feed back on itself and oscillate.
    dim as boolean needed = CVScrollBar_NeedsThumb( pList->hScrollBar )
    if needed <> pList->scrollBarShown then
        pList->scrollBarShown = needed
        ShowWindow( pList->hScrollBar, iif( needed, SW_SHOW, SW_HIDE ) )
        CListBox_PositionWindows( pList->hWin )   ' listbox reclaims / gives up the strip
    end if
end sub


' ========================================================================================
' Pull the Win32 listbox's ACTUAL top row back into the model (topRow). Only valid
' while visibleMap still matches the listbox contents -- i.e. BEFORE a rebuild. Called
' at the top of Refresh (so user scrolls survive rebuilds) and when the control is
' shown (so scrolls made through paths that bypass the model are absorbed).
' ========================================================================================
sub CListBox_CaptureTopRow( byval pList as CLISTBOX ptr )
    if pList = 0 then return
    if pList->visibleCount = 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return
    dim as integer modelRow = pList->VisibleToModel( SendMessage( hList, LB_GETTOPINDEX, 0, 0 ) )
    if modelRow >= 0 then pList->topRow = modelRow
end sub


' ========================================================================================
' Re-derive the Win32 listbox (count, caret, top row) and the scrollbar from the MODEL.
' The listbox is a view: it never holds state the model does not, so after any model
' change -- and whenever the control is shown -- this one routine re-converges it.
' LB_SETCOUNT resets the listbox's scroll and caret, which is exactly why both are
' re-pushed afterwards, in this order: caret first (LB_SETCURSEL scrolls the caret
' into view), then the top row, so topRow is the final authority on scroll position.
' The listbox clamps the scroll for itself; the clamped result is mirrored back into
' topRow so the model always reflects what is actually on screen.
' ========================================================================================
sub CListBox_SyncListboxFromModel( byval pList as CLISTBOX ptr )
    if pList = 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return

    SendMessage( hList, LB_SETCOUNT, pList->visibleCount, 0 )

    if pList->visibleCount = 0 then
        pList->topRow = 0
        CListBox_SyncScrollBar( pList )
        return
    end if

    ' Caret (single-select styles only: LB_SETCURSEL is LB_ERR on multi-select
    ' listboxes, where selection is painted from the model's per-row flags anyway).
    ' A focusRow of -1, or one hidden under a collapsed header, clears the caret.
    if (pList->MultipleSel = false) andalso (pList->ExtendSel = false) then
        ListBox_SetCurSel( hList, pList->ModelToVisible( pList->focusRow ) )
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
    SendMessage( hList, LB_SETTOPINDEX, visRow, 0 )

    ' Mirror what the listbox actually did back into the model and the scrollbar.
    dim as integer actualTop = pList->VisibleToModel( SendMessage( hList, LB_GETTOPINDEX, 0, 0 ) )
    if actualTop >= 0 then pList->topRow = actualTop
    CListBox_SyncScrollBar( pList )
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: CListBox
' ========================================================================================
function CListBox_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        CListBox_PositionWindows( hwnd )
        ' Height changed -> items-per-page changed -> scrollbar range changed
        CListBox_SyncScrollBar( GetListBoxPointer(hwnd) )
    end if
    function = 0
end function
 
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: CListBox
' ========================================================================================
function CListBox_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList = null then return 0

    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColors( pList->BackColor, pList->BackColor )
    b.PaintClientRect()
    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: CListBox
' ========================================================================================
function CListBox_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit function

    dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList then 
        ' Set the height of the list box items. 
        lpmis->itemHeight = pWindow->ScaleY( pList->RowHeight )
    end if
        
    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: CListBox
' ========================================================================================
function CListBox_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = null then exit function

    if lpdis = null then exit function
    
    dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
    if pList = null then exit function

    if ( lpdis->itemAction = ODA_DRAWENTIRE ) orelse _
        ( lpdis->itemAction = ODA_SELECT ) orelse _
        ( lpdis->itemAction = ODA_FOCUS ) then

        dim as clsDoubleBuffer b

        ' Reuse a per-control back buffer (one row) instead of creating/destroying
        ' a compatible DC + bitmap for every row on every repaint.
        dim as integer rowW = lpdis->rcItem.right  - lpdis->rcItem.left
        dim as integer rowH = lpdis->rcItem.bottom - lpdis->rcItem.top
        dim as HDC cachedDC = pList->EnsureCache( lpdis->hDC, rowW, rowH )
        b.BeginDoubleBuffer(lpdis->hwndItem, lpdis->hDC, lpdis->rcItem, cachedDC)
        b.SetFont( pList->hFont )    ' default row font; the paint callback may override

        ' Hot = the single row the mouse is currently over. pList->nLastHotIdx is a
        ' VISIBLE index (same space as lpdis->itemID) and is the ONE source of truth,
        ' so exactly one row is ever hot and it clears deterministically on
        ' WM_MOUSELEAVE. Do NOT hit-test the cursor here (paint-time race) and do NOT
        ' fold selection into "hot" -- selection highlighting is a Phase 3 concern.
        dim as boolean isHot = (lpdis->itemID = pList->nLastHotIdx)

        ' Create our rect that works with the entire line
        dim as RECT rc
        SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )

        ' lpdis->itemID is the VISIBLE (listbox) index; translate to the model row.
        dim as integer modelRow = pList->VisibleToModel( lpdis->itemID )
        dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )

        dim as CLISTBOX_PAINTINFO p
        with p
            .itemID      = modelRow
            .b           = @b
            .rc          = rc
            .isHot       = isHot
            .isSelected  = false
            .isFocused   = (modelRow = pList->focusRow)
            .isHeader    = false
            .isCollapsed = false
            if pRow then
                .isSelected  = pRow->selected
                .isHeader    = pRow->IsHeader
                .isCollapsed = pRow->bCollapsed
                .wszCaption  = pRow->Text
            end if
        end with

        if pList->PaintCallback then pList->PaintCallback( @p )

        b.EndDoubleBuffer()

    end if   

    function = true
    
end function


' ========================================================================================
' CListBox Window procedure
' ========================================================================================
function CListBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        CListBox_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       CListBox_OnPaint)
        HANDLE_MSG (hwnd, WM_MEASUREITEM, CListBox_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    CListBox_OnDrawItem)
    
    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot row's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if pnmh andalso (pnmh->code = TTN_GETDISPINFOW) then
            dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
            if pList then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                dim as integer modelRow = pList->VisibleToModel( pList->nLastHotIdx )
                ' store in an instance-lifetime buffer and point lpszText at it
                pList->wszTooltip = CListBox_ResolveTooltipText( pList, modelRow )
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
            dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
            CListBox_CaptureTopRow( pList )
            CListBox_SyncListboxFromModel( pList )
        end if

    case WM_NCDESTROY
        ' Delete the stored listbox data pointer
        dim as CLISTBOX ptr pList = GetListBoxPointer(hwnd)
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
' Apply a mouse-click selection to the model, honoring the selection mode and the
' Ctrl/Shift modifiers, then repaint. modelRow must be a valid model row.
' ========================================================================================
sub CListBox_ApplyClickSelection( byval pList as CLISTBOX ptr, byval modelRow as integer, byval isCtrl as boolean, byval isShift as boolean )
    if (pList = null) orelse (pList->IsValidRow(modelRow) = false) then return

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
end sub


' ========================================================================================
' Clear any hot-tracked row and stop the safety-net timer. Repaints only if a row was
' actually hot, so this is cheap to call repeatedly.
' ========================================================================================
sub CListBox_ClearHotTracking( byval pList as CLISTBOX ptr, byval hList as HWND )
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
function CListBox_ItemsPerPage( byval pList as CLISTBOX ptr ) as integer
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return 1
    dim as long ih = SendMessage( hList, LB_GETITEMHEIGHT, 0, 0 )
    if ih <= 0 then return 1
    dim as RECT rc : GetClientRect( hList, @rc )
    dim as integer n = (rc.bottom - rc.top) \ ih
    if n < 1 then n = 1
    return n
end function

sub CListBox_EnsureVisible( byval pList as CLISTBOX ptr, byval modelRow as integer )
    dim as integer vis = pList->ModelToVisible( modelRow )
    if vis < 0 then return
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList = 0 then return
    dim as long top = SendMessage( hList, LB_GETTOPINDEX, 0, 0 )
    dim as integer perPage = CListBox_ItemsPerPage( pList )
    if vis < top then
        SendMessage( hList, LB_SETTOPINDEX, vis, 0 )
        CListBox_SyncScrollBar( pList )
    elseif vis >= top + perPage then
        SendMessage( hList, LB_SETTOPINDEX, vis - perPage + 1, 0 )
        CListBox_SyncScrollBar( pList )
    end if
end sub

' Nearest header at or before modelRow (the group a row belongs to), or -1.
function CListBox_ParentHeader( byval pList as CLISTBOX ptr, byval modelRow as integer ) as integer
    for i as integer = modelRow to 0 step -1
        if pList->rows(i).IsHeader then return i
    next
    return -1
end function

' Move the focus to a VISIBLE position (clamped) and apply selection per the
' control's mode and the Shift/Ctrl modifiers, then ensure-visible and repaint.
sub CListBox_MoveFocusVis( byval pList as CLISTBOX ptr, byval newVis as integer, byval isShift as boolean, byval isCtrl as boolean )
    if pList->visibleCount <= 0 then return
    if newVis < 0 then newVis = 0
    if newVis > pList->visibleCount - 1 then newVis = pList->visibleCount - 1
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

    CListBox_EnsureVisible( pList, newModel )
    dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
    if hList then InvalidateRect( hList, NULL, FALSE )
end sub

' Left arrow: collapse an expanded header, or (from an item) jump to its header.
sub CListBox_KeyLeft( byval pList as CLISTBOX ptr )
    dim as integer f = pList->focusRow
    if pList->IsValidRow(f) = false then return
    if pList->rows(f).IsHeader then
        if pList->rows(f).bCollapsed = false then
            pList->rows(f).bCollapsed = true
            pList->NotifyChange()
            CListBox_EnsureVisible( pList, f )
        end if
    else
        dim as integer h = CListBox_ParentHeader( pList, f )
        if h >= 0 then
            pList->focusRow  = h
            pList->SelectOnly( h )
            pList->anchorRow = h
            CListBox_EnsureVisible( pList, h )
            dim as HWND hList = GetDlgItem( pList->hWin, pList->idc_ListBox )
            if hList then InvalidateRect( hList, NULL, FALSE )
        end if
    end if
end sub

' Right arrow: expand a collapsed header, or (from an expanded header) step in.
sub CListBox_KeyRight( byval pList as CLISTBOX ptr )
    dim as integer f = pList->focusRow
    if pList->IsValidRow(f) = false then return
    if pList->rows(f).IsHeader = false then return
    if pList->rows(f).bCollapsed then
        pList->rows(f).bCollapsed = false
        pList->NotifyChange()
        CListBox_EnsureVisible( pList, f )
    else
        dim as integer curVis = pList->ModelToVisible( f )
        CListBox_MoveFocusVis( pList, curVis + 1, false, false )
    end if
end sub

' Space: toggle the focus row's selection (multi/extended) or select it (single).
sub CListBox_KeySpace( byval pList as CLISTBOX ptr, byval isCtrl as boolean )
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
end sub


' ========================================================================================
' CListBoxListBox_SubclassProc
' ========================================================================================
function CListBoxListBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)
    dim as CLISTBOX ptr pList = GetListBoxPointer( GetParent(hWin) )

    ' Per-message scratch info passed to the user MessageCallback. A plain local
    ' (NOT static) so that multiple control instances never share it. The hover
    ' tracking index likewise lives per-instance in pList->nLastHotIdx.
    dim m as CLISTBOX_MESSAGEINFO

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
        case WM_TIMER
            if pList andalso (_wParam = IDT_CLISTBOX_HOTTRACK) then
                ' The cursor may have left without us getting a WM_MOUSELEAVE. If it
                ' is no longer over our client area, clear the hot row ourselves.
                dim as RECT rcClient : GetClientRect( hWin, @rcClient )
                if isMouseOverRECT( hWin, rcClient ) = false then
                    CListBox_ClearHotTracking( pList, hWin )
                end if
                return 0
            end if
            ' any other timer id falls through to the default listbox handler

        case WM_KEYDOWN
            if pList then
                dim as boolean isShift = (GetAsyncKeyState(VK_SHIFT) and &H8000)
                dim as boolean isCtrl  = (GetAsyncKeyState(VK_CONTROL) and &H8000)
                dim as integer curVis  = pList->ModelToVisible( pList->focusRow )
                if curVis < 0 then curVis = 0        ' no/hidden focus -> start at top
                dim as integer perPage = CListBox_ItemsPerPage( pList )
                if perPage < 1 then perPage = 1

                select case _wParam
                    case VK_UP
                        CListBox_MoveFocusVis( pList, curVis - 1, isShift, isCtrl ) : return 0
                    case VK_DOWN
                        CListBox_MoveFocusVis( pList, curVis + 1, isShift, isCtrl ) : return 0
                    case VK_PRIOR    ' PageUp
                        CListBox_MoveFocusVis( pList, curVis - perPage, isShift, isCtrl ) : return 0
                    case VK_NEXT     ' PageDown
                        CListBox_MoveFocusVis( pList, curVis + perPage, isShift, isCtrl ) : return 0
                    case VK_HOME
                        CListBox_MoveFocusVis( pList, 0, isShift, isCtrl ) : return 0
                    case VK_END
                        CListBox_MoveFocusVis( pList, pList->visibleCount - 1, isShift, isCtrl ) : return 0
                    case VK_LEFT
                        CListBox_KeyLeft( pList ) : return 0
                    case VK_RIGHT
                        CListBox_KeyRight( pList ) : return 0
                    case VK_SPACE
                        CListBox_KeySpace( pList, isCtrl ) : return 0
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
                dim as long page = CListBox_ItemsPerPage( pList )
                if (linesPerNotch < 1) orelse (linesPerNotch > page) then linesPerNotch = page  ' WHEEL_PAGESCROLL / bogus
                if linesPerNotch < 1 then linesPerNotch = 1

                pList->accumDelta += GET_WHEEL_DELTA_WPARAM( _wParam )
                dim as long nTopIndex = SendMessage( hWin, LB_GETTOPINDEX, 0, 0 )
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
                    if nTopIndex < 0 then nTopIndex = 0
                    SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 )
                    CListBox_SyncScrollBar( pList )   ' keep the thumb with the wheel
                    if (pList->MessageCallback <> 0) andalso pList->MessageCallback( @m ) then return 0
                end if
            end if


        Case WM_MOUSEMOVE
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
                SetTimer( hWin, IDT_CLISTBOX_HOTTRACK, CLISTBOX_HOTTRACK_MS, NULL )
                pList->hotTimerOn = true
            end if

            ' get the item rect that the mouse is over and only invalidate
            ' that instead of the entire listbox
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if pList andalso hiword(idx) <> 1 then
                if idx <> pList->nLastHotIdx then
                    ' Repaint the newly-hot row and the previously-hot row. FALSE =
                    ' no background erase; the double buffer fully paints each row.
                    ListBox_GetItemRect( hWin, idx, @rc )
                    InvalidateRect( hWin, @rc, FALSE )
                    if pList->nLastHotIdx <> -1 then
                        ListBox_GetItemRect( hWin, pList->nLastHotIdx, @rc )
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
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
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
                CListBox_ClearHotTracking( pList, hWin )
                if (pList->MessageCallback <> 0) andalso pList->MessageCallback( @m ) then return 0
            end if
            

        case WM_LBUTTONDOWN
            ' A fresh press means any armed double-click up-skip lost its pairing
            ' partner (capture change, focus loss) -- disarm it so it cannot eat
            ' this press's own release.
            if pList then pList->skipNextLBtnUp = false
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam))
            if pList andalso hiword(idx) <> 1 then
                dim as integer modelRow = pList->VisibleToModel( idx )
                m.idx = modelRow
                dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )
                if (pRow <> 0) andalso pRow->IsHeader then
                    ' Header: clicking toggles the group collapse and deliberately
                    ' leaves the selection untouched (a fold gesture, not a select).
                    pRow->bCollapsed = not pRow->bCollapsed
                    pList->NotifyChange()
                else
                    ' Item: apply the mouse selection to the model.
                    CListBox_ApplyClickSelection( pList, modelRow, m.isCtrl, m.isShift )
                end if
                if pList->MessageCallback then
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if
            ' fall through to DefSubclassProc so the listbox takes keyboard focus


        case WM_LBUTTONUP
            ' If a suppressed double-click armed the skip, this up is that press's
            ' release: consume the flag and do not surface it to the host callback.
            ' (Consume regardless of where the release lands, or a later legitimate
            ' up would be eaten instead.) Default handling still runs below.
            dim as boolean skipNotify = false
            if (pList <> 0) andalso (pList->skipNextLBtnUp = true) then
                pList->skipNextLBtnUp = false
                skipNotify = true
            end if
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam))
            if (hiword(idx) <> 1) andalso (skipNotify = false) then
                ' idx is a VISIBLE index; the callback sees MODEL indices.
                if pList then m.idx = pList->VisibleToModel( idx )
                if pList andalso pList->MessageCallback then
                    ' Notify, but deliberately IGNORE the "handled" result here: we must
                    ' always fall through to DefSubclassProc. The listbox captures the
                    ' mouse in its default WM_LBUTTONDOWN handling and only releases it
                    ' in its default WM_LBUTTONUP handling. Swallowing this message leaks
                    ' the capture, so every later click is routed to the listbox instead
                    ' of its siblings -- e.g. the scrollbar would need a throwaway first
                    ' click before a drag would start.
                    pList->MessageCallback( @m )
                end if
            end if

        case WM_RBUTTONDOWN, _
             WM_LBUTTONDBLCLK
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam))
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero
            ' if the specified point is in the client area of the list box, or one if it is outside the
            ' client area.
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
                ' regardless of where the press landed. Default handling still runs
                ' below (the listbox takes capture on the dblclk like a down).
                pList->skipNextLBtnUp = true
                if hiword(idx) <> 1 then
                    dim as integer modelRow = pList->VisibleToModel( idx )
                    dim as CLISTBOX_ROWINFO ptr pRow = pList->GetRow( modelRow )
                    if pRow <> 0 then
                        if pRow->IsHeader then
                            pRow->bCollapsed = not pRow->bCollapsed
                            pList->NotifyChange()
                        else
                            CListBox_ApplyClickSelection( pList, modelRow, m.isCtrl, m.isShift )
                        end if
                    end if
                end if
            elseif hiword(idx) <> 1 then
                ' idx is a VISIBLE index; the callback sees MODEL indices.
                if pList then m.idx = pList->VisibleToModel( idx )
                if pList andalso pList->MessageCallback then
                    if pList->MessageCallback( @m ) then return 0
                end if
            end if


        case WM_ERASEBKGND
            ' Paint only the empty area below the last row with the control's back
            ' color (the rows themselves are drawn via WM_DRAWITEM). Prevents the
            ' default full-client erase, which would flicker.
            if pList then
                dim as RECT rc = GetListBoxEmptyClientArea( hWin )
                if rc.top < rc.bottom then
                    dim as HDC _hDC = cast(HDC, _wParam)
                    PaintRect( _hDC, @rc, pList->BackColor )
                end if
                return true
            end if    
            
        Case WM_DESTROY
            ' Don't let the hot-tracking timer outlive the window
            if (pList <> 0) andalso pList->hotTimerOn then
                KillTimer( hWin, IDT_CLISTBOX_HOTTRACK )
                pList->hotTimerOn = false
            end if
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @CListBoxListBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' Create a tooltip that pulls its text on demand (LPSTR_TEXTCALLBACK) for the whole
' listbox. Notifications (TTN_GETDISPINFOW) go to the tool's owner = the parent form,
' which answers them in CListBox_WndProc using the current hot row.
' ========================================================================================
function CListBox_AddTooltip( byval hCtrl as HWND ) as HWND
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
' CListBox_Create
' ========================================================================================
function CListBox_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hForm = _
            pWindow->Create( hWndParent, "", @CListBox_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' Enable double-click messages (WM_LBUTTONDBLCLK) on the container's window
    ' class, preserving the class's existing styles rather than replacing them.
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    ' Define the TYPE that holds all the information about this custom listbox.
    dim as CLISTBOX ptr pList = new CLISTBOX
    pList->hWin = hForm
    pList->idc_ListBox = CtrlID
    
    ' Store the new pointer into the user data area of the listbox window
    pWindow->UserData(0) = cast(LONG_PTR, pList)
    

    dim as long ExtendSel = iif( pList->ExtendSel, LBS_EXTENDEDSEL, 0 )
    dim as long MultipleSel = iif( pList->MultipleSel, LBS_MULTIPLESEL, 0 )
    
    dim as HWND hList = _ 
        pWindow->AddControl( "LISTBOX", , pList->idc_ListBox, "", 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or WS_TABSTOP or _
        LBS_NOINTEGRALHEIGHT or ExtendSel or MultipleSel or _
        LBS_OWNERDRAWFIXED or LBS_NODATA or LBS_NOTIFY, _
        WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass( hList, @CListBoxListBox_SubclassProc, pList->idc_ListBox, @pWindow )
    pList->hTooltip = CListBox_AddTooltip( hList )

    ' Owner-drawn vertical scrollbar, owned and driven by this control. It starts
    ' hidden; the first Refresh/Sync decides whether there is anything to scroll.
    pList->hScrollBar = CVScrollBar_Create( hForm, CtrlID + 1 )
    CVScrollBar_SetScrollCallback( pList->hScrollBar, @CListBox_OnScrollBarScroll )

    function = hForm
    
end function

