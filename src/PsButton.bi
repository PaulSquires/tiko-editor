''
''  PsButton.bi  --  owner-drawn command button (glyph + caption + glyph)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "PsBufferPaint.bi"
' An icon slot can hold a real image file (.ico/.png/.bmp/.jpg) instead of a Fluent glyph.
' PsImage owns the decode; PsBufferPaint.PaintImage does the draw. See PsButton_SetImageLeft.
#include once "PsImage.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly as it
' always has; a host opts this instance into PsTooltip with PsButton_SetTooltipMode. PsTipHost
' holds both backends and is what keeps the hover time honoured across a switch.
#include once "PsTipHost.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
#define IDT_CBUTTON_HOTTRACK   &hCB90
#define PSBUTTON_HOTTRACK_MS    100

' Default geometry. Everything except the two THICKNESS values is DPI-scaled once at Create;
' every setter afterwards takes raw pixels and the caller scales (the family rule).
#define PSBUTTON_DEFAULT_PADLEFT     12
#define PSBUTTON_DEFAULT_PADRIGHT    12
#define PSBUTTON_DEFAULT_PADTOP       6
#define PSBUTTON_DEFAULT_PADBOTTOM    6
' Gap between an icon cell and the caption. Spent ONLY when there is a caption to separate
' the icon from -- an icon-only button charges padding + icon + padding and nothing else.
#define PSBUTTON_DEFAULT_ICONGAP      8
' The icon cell. DECLARED, never measured (PsIconPanel's rule): the glyph font is a paint-time
' input, so a font too large for the cell simply clips rather than resizing the button.
#define PSBUTTON_DEFAULT_ICONWIDTH   16
#define PSBUTTON_DEFAULT_ICONHEIGHT  16
' Corner curvature of the button chrome. This is an ellipse DIAMETER, not a radius --
' PsBufferPaint keeps GDI's vocabulary and halves it internally, so 12 draws a 6px radius.
' Thirteen controls pass GDI-flavoured numbers there; do not "fix" it to a radius here.
#define PSBUTTON_DEFAULT_CURVATURE   12
' The button reads BORDERLESS out of the box (see PSBUTTON_COLORS), so a border thickness of 1
' costs nothing until a host sets a contrasting BorderColor. Zero disables it outright.
#define PSBUTTON_DEFAULT_BORDERTHICK  1     ' not DPI-scaled
#define PSBUTTON_DEFAULT_FOCUSGAP     2
#define PSBUTTON_DEFAULT_FOCUSTHICK   1     ' not DPI-scaled


' Where the caption sits in the span between the two icon cells.
'
' NOTE WHAT THIS DOES NOT DO. The icon cells are PINNED to the padding and are never moved by
' the alignment -- so with a left icon and BTN_ALIGN_CENTER the caption is centred on the
' LEFTOVER span, not on the button. That is the layout model this control was designed to
' (PsComboBox pins its chevron exactly this way); a host that wants the icon to hug a centred
' caption wants the block-centring PsSelectBar does, which is a different control.
enum
    BTN_ALIGN_LEFT = 0
    BTN_ALIGN_CENTER
    BTN_ALIGN_RIGHT
end enum

' Which of the four colour sets the painter uses. Produced by PsButton_ResolveMood, which is a
' PURE FUNCTION precisely so its truth table can be asserted (PsScrollPanel_ShouldShow's
' precedent) rather than only inspected inside a paint.
'
' isDefault is deliberately NOT a mood. It overlays one colour -- the border -- on top of
' whichever mood resolved, so it survives all four for free instead of doubling the matrix.
enum
    BTN_MOOD_IDLE = 0
    BTN_MOOD_HOT
    BTN_MOOD_PRESSED
    BTN_MOOD_DISABLED
end enum

' Which rect PsButton_CountRenderedTones looks at. See that function for why it exists.
enum
    BTN_PART_BUTTON = 0
    BTN_PART_TEXT
    BTN_PART_ICONLEFT
    BTN_PART_ICONRIGHT
end enum


' Colors for the built-in painter. Copied on Set.
'
' FOUR MOODS, precedence  disabled > pressed > hot > idle.  Unlike PsToggle and PsSelectBar --
' which render a press as hot -- this control has a real PRESSED set, because a command
' button's press is the one piece of feedback the user relies on to know the click registered,
' and unlike PsComboBox (where "pressed" and "open" are the same instant) the held-down state
' here genuinely lasts as long as the finger does.
'
' THE BUTTON READS BORDERLESS BY DEFAULT because every BorderColorXxx is defaulted EQUAL to the
' matching BackColorXxx -- the same trick PsComboBox uses, PsToggle uses for its ON pill and
' PsSelectBar for its hot cell. A host that wants a visible outline just sets the four fields.
'
' THE ICON FOLLOWS THE CAPTION BY DEFAULT for the same reason: each IconColorXxx is defaulted
' EQUAL to the matching ForeColorXxx. Set them to break that coupling (a red record dot on an
' otherwise grey button).
'
' THE PRESSED PAIR IS THE THEME'S SELECT PAIR, which is PsIconPanel's call -- its
' BackColorSelect doubles as the press flash. A press therefore reads as an accent push rather
' than as a darkening, and it is the one mood whose border is visible out of the box only
' because its fill is.
'
' DefaultBorderColor is the ONLY colour here that is not part of a mood. It replaces whatever
' border the resolved mood would have used, which is what makes "the default button" a single
' visible outline on an otherwise borderless button.
type PSBUTTON_COLORS
    ' --- idle ---
    BackColor           as COLORREF = BGR( 44, 49, 58)
    ForeColor           as COLORREF = BGR(200,205,214)
    BorderColor         as COLORREF = BGR( 44, 49, 58)   ' = BackColor (see above)
    IconColor           as COLORREF = BGR(200,205,214)   ' = ForeColor (see above)
    ' --- hot (the cursor is over the button) ---
    BackColorHot        as COLORREF = BGR( 55, 61, 72)
    ForeColorHot        as COLORREF = BGR(226,230,238)
    BorderColorHot      as COLORREF = BGR( 55, 61, 72)   ' = BackColorHot
    IconColorHot        as COLORREF = BGR(226,230,238)   ' = ForeColorHot
    ' --- pressed (a live left press, cursor still inside) ---
    BackColorPressed    as COLORREF = BGR( 38, 79,120)
    ForeColorPressed    as COLORREF = BGR(255,255,255)
    BorderColorPressed  as COLORREF = BGR( 38, 79,120)   ' = BackColorPressed
    IconColorPressed    as COLORREF = BGR(255,255,255)   ' = ForeColorPressed
    ' --- disabled ---
    BackColorDisabled   as COLORREF = BGR( 40, 44, 51)
    ForeColorDisabled   as COLORREF = BGR( 96,102,112)
    BorderColorDisabled as COLORREF = BGR( 40, 44, 51)   ' = BackColorDisabled
    IconColorDisabled   as COLORREF = BGR( 96,102,112)   ' = ForeColorDisabled
    ' --- everything else ---
    FocusRingColor      as COLORREF = BGR( 97,175,239)
    DefaultBorderColor  as COLORREF = BGR( 97,175,239)   ' overlays the mood's border
