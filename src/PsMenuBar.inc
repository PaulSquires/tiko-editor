
''
''  PsMenuBar.inc
''

#include once "PsMenuBar.bi"
' The bar drives its dropdowns through the popup's implementation (including a few of
' its module-scope helpers -- everything is #include'd into one module, the family's
' vendoring reality), so pull it in here: including PsMenuBar.inc alone gives a host
' the complete menu system.
#include once "PsPopupMenu.inc"


private function GetMenuBarPointer( byval hMenuBar as HWND ) as PSMENUBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hMenuBar)
    dim as PSMENUBAR ptr pBar = null
    if pWindow then pBar = cast( PSMENUBAR ptr, pWindow->UserData(0) )
    return pBar
end function

declare function MB_OpenMenuAt( byval hMenuBar as HWND, byval idx as long, byval bSelectFirst as boolean ) as boolean


' ========================================================================================
' PSMENUBAR member procedures
' ========================================================================================

' Measure every item and assign its rect. The only place bar geometry is produced.
' Items run contiguously from x = 0 and span the full client height; when they do not
' fit, the rightmost simply clips at the client edge.
sub PSMENUBAR.LayoutItems()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub
    if this.itemCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    ' No geometry yet (created 0x0, sized only once the host positions us). Nothing to
    ' lay out; WM_SIZE will mark the layout dirty again.
    if (rcClient.bottom - rcClient.top) <= 0 then
        this.bLayoutDirty = true
        exit sub
    end if

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true
        exit sub
    end if
    dim as HFONT hFontUse = this.hBarFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as long x = rcClient.left
    for i as long = 0 to this.itemCount - 1
        dim as long textW = 0
        dim as long nChars = PsLen( this.items(i).wszText )
        if nChars > 0 then
            dim as SIZE sz
            if GetTextExtentPoint32W( hDC, this.items(i).wszText.vptr, nChars, @sz ) then
                textW = sz.cx
            end if
        end if
        this.items(i).nWidth = textW + (this.nPadding * 2)
        SetRect( @this.items(i).rc, x, rcClient.top, x + this.items(i).nWidth, rcClient.bottom )
        x += this.items(i).nWidth
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )
end sub

function PSMENUBAR.HitTest( byval x as long, byval y as long ) as long
    dim as POINT pt = ( x, y )
    for i as long = 0 to this.itemCount - 1
        if PtInRect( @this.items(i).rc, pt ) then return i
    next
    return -1
end function

function PSMENUBAR.IsValidItem( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.itemCount)
end function

sub PSMENUBAR.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' Private helpers
' ========================================================================================

private sub MB_InvalidateItem( byval pBar as PSMENUBAR ptr, byval idx as long )
    if pBar = null then exit sub
    if pBar->IsValidItem(idx) = false then exit sub
    InvalidateRect( pBar->hWin, @pBar->items(idx).rc, FALSE )
end sub

' Clear any hovered item and stop the safety-net timer. Cheap to call repeatedly.
private sub MB_ClearHotTracking( byval pBar as PSMENUBAR ptr, byval hMenuBar as HWND )
    if pBar = null then exit sub
    if pBar->hotTimerOn then
        KillTimer( hMenuBar, IDT_CMENUBAR_HOTTRACK )
        pBar->hotTimerOn = false
    end if
    if pBar->nHotIdx <> -1 then
        MB_InvalidateItem( pBar, pBar->nHotIdx )
        pBar->nHotIdx = -1
    end if
end sub

' Set the active (open/highlight) item and repaint the two affected items.
private sub MB_SetActive( byval pBar as PSMENUBAR ptr, byval idx as long )
    if pBar = null then exit sub
    if idx <> pBar->nActiveIdx then
        MB_InvalidateItem( pBar, pBar->nActiveIdx )
        MB_InvalidateItem( pBar, idx )
        pBar->nActiveIdx = idx
    end if
end sub

