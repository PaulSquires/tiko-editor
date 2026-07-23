
#pragma once

#include once "CBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the
' control. WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a
' periodic cursor check acts as a safety net. Timer IDs are per-window, so every
' instance can share this id.
#define IDT_CICONPANEL_HOTTRACK   &hCB50
#define CICONPANEL_HOTTRACK_MS    100

' Default icon cell size. Every icon gets a cell of exactly this size unless the item
' overrides it. DPI-scaled at Create; CIconPanel_SetIconSize takes raw pixels thereafter.
#define CICONPANEL_DEFAULT_ICONWIDTH   20
#define CICONPANEL_DEFAULT_ICONHEIGHT  20

' Default space reserved BEFORE and AFTER each icon. This is the author's spacing control
' and the whole reason this control exists: it is never adjusted, redistributed or
' squeezed by the layout. DPI-scaled at Create; the setters take raw pixels thereafter.
#define CICONPANEL_DEFAULT_PADBEFORE   4
#define CICONPANEL_DEFAULT_PADAFTER    4

' Default thickness of a separator's rule (it takes the place of the icon width in a
' separator's cell). DPI-scaled at Create.
#define CICONPANEL_DEFAULT_SEPWIDTH    1


' Where the whole run of items sits in the client area. This is the ONLY thing the layout
' recomputes when the host resizes the panel: the run is shifted as one block, and the
' spacing inside it never changes.
enum CICONPANEL_JUSTIFY
    IP_JUSTIFY_LEFT = 0
    IP_JUSTIFY_CENTER
    IP_JUSTIFY_RIGHT
end enum

' What an item DOES when clicked. Kind is authored at Add time and never changes.
'   TOGGLE    - latches. Flips isSelected on a matched click and fires SelChangeCallback.
'               Toggles are INDEPENDENT of each other: this control has no "current item",
'               it has a set of switches (contrast CTabBar's single nCurSel).
'   COMMAND   - momentary. Draws pressed while held, fires ClickCallback on a matched
'               release, and never latches.
'   SEPARATOR - a vertical rule the control draws itself. Not hit-testable: it never goes
'               hot, never presses, and HitTest returns -1 for it.
enum CICONPANEL_ITEMKIND
    IP_KIND_TOGGLE = 0
    IP_KIND_COMMAND
    IP_KIND_SEPARATOR
end enum


' Colors for the built-in painter. Copied on Set. Defaults are tiko's dark theme, which is
' what this control was shaped for; a host on a light theme sets all eight.
type CICONPANEL_COLORS
    BackColor         as COLORREF = BGR(33,37,43)     ' panel background + idle cell fill
    ForeColor         as COLORREF = BGR(215,218,224)  ' idle glyph
    BackColorHot      as COLORREF = BGR(44,49,58)
    ForeColorHot      as COLORREF = BGR(255,255,255)
    BackColorSelect   as COLORREF = BGR(38,79,120)    ' latched toggle, and the press flash
    ForeColorSelect   as COLORREF = BGR(255,255,255)
    ForeColorDisabled as COLORREF = BGR(110,115,125)
    SeparatorColor    as COLORREF = BGR(70,76,88)
end type


' One item. Everything above the DERIVED line is authored; everything below belongs to
' LayoutItems and is never set from outside.
'
' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
' that never name the real problem -- hence nIconWidth / nCellWidth throughout.
' NOTE: no isHot here. Hover is transient and single-valued, so the control's nLastHotIdx
' is the one source of truth; a per-item copy would be a second one to keep in sync. The
' same argument does NOT apply to isSelected: selection is multi-valued and persistent, so
' it genuinely lives on the item.
type CICONPANEL_ITEM
    itemKind      as long = IP_KIND_TOGGLE
    wszGlyph      as DWSTRING          ' the Segoe Fluent Icons codepoint(s) to draw
    wszTooltip    as DWSTRING          ' "" = ask TooltipCallback, then show nothing
    id            as long = 0          ' host command id, reported by ClickCallback
    itemData      as integer = 0       ' free-form host payload
    isSelected    as boolean = false   ' latched (TOGGLE items only)
    isEnabled     as boolean = true    ' false = greyed, never hot, swallows clicks
    ' Per-item overrides. The inherit sentinels differ because 0 is a legal padding but
    ' not a legal icon size.
    nPadBefore    as long = -1         ' -1 = inherit the panel's nPadBefore
    nPadAfter     as long = -1         ' -1 = inherit the panel's nPadAfter
    nIconWidth    as long = 0          '  0 = inherit the panel's nIconWidth
    nIconHeight   as long = 0          '  0 = inherit the panel's nIconHeight
    ForeColorItem as COLORREF = 0      ' idle glyph color, when bHasForeColor
    ' A boolean rather than a CLR_NONE sentinel: every COLORREF value is a legal color,
    ' and the flag reads honestly at each use site.
    bHasForeColor as boolean = false
    ' --- DERIVED by LayoutItems ---------------------------------------------------------
    nCellWidth    as long = 0          ' padBefore + icon (or rule) width + padAfter
    rc            as RECT              ' the full cell, client coords: the FILL and HIT rect
    rcIcon        as RECT              ' the icon draw rect, centred in rc both ways