end type


' Everything the painter needs. All five rects are precomputed by LayoutButton -- never
' re-derive one from another, and in particular never re-apply the padding to rcButton to get
' rcText, because the icon cells and the icon gap can change underneath you.
type PSBUTTON_PAINTINFO
    hButton       as HWND              ' the control, so the callback can query it
    b             as PsBufferPaint ptr  ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutButton ---
    rcClient      as RECT              ' the whole client area
    rcButton      as RECT              ' the chrome: rcClient deflated by the focus-ring band
    rcIconLeft    as RECT              ' the left icon cell.  EMPTY when there is no left glyph
    ' THE CAPTION SPAN, NOT THE INK. rcText is the whole box between the two icon blocks, so
    ' DT_END_ELLIPSIS has somewhere to ellipsize into; the caption is placed inside it by
    ' nTextAlign. Do NOT expect it to be a tight box around the glyphs -- that is the natural
    ' mistake here, and drawing a background behind "the text" using this rect fills the span.
    ' EMPTY when there is no caption, and DEGENERATE (left = right) when the client is too
    ' narrow to leave any span at all.
    rcText        as RECT
    rcIconRight   as RECT              ' the right icon cell. EMPTY when there is no right glyph
    rcVisual      as RECT              ' rcButton inflated by the focus-ring band
    ' --- State: the control decides these, you only render. ---
    isHot         as boolean           ' the mouse is over the control
    isPressed     as boolean           ' a live left press AND the cursor is still inside
                                       '   (slide off and this goes false without ending
                                       '   the gesture)
    isEnabled     as boolean
    isFocused     as boolean           ' draw a focus ring when true
    isDefault     as boolean           ' this is the dialog's default button (appearance only)
    ' --- What to draw ---
    wszText       as DWSTRING
    wszGlyphLeft  as DWSTRING
    wszGlyphRight as DWSTRING
    ' Resolved image handles for the two icon cells, or NULL. When set, the icon slot draws
    ' this image (via p->b->PaintImage) INSTEAD OF the glyph -- a custom PaintCallback should
    ' honour it the same way, image first. The PsBufferPaint owns nothing here; the control's
    ' PsImage does.
    pImageLeft    as CGpImage ptr
    pImageRight   as CGpImage ptr
    nTextAlign    as long              ' BTN_ALIGN_*; map it to DT_LEFT / DT_CENTER / DT_RIGHT
end type

type PSBUTTON_MESSAGEINFO
    hButton    as HWND
    uMsg       as UINT
    wParam     as WPARAM
    lParam     as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with the resolved mood's BackColor before calling
