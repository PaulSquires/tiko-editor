
''
''  PsPopupMenu.inc
''

#include once "PsPopupMenu.bi"

' The single open chain's root popup (0 = no chain open). Control-internal shared
' state, not a host global: one popup chain at a time is a documented invariant (real
' menus share it), and this is what the message filter and the foreground watch key on.
dim shared as HWND gPM_OpenRoot


private function GetPopupMenuPointer( byval hPopup as HWND ) as PSPOPUPMENU ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hPopup)
    dim as PSPOPUPMENU ptr pPopup = null
    if pWindow then pPopup = cast( PSPOPUPMENU ptr, pWindow->UserData(0) )
    return pPopup
end function

' Forward declarations for the private chain machinery (the routines are mutually
' recursive: hiding a level hides its children first).
declare sub PM_HideLevel( byval hPopup as HWND )
declare sub PM_CloseChain( byval hAny as HWND, byval nReason as long, byval bNotify as boolean )
declare sub PM_OpenSubMenu( byval hParentPopup as HWND, byval idx as long, byval bSelectFirst as boolean )
declare function PM_ShowCore( byval hPopup as HWND, byval nAlign as long, byval rcAnchor as RECT ) as boolean


' ========================================================================================
' PSPOPUPMENU member procedures
' ========================================================================================

' Measure every row and assign its rect plus the ideal window size. This is the only
' place popup geometry is produced; everything else (hit-testing, painting, showing,
' invalidation) consumes it. Unlike a child control, a popup is sized TO its content,
' so layout does not depend on the client rect -- only on the items and the fonts.
sub PSPOPUPMENU.LayoutItems()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next consume
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as long maxCaptionW = 0
    dim as long maxAccelW = 0
    dim as boolean bHasAccel = false
    for i as long = 0 to this.itemCount - 1
        if this.items(i).isSeparator then continue for
        dim as SIZE sz
        dim as long nChars = len( this.items(i).wszCaption )
        if nChars > 0 then
            if GetTextExtentPoint32W( hDC, this.items(i).wszCaption.vptr, nChars, @sz ) then
                if sz.cx > maxCaptionW then maxCaptionW = sz.cx
            end if
        end if
        nChars = len( this.items(i).wszAccel )
        if nChars > 0 then
            bHasAccel = true
            if GetTextExtentPoint32W( hDC, this.items(i).wszAccel.vptr, nChars, @sz ) then
                if sz.cx > maxAccelW then maxAccelW = sz.cx
            end if
        end if
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' The accel block is only spent when some row actually shows an accelerator; a
    ' menu with none (a context menu) doesn't carry the gap around for nothing.
    dim as long accelBlock = 0
    if bHasAccel then accelBlock = this.nAccelGap + maxAccelW

    dim as long contentW = this.nCheckColWidth + maxCaptionW + accelBlock + this.nChevronColWidth
    if contentW < this.nMinWidth then contentW = this.nMinWidth

    ' Rows are contiguous and inset by the border and the window padding, but they are
    ' NOT all the same height: a separator gets nSeparatorHeight, everything else
    ' nItemHeight. Hence the running y -- the old "y + i * nItemHeight" stride assumed a
    ' uniform row and is exactly what could not survive short separators. Nothing else in
    ' the control needed changing for this: hit-testing, painting, invalidation and
    ' submenu anchoring all read these rects rather than recomputing the stride, and
    ' keyboard navigation skips separators by FLAG (NextSelectable), not by geometry.
    dim as long x = this.nBorderWidth + this.nHPadding
    dim as long y = this.nBorderWidth + this.nVPadding
    for i as long = 0 to this.itemCount - 1
        dim as long rowH = this.nItemHeight
        if this.items(i).isSeparator then rowH = this.nSeparatorHeight
        SetRect( @this.items(i).rc, x, y, x + contentW, y + rowH )
        y += rowH
    next

    this.nIdealW = contentW + (this.nHPadding * 2) + (this.nBorderWidth * 2)
    ' y has walked to the bottom of the last row, so it already carries the leading
    ' border+padding and every row height; only the trailing pair is left to add.
    this.nIdealH = y + this.nVPadding + this.nBorderWidth
end sub

' Which row is the given CLIENT point over? -1 for none (window padding / border).
function PSPOPUPMENU.HitTest( byval x as long, byval y as long ) as long
    dim as POINT pt = ( x, y )
    for i as long = 0 to this.itemCount - 1
        if PtInRect( @this.items(i).rc, pt ) then return i
    next
    return -1
end function

' The next selectable row from nFrom in direction nDir (+1/-1), skipping separators
' and disabled rows, wrapping at either end. Pass nFrom = -1 with nDir = +1 for "first
' selectable", nFrom = itemCount with nDir = -1 for "last". Returns -1 when the menu
' has no selectable row at all.
function PSPOPUPMENU.NextSelectable( byval nFrom as long, byval nDir as long ) as long
    if this.itemCount = 0 then return -1
    if (nDir <> 1) andalso (nDir <> -1) then return -1
    dim as long idx = nFrom
    for k as long = 1 to this.itemCount
        idx += nDir
        if idx < 0 then idx = this.itemCount - 1
        if idx > this.itemCount - 1 then idx = 0
        dim as boolean bSelectable = _
            (this.items(idx).isSeparator = false) andalso (this.items(idx).isDisabled = false)
        if bSelectable then return idx
    next
    return -1
end function

' Find a command id in THIS menu only (the deep search lives in PM_FindItemDeep).
' id 0 is reserved for separators and never matches.
function PSPOPUPMENU.FindItem( byval id as long ) as long
    if id = 0 then return -1
    for i as long = 0 to this.itemCount - 1
        if this.items(i).id = id then return i
    next
    return -1
end function

function PSPOPUPMENU.IsValidItem( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.itemCount)
end function

' Mark the layout stale and request a repaint. Every mutator routes through here, which
' is what makes layout lazy: a burst of AddItem calls (an InitPopup rebuilding an MRU
' menu) costs one measuring pass, not N.
sub PSPOPUPMENU.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' Private helpers
' ========================================================================================