end type


type CICONPANEL_PAINTINFO
    hIconPanel  as HWND                   ' the control, so the callback can query it
    itemID      as long                   ' item index (model index)
    b           as CBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    itemKind    as long                   ' IP_KIND_*
    rc          as RECT                   ' the full cell: fill THIS
    rcIcon      as RECT                   ' the icon rect: draw the glyph in THIS
    ' --- State: the control decides these, you only render. ---
    isHot       as boolean                ' the mouse is over this item
    isSelected  as boolean                ' a latched TOGGLE
    isPressed   as boolean                ' a live left press is on this item AND the
                                          '   cursor is still over it (slide off and this
                                          '   goes false without ending the gesture)
    isEnabled   as boolean
    wszGlyph    as DWSTRING
end type

type CICONPANEL_MESSAGEINFO
    hIconPanel  as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
    idx         as long       ' item index under the mouse (-1 if none)
end type


' Draw one item, INSTEAD OF the built-in painter -- setting this replaces the built-in
' rendering for EVERY item, separators included. Paint through p->b (the control's double
' buffer for this repaint), using p->rc as the cell and p->rcIcon as the glyph box -- do
' not touch the screen DC. Style from the state flags; the control decides them, you only
' render.
'
' Two contracts worth honoring:
'   - Draw the glyph with the SAME font you handed to CIconPanel_SetFont, sized to fit
'     rcIcon. rcIcon is a LAYOUT cell, not a measurement of your glyph: the control never
'     measures text, so a font too large for the cell simply clips.
'   - Fill p->rc, not p->rcIcon. rc includes the item's padding and is what the control
'     background-filled; leaving part of it unpainted shows the panel background through.
type IP_PaintItemCallbackSub as sub( byval p as CICONPANEL_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed.
'
' CAUTION: the result is honored for every message EXCEPT WM_LBUTTONUP, where it is
' IGNORED. The control holds mouse capture across a press (the press/cancel gesture
' consumes the guaranteed down->up pairing), and the up-message is what releases it: a
' callback that suppressed it would strand capture and route every subsequent click to
' this control (the CListBox bug recorded in Learnings.md). Suppressing WM_LBUTTONDOWN
' suppresses the press, never the capture bookkeeping.
type IP_MessageCallbackFunc as function( byval m as CICONPANEL_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for an item, on demand (only when a tip is about to show).
' Consulted only when the item has no tooltip text of its own. Return "" for no tooltip.
' Unlike CStatusBar/CTabBar there is no caption to fall back on -- an icon has none -- so
' an item with neither its own text nor a callback answer simply shows no tip.
type IP_TooltipCallbackFunc as function( byval hIconPanel as HWND, byval idx as long ) as DWSTRING

' A COMMAND item was clicked: a matched press+release on the same item. Fires nothing for
' toggles (they report through SelChangeCallback) and nothing for a cancelled gesture
' (press an icon, slide off, release).
type IP_ClickCallbackSub as sub( byval hIconPanel as HWND, byval idx as long, byval id as long )

' A TOGGLE item latched or unlatched through USER interaction. Fired AFTER the control's
' state is updated, so CIconPanel_GetSelected( h, idx ) = isSelected. Programmatic
' CIconPanel_SetSelected does NOT fire this -- only the user does (the family rule, and
' Win32's TCM_SETCURSEL / TCN_SELCHANGE precedent), which also makes it safe to call the
' setter from inside the handler without recursing.
type IP_SelChangeCallbackSub as sub( byval hIconPanel as HWND, byval idx as long, byval isSelected as boolean )


type CICONPANEL
    hWin              as HWND
    hToolTip          as HWND
    wszTooltip        as DWSTRING
    items(any)        as CICONPANEL_ITEM
    itemCount         as long = 0
    idc_IconPanel     as long = 0
    HoverTime         as long = 250
    ' Two single-valued indices. Because the item set is STATIC -- items are added once and
    ' never inserted, deleted or moved -- neither can ever be left dangling by a mutation,
    ' so this control carries none of the index fix-up code the dynamic siblings need
    ' (three separate sites in CTabBar.bi alone, and the family's most repeated bug class).
    nLastHotIdx       as long = -1       ' item the mouse was last over (hover tracking)
    hotTimerOn        as boolean = false ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press -- unlike
    '     CStatusBar, and deliberately: the press/cancel gesture (press an icon, slide
    '     off, release, nothing happens) consumes the guaranteed down->up pairing, which
    '     is the family's test for taking capture. It pays CVScrollBar's full price:
    '     snapshot the pressed index BEFORE releasing (ReleaseCapture sends
    '     WM_CAPTURECHANGED synchronously -- Learnings.md), release on the up-message
    '     before any callback runs, WM_CAPTURECHANGED cancels the press, WM_DESTROY
    '     releases, and no callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still on the pressed item" is
    '     exactly (nLastHotIdx = nPressedIdx), and hover tracking already maintains that
    '     through the capture (moves outside the client report -1). ---
    nPressedIdx       as long = -1       ' item under a live left press, or -1
    colors            as CICONPANEL_COLORS
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member
    ' procedure of this type, and "dim as HFONT x" there fails with a misleading
    ' "error 14: Expected identifier, found 'HFONT'". See C:\dev\Learnings.md.
    hIconFont         as HFONT
    ' --- Layout. Item rects are derived, never set from outside; LayoutItems() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs
    '     it, which coalesces a burst of mutations into one pass. ---
    nJustify          as long = IP_JUSTIFY_LEFT
    nIconWidth        as long = CICONPANEL_DEFAULT_ICONWIDTH    ' DPI-scaled at Create
    nIconHeight       as long = CICONPANEL_DEFAULT_ICONHEIGHT   ' DPI-scaled at Create
    nPadBefore        as long = CICONPANEL_DEFAULT_PADBEFORE    ' DPI-scaled at Create
    nPadAfter         as long = CICONPANEL_DEFAULT_PADAFTER     ' DPI-scaled at Create
    nSepWidth         as long = CICONPANEL_DEFAULT_SEPWIDTH     ' DPI-scaled at Create
    nTotalWidth       as long = 0        ' summed cell widths = the run's ideal width
    bLayoutDirty      as boolean = true
    PaintItemCallback as IP_PaintItemCallbackSub    ' optional; replaces the built-in painter
    MessageCallback   as IP_MessageCallbackFunc     ' optional
    TooltipCallback   as IP_TooltipCallbackFunc     ' optional
    ClickCallback     as IP_ClickCallbackSub        ' optional; COMMAND items
    SelChangeCallback as IP_SelChangeCallbackSub    ' optional; TOGGLE items, user only

    declare sub      LayoutItems()
    declare function GetCount() as long                                     ' item count
    declare function AddItem() as CICONPANEL_ITEM ptr                       ' append
    declare function GetItem( byval idx as long ) as CICONPANEL_ITEM ptr
    declare function IsValidItem( byval idx as long ) as boolean
    declare function HitTest( byval x as long, byval y as long ) as long
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' Assign every item its cell. This is the only place item geometry is produced; everything
' else (hit-testing, painting, invalidation) consumes it.
'
'   cellWidth(i) = padBefore(i) + iconWidth(i) + padAfter(i)      TOGGLE / COMMAND
'                = padBefore(i) + sepWidth     + padAfter(i)      SEPARATOR
'   totalW       = sum of every cellWidth
'   x0           = 0                        LEFT, or totalW >= clientW (the overflow rule)
'                = (clientW - totalW) \ 2   CENTER
'                = clientW - totalW         RIGHT
'   rc(i)        = ( x, clientTop, x + cellWidth(i), clientBottom )   full client height
'   rcIcon(i)    = iconW x iconH, centred in rc both ways
'
' TWO THINGS THAT MAKE THIS CONTROL DIFFERENT FROM ITS SIBLINGS:
'
'   1. Nothing is measured. Cell widths are DECLARED by the author, so -- alone in the
'      family -- this layout never takes a DC and never calls GetTextExtentPoint32W. The
'      font matters at paint time only. Layout stays lazy purely to coalesce mutations.
'
'   2. A resize moves x0 and nothing else. Relative spacing is authored and is never
'      adjusted, redistributed, or squeezed. That is the whole difference from CStatusBar,
'      whose panels auto-size to their text and whose spring absorbs the slack.
'
' On overflow the justification degrades to LEFT and the tail clips at the client edge:
' the leading icons stay reachable, and rects are computed honestly rather than squeezed
' (CTabBar's rule) -- clipping is the paint pass's job, and the cursor cannot reach past
' the edge anyway, so hit-testing stays correct for free.
'
' rc spans the FULL client height, so the hot/selected fill reads like a menu bar and the
' hit target includes the item's padding. Only rcIcon obeys the "centred vertically in the
' client area" rule. A host wanting a compact pill highlight fills an inflated rcIcon from
' the paint callback instead.
sub CICONPANEL.LayoutItems()
    this.bLayoutDirty = false
    this.nTotalWidth  = 0
    if this.hWin = 0 then exit sub
    if this.itemCount = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' Resolve every per-item override ONCE, here, so the two passes below cannot disagree
    ' about what an item's padding or icon size is.
    dim padB( 0 to this.itemCount - 1 ) as long
    dim coreW( 0 to this.itemCount - 1 ) as long
    dim coreH( 0 to this.itemCount - 1 ) as long

    dim as long totalW = 0
    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            padB(i) = iif( .nPadBefore < 0, this.nPadBefore, .nPadBefore )
            dim as long padA = iif( .nPadAfter < 0, this.nPadAfter, .nPadAfter )
            ' A separator's rule takes the place of the icon width, but keeps the icon
            ' HEIGHT: the rule then stands exactly as tall as the icons it divides.
            coreH(i) = iif( .nIconHeight = 0, this.nIconHeight, .nIconHeight )
            if .itemKind = IP_KIND_SEPARATOR then
                coreW(i) = this.nSepWidth
            else
                coreW(i) = iif( .nIconWidth = 0, this.nIconWidth, .nIconWidth )
            end if
            ' Never taller than the client -- but only once there IS a client to measure
            ' against (see the zero-geometry bail below).
            if (clientH > 0) andalso (coreH(i) > clientH) then coreH(i) = clientH
            .nCellWidth = padB(i) + coreW(i) + padA
            totalW += .nCellWidth
        end with
    next
    this.nTotalWidth = totalW

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again -- but note this bail
    ' comes AFTER nTotalWidth is computed. The run's width does not depend on the client
    ' at all (only x0 does), and GetIdealWidth is exactly what a host calls to decide how
    ' big to make the panel in the first place: returning 0 until it had already been
    ' sized would be a chicken-and-egg trap.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    ' The block offset: the ENTIRE response to a resize.
    dim as long x0 = rcClient.left
    if totalW < clientW then
        select case this.nJustify
        case IP_JUSTIFY_CENTER
            x0 = rcClient.left + ((clientW - totalW) \ 2)
        case IP_JUSTIFY_RIGHT
            x0 = rcClient.right - totalW
        end select
    end if

    dim as long x = x0
    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            SetRect( @.rc, x, rcClient.top, x + .nCellWidth, rcClient.bottom )
            dim as long yTop = rcClient.top + ((clientH - coreH(i)) \ 2)
            SetRect( @.rcIcon, x + padB(i), yTop, _
                               x + padB(i) + coreW(i), yTop + coreH(i) )
            x += .nCellWidth
        end with
    next