' you, so a callback that only wants to add something on top does not have to repaint the
' background.
'
' TWO CONTRACTS WORTH HONOURING:
'   - Draw the caption with the SAME font you handed to PsButton_SetFont. The button width was
'     measured with it, and a different font means the width lies.
'   - DO NOT reach for PaintBorderRect to draw a frame. It FILLS unconditionally, so used as an
'     outline it erases everything beneath it -- a mistake this family has now made three
'     separate times (PsToggle, PsComboBox, PsNumericUpDown), every time by copying a sibling's
'     callback. PaintRoundOutline is the one that strokes without filling.
type BTN_PaintCallbackSub as sub( byval p as PSBUTTON_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' Like PsToggle and PsComboBox -- and unlike the mouse-only siblings -- this control hands over
' the FOCUS and KEYBOARD messages as well as the mouse ones, because they carry real state.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the control holds mouse capture across a press and the up-message is
'                         what releases it: a callback that suppressed it would strand capture
'                         and route every subsequent click to this control (the PsListBox bug
'                         recorded in Learnings.md). Suppressing WM_LBUTTONDOWN suppresses the
'                         press, never the capture bookkeeping.
'                         NOTE this reverses PsComboBox's contract, which the file was seeded
'                         from -- that control takes no capture and honours the result here.
'   WM_SETFOCUS         - focus is a FACT the system reports, not an action to veto. The state
'   WM_KILLFOCUS          is already updated by the time you are called; there is nothing left
'                         to suppress. A host that does not want the control focusable must not
'                         make it a tabstop.
' For every other message TRUE suppresses the control's default handling.
type BTN_MessageCallbackFunc as function( byval m as PSBUTTON_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text on demand (only when a tip is about to show). Consulted ONLY when the
' control has no tooltip text of its own. Return "" for no tooltip.
'
' THERE IS NO CAPTION FALLBACK. A button whose caption is already on screen would otherwise pop
' a tip that just repeats what you are looking at, so a control with neither its own text nor a
' callback answer simply shows nothing. (PsStatusBar and PsTabBar do fall back to the caption;
' that is their call, made for controls whose text is often truncated.)
type BTN_TooltipCallbackFunc as function( byval hButton as HWND ) as DWSTRING

' The button was clicked: a matched press+release with the cursor still inside, or Space/Enter
' while focused. Nothing fires for a cancelled gesture (press, slide off, release).
'
' Unlike the family's state callbacks this one ALSO fires for the programmatic PsButton_Click --
' see that function for why an action is not a setter.
type BTN_ClickCallbackSub as sub( byval hButton as HWND, byval id as long )


type PSBUTTON
    hWin            as HWND
    ' The tooltip, whichever backend it is on. Replaces the old `hToolTip as HWND` +
    ' `HoverTime as long` pair; PsButton_GetTooltipHandle still answers the comctl32 handle
    ' for hosts that reach through it, and returns 0 while this instance is on PsTooltip.
    tip             as PSTIPHOST
    ' Instance-lifetime buffer that TTN_GETDISPINFOW's lpszText is pointed at. It must NOT be
    ' the same field as wszTooltip below: that one is the host's authored text, and overwriting
    ' it with a callback's answer would silently promote a transient string into stored state.
    wszTipBuf       as DWSTRING
    idc_Button      as long = 0
    id              as long = 0        ' host command id, reported by ClickCallback
    itemData        as integer = 0     ' free-form host payload
    ' --- Content ---
    wszText         as DWSTRING        ' the caption. "" = no caption (and no icon gap spent)
    wszGlyphLeft    as DWSTRING        ' Segoe Fluent Icons codepoint(s), or ""
    wszGlyphRight   as DWSTRING        ' likewise
    ' An icon slot holds EITHER a Fluent glyph OR an image, never both: the setters keep that
    ' invariant (SetImageLeft clears wszGlyphLeft, SetGlyphLeft frees pImageLeft). The control
    ' OWNS these PsImage objects and frees them in WM_DESTROY. NULL = no image on that side.
    pImageLeft      as PsImage ptr
    pImageRight     as PsImage ptr
    wszTooltip      as DWSTRING        ' "" = ask TooltipCallback, then show nothing
    ' --- State. All single-valued: there is exactly one of this control, so unlike the
    '     collection siblings there are no indices here to be left dangling, and none of their
    '     index fix-up code exists. There is also NO isSelected: this button is momentary and
    '     never latches, so a "current" state would have nothing to hold. ---
    isEnabled       as boolean = true
    isHot           as boolean = false  ' the mouse is over the client
    isFocused       as boolean = false
    ' Set when a WM_KEYDOWN we consume will be followed by a WM_CHAR, so that character can be
    ' swallowed too. Without it the character reaches DefWindowProc and the system BEEPS on
    ' every Enter/Space -- the keystroke still works, so the beep is the only symptom.
    bAteKeyDown   as boolean = false
    isPressed       as boolean = false  ' a live left press (see the capture note below)
    isDefault       as boolean = false  ' appearance only -- see PsButton_SetDefault
    hotTimerOn      as boolean = false  ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press: the press/cancel
    '     gesture (press, slide off the button, release, nothing happens) consumes the
    '     guaranteed down->up pairing, which is the family's test for taking capture. It pays
    '     the full price: snapshot the press BEFORE releasing (ReleaseCapture sends
    '     WM_CAPTURECHANGED synchronously -- Learnings.md), release on the up-message before
    '     any callback runs, WM_CAPTURECHANGED cancels the press, WM_DESTROY releases, and no
    '     callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still inside" is exactly isHot, and
    '     hover tracking already maintains that through the capture. ---
    colors          as PSBUTTON_COLORS
    ' Caller-supplied fonts (caller owns both). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member procedure
    ' of this type, and "dim as HFONT x" there fails with a misleading "error 14: Expected
    ' identifier, found 'HFONT'". See C:\dev\Learnings.md.
    '
    ' TWO fonts, because the caption font is never the icon font. hTextFont is the MEASURING
    ' font and drives the layout; hGlyphFont is a paint-time input only (PsIconPanel's rule) and
    ' changing it repaints without re-laying-out.
    hTextFont       as HFONT
    hGlyphFont      as HFONT
    ' --- Layout inputs. All pixels; DPI-scaled once at Create, raw thereafter. ---
    nTextAlign      as long = BTN_ALIGN_CENTER
    bAutoSize       as boolean = false   ' opt-in: the control SetWindowPos's ITSELF
    nPadLeft        as long = PSBUTTON_DEFAULT_PADLEFT
    nPadRight       as long = PSBUTTON_DEFAULT_PADRIGHT
    nPadTop         as long = PSBUTTON_DEFAULT_PADTOP
    nPadBottom      as long = PSBUTTON_DEFAULT_PADBOTTOM
    nIconGap        as long = PSBUTTON_DEFAULT_ICONGAP
    ' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
    ' that never name the real problem -- hence nIconWidth throughout.
    nIconWidth      as long = PSBUTTON_DEFAULT_ICONWIDTH
    nIconHeight     as long = PSBUTTON_DEFAULT_ICONHEIGHT
    nCurvature      as long = PSBUTTON_DEFAULT_CURVATURE     ' ellipse DIAMETER
    nBorderThick    as long = PSBUTTON_DEFAULT_BORDERTHICK   ' NOT DPI-scaled
    nFocusGap       as long = PSBUTTON_DEFAULT_FOCUSGAP
    nFocusThick     as long = PSBUTTON_DEFAULT_FOCUSTHICK    ' NOT DPI-scaled
    ' --- Layout outputs. Rects are DERIVED, never set from outside; LayoutButton() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect/size query) runs
    '     it, which coalesces a burst of mutations into ONE measuring pass. ---
    nTextWidth      as long = 0     ' derived: the measured caption width
    nTextHeight     as long = 0     ' derived: one height for the control, from tmHeight
    nIdealW         as long = 0     ' derived
    nIdealH         as long = 0     ' derived
    rcButton        as RECT         ' derived
    rcIconLeft      as RECT         ' derived
    rcText          as RECT         ' derived -- the SPAN, see PSBUTTON_PAINTINFO
    rcIconRight     as RECT         ' derived
    rcVisual        as RECT         ' derived
    bLayoutDirty    as boolean = true
    PaintCallback   as BTN_PaintCallbackSub      ' optional; replaces the built-in painter
    MessageCallback as BTN_MessageCallbackFunc   ' optional
    TooltipCallback as BTN_TooltipCallbackFunc   ' optional
    ClickCallback   as BTN_ClickCallbackSub      ' optional

    declare sub      LayoutButton()
    declare function RingPad() as long
    declare function HasText() as boolean
    declare function HasIconLeft() as boolean
    declare function HasIconRight() as boolean
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' The distance the visual bounds extend beyond the button chrome on every side. The focus ring
' is drawn INSIDE this band, and the band is reserved UNCONDITIONALLY -- whether or not the
' control currently has focus -- which is what stops the button jumping when focus arrives. It
' therefore contributes to the IDEAL SIZE (PsToggle's rule).
function PSBUTTON.RingPad() as long
    return this.nFocusGap + this.nFocusThick