' Append a fresh (reset) row, growing the backing store by doubling so a bulk rebuild
' is amortized O(1). Returns the new row's index.
private function PM_AppendItem( byval pThis as PSPOPUPMENU ptr ) as long
    dim as long cap = ubound(pThis->items) + 1
    if pThis->itemCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve pThis->items( 0 to newcap - 1 )
    end if
    dim as long idx = pThis->itemCount
    ' Reset the slot (frees any DWSTRING left in a recycled capacity slot). hSubMenu in
    ' a recycled slot is always 0 already: Clear/DeleteItem destroy and null it.
    with pThis->items(idx)
        .id          = 0
        .wszCaption  = ""
        .wszAccel    = ""
        .isSeparator = false
        .isDisabled  = false
        .isChecked   = false
        .hSubMenu    = 0
        SetRectEmpty( @.rc )
    end with
    pThis->itemCount += 1
    return idx
end function

' Find a command id anywhere in the menu TREE rooted at hPopup (this level first, then
' each attached submenu depth-first; first match wins). Returns the row index and the
' level that owns it through hLevelOut, or -1 / 0.
private function PM_FindItemDeep( byval hPopup as HWND, byval id as long, byref hLevelOut as HWND ) as long
    hLevelOut = 0
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return -1
    dim as long idx = pThis->FindItem( id )
    if idx <> -1 then
        hLevelOut = hPopup
        return idx
    end if
    for i as long = 0 to pThis->itemCount - 1
        if pThis->items(i).hSubMenu then
            idx = PM_FindItemDeep( pThis->items(i).hSubMenu, id, hLevelOut )
            if idx <> -1 then return idx
        end if
    next
    return -1
end function

' Repaint one row. Silently does nothing for an invalid index. FALSE = no background
' erase; the row's rect is fully painted by the paint pass.
private sub PM_InvalidateItem( byval pThis as PSPOPUPMENU ptr, byval idx as long )
    if pThis = null then exit sub
    if pThis->IsValidItem(idx) = false then exit sub
    InvalidateRect( pThis->hWin, @pThis->items(idx).rc, FALSE )
end sub

' The chain root that hAny belongs to (hAny itself when it is a root or standalone).
private function PM_ResolveRootHwnd( byval hAny as HWND ) as HWND
    dim as HWND h = hAny
    for k as long = 1 to 32               ' cycle guard; real menus are a few levels deep
        dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(h)
        if pThis = null then return 0
        if pThis->hParentLevel = 0 then return h
        h = pThis->hParentLevel
    next
    return 0
end function

' The deepest OPEN level of the chain rooted at hRoot (hRoot itself when no submenu
' is open). Follows the hOpenChild links.
private function PM_InnermostHwnd( byval hRoot as HWND ) as HWND
    dim as HWND h = hRoot
    for k as long = 1 to 32
        dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(h)
        if pThis = null then return h
        if pThis->hOpenChild = 0 then return h
        h = pThis->hOpenChild
    next
    return h
end function

' Is hTest one of the OPEN chain's windows? Walks our own chain links only -- it never
' dereferences state on a foreign window, so hTest can be any HWND in the system.
private function PM_IsChainWindow( byval hTest as HWND ) as boolean
    if hTest = 0 then return false
    if gPM_OpenRoot = 0 then return false
    dim as HWND h = gPM_OpenRoot
    for k as long = 1 to 32
        if h = hTest then return true
        dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(h)
        if pThis = null then return false
        h = pThis->hOpenChild
        if h = 0 then return false
    next
    return false
end function

' Partition a row rect into its three cells: [check][caption+accel][chevron]. The one
' place the cell formulas live -- the paint pass hands these to the callback so it
' never re-derives insets.
private sub PM_ComputeCellRects( _
            byval pThis as PSPOPUPMENU ptr, _
            byval rcRow as RECT, _
            byref rcCheck as RECT, _
            byref rcText as RECT, _
            byref rcChevron as RECT _
            )
    SetRect( @rcCheck, rcRow.left, rcRow.top, rcRow.left + pThis->nCheckColWidth, rcRow.bottom )
    SetRect( @rcChevron, rcRow.right - pThis->nChevronColWidth, rcRow.top, rcRow.right, rcRow.bottom )
    SetRect( @rcText, rcCheck.right, rcRow.top, rcChevron.left, rcRow.bottom )
end sub

' Hide one level: open descendants first (innermost-outward), then this window, then
' unlink it from its parent. Hiding the chain root also stops the poll timer and
' clears gPM_OpenRoot. SILENT -- notifications are the callers' decision.
private sub PM_HideLevel( byval hPopup as HWND )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub

    if pThis->hOpenChild then PM_HideLevel( pThis->hOpenChild )
    pThis->hOpenChild = 0

    ShowWindow( hPopup, SW_HIDE )
    pThis->nCurSel = -1
    pThis->bHoverSel = false

    if pThis->hParentLevel then
        dim as PSPOPUPMENU ptr pParent = GetPopupMenuPointer( pThis->hParentLevel )
        if pParent then
            if pParent->hOpenChild = hPopup then pParent->hOpenChild = 0
        end if
        pThis->hParentLevel = 0
        pThis->nParentItem = -1
    end if

    if gPM_OpenRoot = hPopup then
        gPM_OpenRoot = 0
        if pThis->pollTimerOn then
            KillTimer( hPopup, IDT_CPOPUPMENU_POLL )
            pThis->pollTimerOn = false
        end if
    end if
end sub

' Close the whole chain containing hAny. bNotify = true sends CPM_NOTIFY_CLOSED to the
' root's notify window (user-driven dismissals only; programmatic closes stay silent).
' nReason: 1 = closed by Escape at the root, 0 = everything else.
private sub PM_CloseChain( byval hAny as HWND, byval nReason as long, byval bNotify as boolean )
    dim as HWND hRoot = PM_ResolveRootHwnd( hAny )
    if hRoot = 0 then exit sub
    dim as PSPOPUPMENU ptr pRoot = GetPopupMenuPointer( hRoot )
    dim as HWND hNotify = 0
    if pRoot then hNotify = pRoot->hNotifyWnd
    PM_HideLevel( hRoot )
    if bNotify then
        if hNotify then SendMessage( hNotify, CPM_NOTIFY_CLOSED, cast(WPARAM, nReason), cast(LPARAM, hRoot) )
    end if
end sub

