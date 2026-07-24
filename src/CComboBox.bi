''
''  CComboBox.bi  --  owner-drawn dropdown selector (button + checkmarked list)
''

#pragma once

' BOTH dependencies are included HERE rather than left to the call site. CListBox.bi names
' typedefs it does not include and so only compiles where the host happens to have pre-loaded
' them (see CLAUDE.md); this file deliberately does not repeat that trap.
'
' A host that already vendors CPopupMenu.bi gets ONE copy: #include once dedupes by resolved
' path, which is how tiko already shares a single copy between CTextBox and CMenuBar.
'
' ONE THING CBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "CBufferPaint.bi"
#include once "CPopupMenu.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
#define IDT_CCOMBOBOX_HOTTRACK   &hCB80
#define CCOMBOBOX_HOTTRACK_MS    100

' Default geometry. Everything except the two THICKNESS values is DPI-scaled once at Create;
' every setter afterwards takes raw pixels and the caller scales (the family rule).
#define CCOMBOBOX_DEFAULT_PADLEFT     12
#define CCOMBOBOX_DEFAULT_PADRIGHT    10
#define CCOMBOBOX_DEFAULT_PADTOP       6
#define CCOMBOBOX_DEFAULT_PADBOTTOM    6
' Gap between the caption and the chevron. Only spent when the caption is shown -- in
' arrow-only mode there is nothing to separate the chevron FROM, so it is not added.
#define CCOMBOBOX_DEFAULT_TEXTGAP      8
#define CCOMBOBOX_DEFAULT_CHEVRONW     9
#define CCOMBOBOX_DEFAULT_CHEVRONH    12
' Vertical air between the up chevron and the down chevron, inside CHEVRONH.
#define CCOMBOBOX_DEFAULT_CHEVRONGAP   3
' Corner curvature of the button chrome. This is an ellipse DIAMETER, not a radius --
' CBufferPaint keeps GDI's vocabulary and halves it internally, so 12 draws a 6px radius.
' Twelve controls pass GDI-flavoured numbers there; do not "fix" it to a radius here.
#define CCOMBOBOX_DEFAULT_CURVATURE   12
' The button reads BORDERLESS out of the box (see CCOMBOBOX_COLORS), so a border thickness
' of 1 costs nothing until a host sets a contrasting BorderColor. Zero disables it outright.
#define CCOMBOBOX_DEFAULT_BORDERTHICK  1     ' not DPI-scaled
#define CCOMBOBOX_DEFAULT_FOCUSGAP     2
#define CCOMBOBOX_DEFAULT_FOCUSTHICK   1     ' not DPI-scaled


' Whether the button shows the selected item's caption, or only the chevron.
'
'   CBO_TEXT_ALWAYS        the caption is always drawn. With nothing selected that means the
'                          placeholder, or an empty button. The width never changes.
'   CBO_TEXT_NEVER         arrow-only, permanently. The control still HAS a selection and the
'                          list still checkmarks it; only the caption is suppressed.
'   CBO_TEXT_WHENSELECTED  arrow-only WHILE nothing is selected, and a normal captioned
'                          combobox once something is. The button collapses to just the
'                          chevron until the user answers it.
'
' THE THIRD MODE IS THE ONLY ONE WHOSE WIDTH MOVES, and it is worth being clear about why
' that is not a contradiction of the widest-item rule. That rule exists so the button does not
' resize as the user picks DIFFERENT items -- and it still holds: every selected state has the
' same width. What changes is the -1 <-> selected boundary, which is a one-time transition
' from "unanswered" to "answered", and the whole point of the mode is that it be visible.
'
' Practical consequence: in this mode, either turn on CComboBox_SetAutoSize, or re-place the
' control from your SelChange handler. Left alone with a fixed size, the control keeps its
' collapsed width and the caption arrives ellipsized into a chevron-sized box.
enum
    CBO_TEXT_ALWAYS = 0
    CBO_TEXT_NEVER
    CBO_TEXT_WHENSELECTED
end enum


' Colors for the built-in painter. Copied on Set.
'
' FOUR MOODS, not five. There is deliberately no PRESSED set: this control opens its list on
' mouse-DOWN, so "pressed" and "open" are the same instant and collapse into one state.
' Painter precedence is  disabled > open > hot > idle  -- OPEN BEATS HOT on purpose, because
' while the list is up the cursor is normally over the LIST, and the button must stay lit.
'
' THE BUTTON READS BORDERLESS BY DEFAULT because every BorderColorXxx is defaulted EQUAL to
' the matching BackColorXxx -- the same trick CToggle uses for its ON pill and CSelectBar for
' its hot cell. A host that wants a visible outline just sets the three fields.
'
' THE CHEVRON FOLLOWS THE TEXT BY DEFAULT for the same reason: each ChevronColorXxx is
' defaulted EQUAL to the matching ForeColorXxx, which is what the reference screenshot shows
' (caption and chevron turn blue together). Set them to break that coupling.
'
' BackColorOpen is defaulted EQUAL to BackColor, not to BackColorHot: in the reference the
' open button's FILL does not change at all, only its caption and chevron turn blue.
type CCOMBOBOX_COLORS
    ' --- idle ---
    BackColor            as COLORREF = BGR( 44, 49, 58)
    ForeColor            as COLORREF = BGR(200,205,214)
    BorderColor          as COLORREF = BGR( 44, 49, 58)   ' = BackColor (see above)
    ChevronColor         as COLORREF = BGR(200,205,214)   ' = ForeColor (see above)
    ' --- hot (cursor over the button, list closed) ---
    BackColorHot         as COLORREF = BGR( 55, 61, 72)
    ForeColorHot         as COLORREF = BGR(226,230,238)
    BorderColorHot       as COLORREF = BGR( 55, 61, 72)   ' = BackColorHot
    ChevronColorHot      as COLORREF = BGR(226,230,238)   ' = ForeColorHot
    ' --- open (the list is dropped) ---
    BackColorOpen        as COLORREF = BGR( 44, 49, 58)   ' = BackColor (see above)
    ForeColorOpen        as COLORREF = BGR( 97,175,239)
    BorderColorOpen      as COLORREF = BGR( 44, 49, 58)   ' = BackColorOpen
    ChevronColorOpen     as COLORREF = BGR( 97,175,239)   ' = ForeColorOpen
    ' --- disabled ---
    BackColorDisabled    as COLORREF = BGR( 40, 44, 51)
    ForeColorDisabled    as COLORREF = BGR( 96,102,112)
    BorderColorDisabled  as COLORREF = BGR( 40, 44, 51)   ' = BackColorDisabled
    ChevronColorDisabled as COLORREF = BGR( 96,102,112)   ' = ForeColorDisabled
    ' --- everything else ---
    ' Drawn instead of ForeColor when nCurSel = -1 and a placeholder string is set. Only the
    ' idle/hot/open distinction is dropped for it: a placeholder is already a muted state and
    ' four more fields would earn nothing.
    ForeColorPlaceholder as COLORREF = BGR(120,126,136)
    FocusRingColor       as COLORREF = BGR( 97,175,239)