end function

' The three "is this part present at all" tests. They exist as named members rather than inline
' len() checks because the layout, the painter and the ideal-size arithmetic must all agree
' about which parts exist, and three copies of `len(x) > 0` is three places to disagree.
function PSBUTTON.HasText() as boolean
    return (PsLen( this.wszText ) > 0)
end function

' An icon cell is present when the side carries EITHER a glyph OR a valid image. Layout, painter
' and ideal-size arithmetic all read these, so the image has to count here or its cell is never
' reserved and PaintImage draws into an empty rect.
function PSBUTTON.HasIconLeft() as boolean
    if PsLen( this.wszGlyphLeft ) > 0 then return true
    return (this.pImageLeft <> 0) andalso (this.pImageLeft->IsValid() <> 0)
end function

function PSBUTTON.HasIconRight() as boolean
    if PsLen( this.wszGlyphRight ) > 0 then return true
    return (this.pImageRight <> 0) andalso (this.pImageRight->IsValid() <> 0)
end function


' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's job,
' and only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it.
sub PSBUTTON.CancelPress()
    this.isPressed = false
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is what
' makes layout lazy: a burst of setters costs one measuring pass, not N.
sub PSBUTTON.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking button is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' Measure the caption and assign the five rects. This is the only place button geometry is
' produced; painting, invalidation and every public size query consume it.
'
'   ringPad     = nFocusGap + nFocusThick        reserved ALWAYS, focused or not
'   textW       = the measured caption width, 0 when there is no caption
'   textH       = GetTextMetrics.tmHeight        ONE height for the control
'   leftBlock   = hasLeft  ? nIconWidth + (hasText ? nIconGap : 0) : 0
'   rightBlock  = hasRight ? nIconWidth + (hasText ? nIconGap : 0) : 0
'   contentH    = max( hasText ? textH : 0 , (hasLeft or hasRight) ? nIconHeight : 0 )
'
'   nIdealW     = 2*ringPad + nPadLeft + leftBlock + textW + rightBlock + nPadRight
'   nIdealH     = 2*ringPad + nPadTop  + contentH + nPadBottom
'
'   rcButton    = rcClient deflated by ringPad on all four sides
'   rcIconLeft  = pinned at rcButton.left + nPadLeft,  nIconWidth x nIconHeight, v-centred
'   rcIconRight = pinned at rcButton.right - nPadRight - nIconWidth, same, v-centred
'   rcText      = ( rcButton.left + nPadLeft + leftBlock , rcButton.right - nPadRight
'                   - rightBlock ), v-centred on textH
'   rcVisual    = rcButton inflated by ringPad  ( = rcClient when the host sized us ideally )
'
' THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION TO SEPARATE FROM. An icon-only button charges
' padLeft + icon + padRight, so the glyph sits properly centred in its own padding instead of
' being pushed off-centre by a gap with nothing on the other side of it. Same reasoning as
' PsComboBox's arrow-only mode.
'
' AN ABSENT PART GIVES AN EMPTY RECT, not a zero-width one tucked against an edge. A paint
' callback can therefore test IsRectEmpty rather than having to re-derive presence from the
' strings it was handed.
'
' CONTENT HEIGHT IGNORES THE FONT WHEN THERE IS NO CAPTION, which means an icon-only button in a
' row of captioned ones has a SHORTER ideal height. That is the honest answer -- nothing in
' this button is as tall as a line of text -- and a host laying out a row takes the max itself
' rather than having a phantom text height baked in here.
'
' WHY THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area at all, and PsButton_GetIdealSize is exactly what a host calls to decide how big to
' make the control in the FIRST place. Returning 0 until it had already been sized would be a
' chicken-and-egg trap (PsComboBox dodges it by this same ordering).
'
' OVERFLOW: when the client is smaller than ideal the rects are computed HONESTLY rather than
' squeezed (PsIconPanel's and PsTabBar's rule) -- the icons keep their size and stay pinned to
' their padding, and rcText is what collapses, clamped to EMPTY rather than going negative. So
' a too-narrow button loses its caption to the ellipsis and keeps its icons, which is the
' useful failure. Vertically a client shorter than contentH clips rather than deforming.
sub PSBUTTON.LayoutButton()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    ' ---- the measuring pass --------------------------------------------------------------
    this.nTextWidth  = 0
    this.nTextHeight = 0

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font -- never
    ' "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    this.nTextHeight = tm.tmHeight

    ' Resolved ONCE for the whole pass, not re-tested at each of the sites below: three
    ' separate len() tests would be correct today but invite a future edit that changes the
    ' caption mid-layout and produces a button measured one way and placed another.
    dim as boolean bText = this.HasText()
    dim as boolean bLeft = this.HasIconLeft()
    dim as boolean bRght = this.HasIconRight()

    if bText then
        dim as SIZE sz
        if GetTextExtentPoint32W( hDC, this.wszText.vptr, PsLen( this.wszText ), @sz ) then
            this.nTextWidth = sz.cx
        end if
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' ---- the ideal size, which does not depend on the client area -------------------------
    dim as long nRingPad = this.RingPad()

    dim as long leftBlock = 0
    if bLeft then
        leftBlock = this.nIconWidth
        if bText then leftBlock += this.nIconGap
    end if

    dim as long rightBlock = 0
    if bRght then
        rightBlock = this.nIconWidth
        if bText then rightBlock += this.nIconGap
    end if

    dim as long contentH = 0
    if bText then contentH = this.nTextHeight
    dim as boolean bAnyIcon = (bLeft orelse bRght)
    if bAnyIcon then
        if this.nIconHeight > contentH then contentH = this.nIconHeight
    end if

    dim as long textW = 0
    if bText then textW = this.nTextWidth

    this.nIdealW = (2 * nRingPad) + this.nPadLeft + leftBlock + textW + rightBlock + this.nPadRight
    this.nIdealH = (2 * nRingPad) + this.nPadTop + contentH + this.nPadBottom

    ' ---- placement, which does ------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again -- but note this bail comes
    ' AFTER the ideal size is computed. See the header.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcButton = rcClient
    InflateRect( @this.rcButton, -nRingPad, -nRingPad )
    dim as long btnH = this.rcButton.bottom - this.rcButton.top

    if bLeft then
        dim as long xL = this.rcButton.left + this.nPadLeft
        dim as long yL = this.rcButton.top + ((btnH - this.nIconHeight) \ 2)
        SetRect( @this.rcIconLeft, xL, yL, xL + this.nIconWidth, yL + this.nIconHeight )
    else
        SetRectEmpty( @this.rcIconLeft )
    end if

    if bRght then
        dim as long xR = this.rcButton.right - this.nPadRight - this.nIconWidth
        dim as long yR = this.rcButton.top + ((btnH - this.nIconHeight) \ 2)
        SetRect( @this.rcIconRight, xR, yR, xR + this.nIconWidth, yR + this.nIconHeight )
    else
        SetRectEmpty( @this.rcIconRight )
    end if

    if bText then
        dim as long tLeft  = this.rcButton.left + this.nPadLeft + leftBlock
        dim as long tRight = this.rcButton.right - this.nPadRight - rightBlock
        ' Degenerate rather than negative when the client is too narrow (see OVERFLOW above).
        if tRight < tLeft then tRight = tLeft
        dim as long tTop = this.rcButton.top + ((btnH - this.nTextHeight) \ 2)
        SetRect( @this.rcText, tLeft, tTop, tRight, tTop + this.nTextHeight )
    else
        SetRectEmpty( @this.rcText )
    end if

    this.rcVisual = this.rcButton
    InflateRect( @this.rcVisual, nRingPad, nRingPad )
