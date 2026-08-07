
#pragma once

#include once "PsBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id.
#define IDT_CSELECTBAR_HOTTRACK   &hCB60
#define PSSELECTBAR_HOTTRACK_MS    100

' Default horizontal padding added to the LEFT and RIGHT of a panel's measured text. These
' are the only things besides the text itself that decide a panel's width. DPI-scaled at
' Create; PsSelectBar_SetPadding takes raw pixels thereafter.
#define PSSELECTBAR_DEFAULT_PADLEFT   12
#define PSSELECTBAR_DEFAULT_PADRIGHT  12

' Default thickness of the selection indicator, and the gap between the bottom of the text
' and the top of that line. Both are LAYOUT inputs: they are part of the block height that
' gets centred in the client area. DPI-scaled at Create.
#define PSSELECTBAR_DEFAULT_INDICATORH    2
#define PSSELECTBAR_DEFAULT_INDICATORGAP  4


' Colors for the built-in painter. Copied on Set.
'
' BackColorHot and BackColorSelect default to BackColor on purpose: out of the box the bar
' is a uniform background where only the label color and the indicator move, which is the
' look this control was drawn for. A host that wants a filled hover or selected cell just
' sets them; nothing else has to change.
type PSSELECTBAR_COLORS
    BackColor         as COLORREF = BGR(33,37,43)     ' bar background + idle cell fill
    ForeColor         as COLORREF = BGR(150,156,166)  ' idle label
    BackColorHot      as COLORREF = BGR(33,37,43)     ' = BackColor (see above)
    ForeColorHot      as COLORREF = BGR(215,218,224)
    BackColorSelect   as COLORREF = BGR(33,37,43)     ' = BackColor (see above)
    ForeColorSelect   as COLORREF = BGR(255,255,255)
    ForeColorDisabled as COLORREF = BGR(90,96,106)
    IndicatorColor    as COLORREF = BGR(255,255,255)  ' the line under the selected panel
end type


' One panel. Everything above the DERIVED line is authored; everything below belongs to
' LayoutPanels and is never set from outside.
'
' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
' that never name the real problem -- hence nCellWidth / nPadLeft throughout.
' NOTE: neither isHot NOR isSelected lives here. Both are single-valued facts, so the
' control owns them (nLastHotIdx, nCurSel) as one source of truth. This differs from
' PsIconPanel, where selection is a multi-valued SET and therefore genuinely belongs on the
' item -- a distinction worth keeping straight when copying between the two.
type PSSELECTBAR_PANEL
    wszText     as DWSTRING          ' the label. Its measured width drives the cell width.
    wszTooltip  as DWSTRING          ' "" = ask SEL_TooltipCallback, then show nothing
    id          as long = 0          ' host id for this panel
    itemData    as integer = 0       ' free-form host payload
    isEnabled   as boolean = true    ' false = greyed, never hot, swallows clicks
    nPadLeft    as long = -1         ' -1 = inherit the bar's nPadLeft
    nPadRight   as long = -1         ' -1 = inherit the bar's nPadRight
    ' --- DERIVED by LayoutPanels --------------------------------------------------------
    nCellWidth  as long = 0          ' padLeft + measured text width + padRight
    rc          as RECT              ' the full cell, client coords: the FILL and HIT rect
    rcText      as RECT              ' exactly the measured text box
    rcIndicator as RECT              ' spans rcText horizontally, gap below it
end type


type PSSELECTBAR_PAINTINFO
    hSelectBar  as HWND                   ' the control, so the callback can query it
    itemID      as long                   ' panel index (model index)
    b           as PsBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutPanels. Never re-derive insets from rc. ---
    rc          as RECT                   ' the full cell: fill THIS
    rcText      as RECT                   ' the label goes here
    rcIndicator as RECT                   ' fill this when isSelected
    ' --- State: the control decides these, you only render. ---
    isHot       as boolean                ' the mouse is over this panel
    isSelected  as boolean                ' this is the current panel
    isPressed   as boolean                ' a live left press is on this panel AND the
                                          '   cursor is still over it (slide off and this
                                          '   goes false without ending the gesture)
    isEnabled   as boolean
    wszCaption  as DWSTRING
end type

type PSSELECTBAR_MESSAGEINFO
    hSelectBar  as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
    idx         as long       ' panel index under the mouse (-1 if none)
end type