end type


' One item. Everything above the DERIVED line is authored.
'
' NOTE there is no wszTooltip and no per-item rect. This control HAS NO TOOLTIPS (by request),
' and the items are drawn by CPopupMenu inside the dropdown -- CComboBox never lays them out,
' so there is no geometry to store. The only measured quantity that comes off the item set is
' nTextWidth on the control, the widest caption, which sizes the BUTTON.
'
' `id` is a HOST PAYLOAD and is never used as a command id. The dropdown addresses rows by
' (model index + 1), so a host is free to use whatever id space it likes without colliding
' with anything -- the trap recorded in Learnings.md under "Replacing TrackPopupMenu".
type CCOMBOBOX_ITEM
    wszText   as DWSTRING
    id        as long = 0        ' free-form host id
    itemData  as integer = 0     ' free-form host payload
    isEnabled as boolean = true  ' false = greyed in the list, never selectable
end type


' Everything the painter needs. All four rects are precomputed by LayoutCombo -- never
' re-derive one from another, and in particular never re-apply the padding to rcButton to get
' rcText, because the chevron size and the text gap can change underneath you.
type CCOMBOBOX_PAINTINFO
    hCombo     as HWND                 ' the control, so the callback can query it
    b          as CBufferPaint ptr     ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutCombo ---
    rcClient   as RECT                 ' the whole client area
    rcButton   as RECT                 ' the chrome: rcClient deflated by the focus-ring band
    rcText     as RECT                 ' the caption box (empty in arrow-only mode)
    rcChevron  as RECT                 ' the stacked up/down chevron cell
    rcVisual   as RECT                 ' rcButton inflated by the focus-ring band
    ' --- State: the control decides these, you only render. ---
    isHot      as boolean              ' the mouse is over the control (list closed)
    isOpen     as boolean              ' the dropdown is showing
    isEnabled  as boolean
    isFocused  as boolean              ' draw a focus ring when true
    ' --- What to draw ---
    nCurSel    as long                 ' -1 = nothing selected
    wszText    as DWSTRING             ' the selected caption, or the placeholder, or ""
    isPlaceholder as boolean           ' wszText is the placeholder, not a real selection
    ' Is the caption drawn at all right now? Resolved from the text mode and the selection, so
    ' a callback never has to re-derive it. FALSE means rcText is EMPTY and only the chevron
    ' should be drawn -- do not fall back to painting wszText somewhere of your own choosing.
    isTextVisible as boolean
end type

type CCOMBOBOX_MESSAGEINFO
    hCombo     as HWND
    uMsg       as UINT
    wParam     as WPARAM
    lParam     as LPARAM
end type