' Execute a command row: capture the id, close the WHOLE chain first, then tell the
' host (notify window message, then the root's select callback). Closed-before-dispatch
' is the contract -- a handler can open a dialog without fighting a live popup.
private sub PM_ExecuteItem( byval hLevel as HWND, byval idx as long )
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then exit sub
    if pLevel->IsValidItem(idx) = false then exit sub
    dim as boolean bExecutable = _
        (pLevel->items(idx).isSeparator = false) andalso _
        (pLevel->items(idx).isDisabled = false) andalso _
        (pLevel->items(idx).hSubMenu = 0)
    if bExecutable = false then exit sub

    dim as long id = pLevel->items(idx).id
    dim as HWND hRoot = PM_ResolveRootHwnd( hLevel )
    if hRoot = 0 then hRoot = hLevel
    dim as PSPOPUPMENU ptr pRoot = GetPopupMenuPointer( hRoot )
    dim as HWND hNotify = 0
    dim as PM_SelectCallbackSub cbSelect = 0
    if pRoot then
        hNotify = pRoot->hNotifyWnd
        cbSelect = pRoot->SelectCallback
    end if

    PM_HideLevel( hRoot )

    if hNotify then SendMessage( hNotify, CPM_NOTIFY_COMMAND, cast(WPARAM, id), cast(LPARAM, hRoot) )
    if cbSelect then cbSelect( hRoot, id )
end sub