end sub


' ========================================================================================
' Which colour set does this state call for?  disabled > pressed > hot > idle.
'
' A PURE FUNCTION on purpose: it takes no control pointer and touches no global, so its whole
' truth table can be asserted directly (PsScrollPanel_ShouldShow's precedent) instead of only
' being observable from inside a WM_PAINT. The painter and the self-test call the same one.
' ========================================================================================
function PsButton_ResolveMood( _
            byval isEnabled as boolean, _
            byval isPressed as boolean, _
            byval isHot     as boolean _
            ) as long

    if isEnabled = false then return BTN_MOOD_DISABLED
    if isPressed then return BTN_MOOD_PRESSED
    if isHot then return BTN_MOOD_HOT
    return BTN_MOOD_IDLE
end function


' ========================================================================================
' Turn a mood (plus the default-button flag) into the four colours the painter uses.
'
' Split out of the painter for the same reason as PsButton_ResolveMood: the isDefault border
' overlay has to hold in EVERY mood, and that is a claim worth asserting rather than eyeballing
' in four screenshots. The overlay is applied LAST and unconditionally, which is what makes it
' one rule rather than four.
' ========================================================================================
sub PsButton_ResolveColors( _
            byval pColors   as PSBUTTON_COLORS ptr, _
            byval nMood     as long, _
            byval isDefault as boolean, _
            byref clrBack   as COLORREF, _
            byref clrFore   as COLORREF, _
            byref clrBorder as COLORREF, _
            byref clrIcon   as COLORREF _
            )

    if pColors = 0 then exit sub

    with *pColors
        select case nMood
        case BTN_MOOD_DISABLED
            clrBack   = .BackColorDisabled
            clrFore   = .ForeColorDisabled
            clrBorder = .BorderColorDisabled
            clrIcon   = .IconColorDisabled
        case BTN_MOOD_PRESSED
            clrBack   = .BackColorPressed
            clrFore   = .ForeColorPressed
            clrBorder = .BorderColorPressed
            clrIcon   = .IconColorPressed
        case BTN_MOOD_HOT
            clrBack   = .BackColorHot
            clrFore   = .ForeColorHot
            clrBorder = .BorderColorHot
            clrIcon   = .IconColorHot
        case else
            clrBack   = .BackColor
            clrFore   = .ForeColor
            clrBorder = .BorderColor
            clrIcon   = .IconColor
        end select

        ' The default button's outline replaces whatever the mood chose. Last, and with no
        ' mood test, so it cannot be true in three moods and forgotten in the fourth.
        if isDefault then clrBorder = .DefaultBorderColor
    end with
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A COMMAND BUTTON: a rounded rect with an optional glyph on the left, an optional caption, and
' an optional glyph on the right. Momentary -- it fires on release and never latches.
'
' IT DOES NOT LATCH, AND THERE IS NO isSelected
'   Every state this control has is transient (hot, pressed, focused) or host-owned (enabled,
'   default). A toolbar-style latching button is PsIconPanel's IP_KIND_TOGGLE and an on/off
'   switch is PsToggle; neither behaviour lives here, and the absence is a decision.
'
' isDefault IS APPEARANCE ONLY
'   It draws DefaultBorderColor over whichever mood's border would have been used, and does
'   nothing else. Routing Enter to the default button while some OTHER control has focus stays
'   the host's job -- this control claims Enter only when it is itself focused. Nothing stops a
'   host marking two buttons default; the control does not police it.
'
' AN ICON CELL HOLDS A GLYPH OR AN IMAGE, NEVER BOTH
'   wszGlyphLeft / wszGlyphRight are Segoe Fluent Icons codepoints (or any font's), drawn with
'   the SEPARATE font handed to PsButton_SetGlyphFont. Alternatively a cell holds a real image
'   file (.ico/.png/.bmp/.jpg) set with PsButton_SetImageLeft / SetImageRight, fitted into the
'   cell with the aspect preserved. The setters keep the invariant: setting an image clears
'   that side's glyph, and setting a glyph frees that side's image.
'
'   The cell is DECLARED by PsButton_SetIconSize and never measured either way, so an image
'   costs no layout change and a glyph font too large for the cell clips rather than resizing
'   the button. There is still no HICON path -- an image comes from a file path (or from
'   PsImage's HBITMAP/HICON wrappers), not from an HICON handed to this control.
'
' NO MNEMONICS
'   "&Save" draws a literal ampersand -- PsBufferPaint.PaintText forces DT_NOPREFIX, so this is
'   enforced by the renderer rather than merely intended. A host wanting Alt+S wires its own
'   accelerator and calls PsButton_Click().
'
' IT IS FOCUSABLE
'   WS_TABSTOP, real focus tracking, a painted focus ring, and Space/Enter activation. PsToggle
'   and PsComboBox are the other focusable siblings; the rest of the family is mouse-only. Tab
'   NAVIGATION needs IsDialogMessage in the host pump. Without it, mouse and -- once the control
'   has focus -- keyboard both still work.
'
' THERE IS NO PUMP OBLIGATION
'   Unlike PsComboBox, PsNumericUpDown and PsTextBox there is no PsButton_FilterMessage: this
'   control owns no second top-level window and no popup, so nothing needs intercepting ahead of
'   the dispatch.
'
' THE CONTROL HANDLE
'   Every PsButton_* function takes the handle returned by PsButton_Create(). It is a real HWND on
'   purpose (not an opaque type): callers legitimately need to treat the control as a window,
'   e.g. SetWindowPos() to place and size it.
'
' THERE IS NO PsButton_HitTest
'   The whole client rect is the hit area, so a hit test could only ever be PtInRect(client) --
'   a function that returns TRUE for exactly the points the caller already knows are inside.
'   Its absence is a decision, not an omission (PsToggle's reasoning).
'
' LIFETIME
'   The control frees itself when its window is destroyed, and destroys its tooltip with it. The
'   two fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID), so hosts can find it with GetDlgItem.
'   There are no child controls. The control is created zero-sized: size it with
'   PsButton_GetIdealSize and position it with SetWindowPos() -- or turn on PsButton_SetAutoSize
'   and let it size itself.
' ----------------------------------------------------------------------------------------
declare function PsButton_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Content.  All of these are SILENT: they change what is drawn, never what was clicked.
'
'   SetText is also reachable as the WINDOW TEXT -- the control handles WM_SETTEXT, WM_GETTEXT
'     and WM_GETTEXTLENGTH against this same store, so SetWindowText / GetWindowText work and
'     generic code that walks children and reads their text sees a real caption. One store, two
'     doors. (No sibling does this; it is here because a button is the control most likely to
'     meet generic dialog code.)
'   SetGlyphLeft / SetGlyphRight take the codepoint(s) to draw, or "" to remove that icon --
'     which also gives back its cell AND its gap, so it changes the ideal width. Setting a
'     glyph FREES any image on that side (the slot is glyph OR image, never both).
'   SetImageLeft / SetImageRight take a file path (.ico/.png/.bmp/.jpg) and draw the image in
'     that icon cell INSTEAD OF a glyph -- "" removes it. Returns non-zero on a successful load.
'     Setting an image clears that side's glyph. The image fits the SAME declared icon cell a
'     glyph would use (nIconWidth/nIconHeight), aspect preserved.
'   SetID sets the value handed to ClickCallback. It is a HOST PAYLOAD and is never used as a
'     command id by this control, so it cannot collide with anything.
' ----------------------------------------------------------------------------------------
declare function PsButton_GetText( byval hButton as HWND ) as DWSTRING
declare sub      PsButton_SetText( byval hButton as HWND, byval Text as DWSTRING )
declare function PsButton_GetGlyphLeft( byval hButton as HWND ) as DWSTRING
declare sub      PsButton_SetGlyphLeft( byval hButton as HWND, byval Glyph as DWSTRING )
declare function PsButton_GetGlyphRight( byval hButton as HWND ) as DWSTRING
declare sub      PsButton_SetGlyphRight( byval hButton as HWND, byval Glyph as DWSTRING )
declare function PsButton_SetImageLeft( byval hButton as HWND, byval Path as DWSTRING ) as long
declare function PsButton_SetImageRight( byval hButton as HWND, byval Path as DWSTRING ) as long
declare function PsButton_GetID( byval hButton as HWND ) as long
declare sub      PsButton_SetID( byval hButton as HWND, byval id as long )
declare function PsButton_GetItemData( byval hButton as HWND ) as integer
declare sub      PsButton_SetItemData( byval hButton as HWND, byval itemData as integer )