' Draw the whole BUTTON, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' This callback owns the button only. The DROPDOWN is a CPopupMenu and is painted by its own
' PM_PaintItemCallbackSub -- reach it with CComboBox_GetListHandle().
'
' The control has already filled the client with the mood's BackColor before calling you, so a
' callback that only wants to add something on top does not have to repaint the background.
' Draw with the SAME font you handed to CComboBox_SetFont: the button width was measured with
' it, and a different font means the width lies.
type CBO_PaintCallbackSub as sub( byval p as CCOMBOBOX_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' Like CToggle -- and unlike the mouse-only siblings -- this control hands over the FOCUS and
' KEYBOARD messages as well as the mouse ones, because they carry real state here.
'
' CAUTION: the result is IGNORED for WM_SETFOCUS and WM_KILLFOCUS. Focus is a FACT the system
' reports, not an action to veto; the state is already updated by the time you are called, so
' there is nothing left to suppress. A host that does not want the control focusable must not
' make it a tabstop.
'
' NOTE what is NOT on that ignore list, and why. Every capture-taking sibling also ignores the
' result for WM_LBUTTONUP, because suppressing it would strand mouse capture. THIS CONTROL
' TAKES NO CAPTURE (the list opens on the DOWN and the popup owns the mouse from that instant),
' so there is no bookkeeping to strand and the up-message is honoured like any other. Do not
' copy that caution in from a sibling -- here it would be a lie.
type CBO_MessageCallbackFunc as function( byval m as CCOMBOBOX_MESSAGEINFO ptr ) as boolean

' The selection changed through USER interaction -- a pick from the list, or an arrow/Home/End
' key while the list is closed. Fired AFTER the control's state is updated, so
' CComboBox_GetCurSel() = idxNew.
'
' The programmatic CComboBox_SetCurSel does NOT fire this (Win32's CB_SETCURSEL / CBN_SELCHANGE
' precedent), which is also what makes it safe to call SetCurSel from inside the handler.
' Picking the item that is ALREADY current is not a change and fires nothing.
type CBO_SelChangeCallbackSub as sub( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )

' The dropdown is about to open (isOpen = TRUE, fired BEFORE the rows are built) or has just
' closed (isOpen = FALSE). The opening edge is the just-in-time hook: rebuild a dynamic item
' set here with Clear + AddItem and it is picked up by the same layout pass that follows.
'
' Fired for USER interaction and for the programmatic CComboBox_DropDown / CComboBox_CloseUp
' alike -- unlike the selection callbacks. It reports a WINDOW-STATE transition rather than a
' value change, and a host that rebuilds items on open needs that to run however the list was
' opened.
type CBO_DropDownCallbackSub as sub( byval hCombo as HWND, byval isOpen as boolean )


type CCOMBOBOX
    hWin              as HWND
    idc_ComboBox      as long = 0
    items(any)        as CCOMBOBOX_ITEM
    itemCount         as long = 0
    ' --- Selection and state ---
    ' nCurSel is a MODEL index, or -1 for "nothing selected" (which is legal here, unlike
    ' CSelectBar's always-one contract -- a picker that has not been answered yet is a real
    ' state). The item set is DYNAMIC, so every structural mutation must fix this up; the
    ' three sites are AddItemSlot / InsertItemSlot / RemoveItemAt, plus Clear.
    nCurSel           as long = -1
    isEnabled         as boolean = true
    isHot             as boolean = false   ' the mouse is over the client
    isFocused         as boolean = false
    hotTimerOn        as boolean = false   ' is the hot-tracking safety-net timer running?
    ' --- The dropdown. One CPopupMenu, created lazily on first open and owned for the
    '     control's lifetime (destroyed in WM_DESTROY). Its rows are rebuilt from items()
    '     on every open, so the two can never drift.
    '     NO MOUSE CAPTURE IS EVER TAKEN. The family's test -- "take capture only if
    '     something consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing" -- comes
    '     out negative: the down opens the list and the popup owns the mouse from there, so
    '     there is no press/cancel gesture to protect. None of the capture price is paid and
    '     there is no CancelPress in this control. ---
    hList             as HWND
    isOpen            as boolean = false
    ' Has the host set the dropdown's colors itself? When it has NOT, the list's colors are
    ' re-derived from CCOMBOBOX_COLORS every time CComboBox_SetColors runs, so theming the
    ' button themes the list for free. CComboBox_SetListColors sets this and the derivation
    ' stands down permanently -- an explicit choice is never overwritten.
    bListColorsCustom as boolean = false
    ' Set by CComboBox_FilterMessage when a click lands on THIS button while the list is
    ' open, and consumed by the very next WM_LBUTTONDOWN. CPopupMenu's filter dismisses on
    ' an outside click and then returns FALSE ON PURPOSE so the click still reaches its
    ' target -- which, for a click on our own button, would close the list and immediately
    ' re-open it. This flag is the guard. Deterministic; no timer, no close-time heuristic.
    bSuppressNextOpen as boolean = false
    colors            as CCOMBOBOX_COLORS
    wszPlaceholder    as DWSTRING          ' drawn when nCurSel = -1; "" = draw nothing
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member procedure
    ' of this type, and "dim as HFONT x" there fails with a misleading "error 14: Expected
    ' identifier, found 'HFONT'". See C:\dev\Learnings.md.
    hTextFont         as HFONT
    ' --- Layout inputs. All pixels; DPI-scaled once at Create, raw thereafter. ---
    ' Never read this directly to decide whether to draw a caption -- CBO_TEXT_WHENSELECTED
    ' also depends on nCurSel. IsTextVisible() is the single resolver and everything goes
    ' through it: the layout, the painter, and the PAINTINFO flag handed to callbacks.
    nTextMode         as long = CBO_TEXT_ALWAYS
    bAutoSize         as boolean = false   ' opt-in: the control SetWindowPos's ITSELF
    nPadLeft          as long = CCOMBOBOX_DEFAULT_PADLEFT
    nPadRight         as long = CCOMBOBOX_DEFAULT_PADRIGHT
    nPadTop           as long = CCOMBOBOX_DEFAULT_PADTOP
    nPadBottom        as long = CCOMBOBOX_DEFAULT_PADBOTTOM
    nTextGap          as long = CCOMBOBOX_DEFAULT_TEXTGAP
    nChevronWidth     as long = CCOMBOBOX_DEFAULT_CHEVRONW
    nChevronHeight    as long = CCOMBOBOX_DEFAULT_CHEVRONH
    nChevronGap       as long = CCOMBOBOX_DEFAULT_CHEVRONGAP
    nChevronThick     as long = 1          ' NOT DPI-scaled here (see the note on the setter)
    nCurvature        as long = CCOMBOBOX_DEFAULT_CURVATURE   ' ellipse DIAMETER
    nBorderThick      as long = CCOMBOBOX_DEFAULT_BORDERTHICK ' NOT DPI-scaled
    nFocusGap         as long = CCOMBOBOX_DEFAULT_FOCUSGAP
    nFocusThick       as long = CCOMBOBOX_DEFAULT_FOCUSTHICK  ' NOT DPI-scaled
    ' --- Layout outputs. Rects are DERIVED, never set from outside; LayoutCombo() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect/size query) runs
    '     it, which coalesces a burst of AddItem calls into ONE measuring pass. ---
    nTextWidth        as long = 0     ' derived: the WIDEST caption, which is what sizes us
    nTextHeight       as long = 0     ' derived: one height for the control, from tmHeight
    nIdealW           as long = 0     ' derived
    nIdealH           as long = 0     ' derived
    rcButton          as RECT         ' derived
    rcText            as RECT         ' derived
    rcChevron         as RECT         ' derived
    rcVisual          as RECT         ' derived
    bLayoutDirty      as boolean = true
    PaintCallback     as CBO_PaintCallbackSub       ' optional; replaces the built-in painter
    MessageCallback   as CBO_MessageCallbackFunc    ' optional
    SelChangeCallback as CBO_SelChangeCallbackSub   ' optional; user-driven changes only
    DropDownCallback  as CBO_DropDownCallbackSub    ' optional

    declare sub      LayoutCombo()
    declare function RingPad() as long
    declare function IsTextVisible() as boolean
    declare function GetCount() as long
    declare function IsValidItem( byval idx as long ) as boolean
    declare function GetItem( byval idx as long ) as CCOMBOBOX_ITEM ptr
    declare function AddItemSlot() as CCOMBOBOX_ITEM ptr
    declare function InsertItemSlot( byval idx as long ) as CCOMBOBOX_ITEM ptr
    declare function RemoveItemAt( byval idx as long ) as boolean
    declare function NextSelectable( byval nFrom as long, byval nDir as long ) as long
    declare function DisplayText( byref bIsPlaceholder as boolean ) as DWSTRING
    declare sub      Clear()
    declare sub      Refresh()
end type


' The distance the visual bounds extend beyond the button chrome on every side. The focus ring
' is drawn INSIDE this band, and the band is reserved UNCONDITIONALLY -- whether or not the
' control currently has focus -- which is what stops the button jumping when focus arrives.
' It therefore contributes to the IDEAL SIZE (CToggle's rule).
function CCOMBOBOX.RingPad() as long
    return this.nFocusGap + this.nFocusThick