' Open the submenu attached to a parent level's row: link the two levels, pin the
' parent's selection to the row (the leave/poll clear paths stand down for a pinned
' parent), and show against the row's right edge.
private sub PM_OpenSubMenu( byval hParentPopup as HWND, byval idx as long, byval bSelectFirst as boolean )
    dim as PSPOPUPMENU ptr pParent = GetPopupMenuPointer(hParentPopup)
    if pParent = null then exit sub
    if pParent->IsValidItem(idx) = false then exit sub
    dim as HWND hSub = pParent->items(idx).hSubMenu
    dim as PSPOPUPMENU ptr pSub = GetPopupMenuPointer(hSub)
    if pSub = null then exit sub
    if pParent->items(idx).isDisabled then exit sub

    if pParent->bLayoutDirty then pParent->LayoutItems()

    pSub->hParentLevel = hParentPopup
    pSub->nParentItem = idx
    pParent->hOpenChild = hSub
    if pParent->nCurSel <> idx then
        PM_InvalidateItem( pParent, pParent->nCurSel )
        PM_InvalidateItem( pParent, idx )
        pParent->nCurSel = idx
    end if
    pParent->bHoverSel = false      ' the pin is not hover-owned

    dim as RECT rcAnchor = pParent->items(idx).rc
    MapWindowPoints( hParentPopup, HWND_DESKTOP, cast(POINT ptr, @rcAnchor), 2 )
    PM_ShowCore( hSub, PM_ALIGN_RIGHT, rcAnchor )

    if bSelectFirst then
        dim as long nFirst = pSub->NextSelectable( -1, 1 )
        if pSub->nCurSel <> nFirst then
            PM_InvalidateItem( pSub, pSub->nCurSel )
            PM_InvalidateItem( pSub, nFirst )
            pSub->nCurSel = nFirst
        end if
    end if
end sub

' The one show path. Fires the popup's InitPopup callback (mutations there are
' coalesced into the layout pass that follows), sizes to the measured content, clamps
' to the work area of the nearest monitor, and shows WITHOUT activation. A root show
' closes any other open chain first (a silent internal switch), then claims
' gPM_OpenRoot and starts the safety-net poll timer.
private function PM_ShowCore( byval hPopup as HWND, byval nAlign as long, byval rcAnchor as RECT ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false

    if pThis->hParentLevel = 0 then
        if (gPM_OpenRoot <> 0) andalso (gPM_OpenRoot <> hPopup) then
            PM_CloseChain( gPM_OpenRoot, 0, false )
        end if
    end if

    if pThis->InitPopupCallback then pThis->InitPopupCallback( hPopup )
    if pThis->bLayoutDirty then pThis->LayoutItems()
    if pThis->bLayoutDirty then return false      ' could not measure (no DC)

    dim as long w = pThis->nIdealW
    dim as long h = pThis->nIdealH
    dim as long x, y
    select case nAlign
        case PM_ALIGN_BELOW
            x = rcAnchor.left
            y = rcAnchor.bottom - 1     ' merge the popup border with the bar's edge
        case PM_ALIGN_RIGHT
            x = rcAnchor.right
            y = rcAnchor.top
        case else                       ' show-at-point packs the point into rcAnchor
            x = rcAnchor.left
            y = rcAnchor.top
    end select

    dim as RECT rcTarget = type( x, y, x + w, y + h )
    dim as MONITORINFO mi
    mi.cbSize = sizeof(MONITORINFO)
    dim as HMONITOR hMon = MonitorFromRect( @rcTarget, MONITOR_DEFAULTTONEAREST )
    if GetMonitorInfo( hMon, @mi ) then
        if x + w > mi.rcWork.right then
            if nAlign = PM_ALIGN_RIGHT then
                x = rcAnchor.left - w   ' submenu flips to the anchor's other side
            else
                x = mi.rcWork.right - w
            end if
        end if
        if x < mi.rcWork.left then x = mi.rcWork.left
        if y + h > mi.rcWork.bottom then
            if nAlign = PM_ALIGN_BELOW then
                ' No room below the anchor: FLIP ABOVE it rather than clamp the popup up
                ' over it. A PM_ALIGN_BELOW popup (a combobox dropdown, a menubar menu)
                ' that opened covering its own anchor button would have the OPENING click's
                ' release land on a row and immediately self-dismiss. Placing the popup
                ' entirely above the button keeps the release on the button.
                dim as long yAbove = rcAnchor.top - h
                if yAbove >= mi.rcWork.top then y = yAbove else y = mi.rcWork.top
            else
                y = mi.rcWork.bottom - h
            end if
        end if
        if y < mi.rcWork.top then y = mi.rcWork.top
    end if

    pThis->nCurSel = -1
    pThis->bHoverSel = false
    pThis->hOpenChild = 0
    SetWindowPos( hPopup, HWND_TOP, x, y, w, h, SWP_NOACTIVATE or SWP_SHOWWINDOW )
    InvalidateRect( hPopup, NULL, TRUE )      ' a same-size re-show still needs a repaint

    if pThis->hParentLevel = 0 then
        gPM_OpenRoot = hPopup
        if pThis->pollTimerOn = false then
            SetTimer( hPopup, IDT_CPOPUPMENU_POLL, PSPOPUPMENU_POLL_MS, NULL )
            pThis->pollTimerOn = true
        end if
    end if
    return true
end function

' Hover net on the innermost level only -- outer levels are pinned to the row whose
' submenu is open, and a pinned highlight must never be polled away. Clears ONLY a
' HOVER-owned selection (bHoverSel): a keyboard-set highlight must survive the poll,
' or keyboard navigation gets wiped ~100ms after every keypress.
private sub PM_PollHoverNet( byval hRoot as HWND )
    dim as HWND hInner = PM_InnermostHwnd( hRoot )
    dim as PSPOPUPMENU ptr pInner = GetPopupMenuPointer( hInner )
    if pInner = null then exit sub
    dim as boolean bCandidate = (pInner->nCurSel <> -1) andalso (pInner->hOpenChild = 0)
    if bCandidate then
        if pInner->bHoverSel then
            dim as RECT rcClient
            GetClientRect( hInner, @rcClient )
            if isMouseOverRECT( hInner, rcClient ) = false then
                PM_InvalidateItem( pInner, pInner->nCurSel )
                pInner->nCurSel = -1
                pInner->bHoverSel = false
            end if
        end if
    end if
end sub

' The safety-net poll (chain root only): close the chain when the foreground moved
' outside the owner+chain set (the popups never activate, so they cannot see focus
' loss themselves), and clear a hover highlight WM_MOUSELEAVE failed to clear.
private sub PM_PollTick( byval hRoot as HWND, byval pRoot as PSPOPUPMENU ptr )
    if gPM_OpenRoot <> hRoot then exit sub

    dim as HWND hFg = GetForegroundWindow()
    dim as HWND hOwnerRoot = GetAncestor( pRoot->hOwner, GA_ROOT )
    dim as boolean bOurs = (hFg = hOwnerRoot)
    if bOurs = false then bOurs = PM_IsChainWindow( hFg )
    if bOurs = false then
        PM_CloseChain( hRoot, 0, true )
        exit sub
    end if

    PM_PollHoverNet( hRoot )
end sub

' The built-in row painter (the default when no paint callback is set). Draws from the
' colors struct and the borrowed fonts; a disabled row never lights up (ForeColorDisabled
' over the NORMAL background).
private sub PM_DefaultPaintItem( byval pThis as PSPOPUPMENU ptr, byval p as PSPOPUPMENU_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b

    if p->isSeparator then
        b->SetBackColor( pThis->colors.BackColor )
        b->PaintRect( @p->rc )
        ' HORIZONTAL inset of the rule, derived from the ITEM height (not the separator
        ' height): it is a left/right margin that should track the menu's overall scale,
        ' and it must not shrink just because the separator row got short. Lands at
        ' tiko's 12px inset for the default 24px item.
        dim as long inset = pThis->nItemHeight \ 2
        dim as long yMid = (p->rc.top + p->rc.bottom) \ 2
        b->SetPenColor( pThis->colors.SeparatorColor )
        b->PaintLine( pThis->nSeparatorThickness, p->rc.left + inset, yMid, _
                      p->rc.right - inset, yMid )
        exit sub
    end if

    dim as COLORREF backclr, foreclr
    if p->isDisabled then
        backclr = pThis->colors.BackColor
        foreclr = pThis->colors.ForeColorDisabled
    elseif p->isHot then
        backclr = pThis->colors.BackColorHot
        foreclr = pThis->colors.ForeColorHot
    else
        backclr = pThis->colors.BackColor
        foreclr = pThis->colors.ForeColor
    end if
    b->SetColors( foreclr, backclr )
    b->PaintRect( @p->rc )

    dim as HFONT hGlyphUse = pThis->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = pThis->hTextFont

    if p->isChecked then
        b->SetFont( hGlyphUse )
        b->PaintText( pThis->wszCheckGlyph, @p->rcCheck, DT_CENTER )
    end if

    b->SetFont( pThis->hTextFont )
    b->PaintText( p->wszCaption, @p->rcText, DT_LEFT )
    if len( p->wszAccel ) then b->PaintText( p->wszAccel, @p->rcText, DT_RIGHT )

    if p->hasSubMenu then
        b->SetFont( hGlyphUse )
        b->PaintText( pThis->wszChevronGlyph, @p->rcChevron, DT_CENTER )
    end if
end sub


' ========================================================================================
' Process WM_PAINT message for window: PsPopupMenu
' ========================================================================================
private function PsPopupMenu_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hwnd)
    if pThis = null then return 0

    ' Consume any pending layout before painting (the lazy-layout consume point).
    if pThis->bLayoutDirty then pThis->LayoutItems()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' Fill the whole window (rows + padding) with the panel background.
    b.SetBackColor( pThis->colors.BackColor )
    b.PaintClientRect()

    ' Rows: the host's per-item callback when set, else the built-in painter.
    dim p as PSPOPUPMENU_PAINTINFO
    p.hPopup = hwnd
    p.b = @b
    for i as long = 0 to pThis->itemCount - 1
        p.itemID      = i
        p.id          = pThis->items(i).id
        p.rc          = pThis->items(i).rc
        PM_ComputeCellRects( pThis, p.rc, p.rcCheck, p.rcText, p.rcChevron )
        p.isSeparator = pThis->items(i).isSeparator
        p.isDisabled  = pThis->items(i).isDisabled
        p.isChecked   = pThis->items(i).isChecked
        p.hasSubMenu  = (pThis->items(i).hSubMenu <> 0)
        p.isHot       = (i = pThis->nCurSel)
        p.wszCaption  = pThis->items(i).wszCaption
        p.wszAccel    = pThis->items(i).wszAccel
        if pThis->PaintItemCallback then
            pThis->PaintItemCallback( @p )
        else
            PM_DefaultPaintItem( pThis, @p )
        end if
    next

    ' Control-drawn chrome AFTER the rows, so it is authoritative no matter what a
    ' callback painted: the 1px window border, as four filled strips.
    if pThis->nBorderWidth > 0 then
        dim as long bw = pThis->nBorderWidth
        dim as RECT rcC
        GetClientRect( hwnd, @rcC )
        dim as RECT rcStrip
        b.SetBackColor( pThis->colors.BorderColor )
        SetRect( @rcStrip, rcC.left, rcC.top, rcC.right, rcC.top + bw )
        b.PaintRect( @rcStrip )
        SetRect( @rcStrip, rcC.left, rcC.bottom - bw, rcC.right, rcC.bottom )
        b.PaintRect( @rcStrip )
        SetRect( @rcStrip, rcC.left, rcC.top, rcC.left + bw, rcC.bottom )
        b.PaintRect( @rcStrip )
        SetRect( @rcStrip, rcC.right - bw, rcC.top, rcC.right, rcC.bottom )
        b.PaintRect( @rcStrip )
    end if

    b.EndDoubleBuffer()
    return 0
end function


' ========================================================================================
' PsPopupMenu Window procedure
'
' This control has no child windows: the rows are painted rects, hit-test is a row
' lookup, and every message is handled here.
' ========================================================================================
function PsPopupMenu_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer( hwnd )

    select case uMsg
    case WM_PAINT
        return PsPopupMenu_OnPaint( hwnd )

    ' The popup must never steal activation (and with it, focus) from the host: the
    ' click is still delivered, the host's title bar stays painted active, and the
    ' keyboard keeps flowing to the host, where the message filter intercepts it.
    case WM_MOUSEACTIVATE
        return MA_NOACTIVATE

    case WM_MOUSEMOVE
        if pThis then
            ' Arm hover (submenu open/close delay) and leave tracking. TME_LEAVE is
            ' unreliable on fast exits; the root's poll timer is the safety net.
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hwnd
            tme.dwHoverTime = PSPOPUPMENU_HOVER_MS
            TrackMouseEvent(@tme)

            ' Hover IS selection (menu semantics): move the single highlight, repainting
            ' only the two changed rows. Any mouse movement over the popup makes the
            ' selection HOVER-owned (even when it lands on the row the keyboard had
            ' selected) -- from here the hover-clear paths may clear it again.
            dim as long idx = pThis->HitTest( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if idx <> pThis->nCurSel then
                PM_InvalidateItem( pThis, pThis->nCurSel )
                PM_InvalidateItem( pThis, idx )
                pThis->nCurSel = idx
            end if
            pThis->bHoverSel = true
        end if
        return 0

    case WM_MOUSEHOVER
        ' The cursor rested on a row: open its submenu, or close a stale sibling
        ' submenu when the rested row doesn't have one.
        if pThis then
            dim as long idx = pThis->nCurSel
            if pThis->IsValidItem(idx) then
                dim as HWND hSub = pThis->items(idx).hSubMenu
                dim as boolean bCanOpen = (hSub <> 0) andalso (pThis->items(idx).isDisabled = false)
                if bCanOpen then
                    if pThis->hOpenChild <> hSub then
                        if pThis->hOpenChild then PM_HideLevel( pThis->hOpenChild )
                        PM_OpenSubMenu( hwnd, idx, false )
                    end if
                else
                    if pThis->hOpenChild then PM_HideLevel( pThis->hOpenChild )
                end if
            end if
        end if
        return 0

    case WM_MOUSELEAVE
        if pThis then
            ' Pin rule: while this level has an open submenu, the selection stays on
            ' the submenu's parent row -- crossing into the submenu must never strip
            ' the parent row's highlight. With no child open, a HOVER-owned highlight
            ' clears; a keyboard-set one stays (the cursor was never on it to begin
            ' with, so the mouse leaving says nothing about it).
            if pThis->hOpenChild then
                dim as PSPOPUPMENU ptr pChild = GetPopupMenuPointer( pThis->hOpenChild )
                dim as long nPin = -1
                if pChild then nPin = pChild->nParentItem
                if pThis->nCurSel <> nPin then
                    PM_InvalidateItem( pThis, pThis->nCurSel )
                    PM_InvalidateItem( pThis, nPin )
                    pThis->nCurSel = nPin
                    pThis->bHoverSel = false
                end if
            else
                dim as boolean bClear = (pThis->nCurSel <> -1) andalso (pThis->bHoverSel = true)
                if bClear then
                    PM_InvalidateItem( pThis, pThis->nCurSel )
                    pThis->nCurSel = -1
                    pThis->bHoverSel = false
                end if
            end if
        end if
        return 0

    ' No mouse capture is taken anywhere in this control -- a re-run of the per-control
    ' capture test, not an inherited assumption: selection follows hover, execution is
    ' the up-click over a row (a press that slides off simply releases elsewhere and
    ' nothing fires), and outside-click dismissal belongs to the message filter. So
    ' nothing consumes the guaranteed down->up pairing, and the capture bug class is
    ' gone by construction.
    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        ' A press on a submenu row opens the submenu immediately (hover would open it
        ' after the rest delay anyway). WM_LBUTTONDBLCLK substitutes for the second
        ' down of a fast double-click (CS_DBLCLKS), so it runs the same logic.
        if pThis then
            dim as long idx = pThis->HitTest( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if pThis->IsValidItem(idx) then
                dim as HWND hSub = pThis->items(idx).hSubMenu
                dim as boolean bCanOpen = (hSub <> 0) andalso (pThis->items(idx).isDisabled = false)
                if bCanOpen then
                    if pThis->hOpenChild <> hSub then
                        if pThis->hOpenChild then PM_HideLevel( pThis->hOpenChild )
                        PM_OpenSubMenu( hwnd, idx, false )
                    end if
                end if
            end if
        end if
        return 0

    case WM_LBUTTONUP
        ' Execute on the release over an enabled command row (separators, disabled
        ' rows and submenu parents are guarded inside PM_ExecuteItem).
        if pThis then
            dim as long idx = pThis->HitTest( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if pThis->IsValidItem(idx) then PM_ExecuteItem( hwnd, idx )
        end if
        return 0

    case WM_TIMER
        if (pThis <> 0) andalso (wParam = IDT_CPOPUPMENU_POLL) then
            PM_PollTick( hwnd, pThis )
            return 0
        end if

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer.
        return true

    case WM_DESTROY
        if pThis then
            if pThis->pollTimerOn then
                KillTimer( hwnd, IDT_CPOPUPMENU_POLL )
                pThis->pollTimerOn = false
            end if
            ' Unlink from a live chain so the survivors' links stay truthful.
            if gPM_OpenRoot = hwnd then gPM_OpenRoot = 0
            if pThis->hParentLevel then
                dim as PSPOPUPMENU ptr pParent = GetPopupMenuPointer( pThis->hParentLevel )
                if pParent then
                    if pParent->hOpenChild = hwnd then pParent->hOpenChild = 0
                end if
            end if
            ' No capture to release: none is ever taken (see the press handlers).
        end if

    case WM_NCDESTROY
        if pThis then
            ' Attached submenus are OWNED (ownership transferred at SetItemSubMenu):
            ' destroying a level destroys its subtree, so one DestroyWindow on the
            ' root -- or on the bar above it -- unwinds the whole menu tree.
            for i as long = 0 to pThis->itemCount - 1
                if pThis->items(i).hSubMenu then
                    DestroyWindow( pThis->items(i).hSubMenu )
                    pThis->items(i).hSubMenu = 0
                end if
            next
            delete(pThis)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' PUBLIC API implementation
' ========================================================================================

function PsPopupMenu_Create( byval hOwner as HWND, byval CtrlID as long = 0 ) as HWND
    dim pWindow as CWindow ptr = new CWindow

    ' WS_POPUP owned by the host's window (z-order above it, hidden with it on
    ' minimize). WS_EX_NOACTIVATE + the MA_NOACTIVATE answer keep it from ever taking
    ' activation; WS_EX_TOOLWINDOW keeps it out of the taskbar and Alt-Tab.
    dim as HWND hCtrl = pWindow->Create( hOwner, "", @PsPopupMenu_WndProc, _
            0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE )

    ' Replace the class style like every sibling does (dropping CS_HREDRAW/CS_VREDRAW,
    ' which only add flicker), and add the system drop shadow. CWindow registers a
    ' UNIQUE class per instance, so this cannot leak to other windows; the ClassStyle
    ' setter REPLACES and needs m_hwnd, so it must run after Create and before the
    ' first show. The shadow honors the system drop-shadow setting and degrades to
    ' no-shadow silently.
    pWindow->ClassStyle = CS_DBLCLKS or CS_DROPSHADOW

    dim as PSPOPUPMENU ptr pThis = new PSPOPUPMENU
    pThis->hWin = hCtrl
    pThis->idc_Popup = CtrlID
    pThis->hOwner = hOwner
    pThis->nItemHeight      = pWindow->ScaleY( PSPOPUPMENU_DEFAULT_ITEMHEIGHT )
    pThis->nSeparatorHeight = pWindow->ScaleY( PSPOPUPMENU_DEFAULT_SEPHEIGHT )
    pThis->nCheckColWidth   = pWindow->ScaleX( PSPOPUPMENU_DEFAULT_CHECKCOL )
    pThis->nChevronColWidth = pWindow->ScaleX( PSPOPUPMENU_DEFAULT_CHEVRONCOL )
    pThis->nAccelGap        = pWindow->ScaleX( PSPOPUPMENU_DEFAULT_ACCELGAP )
    pThis->nHPadding        = pWindow->ScaleX( PSPOPUPMENU_DEFAULT_HPADDING )
    pThis->nVPadding        = pWindow->ScaleY( PSPOPUPMENU_DEFAULT_VPADDING )
    pThis->nBorderWidth     = pWindow->ScaleX( 1 )
    if pThis->nBorderWidth < 1 then pThis->nBorderWidth = 1
    ' Plain Unicode defaults that render in any text font; hosts with a glyph font
    ' (Segoe Fluent Icons) override via SetGlyphs + SetFonts.
    pThis->wszCheckGlyph   = wchr(&h2713)
    pThis->wszChevronGlyph = wchr(&h203A)
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    pWindow->UserData(0) = cast(LONG_PTR, pThis)
    return hCtrl
end function


function PsPopupMenu_AddItem( byval hPopup as HWND, byval id as long, byval Caption as DWSTRING, byval Accel as DWSTRING = "", byval bDisabled as boolean = false, byval bChecked as boolean = false ) as long
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return -1
    dim as long idx = PM_AppendItem( pThis )
    with pThis->items(idx)
        .id         = id
        .wszCaption = Caption
        .wszAccel   = Accel
        .isDisabled = bDisabled
        .isChecked  = bChecked
    end with
    pThis->Refresh()
    return idx
end function


function PsPopupMenu_AddSeparator( byval hPopup as HWND ) as long
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return -1
    dim as long idx = PM_AppendItem( pThis )
    pThis->items(idx).isSeparator = true
    pThis->Refresh()
    return idx
end function


function PsPopupMenu_GetItemCount( byval hPopup as HWND ) as long
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return 0
    return pThis->itemCount
end function


' Attach a submenu window to the item with the given command id (searched through the
' tree). OWNERSHIP TRANSFERS: the parent destroys the attached window when it is
' destroyed. Attaching over an existing submenu destroys the old one; hSubMenu = 0
' detaches without destroying (ownership returns to the caller).
function PsPopupMenu_SetItemSubMenu( byval hPopup as HWND, byval id as long, byval hSubMenu as HWND ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    dim as HWND hOld = pLevel->items(idx).hSubMenu
    if (hOld <> 0) andalso (hOld <> hSubMenu) then DestroyWindow( hOld )
    pLevel->items(idx).hSubMenu = hSubMenu
    pLevel->Refresh()
    return true
end function


function PsPopupMenu_GetItemSubMenu( byval hPopup as HWND, byval id as long ) as HWND
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return 0
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return 0
    return pLevel->items(idx).hSubMenu
end function


' Delete the item with the given command id (searched through the tree), destroying
' any submenu window attached to it.
function PsPopupMenu_DeleteItem( byval hPopup as HWND, byval id as long ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    if pLevel->items(idx).hSubMenu then
        DestroyWindow( pLevel->items(idx).hSubMenu )
        pLevel->items(idx).hSubMenu = 0
    end if
    for i as long = idx to pLevel->itemCount - 2
        pLevel->items(i) = pLevel->items(i + 1)
    next
    ' Free the vacated last slot's strings and null its (already copied-down) handle.
    with pLevel->items(pLevel->itemCount - 1)
        .wszCaption = ""
        .wszAccel   = ""
        .hSubMenu   = 0
    end with
    pLevel->itemCount -= 1
    if pLevel->nCurSel >= pLevel->itemCount then pLevel->nCurSel = -1
    pLevel->Refresh()
    return true
end function


' Remove every item from THIS popup (not the tree above it), destroying attached
' submenu windows. The InitPopup rebuild path: Clear + AddItem*.
sub PsPopupMenu_Clear( byval hPopup as HWND )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    for i as long = 0 to pThis->itemCount - 1
        with pThis->items(i)
            if .hSubMenu then
                DestroyWindow( .hSubMenu )
                .hSubMenu = 0
            end if
            .wszCaption = ""
            .wszAccel   = ""
        end with
    next
    pThis->itemCount = 0
    pThis->nCurSel = -1
    pThis->bHoverSel = false
    pThis->hOpenChild = 0
    pThis->Refresh()
end sub


function PsPopupMenu_SetItemEnabled( byval hPopup as HWND, byval id as long, byval bEnabled as boolean ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    pLevel->items(idx).isDisabled = (bEnabled = false)
    PM_InvalidateItem( pLevel, idx )
    return true
end function


function PsPopupMenu_GetItemEnabled( byval hPopup as HWND, byval id as long ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    return (pLevel->items(idx).isDisabled = false)
end function


function PsPopupMenu_SetItemChecked( byval hPopup as HWND, byval id as long, byval bChecked as boolean ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    pLevel->items(idx).isChecked = bChecked
    PM_InvalidateItem( pLevel, idx )
    return true
end function


function PsPopupMenu_GetItemChecked( byval hPopup as HWND, byval id as long ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    return pLevel->items(idx).isChecked
end function


function PsPopupMenu_SetItemText( byval hPopup as HWND, byval id as long, byval Caption as DWSTRING, byval Accel as DWSTRING = "" ) as boolean
    dim as HWND hLevel
    dim as long idx = PM_FindItemDeep( hPopup, id, hLevel )
    if idx = -1 then return false
    dim as PSPOPUPMENU ptr pLevel = GetPopupMenuPointer(hLevel)
    if pLevel = null then return false
    pLevel->items(idx).wszCaption = Caption
    pLevel->items(idx).wszAccel = Accel
    ' Text drives the measured width, so this needs a re-layout, not just a repaint.
    pLevel->Refresh()
    return true
end function


function PsPopupMenu_GetItemRect( byval hPopup as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    SetRectEmpty( @rc )
    if pThis = null then return false
    if pThis->IsValidItem(idx) = false then return false
    if pThis->bLayoutDirty then pThis->LayoutItems()
    rc = pThis->items(idx).rc
    return true
end function


' The inverse of GetItemRect. Forces a pending layout for the same reason the rect query
' does -- a caller asking before the first paint must not get stale (or empty) geometry.
function PsPopupMenu_HitTest( byval hPopup as HWND, byval x as long, byval y as long ) as long
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return -1
    if pThis->bLayoutDirty then pThis->LayoutItems()
    return pThis->HitTest( x, y )
end function


function PsPopupMenu_GetIdealSize( byval hPopup as HWND, byref nWidth as long, byref nHeight as long ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    nWidth = 0
    nHeight = 0
    if pThis = null then return false
    if pThis->bLayoutDirty then pThis->LayoutItems()
    if pThis->bLayoutDirty then return false      ' could not measure
    nWidth = pThis->nIdealW
    nHeight = pThis->nIdealH
    return true
end function


function PsPopupMenu_SetMinWidth( byval hPopup as HWND, byval nMinWidth as long ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false
    if nMinWidth < 0 then nMinWidth = 0
    pThis->nMinWidth = nMinWidth
    pThis->Refresh()
    return true
end function


sub PsPopupMenu_SetItemHeight( byval hPopup as HWND, byval nItemHeight as long )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    if nItemHeight < 1 then nItemHeight = 1
    pThis->nItemHeight = nItemHeight
    pThis->Refresh()
end sub


' Separator rows only. Independent of the item height on purpose: a host running a
' compact item height still wants the SAME thin rule, not a proportionally thinner one.
' Set it equal to the item height for uniform rows (the pre-2026-07-21 look).
sub PsPopupMenu_SetSeparatorHeight( byval hPopup as HWND, byval nSeparatorHeight as long )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    if nSeparatorHeight < 1 then nSeparatorHeight = 1
    pThis->nSeparatorHeight = nSeparatorHeight
    pThis->Refresh()
end sub


' The rule's weight in pixels, independent of the row height it sits in. Refresh (not a
' bare repaint) because a host may set this before the first show, when the layout is
' still pending -- and it costs nothing when it is not.
sub PsPopupMenu_SetSeparatorThickness( byval hPopup as HWND, byval nThickness as long )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    if nThickness < 1 then nThickness = 1
    pThis->nSeparatorThickness = nThickness
    pThis->Refresh()
end sub


sub PsPopupMenu_SetColors( byval hPopup as HWND, byval pColors as PSPOPUPMENU_COLORS ptr )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    if pColors = null then exit sub
    pThis->colors = *pColors              ' copied; the caller's struct stays theirs
    if pThis->hWin then InvalidateRect( pThis->hWin, NULL, TRUE )
end sub


sub PsPopupMenu_SetFonts( byval hPopup as HWND, byval hTextFont as HFONT, byval hGlyphFont as HFONT = 0 )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->hTextFont = hTextFont
    pThis->hGlyphFont = hGlyphFont
    ' The text font is what widths are measured with, so changing it re-lays out.
    pThis->Refresh()
end sub


sub PsPopupMenu_SetGlyphs( byval hPopup as HWND, byval wszCheck as DWSTRING, byval wszChevron as DWSTRING )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->wszCheckGlyph = wszCheck
    pThis->wszChevronGlyph = wszChevron
    if pThis->hWin then InvalidateRect( pThis->hWin, NULL, TRUE )
end sub


' Show at a screen point (the context-menu door). Always shows as a chain ROOT.
function PsPopupMenu_Show( byval hPopup as HWND, byval x as long, byval y as long ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false
    pThis->hParentLevel = 0
    pThis->nParentItem = -1
    dim as RECT rcAnchor = type( x, y, x, y )
    return PM_ShowCore( hPopup, 0, rcAnchor )
end function


' Show against a screen anchor rect (PM_ALIGN_BELOW / PM_ALIGN_RIGHT). Always shows as
' a chain ROOT -- submenu levels are opened internally by their parent level.
function PsPopupMenu_ShowForRect( byval hPopup as HWND, byval rcAnchor as RECT, byval nAlign as long ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false
    pThis->hParentLevel = 0
    pThis->nParentItem = -1
    return PM_ShowCore( hPopup, nAlign, rcAnchor )
end function


sub PsPopupMenu_Hide( byval hPopup as HWND )
    PM_HideLevel( hPopup )
end sub


sub PsPopupMenu_CloseChain( byval hPopup as HWND )
    ' Programmatic close: SILENT, like every setter.
    PM_CloseChain( hPopup, 0, false )
end sub


function PsPopupMenu_IsOpen( byval hPopup as HWND ) as boolean
    if hPopup = 0 then return false
    return (IsWindowVisible(hPopup) <> 0)
end function


function PsPopupMenu_GetCurSel( byval hPopup as HWND ) as long
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return -1
    return pThis->nCurSel
end function


function PsPopupMenu_SetCurSel( byval hPopup as HWND, byval idx as long ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false
    if (idx <> -1) andalso (pThis->IsValidItem(idx) = false) then return false
    if idx <> pThis->nCurSel then
        PM_InvalidateItem( pThis, pThis->nCurSel )
        PM_InvalidateItem( pThis, idx )
        pThis->nCurSel = idx
    end if
    ' Keyboard and programmatic selection is NOT hover-owned: the hover-clear paths
    ' (WM_MOUSELEAVE, the poll net) must leave it alone.
    pThis->bHoverSel = false
    return true
end function


function PsPopupMenu_SelectFirst( byval hPopup as HWND ) as boolean
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then return false
    return PsPopupMenu_SetCurSel( hPopup, pThis->NextSelectable( -1, 1 ) )
end function


sub PsPopupMenu_SetNotifyWindow( byval hPopup as HWND, byval hNotifyWnd as HWND )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->hNotifyWnd = hNotifyWnd
end sub


sub PsPopupMenu_SetSelectCallback( byval hPopup as HWND, byval usersub as PM_SelectCallbackSub )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->SelectCallback = usersub
end sub


sub PsPopupMenu_SetInitPopupCallback( byval hPopup as HWND, byval usersub as PM_InitPopupCallbackSub )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->InitPopupCallback = usersub
end sub


sub PsPopupMenu_SetPaintItemCallback( byval hPopup as HWND, byval usersub as PM_PaintItemCallbackSub )
    dim as PSPOPUPMENU ptr pThis = GetPopupMenuPointer(hPopup)
    if pThis = null then exit sub
    pThis->PaintItemCallback = usersub
end sub


' ========================================================================================
' The message-pump filter (see the contract in PsPopupMenu.bi). Accepts any handle in
' the popup's chain; acts only while that chain is THE open chain.
' ========================================================================================
function PsPopupMenu_FilterMessage( byval hPopup as HWND, byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if gPM_OpenRoot = 0 then return false
    dim as HWND hRoot = PM_ResolveRootHwnd( hPopup )
    if hRoot <> gPM_OpenRoot then return false
    if IsWindowVisible(hRoot) = 0 then return false

    select case pMsg->message

    ' Outside-click dismissal. Returns FALSE on purpose: the click that dismissed the
    ' menu still reaches its target (tiko behavior -- clicking a toolbar button while
    ' a menu is open both closes the menu and presses the button).
    case WM_LBUTTONDOWN, WM_NCLBUTTONDOWN
        if PM_IsChainWindow( pMsg->hwnd ) = false then
            PM_CloseChain( hRoot, 0, true )
        end if
        return false

    case WM_KEYDOWN
        dim as HWND hInner = PM_InnermostHwnd( hRoot )
        dim as PSPOPUPMENU ptr pInner = GetPopupMenuPointer( hInner )
        if pInner = null then return false

        select case pMsg->wParam
        case VK_DOWN
            PsPopupMenu_SetCurSel( hInner, pInner->NextSelectable( pInner->nCurSel, 1 ) )
        case VK_UP
            ' From "nothing selected", Up lands on the LAST selectable (wrap semantics).
            dim as long nFrom = pInner->nCurSel
            if nFrom = -1 then nFrom = pInner->itemCount
            PsPopupMenu_SetCurSel( hInner, pInner->NextSelectable( nFrom, -1 ) )
        case VK_HOME
            PsPopupMenu_SetCurSel( hInner, pInner->NextSelectable( -1, 1 ) )
        case VK_END
            PsPopupMenu_SetCurSel( hInner, pInner->NextSelectable( pInner->itemCount, -1 ) )
        case VK_RIGHT
            ' Open the selected row's submenu. A row without one is a consumed no-op
            ' here -- under a menubar, PsMenuBar's filter pre-handles that case as
            ' "move to the next bar item" before delegating.
            dim as long idx = pInner->nCurSel
            if pInner->IsValidItem(idx) then
                dim as boolean bCanOpen = _
                    (pInner->items(idx).hSubMenu <> 0) andalso _
                    (pInner->items(idx).isDisabled = false)
                if bCanOpen then PM_OpenSubMenu( hInner, idx, true )
            end if
        case VK_LEFT
            ' Close the innermost submenu (its parent keeps the highlight). At the
            ' root this is a consumed no-op -- PsMenuBar pre-handles it.
            if hInner <> hRoot then PM_HideLevel( hInner )
        case VK_RETURN
            dim as long idx = pInner->nCurSel
            if pInner->IsValidItem(idx) then
                dim as boolean bHasSub = (pInner->items(idx).hSubMenu <> 0)
                if bHasSub then
                    if pInner->items(idx).isDisabled = false then
                        PM_OpenSubMenu( hInner, idx, true )
                    end if
                else
                    PM_ExecuteItem( hInner, idx )
                end if
            end if
        case VK_ESCAPE
            if hInner <> hRoot then
                PM_HideLevel( hInner )
            else
                PM_CloseChain( hRoot, 1, true )      ' reason 1: Escape at the root
            end if
        end select
        ' EVERY keydown is consumed while the chain is open: stray typing must not
        ' reach the host's editor.
        return true

    case WM_KEYUP, WM_CHAR
        return true

    ' WM_SYSKEY* stays the host's: Alt handling is a host decision (tiko's Alt
    ' conflicts with Scintilla's Alt+column select).

    end select

    return false
end function