end sub

function CICONPANEL.GetCount() as long
    return this.itemCount
end function

function CICONPANEL.IsValidItem( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.itemCount)
end function

function CICONPANEL.GetItem( byval idx as long ) as CICONPANEL_ITEM ptr
    if this.IsValidItem(idx) = false then return null
    return @this.items(idx)
end function

' Which item contains this client-coordinate point? Separators are excluded -- they are
' decoration, not targets. DISABLED items are NOT excluded: they still occupy their cell,
' and a host asking "what is here" deserves the truth. The hover and click paths add the
' enabled test themselves.
function CICONPANEL.HitTest( byval x as long, byval y as long ) as long
    dim as POINT pt
    pt.x = x
    pt.y = y
    for i as long = 0 to this.itemCount - 1
        if this.items(i).itemKind = IP_KIND_SEPARATOR then continue for
        if PtInRect( @this.items(i).rc, pt ) then return i
    next
    return -1
end function

' Append a fresh (reset) item. Grows the backing store by doubling so building a panel is
' amortized O(1), not O(n^2).
'
' There is NO InsertItemAt and NO DeleteItemAt, deliberately -- the item set is static. See
' the type's index comment for what that buys.
function CICONPANEL.AddItem() as CICONPANEL_ITEM ptr
    dim as long cap = ubound(this.items) + 1
    if this.itemCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.items( 0 to newcap - 1 )
    end if

    dim as long idx = this.itemCount

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.items(idx)
        .itemKind      = IP_KIND_TOGGLE
        .wszGlyph      = ""
        .wszTooltip    = ""
        .id            = 0
        .itemData      = 0
        .isSelected    = false
        .isEnabled     = true
        .nPadBefore    = -1
        .nPadAfter     = -1
        .nIconWidth    = 0
        .nIconHeight   = 0
        .ForeColorItem = 0
        .bHasForeColor = false
        .nCellWidth    = 0
        SetRectEmpty( @.rc )
        SetRectEmpty( @.rcIcon )
    end with

    this.itemCount += 1
    this.Refresh()
    return @this.items(idx)