end function

' Is a caption drawn at the moment? The ONE resolver for the text mode -- the layout, the
' painter and the PAINTINFO flag all come through here, so they cannot disagree.
'
' CBO_TEXT_WHENSELECTED reads nCurSel, which is why nTextMode must never be tested directly:
' the answer depends on state, not just on the author's choice.
function CCOMBOBOX.IsTextVisible() as boolean
    select case this.nTextMode
    case CBO_TEXT_NEVER
        return false
    case CBO_TEXT_WHENSELECTED
        return this.IsValidItem( this.nCurSel )
    end select
    return true                                   ' CBO_TEXT_ALWAYS
end function

function CCOMBOBOX.GetCount() as long
    return this.itemCount
end function

function CCOMBOBOX.IsValidItem( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.itemCount)
end function

function CCOMBOBOX.GetItem( byval idx as long ) as CCOMBOBOX_ITEM ptr
    if this.IsValidItem(idx) = false then return null
    return @this.items(idx)
end function


' The caption that represents the current state, and whether it is the placeholder.
'
' Three cases, in order: a valid selection gives its caption; no selection with a placeholder
' set gives the placeholder (to be drawn in ForeColorPlaceholder); no selection and no
' placeholder gives nothing at all. The width is unaffected either way -- it always comes from
' the widest ITEM, so a combobox does not shrink to fit a short placeholder and then jump on
' the first pick.
'
' THIS IS INDEPENDENT OF WHETHER THE CAPTION IS ACTUALLY DRAWN. IsTextVisible() decides that,
' and in CBO_TEXT_NEVER or a collapsed CBO_TEXT_WHENSELECTED the answer here is still computed
' and handed to the paint callback as information -- the callback is told not to draw it by
' PAINTINFO's isTextVisible. One consequence worth knowing: a placeholder can never appear in
' CBO_TEXT_WHENSELECTED, because the only state that would show it is the collapsed one.
function CCOMBOBOX.DisplayText( byref bIsPlaceholder as boolean ) as DWSTRING
    bIsPlaceholder = false
    if this.IsValidItem( this.nCurSel ) then
        dim as DWSTRING wszOut = this.items(this.nCurSel).wszText
        return wszOut
    end if
    if len( this.wszPlaceholder ) > 0 then
        bIsPlaceholder = true
        dim as DWSTRING wszOut = this.wszPlaceholder
        return wszOut
    end if
    return ""
end function


' The next ENABLED item at or past nFrom, walking in nDir (+1 / -1). Returns -1 when there is
' none in that direction.
'
' IT CLAMPS, IT DOES NOT WRAP -- deliberately, and this is where it differs from
' CPopupMenu.NextSelectable, which wraps because menu semantics want that. Win32's own combobox
' arrow keys stop at the ends, and a wrapping Down at the last item would silently jump the
' user back to the first without any list being visible to explain it.
function CCOMBOBOX.NextSelectable( byval nFrom as long, byval nDir as long ) as long
    if this.itemCount = 0 then return -1
    if nDir = 0 then return -1
    dim as long i = nFrom + nDir
    do while (i >= 0) andalso (i < this.itemCount)
        if this.items(i).isEnabled then return i
        i += nDir
    loop
    return -1
end function


' Append a fresh (reset) item. Grows the backing store by doubling so building a list is
' amortized O(1), not O(n^2).
'
' NO SELECTION FIX-UP IS NEEDED HERE and that is worth stating rather than leaving as an
' apparent omission: appending cannot move any existing item's index. It also does NOT
' auto-select the first item the way CSelectBar does, because -1 is a legal state here.
function CCOMBOBOX.AddItemSlot() as CCOMBOBOX_ITEM ptr
    dim as long cap = ubound(this.items) + 1
    if this.itemCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.items( 0 to newcap - 1 )
    end if

    dim as long idx = this.itemCount

    ' reset the new slot (frees any DWSTRING left in a recycled capacity slot)
    with this.items(idx)
        .wszText   = ""
        .id        = 0
        .itemData  = 0
        .isEnabled = true
    end with

    this.itemCount += 1
    this.Refresh()
    return @this.items(idx)
end function


' Insert a fresh (reset) item BEFORE idx, shifting everything from idx up by one. idx equal to
' the count appends. Returns null for an out-of-range index.
'
' SELECTION FIX-UP SITE 1 OF 3: an insertion at or before the current item pushes it up one.
function CCOMBOBOX.InsertItemSlot( byval idx as long ) as CCOMBOBOX_ITEM ptr
    if idx < 0 then return null
    if idx > this.itemCount then return null
    if idx = this.itemCount then return this.AddItemSlot()

    ' Grow through AddItemSlot so the doubling logic lives in exactly one place, then shuffle
    ' the tail up. AddItemSlot has already bumped itemCount and marked the layout dirty.
    if this.AddItemSlot() = null then return null

    for i as long = this.itemCount - 1 to idx + 1 step -1
        this.items(i) = this.items(i - 1)
    next

    with this.items(idx)
        .wszText   = ""
        .id        = 0
        .itemData  = 0
        .isEnabled = true
    end with

    if this.nCurSel >= idx then this.nCurSel += 1
    return @this.items(idx)