' ----------------------------------------------------------------------------------------
' State.
'
'   SetEnabled(false) goes through EnableWindow, so the disable is enforced by the system rather
'     than being a cosmetic flag: a disabled window gets no mouse input and the dialog manager's
'     Tab skips it. It clears any live press and hover at once rather than waiting for the next
'     mouse move.
'   SetDefault is appearance only -- see the header. It is SILENT and cheap; only the border
'     colour changes, so it never re-measures.
'   GetFocused answers for this window directly (there is no child to hold focus, unlike
'     PsNumericUpDown where GetFocus() = hCtrl is never true).
' ----------------------------------------------------------------------------------------
declare function PsButton_GetEnabled( byval hButton as HWND ) as boolean
declare sub      PsButton_SetEnabled( byval hButton as HWND, byval isEnabled as boolean )
declare function PsButton_GetFocused( byval hButton as HWND ) as boolean
declare function PsButton_GetDefault( byval hButton as HWND ) as boolean
declare sub      PsButton_SetDefault( byval hButton as HWND, byval isDefault as boolean )
declare sub      PsButton_Refresh( byval hButton as HWND )

' ----------------------------------------------------------------------------------------
' Action.
'
'   PsButton_Click FIRES ClickCallback, and that is a DELIBERATE EXCEPTION to the family's
'   "programmatic setters are silent" rule. It is an ACTION, not a state setter -- Win32's
'   BM_CLICK sends BN_CLICKED too -- and it is the door a host's accelerator uses now that this
'   control parses no mnemonics. A silent version would have no purpose.
'
'   Every genuine SETTER above and below stays silent, so the rule is intact where it applies.
'   The call is refused on a disabled button, exactly as a real click would be. Note it does NOT
'   paint a press flash: it reports an action, it does not animate one.
' ----------------------------------------------------------------------------------------
declare sub      PsButton_Click( byval hButton as HWND )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). The two THICKNESS values should not be scaled at
' all: a hairline stays a hairline.
'
'   SetTextAlign places the caption in the SPAN BETWEEN THE ICONS, not in the button -- see the
'     BTN_ALIGN_* enum for what that means when only one side carries an icon.
'   SetIconGap is the space between an icon cell and the caption. It is charged only on a side
'     that has an icon, and only when there IS a caption.
'   SetIconSize is the declared cell for BOTH icons (they are always the same size). Nothing is
'     measured, so this is the only thing that decides how much room a glyph gets.
'   SetCornerCurvature takes an ellipse DIAMETER, not a radius: PsBufferPaint keeps GDI's
'     vocabulary and halves it internally. 12 draws a 6px radius. 0 = square corners.
'   SetFocusRing sets the gap from the chrome to the ring and the ring's own thickness. Both are
'     reserved unconditionally, focused or not, so the button never jumps when focus arrives --
'     which means changing either changes the IDEAL SIZE.
'   SetAutoSize(true) makes the control SetWindowPos ITSELF (preserving its top-left) whenever
'     something that changes the ideal size changes: caption, glyphs, font, padding, icon gap,
'     icon size, focus ring. Default FALSE, which is the strict family rule -- the host measures
'     with GetIdealSize and sizes.
'
'   GetIdealSize forces a pending layout and returns the measured size. It is valid BEFORE the
'   control has ever been sized (the measuring pass runs ahead of the zero-client bail), which
'   is the whole point -- a host calls it to decide how big to make the control.
'
'   The rect queries force a pending layout, so results are always current. They return FALSE if
'   the control has no geometry yet (created but never sized). An absent part's rect is EMPTY
'   and the query still returns TRUE -- empty is the honest answer.
' ----------------------------------------------------------------------------------------
declare function PsButton_GetTextAlign( byval hButton as HWND ) as long
declare sub      PsButton_SetTextAlign( byval hButton as HWND, byval nTextAlign as long )
declare sub      PsButton_GetPadding( byval hButton as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
declare sub      PsButton_SetPadding( byval hButton as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
declare function PsButton_GetIconGap( byval hButton as HWND ) as long
declare sub      PsButton_SetIconGap( byval hButton as HWND, byval nIconGap as long )
declare sub      PsButton_GetIconSize( byval hButton as HWND, byref nIconWidth as long, byref nIconHeight as long )
declare sub      PsButton_SetIconSize( byval hButton as HWND, byval nIconWidth as long, byval nIconHeight as long )
declare function PsButton_GetCornerCurvature( byval hButton as HWND ) as long
declare sub      PsButton_SetCornerCurvature( byval hButton as HWND, byval nCurvature as long )
declare function PsButton_GetBorderThickness( byval hButton as HWND ) as long
declare sub      PsButton_SetBorderThickness( byval hButton as HWND, byval nThickness as long )
declare sub      PsButton_GetFocusRing( byval hButton as HWND, byref nGap as long, byref nThickness as long )
declare sub      PsButton_SetFocusRing( byval hButton as HWND, byval nGap as long, byval nThickness as long )
declare function PsButton_GetAutoSize( byval hButton as HWND ) as boolean
declare sub      PsButton_SetAutoSize( byval hButton as HWND, byval bAutoSize as boolean )
declare sub      PsButton_GetIdealSize( byval hButton as HWND, byref nWidth as long, byref nHeight as long )
declare function PsButton_GetButtonRect( byval hButton as HWND, byref rc as RECT ) as boolean
declare function PsButton_GetTextRect( byval hButton as HWND, byref rc as RECT ) as boolean
declare function PsButton_GetIconLeftRect( byval hButton as HWND, byref rc as RECT ) as boolean
declare function PsButton_GetIconRightRect( byval hButton as HWND, byref rc as RECT ) as boolean
declare function PsButton_GetVisualRect( byval hButton as HWND, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct. GetColors fills one out, so the
' read-modify-write "change one color" idiom is Get, assign, Set.
'
' BOTH fonts are BORROWED, never owned: keep them alive and destroy them yourself.
'   SetFont      - the CAPTION font, and the MEASURING font. Changing it re-measures.
'   SetGlyphFont - the ICON font (normally Segoe Fluent Icons). A paint-time input only:
'                  changing it repaints but never re-lays-out, because the icon cell is
'                  declared rather than measured. Unset falls back to the caption font.
' ----------------------------------------------------------------------------------------
declare sub      PsButton_GetColors( byval hButton as HWND, byval pColors as PSBUTTON_COLORS ptr )
declare sub      PsButton_SetColors( byval hButton as HWND, byval pColors as PSBUTTON_COLORS ptr )
declare function PsButton_GetFont( byval hButton as HWND ) as HFONT
declare sub      PsButton_SetFont( byval hButton as HWND, byval hTextFont as HFONT )
declare function PsButton_GetGlyphFont( byval hButton as HWND ) as HFONT
declare sub      PsButton_SetGlyphFont( byval hButton as HWND, byval hGlyphFont as HFONT )

' ----------------------------------------------------------------------------------------
' Tooltips.  The control's own text wins; the callback is consulted only when it is empty; with
' neither, no tip is shown -- there is no caption fallback (see BTN_TooltipCallbackFunc).
' ----------------------------------------------------------------------------------------
declare function PsButton_GetTooltipText( byval hButton as HWND ) as DWSTRING
declare sub      PsButton_SetTooltipText( byval hButton as HWND, byval Text as DWSTRING )
declare function PsButton_GetTooltipHandle( byval hButton as HWND ) as HWND
'
' WHICH TOOLTIP DRAWS. PSTIP_MODE_SYSTEM (the default) is the comctl32 tip this control has
' always used; PSTIP_MODE_PS is PsTooltip -- owner-drawn, themeable, wrapping without a
' hand-sent TTM_SETMAXTIPWIDTH, and not a subclass of the control it serves. Switching adds NO
' pump obligation either way: PsTooltip has no FilterMessage.
'
' The default is deliberate rather than cautious. PsTooltip's colour defaults are DARK, so a
' control that switched itself would put a dark tip on a light form. Theme every tip at once
' with PsTooltip_SetDefaultColors/Fonts/Style/MaxWidth before creating any control, then opt in
' here per instance.
'
' Text is unaffected by the mode: both backends resolve it through the same rule (this
' control's own text first, then TooltipCallback, then nothing).
declare function PsButton_SetTooltipMode( byval hButton as HWND, byval nMode as long ) as boolean
declare function PsButton_GetTooltipMode( byval hButton as HWND ) as long
' The PsTooltip window, or 0 while this instance is on the system backend. This is the door to
' PsTooltip's own SetColors / SetFonts / SetStyle / SetMaxWidth / SetTitle / SetGlyph -- they
' are NOT mirrored here, deliberately: mirroring twenty setters across the thirteen controls
' that carry a tooltip is 260 wrappers to keep in step.
declare function PsButton_GetPsTooltipHandle( byval hButton as HWND ) as HWND
'
' The three delays, all in milliseconds and all honoured by BOTH backends -- a host's hover
' time must not depend on which tooltip it is looking at. A delay never set keeps the backend's
' own derivation from the system double-click time, which is what makes a tip appear on the
' same beat as every other tip on the machine.
declare sub      PsButton_SetHoverTime( byval hButton as HWND, byval milliseconds as long )
declare sub      PsButton_SetAutoPopTime( byval hButton as HWND, byval milliseconds as long )
declare sub      PsButton_SetReshowTime( byval hButton as HWND, byval milliseconds as long )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback   - draw the whole control INSTEAD OF the built-in painter.
'   MessageCallback - observe mouse, focus and key messages; TRUE suppresses the control's
'                     default handling (IGNORED for WM_LBUTTONUP and the two focus messages).
'   TooltipCallback - supply tooltip text on demand when the control has none of its own.
'   ClickCallback   - the button was clicked (matched press+release, or Space/Enter, or the
'                     programmatic PsButton_Click).
' ----------------------------------------------------------------------------------------
declare sub      PsButton_SetPaintCallback( byval hButton as HWND, byval usersub as BTN_PaintCallbackSub )
declare sub      PsButton_SetMessageCallback( byval hButton as HWND, byval userfunc as BTN_MessageCallbackFunc )
declare sub      PsButton_SetTooltipCallback( byval hButton as HWND, byval userfunc as BTN_TooltipCallbackFunc )
declare sub      PsButton_SetClickCallback( byval hButton as HWND, byval usersub as BTN_ClickCallbackSub )

' ----------------------------------------------------------------------------------------
' Plumbing.
'
'   CountRenderedTones - render the control OFFSCREEN with its current painter and return how
'                 many distinct colours land inside one of its rects (capped at 64). Nothing in
'                 the control calls it; it exists to make one specific defect measurable.
'
'                 THE DEFECT: PsBufferPaint.PaintBorderRect FILLS before it strokes, so a paint
'                 callback that reaches for it to draw a frame or a focus ring floods everything
'                 beneath and the control renders as one solid block. That has shipped three
'                 times in this family (PsToggle, PsComboBox, PsNumericUpDown), every time from a
'                 copied callback, and it survives a glance because the flood colour is a real
'                 colour from the control. A wiped part is literally ONE tone, so
'                 "count > 1" is an unambiguous assertion where a screenshot is a judgement.
'
'                 A HOST THAT WRITES ITS OWN PaintCallback SHOULD ASSERT THIS. That is the whole
'                 reason it is public rather than private to the self-test.
'
'                 It renders with isFocused forced TRUE (restored afterwards) so the focus-ring
'                 path -- the one the defect lives on -- is actually exercised. The PAINTINFO is
'                 built by the same routine WM_PAINT uses, because a probe that hand-builds that
'                 struct and leaves one field zeroed silently skips the thing it is measuring;
'                 that produced a near-miss false PASS the first time this was written
'                 (Learnings.md). Returns 0 if the control has no geometry yet.
'
'   RunSelfTest - geometry, colour-resolution and window-text assertions, gated on the
'                 PSBUTTON_SELFTEST env var. Call it from the host before the message loop.
' ----------------------------------------------------------------------------------------
declare function PsButton_CountRenderedTones( byval hButton as HWND, byval nPart as long ) as long
declare sub      PsButton_RunSelfTest( byval hWndParent as HWND )