' Draw one panel, INSTEAD OF the built-in painter -- setting this replaces the built-in
' rendering for EVERY panel. Paint through p->b (the control's double buffer for this
' repaint) -- do not touch the screen DC. Style from the state flags; the control decides
' them, you only render.
'
' Three contracts worth honoring:
'   - Draw with the SAME font you handed to PsSelectBar_SetFont. The control measured the
'     panel widths with it; a different font means the widths lie.
'   - Fill p->rc, not p->rcText. rc is the whole cell including padding, and is what the
'     control background-filled; leaving part of it unpainted shows the bar through.
'   - Use the rects as handed to you rather than re-deriving insets from p->rc. rcText is
'     exactly the measured text, and rcIndicator is already positioned under it.
type SEL_PaintPanelCallbackSub as sub( byval p as PSSELECTBAR_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed.
'
' CAUTION: the result is honored for every message EXCEPT WM_LBUTTONUP, where it is
' IGNORED. The control holds mouse capture across a press, and the up-message is what
' releases it: a callback that suppressed it would strand capture and route every
' subsequent click to this control (the PsListBox bug recorded in Learnings.md).
' Suppressing WM_LBUTTONDOWN suppresses the press, never the capture bookkeeping.
type SEL_MessageCallbackFunc as function( byval m as PSSELECTBAR_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for a panel, on demand (only when a tip is about to show).
' Consulted only when the panel has no tooltip text of its own. Return "" for no tooltip.
' Deliberately NO fallback to the panel's caption (which PsStatusBar and PsTabBar do): the
' caption is already on screen, so a tip repeating it is noise.
type SEL_TooltipCallbackFunc as function( byval hSelectBar as HWND, byval idx as long ) as DWSTRING

' The current panel changed through USER interaction. Fired AFTER the control's state is
' updated, so PsSelectBar_GetCurSel() = idxNew. Programmatic SetCurSel does NOT fire this --
' only the user does (Win32's TCM_SETCURSEL / TCN_SELCHANGE precedent), which also makes it
' safe to call SetCurSel from inside the handler without recursing. Clicking the panel that
' is ALREADY current is not a change and fires nothing.
type SEL_SelChangeCallbackSub as sub( byval hSelectBar as HWND, byval idxOld as long, byval idxNew as long )


type PSSELECTBAR
    hWin              as HWND
    hToolTip          as HWND
    wszTooltip        as DWSTRING
    panels(any)       as PSSELECTBAR_PANEL
    panelCount        as long = 0
    idc_SelectBar     as long = 0
    HoverTime         as long = 250
    ' Three single-valued indices. Because the panel set is STATIC -- panels are added once
    ' and never inserted, deleted or moved -- none of them can ever be left dangling by a
    ' mutation, so this control carries none of the index fix-up code every other sibling
    ' needs (three separate sites in PsTabBar.bi alone, and the family's most repeated bug
    ' class). Clear() is the one exception and it resets all three.
    nCurSel           as long = -1       ' the current panel; -1 only while the bar is empty
    nLastHotIdx       as long = -1       ' panel the mouse was last over (hover tracking)
    nPressedIdx       as long = -1       ' panel under a live left press, or -1
    hotTimerOn        as boolean = false ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press: the
    '     press/cancel gesture (press a panel, slide off, release, selection does not move)
    '     consumes the guaranteed down->up pairing, which is the family's test for taking
    '     capture. It pays the full price: snapshot the pressed index BEFORE releasing
    '     (ReleaseCapture sends WM_CAPTURECHANGED synchronously -- Learnings.md), release
    '     on the up-message before any callback runs, WM_CAPTURECHANGED cancels the press,
    '     WM_DESTROY releases, and no callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still on the pressed panel" is
    '     exactly (nLastHotIdx = nPressedIdx), and hover tracking already maintains that
    '     through the capture (moves outside the client report -1). ---
    colors            as PSSELECTBAR_COLORS
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member
    ' procedure of this type, and "dim as HFONT x" there fails with a misleading
    ' "error 14: Expected identifier, found 'HFONT'". See C:\dev\Learnings.md.
    hPanelFont        as HFONT
    ' --- Layout. Panel rects are derived, never set from outside; LayoutPanels() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs
    '     it, which coalesces a burst of AddPanel calls into one measuring pass. ---
    nPadLeft          as long = PSSELECTBAR_DEFAULT_PADLEFT       ' DPI-scaled at Create
    nPadRight         as long = PSSELECTBAR_DEFAULT_PADRIGHT      ' DPI-scaled at Create
    nIndicatorHeight  as long = PSSELECTBAR_DEFAULT_INDICATORH    ' DPI-scaled at Create
    nIndicatorGap     as long = PSSELECTBAR_DEFAULT_INDICATORGAP  ' DPI-scaled at Create
    nTextHeight       as long = 0        ' derived: ONE text height for the whole bar
    nTotalWidth       as long = 0        ' derived: summed cell widths = the run's width
    bLayoutDirty      as boolean = true
    PaintPanelCallback as SEL_PaintPanelCallbackSub   ' optional; replaces the built-in painter
    MessageCallback    as SEL_MessageCallbackFunc     ' optional
    TooltipCallback    as SEL_TooltipCallbackFunc     ' optional
    SelChangeCallback  as SEL_SelChangeCallbackSub    ' optional; user-driven changes only

    declare sub      LayoutPanels()
    declare function GetCount() as long                                     ' panel count
    declare function AddPanel() as PSSELECTBAR_PANEL ptr                     ' append
    declare function GetPanel( byval idx as long ) as PSSELECTBAR_PANEL ptr
    declare function IsValidPanel( byval idx as long ) as boolean
    declare function HitTest( byval x as long, byval y as long ) as long
    declare sub      CancelPress()
    declare sub      Clear()
    declare sub      Refresh()
end type


' Measure every panel and assign its three rects. This is the only place panel geometry is
' produced; everything else (hit-testing, painting, invalidation) consumes it.
'
'   textW(i)       = measured width of the label, in the control's font
'   cellWidth(i)   = padLeft(i) + textW(i) + padRight(i)
'   totalW         = sum of every cellWidth                    ( = GetIdealWidth )
'
'   textH          = GetTextMetrics.tmHeight       ONE height for the whole bar
'   blockH         = textH + gap + indicatorHeight
'   blockTop       = clientTop + (clientH - blockH) \ 2        the block-centred rule
'
'   rc(i)          = ( x, clientTop, x + cellWidth(i), clientBottom )   full client height
'   rcText(i)      = ( x + padLeft(i), blockTop, + textW(i), + textH )
'   rcIndicator(i) = ( rcText.left, rcText.bottom + gap, rcText.right, + indicatorHeight )
'
' WHY textH COMES FROM GetTextMetrics AND NOT FROM EACH PANEL'S OWN EXTENT: one height for
' the whole bar means every label sits on the same baseline and every indicator on the same
' pixel row. Per-panel extents would let an all-lowercase or empty caption ride up or down
' relative to its neighbours, and the indicators would visibly stagger.
'
' WHY THE BLOCK IS CENTRED AS A UNIT: the centred quantity is text + gap + line, not the
' text alone, so the space above the text and below the line stay balanced no matter what
' the thickness, the gap or the font does. Centring the text alone and hanging the line
' underneath looks bottom-heavy and can push the line off a short bar.
'
' The run always starts at x = 0. The visual indent before the first label is simply that
' panel's left padding -- there is deliberately no separate indent property. On overflow the
' tail clips at the client edge: rects are computed honestly rather than squeezed (PsTabBar's
' rule), so hit-testing stays correct for free and the cursor cannot reach past the edge.
sub PSSELECTBAR.LayoutPanels()
    this.bLayoutDirty = false
    this.nTotalWidth  = 0
    if this.hWin = 0 then exit sub
    if this.panelCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hPanelFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    this.nTextHeight = tm.tmHeight

    ' Resolve every per-panel override ONCE, here, so the two passes below cannot disagree
    ' about what a panel's padding is.
    dim textW( 0 to this.panelCount - 1 ) as long
    dim padL( 0 to this.panelCount - 1 ) as long

    dim as long totalW = 0
    for i as long = 0 to this.panelCount - 1
        with this.panels(i)
            dim as long nChars = len( .wszText )
            if nChars > 0 then
                dim as SIZE sz
                if GetTextExtentPoint32W( hDC, .wszText.Wz(), nChars, @sz ) then
                    textW(i) = sz.cx
                end if
            end if
            padL(i) = iif( .nPadLeft < 0, this.nPadLeft, .nPadLeft )
            dim as long padR = iif( .nPadRight < 0, this.nPadRight, .nPadRight )
            .nCellWidth = padL(i) + textW(i) + padR
            totalW += .nCellWidth
        end with
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    this.nTotalWidth = totalW

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again -- but note this bail
    ' comes AFTER nTotalWidth is computed. The run's width does not depend on the client at
    ' all, and GetIdealWidth is exactly what a host calls to decide how big to make the bar
    ' in the first place: returning 0 until it had already been sized would be a
    ' chicken-and-egg trap.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    dim as long blockH   = this.nTextHeight + this.nIndicatorGap + this.nIndicatorHeight
    dim as long blockTop = rcClient.top + ((clientH - blockH) \ 2)

    dim as long x = rcClient.left
    for i as long = 0 to this.panelCount - 1
        with this.panels(i)
            SetRect( @.rc, x, rcClient.top, x + .nCellWidth, rcClient.bottom )
            SetRect( @.rcText, x + padL(i), blockTop, _
                               x + padL(i) + textW(i), blockTop + this.nTextHeight )
            SetRect( @.rcIndicator, .rcText.left, .rcText.bottom + this.nIndicatorGap, _
                                    .rcText.right, .rcText.bottom + this.nIndicatorGap + this.nIndicatorHeight )
            x += .nCellWidth
        end with
    next
end sub

function PSSELECTBAR.GetCount() as long
    return this.panelCount
end function

function PSSELECTBAR.IsValidPanel( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.panelCount)
end function

function PSSELECTBAR.GetPanel( byval idx as long ) as PSSELECTBAR_PANEL ptr
    if this.IsValidPanel(idx) = false then return null
    return @this.panels(idx)
end function

' Which panel contains this client-coordinate point? DISABLED panels are NOT excluded: they
' still occupy their cell, and a caller asking "what is here" deserves the truth. The hover
' and click paths add the enabled test themselves.
function PSSELECTBAR.HitTest( byval x as long, byval y as long ) as long
    dim as POINT pt
    pt.x = x
    pt.y = y
    for i as long = 0 to this.panelCount - 1
        if PtInRect( @this.panels(i).rc, pt ) then return i
    next
    return -1
end function

' Append a fresh (reset) panel. Grows the backing store by doubling so building a bar is
' amortized O(1), not O(n^2).
'
' THE FIRST PANEL ADDED BECOMES THE CURRENT ONE. This control's contract is that exactly
' one panel is always current, and auto-selecting here is what establishes that invariant
' at the only moment it could otherwise be violated. It is silent: no callback fires, in
' keeping with "programmatic is silent, only the user speaks".
'
' There is NO InsertPanelAt and NO DeletePanelAt, deliberately -- the panel set is static.
' See the type's index comment for what that buys.
function PSSELECTBAR.AddPanel() as PSSELECTBAR_PANEL ptr
    dim as long cap = ubound(this.panels) + 1
    if this.panelCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.panels( 0 to newcap - 1 )
    end if

    dim as long idx = this.panelCount

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.panels(idx)
        .wszText     = ""
        .wszTooltip  = ""
        .id          = 0
        .itemData    = 0
        .isEnabled   = true
        .nPadLeft    = -1
        .nPadRight   = -1
        .nCellWidth  = 0
        SetRectEmpty( @.rc )
        SetRectEmpty( @.rcText )
        SetRectEmpty( @.rcIndicator )
    end with

    this.panelCount += 1
    if this.panelCount = 1 then this.nCurSel = 0     ' establish the always-one invariant
    this.Refresh()
    return @this.panels(idx)
end function

' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's
' job, and only on the up-message or WM_DESTROY -- doing it here would let a mutation
' inside a callback strand or double-release it.
sub PSSELECTBAR.CancelPress()
    this.nPressedIdx = -1
end sub

' Tear the bar down to nothing. This is the ONE structural mutation besides AddPanel, and
' it exists for a full rebuild (re-localising the labels, say) -- not for runtime editing.
' It resets the selection, so the next AddPanel re-establishes the always-one invariant.
sub PSSELECTBAR.Clear()
    for i as long = 0 to this.panelCount - 1
        this.panels(i).wszText    = ""
        this.panels(i).wszTooltip = ""
    next
    this.panelCount  = 0
    this.nCurSel     = -1
    this.nLastHotIdx = -1
    this.CancelPress()
    this.Refresh()
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of AddPanel calls costs one measuring pass, not N.
sub PSSELECTBAR.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking run is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A SELECT BAR: a flat row of text labels where exactly one is current, marked by a colored
' line under the word. No tab chrome, no close buttons, no reordering, no scrolling.
'
' STATIC BY CONTRACT
'   Panels are added once, at construction, and are never inserted, deleted or moved
'   afterwards -- which is why there is no InsertPanel and no DeletePanel. Their widths are
'   fixed by their text and padding, so they do not move at runtime either. SetText and the
'   padding setters do re-measure and therefore do shift every panel to their right; that is
'   a design-time operation, and keeping it out of the runtime path is the host's job.
'
' EXACTLY ONE PANEL IS CURRENT
'   The first panel added becomes current. SetCurSel rejects -1 and rejects a disabled
'   panel; SetEnabled(false) refuses to disable the panel that is currently current. Between
'   them those three rules mean the current panel always exists and is always enabled, so
'   the indicator is always somewhere -- there is no state where the bar looks like nothing
'   is selected.
'
' THE CONTROL HANDLE
'   Every PsSelectBar_* function takes the handle returned by PsSelectBar_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need to
'   treat the control as a window, e.g. SetWindowPos() to place and size it. An opaque
'   wrapper would buy a little type-safety and break that, so it was rejected.
'
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized: position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Building the bar.  AddPanel appends and returns the new panel's index, or -1. The FIRST
' panel added becomes the current one (silently). Clear tears everything down for a full
' rebuild -- it is not a runtime editing tool.
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_AddPanel( byval hSelectBar as HWND, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
declare sub      PsSelectBar_Clear( byval hSelectBar as HWND )
declare sub      PsSelectBar_Refresh( byval hSelectBar as HWND )

' ----------------------------------------------------------------------------------------
' Counts and lookup.
'   FindPanelByID returns the FIRST panel with that id, or -1. HitTest takes CLIENT
'   coordinates, forces a pending layout, and returns the panel there (disabled panels
'   included) or -1.
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_GetCount( byval hSelectBar as HWND ) as long
declare function PsSelectBar_IsValidPanel( byval hSelectBar as HWND, byval idx as long ) as boolean
declare function PsSelectBar_FindPanelByID( byval hSelectBar as HWND, byval id as long ) as long
declare function PsSelectBar_HitTest( byval hSelectBar as HWND, byval x as long, byval y as long ) as long

' ----------------------------------------------------------------------------------------
' Panel contents and state.  Set* return FALSE for an invalid panel index.
'
'   SetText re-measures and re-lays out: every panel to its right moves. Design-time only.
'   SetEnabled(false) is REFUSED for the current panel (returns FALSE and changes nothing) --
'   switch away first. Disabling the hovered or pressed panel clears that state at once
'   rather than waiting for the next mouse move.
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_GetText( byval hSelectBar as HWND, byval idx as long ) as DWSTRING
declare function PsSelectBar_SetText( byval hSelectBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
declare function PsSelectBar_GetPanelID( byval hSelectBar as HWND, byval idx as long ) as long
declare function PsSelectBar_SetPanelID( byval hSelectBar as HWND, byval idx as long, byval id as long ) as boolean
declare function PsSelectBar_GetItemData( byval hSelectBar as HWND, byval idx as long ) as integer
declare function PsSelectBar_SetItemData( byval hSelectBar as HWND, byval idx as long, byval itemData as integer ) as boolean
declare function PsSelectBar_GetEnabled( byval hSelectBar as HWND, byval idx as long ) as boolean
declare function PsSelectBar_SetEnabled( byval hSelectBar as HWND, byval idx as long, byval isEnabled as boolean ) as boolean

' ----------------------------------------------------------------------------------------
' Selection.  Exactly one panel is current at any time.
'
'   SetCurSel REJECTS -1 (there is no "nothing selected" state) and rejects a disabled
'   panel. It is SILENT: the selection-change callback fires only for user interaction,
'   following Win32's TCM_SETCURSEL / TCN_SELCHANGE precedent, which also stops a host
'   calling SetCurSel from inside its own handler from recursing.
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_GetCurSel( byval hSelectBar as HWND ) as long
declare function PsSelectBar_SetCurSel( byval hSelectBar as HWND, byval idx as long ) as boolean

' ----------------------------------------------------------------------------------------
' Layout.
'
'   A panel is exactly as wide as its own text plus its padding:
'       cellWidth = padLeft + measured text width + padRight
'
'   SetPadding sets the bar-wide default every panel inherits; SetPanelPadding overrides one
'   panel, and -1 on either side goes back to inheriting. Both take RAW PIXELS -- the caller
'   DPI-scales (the family rule; only the Create-time defaults are scaled for you).
'
'   SetIndicator sets the line's thickness and the gap between the text and the line. BOTH
'   ARE LAYOUT INPUTS -- they are part of the block height that gets centred, so changing
'   either moves the text as well as the line.
'
'   GetIdealWidth is the summed width of every cell -- size the bar with it. It is valid
'   before the bar has ever been sized. Rects are computed lazily; every query here forces a
'   pending layout, so results are always current.
' ----------------------------------------------------------------------------------------
declare sub      PsSelectBar_GetPadding( byval hSelectBar as HWND, byref nPadLeft as long, byref nPadRight as long )
declare sub      PsSelectBar_SetPadding( byval hSelectBar as HWND, byval nPadLeft as long, byval nPadRight as long )
declare function PsSelectBar_SetPanelPadding( byval hSelectBar as HWND, byval idx as long, byval nPadLeft as long, byval nPadRight as long ) as boolean
declare sub      PsSelectBar_GetIndicator( byval hSelectBar as HWND, byref nThickness as long, byref nGap as long )
declare sub      PsSelectBar_SetIndicator( byval hSelectBar as HWND, byval nThickness as long, byval nGap as long )
declare function PsSelectBar_GetPanelRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsSelectBar_GetPanelTextRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsSelectBar_GetPanelIndicatorRect( byval hSelectBar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsSelectBar_GetIdealWidth( byval hSelectBar as HWND ) as long

' ----------------------------------------------------------------------------------------
' Appearance.  The font is borrowed, never owned: keep it alive and destroy it yourself.
' It is also the MEASURING font, so changing it re-lays out the whole bar.
'
'   SetColors copies the whole struct. GetColors fills one out, so the read-modify-write
'   "change one color" idiom is Get, assign, Set.
' ----------------------------------------------------------------------------------------
declare sub      PsSelectBar_GetColors( byval hSelectBar as HWND, byval pColors as PSSELECTBAR_COLORS ptr )
declare sub      PsSelectBar_SetColors( byval hSelectBar as HWND, byval pColors as PSSELECTBAR_COLORS ptr )
declare function PsSelectBar_GetFont( byval hSelectBar as HWND ) as HFONT
declare function PsSelectBar_SetFont( byval hSelectBar as HWND, byval hPanelFont as HFONT ) as boolean
declare function PsSelectBar_GetTooltipHandle( byval hSelectBar as HWND ) as HWND
declare sub      PsSelectBar_SetHoverTime( byval hSelectBar as HWND, byval milliseconds as long )

' ----------------------------------------------------------------------------------------
' Tooltips.  Per-panel text wins; the callback is consulted only for panels with none; a
' panel with neither shows no tip.
' ----------------------------------------------------------------------------------------
declare function PsSelectBar_GetTooltipText( byval hSelectBar as HWND, byval idx as long ) as DWSTRING
declare function PsSelectBar_SetTooltipText( byval hSelectBar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintPanelCallback - draw one panel INSTEAD OF the built-in painter (which handles
'                        every panel when this is unset).
'   MessageCallback    - observe mouse messages; return TRUE to suppress default handling
'                        (IGNORED for WM_LBUTTONUP -- capture-critical).
'   TooltipCallback    - supply tooltip text on demand for panels without their own.
'   SelChangeCallback  - the USER changed the current panel. Programmatic SetCurSel is
'                        silent, and re-clicking the current panel is not a change.
' ----------------------------------------------------------------------------------------
declare sub      PsSelectBar_SetPaintPanelCallback( byval hSelectBar as HWND, byval usersub as SEL_PaintPanelCallbackSub )
declare sub      PsSelectBar_SetMessageCallback( byval hSelectBar as HWND, byval userfunc as SEL_MessageCallbackFunc )
declare sub      PsSelectBar_SetTooltipCallback( byval hSelectBar as HWND, byval userfunc as SEL_TooltipCallbackFunc )
declare sub      PsSelectBar_SetSelChangeCallback( byval hSelectBar as HWND, byval usersub as SEL_SelChangeCallbackSub )