end function

' Forget any live press WITHOUT touching the capture. Called from the capture-loss path and
' from SetEnabled when the pressed item is disabled out from under the gesture. Releasing
' capture is the WndProc's job, and only on the up-message or WM_DESTROY -- doing it here
' would let a callback strand or double-release it.
sub CICONPANEL.CancelPress()
    this.nPressedIdx = -1
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of Add calls costs one layout pass, not N.
sub CICONPANEL.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking run is cleared --
    ' with three justifications the run can move wholesale, so this matters more here than
    ' anywhere else in the family.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' AN ICON STRIP WHERE THE AUTHOR OWNS THE SPACING
'   Every item declares its own cell: an icon size plus the space before and after it. The
'   control lays the cells out in the order supplied and NEVER adjusts them -- no
'   auto-sizing, no spring panel, no squeezing to fit. The one thing it recomputes when the
'   host resizes is where the whole run sits: left, centred, or right, as one block.
'
'   That is the difference from CStatusBar, whose panels measure themselves against their
'   text and whose nominated spring panel absorbs all slack.
'
' STATIC BY CONTRACT
'   Items are added once, at construction, and are never inserted, deleted or moved
'   afterwards -- which is why there is no InsertItem, no DeleteItem and no Clear. Their
'   widths are declared rather than measured, so they do not move at runtime either; only
'   the justification moves them, and only as one block.
'
'   What that buys: none of the stored-index fix-up code the dynamic siblings carry, because
'   nLastHotIdx and nPressedIdx cannot go stale. Everything a host does at runtime --
'   toggling, enabling, recolouring, swapping a glyph -- addresses an item that is still
'   exactly where it was.
'
' THE CONTROL HANDLE
'   Every CIconPanel_* function takes the handle returned by CIconPanel_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need
'   to treat the control as a window, e.g. SetWindowPos() to place and size it. An opaque
'   wrapper would buy a little type-safety and break that, so it was rejected.
'
' SELECTION IS A SET, NOT A CURRENT ITEM
'   Every TOGGLE item latches independently -- there is no nCurSel here. Programmatic
'   SetSelected is SILENT; only the user fires SelChangeCallback.
'
' LIFETIME
'   The control frees itself when its window is destroyed. Fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized: position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function CIconPanel_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Building the panel.  Add* return the new item's index, or -1.
'   AddItem's kind is IP_KIND_TOGGLE or IP_KIND_COMMAND; AddSeparator adds the third kind
'   (which has no glyph, no id and no click behaviour of its own).
'
'   These are the ONLY way items come into existence, and there is no way to take one out
'   again -- the static contract above. Build the panel once, then drive it with the state
'   setters (SetSelected, SetEnabled, SetGlyph, SetItemForeColor).
' ----------------------------------------------------------------------------------------
declare function CIconPanel_AddItem( byval hIconPanel as HWND, byval itemKind as long, byval Glyph as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
declare function CIconPanel_AddSeparator( byval hIconPanel as HWND ) as long
declare sub      CIconPanel_Refresh( byval hIconPanel as HWND )

' ----------------------------------------------------------------------------------------
' Counts and lookup.
'   FindItemByID returns the FIRST item with that id, or -1. HitTest takes CLIENT
'   coordinates, forces a pending layout, and returns the item there (separators excluded,
'   disabled items included) or -1.
' ----------------------------------------------------------------------------------------
declare function CIconPanel_GetCount( byval hIconPanel as HWND ) as long
declare function CIconPanel_IsValidItem( byval hIconPanel as HWND, byval idx as long ) as boolean
declare function CIconPanel_FindItemByID( byval hIconPanel as HWND, byval id as long ) as long
declare function CIconPanel_HitTest( byval hIconPanel as HWND, byval x as long, byval y as long ) as long

' ----------------------------------------------------------------------------------------
' Item contents and state.  Set* return FALSE for an invalid item index.
'
'   SetSelected is SILENT (no SelChangeCallback) and applies to TOGGLE items only -- it is
'   how a host restores latched state at startup, and it is safe to call from inside the
'   change handler. SetEnabled(false) greys the item, stops it going hot, and makes it
'   swallow clicks; disabling the item under a live press cancels the press.
' ----------------------------------------------------------------------------------------
declare function CIconPanel_GetItemKind( byval hIconPanel as HWND, byval idx as long ) as long
declare function CIconPanel_GetGlyph( byval hIconPanel as HWND, byval idx as long ) as DWSTRING
declare function CIconPanel_SetGlyph( byval hIconPanel as HWND, byval idx as long, byval Glyph as DWSTRING ) as boolean
declare function CIconPanel_GetItemID( byval hIconPanel as HWND, byval idx as long ) as long
declare function CIconPanel_SetItemID( byval hIconPanel as HWND, byval idx as long, byval id as long ) as boolean
declare function CIconPanel_GetItemData( byval hIconPanel as HWND, byval idx as long ) as integer
declare function CIconPanel_SetItemData( byval hIconPanel as HWND, byval idx as long, byval itemData as integer ) as boolean
declare function CIconPanel_GetSelected( byval hIconPanel as HWND, byval idx as long ) as boolean
declare function CIconPanel_SetSelected( byval hIconPanel as HWND, byval idx as long, byval isSelected as boolean ) as boolean
declare function CIconPanel_GetEnabled( byval hIconPanel as HWND, byval idx as long ) as boolean
declare function CIconPanel_SetEnabled( byval hIconPanel as HWND, byval idx as long, byval isEnabled as boolean ) as boolean

' ----------------------------------------------------------------------------------------
' Layout.
'
'   SetJustify moves the whole run as one block and is re-applied on every resize. When the
'   run is wider than the client area the justification degrades to LEFT and the tail clips
'   at the right edge.
'
'   Panel-level SetIconSize / SetPadding / SetSeparatorWidth take RAW PIXELS -- the caller
'   DPI-scales (the family rule; only the Create-time defaults are scaled for you). The
'   per-item setters override them: pass -1 for a padding or 0 for an icon dimension to go
'   back to inheriting.
'
'   GetIdealWidth is the summed width of every cell -- size the panel with it (or centre a
'   fixed-width panel yourself). Rects are computed lazily; every query here forces a
'   pending layout, so results are always current.
' ----------------------------------------------------------------------------------------
declare function CIconPanel_GetJustify( byval hIconPanel as HWND ) as long
declare sub      CIconPanel_SetJustify( byval hIconPanel as HWND, byval nJustify as long )
declare sub      CIconPanel_GetIconSize( byval hIconPanel as HWND, byref nIconWidth as long, byref nIconHeight as long )
declare sub      CIconPanel_SetIconSize( byval hIconPanel as HWND, byval nIconWidth as long, byval nIconHeight as long )
declare sub      CIconPanel_GetPadding( byval hIconPanel as HWND, byref nPadBefore as long, byref nPadAfter as long )
declare sub      CIconPanel_SetPadding( byval hIconPanel as HWND, byval nPadBefore as long, byval nPadAfter as long )
declare function CIconPanel_GetSeparatorWidth( byval hIconPanel as HWND ) as long
declare sub      CIconPanel_SetSeparatorWidth( byval hIconPanel as HWND, byval nSepWidth as long )
declare function CIconPanel_SetItemPadding( byval hIconPanel as HWND, byval idx as long, byval nPadBefore as long, byval nPadAfter as long ) as boolean
declare function CIconPanel_SetItemIconSize( byval hIconPanel as HWND, byval idx as long, byval nIconWidth as long, byval nIconHeight as long ) as boolean
declare function CIconPanel_GetItemRect( byval hIconPanel as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function CIconPanel_GetItemIconRect( byval hIconPanel as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function CIconPanel_GetIdealWidth( byval hIconPanel as HWND ) as long

' ----------------------------------------------------------------------------------------
' Appearance.  The font is borrowed, never owned: keep it alive and destroy it yourself.
' It is a PAINT-time input only -- nothing here is measured, so changing it never changes
' the layout.
'
'   SetColors copies the whole struct. GetColors fills one out, so the read-modify-write
'   "change one color" idiom is Get, assign, Set.
'
'   SetItemForeColor overrides the IDLE glyph color of one item (a red record dot, a green
'   run arrow); hot, selected and disabled keep the panel's colors, so a hover always
'   speaks the panel's language. ClearItemForeColor goes back to the panel's ForeColor.
' ----------------------------------------------------------------------------------------
declare sub      CIconPanel_GetColors( byval hIconPanel as HWND, byval pColors as CICONPANEL_COLORS ptr )
declare sub      CIconPanel_SetColors( byval hIconPanel as HWND, byval pColors as CICONPANEL_COLORS ptr )
declare function CIconPanel_SetItemForeColor( byval hIconPanel as HWND, byval idx as long, byval clr as COLORREF ) as boolean
declare function CIconPanel_ClearItemForeColor( byval hIconPanel as HWND, byval idx as long ) as boolean
declare function CIconPanel_GetFont( byval hIconPanel as HWND ) as HFONT
declare function CIconPanel_SetFont( byval hIconPanel as HWND, byval hIconFont as HFONT ) as boolean
declare function CIconPanel_GetTooltipHandle( byval hIconPanel as HWND ) as HWND
declare sub      CIconPanel_SetHoverTime( byval hIconPanel as HWND, byval milliseconds as long )

' ----------------------------------------------------------------------------------------
' Tooltips.  Per-item text wins; the callback is consulted only for items with none; an
' item with neither shows no tip.
' ----------------------------------------------------------------------------------------
declare function CIconPanel_GetTooltipText( byval hIconPanel as HWND, byval idx as long ) as DWSTRING
declare function CIconPanel_SetTooltipText( byval hIconPanel as HWND, byval idx as long, byval Text as DWSTRING ) as boolean

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintItemCallback - draw one item INSTEAD OF the built-in painter (which handles every
'                       item, including separators, when this is unset).
'   MessageCallback   - observe mouse messages; return TRUE to suppress default handling
'                       (IGNORED for WM_LBUTTONUP -- capture-critical).
'   TooltipCallback   - supply tooltip text on demand for items without their own.
'   ClickCallback     - a COMMAND item was clicked (matched press+release).
'   SelChangeCallback - a TOGGLE item latched/unlatched. Programmatic SetSelected is silent.
' ----------------------------------------------------------------------------------------
declare sub      CIconPanel_SetPaintItemCallback( byval hIconPanel as HWND, byval usersub as IP_PaintItemCallbackSub )
declare sub      CIconPanel_SetMessageCallback( byval hIconPanel as HWND, byval userfunc as IP_MessageCallbackFunc )
declare sub      CIconPanel_SetTooltipCallback( byval hIconPanel as HWND, byval userfunc as IP_TooltipCallbackFunc )
declare sub      CIconPanel_SetClickCallback( byval hIconPanel as HWND, byval usersub as IP_ClickCallbackSub )
declare sub      CIconPanel_SetSelChangeCallback( byval hIconPanel as HWND, byval usersub as IP_SelChangeCallbackSub )
