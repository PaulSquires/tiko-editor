
''
''  PsTabBar.inc
''   

#include once "PsTabBar.bi"


private function GetTabBarPointer( byval hTabBar as HWND ) as PSTABBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hTabBar)
    dim as PSTABBAR ptr pTabBar = null
    if pWindow then pTabBar = cast( PSTABBAR ptr, pWindow->UserData(0) )
    return pTabBar
end function

' Which tab is the cursor over? Returns a index, or -1 for none (the cursor is
' over empty bar area, or outside the control entirely).
private function GetHotTabBarTab( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return -1
    dim as integer idx = -1
    for i as integer = 0 to pTabBar->tabCount - 1
        if isMouseOverRECT( hTabBar, pTabBar->tabs(i).rc ) then
            idx = i: exit for
        end if
    next
    return idx
end function


' Repaint one tab. Silently does nothing for an invalid index, which is what makes the
' hover path safe when the cursor is over empty bar area (idx = -1). FALSE = no background
' erase; the tab's rect is fully painted by the paint callback.
private sub PsTabBar_InvalidateTab( byval pTabBar as PSTABBAR ptr, byval idx as integer )
    if pTabBar = null then exit sub
    if pTabBar->IsValidTab(idx) = false then exit sub
    InvalidateRect( pTabBar->hWin, @pTabBar->tabs(idx).rc, FALSE )
end sub



' ========================================================================================
' Add a string (and optional itemData) to the end of the custom tab bar.
' ========================================================================================
function PsTabBar_AddTab( byval hTabBar as HWND, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return -1
    dim as PSTABBAR_TABINFO ptr pTab = pTabBar->AddTab()
    if pTab = null then return -1
    pTab->Text = Text
    pTab->itemData = itemData
    return pTabBar->GetCount() - 1  ' index of the appended tab
end function


' ========================================================================================
' Returns whether the specified tab index is valid.
' ========================================================================================
function PsTabBar_IsValidTab( byval hTabBar as HWND, byval idx as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    return pTabBar->IsValidTab(idx)
end function


' ========================================================================================
' Insert a tab at the given index (existing tabs shift right). Returns the
' actual index used (clamped to [0, count]), or -1 on failure.
' ========================================================================================
function PsTabBar_InsertTab( byval hTabBar as HWND, byval idx as integer, byval Text as DWSTRING, byval itemData as integer = 0 ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return -1
    dim as integer at = idx
    if at < 0 then at = 0
    if at > pTabBar->GetCount() then at = pTabBar->GetCount()
    dim as PSTABBAR_TABINFO ptr pTab = pTabBar->InsertTabAt( at )
    if pTab = null then return -1
    pTab->Text = Text
    pTab->itemData = itemData
    return at
end function


' ========================================================================================
' Delete the tab at the given index. Returns TRUE on success.
' ========================================================================================
function PsTabBar_DeleteTab( byval hTabBar as HWND, byval idx as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    return pTabBar->DeleteTabAt( idx )
end function


' ========================================================================================
' Move a tab to another slot programmatically -- the same index remapping a drag does
' (selection and hover follow the moved tab; scroll position stays put). SILENT, like
' SetCurSel: the reorder callback fires only for user drags. FALSE for a bad index.
' ========================================================================================
function PsTabBar_MoveTab( byval hTabBar as HWND, byval idxFrom as integer, byval idxTo as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idxFrom) = false then return false
    if pTabBar->IsValidTab(idxTo) = false then return false
    pTabBar->MoveTab( idxFrom, idxTo )
    return true
end function


' ========================================================================================
' Remove all tabs from the control.
' ========================================================================================
sub PsTabBar_Clear( byval hTabBar as HWND )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->Clear()
end sub


' ========================================================================================
' Set specified tab text in the custom tab bar. Returns TRUE on success,
' FALSE if the idx index is invalid.
' ========================================================================================
function PsTabBar_SetText( byval hTabBar as HWND, byval idx as integer, byval Text as DWSTRING ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) then
        pTabBar->GetTab(idx)->Text = Text
        ' Text drives the tab's width, so this needs a re-layout, not just a repaint.
        pTabBar->Refresh()
        return true     ' success
    end if
    return false  ' invalid tab
end function


' ========================================================================================
' Retrieve specified tab text from the custom tab bar.
' ========================================================================================
function PsTabBar_GetText( byval hTabBar as HWND, byval idx as integer ) as DWSTRING
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return ""
    if pTabBar->IsValidTab(idx) = false then return ""
    dim as PSTABBAR_TABINFO ptr pTab = pTabBar->GetTab(idx)
    dim as DWSTRING wszText = pTab->Text
    return wszText
end function


' ========================================================================================
' Set the user defined tab's painting callback for the custom tab bar.
' ========================================================================================
sub PsTabBar_SetPaintCallback( byval hTabBar as HWND, byval usersub as TB_PaintCallbackSub )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 
    pTabBar->PaintCallback = usersub
end sub


' ========================================================================================
' Set the user defined message callback for the custom tab bar.
' ========================================================================================
sub PsTabBar_SetMessageCallback( byval hTabBar as HWND, byval userfunc as TB_MessageCallbackFunc )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->MessageCallback = userfunc
end sub


' ========================================================================================
' Set the on-demand tooltip text callback. If none is set, the tooltip shows the
' hovered tab's Text. Return "" from the callback to suppress the tooltip.
' ========================================================================================
sub PsTabBar_SetTooltipCallback( byval hTabBar as HWND, byval userfunc as TB_TooltipCallbackFunc )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->TooltipCallback = userfunc
end sub


' ========================================================================================
' Set the selection-change callback. Fired only for USER-driven changes, after the
' control's state is updated (GetCurSel already answers the new index). Programmatic
' SetCurSel never fires it.
' ========================================================================================
sub PsTabBar_SetSelChangeCallback( byval hTabBar as HWND, byval usersub as TB_SelChangeCallbackSub )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->SelChangeCallback = usersub
end sub


' ========================================================================================
' Set the close-request callback. Notify-only: the control never deletes a tab. The
' host prompts/vetoes/deletes as it sees fit.
' ========================================================================================
sub PsTabBar_SetCloseCallback( byval hTabBar as HWND, byval usersub as TB_CloseCallbackSub )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->CloseCallback = usersub
end sub


' ========================================================================================
' Set the reorder callback: one call per live drag move, fired after the control's
' array and indices are updated. Programmatic MoveTab never fires it.
' ========================================================================================
sub PsTabBar_SetReorderCallback( byval hTabBar as HWND, byval usersub as TB_ReorderCallbackSub )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return
    pTabBar->ReorderCallback = usersub
end sub


' Resolve the tooltip text for a tab: host callback if set, else the tab's Text.
function PsTabBar_ResolveTooltipText( byval pTabBar as PSTABBAR ptr, byval idx as integer ) as DWSTRING
    if (pTabBar = null) orelse (pTabBar->IsValidTab(idx) = false) then return ""
    if pTabBar->TooltipCallback then return pTabBar->TooltipCallback( pTabBar->hWin, idx )
    return pTabBar->tabs(idx).Text
end function


' ========================================================================================
' Set the tooltip hovertime for the custom tab bar.
' ========================================================================================
sub PsTabBar_SetHoverTime( byval hTabBar as HWND, byval milliseconds as integer )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 
    pTabBar->HoverTime = milliseconds
end sub


' ========================================================================================
' Set specified idx itemData for the custom tab bar.
' ========================================================================================
' Returns TRUE on success, FALSE if the idx index is invalid.
function PsTabBar_SetItemData( byval hTabBar as HWND, byval idx as integer, byval itemData as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) then
        pTabBar->GetTab(idx)->itemData = itemData
        return true     ' success
    end if
    return false  ' invalid tab
end function


' ========================================================================================
' Retrieve specified tab itemData from the custom tab bar.
' ========================================================================================
function PsTabBar_GetItemData( byval hTabBar as HWND, byval idx as integer ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    if pTabBar->IsValidTab(idx) = false then return 0
    return pTabBar->GetTab(idx)->itemData
end function


' ========================================================================================
' Retrieve the font used to measure tab text.
' ========================================================================================
function PsTabBar_GetFont( byval hTabBar as HWND ) as HFONT
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->hTabFont
end function


' ========================================================================================
' Set the font used to paint tab text. The caller owns the HFONT and is responsible for
' destroying it; the control only stores the handle.
' ========================================================================================
function PsTabBar_SetFont( byval hTabBar as HWND, byval hFont as HFONT ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    pTabBar->hTabFont = hFont
    ' The font is what tab widths are measured with, so changing it re-lays out.
    pTabBar->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the minimum width (pixels) of the specified tab. 0 = auto-size to text.
' ========================================================================================
function PsTabBar_GetTabMinWidth( byval hTabBar as HWND, byval idx as integer ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    if pTabBar->IsValidTab(idx) = false then return 0
    return pTabBar->GetTab(idx)->nMinWidth
end function


' ========================================================================================
' Set the minimum width (pixels) of the specified tab. The tab never renders narrower
' than its text, so this is a floor, not a fixed width. Returns FALSE for a bad index.
' ========================================================================================
function PsTabBar_SetTabMinWidth( byval hTabBar as HWND, byval idx as integer, byval nMinWidth as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) = false then return false
    if nMinWidth < 0 then nMinWidth = 0
    pTabBar->GetTab(idx)->nMinWidth = nMinWidth
    pTabBar->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the horizontal padding (pixels) added to each side of a tab's text.
' ========================================================================================
function PsTabBar_GetPadding( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->nPadding
end function


' ========================================================================================
' Set the horizontal padding (raw pixels -- the caller DPI-scales) added to each side of a
' tab's text when measuring. Inset your paint callback's text by the same amount.
' ========================================================================================
sub PsTabBar_SetPadding( byval hTabBar as HWND, byval nPadding as integer )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then exit sub
    if nPadding < 0 then nPadding = 0
    pTabBar->nPadding = nPadding
    pTabBar->Refresh()
end sub


' ========================================================================================
' Retrieve the side (pixels) of the two square icon rects reserved in every tab.
' ========================================================================================
function PsTabBar_GetIconWidth( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->nIconWidth
end function


' ========================================================================================
' Set the icon rect side (raw pixels -- the caller DPI-scales). A layout input: every
' tab reserves 2*nIconWidth, so changing it re-measures everything.
' ========================================================================================
sub PsTabBar_SetIconWidth( byval hTabBar as HWND, byval nIconWidth as integer )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then exit sub
    if nIconWidth < 0 then nIconWidth = 0
    pTabBar->nIconWidth = nIconWidth
    pTabBar->Refresh()
end sub


' ========================================================================================
' Retrieve the width (pixels) of the control-drawn border chrome.
' ========================================================================================
function PsTabBar_GetBorderWidth( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->nBorderWidth
end function


' ========================================================================================
' Set the border width (raw pixels -- the caller DPI-scales). A layout input: rcPaint
' is deflated by it. 0 is legal and means no chrome.
' ========================================================================================
sub PsTabBar_SetBorderWidth( byval hTabBar as HWND, byval nBorderWidth as integer )
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then exit sub
    if nBorderWidth < 0 then nBorderWidth = 0
    pTabBar->nBorderWidth = nBorderWidth
    pTabBar->Refresh()
end sub


' ========================================================================================
' Retrieve the border (chrome) color.
' ========================================================================================
function PsTabBar_GetBorderColor( byval hTabBar as HWND ) as COLORREF
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->BorderColor
end function


' ========================================================================================
' Set the border (chrome) color. Returns the previous color.
' ========================================================================================
function PsTabBar_SetBorderColor( byval hTabBar as HWND, byval clr as COLORREF ) as COLORREF
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    dim as COLORREF oldClr = pTabBar->BorderColor
    pTabBar->BorderColor = clr
    if pTabBar->hWin then InvalidateRect( pTabBar->hWin, NULL, TRUE )
    return oldClr
end function


' ========================================================================================
' Retrieve the computed client-coordinate rect of the specified tab. Forces a pending
' layout so the result is always current. Returns FALSE for a bad index.
' ========================================================================================
function PsTabBar_GetTabRect( byval hTabBar as HWND, byval idx as integer, byref rc as RECT ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    SetRectEmpty( @rc )
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) = false then return false
    if pTabBar->bLayoutDirty then pTabBar->LayoutTabs()
    rc = pTabBar->GetTab(idx)->rc
    return true
end function


' ========================================================================================
' Retrieve the derived interior rects of the specified tab: rcPaint (the full rect
' pre-deflated by the border -- the space the paint callback owns), rcText (caption
' area between the icon rects), rcIcon (left icon) and rcClose (the close "X"). The
' same rects the paint callback receives. Forces a pending layout. Returns FALSE for a
' bad index. A tab scrolled off the left returns empty rects -- its honest geometry.
' ========================================================================================
function PsTabBar_GetTabParts( byval hTabBar as HWND, byval idx as integer, byref rcPaint as RECT, byref rcText as RECT, byref rcIcon as RECT, byref rcClose as RECT ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    SetRectEmpty( @rcPaint )
    SetRectEmpty( @rcText )
    SetRectEmpty( @rcIcon )
    SetRectEmpty( @rcClose )
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) = false then return false
    if pTabBar->bLayoutDirty then pTabBar->LayoutTabs()
    rcPaint = pTabBar->GetTab(idx)->rcPaint
    rcText  = pTabBar->GetTab(idx)->rcText
    rcIcon  = pTabBar->GetTab(idx)->rcIcon
    rcClose = pTabBar->GetTab(idx)->rcClose
    return true
end function


' ========================================================================================
' Retrieve the index of the active tab, or -1 for none.
' ========================================================================================
function PsTabBar_GetCurSel( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return -1
    return pTabBar->nCurSel
end function


' ========================================================================================
' Set the active tab (-1 clears the selection). A valid index is scrolled into view on
' the next layout, which is where measured widths exist -- so calling this before the
' control is sized is safe, and the scroll happens once it is. Returns FALSE for a bad
' index. Does NOT fire the selection-change notification: only user interaction does.
' ========================================================================================
function PsTabBar_SetCurSel( byval hTabBar as HWND, byval idx as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if (idx <> -1) andalso (pTabBar->IsValidTab(idx) = false) then return false
    pTabBar->nCurSel = idx
    if idx <> -1 then pTabBar->bEnsureVisible = true
    ' Selection is a layout input (the active tab's rcPaint differs), not just a repaint.
    pTabBar->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the first-visible (leftmost drawn) tab index.
' ========================================================================================
function PsTabBar_GetFirstVisible( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->nFirstVisible
end function


' ========================================================================================
' Set the first-visible tab index directly (Navigate is the stepwise form). Returns
' FALSE for a bad index.
' ========================================================================================
function PsTabBar_SetFirstVisible( byval hTabBar as HWND, byval idx as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->IsValidTab(idx) = false then return false
    pTabBar->nFirstVisible = idx
    ' Layout is lazy, so a scroll-into-view queued by an earlier SetCurSel would be
    ' consumed in the same later pass and OVERRIDE this explicit scroll position.
    ' Last mutation wins: an explicit SetFirstVisible cancels the pending request.
    pTabBar->bEnsureVisible = false
    pTabBar->Refresh()
    return true
end function


' ========================================================================================
' Would a one-tab step in the given direction (-1 left / +1 right) actually move?
' Left: not already at the first tab. Right: the last tab still clips at the client
' edge (scrolling further right than "last tab visible" would serve nothing).
' Forces a pending layout; FALSE while the control has no geometry yet.
' ========================================================================================
function PsTabBar_CanNavigate( byval hTabBar as HWND, byval nDirection as integer ) as boolean
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return false
    if pTabBar->tabCount = 0 then return false
    if (nDirection <> -1) andalso (nDirection <> 1) then return false
    if pTabBar->bLayoutDirty then pTabBar->LayoutTabs()
    ' Still dirty means no geometry (0x0 client): the rects are stale, answer nothing.
    if pTabBar->bLayoutDirty then return false
    if nDirection = -1 then return (pTabBar->nFirstVisible > 0)
    if pTabBar->nFirstVisible >= pTabBar->tabCount - 1 then return false
    dim as RECT rcClient
    GetClientRect( pTabBar->hWin, @rcClient )
    return (pTabBar->tabs(pTabBar->tabCount - 1).rc.right > rcClient.right)
end function


' ========================================================================================
' Step the first-visible tab one to the left (-1) or right (+1), gated on CanNavigate,
' so a no-op step is harmless. Returns the (possibly unchanged) first-visible index.
' ========================================================================================
function PsTabBar_Navigate( byval hTabBar as HWND, byval nDirection as integer ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return -1
    if PsTabBar_CanNavigate( hTabBar, nDirection ) then
        pTabBar->nFirstVisible += iif( nDirection < 0, -1, 1 )
        pTabBar->Refresh()
    end if
    return pTabBar->nFirstVisible
end function


' ========================================================================================
' Retrieve the background color from the custom tab bar.
' ========================================================================================
function PsTabBar_GetBackColor( byval hTabBar as HWND ) as COLORREF
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->BackColor
end function


' ========================================================================================
' Set the background color for the custom tab bar.
' ========================================================================================
function PsTabBar_SetBackColor( byval hTabBar as HWND, byval clr as COLORREF ) as COLORREF 
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    dim as COLORREF oldClr = pTabBar->BackColor
    pTabBar->BackColor = clr
    return oldClr
end function


' ========================================================================================
' Retrieve the current tab count in the custom tab bar.
' ========================================================================================
function PsTabBar_GetCount( byval hTabBar as HWND ) as integer
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar = null then return 0
    return pTabBar->GetCount()
end function


' ========================================================================================
' Refresh the custom tab bar.
' ========================================================================================
sub PsTabBar_Refresh( byval hTabBar as HWND ) 
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hTabBar)
    if pTabBar then pTabBar->Refresh()
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: PsTabBar
' ========================================================================================
function PsTabBar_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' There are no child windows to position, but tab rects span the full client
        ' height and the client width decides where the rightmost tab clips, so a
        ' resize invalidates the layout.
        dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hwnd)
        if pTabBar then pTabBar->Refresh()
    end if
    function = 0
end function
 
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: PsTabBar
' ========================================================================================
function PsTabBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSTABBAR ptr pTabBar = GetTabBarPointer(hwnd)
    if pTabBar = null then return 0

    ' Consume any pending layout before painting. This is what makes a burst of SetText
    ' calls cost one measuring pass instead of N.
    if pTabBar->bLayoutDirty then pTabBar->LayoutTabs()

    ' Capture the update rect BEFORE BeginDoubleBuffer -- BeginPaint clears it.
    '
    ' This is a CONSERVATIVE approximation and correctness never depends on it: it only
    ' skips handing the host tabs that could not show a pixel anyway. GetUpdateRect
    ' returns the BOUNDING BOX of the update region, not the region, so when hover moves
    ' between two far-apart tabs their union bounds the whole bar and nothing is skipped.
    ' Measured: hovering one tab repaints one; moving from tab 0 to tab 4 repaints all
    ' five. That is fine -- the buffer allocates and fills the whole client every paint
    ' regardless, so the skip is noise next to that. Reach for GetUpdateRgn/RectInRegion
    ' only if a real tab count ever makes this matter.
    dim as RECT rcUpdate
    if GetUpdateRect( hwnd, @rcUpdate, FALSE ) = 0 then GetClientRect( hwnd, @rcUpdate )

    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pTabBar->BackColor )

    ' Fill the whole bar, which also covers any gap no tab owns (the control's own
    ' background is the only thing it paints; tabs belong to the paint callback).
    b.PaintClientRect()

    ' Hand each affected tab to the host. Skipping the rest is safe even though
    ' EndDoubleBuffer blits the WHOLE client: BeginPaint's DC is clipped to the update
    ' region, so the skipped (background-filled) areas never reach the screen.
    if pTabBar->PaintCallback then
        dim p as PSTABBAR_PAINTINFO
        p.hTabBar   = hwnd
        p.b          = @b
        for i as integer = 0 to pTabBar->tabCount - 1
            dim as RECT rcHit
            if IntersectRect( @rcHit, @pTabBar->tabs(i).rc, @rcUpdate ) = 0 then continue for
            p.itemID     = i
            ' The DEFLATED paint area, plus every derived rect precomputed -- the
            ' callback never re-derives geometry (author's call, see REFACTOR_PLAN.md).
            p.rc         = pTabBar->tabs(i).rcPaint
            p.rcText     = pTabBar->tabs(i).rcText
            p.rcIcon     = pTabBar->tabs(i).rcIcon
            p.rcClose    = pTabBar->tabs(i).rcClose
            p.isSelected = (i = pTabBar->nCurSel)
            p.isHot      = (i = pTabBar->nLastHotIdx)
            p.isHotClose = (i = pTabBar->nLastHotIdx) andalso pTabBar->bLastHotClose
            p.isPressed  = (i = pTabBar->nPressedIdx)
            p.isDragging = (i = pTabBar->nPressedIdx) andalso pTabBar->bDragging
            p.wszCaption = pTabBar->tabs(i).Text
            pTabBar->PaintCallback( @p )
        next
    end if

    ' ------------------------------------------------------------------------------------
    ' Control-drawn chrome, AFTER the callbacks so it is authoritative no matter what a
    ' callback painted: the full-width bottom line, broken under the active tab, and the
    ' active tab's 3-sided (left/top/right) border. The line is drawn as TWO SEGMENTS
    ' skipping the active tab's x-range -- never "draw it fully and let the active tab
    ' paint over it": the control's chrome must not depend on a host callback to
    ' complete it. The active tab's own fill (its rcPaint runs to the client bottom) is
    ' what shows through the break.
    ' ------------------------------------------------------------------------------------
    if pTabBar->nBorderWidth > 0 then
        dim as integer bw = pTabBar->nBorderWidth
        dim as RECT rcC : GetClientRect( hwnd, @rcC )
        b.SetBackColor( pTabBar->BorderColor )

        ' The active tab's x-range, if there is an active tab and it is not scrolled off
        ' (an empty rect must not break the line at 0..0).
        dim as RECT rcSel : SetRectEmpty( @rcSel )
        if pTabBar->IsValidTab( pTabBar->nCurSel ) then rcSel = pTabBar->tabs(pTabBar->nCurSel).rc

        dim as RECT rcStrip
        if IsRectEmpty( @rcSel ) then
            SetRect( @rcStrip, rcC.left, rcC.bottom - bw, rcC.right, rcC.bottom )
            b.PaintRect( @rcStrip )
        else
            if rcSel.left > rcC.left then
                SetRect( @rcStrip, rcC.left, rcC.bottom - bw, rcSel.left, rcC.bottom )
                b.PaintRect( @rcStrip )
            end if
            if rcSel.right < rcC.right then
                SetRect( @rcStrip, rcSel.right, rcC.bottom - bw, rcC.right, rcC.bottom )
                b.PaintRect( @rcStrip )
            end if
            ' 3-sided border; the sides run the full height to meet the broken line's ends.
            SetRect( @rcStrip, rcSel.left, rcSel.top, rcSel.left + bw, rcSel.bottom )
            b.PaintRect( @rcStrip )
            SetRect( @rcStrip, rcSel.left, rcSel.top, rcSel.right, rcSel.top + bw )
            b.PaintRect( @rcStrip )
            SetRect( @rcStrip, rcSel.right - bw, rcSel.top, rcSel.right, rcSel.bottom )
            b.PaintRect( @rcStrip )
        end if
    end if

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Clear any hot-tracked tab and stop the safety-net timer. Repaints only if a tab was
' actually hot, so this is cheap to call repeatedly.
' ========================================================================================
private sub PsTabBar_ClearHotTracking( byval pTabBar as PSTABBAR ptr, byval hTabBar as HWND )
    if pTabBar = null then exit sub
    if pTabBar->hotTimerOn then
        KillTimer( hTabBar, IDT_CTABBAR_HOTTRACK )
        pTabBar->hotTimerOn = false
    end if
    if pTabBar->nLastHotIdx <> -1 then
        pTabBar->nLastHotIdx = -1
        pTabBar->bLastHotClose = false
        AfxRedrawWindow( hTabBar )
    end if
end sub


' ========================================================================================
' Hand the cursor back to the window class after a drag ends. Needed because the drop can
' happen without any further mouse movement: WM_SETCURSOR only arrives with motion, so
' without this the four-headed arrow would stay on screen until the user moved the mouse.
' ========================================================================================
private sub PsTabBar_RestoreCursor( byval hTabBar as HWND )
    dim as HCURSOR hCur = cast( HCURSOR, GetClassLongPtr( hTabBar, GCLP_HCURSOR ) )
    if hCur = 0 then hCur = LoadCursor( null, IDC_ARROW )
    SetCursor( hCur )
end sub


' ========================================================================================
' PsTabBar Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc. 
' ========================================================================================
function PsTabBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSTABBAR ptr pTabBar = GetTabBarPointer( hwnd )

    ' Per-message scratch info passed to the user MessageCallback. A plain local (NOT
    ' static) so that multiple control instances never share it. The hover tracking index
    ' likewise lives per-instance in pTabBar->nLastHotIdx.
    dim m as PSTABBAR_MESSAGEINFO

    select case uMsg
        ' Prepare the callback MessageInfo for only the messages that we hand to the host.
        case WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, _
             WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MOUSEWHEEL
            with m
                .hTabBar   = hwnd
                .uMsg       = uMsg
                .wParam     = wParam
                .lParam     = lParam
                .idx        = -1
            end with
    end select

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        PsTabBar_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       PsTabBar_OnPaint)

    ' The SendMessage doors for a separate nav control. wParam is -1/+1; anything else
    ' is answered without moving. (wParam arrives zero-extended, so cast via LONG_PTR
    ' to get the sign back before treating it as a direction.)
    case TCM_NAVIGATE
        return PsTabBar_Navigate( hwnd, cast(integer, cast(LONG_PTR, wParam)) )

    case TCM_CANNAVIGATE
        return iif( PsTabBar_CanNavigate( hwnd, cast(integer, cast(LONG_PTR, wParam)) ), 1, 0 )

    case WM_TIMER
        if (pTabBar <> 0) andalso (wParam = IDT_CTABBAR_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no
            ' longer over our client area, clear the hot tab ourselves. Never while
            ' dragging: hover is pinned to the dragged tab, and with capture held the
            ' cursor legitimately roams outside the client mid-gesture.
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if (pTabBar->bDragging = false) andalso (isMouseOverRECT( hwnd, rcClient ) = false) then
                PsTabBar_ClearHotTracking( pTabBar, hwnd )
            end if
            return 0
        end if

    case WM_SETCURSOR
        ' A drag-reorder is in progress: show the four-headed move cursor for the whole
        ' gesture. Under capture the hit-test code in loword(lParam) can be anything (the
        ' cursor legitimately roams off the control mid-drag), so it is deliberately not
        ' tested here -- while bDragging we own the cursor unconditionally, and while we
        ' are not dragging we claim nothing and DefWindowProc sets the class cursor.
        ' Not offered to MessageCallback: the host's message list is unchanged by this.
        if pTabBar then
            if pTabBar->bDragging andalso (pTabBar->hDragCursor <> 0) then
                SetCursor( pTabBar->hDragCursor )
                return true
            end if
        end if

    case WM_MOUSEMOVE
        ' Track that we are over the control in order to catch the eventual
        ' WM_MOUSEHOVER and WM_MOUSELEAVE events.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 250    ' HOVER_DEFAULT is 500
        if pTabBar then tme.dwHoverTime = pTabBar->HoverTime
        TrackMouseEvent(@tme)

        ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
        ' quickly (or onto another control), which would strand the hot tab painted.
        ' Poll the cursor while we are over the control.
        if (pTabBar <> 0) andalso (pTabBar->hotTimerOn = false) then
            SetTimer( hwnd, IDT_CTABBAR_HOTTRACK, PSTABBAR_HOTTRACK_MS, NULL )
            pTabBar->hotTimerOn = true
        end if

        ' Repaint only the tabs whose state changed, not the whole bar.
        if pTabBar then
            dim as integer idx = GetHotTabBarTab(hwnd)
            ' Hover tracking is SUSPENDED while dragging: hover stays pinned to the
            ' dragged tab (MoveTab keeps nLastHotIdx following it), so the dragged tab
            ' keeps its hot state -- and its close "X" -- for the whole gesture, and a
            ' tab being dragged over can never light up or grow an X of its own.
            ' Normal tracking resumes on the first move after the drop.
            if pTabBar->bDragging = false then
                ' Is the cursor over the hot tab's close "X"? A tab is both isHot and
                ' isHotClose then -- the X is inside the tab.
                dim as boolean hotClose = false
                if idx <> -1 then hotClose = isMouseOverRECT( hwnd, pTabBar->tabs(idx).rcClose )
                if idx <> pTabBar->nLastHotIdx then
                    PsTabBar_InvalidateTab( pTabBar, idx )                   ' newly hot
                    PsTabBar_InvalidateTab( pTabBar, pTabBar->nLastHotIdx )  ' previously hot
                    pTabBar->nLastHotIdx = idx
                    pTabBar->bLastHotClose = hotClose
                    ' Tab changed: pop the current tip so it re-queries TTN_GETDISPINFOW
                    ' for the new tab on the next hover.
                    if pTabBar->hTooltip then SendMessage( pTabBar->hTooltip, TTM_POP, 0, 0 )
                elseif hotClose <> pTabBar->bLastHotClose then
                    ' Same tab, but the cursor crossed the close rect's edge: the X's
                    ' rendering changes, so repaint just this tab.
                    pTabBar->bLastHotClose = hotClose
                    PsTabBar_InvalidateTab( pTabBar, idx )
                end if
            end if

            ' --------------------------------------------------------------------------
            ' Drag-to-reorder. A live left press (not on the "X" -- the X is a click
            ' target, not a handle) that travels past the system drag threshold becomes
            ' a drag. Reordering is LIVE: the moment the pointer crosses INTO an
            ' adjacent tab, the tabs exchange immediately -- the tabs visibly changing
            ' places ARE the drag feedback. The exchange is gated on the DIRECTION OF
            ' TRAVEL, which is what prevents oscillation when a narrow tab is dragged
            ' over a wide one: after that exchange the pointer still sits over the wide
            ' tab (now on the other side of the dragged one), and without the gate the
            ' next pixel of movement would exchange them straight back.
            ' --------------------------------------------------------------------------
            if (pTabBar->nPressedIdx <> -1) andalso (pTabBar->bPressedClose = false) then
                dim as integer ptX = cast( short, loword(lParam) )
                dim as integer ptY = cast( short, hiword(lParam) )
                if pTabBar->bDragging = false then
                    if (abs(ptX - pTabBar->ptPress.x) >= GetSystemMetrics(SM_CXDRAG)) orelse _
                       (abs(ptY - pTabBar->ptPress.y) >= GetSystemMetrics(SM_CYDRAG)) then
                        pTabBar->bDragging = true
                        ' Pin hover to the dragged tab for the whole gesture (tracking
                        ' above is suspended while bDragging): its X stays displayed,
                        ' and no dragged-over tab can become hot.
                        pTabBar->nLastHotIdx = pTabBar->nPressedIdx
                        pTabBar->bLastHotClose = false
                        pTabBar->nLastDragX = ptX   ' seed the direction-of-travel gate
                        PsTabBar_InvalidateTab( pTabBar, pTabBar->nPressedIdx )   ' isDragging visual on
                        ' Set the move cursor here rather than waiting for WM_SETCURSOR:
                        ' that message is sent BEFORE the WM_MOUSEMOVE that starts the
                        ' drag, so leaving it to the handler above would cost one move of
                        ' visible lag on every gesture.
                        if pTabBar->hDragCursor then SetCursor( pTabBar->hDragCursor )
                    end if
                end if
                if pTabBar->bDragging andalso (idx <> -1) andalso (idx <> pTabBar->nPressedIdx) then
                    ' Exchange on entering the adjacent tab, gated on travelling toward it.
                    dim as boolean doMove
                    if idx > pTabBar->nPressedIdx then
                        doMove = (ptX > pTabBar->nLastDragX)    ' over a tab to the right, still moving right
                    else
                        doMove = (ptX < pTabBar->nLastDragX)    ' over a tab to the left, still moving left
                    end if
                    if doMove then
                        dim as integer idxFrom = pTabBar->nPressedIdx
                        pTabBar->MoveTab( idxFrom, idx )   ' remaps nCurSel / nLastHotIdx itself
                        pTabBar->nPressedIdx = idx
                        ' USER-driven move: the one path that fires the reorder callback
                        ' (programmatic PsTabBar_MoveTab stays silent). State already updated.
                        if pTabBar->ReorderCallback then pTabBar->ReorderCallback( hwnd, idxFrom, idx )
                    end if
                end if
                if pTabBar->bDragging then pTabBar->nLastDragX = ptX
            end if

            m.idx = idx
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEHOVER
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pTabBar then
            ' Fast path for clearing the hot tab (the WM_TIMER poll is the fallback for
            ' when this message never arrives). The tooltip hides itself when the mouse
            ' leaves the tool, so nothing to push. m.idx stays -1: the mouse is gone.
            ' Never while dragging -- hover is pinned to the dragged tab until the drop.
            if pTabBar->bDragging = false then
                PsTabBar_ClearHotTracking( pTabBar, hwnd )
            end if
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
        end if

    ' ------------------------------------------------------------------------------------
    ' The press machinery. The control TAKES capture on a press (the PsStatusBar decision
    ' reversed, deliberately -- see PsTabBar.bi): capture is what makes "press the X,
    ' slide off, release" a reliable cancel. It pays PsVScrollBar's full price:
    '   - capture taken BEFORE the host callback can suppress anything;
    '   - released in the up-handler unconditionally, callback result IGNORED there;
    '   - WM_CAPTURECHANGED cancels the press;
    '   - WM_DESTROY releases if we still hold it.
    ' No SetFocus anywhere: a tab must not steal focus from the host's editor.
    ' ------------------------------------------------------------------------------------
    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        ' WM_LBUTTONDBLCLK REPLACES the second WM_LBUTTONDOWN (CS_DBLCLKS) but is still
        ' followed by its own WM_LBUTTONUP, so it must run the same press bookkeeping or
        ' that up would release a capture never taken.
        SetCapture( hwnd )
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            ' Host observer first: TRUE suppresses selection and the press -- but never
            ' the capture bookkeeping, whose release is already guaranteed above.
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
            if m.idx <> -1 then
                pTabBar->nPressedIdx   = m.idx
                pTabBar->bPressedClose = isMouseOverRECT( hwnd, pTabBar->tabs(m.idx).rcClose )
                ' Remember where the press landed: the drag threshold measures from here.
                pTabBar->ptPress.x = cast( short, loword(lParam) )
                pTabBar->ptPress.y = cast( short, hiword(lParam) )
                ' Select on DOWN (what real tab controls do) -- unless the press landed
                ' on the close "X": closing must not first activate the closing tab.
                if (pTabBar->bPressedClose = false) andalso (m.idx <> pTabBar->nCurSel) then
                    dim as integer idxOld = pTabBar->nCurSel
                    pTabBar->nCurSel = m.idx
                    ' A partially clipped rightmost tab scrolls into view on selection.
                    pTabBar->bEnsureVisible = true
                    pTabBar->Refresh()
                    ' USER-driven change: this is the one path that fires the callback
                    ' (programmatic SetCurSel stays silent). State is already updated.
                    if pTabBar->SelChangeCallback then pTabBar->SelChangeCallback( hwnd, idxOld, m.idx )
                end if
            end if
        end if

    case WM_LBUTTONUP
        ' Save-then-clear the press BEFORE releasing: WM_CAPTURECHANGED arrives during
        ' ReleaseCapture and must find nothing to cancel.
        dim as integer pressedIdx = -1
        dim as boolean pressedClose = false
        if pTabBar then
            pressedIdx   = pTabBar->nPressedIdx
            pressedClose = pTabBar->bPressedClose
            pTabBar->nPressedIdx   = -1
            pTabBar->bPressedClose = false
            if pTabBar->bDragging then
                ' The drop itself: the array is already in its final order (live
                ' reordering), so ending the drag is just turning the visual off.
                pTabBar->bDragging = false
                PsTabBar_InvalidateTab( pTabBar, pressedIdx )
                PsTabBar_RestoreCursor( hwnd )
            end if
        end if
        if GetCapture() = hwnd then ReleaseCapture()
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            ' Result IGNORED: this message just released the capture, and a callback
            ' must never be able to strand that (Learnings.md, the PsListBox bug).
            if pTabBar->MessageCallback then pTabBar->MessageCallback( @m )
            ' Close fires only on a MATCHED gesture: pressed this tab's X, and released
            ' still on this tab's X. Capture is what routed the outside-release here as
            ' idx = -1, making slide-off a reliable cancel.
            if pressedClose andalso (pressedIdx <> -1) andalso (pressedIdx = m.idx) then
                if isMouseOverRECT( hwnd, pTabBar->tabs(m.idx).rcClose ) then
                    if pTabBar->CloseCallback then pTabBar->CloseCallback( hwnd, m.idx )
                end if
            end if
        end if

    case WM_MBUTTONDOWN
        ' Middle-click closes a tab (editor convention): same capture shape as the left
        ' press, same matched down+up rule -- just anywhere on the tab, not only the X.
        SetCapture( hwnd )
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
            pTabBar->nPressedMidIdx = m.idx
        end if

    case WM_MBUTTONUP
        dim as integer pressedMid = -1
        if pTabBar then
            pressedMid = pTabBar->nPressedMidIdx
            pTabBar->nPressedMidIdx = -1
        end if
        if GetCapture() = hwnd then ReleaseCapture()
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            if pTabBar->MessageCallback then pTabBar->MessageCallback( @m )   ' result IGNORED
            if (pressedMid <> -1) andalso (pressedMid = m.idx) then
                if pTabBar->CloseCallback then pTabBar->CloseCallback( hwnd, m.idx )
            end if
        end if

    case WM_RBUTTONDOWN
        ' Observer only: no press semantics, no capture.
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEWHEEL
        ' Wheel steps the scroll position through the same gate as the nav buttons.
        ' Reaches us on Win10+ via the scroll-under-cursor setting (on by default);
        ' a host can also forward wheel messages here explicitly.
        if pTabBar then
            m.idx = GetHotTabBarTab(hwnd)
            if pTabBar->MessageCallback then
                if pTabBar->MessageCallback( @m ) then return 0   ' host suppressed the scroll
            end if
            dim as integer delta = cast( short, HIWORD(wParam) )
            PsTabBar_Navigate( hwnd, iif( delta > 0, -1, 1 ) )    ' wheel up = left
            return 0
        end if

    case WM_CAPTURECHANGED
        ' Someone stole the capture mid-press (Alt-Tab, a menu, another SetCapture).
        ' The gesture is dead: cancel the press so no click can fire from its corpse.
        ' A drag ends here too, WITHOUT restoring the original order: reordering is
        ' live, so every intermediate order is already a legitimate, notified state.
        if pTabBar then
            dim as integer wasDragIdx = iif( pTabBar->bDragging, pTabBar->nPressedIdx, -1 )
            pTabBar->nPressedIdx    = -1
            pTabBar->bPressedClose  = false
            pTabBar->nPressedMidIdx = -1
            pTabBar->bDragging      = false
            PsTabBar_InvalidateTab( pTabBar, wasDragIdx )   ' no-op for -1
            if wasDragIdx <> -1 then PsTabBar_RestoreCursor( hwnd )
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot tab's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pTabBar then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' store in an instance-lifetime buffer and point lpszText at it
                pTabBar->wszTooltip = PsTabBar_ResolveTooltipText( pTabBar, pTabBar->nLastHotIdx )
                pdi->lpszText = cast( LPWSTR, pTabBar->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        ' Don't let the hot-tracking timer -- or a live mouse capture -- outlive the window.
        if (pTabBar <> 0) andalso pTabBar->hotTimerOn then
            KillTimer( hwnd, IDT_CTABBAR_HOTTRACK )
            pTabBar->hotTimerOn = false
        end if
        if GetCapture() = hwnd then ReleaseCapture()

    case WM_NCDESTROY
        ' Delete the stored tab data pointer
        if pTabBar then
            if IsWindow(pTabBar->hToolTip) then DestroyWindow(pTabBar->hToolTip)
            delete(pTabBar)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Create a tooltip that pulls its text on demand (LPSTR_TEXTCALLBACK) for the whole
' control -- one tool covering the entire bar, not one per tab, because the tabs are
' painted rects rather than windows. TTN_GETDISPINFOW therefore goes to TOOLINFO.hwnd,
' which is the control itself (NOT the parent form), and PsTabBar_WndProc answers it
' using the current hot tab.
' ========================================================================================
private function PsTabBar_AddTooltip( byval hCtrl as HWND ) as HWND
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
    tti.uFlags   = TTF_IDISHWND or TTF_SUBCLASS
    tti.hwnd     = hCtrl
    tti.uId      = cast(UINT_PTR, hCtrl)
    tti.hinst    = GetModuleHandleW(NULL)
    tti.lpszText = LPSTR_TEXTCALLBACK
    SendMessageW( hTooltip, TTM_ADDTOOLW, 0, cast(LPARAM, @tti) )
    return hTooltip
end function


' ========================================================================================
' PsTabBar_Create
' ========================================================================================
function PsTabBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsTabBar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' Enable double-click messages (WM_LBUTTONDBLCLK) on the control's window class,
    ' preserving the class's existing styles rather than replacing them. (CWindow's
    ' ClassStyle setter REPLACES, so this must be read-modify-write, and it needs
    ' m_hwnd -- i.e. it only works after Create.)
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    ' Define the TYPE that holds all the information about this custom tab bar.
    dim as PSTABBAR ptr pTabBar = new PSTABBAR
    pTabBar->hWin = hCtrl
    pTabBar->idc_TabBar = CtrlID
    pTabBar->nPadding    = pWindow->ScaleX( PSTABBAR_DEFAULT_PADDING )
    pTabBar->nIconWidth  = pWindow->ScaleX( PSTABBAR_DEFAULT_ICONWIDTH )
    pTabBar->nBorderWidth = pWindow->ScaleX( 1 )
    if pTabBar->nBorderWidth < 1 then pTabBar->nBorderWidth = 1
    ' Shared system cursor: loaded once, never destroyed (DestroyCursor is only for
    ' cursors the caller created).
    pTabBar->hDragCursor = LoadCursor( null, IDC_SIZEALL )
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the tab window
    pWindow->UserData(0) = cast(LONG_PTR, pTabBar)
    
    pTabBar->hTooltip = PsTabBar_AddTooltip( hCtrl )

    function = hCtrl
    
end function