' Open item idx's dropdown: fire the bar's InitPopup hook, aim the popup's
' notifications back at the bar, and show it under the item. Showing a new root
' silently closes any other open chain (PM_ShowCore), so a hover-switch is just
' "open the other one".
private function MB_OpenMenuAt( byval hMenuBar as HWND, byval idx as long, byval bSelectFirst as boolean ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    if pBar->IsValidItem(idx) = false then return false
    dim as HWND hDrop = pBar->items(idx).hDropDown
    if hDrop = 0 then return false
    if pBar->bLayoutDirty then pBar->LayoutItems()
    if pBar->bLayoutDirty then return false       ' no geometry to anchor against yet

    if pBar->InitPopupCallback then pBar->InitPopupCallback( hMenuBar, idx, hDrop )
    PsPopupMenu_SetNotifyWindow( hDrop, hMenuBar )

    dim as RECT rcAnchor = pBar->items(idx).rc
    MapWindowPoints( hMenuBar, HWND_DESKTOP, cast(POINT ptr, @rcAnchor), 2 )
    if PsPopupMenu_ShowForRect( hDrop, rcAnchor, PM_ALIGN_BELOW ) = false then return false
    if bSelectFirst then PsPopupMenu_SelectFirst( hDrop )

    MB_SetActive( pBar, idx )
    pBar->bMenuOpen = true
    return true
end function

' Step the active highlight/menu to the previous/next bar item that HAS a dropdown,
' wrapping (the keyboard Left/Right-at-the-boundary move).
private sub MB_OpenAdjacent( byval hMenuBar as HWND, byval nDir as long )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    if pBar->itemCount = 0 then exit sub
    dim as long idx = pBar->nActiveIdx
    for k as long = 1 to pBar->itemCount
        idx += nDir
        if idx < 0 then idx = pBar->itemCount - 1
        if idx > pBar->itemCount - 1 then idx = 0
        if pBar->items(idx).hDropDown then
            MB_OpenMenuAt( hMenuBar, idx, true )
            exit sub
        end if
    next
end sub

' Move the highlight-only selection (no menu open) to the previous/next item, wrapping.
private sub MB_MoveHighlight( byval pBar as PSMENUBAR ptr, byval nDir as long )
    if pBar = null then exit sub
    if pBar->itemCount = 0 then exit sub
    dim as long idx = pBar->nActiveIdx
    idx += nDir
    if idx < 0 then idx = pBar->itemCount - 1
    if idx > pBar->itemCount - 1 then idx = 0
    MB_SetActive( pBar, idx )
end sub


' ========================================================================================
' Process WM_PAINT message for window: PsMenuBar
' ========================================================================================
private function PsMenuBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hwnd)
    if pBar = null then return 0

    if pBar->bLayoutDirty then pBar->LayoutItems()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pBar->colors.BackColor )
    b.PaintClientRect()

    dim p as PSMENUBAR_PAINTINFO
    p.hMenuBar = hwnd
    p.b = @b
    for i as long = 0 to pBar->itemCount - 1
        p.itemID     = i
        p.rc         = pBar->items(i).rc
        p.isHot      = (i = pBar->nHotIdx)
        p.isActive   = (i = pBar->nActiveIdx)
        p.wszCaption = pBar->items(i).wszText
        if pBar->PaintItemCallback then
            pBar->PaintItemCallback( @p )
        else
            ' Hover and active render the SAME: hot fill + hot text. The active item
            ' is the one whose dropdown is open (or holds the keyboard highlight), and
            ' since a click opens that dropdown immediately, hover and active are two
            ' frames of one gesture -- styling them differently made the fill flicker
            ' in under the cursor on press. Use the paint callback for a design that
            ' wants them distinct.
            dim as COLORREF backclr, foreclr
            if p.isActive orelse p.isHot then
                backclr = pBar->colors.BackColorHot
                foreclr = pBar->colors.ForeColorHot
            else
                backclr = pBar->colors.BackColor
                foreclr = pBar->colors.ForeColor
            end if
            b.SetColors( foreclr, backclr )
            b.PaintRect( @p.rc )
            b.SetFont( pBar->hBarFont )
            b.PaintText( p.wszCaption, @p.rc, DT_CENTER )
        end if
    next

    b.EndDoubleBuffer()
    return 0
end function


