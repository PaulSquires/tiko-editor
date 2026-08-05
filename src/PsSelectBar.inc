
''
''  PsSelectBar.inc
''

#include once "PsSelectBar.bi"


function GetSelectBarPointer( byval hSelectBar as HWND ) as PSSELECTBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hSelectBar)
    dim as PSSELECTBAR ptr pSel = null
    if pWindow then pSel = cast( PSSELECTBAR ptr, pWindow->UserData(0) )
    return pSel
end function


' Which panel is the cursor over, for HOVER and CLICK purposes? Returns a model index, or
' -1 for none. This is the public HitTest plus two extra rules that belong to interaction
' and not to geometry:
'   - the cursor must be inside the client area. Cells can legitimately run PAST the right
'     edge when the run overflows, and without this test a cursor over the sibling window
'     next door would light one of them up.
'   - a DISABLED panel is never hot and never pressable.
private function GetHotSelectBarPanel( byval hSelectBar as HWND ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return -1

    dim as POINT pt
    GetCursorPos( @pt )
    ScreenToClient( hSelectBar, @pt )

    dim as RECT rcClient
    GetClientRect( hSelectBar, @rcClient )
    if PtInRect( @rcClient, pt ) = 0 then return -1

    dim as long idx = pSel->HitTest( pt.x, pt.y )
    if idx = -1 then return -1
    if pSel->panels(idx).isEnabled = false then return -1
    return idx
end function


' Repaint one panel. Silently does nothing for an invalid index, which is what makes the
' hover path safe when the cursor is over empty bar area (idx = -1). FALSE = no background
' erase; the panel's whole cell is painted every time.
private sub PsSelectBar_InvalidatePanel( byval pSel as PSSELECTBAR ptr, byval idx as long )
    if pSel = null then exit sub
    if pSel->IsValidPanel(idx) = false then exit sub
    InvalidateRect( pSel->hWin, @pSel->panels(idx).rc, FALSE )
end sub


' ========================================================================================
' Append a panel. The FIRST panel added silently becomes the current one. Returns the new
' panel's index, or -1.
' ========================================================================================
function PsSelectBar_AddPanel( byval hSelectBar as HWND, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return -1
    dim as PSSELECTBAR_PANEL ptr pPanel = pSel->AddPanel()
    if pPanel = null then return -1
    pPanel->wszText  = Text
    pPanel->id       = id
    pPanel->itemData = itemData
    return pSel->GetCount() - 1  ' model index of the appended panel
end function


' ========================================================================================
' Remove all panels. For a FULL REBUILD only -- see the header's static contract.
' ========================================================================================
sub PsSelectBar_Clear( byval hSelectBar as HWND )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->Clear()
end sub


' ========================================================================================
' Refresh the custom select bar control.
' ========================================================================================
sub PsSelectBar_Refresh( byval hSelectBar as HWND )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel then pSel->Refresh()
end sub


' ========================================================================================
' Retrieve the current panel count in the custom select bar control.
' ========================================================================================
function PsSelectBar_GetCount( byval hSelectBar as HWND ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    return pSel->GetCount()
end function


' ========================================================================================
' Is idx a valid panel index?
' ========================================================================================
function PsSelectBar_IsValidPanel( byval hSelectBar as HWND, byval idx as long ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    return pSel->IsValidPanel( idx )
end function


' ========================================================================================
' Index of the FIRST panel carrying this id, or -1.
' ========================================================================================
function PsSelectBar_FindPanelByID( byval hSelectBar as HWND, byval id as long ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return -1
    for i as long = 0 to pSel->panelCount - 1
        if pSel->panels(i).id = id then return i
    next
    return -1
end function


' ========================================================================================
' Which panel is at this CLIENT-coordinate point? Forces a pending layout so the answer is
' always current. Disabled panels are included (they occupy their cell, and a caller asking
' "what is here" deserves the truth).
' ========================================================================================
function PsSelectBar_HitTest( byval hSelectBar as HWND, byval x as long, byval y as long ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return -1
    if pSel->bLayoutDirty then pSel->LayoutPanels()
    return pSel->HitTest( x, y )
end function


' ========================================================================================
' Retrieve a panel's label.
' ========================================================================================
function PsSelectBar_GetText( byval hSelectBar as HWND, byval idx as long ) as DWSTRING
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return ""
    if pSel->IsValidPanel(idx) = false then return ""
    dim as DWSTRING wszText = pSel->GetPanel(idx)->wszText
    return wszText
end function


' ========================================================================================
' Set a panel's label. The text drives the cell width, so this RE-MEASURES and re-lays out:
' every panel to its right moves. A design-time operation -- see the static contract.
' ========================================================================================
function PsSelectBar_SetText( byval hSelectBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    pSel->GetPanel(idx)->wszText = Text
    pSel->Refresh()
    return true
end function


' ========================================================================================
' Get / set a panel's host id.
' ========================================================================================
function PsSelectBar_GetPanelID( byval hSelectBar as HWND, byval idx as long ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    if pSel->IsValidPanel(idx) = false then return 0
    return pSel->GetPanel(idx)->id
end function

function PsSelectBar_SetPanelID( byval hSelectBar as HWND, byval idx as long, byval id as long ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    pSel->GetPanel(idx)->id = id
    return true
end function


' ========================================================================================
' Get / set a panel's free-form host payload.
' ========================================================================================
function PsSelectBar_GetItemData( byval hSelectBar as HWND, byval idx as long ) as integer
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    if pSel->IsValidPanel(idx) = false then return 0
    return pSel->GetPanel(idx)->itemData
end function

function PsSelectBar_SetItemData( byval hSelectBar as HWND, byval idx as long, byval itemData as integer ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    pSel->GetPanel(idx)->itemData = itemData
    return true
end function


' ========================================================================================
' Is this panel enabled?
' ========================================================================================
function PsSelectBar_GetEnabled( byval hSelectBar as HWND, byval idx as long ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    return pSel->GetPanel(idx)->isEnabled
end function


' ========================================================================================
' Enable / disable a panel.
'
' REFUSED for the panel that is currently current: returns FALSE and changes nothing.
' Together with SetCurSel's rejection of disabled panels, that is what guarantees the
' current panel is always enabled -- and therefore that the indicator is always visible
' somewhere. Switch away first, then disable.
'
' A disabled panel greys out, stops answering hover, and swallows clicks -- so disabling the
' panel the cursor is on, or the one under a live press, must clear that state here rather
' than wait for the next mouse move.
' ========================================================================================
function PsSelectBar_SetEnabled( byval hSelectBar as HWND, byval idx as long, byval isEnabled as boolean ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if (isEnabled = false) andalso (idx = pSel->nCurSel) then return false
    dim as PSSELECTBAR_PANEL ptr pPanel = pSel->GetPanel(idx)
    if pPanel->isEnabled = isEnabled then return true         ' no repaint for a no-op
    pPanel->isEnabled = isEnabled
    if isEnabled = false then
        if pSel->nLastHotIdx = idx then pSel->nLastHotIdx = -1
        if pSel->nPressedIdx = idx then pSel->CancelPress()
    end if
    PsSelectBar_InvalidatePanel( pSel, idx )
    return true
end function


' ========================================================================================
' Which panel is current? -1 only while the bar is empty.
' ========================================================================================
function PsSelectBar_GetCurSel( byval hSelectBar as HWND ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return -1
    return pSel->nCurSel
end function


' ========================================================================================
' Make a panel current. SILENT -- no SelChangeCallback, because only the user fires that
' (the family rule); this also makes it safe to call from inside the change handler.
'
' Rejects -1 (there is no "nothing selected" state in this control) and rejects a disabled
' panel. Repaints only the two panels involved.
' ========================================================================================
function PsSelectBar_SetCurSel( byval hSelectBar as HWND, byval idx as long ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if pSel->panels(idx).isEnabled = false then return false
    if pSel->nCurSel = idx then return true                   ' already current
    dim as long idxOld = pSel->nCurSel
    pSel->nCurSel = idx
    PsSelectBar_InvalidatePanel( pSel, idxOld )
    PsSelectBar_InvalidatePanel( pSel, idx )
    return true
end function


' ========================================================================================
' Get / set the bar-wide padding added to the left and right of each panel's text (raw
' pixels -- the caller DPI-scales). A LAYOUT input: it is what a panel's width is measured
' against.
' ========================================================================================
sub PsSelectBar_GetPadding( byval hSelectBar as HWND, byref nPadLeft as long, byref nPadRight as long )
    nPadLeft  = 0
    nPadRight = 0
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    nPadLeft  = pSel->nPadLeft
    nPadRight = pSel->nPadRight
end sub

sub PsSelectBar_SetPadding( byval hSelectBar as HWND, byval nPadLeft as long, byval nPadRight as long )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    if nPadLeft  < 0 then nPadLeft  = 0
    if nPadRight < 0 then nPadRight = 0
    pSel->nPadLeft  = nPadLeft
    pSel->nPadRight = nPadRight
    pSel->Refresh()
end sub


' ========================================================================================
' Override one panel's padding (raw pixels). Pass -1 for either side to go back to
' inheriting the bar's value. Use it on panel 0 to set the bar's lead-in indent.
' ========================================================================================
function PsSelectBar_SetPanelPadding( byval hSelectBar as HWND, byval idx as long, byval nPadLeft as long, byval nPadRight as long ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if nPadLeft  < -1 then nPadLeft  = -1
    if nPadRight < -1 then nPadRight = -1
    pSel->GetPanel(idx)->nPadLeft  = nPadLeft
    pSel->GetPanel(idx)->nPadRight = nPadRight
    pSel->Refresh()
    return true
end function


' ========================================================================================
' Get / set the indicator line's thickness and the gap between the text and the line (raw
' pixels -- the caller DPI-scales).
'
' BOTH ARE LAYOUT INPUTS, not just cosmetics: the centred block is text + gap + thickness,
' so changing either moves the TEXT as well as the line. That is deliberate -- it is what
' keeps the space above the text and below the line balanced.
' ========================================================================================
sub PsSelectBar_GetIndicator( byval hSelectBar as HWND, byref nThickness as long, byref nGap as long )
    nThickness = 0
    nGap       = 0
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    nThickness = pSel->nIndicatorHeight
    nGap       = pSel->nIndicatorGap
end sub

sub PsSelectBar_SetIndicator( byval hSelectBar as HWND, byval nThickness as long, byval nGap as long )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    if nThickness < 0 then nThickness = 0
    if nGap       < 0 then nGap       = 0
    pSel->nIndicatorHeight = nThickness
    pSel->nIndicatorGap    = nGap
    pSel->Refresh()
end sub


' ========================================================================================
' Retrieve the computed rects of the specified panel. Each forces a pending layout so the
' result is always current, and returns FALSE for a bad index.
'   GetPanelRect          - the full cell (padding included): the fill and hit-test rect
'   GetPanelTextRect      - exactly the measured text box
'   GetPanelIndicatorRect - where the line goes, whether or not this panel is current
' ========================================================================================
function PsSelectBar_GetPanelRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    SetRectEmpty( @rc )
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if pSel->bLayoutDirty then pSel->LayoutPanels()
    rc = pSel->GetPanel(idx)->rc
    return true
end function

function PsSelectBar_GetPanelTextRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    SetRectEmpty( @rc )
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if pSel->bLayoutDirty then pSel->LayoutPanels()
    rc = pSel->GetPanel(idx)->rcText
    return true
end function

function PsSelectBar_GetPanelIndicatorRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    SetRectEmpty( @rc )
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    if pSel->bLayoutDirty then pSel->LayoutPanels()
    rc = pSel->GetPanel(idx)->rcIndicator
    return true
end function


' ========================================================================================
' The summed width of every cell: what the bar would have to be for the run to exactly fill
' it. Forces a pending layout, and is valid before the bar has ever been sized.
' ========================================================================================
function PsSelectBar_GetIdealWidth( byval hSelectBar as HWND ) as long
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    if pSel->bLayoutDirty then pSel->LayoutPanels()
    return pSel->nTotalWidth
end function


' ========================================================================================
' Get / set the built-in painter's colors. Both copy the whole struct, so the
' change-one-color idiom is Get, assign, Set.
' ========================================================================================
sub PsSelectBar_GetColors( byval hSelectBar as HWND, byval pColors as PSSELECTBAR_COLORS ptr )
    if pColors = null then exit sub
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    *pColors = pSel->colors
end sub

sub PsSelectBar_SetColors( byval hSelectBar as HWND, byval pColors as PSSELECTBAR_COLORS ptr )
    if pColors = null then exit sub
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then exit sub
    pSel->colors = *pColors
    if pSel->hWin then InvalidateRect( pSel->hWin, NULL, TRUE )
end sub


' ========================================================================================
' Retrieve the font used to measure and draw panel text.
' ========================================================================================
function PsSelectBar_GetFont( byval hSelectBar as HWND ) as HFONT
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    return pSel->hPanelFont
end function


' ========================================================================================
' Set the font used to measure and draw panel text. The caller owns the HFONT and is
' responsible for destroying it; the control only stores the handle.
'
' This RE-LAYS-OUT, because the font is what panel widths are measured with AND what the
' block height comes from. (PsIconPanel's SetFont only repaints -- its cells are declared
' rather than measured. That difference does not survive a copy between the two.)
' ========================================================================================
function PsSelectBar_SetFont( byval hSelectBar as HWND, byval hPanelFont as HFONT ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    pSel->hPanelFont = hPanelFont
    pSel->Refresh()
    return true
end function


' ========================================================================================
' Retrieve the handle of the bar's tooltip control.
' ========================================================================================
function PsSelectBar_GetTooltipHandle( byval hSelectBar as HWND ) as HWND
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return 0
    return pSel->hTooltip
end function


' ========================================================================================
' Set the tooltip hovertime for the custom select bar control.
' ========================================================================================
sub PsSelectBar_SetHoverTime( byval hSelectBar as HWND, byval milliseconds as long )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->HoverTime = milliseconds
end sub


' ========================================================================================
' Get / set a panel's own tooltip text. "" means "ask the tooltip callback".
' ========================================================================================
function PsSelectBar_GetTooltipText( byval hSelectBar as HWND, byval idx as long ) as DWSTRING
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return ""
    if pSel->IsValidPanel(idx) = false then return ""
    dim as DWSTRING wszText = pSel->GetPanel(idx)->wszTooltip
    return wszText
end function

function PsSelectBar_SetTooltipText( byval hSelectBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return false
    if pSel->IsValidPanel(idx) = false then return false
    pSel->GetPanel(idx)->wszTooltip = Text
    return true
end function


' Resolve the tooltip text for a panel: the panel's own text if it has any, else the host
' callback. No caption fallback -- the caption is already on screen.
private function PsSelectBar_ResolveTooltipText( byval pSel as PSSELECTBAR ptr, byval idx as long ) as DWSTRING
    if pSel = null then return ""
    if pSel->IsValidPanel(idx) = false then return ""
    dim as DWSTRING wszText = pSel->panels(idx).wszTooltip
    if PsLen(wszText) then return wszText
    if pSel->TooltipCallback then return pSel->TooltipCallback( pSel->hWin, idx )
    return ""
end function


' ========================================================================================
' Callback setters. See the type declarations in PsSelectBar.bi for each contract.
' ========================================================================================
sub PsSelectBar_SetPaintPanelCallback( byval hSelectBar as HWND, byval usersub as SEL_PaintPanelCallbackSub )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->PaintPanelCallback = usersub
end sub

sub PsSelectBar_SetMessageCallback( byval hSelectBar as HWND, byval userfunc as SEL_MessageCallbackFunc )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->MessageCallback = userfunc
end sub

sub PsSelectBar_SetTooltipCallback( byval hSelectBar as HWND, byval userfunc as SEL_TooltipCallbackFunc )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->TooltipCallback = userfunc
end sub

sub PsSelectBar_SetSelChangeCallback( byval hSelectBar as HWND, byval usersub as SEL_SelChangeCallbackSub )
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hSelectBar)
    if pSel = null then return
    pSel->SelChangeCallback = usersub
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: PsSelectBar
' ========================================================================================
function PsSelectBar_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' There are no child windows to position. Panel WIDTHS do not depend on the client
        ' at all -- but the block-centred vertical placement does, so a resize still
        ' invalidates the layout.
        dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hwnd)
        if pSel then pSel->Refresh()
    end if
    function = 0
end function


' ========================================================================================
' The built-in painter: draws one panel when no PaintPanelCallback is set.
'
' State precedence, decided once here so every renderer agrees:
'   disabled > pressed > hot > selected > idle
' A disabled panel is never hot or pressed (GetHotSelectBarPanel refuses it), so the first
' rung is unreachable from the others rather than merely winning over them.
'
' Pressed renders the SAME as hot on purpose: a press here is a half-completed selection,
' not a button being pushed, and flashing a "pressed" style for the fraction of a second
' before the indicator moves reads as noise. isPressed is still handed to a custom painter,
' which is free to disagree.
'
' The indicator is drawn LAST so that a host-set BackColorSelect cannot paint over it.
' ========================================================================================
private sub PsSelectBar_PaintPanelDefault( _
            byval pSel as PSSELECTBAR ptr, _
            byval b as PsBufferPaint ptr, _
            byval idx as long, _
            byval isHot as boolean, _
            byval isPressed as boolean, _
            byval isSelected as boolean _
            )
    if (pSel = null) orelse (b = null) then exit sub

    with pSel->panels(idx)
        dim as COLORREF backclr = pSel->colors.BackColor
        dim as COLORREF foreclr = pSel->colors.ForeColor
        if .isEnabled = false then
            foreclr = pSel->colors.ForeColorDisabled
        elseif isPressed orelse isHot then
            backclr = pSel->colors.BackColorHot
            foreclr = pSel->colors.ForeColorHot
        elseif isSelected then
            backclr = pSel->colors.BackColorSelect
            foreclr = pSel->colors.ForeColorSelect
        end if

        b->SetColors( foreclr, backclr )
        b->PaintRect( @.rc )             ' fill the WHOLE cell, padding included

        if len(.wszText) then
            b->SetFont( pSel->hPanelFont )
            ' rcText is exactly the measured text, so DT_CENTER here is equivalent to
            ' DT_LEFT and simply survives a 1px measurement drift. PaintText already adds
            ' DT_VCENTER / DT_SINGLELINE / DT_NOPREFIX.
            b->PaintText( .wszText, @.rcText, DT_CENTER )
        end if

        ' The indicator belongs to the current panel only. The isEnabled test is
        ' belt-and-braces: SetEnabled refuses to disable the current panel, so this state
        ' should be unreachable.
        if isSelected andalso .isEnabled then
            b->SetBackColor( pSel->colors.IndicatorColor )
            b->PaintRect( @.rcIndicator )
        end if
    end with
end sub


' ========================================================================================
' Process WM_PAINT message for window/dialog: PsSelectBar
' ========================================================================================
function PsSelectBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer(hwnd)
    if pSel = null then return 0

    ' Consume any pending layout before painting. This is what makes a burst of AddPanel
    ' calls cost one measuring pass instead of N.
    if pSel->bLayoutDirty then pSel->LayoutPanels()

    ' Capture the update rect BEFORE BeginDoubleBuffer -- BeginPaint clears it. This is a
    ' CONSERVATIVE approximation and correctness never depends on it: GetUpdateRect returns
    ' the BOUNDING BOX of the update region, so it only ever skips panels that could not
    ' show a pixel anyway.
    dim as RECT rcUpdate
    if GetUpdateRect( hwnd, @rcUpdate, FALSE ) = 0 then GetClientRect( hwnd, @rcUpdate )

    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pSel->colors.BackColor )

    ' Fill the whole bar, which also covers the area to the right of the last panel.
    b.PaintClientRect()

    ' Skipping panels outside the update rect is safe even though EndDoubleBuffer blits the
    ' WHOLE client: BeginPaint's DC is clipped to the update region, so the skipped
    ' (background-filled) areas never reach the screen.
    dim p as PSSELECTBAR_PAINTINFO
    p.hSelectBar = hwnd
    p.b          = @b

    for i as long = 0 to pSel->panelCount - 1
        dim as RECT rcHit
        if IntersectRect( @rcHit, @pSel->panels(i).rc, @rcUpdate ) = 0 then continue for

        dim as boolean isHot      = (i = pSel->nLastHotIdx)
        dim as boolean isSelected = (i = pSel->nCurSel)
        ' "The press is still on this panel" is exactly "the pressed panel is also the hot
        ' one" -- hover tracking maintains that through the capture, so no second flag.
        dim as boolean isPressed  = (i = pSel->nPressedIdx) andalso (pSel->nLastHotIdx = pSel->nPressedIdx)

        if pSel->PaintPanelCallback then
            p.itemID      = i
            p.rc          = pSel->panels(i).rc
            p.rcText      = pSel->panels(i).rcText
            p.rcIndicator = pSel->panels(i).rcIndicator
            p.isHot       = isHot
            p.isSelected  = isSelected
            p.isPressed   = isPressed
            p.isEnabled   = pSel->panels(i).isEnabled
            p.wszCaption  = pSel->panels(i).wszText
            pSel->PaintPanelCallback( @p )
        else
            PsSelectBar_PaintPanelDefault( pSel, @b, i, isHot, isPressed, isSelected )
        end if
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Clear any hot-tracked panel and stop the safety-net timer. Repaints only if a panel was
' actually hot, so this is cheap to call repeatedly.
'
' Note this also changes how a live press LOOKS (isPressed is derived from the hot index),
' which is exactly right: the cursor left the panel, so the press must stop looking armed.
' The gesture itself survives -- only a release or a capture loss ends it.
' ========================================================================================
private sub PsSelectBar_ClearHotTracking( byval pSel as PSSELECTBAR ptr, byval hSelectBar as HWND )
    if pSel = null then exit sub
    if pSel->hotTimerOn then
        KillTimer( hSelectBar, IDT_CSELECTBAR_HOTTRACK )
        pSel->hotTimerOn = false
    end if
    if pSel->nLastHotIdx <> -1 then
        pSel->nLastHotIdx = -1
        AfxRedrawWindow( hSelectBar )
    end if
end sub


' ========================================================================================
' PsSelectBar Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc.
'
' The control TAKES mouse capture on a press. Something consumes the guaranteed
' WM_LBUTTONDOWN -> WM_LBUTTONUP pairing: the press/cancel gesture. Press a panel, slide off
' it, release -- the selection does not move. The full price is paid, in the order the
' family learned to pay it:
'   - WM_LBUTTONUP SNAPSHOTS the pressed index, THEN releases capture. ReleaseCapture sends
'     WM_CAPTURECHANGED synchronously and that handler cancels the press, so reading the
'     index afterwards would always find -1 and no click would ever fire (Learnings.md);
'   - the release happens before any callback runs;
'   - the message callback's result is IGNORED for WM_LBUTTONUP, so it cannot strand it;
'   - WM_CAPTURECHANGED cancels the press (anything can steal capture at any time);
'   - WM_DESTROY releases it if we still hold it.
'
' CS_DBLCLKS IS enabled, and WM_LBUTTONDBLCLK shares the WM_LBUTTONDOWN handler. The two
' facts go together: with CS_DBLCLKS the dblclk SUBSTITUTES for the second rapid down
' rather than arriving on top of it, so a control that ignored it would drop that press --
' a quick A -> B -> A would silently lose a selection change, and the trailing up would
' release a capture that was never taken. Handling both identically costs one extra case
' label and makes double-click available to hosts through the message callback (tiko's
' Output panel toggles the panel minimized/restored on it).
' ========================================================================================
function PsSelectBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSSELECTBAR ptr pSel = GetSelectBarPointer( hwnd )

    ' Per-message scratch info passed to the user MessageCallback. A plain local (NOT
    ' static) so that multiple control instances never share it. The hover tracking index
    ' likewise lives per-instance in pSel->nLastHotIdx.
    dim m as PSSELECTBAR_MESSAGEINFO

    select case uMsg
        ' Prepare the callback MessageInfo for only the messages that we hand to the host.
        case WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_LBUTTONDBLCLK, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP
            with m
                .hSelectBar = hwnd
                .uMsg       = uMsg
                .wParam     = wParam
                .lParam     = lParam
                .idx        = -1
            end with
    end select

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        PsSelectBar_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       PsSelectBar_OnPaint)

    case WM_TIMER
        if (pSel <> 0) andalso (wParam = IDT_CSELECTBAR_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no
            ' longer over our client area, clear the hot panel ourselves.
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                PsSelectBar_ClearHotTracking( pSel, hwnd )
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
        if pSel then tme.dwHoverTime = pSel->HoverTime
        TrackMouseEvent(@tme)

        ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
        ' quickly (or onto another control), which would strand the hot panel painted.
        ' Poll the cursor while we are over the control.
        if (pSel <> 0) andalso (pSel->hotTimerOn = false) then
            SetTimer( hwnd, IDT_CSELECTBAR_HOTTRACK, PSSELECTBAR_HOTTRACK_MS, NULL )
            pSel->hotTimerOn = true
        end if

        ' Repaint only the panels whose state changed, not the whole bar. This is also what
        ' keeps a live press's appearance current: isPressed is derived from the hot index,
        ' so arming and disarming it costs nothing extra here.
        if pSel then
            dim as long idx = GetHotSelectBarPanel(hwnd)
            if idx <> pSel->nLastHotIdx then
                PsSelectBar_InvalidatePanel( pSel, idx )                 ' newly hot
                PsSelectBar_InvalidatePanel( pSel, pSel->nLastHotIdx )   ' previously hot
                pSel->nLastHotIdx = idx
                ' Panel changed: pop the current tip so it re-queries TTN_GETDISPINFOW for
                ' the new panel on the next hover.
                if pSel->hTooltip then SendMessage( pSel->hTooltip, TTM_POP, 0, 0 )
            end if
            m.idx = idx
            if pSel->MessageCallback then
                if pSel->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEHOVER
        if pSel then
            m.idx = GetHotSelectBarPanel(hwnd)
            if pSel->MessageCallback then
                if pSel->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pSel then
            ' Fast path for clearing the hot panel (the WM_TIMER poll is the fallback for
            ' when this message never arrives). The tooltip hides itself when the mouse
            ' leaves the tool, so nothing to push. m.idx stays -1: the mouse is gone.
            PsSelectBar_ClearHotTracking( pSel, hwnd )
            if pSel->MessageCallback then
                if pSel->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        ' The two share a handler, and that is REQUIRED rather than tidy. With CS_DBLCLKS
        ' on the class, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD
        ' OF a second WM_LBUTTONDOWN -- it substitutes for it, it is not an extra message
        ' on top (Learnings.md). A control that took capture only on the DOWN would
        ' therefore see the trailing WM_LBUTTONUP release a capture it never took, and a
        ' quick A -> B -> A would silently drop a selection change.
        '
        ' The host sees the dblclk through the message callback with a valid idx, which is
        ' how tiko's Output panel toggles minimize/restore on it.
        if pSel then
            ' GetHotSelectBarPanel already rejects disabled panels, so this index is always
            ' something that can legitimately be pressed.
            dim as long idx = GetHotSelectBarPanel(hwnd)
            m.idx = idx
            if pSel->MessageCallback then
                ' Suppressing the press is allowed: no capture has been taken yet, so
                ' there is no bookkeeping to strand.
                if pSel->MessageCallback( @m ) then return 0
            end if
            ' No SetFocus: a select bar must not steal focus from the host's editor.
            if idx <> -1 then
                SetCapture( hwnd )
                pSel->nPressedIdx = idx
                PsSelectBar_InvalidatePanel( pSel, idx )
            end if
            return 0
        end if

    case WM_LBUTTONUP
        if pSel then
            ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the
            ' window losing capture SYNCHRONOUSLY, and our handler for it cancels the
            ' press -- so reading nPressedIdx after the release would always find -1 and
            ' no selection change would ever fire.
            dim as long idxPressed = pSel->nPressedIdx
            ' Release capture unconditionally, before any callback runs. Nothing below --
            ' not a callback, not a host handler -- gets the chance to leave it held.
            if GetCapture() = hwnd then ReleaseCapture()

            dim as long idx = GetHotSelectBarPanel(hwnd)
            pSel->CancelPress()
            PsSelectBar_InvalidatePanel( pSel, idxPressed )

            m.idx = idx
            ' The result is deliberately IGNORED here (see SEL_MessageCallbackFunc).
            if pSel->MessageCallback then pSel->MessageCallback( @m )

            ' Only a matched press+release on the SAME panel moves the selection. Press,
            ' slide off, release = nothing happened, which is the whole reason for capture.
            ' Re-clicking the panel that is already current is not a change either.
            dim as boolean bMatched = (idxPressed <> -1) andalso (idx = idxPressed)
            if bMatched andalso (idx <> pSel->nCurSel) then
                dim as PSSELECTBAR_PANEL ptr pPanel = pSel->GetPanel( idx )
                ' A callback may have mutated the model mid-gesture; re-check.
                if pPanel <> null then
                    if pPanel->isEnabled then
                        dim as long idxOld = pSel->nCurSel
                        pSel->nCurSel = idx
                        PsSelectBar_InvalidatePanel( pSel, idxOld )
                        PsSelectBar_InvalidatePanel( pSel, idx )
                        if pSel->SelChangeCallback then
                            pSel->SelChangeCallback( hwnd, idxOld, idx )
                        end if
                    end if
                end if
            end if
            return 0
        end if

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell).
        ' The gesture is over; forget it and repaint the panel that was armed.
        if pSel then
            dim as long idxPressed = pSel->nPressedIdx
            pSel->CancelPress()
            PsSelectBar_InvalidatePanel( pSel, idxPressed )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu).
        ' No capture is taken for the right button, so an up can arrive without a down.
        if pSel then
            m.idx = GetHotSelectBarPanel(hwnd)
            if pSel->MessageCallback then
                if pSel->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot panel's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pSel then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' store in an instance-lifetime buffer and point lpszText at it
                pSel->wszTooltip = PsSelectBar_ResolveTooltipText( pSel, pSel->nLastHotIdx )
                pdi->lpszText = cast( LPWSTR, pSel->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pSel then
            ' Don't let the hot-tracking timer outlive the window...
            if pSel->hotTimerOn then
                KillTimer( hwnd, IDT_CSELECTBAR_HOTTRACK )
                pSel->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pSel->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored select bar data pointer
        if pSel then
            if IsWindow(pSel->hToolTip) then DestroyWindow(pSel->hToolTip)
            delete(pSel)
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
' control -- one tool covering the entire bar, not one per panel, because the panels are
' painted rects rather than windows. TTN_GETDISPINFOW therefore goes to TOOLINFO.hwnd,
' which is the control itself (NOT the parent form), and PsSelectBar_WndProc answers it
' using the current hot panel.
' ========================================================================================
private function PsSelectBar_AddTooltip( byval hCtrl as HWND ) as HWND
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
' PsSelectBar_Create
' ========================================================================================
function PsSelectBar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsSelectBar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN _
            )
    ' Enable double-click messages, preserving the class's existing styles rather than
    ' replacing them. (CWindow's ClassStyle setter REPLACES, so this must be
    ' read-modify-write, and it needs m_hwnd -- i.e. it only works after Create.)
    ' WM_LBUTTONDBLCLK is handled exactly like WM_LBUTTONDOWN -- see the WndProc header.
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    ' Define the TYPE that holds all the information about this custom select bar.
    dim as PSSELECTBAR ptr pSel = new PSSELECTBAR
    pSel->hWin          = hCtrl
    pSel->idc_SelectBar = CtrlID
    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes
    ' raw pixels and the caller scales (the family rule).
    pSel->nPadLeft         = pWindow->ScaleX( PSSELECTBAR_DEFAULT_PADLEFT )
    pSel->nPadRight        = pWindow->ScaleX( PSSELECTBAR_DEFAULT_PADRIGHT )
    pSel->nIndicatorHeight = pWindow->ScaleY( PSSELECTBAR_DEFAULT_INDICATORH )
    pSel->nIndicatorGap    = pWindow->ScaleY( PSSELECTBAR_DEFAULT_INDICATORGAP )
    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the select bar window
    pWindow->UserData(0) = cast(LONG_PTR, pSel)

    pSel->hTooltip = PsSelectBar_AddTooltip( hCtrl )

    function = hCtrl

end function