end function


' Remove one item, shifting the tail down.
'
' SELECTION FIX-UP SITE 2 OF 3, and the one with a real choice in it: deleting the CURRENT
' item clears the selection to -1 rather than sliding it onto the neighbour. -1 is a legal
' state here, and silently re-pointing the selection at a different value is the kind of thing
' a host discovers by shipping it. Deleting BELOW the current item shifts it down by one.
function CCOMBOBOX.RemoveItemAt( byval idx as long ) as boolean
    if this.IsValidItem(idx) = false then return false

    for i as long = idx to this.itemCount - 2
        this.items(i) = this.items(i + 1)
    next
    ' Release the vacated tail slot's string; it stays in the backing store as spare capacity.
    this.items(this.itemCount - 1).wszText = ""
    this.itemCount -= 1

    if this.nCurSel = idx then
        this.nCurSel = -1
    elseif this.nCurSel > idx then
        this.nCurSel -= 1
    end if

    this.Refresh()
    return true
end function


' Tear the list down to nothing.
'
' SELECTION FIX-UP SITE 3 OF 3, and the trivial one: nothing is left to point at.
sub CCOMBOBOX.Clear()
    for i as long = 0 to this.itemCount - 1
        this.items(i).wszText = ""
    next
    this.itemCount = 0
    this.nCurSel   = -1
    this.Refresh()
end sub


' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of AddItem calls costs one measuring pass, not N.
sub CCOMBOBOX.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking button is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' Measure the item set and assign the four rects. This is the only place combobox geometry is
' produced; painting, hit-testing, invalidation and every public size query consume it.
'
'   ringPad     = nFocusGap + nFocusThick        reserved ALWAYS, focused or not
'   textW       = MAX over items of the measured caption width, 0 when no caption is drawn
'   textH       = GetTextMetrics.tmHeight        ONE height for the control
'   contentH    = max( textH, nChevronHeight )
'
'   nIdealW     = 2*ringPad + nPadLeft + [textW + nTextGap] + nChevronWidth + nPadRight
'   nIdealH     = 2*ringPad + nPadTop  + contentH + nPadBottom
'
'   rcButton    = rcClient deflated by ringPad on all four sides
'   rcChevron   = right-aligned inside rcButton at nPadRight, v-centred, nChevronWidth wide
'   rcText      = from rcButton.left + nPadLeft to rcChevron.left - nTextGap, v-centred on
'                 textH.  EMPTY in arrow-only mode.
'   rcVisual    = rcButton inflated by ringPad  ( = rcClient when the host sized us ideally )
'
' WHY textW IS THE WIDEST ITEM AND NOT THE SELECTED ONE: the button must not change width when
' the selection changes. A combobox that grows and shrinks as the user picks reflows whatever
' surrounds it and drags its own dropdown anchor sideways.
'
' THE ONE PLACE THE WIDTH DOES MOVE is the -1 <-> selected boundary under
' CBO_TEXT_WHENSELECTED, and that is the mode's whole purpose rather than an exception to the
' rule above: every SELECTED state still has the same width as every other. The callers that
' cross that boundary (SetCurSel, the user-selection path, SetItemEnabled disabling the current
' item) mark the layout dirty only when the boundary is actually crossed, so an ordinary pick
' still costs no re-measure.
'
' WHY THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area at all, and CComboBox_GetIdealSize is exactly what a host calls to decide how big
' to make the control in the FIRST place. Returning 0 until it had already been sized would be
' a chicken-and-egg trap (CSelectBar's nTotalWidth and CToggle's GetIdealSize both dodge it,
' by different means -- this control needs a DC, so it dodges it by ordering).
'
' OVERFLOW: when the client is smaller than ideal the rects are computed HONESTLY rather than
' squeezed (CIconPanel's and CTabBar's rule) -- the chevron keeps its size and stays pinned to
' the right, and rcText is what collapses, degenerating to an empty rect rather than a negative
' one. So a too-narrow combobox loses its caption and keeps its arrow, which is the useful
' failure.
sub CCOMBOBOX.LayoutCombo()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    ' ---- the measuring pass ------------------------------------------------------------
    this.nTextWidth  = 0
    this.nTextHeight = 0

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    this.nTextHeight = tm.tmHeight

    ' Resolved ONCE for the whole pass. Calling IsTextVisible() at each of the three sites
    ' below would be correct today but invites a future edit that changes the selection
    ' mid-layout and produces a button measured one way and placed another.
    dim as boolean bText = this.IsTextVisible()

    if bText then
        ' The PLACEHOLDER is deliberately NOT measured. Width comes from the item set alone,
        ' so a long placeholder cannot inflate the button and a short one cannot let it
        ' shrink -- either would make the button resize on the user's first pick.
        for i as long = 0 to this.itemCount - 1
            dim as long nChars = len( this.items(i).wszText )
            if nChars > 0 then
                dim as SIZE sz
                if GetTextExtentPoint32W( hDC, this.items(i).wszText.vptr, nChars, @sz ) then
                    if sz.cx > this.nTextWidth then this.nTextWidth = sz.cx
                end if
            end if
        next
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' ---- the ideal size, which does not depend on the client area ------------------------
    dim as long nRingPad  = this.RingPad()
    dim as long contentH  = this.nTextHeight
    if this.nChevronHeight > contentH then contentH = this.nChevronHeight

    dim as long textBlock = 0
    ' The gap is spent only when there is a caption to separate from the chevron. In
    ' arrow-only mode there is nothing on the left, so charging for it would leave the arrow
    ' visibly off-centre in its own padding.
    if bText then textBlock = this.nTextWidth + this.nTextGap

    this.nIdealW = (2 * nRingPad) + this.nPadLeft + textBlock + this.nChevronWidth + this.nPadRight
    this.nIdealH = (2 * nRingPad) + this.nPadTop + contentH + this.nPadBottom

    ' ---- placement, which does --------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again -- but note this bail
    ' comes AFTER the ideal size is computed. See the header.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcButton = rcClient
    InflateRect( @this.rcButton, -nRingPad, -nRingPad )

    dim as long chevRight = this.rcButton.right - this.nPadRight
    dim as long chevLeft  = chevRight - this.nChevronWidth
    dim as long chevTop   = this.rcButton.top + _
                            (((this.rcButton.bottom - this.rcButton.top) - this.nChevronHeight) \ 2)
    SetRect( @this.rcChevron, chevLeft, chevTop, chevRight, chevTop + this.nChevronHeight )

    if bText then
        dim as long textLeft  = this.rcButton.left + this.nPadLeft
        dim as long textRight = this.rcChevron.left - this.nTextGap
        ' Degenerate rather than negative when the client is too narrow (see OVERFLOW above).
        if textRight < textLeft then textRight = textLeft
        dim as long textTop = this.rcButton.top + _
                              (((this.rcButton.bottom - this.rcButton.top) - this.nTextHeight) \ 2)
        SetRect( @this.rcText, textLeft, textTop, textRight, textTop + this.nTextHeight )
    else
        SetRectEmpty( @this.rcText )
    end if

    this.rcVisual = this.rcButton
    InflateRect( @this.rcVisual, nRingPad, nRingPad )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A DROPDOWN SELECTOR: a button showing the current choice with a stacked up/down chevron on
