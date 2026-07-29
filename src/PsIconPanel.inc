
''
''  PsIconPanel.inc
''

#include once "PsIconPanel.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"
' Image loader implementation, pulled in here so a host needs no extra include line
' (#include once guards it if the host includes it too).
#include once "PsImage.inc"


function GetIconPanelPointer( byval hIconPanel as HWND ) as PSICONPANEL ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hIconPanel)
    dim as PSICONPANEL ptr pIcon = null
    if pWindow then pIcon = cast( PSICONPANEL ptr, pWindow->UserData(0) )
    return pIcon
end function


' Which item is the cursor over, for HOVER and CLICK purposes? Returns a model index, or
' -1 for none. This is the public HitTest plus two extra rules that belong to interaction
' and not to geometry:
'   - the cursor must be inside the client area. Cells can legitimately run PAST the right
'     edge when the run overflows, and without this test a cursor over the sibling window
'     next door would light one of them up.
'   - a DISABLED item is never hot and never pressable.
private function GetHotIconPanelItem( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return -1

    dim as POINT pt
    GetCursorPos( @pt )
    ScreenToClient( hIconPanel, @pt )

    dim as RECT rcClient
    GetClientRect( hIconPanel, @rcClient )
    if PtInRect( @rcClient, pt ) = 0 then return -1

    dim as long idx = pIcon->HitTest( pt.x, pt.y )
    if idx = -1 then return -1
    if pIcon->items(idx).isEnabled = false then return -1
    return idx
end function


' Repaint one item. Silently does nothing for an invalid index, which is what makes the
' hover path safe when the cursor is over empty panel area (idx = -1). FALSE = no
' background erase; the item's whole cell is painted every time.
private sub PsIconPanel_InvalidateItem( byval pIcon as PSICONPANEL ptr, byval idx as long )
    if pIcon = null then exit sub
    if pIcon->IsValidItem(idx) = false then exit sub
    InvalidateRect( pIcon->hWin, @pIcon->items(idx).rc, FALSE )
end sub


' ========================================================================================
' Append an item. itemKind is IP_KIND_TOGGLE or IP_KIND_COMMAND (a separator has no glyph
' or id of its own -- use PsIconPanel_AddSeparator). Returns the new item's index, or -1.
' ========================================================================================
function PsIconPanel_AddItem( byval hIconPanel as HWND, byval itemKind as long, byval Glyph as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return -1
    dim as PSICONPANEL_ITEM ptr pItem = pIcon->AddItem()
    if pItem = null then return -1
    pItem->itemKind = itemKind
    pItem->wszGlyph = Glyph
    pItem->id       = id
    pItem->itemData = itemData
    return pIcon->GetCount() - 1  ' model index of the appended item
end function


' ========================================================================================
' Append a separator: a vertical rule as tall as an icon, drawn by the control itself.
' Give it its own padding (PsIconPanel_SetItemPadding) to control the size of the visual
' gap it makes.
' ========================================================================================
function PsIconPanel_AddSeparator( byval hIconPanel as HWND ) as long
    return PsIconPanel_AddItem( hIconPanel, IP_KIND_SEPARATOR, "", 0, 0 )
end function


' ========================================================================================
' Refresh the custom icon panel control.
' ========================================================================================
sub PsIconPanel_Refresh( byval hIconPanel as HWND )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon then pIcon->Refresh()
end sub


' ========================================================================================
' Retrieve the current item count in the custom icon panel control.
' ========================================================================================
function PsIconPanel_GetCount( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    return pIcon->GetCount()
end function


' ========================================================================================
' Is idx a valid item index?
' ========================================================================================
function PsIconPanel_IsValidItem( byval hIconPanel as HWND, byval idx as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    return pIcon->IsValidItem( idx )
end function


' ========================================================================================
' Index of the FIRST item carrying this id, or -1. Separators are skipped: they all carry
' id 0, so a search for 0 would otherwise find decoration.
' ========================================================================================
function PsIconPanel_FindItemByID( byval hIconPanel as HWND, byval id as long ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return -1
    for i as long = 0 to pIcon->itemCount - 1
        if pIcon->items(i).itemKind = IP_KIND_SEPARATOR then continue for
        if pIcon->items(i).id = id then return i
    next
    return -1
end function


' ========================================================================================
' Which item is at this CLIENT-coordinate point? Forces a pending layout so the answer is
' always current. Separators are excluded; disabled items are included (they occupy their
' cell, and a caller asking "what is here" deserves the truth).
' ========================================================================================
function PsIconPanel_HitTest( byval hIconPanel as HWND, byval x as long, byval y as long ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return -1
    if pIcon->bLayoutDirty then pIcon->LayoutItems()
    return pIcon->HitTest( x, y )
end function


' ========================================================================================
' Retrieve an item's kind (IP_KIND_*), or -1 for an invalid index.
' ========================================================================================
function PsIconPanel_GetItemKind( byval hIconPanel as HWND, byval idx as long ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return -1
    if pIcon->IsValidItem(idx) = false then return -1
    return pIcon->GetItem(idx)->itemKind
end function


' ========================================================================================
' Retrieve an item's glyph.
' ========================================================================================
function PsIconPanel_GetGlyph( byval hIconPanel as HWND, byval idx as long ) as DWSTRING
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return ""
    if pIcon->IsValidItem(idx) = false then return ""
    dim as DWSTRING wszGlyph = pIcon->GetItem(idx)->wszGlyph
    return wszGlyph
end function


' ========================================================================================
' Set an item's glyph. Repaints that item only -- NOT a re-layout: the cell width is
' declared, never measured, so a glyph swap cannot move anything.
' ========================================================================================
function PsIconPanel_SetGlyph( byval hIconPanel as HWND, byval idx as long, byval Glyph as DWSTRING ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    dim as PSICONPANEL_ITEM ptr pit = pIcon->GetItem(idx)
    pit->wszGlyph = Glyph
    ' Glyph OR image: setting an actual glyph frees any image on this item.
    if (len( Glyph ) > 0) andalso (pit->pImage <> 0) then
        delete pit->pImage : pit->pImage = 0
    end if
    PsIconPanel_InvalidateItem( pIcon, idx )
    return true
end function

' ========================================================================================
' Load an image into an item's cell, replacing its glyph. "" removes it. TRUE on a
' successful load; on failure the empty image is dropped so nothing draws.
' ========================================================================================
function PsIconPanel_SetImage( byval hIconPanel as HWND, byval idx as long, byval Path as DWSTRING ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    dim as PSICONPANEL_ITEM ptr pit = pIcon->GetItem(idx)

    if len( Path ) = 0 then
        ' Removal SUCCEEDS. It used to return FALSE, which a caller could not tell apart from
        ' a load that failed -- so "did that do what I asked" had no answer on this path.
        if pit->pImage then delete pit->pImage : pit->pImage = 0
        PsIconPanel_InvalidateItem( pIcon, idx )
        return true
    end if

    if pit->pImage = 0 then pit->pImage = new PsImage
    dim as long bOK = pit->pImage->Load( Path )
    if bOK = 0 then
        delete pit->pImage : pit->pImage = 0
    else
        pit->wszGlyph = ""                 ' glyph OR image
    end if
    PsIconPanel_InvalidateItem( pIcon, idx )
    return (bOK <> 0)
end function


' ========================================================================================
' Get / set an item's host command id (reported by the click callback).
' ========================================================================================
function PsIconPanel_GetItemID( byval hIconPanel as HWND, byval idx as long ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    if pIcon->IsValidItem(idx) = false then return 0
    return pIcon->GetItem(idx)->id
end function

function PsIconPanel_SetItemID( byval hIconPanel as HWND, byval idx as long, byval id as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    pIcon->GetItem(idx)->id = id
    return true
end function


' ========================================================================================
' Get / set an item's free-form host payload.
' ========================================================================================
function PsIconPanel_GetItemData( byval hIconPanel as HWND, byval idx as long ) as integer
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    if pIcon->IsValidItem(idx) = false then return 0
    return pIcon->GetItem(idx)->itemData
end function

function PsIconPanel_SetItemData( byval hIconPanel as HWND, byval idx as long, byval itemData as integer ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    pIcon->GetItem(idx)->itemData = itemData
    return true
end function


' ========================================================================================
' Is this TOGGLE item latched?
' ========================================================================================
function PsIconPanel_GetSelected( byval hIconPanel as HWND, byval idx as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    return pIcon->GetItem(idx)->isSelected
end function


' ========================================================================================
' Latch / unlatch a TOGGLE item. SILENT -- no SelChangeCallback, because only the user
' fires that (the family rule); this also makes it safe to call from inside the change
' handler. Returns FALSE for an invalid index or an item that is not a toggle.
' ========================================================================================
function PsIconPanel_SetSelected( byval hIconPanel as HWND, byval idx as long, byval isSelected as boolean ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    dim as PSICONPANEL_ITEM ptr pItem = pIcon->GetItem(idx)
    if pItem->itemKind <> IP_KIND_TOGGLE then return false
    if pItem->isSelected = isSelected then return true      ' no repaint for a no-op
    pItem->isSelected = isSelected
    PsIconPanel_InvalidateItem( pIcon, idx )
    return true
end function


' ========================================================================================
' Is this item enabled?
' ========================================================================================
function PsIconPanel_GetEnabled( byval hIconPanel as HWND, byval idx as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    return pIcon->GetItem(idx)->isEnabled
end function


' ========================================================================================
' Enable / disable an item. A disabled item greys out, stops answering hover, and swallows
' clicks -- so disabling the item the cursor is on, or the one under a live press, must
' clear that state here rather than wait for the next mouse move.
' ========================================================================================
function PsIconPanel_SetEnabled( byval hIconPanel as HWND, byval idx as long, byval isEnabled as boolean ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    dim as PSICONPANEL_ITEM ptr pItem = pIcon->GetItem(idx)
    if pItem->isEnabled = isEnabled then return true         ' no repaint for a no-op
    pItem->isEnabled = isEnabled
    if isEnabled = false then
        if pIcon->nLastHotIdx = idx then pIcon->nLastHotIdx = -1
        if pIcon->nPressedIdx = idx then pIcon->CancelPress()
    end if
    PsIconPanel_InvalidateItem( pIcon, idx )
    return true
end function


' ========================================================================================
' Get / set where the whole run sits in the client area. Re-applied on every resize.
' ========================================================================================
function PsIconPanel_GetJustify( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return IP_JUSTIFY_LEFT
    return pIcon->nJustify
end function

sub PsIconPanel_SetJustify( byval hIconPanel as HWND, byval nJustify as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    if (nJustify < IP_JUSTIFY_LEFT) orelse (nJustify > IP_JUSTIFY_RIGHT) then exit sub
    pIcon->nJustify = nJustify
    pIcon->Refresh()
end sub


' ========================================================================================
' Get / set the panel-wide icon cell size (raw pixels -- the caller DPI-scales). A LAYOUT
' input: it is the width every item contributes to the run unless it overrides it.
' ========================================================================================
sub PsIconPanel_GetIconSize( byval hIconPanel as HWND, byref nIconWidth as long, byref nIconHeight as long )
    nIconWidth  = 0
    nIconHeight = 0
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    nIconWidth  = pIcon->nIconWidth
    nIconHeight = pIcon->nIconHeight
end sub

sub PsIconPanel_SetIconSize( byval hIconPanel as HWND, byval nIconWidth as long, byval nIconHeight as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    if nIconWidth  < 0 then nIconWidth  = 0
    if nIconHeight < 0 then nIconHeight = 0
    pIcon->nIconWidth  = nIconWidth
    pIcon->nIconHeight = nIconHeight
    pIcon->Refresh()
end sub


' ========================================================================================
' Get / set the panel-wide padding before and after each icon (raw pixels -- the caller
' DPI-scales). This is the author's spacing control: the layout never adjusts it.
' ========================================================================================
sub PsIconPanel_GetPadding( byval hIconPanel as HWND, byref nPadBefore as long, byref nPadAfter as long )
    nPadBefore = 0
    nPadAfter  = 0
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    nPadBefore = pIcon->nPadBefore
    nPadAfter  = pIcon->nPadAfter
end sub

sub PsIconPanel_SetPadding( byval hIconPanel as HWND, byval nPadBefore as long, byval nPadAfter as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    if nPadBefore < 0 then nPadBefore = 0
    if nPadAfter  < 0 then nPadAfter  = 0
    pIcon->nPadBefore = nPadBefore
    pIcon->nPadAfter  = nPadAfter
    pIcon->Refresh()
end sub


' ========================================================================================
' Get / set the thickness of a separator's rule (raw pixels -- the caller DPI-scales).
' ========================================================================================
function PsIconPanel_GetSeparatorWidth( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    return pIcon->nSepWidth
end function

sub PsIconPanel_SetSeparatorWidth( byval hIconPanel as HWND, byval nSepWidth as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    if nSepWidth < 0 then nSepWidth = 0
    pIcon->nSepWidth = nSepWidth
    pIcon->Refresh()
end sub


' ========================================================================================
' Override one item's padding (raw pixels). Pass -1 for either side to go back to
' inheriting the panel's value.
' ========================================================================================
function PsIconPanel_SetItemPadding( byval hIconPanel as HWND, byval idx as long, byval nPadBefore as long, byval nPadAfter as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    if nPadBefore < -1 then nPadBefore = -1
    if nPadAfter  < -1 then nPadAfter  = -1
    pIcon->GetItem(idx)->nPadBefore = nPadBefore
    pIcon->GetItem(idx)->nPadAfter  = nPadAfter
    pIcon->Refresh()
    return true
end function


' ========================================================================================
' Override one item's icon size (raw pixels). Pass 0 for either dimension to go back to
' inheriting the panel's value. On a SEPARATOR the width is ignored (its rule is
' nSepWidth wide) but the height still sets how tall the rule stands.
' ========================================================================================
function PsIconPanel_SetItemIconSize( byval hIconPanel as HWND, byval idx as long, byval nIconWidth as long, byval nIconHeight as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    if nIconWidth  < 0 then nIconWidth  = 0
    if nIconHeight < 0 then nIconHeight = 0
    pIcon->GetItem(idx)->nIconWidth  = nIconWidth
    pIcon->GetItem(idx)->nIconHeight = nIconHeight
    pIcon->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the computed client-coordinate cell rect of the specified item. Forces a
' pending layout so the result is always current. Returns FALSE for a bad index.
' ========================================================================================
function PsIconPanel_GetItemRect( byval hIconPanel as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    SetRectEmpty( @rc )
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    if pIcon->bLayoutDirty then pIcon->LayoutItems()
    rc = pIcon->GetItem(idx)->rc
    return true
end function


' ========================================================================================
' Retrieve the computed icon rect (the vertically centred glyph box) of the specified
' item. Forces a pending layout. Returns FALSE for a bad index.
' ========================================================================================
function PsIconPanel_GetItemIconRect( byval hIconPanel as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    SetRectEmpty( @rc )
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    if pIcon->bLayoutDirty then pIcon->LayoutItems()
    rc = pIcon->GetItem(idx)->rcIcon
    return true
end function


' ========================================================================================
' The summed width of every cell: what the panel would have to be for the run to exactly
' fill it. Forces a pending layout.
' ========================================================================================
function PsIconPanel_GetIdealWidth( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    if pIcon->bLayoutDirty then pIcon->LayoutItems()
    return pIcon->nTotalWidth
end function


' ========================================================================================
' Get / set the built-in painter's colors. Both copy the whole struct, so the
' change-one-color idiom is Get, assign, Set.
' ========================================================================================
sub PsIconPanel_GetColors( byval hIconPanel as HWND, byval pColors as PSICONPANEL_COLORS ptr )
    if pColors = null then exit sub
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    *pColors = pIcon->colors
end sub

sub PsIconPanel_SetColors( byval hIconPanel as HWND, byval pColors as PSICONPANEL_COLORS ptr )
    if pColors = null then exit sub
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then exit sub
    pIcon->colors = *pColors
    if pIcon->hWin then InvalidateRect( pIcon->hWin, NULL, TRUE )
end sub


' ========================================================================================
' Override one item's IDLE glyph color. Hot, selected and disabled keep the panel's
' colors, so a hover always speaks the panel's language.
' ========================================================================================
function PsIconPanel_SetItemForeColor( byval hIconPanel as HWND, byval idx as long, byval clr as COLORREF ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    pIcon->GetItem(idx)->ForeColorItem = clr
    pIcon->GetItem(idx)->bHasForeColor = true
    PsIconPanel_InvalidateItem( pIcon, idx )
    return true
end function

function PsIconPanel_ClearItemForeColor( byval hIconPanel as HWND, byval idx as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    pIcon->GetItem(idx)->ForeColorItem = 0
    pIcon->GetItem(idx)->bHasForeColor = false
    PsIconPanel_InvalidateItem( pIcon, idx )
    return true
end function


' ========================================================================================
' Retrieve the font used to draw the glyphs.
' ========================================================================================
function PsIconPanel_GetFont( byval hIconPanel as HWND ) as HFONT
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    return pIcon->hIconFont
end function


' ========================================================================================
' Set the font used to draw the glyphs (normally Segoe Fluent Icons). The caller owns the
' HFONT and is responsible for destroying it; the control only stores the handle.
'
' Unlike every text-driven sibling this does NOT re-lay-out: cell widths are declared, not
' measured, so the font is a paint-time input only. Pick a size that fits the icon cell --
' nothing here measures the glyph, so an oversized font simply clips.
' ========================================================================================
function PsIconPanel_SetFont( byval hIconPanel as HWND, byval hIconFont as HFONT ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    pIcon->hIconFont = hIconFont
    if pIcon->hWin then InvalidateRect( pIcon->hWin, NULL, TRUE )
    return true
end function


' ========================================================================================
' Retrieve the handle of the panel's tooltip control.
' ========================================================================================
' The comctl32 handle, and 0 while on PsTooltip -- the honest answer, since a TTM_* sent
' to a PsTooltip window is silently ignored.
function PsIconPanel_GetTooltipHandle( byval hIconPanel as HWND ) as HWND
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return 0
    return PsTipHost_SysHandle( @pIcon->tip )
end function


' The PsTooltip window, and 0 while on the system backend. This is the door to PsTooltip's
' own SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph, none of which is mirrored
' on this control.
function PsIconPanel_GetPsTooltipHandle( byval hIconPanel as HWND ) as HWND
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then return 0
    return PsTipHost_PsHandle( @pIcon->tip )
end function

function PsIconPanel_SetTooltipMode( byval hIconPanel as HWND, byval nMode as long ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then return false
    return PsTipHost_SetMode( @pIcon->tip, nMode )
end function

function PsIconPanel_GetTooltipMode( byval hIconPanel as HWND ) as long
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pIcon->tip )
end function

' The three delays go through PsTipHost, which STORES them as well as pushing them, so a
' hover time set before a mode switch is still in force after it. Pushing straight at the
' live tooltip loses it at the switch -- and the tip still appears, just on a different
' beat, with nothing reporting the change.
sub PsIconPanel_SetHoverTime( byval hIconPanel as HWND, byval milliseconds as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then exit sub
    ' The field is STILL READ by TrackMouseEvent as dwHoverTime, so this setter
    ' drives the hot-tracking dwell as well as the tooltip. Dropping it here would
    ' silently change when the control considers itself hot.
    pIcon->HoverTime = milliseconds
    PsTipHost_SetDelay( @pIcon->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsIconPanel_SetAutoPopTime( byval hIconPanel as HWND, byval milliseconds as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then exit sub
    PsTipHost_SetDelay( @pIcon->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsIconPanel_SetReshowTime( byval hIconPanel as HWND, byval milliseconds as long )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = 0 then exit sub
    PsTipHost_SetDelay( @pIcon->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Get / set an item's own tooltip text. "" means "ask the tooltip callback".
' ========================================================================================
function PsIconPanel_GetTooltipText( byval hIconPanel as HWND, byval idx as long ) as DWSTRING
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return ""
    if pIcon->IsValidItem(idx) = false then return ""
    dim as DWSTRING wszText = pIcon->GetItem(idx)->wszTooltip
    return wszText
end function

function PsIconPanel_SetTooltipText( byval hIconPanel as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return false
    if pIcon->IsValidItem(idx) = false then return false
    pIcon->GetItem(idx)->wszTooltip = Text
    return true
end function


' Resolve the tooltip text for an item: the item's own text if it has any, else the host
' callback. No caption fallback -- an icon has no caption.
private function PsIconPanel_ResolveTooltipText( byval pIcon as PSICONPANEL ptr, byval idx as long ) as DWSTRING
    if pIcon = null then return ""
    if pIcon->IsValidItem(idx) = false then return ""
    dim as DWSTRING wszText = pIcon->items(idx).wszTooltip
    if len(wszText) then return wszText
    if pIcon->TooltipCallback then return pIcon->TooltipCallback( pIcon->hWin, idx )
    return ""
end function


' ========================================================================================
' Callback setters. See the type declarations in PsIconPanel.bi for each contract.
' ========================================================================================
sub PsIconPanel_SetPaintItemCallback( byval hIconPanel as HWND, byval usersub as IP_PaintItemCallbackSub )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return
    pIcon->PaintItemCallback = usersub
end sub

sub PsIconPanel_SetMessageCallback( byval hIconPanel as HWND, byval userfunc as IP_MessageCallbackFunc )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return
    pIcon->MessageCallback = userfunc
end sub

sub PsIconPanel_SetTooltipCallback( byval hIconPanel as HWND, byval userfunc as IP_TooltipCallbackFunc )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return
    pIcon->TooltipCallback = userfunc
end sub

sub PsIconPanel_SetClickCallback( byval hIconPanel as HWND, byval usersub as IP_ClickCallbackSub )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return
    pIcon->ClickCallback = usersub
end sub

sub PsIconPanel_SetSelChangeCallback( byval hIconPanel as HWND, byval usersub as IP_SelChangeCallbackSub )
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hIconPanel)
    if pIcon = null then return
    pIcon->SelChangeCallback = usersub
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: PsIconPanel
' ========================================================================================
function PsIconPanel_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' There are no child windows to position. The client width decides where the
        ' justified run sits, so a resize invalidates the layout -- and unlike the
        ' siblings, that IS the whole of the resize response.
        dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hwnd)
        if pIcon then pIcon->Refresh()
    end if
    function = 0
end function


' ========================================================================================
' The built-in painter: draws one item when no PaintItemCallback is set.
'
' State precedence, decided once here so every renderer agrees:
'   disabled > pressed > hot > selected > idle
' A disabled item is never hot or pressed (GetHotIconPanelItem refuses it), so the first
' rung is unreachable from the others rather than merely winning over them.
'
' Pressed reuses the SELECT color pair rather than adding a ninth color: on a COMMAND item
' it is the momentary flash that makes the icon feel like a button, and on a TOGGLE it
' previews the latched look the release is about to produce.
' ========================================================================================
private sub PsIconPanel_PaintItemDefault( _
            byval pIcon as PSICONPANEL ptr, _
            byval b as PsBufferPaint ptr, _
            byval idx as long, _
            byval isHot as boolean, _
            byval isPressed as boolean _
            )
    if (pIcon = null) orelse (b = null) then exit sub

    with pIcon->items(idx)
        if .itemKind = IP_KIND_SEPARATOR then
            b->SetBackColor( pIcon->colors.BackColor )
            b->PaintRect( @.rc )
            b->SetBackColor( pIcon->colors.SeparatorColor )
            b->PaintRect( @.rcIcon )     ' the rule: sepWidth wide, icon-height tall
            exit sub
        end if

        dim as COLORREF backclr = pIcon->colors.BackColor
        dim as COLORREF foreclr = pIcon->colors.ForeColor
        if .isEnabled = false then
            foreclr = pIcon->colors.ForeColorDisabled
        elseif isPressed then
            backclr = pIcon->colors.BackColorSelect
            foreclr = pIcon->colors.ForeColorSelect
        elseif isHot then
            backclr = pIcon->colors.BackColorHot
            foreclr = pIcon->colors.ForeColorHot
        elseif .isSelected then
            backclr = pIcon->colors.BackColorSelect
            foreclr = pIcon->colors.ForeColorSelect
        elseif .bHasForeColor then
            foreclr = .ForeColorItem
        end if

        b->SetColors( foreclr, backclr )
        b->PaintRect( @.rc )             ' fill the WHOLE cell, padding included

        ' Image first, else glyph. Both draw into the same declared icon cell (rcIcon).
        if (.pImage <> 0) andalso (.pImage->IsValid() <> 0) then
            b->PaintImage( .pImage->Image(), @.rcIcon, PS_IMGFIT_ASPECT )
        elseif len(.wszGlyph) then
            b->SetFont( pIcon->hIconFont )
            ' PaintText already adds DT_VCENTER / DT_SINGLELINE / DT_NOPREFIX.
            b->PaintText( .wszGlyph, @.rcIcon, DT_CENTER )
        end if
    end with
end sub


' ========================================================================================
' Process WM_PAINT message for window/dialog: PsIconPanel
' ========================================================================================
function PsIconPanel_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer(hwnd)
    if pIcon = null then return 0

    ' Consume any pending layout before painting. This is what makes a burst of Add calls
    ' cost one layout pass instead of N.
    if pIcon->bLayoutDirty then pIcon->LayoutItems()

    ' Capture the update rect BEFORE BeginDoubleBuffer -- BeginPaint clears it. This is a
    ' CONSERVATIVE approximation and correctness never depends on it: GetUpdateRect returns
    ' the BOUNDING BOX of the update region, so it only ever skips items that could not
    ' show a pixel anyway.
    dim as RECT rcUpdate
    if GetUpdateRect( hwnd, @rcUpdate, FALSE ) = 0 then GetClientRect( hwnd, @rcUpdate )

    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pIcon->colors.BackColor )

    ' Fill the whole panel, which also covers everything outside the run -- with three
    ' justifications that is usually most of the client area.
    b.PaintClientRect()

    ' Skipping items outside the update rect is safe even though EndDoubleBuffer blits the
    ' WHOLE client: BeginPaint's DC is clipped to the update region, so the skipped
    ' (background-filled) areas never reach the screen.
    dim p as PSICONPANEL_PAINTINFO
    p.hIconPanel = hwnd
    p.b          = @b

    for i as long = 0 to pIcon->itemCount - 1
        dim as RECT rcHit
        if IntersectRect( @rcHit, @pIcon->items(i).rc, @rcUpdate ) = 0 then continue for

        dim as boolean isHot     = (i = pIcon->nLastHotIdx)
        ' "The press is still on this item" is exactly "the pressed item is also the hot
        ' one" -- hover tracking maintains that through the capture, so no second flag.
        dim as boolean isPressed = (i = pIcon->nPressedIdx) andalso (pIcon->nLastHotIdx = pIcon->nPressedIdx)

        if pIcon->PaintItemCallback then
            p.itemID     = i
            p.itemKind   = pIcon->items(i).itemKind
            p.rc         = pIcon->items(i).rc
            p.rcIcon     = pIcon->items(i).rcIcon
            p.isHot      = isHot
            p.isSelected = pIcon->items(i).isSelected
            p.isPressed  = isPressed
            p.isEnabled  = pIcon->items(i).isEnabled
            p.wszGlyph   = pIcon->items(i).wszGlyph
            dim as PSICONPANEL_ITEM ptr pit = @pIcon->items(i)
            p.pImage     = iif( (pit->pImage <> 0) andalso (pit->pImage->IsValid() <> 0), pit->pImage->Image(), cast( CGpImage ptr, 0 ) )
            pIcon->PaintItemCallback( @p )
        else
            PsIconPanel_PaintItemDefault( pIcon, @b, i, isHot, isPressed )
        end if
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Clear any hot-tracked item and stop the safety-net timer. Repaints only if an item was
' actually hot, so this is cheap to call repeatedly.
'
' Note this also changes how a live press LOOKS (isPressed is derived from the hot index),
' which is exactly right: the cursor left the item, so the press must stop looking armed.
' The gesture itself survives -- only a release or a capture loss ends it.
' ========================================================================================
private sub PsIconPanel_ClearHotTracking( byval pIcon as PSICONPANEL ptr, byval hIconPanel as HWND )
    if pIcon = null then exit sub
    if pIcon->hotTimerOn then
        KillTimer( hIconPanel, IDT_CICONPANEL_HOTTRACK )
        pIcon->hotTimerOn = false
    end if
    if pIcon->nLastHotIdx <> -1 then
        pIcon->nLastHotIdx = -1
        AfxRedrawWindow( hIconPanel )
    end if
end sub


' ========================================================================================
' PsIconPanel Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc.
'
' The control TAKES mouse capture on a press. Unlike PsStatusBar (whose "no capture" note
' must not be inherited here), something does consume the guaranteed WM_LBUTTONDOWN ->
' WM_LBUTTONUP pairing: the press/cancel gesture. Press an icon, slide off it, release --
' nothing fires. That requires knowing the press is still live while the cursor is
' elsewhere, which is what capture buys. The full price is paid, in the order the family
' learned to pay it:
'   - WM_LBUTTONUP releases capture FIRST and unconditionally, before any callback runs;
'   - the message callback's result is IGNORED for WM_LBUTTONUP, so it cannot strand it;
'   - WM_CAPTURECHANGED cancels the press (anything can steal capture at any time);
'   - WM_DESTROY releases it if we still hold it.
'
' There is deliberately NO CS_DBLCLKS on this window class. PsStatusBar sets it, and copying
' that here would be a bug: with double-click messages enabled the second of two rapid
' clicks arrives as WM_LBUTTONDBLCLK instead of WM_LBUTTONDOWN, so every other click on a
' toggle would be swallowed. A toolbar has no double-click semantic to gain in exchange.
' ========================================================================================
function PsIconPanel_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSICONPANEL ptr pIcon = GetIconPanelPointer( hwnd )

    ' Per-message scratch info passed to the user MessageCallback. A plain local (NOT
    ' static) so that multiple control instances never share it. The hover tracking index
    ' likewise lives per-instance in pIcon->nLastHotIdx.
    dim m as PSICONPANEL_MESSAGEINFO

    select case uMsg
        ' Prepare the callback MessageInfo for only the messages that we hand to the host.
        case WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP
            with m
                .hIconPanel = hwnd
                .uMsg       = uMsg
                .wParam     = wParam
                .lParam     = lParam
                .idx        = -1
            end with
    end select

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        PsIconPanel_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       PsIconPanel_OnPaint)

    case WM_TIMER
        if (pIcon <> 0) andalso (wParam = IDT_CICONPANEL_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no
            ' longer over our client area, clear the hot item ourselves.
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                PsIconPanel_ClearHotTracking( pIcon, hwnd )
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        ' Track that we are over the control in order to catch the eventual
        ' WM_MOUSEHOVER and WM_MOUSELEAVE events.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 250    ' HOVER_DEFAULT is 500
        if pIcon then tme.dwHoverTime = pIcon->HoverTime
        TrackMouseEvent(@tme)

        ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
        ' quickly (or onto another control), which would strand the hot item painted.
        ' Poll the cursor while we are over the control.
        if (pIcon <> 0) andalso (pIcon->hotTimerOn = false) then
            SetTimer( hwnd, IDT_CICONPANEL_HOTTRACK, PSICONPANEL_HOTTRACK_MS, NULL )
            pIcon->hotTimerOn = true
        end if

        ' Repaint only the items whose state changed, not the whole panel. This is also
        ' what keeps a live press's appearance current: isPressed is derived from the hot
        ' index, so arming and disarming it costs nothing extra here.
        if pIcon then
            dim as long idx = GetHotIconPanelItem(hwnd)
            if idx <> pIcon->nLastHotIdx then
                PsIconPanel_InvalidateItem( pIcon, idx )                 ' newly hot
                PsIconPanel_InvalidateItem( pIcon, pIcon->nLastHotIdx )  ' previously hot
                pIcon->nLastHotIdx = idx
                ' Item changed. On the system backend this pops the tip so it re-queries
                ' TTN_GETDISPINFOW for the new item; on PsTooltip it moves the tool index,
                ' which hides a visible tip and re-resolves on the next dwell. One call site
                ' either way -- and the index is what makes the PsTooltip backend able to
                ' report WHICH item a tip is for, which comctl32 never knew.
                PsTipHost_SetToolIndex( @pIcon->tip, idx )
            end if
            m.idx = idx
            if pIcon->MessageCallback then
                if pIcon->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEHOVER
        if pIcon then
            m.idx = GetHotIconPanelItem(hwnd)
            if pIcon->MessageCallback then
                if pIcon->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pIcon then
            ' Fast path for clearing the hot item (the WM_TIMER poll is the fallback for
            ' when this message never arrives). The tooltip hides itself when the mouse
            ' leaves the tool, so nothing to push. m.idx stays -1: the mouse is gone.
            PsIconPanel_ClearHotTracking( pIcon, hwnd )
            if pIcon->MessageCallback then
                if pIcon->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_LBUTTONDOWN
        if pIcon then
            ' GetHotIconPanelItem already rejects separators and disabled items, so this
            ' index is always something that can legitimately be pressed.
            dim as long idx = GetHotIconPanelItem(hwnd)
            m.idx = idx
            if pIcon->MessageCallback then
                ' Suppressing the press is allowed: no capture has been taken yet, so
                ' there is no bookkeeping to strand.
                if pIcon->MessageCallback( @m ) then return 0
            end if
            ' No SetFocus: a toolbar must not steal focus from the host's editor.
            if idx <> -1 then
                SetCapture( hwnd )
                pIcon->nPressedIdx = idx
                PsIconPanel_InvalidateItem( pIcon, idx )
            end if
            return 0
        end if

    case WM_LBUTTONUP
        if pIcon then
            ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the
            ' window losing capture SYNCHRONOUSLY, and our handler for it cancels the
            ' press -- so reading nPressedIdx after the release would always find -1 and
            ' no click would ever fire.
            dim as long idxPressed = pIcon->nPressedIdx
            ' Release capture unconditionally, before any callback runs. Nothing below --
            ' not a callback, not a host handler -- gets the chance to leave it held.
            if GetCapture() = hwnd then ReleaseCapture()

            dim as long idx = GetHotIconPanelItem(hwnd)
            pIcon->CancelPress()
            PsIconPanel_InvalidateItem( pIcon, idxPressed )

            m.idx = idx
            ' The result is deliberately IGNORED here (see IP_MessageCallbackFunc).
            if pIcon->MessageCallback then pIcon->MessageCallback( @m )

            ' Only a matched press+release on the SAME item acts. Press, slide off,
            ' release = nothing happened, which is the whole reason for the capture.
            if (idxPressed <> -1) andalso (idx = idxPressed) then
                dim as PSICONPANEL_ITEM ptr pItem = pIcon->GetItem( idxPressed )
                ' The item set is static, so this pointer cannot have been invalidated by
                ' an insert or delete inside a callback -- but a callback CAN still have
                ' disabled the item mid-gesture, so that is re-checked below.
                if pItem <> null then
                    if pItem->isEnabled then
                        select case pItem->itemKind
                        case IP_KIND_TOGGLE
                            dim as boolean bNewState = not pItem->isSelected
                            pItem->isSelected = bNewState
                            PsIconPanel_InvalidateItem( pIcon, idxPressed )
                            ' Snapshot before the callback: it may delete items, which
                            ' would leave pItem dangling.
                            if pIcon->SelChangeCallback then
                                pIcon->SelChangeCallback( hwnd, idxPressed, bNewState )
                            end if
                        case IP_KIND_COMMAND
                            dim as long nId = pItem->id
                            if pIcon->ClickCallback then
                                pIcon->ClickCallback( hwnd, idxPressed, nId )
                            end if
                        end select
                    end if
                end if
            end if
            return 0
        end if

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell).
        ' The gesture is over; forget it and repaint the item that was armed.
        if pIcon then
            dim as long idxPressed = pIcon->nPressedIdx
            pIcon->CancelPress()
            PsIconPanel_InvalidateItem( pIcon, idxPressed )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu).
        ' No capture is taken for the right button, so an up can arrive without a down.
        if pIcon then
            m.idx = GetHotIconPanelItem(hwnd)
            if pIcon->MessageCallback then
                if pIcon->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot item's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pIcon then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' store in an instance-lifetime buffer and point lpszText at it
                pIcon->wszTooltip = PsIconPanel_ResolveTooltipText( pIcon, pIcon->nLastHotIdx )
                pdi->lpszText = cast( LPWSTR, pIcon->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pIcon then
            ' Don't let the hot-tracking timer outlive the window...
            if pIcon->hotTimerOn then
                KillTimer( hwnd, IDT_CICONPANEL_HOTTRACK )
                pIcon->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pIcon->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored icon panel data pointer
        if pIcon then
            PsTipHost_Destroy( @pIcon->tip )
            ' Free any per-item images we own (raw pointers, not auto-freed by delete pIcon).
            for i as long = 0 to pIcon->itemCount - 1
                if pIcon->items(i).pImage then delete pIcon->items(i).pImage : pIcon->items(i).pImage = 0
            next
            delete(pIcon)
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
' Supply the tooltip text to the PsTooltip backend.
'
' This thunk is why PsTipHost never learns the text policy: PsIconPanel_ResolveTooltipText
' stays the ONE place that decides what a tip says, and the system backend reaches the
' same function by the other road (comctl32 asking through TTN_GETDISPINFOW), which is
' why that arm stays in the WndProc.
' ========================================================================================
private function PsIconPanel_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                        byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsIconPanel_ResolveTooltipText( GetIconPanelPointer(hAttached), idx )
    return wszOut
end function


' ========================================================================================
' PsIconPanel_Create
' ========================================================================================
function PsIconPanel_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsIconPanel_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' NO CS_DBLCLKS here -- see the WndProc header. It would swallow every second rapid
    ' click on a toggle, and this control has nothing to do with a double-click.

    ' Define the TYPE that holds all the information about this custom icon panel.
    dim as PSICONPANEL ptr pIcon = new PSICONPANEL
    pIcon->hWin          = hCtrl
    pIcon->idc_IconPanel = CtrlID
    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes
    ' raw pixels and the caller scales (the family rule).
    pIcon->nIconWidth  = pWindow->ScaleX( PSICONPANEL_DEFAULT_ICONWIDTH )
    pIcon->nIconHeight = pWindow->ScaleY( PSICONPANEL_DEFAULT_ICONHEIGHT )
    pIcon->nPadBefore  = pWindow->ScaleX( PSICONPANEL_DEFAULT_PADBEFORE )
    pIcon->nPadAfter   = pWindow->ScaleX( PSICONPANEL_DEFAULT_PADAFTER )
    pIcon->nSepWidth   = pWindow->ScaleX( PSICONPANEL_DEFAULT_SEPWIDTH )
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the icon panel window
    pWindow->UserData(0) = cast(LONG_PTR, pIcon)

    ' AFTER UserData, not before: PsIconPanel_TipTextCallback resolves the control through
    ' GetIconPanelPointer(hAttached), which reads exactly that slot.
    PsTipHost_Ensure( @pIcon->tip, hCtrl, @PsIconPanel_TipTextCallback )


    function = hCtrl

end function