' ========================================================================================
' PsMenuBar Window procedure
' ========================================================================================
function PsMenuBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSMENUBAR ptr pBar = GetMenuBarPointer( hwnd )

    select case uMsg
    case WM_PAINT
        return PsMenuBar_OnPaint( hwnd )

    case WM_SIZE
        ' Item rects span the full client height, so a resize invalidates the layout.
        if wParam <> SIZE_MINIMIZED then
            if pBar then pBar->Refresh()
        end if
        return 0

    case WM_MOUSEMOVE
        if pBar then
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            ' Safety net for the unreliable WM_MOUSELEAVE.
            if pBar->hotTimerOn = false then
                SetTimer( hwnd, IDT_CMENUBAR_HOTTRACK, PSMENUBAR_HOTTRACK_MS, NULL )
                pBar->hotTimerOn = true
            end if

            dim as long idx = pBar->HitTest( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if idx <> pBar->nHotIdx then
                MB_InvalidateItem( pBar, pBar->nHotIdx )
                MB_InvalidateItem( pBar, idx )
                pBar->nHotIdx = idx
            end if

            ' Hover-switch: while a dropdown is open, moving onto another bar item
            ' that has one switches the open dropdown to it (no click needed).
            if pBar->bMenuOpen then
                dim as boolean bSwitch = pBar->IsValidItem(idx) andalso (idx <> pBar->nActiveIdx)
                if bSwitch then
                    if pBar->items(idx).hDropDown then MB_OpenMenuAt( hwnd, idx, false )
                end if
            end if
        end if
        return 0

    case WM_MOUSELEAVE
        ' Clears the hover highlight only -- the active (open/keyboard) item persists.
        MB_ClearHotTracking( pBar, hwnd )
        return 0

    case WM_TIMER
        if (pBar <> 0) andalso (wParam = IDT_CMENUBAR_HOTTRACK) then
            dim as RECT rcClient
            GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                MB_ClearHotTracking( pBar, hwnd )
            end if
            return 0
        end if

    ' No mouse capture is taken -- the per-control test, re-run for this control: the
    ' bar acts on the DOWN alone (menus open on press), tracks no press state and has
    ' no drag, so nothing consumes the down->up pairing.
    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        ' WM_LBUTTONDBLCLK substitutes for the second down of a fast double-click
        ' (CS_DBLCLKS), so it runs the same toggle logic.
        if pBar then
            dim as long idx = pBar->HitTest( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if pBar->bMenuOpen then
                ' Any bar click while a dropdown is open closes it (hover-switch has
                ' already moved the dropdown to whatever item the cursor is on).
                PsMenuBar_CloseMenu( hwnd )
            else
                if pBar->IsValidItem(idx) then
                    if pBar->items(idx).hDropDown then MB_OpenMenuAt( hwnd, idx, false )
                end if
            end if
        end if
        return 0

    ' The dropdowns report back through these (the bar registered itself as their
    ' notify window). The chain is already closed when either arrives.
    case CPM_NOTIFY_COMMAND
        if pBar then
            MB_SetActive( pBar, -1 )
            pBar->bMenuOpen = false
            dim as long id = cast(long, cast(LONG_PTR, wParam))
            if pBar->CommandCallback then pBar->CommandCallback( hwnd, id )
        end if
        return 0

    case CPM_NOTIFY_CLOSED
        if pBar then
            pBar->bMenuOpen = false
            ' Escape at the root (wParam = 1) drops back to the keyboard highlight;
            ' any other dismissal (outside click, foreground loss) clears it.
            if wParam <> 1 then MB_SetActive( pBar, -1 )
            MB_InvalidateItem( pBar, pBar->nActiveIdx )
        end if
        return 0

    case WM_ERASEBKGND
        return true

    case WM_DESTROY
        if (pBar <> 0) andalso pBar->hotTimerOn then
            KillTimer( hwnd, IDT_CMENUBAR_HOTTRACK )
            pBar->hotTimerOn = false
        end if
        ' No capture to release: none is ever taken (see the press handler).

    case WM_NCDESTROY
        if pBar then
            ' Attached dropdowns are OWNED: destroying the bar unwinds the whole menu
            ' tree (each popup's own WM_NCDESTROY destroys its submenus).
            for i as long = 0 to pBar->itemCount - 1
                if pBar->items(i).hDropDown then
                    DestroyWindow( pBar->items(i).hDropDown )
                    pBar->items(i).hDropDown = 0
                end if
            next
            delete(pBar)
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

function PsMenuBar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND
    dim pWindow as CWindow ptr = new CWindow

    dim as HWND hCtrl = pWindow->Create( hWndParent, "", @PsMenuBar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN )
    ' Replace the class style (dropping CS_HREDRAW/CS_VREDRAW, which only add
    ' flicker); CS_DBLCLKS so a fast double-click still toggles per press.
    pWindow->ClassStyle = CS_DBLCLKS

    dim as PSMENUBAR ptr pBar = new PSMENUBAR
    pBar->hWin = hCtrl
    pBar->idc_MenuBar = CtrlID
    pBar->nPadding = pWindow->ScaleX( PSMENUBAR_DEFAULT_PADDING )
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    pWindow->UserData(0) = cast(LONG_PTR, pBar)
    return hCtrl
end function


function PsMenuBar_AddItem( byval hMenuBar as HWND, byval Text as DWSTRING, byval id as long ) as long
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return -1
    dim as long cap = ubound(pBar->items) + 1
    if pBar->itemCount >= cap then
        dim as long newcap = iif( cap = 0, 8, cap * 2 )
        redim preserve pBar->items( 0 to newcap - 1 )
    end if
    dim as long idx = pBar->itemCount
    with pBar->items(idx)
        .id        = id
        .wszText   = Text
        .hDropDown = 0
        .nWidth    = 0
        SetRectEmpty( @.rc )
    end with
    pBar->itemCount += 1
    pBar->Refresh()
    return idx
end function


function PsMenuBar_GetItemCount( byval hMenuBar as HWND ) as long
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return 0
    return pBar->itemCount
end function


function PsMenuBar_GetItemText( byval hMenuBar as HWND, byval idx as long ) as DWSTRING
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return ""
    if pBar->IsValidItem(idx) = false then return ""
    dim as DWSTRING wszText = pBar->items(idx).wszText
    return wszText
end function


function PsMenuBar_SetItemText( byval hMenuBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    if pBar->IsValidItem(idx) = false then return false
    pBar->items(idx).wszText = Text
    ' Text drives the item's width, so this needs a re-layout, not just a repaint.
    pBar->Refresh()
    return true
end function


' Attach a PsPopupMenu as item idx's dropdown. OWNERSHIP TRANSFERS: the bar destroys
' attached dropdowns when it is destroyed. Attaching over an existing dropdown
' destroys the old one; hPopup = 0 detaches without destroying.
function PsMenuBar_SetDropDown( byval hMenuBar as HWND, byval idx as long, byval hPopup as HWND ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    if pBar->IsValidItem(idx) = false then return false
    dim as HWND hOld = pBar->items(idx).hDropDown
    if (hOld <> 0) andalso (hOld <> hPopup) then DestroyWindow( hOld )
    pBar->items(idx).hDropDown = hPopup
    return true
end function


function PsMenuBar_GetDropDown( byval hMenuBar as HWND, byval idx as long ) as HWND
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return 0
    if pBar->IsValidItem(idx) = false then return 0
    return pBar->items(idx).hDropDown
end function


function PsMenuBar_GetItemRect( byval hMenuBar as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    SetRectEmpty( @rc )
    if pBar = null then return false
    if pBar->IsValidItem(idx) = false then return false
    if pBar->bLayoutDirty then pBar->LayoutItems()
    rc = pBar->items(idx).rc
    return true
end function


' The right edge of the last item -- the width the items actually need. The host sizes
' the bar with this, or draws its own content in the leftover strip of a wider bar.
function PsMenuBar_GetIdealWidth( byval hMenuBar as HWND ) as long
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return 0
    if pBar->itemCount = 0 then return 0
    if pBar->bLayoutDirty then pBar->LayoutItems()
    if pBar->bLayoutDirty then return 0           ' no geometry yet
    return pBar->items(pBar->itemCount - 1).rc.right
end function


function PsMenuBar_GetPadding( byval hMenuBar as HWND ) as long
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return 0
    return pBar->nPadding
end function


sub PsMenuBar_SetPadding( byval hMenuBar as HWND, byval nPadding as long )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    if nPadding < 0 then nPadding = 0
    pBar->nPadding = nPadding
    pBar->Refresh()
end sub


' The host's keyboard entry point (tiko's Alt+F accelerator): open item idx's dropdown,
' optionally selecting its first selectable row. SILENT beyond the InitPopup hooks.
function PsMenuBar_OpenMenu( byval hMenuBar as HWND, byval idx as long, byval bSelectFirst as boolean = false ) as boolean
    return MB_OpenMenuAt( hMenuBar, idx, bSelectFirst )
end function


' Close any open dropdown chain AND clear the highlight. SILENT (programmatic) --
' also the host's instant-dismissal hook for WM_ACTIVATEAPP(FALSE).
sub PsMenuBar_CloseMenu( byval hMenuBar as HWND )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    if pBar->bMenuOpen then
        if pBar->IsValidItem(pBar->nActiveIdx) then
            dim as HWND hDrop = pBar->items(pBar->nActiveIdx).hDropDown
            if hDrop then PsPopupMenu_CloseChain( hDrop )
        end if
    end if
    pBar->bMenuOpen = false
    MB_SetActive( pBar, -1 )
end sub


function PsMenuBar_IsMenuOpen( byval hMenuBar as HWND ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    return pBar->bMenuOpen
end function


function PsMenuBar_GetActiveIndex( byval hMenuBar as HWND ) as long
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return -1
    return pBar->nActiveIdx
end function


' Set the keyboard highlight WITHOUT opening a menu (-1 clears). What a host's bare
' Alt-press handler would call. SILENT.
function PsMenuBar_SetActiveIndex( byval hMenuBar as HWND, byval idx as long ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    if (idx <> -1) andalso (pBar->IsValidItem(idx) = false) then return false
    MB_SetActive( pBar, idx )
    return true
end function


sub PsMenuBar_SetColors( byval hMenuBar as HWND, byval pColors as PSMENUBAR_COLORS ptr )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    if pColors = null then exit sub
    pBar->colors = *pColors               ' copied; the caller's struct stays theirs
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, TRUE )
end sub


sub PsMenuBar_SetFont( byval hMenuBar as HWND, byval hBarFont as HFONT )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    pBar->hBarFont = hBarFont
    ' The font is what item widths are measured with, so changing it re-lays out.
    pBar->Refresh()
end sub


sub PsMenuBar_SetCommandCallback( byval hMenuBar as HWND, byval usersub as MB_CommandCallbackSub )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    pBar->CommandCallback = usersub
end sub


sub PsMenuBar_SetInitPopupCallback( byval hMenuBar as HWND, byval usersub as MB_InitPopupCallbackSub )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    pBar->InitPopupCallback = usersub
end sub


sub PsMenuBar_SetPaintItemCallback( byval hMenuBar as HWND, byval usersub as MB_PaintItemCallbackSub )
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then exit sub
    pBar->PaintItemCallback = usersub
end sub


' ========================================================================================
' The message-pump filter for the whole menu system (see PsMenuBar.bi). Wraps the
' PsPopupMenu filter, adding the bar-boundary keyboard moves and the highlight-only
' keyboard mode.
' ========================================================================================
function PsMenuBar_FilterMessage( byval hMenuBar as HWND, byval pMsg as MSG ptr ) as boolean
    dim as PSMENUBAR ptr pBar = GetMenuBarPointer(hMenuBar)
    if pBar = null then return false
    if pMsg = 0 then return false

    if pBar->bMenuOpen then
        dim as HWND hRoot = 0
        if pBar->IsValidItem(pBar->nActiveIdx) then hRoot = pBar->items(pBar->nActiveIdx).hDropDown
        if hRoot = 0 orelse (PsPopupMenu_IsOpen(hRoot) = false) then
            ' The chain closed behind our back (it always notifies, so this is a
            ' belt-and-braces resync, not an expected path).
            pBar->bMenuOpen = false
            return false
        end if

        ' Bar-boundary keys, pre-handled before delegating to the popup filter:
        ' Left at the root level / Right on a row without a submenu step between
        ' bar items (wrapping), with the first row selected -- classic menubar keys.
        if pMsg->message = WM_KEYDOWN then
            select case pMsg->wParam
            case VK_LEFT
                dim as HWND hInner = PM_InnermostHwnd( hRoot )
                if hInner = hRoot then
                    MB_OpenAdjacent( hMenuBar, -1 )
                    return true
                end if
            case VK_RIGHT
                dim as HWND hInner = PM_InnermostHwnd( hRoot )
                dim as PSPOPUPMENU ptr pInner = GetPopupMenuPointer( hInner )
                if pInner then
                    dim as long idx = pInner->nCurSel
                    dim as boolean bHasSub = false
                    if pInner->IsValidItem(idx) then
                        bHasSub = (pInner->items(idx).hSubMenu <> 0) andalso _
                                  (pInner->items(idx).isDisabled = false)
                    end if
                    if bHasSub = false then
                        MB_OpenAdjacent( hMenuBar, 1 )
                        return true
                    end if
                end if
            end select
        end if

        ' A click on the bar itself must reach the bar's own toggle handler, not be
        ' treated as an outside click.
        if (pMsg->message = WM_LBUTTONDOWN) andalso (pMsg->hwnd = pBar->hWin) then return false

        return PsPopupMenu_FilterMessage( hRoot, pMsg )
    end if

    ' Highlight-only mode: a bar item is highlighted (keyboard) with no dropdown open.
    if pBar->nActiveIdx <> -1 then
        select case pMsg->message
        case WM_KEYDOWN
            select case pMsg->wParam
            case VK_LEFT
                MB_MoveHighlight( pBar, -1 )
                return true
            case VK_RIGHT
                MB_MoveHighlight( pBar, 1 )
                return true
            case VK_DOWN, VK_RETURN
                MB_OpenMenuAt( hMenuBar, pBar->nActiveIdx, true )
                return true
            case VK_ESCAPE
                MB_SetActive( pBar, -1 )
                return true
            end select
        case WM_LBUTTONDOWN, WM_NCLBUTTONDOWN
            ' A click anywhere but the bar abandons the highlight; the click proceeds.
            if pMsg->hwnd <> pBar->hWin then MB_SetActive( pBar, -1 )
            return false
        end select
    end if

    return false
end function