' its right, which drops a list of alternatives where exactly one -- the current one -- is
' checkmarked. Single selection only; no multi-select, no text entry.
'
' THE DROPDOWN IS A CPopupMenu, USED AS-IS
'   Which buys the whole floating-window problem already solved: a WS_POPUP that never takes
'   activation, hover-is-selection, keyboard navigation, outside-click dismissal, Escape, and
'   a checkmark per row. TWO CONSEQUENCES FOLLOW, and neither is a bug to be fixed here:
'
'     1. THE CHECKMARK SITS IN A LEFT GUTTER. CPopupMenu's nCheckColWidth (30) and
'        nChevronColWidth (20) are fixed at its Create with no setters, so rows render as a
'        Windows menu -- check on the left, and a reserved-but-unused column on the right.
'        Making the check right-aligned would mean patching CMenuBar, which was ruled out.
'
'     2. THERE IS NO SCROLLING. CPopupMenu bottom-clamps to the work area; a list taller than
'        the screen is CLIPPED and its tail is unreachable. This control is built for a dozen
'        items, not fifty. If you need a long list, you need a different control.
'
' THE MESSAGE-PUMP CONTRACT (this is not optional)
'   Keyboard navigation inside the list and click-outside dismissal both live in the filter.
'   A host that never calls it gets a list that opens and paints but cannot be keyboard-driven
'   and never dismisses on an outside click:
'
'       do while GetMessage( @uMsg, null, 0, 0 )
'           if CComboBox_FilterMessage( hCombo, @uMsg ) then continue do
'           if IsDialogMessage( hMain, @uMsg ) then continue do
'           TranslateMessage( @uMsg ) : DispatchMessage( @uMsg )
'       loop
'
'   Call it once per combobox, ahead of IsDialogMessage. See CComboBox_FilterMessage.
'
' IT IS FOCUSABLE
'   WS_TABSTOP, real focus tracking, a painted focus ring, and Win32 combobox key handling.
'   CToggle is the only other focusable control in the family; everything else is mouse-only.
'   Tab NAVIGATION needs IsDialogMessage in the host pump. Without it, mouse and -- once the
'   control has focus -- keyboard both still work.
'
' NO TOOLTIPS
'   No tooltip window is created and there is no CBO_TooltipCallback (by request). A host that
'   wants a tip can add its own tool over the control's HWND.
'
' THE CONTROL HANDLE
'   Every CComboBox_* function takes the handle returned by CComboBox_Create(). It is a real
'   HWND on purpose (not an opaque type): callers legitimately need to treat the control as a
'   window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed, and destroys its dropdown with it.
'   The font you pass in stays yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID), so hosts can find it with GetDlgItem.
'   The control is created zero-sized: size it with CComboBox_GetIdealSize and position it
'   with SetWindowPos() -- or turn on CComboBox_SetAutoSize and let it size itself.
' ----------------------------------------------------------------------------------------
declare function CComboBox_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Items.  The set is DYNAMIC; every structural mutation fixes up the current selection:
'   AddItem     appends. Cannot move an existing index, so the selection is untouched. Note
'               it does NOT auto-select the first item -- -1 is a legal state here.
'   InsertItem  inserts BEFORE idx (idx = count appends). A selection at or after idx moves up.
'   DeleteItem  removes idx. Deleting the CURRENT item clears the selection to -1 rather than
'               sliding it onto a neighbour; a selection after idx moves down.
'   Clear       removes everything and clears the selection.
' All of them are SILENT -- no callbacks, no notifications.
' A disabled item is greyed in the list, is not selectable by mouse or keyboard, and is
' skipped by the arrow keys. It still counts for indexing and still contributes its width.
' ----------------------------------------------------------------------------------------
declare function CComboBox_AddItem( byval hCombo as HWND, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
declare function CComboBox_InsertItem( byval hCombo as HWND, byval idx as long, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
declare function CComboBox_DeleteItem( byval hCombo as HWND, byval idx as long ) as boolean
declare sub      CComboBox_Clear( byval hCombo as HWND )
declare function CComboBox_GetCount( byval hCombo as HWND ) as long
declare function CComboBox_IsValidItem( byval hCombo as HWND, byval idx as long ) as boolean
declare function CComboBox_GetItemText( byval hCombo as HWND, byval idx as long ) as DWSTRING
declare function CComboBox_SetItemText( byval hCombo as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
declare function CComboBox_GetItemID( byval hCombo as HWND, byval idx as long ) as long
declare function CComboBox_SetItemID( byval hCombo as HWND, byval idx as long, byval id as long ) as boolean
declare function CComboBox_GetItemData( byval hCombo as HWND, byval idx as long ) as integer
declare function CComboBox_SetItemData( byval hCombo as HWND, byval idx as long, byval itemData as integer ) as boolean
declare function CComboBox_GetItemEnabled( byval hCombo as HWND, byval idx as long ) as boolean
declare function CComboBox_SetItemEnabled( byval hCombo as HWND, byval idx as long, byval bEnabled as boolean ) as boolean
' Index of the FIRST item carrying this id / this exact text (case-insensitive), or -1.
declare function CComboBox_FindItemByID( byval hCombo as HWND, byval id as long ) as long
declare function CComboBox_FindItemByText( byval hCombo as HWND, byval Text as DWSTRING ) as long

' ----------------------------------------------------------------------------------------
' Selection.  -1 is legal and means "nothing selected" -- the button then draws the
' placeholder, or nothing.
'
' SetCurSel is SILENT: the change callback fires only for user interaction (a pick from the
' list, or an arrow/Home/End key). Win32's CB_SETCURSEL / CBN_SELCHANGE precedent, and it is
' what stops a host calling SetCurSel from inside its own handler from recursing.
' It REFUSES a disabled item (returns FALSE and changes nothing), because the user could not
' have reached that state and a host should not be able to fake it silently. -1 always
' succeeds; an out-of-range index does not.
'
' GetText returns what the BUTTON is currently showing: the selected caption, or the
' placeholder, or "".
' ----------------------------------------------------------------------------------------
declare function CComboBox_GetCurSel( byval hCombo as HWND ) as long
declare function CComboBox_SetCurSel( byval hCombo as HWND, byval idx as long ) as boolean
declare function CComboBox_GetText( byval hCombo as HWND ) as DWSTRING

' ----------------------------------------------------------------------------------------
' State.
'   SetEnabled(false) goes through EnableWindow, so the disable is enforced by the system
'     rather than being a cosmetic flag: a disabled window gets no mouse input and the dialog
'     manager's Tab skips it. It closes an open list, and clears hover at once rather than
'     waiting for the next mouse move.
'   DropDown / CloseUp are the programmatic doors. They fire the DropDown callback (it reports
'     a window-state transition, not a value change) but never SelChange.
' ----------------------------------------------------------------------------------------
declare function CComboBox_GetEnabled( byval hCombo as HWND ) as boolean
declare sub      CComboBox_SetEnabled( byval hCombo as HWND, byval isEnabled as boolean )
declare function CComboBox_GetFocused( byval hCombo as HWND ) as boolean
declare function CComboBox_IsDroppedDown( byval hCombo as HWND ) as boolean
declare function CComboBox_DropDown( byval hCombo as HWND ) as boolean
declare sub      CComboBox_CloseUp( byval hCombo as HWND )
declare sub      CComboBox_Refresh( byval hCombo as HWND )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). The THICKNESS values should not be scaled at all.
'
'   SetTextMode picks one of CBO_TEXT_ALWAYS / _NEVER / _WHENSELECTED -- see the enum for what
'     each means and for the width consequence of the third. SetShowText is the two-state
'     convenience over it (TRUE = ALWAYS, FALSE = NEVER); GetShowText answers "is a caption
'     drawn RIGHT NOW", which for CBO_TEXT_WHENSELECTED depends on the selection.
'     In arrow-only there is no caption and no text gap, and the ideal width collapses to
'     padding + chevron. The selection still exists and the list still checkmarks it -- only
'     the button's caption is suppressed.
'   SetAutoSize(true) makes the control SetWindowPos ITSELF (preserving its top-left) whenever
'     something that changes the ideal width changes: items, font, show-text, padding, chevron
'     size, focus ring. Default FALSE, which is the strict family rule -- the host measures
'     with GetIdealSize and sizes.
'   SetCornerCurvature takes an ellipse DIAMETER, not a radius: CBufferPaint keeps GDI's
'     vocabulary and halves it internally. 12 draws a 6px radius. 0 = square corners.
'   SetChevronSize is the cell the two chevrons are drawn in; SetChevronThickness is the
'     stroke weight. NOTE the stroke is DPI-scaled by CBufferPaint.PaintLine itself, unlike
'     the border and ring thicknesses which are not scaled anywhere -- that difference is
'     deliberate: a divider hairline should stay a hairline, but a glyph's stroke should grow
'     with the glyph.
'   SetFocusRing sets the gap from the chrome to the ring and the ring's own thickness. Both
'     are reserved unconditionally, focused or not, so the button never jumps when focus
'     arrives -- which means changing either changes the IDEAL SIZE.
'
'   GetIdealSize forces a pending layout and returns the measured button size. It is valid
'   BEFORE the control has ever been sized (the measuring pass runs ahead of the zero-client
'   bail), which is the whole point -- a host calls it to decide how big to make the control.
'
'   The rect queries force a pending layout, so results are always current. They return FALSE
'   if the control has no geometry yet (created but never sized). rcText is EMPTY in
'   arrow-only mode, and the query still returns TRUE -- empty is the honest answer.
' ----------------------------------------------------------------------------------------
declare function CComboBox_GetTextMode( byval hCombo as HWND ) as long
declare sub      CComboBox_SetTextMode( byval hCombo as HWND, byval nTextMode as long )
declare function CComboBox_GetShowText( byval hCombo as HWND ) as boolean
declare sub      CComboBox_SetShowText( byval hCombo as HWND, byval bShowText as boolean )
declare function CComboBox_GetAutoSize( byval hCombo as HWND ) as boolean
declare sub      CComboBox_SetAutoSize( byval hCombo as HWND, byval bAutoSize as boolean )
declare sub      CComboBox_GetPadding( byval hCombo as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
declare sub      CComboBox_SetPadding( byval hCombo as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
declare function CComboBox_GetTextGap( byval hCombo as HWND ) as long
declare sub      CComboBox_SetTextGap( byval hCombo as HWND, byval nTextGap as long )
declare sub      CComboBox_GetChevronSize( byval hCombo as HWND, byref nWidth as long, byref nHeight as long )
declare sub      CComboBox_SetChevronSize( byval hCombo as HWND, byval nWidth as long, byval nHeight as long )
declare function CComboBox_GetChevronGap( byval hCombo as HWND ) as long
declare sub      CComboBox_SetChevronGap( byval hCombo as HWND, byval nGap as long )
declare function CComboBox_GetChevronThickness( byval hCombo as HWND ) as long
declare sub      CComboBox_SetChevronThickness( byval hCombo as HWND, byval nThickness as long )
declare function CComboBox_GetCornerCurvature( byval hCombo as HWND ) as long
declare sub      CComboBox_SetCornerCurvature( byval hCombo as HWND, byval nCurvature as long )
declare function CComboBox_GetBorderThickness( byval hCombo as HWND ) as long
declare sub      CComboBox_SetBorderThickness( byval hCombo as HWND, byval nThickness as long )
declare sub      CComboBox_GetFocusRing( byval hCombo as HWND, byref nGap as long, byref nThickness as long )
declare sub      CComboBox_SetFocusRing( byval hCombo as HWND, byval nGap as long, byval nThickness as long )
declare sub      CComboBox_GetIdealSize( byval hCombo as HWND, byref nWidth as long, byref nHeight as long )
declare function CComboBox_GetButtonRect( byval hCombo as HWND, byref rc as RECT ) as boolean
declare function CComboBox_GetTextRect( byval hCombo as HWND, byref rc as RECT ) as boolean
declare function CComboBox_GetChevronRect( byval hCombo as HWND, byref rc as RECT ) as boolean
declare function CComboBox_GetVisualRect( byval hCombo as HWND, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct. GetColors fills one out, so the
' read-modify-write "change one color" idiom is Get, assign, Set.
'
' The font is BORROWED, never owned: keep it alive and destroy it yourself. It is also the
' MEASURING font, and it is passed straight through to the dropdown, so button and list
' always agree.
'
' THE DROPDOWN'S OWN APPEARANCE goes through CComboBox_GetListHandle(), which returns the
' internal CPopupMenu HWND -- so every CPopupMenu_Set* appearance call is available
' (SetColors, SetGlyphs, SetItemHeight, SetSeparatorThickness, SetPaintItemCallback...).
' The handle is 0 until the list has been created, which happens on the first DropDown; call
' CComboBox_EnsureList() first if you want to theme it up front.
'
' THAT HANDLE IS FOR APPEARANCE ONLY. Adding, deleting or re-ordering rows through it
' desyncs them from the model and is not supported: the rows are REBUILT from items() on
' every open and your changes would vanish at best. Selection state likewise -- the check
' mark is placed by CComboBox, from nCurSel.
' ----------------------------------------------------------------------------------------
declare sub      CComboBox_GetColors( byval hCombo as HWND, byval pColors as CCOMBOBOX_COLORS ptr )
declare sub      CComboBox_SetColors( byval hCombo as HWND, byval pColors as CCOMBOBOX_COLORS ptr )
declare function CComboBox_GetFont( byval hCombo as HWND ) as HFONT
declare sub      CComboBox_SetFont( byval hCombo as HWND, byval hTextFont as HFONT )
declare function CComboBox_GetPlaceholderText( byval hCombo as HWND ) as DWSTRING
declare sub      CComboBox_SetPlaceholderText( byval hCombo as HWND, byval Text as DWSTRING )
declare function CComboBox_EnsureList( byval hCombo as HWND ) as HWND
declare function CComboBox_GetListHandle( byval hCombo as HWND ) as HWND
declare sub      CComboBox_SetListColors( byval hCombo as HWND, byval pColors as CPOPUPMENU_COLORS ptr )
declare sub      CComboBox_SetListItemHeight( byval hCombo as HWND, byval nItemHeight as long )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback     - draw the whole BUTTON instead of the built-in painter. The dropdown
'                       is painted by CPopupMenu; reach it through GetListHandle.
'   MessageCallback   - observe mouse, focus and key messages; TRUE suppresses the control's
'                       default handling (IGNORED for the two focus messages).
'   SelChangeCallback - the USER changed the selection. Programmatic SetCurSel is silent.
'   DropDownCallback  - the list opened or closed. Fires for programmatic opens too, because
'                       it reports a window-state transition rather than a value change.
' ----------------------------------------------------------------------------------------
declare sub      CComboBox_SetPaintCallback( byval hCombo as HWND, byval usersub as CBO_PaintCallbackSub )
declare sub      CComboBox_SetMessageCallback( byval hCombo as HWND, byval userfunc as CBO_MessageCallbackFunc )
declare sub      CComboBox_SetSelChangeCallback( byval hCombo as HWND, byval usersub as CBO_SelChangeCallbackSub )
declare sub      CComboBox_SetDropDownCallback( byval hCombo as HWND, byval usersub as CBO_DropDownCallbackSub )

' ----------------------------------------------------------------------------------------
' Plumbing.
'   FilterMessage - THE message-pump hook (see the contract at the top). Returns TRUE when
'                   the message was consumed and the pump must skip Translate/Dispatch.
'                   Safe to call when the list is closed, and safe with a stale handle.
'
'                   It also carries this design's one trap, and handles it. CPopupMenu's own
'                   filter dismisses on an outside click and then returns FALSE ON PURPOSE so
'                   the click still reaches its target -- which for a click on OUR button
'                   would close the list and immediately reopen it. This function notices
'                   that case and arms a one-shot suppression that the following
'                   WM_LBUTTONDOWN consumes. Nothing timer-based, nothing heuristic.
'
'   RunSelfTest   - geometry assertions, gated on the CCOMBOBOX_SELFTEST env var. Call it
'                   from the host before the message loop.
' ----------------------------------------------------------------------------------------
declare function CComboBox_FilterMessage( byval hCombo as HWND, byval pMsg as MSG ptr ) as boolean
declare sub      CComboBox_RunSelfTest( byval hWndParent as HWND )
