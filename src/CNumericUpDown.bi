''
''  CNumericUpDown.bi  --  owner-drawn numeric up/down (spinner) with an editable value
''

#pragma once

#include once "CBufferPaint.bi"
#include once "CTextBox.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves a button.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, AND the CTextBox child
' covers the middle of our client, so a periodic cursor check is the safety net. Timer ids
' are per-window, so every instance can share these two.
#define IDT_CNUMERICUPDOWN_HOTTRACK   &hCB80
#define IDT_CNUMERICUPDOWN_REPEAT     &hCB81
#define CNUMERICUPDOWN_HOTTRACK_MS    100

' Defaults. The SIZE values are DPI-scaled at Create; every setter afterwards takes raw
' pixels and the caller scales (the family rule).
'
' TWO of the three THICKNESS values are NOT DPI-scaled, ever -- a hairline should stay a
' hairline (CMenuBar's nSeparatorThickness rule): the border and the divider are RULES, and a
' rule that thickens with the display stops reading as a rule.
'
' THE GLYPH THICKNESS IS THE EXCEPTION, and the asymmetry is deliberate -- do not "harmonise"
' it. The bars of the "-" and "+" are not rules, they are the strokes of an ICON, and an icon
' whose length scales while its weight does not gets thinner and thinner relative to itself as
' the display scales. At 1.75x an unscaled bar is 18 pixels long and 1 thick, which reads as a
' thread rather than a minus sign. Asked of a real screenshot, not reasoned about.
#define CNUD_DEFAULT_BUTTONW        28
#define CNUD_DEFAULT_VALUEPAD        6     ' left and right, inside the value cell
#define CNUD_DEFAULT_VERTPAD         5     ' above and below the text, for GetIdealSize
#define CNUD_DEFAULT_CORNERRADIUS    6
#define CNUD_DEFAULT_BORDERTHICK     1     ' not DPI-scaled
#define CNUD_DEFAULT_DIVIDERTHICK    1     ' not DPI-scaled
#define CNUD_DEFAULT_GLYPHLENGTH    10     ' the bar of the "-", and each bar of the "+"
#define CNUD_DEFAULT_GLYPHTHICK      1     ' DPI-scaled -- it is an icon stroke, not a rule

' Auto-repeat: hold a button and it keeps stepping. Delay before the first repeat, then one
' step per interval. Either at 0 disables repeating entirely (one click = one step).
#define CNUD_DEFAULT_REPEATDELAY   400
#define CNUD_DEFAULT_REPEATINTERVAL 60

' Value defaults. The range is deliberately wide: a host that never calls SetRange gets a
' control that does not silently clamp, but the range machinery is always present so the
' buttons can grey out at a limit.
#define CNUD_DEFAULT_MIN            -1000000.0
#define CNUD_DEFAULT_MAX             1000000.0
#define CNUD_DEFAULT_INCREMENT       1.0
#define CNUD_DEFAULT_DECIMALPLACES   0

' The three hit-testable parts, plus "nothing". The value cell is reported by HitTest for
' completeness, but the control never acts on it -- that area belongs to the CTextBox child,
' which receives its own mouse messages and never routes them here.
enum
    NUD_PART_NONE = 0
    NUD_PART_MINUS
    NUD_PART_VALUE
    NUD_PART_PLUS
end enum


' Colors for the built-in painter. Copied on Set.
'
' Flat COLORREF fields with defaults, exactly like CTOGGLE_COLORS and CSELECTBAR_COLORS --
' read-modify-write is CNumericUpDown_GetColors, assign, CNumericUpDown_SetColors.
'
' THE CONTROL READS AS ONE FLAT CELL until you hover a button, because ButtonBackColor is
' defaulted EQUAL to ValueBackColor. That is CSelectBar's trick (BackColorHot = BackColor)
' applied the other way round, and it is what makes the dividers the only thing separating
' the three parts at rest. A host that wants visibly raised buttons just sets the field.
'
' The value cell's two colors are pushed into the embedded CTextBox rather than painted by
' us -- the RichEdit draws the number. They live here so a host sets every color of the
' control in one place, which is the point of the struct.
type CNUMERICUPDOWN_COLORS
    BackColor                as COLORREF = BGR( 33, 37, 43)   ' behind the rounded corners
    BorderColor              as COLORREF = BGR( 60, 66, 77)
    FocusBorderColor         as COLORREF = BGR( 86,156,214)   ' while the value field has focus
    BorderColorDisabled      as COLORREF = BGR( 48, 52, 60)
    DividerColor             as COLORREF = BGR( 60, 66, 77)
    DividerColorDisabled     as COLORREF = BGR( 48, 52, 60)
    ' --- the value cell (handed to the CTextBox, not painted here) ---
    ValueBackColor           as COLORREF = BGR( 44, 49, 58)
    ValueForeColor           as COLORREF = BGR(215,218,224)
    ValueBackColorDisabled   as COLORREF = BGR( 38, 42, 49)
    ValueForeColorDisabled   as COLORREF = BGR( 90, 96,106)
    ' --- the two button cells ---
    ButtonBackColor          as COLORREF = BGR( 44, 49, 58)   ' = ValueBackColor (see above)
    ButtonBackColorHot       as COLORREF = BGR( 56, 62, 73)
    ButtonBackColorPressed   as COLORREF = BGR( 38, 79,120)
    ButtonBackColorDisabled  as COLORREF = BGR( 38, 42, 49)
    GlyphColor               as COLORREF = BGR(190,196,206)
    GlyphColorHot            as COLORREF = BGR(255,255,255)
    GlyphColorPressed        as COLORREF = BGR(255,255,255)
    GlyphColorDisabled       as COLORREF = BGR( 90, 96,106)
end type


' Everything the painter needs. Every rect is precomputed by LayoutControl -- never
' re-derive one from another, and in particular never derive a glyph bar from its cell by
' repeating the centring arithmetic, because SetGlyphSize can change it underneath you.
'
' NOTE WHAT IS NOT HERE: the value text. It is drawn by the RichEdit inside the embedded
' CTextBox, not by this control and not by a paint callback. A callback replaces the frame,
' the cells, the dividers and the glyphs -- never the number.
type CNUMERICUPDOWN_PAINTINFO
    hCtrl        as HWND                ' the control, so the callback can query it
    b            as CBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutControl ---
    rcClient     as RECT                ' the whole client area
    rcFrame      as RECT                ' where the rounded border is stroked
    rcMinus      as RECT                ' the left button cell
    rcDiv1       as RECT                ' the hairline between the "-" cell and the value
    rcValue      as RECT                ' the value cell -- the CTextBox child sits exactly here
    rcDiv2       as RECT                ' the hairline between the value and the "+" cell
    rcPlus       as RECT                ' the right button cell
    rcMinusBar   as RECT                ' the "-" glyph
    rcPlusBarH   as RECT                ' the "+" glyph, horizontal bar
    rcPlusBarV   as RECT                ' the "+" glyph, vertical bar
    ' --- State: the control decides these, you only render. ---
    hotPart      as long                ' NUD_PART_* under the cursor (NONE if not hot)
    pressedPart  as long                ' NUD_PART_* held down AND the cursor still inside
    isEnabled    as boolean
    isFocused    as boolean             ' the value field owns the keyboard focus
    bAtMin       as boolean             ' the "-" button can do nothing: draw it disabled
    bAtMax       as boolean             ' likewise the "+" button
    nValue       as double              ' the current value, already clamped and rounded
end type

type CNUMERICUPDOWN_MESSAGEINFO
    hCtrl        as HWND
    uMsg         as UINT
    wParam       as WPARAM
    lParam       as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with BackColor before calling you, so a callback
' that only wants to add something on top does not have to repaint the background.
'
' It does NOT draw the number: that is the RichEdit child's job, and it paints itself over
' whatever this callback puts in rcValue.
type NUD_PaintCallbackSub as sub( byval p as CNUMERICUPDOWN_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' TWO SOURCES FEED THIS ONE CALLBACK. The container's own messages (the mouse over the two
' buttons, the timers, enable/disable) arrive directly; the value field's key, wheel and
' focus messages arrive relayed from the embedded CTextBox's own message callback, with
' m->hCtrl rewritten to THIS control. So a host sees one message stream for the whole
' control and never has to know a RichEdit is in there.
'
' CAUTION: the result is IGNORED for two messages.
'   WM_LBUTTONUP  - the control holds mouse capture across a button press and the up-message
'                   is what releases it: a callback that suppressed it would strand capture
'                   and route every subsequent click here (the CListBox bug recorded in
'                   Learnings.md). Suppressing WM_LBUTTONDOWN suppresses the press, never
'                   the capture bookkeeping.
'   WM_KILLFOCUS  - focus loss is what COMMITS a typed value. Suppressing it would leave the
'                   control displaying text it has not accepted. (WM_SETFOCUS may be
'                   suppressed; nothing depends on it but the border color.)
type NUD_MessageCallbackFunc as function( byval m as CNUMERICUPDOWN_MESSAGEINFO ptr ) as boolean

' The value changed through USER interaction. Fired AFTER the value is clamped, rounded and
' displayed, so CNumericUpDown_GetValue() = nValue inside the handler.
'
' WHEN IT FIRES: immediately for a button click, an auto-repeat tick, an arrow key, PgUp/PgDn
' and the wheel -- every one of those produces a complete value. For TYPING it fires on
' COMMIT ONLY (ENTER, or focus leaving the field), which is also when the typed text is
' clamped and reformatted. Typing "16" therefore never reports the transient 1.
'
' The programmatic setters (SetValue, SetRange, SetDecimalPlaces, StepUp, StepDown) are all
' SILENT -- Win32's BM_SETCHECK / BN_CLICKED precedent, and what makes it safe to call one
' from inside this handler.
type NUD_ValueChangedCallbackSub as sub( byval hCtrl as HWND, byval nValue as double )


type CNUMERICUPDOWN
    hWin            as HWND
    hTextBox        as HWND               ' the embedded CTextBox (numeric mode, borderless)
    idc_Ctrl        as long = 0
    ' --- State. Single-valued: there is exactly one of this control, so unlike the
    '     collection siblings there are no indices here to be left dangling. ---
    isEnabled       as boolean = true
    isFocused       as boolean = false    ' the value field has the keyboard focus
    bReadOnly       as boolean = false
    hotPart         as long = NUD_PART_NONE
    pressedPart     as long = NUD_PART_NONE   ' which button is held (see the capture note)
    hotTimerOn      as boolean = false
    repeatTimerOn   as boolean = false
    ' --- Press state (mouse capture). The control TAKES capture on a button press: the
    '     press/cancel gesture (press, slide off the button, release, the value does NOT
    '     step) consumes the guaranteed down->up pairing, which is the family's test for
    '     taking capture. It pays the full price: snapshot the press BEFORE releasing
    '     (ReleaseCapture sends WM_CAPTURECHANGED synchronously -- Learnings.md), release on
    '     the up-message before any callback runs, WM_CAPTURECHANGED cancels the press and
    '     kills the repeat timer, WM_DESTROY releases, and no callback may suppress an
    '     up-message. ---
    ' --- Value. THE CTEXTBOX'S TEXT IS THE SOURCE OF TRUTH; nValue is the last value the
    '     control committed, kept only so a change can be detected and reported once. ---
    nValue          as double = 0.0
    nMin            as double = CNUD_DEFAULT_MIN
    nMax            as double = CNUD_DEFAULT_MAX
    nIncrement      as double = CNUD_DEFAULT_INCREMENT
    nLargeIncrement as double = CNUD_DEFAULT_INCREMENT * 10.0
    nDecimalPlaces  as long   = CNUD_DEFAULT_DECIMALPLACES
    nRepeatDelayMs    as long = CNUD_DEFAULT_REPEATDELAY
    nRepeatIntervalMs as long = CNUD_DEFAULT_REPEATINTERVAL
    ' Sub-notch wheel accumulation, the scrollbar siblings' rule: a precision touchpad sends
    ' deltas well under one 120 notch, and truncating each one would ignore a slow swipe
    ' entirely.
    nWheelAccum     as long = 0
    colors          as CNUMERICUPDOWN_COLORS
    hFont           as HFONT = 0          ' CALLER-OWNED; never deleted here
    ' --- Layout. Rects are DERIVED, never set from outside; LayoutControl() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs
    '     it. LayoutControl takes NO DC -- every cell is derived from the client rect and
    '     authored scalars, and nothing in it is measured. Only GetIdealSize measures, and
    '     it takes its own DC to do it. ---
    nButtonWidth    as long = CNUD_DEFAULT_BUTTONW        ' DPI-scaled at Create
    nValuePadLeft   as long = CNUD_DEFAULT_VALUEPAD       ' DPI-scaled at Create
    nValuePadRight  as long = CNUD_DEFAULT_VALUEPAD       ' DPI-scaled at Create
    nVertPadding    as long = CNUD_DEFAULT_VERTPAD        ' DPI-scaled at Create
    nCornerRadius   as long = CNUD_DEFAULT_CORNERRADIUS   ' DPI-scaled at Create
    nBorderThick    as long = CNUD_DEFAULT_BORDERTHICK    ' NOT DPI-scaled
    nDividerThick   as long = CNUD_DEFAULT_DIVIDERTHICK   ' NOT DPI-scaled
    nGlyphLength    as long = CNUD_DEFAULT_GLYPHLENGTH    ' DPI-scaled at Create
    nGlyphThick     as long = CNUD_DEFAULT_GLYPHTHICK     ' DPI-scaled at Create (see above)
    rcFrame         as RECT               ' derived
    rcMinus         as RECT               ' derived
    rcDiv1          as RECT               ' derived
    rcValue         as RECT               ' derived
    rcDiv2          as RECT               ' derived
    rcPlus          as RECT               ' derived
    rcMinusBar      as RECT               ' derived
    rcPlusBarH      as RECT               ' derived
    rcPlusBarV      as RECT               ' derived
    bLayoutDirty    as boolean = true
    PaintCallback        as NUD_PaintCallbackSub          ' optional; replaces the built-in painter
    MessageCallback      as NUD_MessageCallbackFunc       ' optional
    ValueChangedCallback as NUD_ValueChangedCallbackSub   ' optional; user-driven changes only

    declare sub      LayoutControl()
    declare function ClampToRange( byval nValue as double ) as double
    declare function SnapToPlaces( byval nValue as double ) as double
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' Round to the decimal grid the control displays on. Applied after EVERY step, not merely on
' display: adding 0.1 ten times to a binary double lands on 0.9999999999999999, and a value
' that is a hair below the grid formats correctly but compares wrong -- so the drift would be
' invisible until the range check at a limit disagreed with the text on screen.
function CNUMERICUPDOWN.SnapToPlaces( byval nValue as double ) as double
    dim as double scaleBy = 10.0 ^ this.nDecimalPlaces
    ' Int(x + 0.5) rounds half away from zero for positives; mirror it for negatives so
    ' -0.5 rounds to -1 rather than to 0 (FreeBASIC's Int floors).
    if nValue < 0 then return -int( (-nValue * scaleBy) + 0.5 ) / scaleBy
    return int( (nValue * scaleBy) + 0.5 ) / scaleBy
end function

' Clamp into [nMin, nMax]. A host that sets an inverted range (min > max) gets nMin, which is
' at least deterministic; the setter also swaps them, so this is only reachable if the fields
' are written directly.
function CNUMERICUPDOWN.ClampToRange( byval nValue as double ) as double
    if nValue > this.nMax then nValue = this.nMax
    if nValue < this.nMin then nValue = this.nMin
    return nValue
end function


' Assign every rect. This is the only place the control's geometry is produced; painting,
' the child's placement, invalidation and every public rect query consume it.
'
'   rcFrame  = the whole client (the rounded border is stroked INSIDE it, over the cells)
'
'   rcMinus  = client.left ..              client.left + nButtonWidth
'   rcDiv1   = rcMinus.right ..            + nDividerThick
'   rcPlus   = client.right - nButtonWidth .. client.right
'   rcDiv2   = rcPlus.left - nDividerThick .. rcPlus.left
'   rcValue  = rcDiv1.right ..             rcDiv2.left       (the CTextBox goes exactly here)
'
' THE CELLS ARE NOT DEFLATED BY THE BORDER, and that is deliberate. The button cells have to
' reach the frame's rounded corners: a cell inset by the border width would round its own
' corners a pixel inside the frame's, leaving a sliver of the base fill showing through
' whenever a hovered button's color differs from it. So the cells span the full client and
' the border is stroked over their outer edges, last.
'
' rcValue IS inset vertically, by exactly nBorderThick, and only rcValue. The CTextBox child
' covers every pixel of that rect and paints its own background there -- so a full-height
' value cell would put the child on top of the frame's top and bottom rows and the border
' would be interrupted across the middle of the control. Nothing else needs the inset,
' because nothing else is covered by a child window.
'
'   the glyph bars are centred in their own cell:
'     rcMinusBar = nGlyphLength x nGlyphThick,  centred in rcMinus
'     rcPlusBarH = nGlyphLength x nGlyphThick,  centred in rcPlus
'     rcPlusBarV = nGlyphThick  x nGlyphLength, centred in rcPlus
'
' Both "+" bars are centred on the SAME cell centre rather than one being derived from the
' other, so the cross is exactly symmetric whatever the parity of the cell and the bar.
'
' OVERFLOW: when the inner width cannot hold both buttons and both dividers, the value cell
' collapses to zero width (left = right) and the buttons keep their size, clipping at the
' client edge. Rects are computed HONESTLY rather than squeezed (CIconPanel's and CTabBar's
' rule), so the buttons stay square and only what is past the edge is lost. rcValue is never
' allowed to invert -- a negative-width rect handed to SetWindowPos is a different kind of
' bug from a clipped one.
'
' NO DC IS TAKEN. Every number above is authored or derived from the client rect; the value
' text is measured by the RichEdit, not by us. CNumericUpDown_GetIdealSize is the one place
' this control measures anything, and it opens its own DC.
sub CNUMERICUPDOWN.LayoutControl()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again. Note that
    ' CNumericUpDown_GetIdealSize deliberately does NOT come through here -- it is computed
    ' from the scalars and a measured string, because a host calls it to decide how big to
    ' make the control in the first place.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcFrame = rcClient

    dim as long L = rcClient.left
    dim as long T = rcClient.top
    dim as long R = rcClient.right
    dim as long B = rcClient.bottom

    SetRect( @this.rcMinus, L, T, L + this.nButtonWidth, B )
    SetRect( @this.rcDiv1, this.rcMinus.right, T, this.rcMinus.right + this.nDividerThick, B )
    SetRect( @this.rcPlus, R - this.nButtonWidth, T, R, B )
    SetRect( @this.rcDiv2, this.rcPlus.left - this.nDividerThick, T, this.rcPlus.left, B )

    ' The value cell alone is inset vertically -- see the note above: it is the only rect a
    ' child window covers, and a child over the top and bottom rows would break the frame.
    dim as long bt = this.nBorderThick
    dim as long valueL = this.rcDiv1.right
    dim as long valueR = this.rcDiv2.left
    if valueR < valueL then valueR = valueL
    dim as long valueT = T + bt
    dim as long valueB = B - bt
    if valueB < valueT then valueB = valueT
    SetRect( @this.rcValue, valueL, valueT, valueR, valueB )

    ' --- the glyphs ---------------------------------------------------------------------
    dim as long gl = this.nGlyphLength
    dim as long gt = this.nGlyphThick
    if gl < 1 then gl = 1
    if gt < 1 then gt = 1

    dim as long cxMinus = this.rcMinus.left + ((this.rcMinus.right - this.rcMinus.left) \ 2)
    dim as long cyMinus = this.rcMinus.top + ((this.rcMinus.bottom - this.rcMinus.top) \ 2)
    SetRect( @this.rcMinusBar, cxMinus - (gl \ 2), cyMinus - (gt \ 2), _
                               cxMinus - (gl \ 2) + gl, cyMinus - (gt \ 2) + gt )

    dim as long cxPlus = this.rcPlus.left + ((this.rcPlus.right - this.rcPlus.left) \ 2)
    dim as long cyPlus = this.rcPlus.top + ((this.rcPlus.bottom - this.rcPlus.top) \ 2)
    SetRect( @this.rcPlusBarH, cxPlus - (gl \ 2), cyPlus - (gt \ 2), _
                               cxPlus - (gl \ 2) + gl, cyPlus - (gt \ 2) + gt )
    SetRect( @this.rcPlusBarV, cxPlus - (gt \ 2), cyPlus - (gl \ 2), _
                               cxPlus - (gt \ 2) + gt, cyPlus - (gl \ 2) + gl )
end sub


' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's job,
' and only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it. The repeat timer IS killed here, because it is ours alone and a tick
' arriving after the gesture ended would step the value with nothing holding the button.
sub CNUMERICUPDOWN.CancelPress()
    this.pressedPart = NUD_PART_NONE
    if this.repeatTimerOn then
        if this.hWin then KillTimer( this.hWin, IDT_CNUMERICUPDOWN_REPEAT )
        this.repeatTimerOn = false
    end if
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy.
sub CNUMERICUPDOWN.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking cell is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A NUMERIC UP/DOWN: a rounded frame holding a "-" button, an editable numeric field and a
' "+" button, separated by hairline dividers. The value has a range, a settable number of
' decimal places, and a settable increment.
'
' IT WRAPS A REAL CHILD
'   The value field is a CTextBox in numeric mode, whose own child is a RichEdit50W. That is
'   where typing, selection, the clipboard, undo and the right-click menu come from -- none
'   of it is reimplemented here. The window tree is:
'
'       CNumericUpDown container      WS_EX_CONTROLPARENT
'         +-- CTextBox                borderless: THIS control owns all the chrome
'               +-- RichEdit50W       WS_TABSTOP -- the real focus target
'
'   CNumericUpDown_GetTextBoxHandle is the escape hatch: every CTextBox_* setter applies to
'   it (cue banner, limit text, context-menu theming). Two of them are OWNED by this control
'   and must not be set behind its back -- CTextBox_SetValue/SetDecimalPlaces (use ours, or
'   the cached value and the buttons' limit state go stale) and CTextBox_SetBorderWidth
'   (nonzero would draw a second frame inside ours).
'
' FOCUS
'   Keyboard focus sits on the RichEdit two levels down, so GetFocus() = hCtrl is ALWAYS
'   false -- use CNumericUpDown_HasFocus(). The frame is drawn in FocusBorderColor while the
'   field has focus; there is no separate focus ring and therefore no reserved ring band, so
'   the control's ideal size does not change when focus arrives.
'
'   TAB NAVIGATION works without IsDialogMessage in the host's pump, because CTextBox
'   handles VK_TAB itself. It needs WS_EX_CONTROLPARENT on every window between the RichEdit
'   and the top-level window -- this container sets it, which is the only reason tabbing
'   works through the extra nesting level this control adds.
'
' THE HOST MESSAGE-PUMP OBLIGATION -- NOT OPTIONAL
'   The value field carries CTextBox's built-in right-click Cut/Copy/Paste/Select All menu,
'   which is a CPopupMenu and therefore not modal: keyboard navigation and click-outside
'   dismissal live in a message filter. A host that never calls CNumericUpDown_FilterMessage
'   gets a menu that opens and paints but cannot be driven from the keyboard and never
'   closes on an outside click.
'
'       do while GetMessage(@uMsg, null, 0, 0)
'           if CNumericUpDown_FilterMessage( @uMsg ) then continue do
'           TranslateMessage @uMsg
'           DispatchMessage @uMsg
'       loop
'
'   It forwards to CTextBox_FilterMessage, so an app already calling that is covered either
'   way and calling both is harmless.
'
' NO TOOLTIPS
'   No tooltip window is created and there is no NUD_TooltipCallback. A host that wants a tip
'   can add its own tool over the control's HWND.
'
' THE CONTROL HANDLE
'   Every CNumericUpDown_* function takes the handle returned by CNumericUpDown_Create().
'   It is a real HWND on purpose (not an opaque type): callers legitimately need to treat the
'   control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed, taking the CTextBox with it. It
'   owns no host resources -- in particular the font is caller-owned and is never deleted.
'
' ----------------------------------------------------------------------------------------
' Creation.
'   CtrlID becomes the control window's id (GWLP_ID), so hosts can find it with GetDlgItem.
'   The control is created zero-sized: size it with CNumericUpDown_GetIdealSize and position
'   it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function CNumericUpDown_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND
declare function CNumericUpDown_GetTextBoxHandle( byval hCtrl as HWND ) as HWND
declare function CNumericUpDown_HasFocus( byval hCtrl as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Value.  EVERY SETTER HERE IS SILENT -- only user interaction fires the change callback.
'
'   SetValue clamps to the range and snaps to the decimal grid, so what you set is what
'     GetValue returns, and it is what is displayed.
'   SetRange re-clamps the current value (silently, again) and swaps an inverted pair.
'   SetDecimalPlaces re-formats the displayed value; 0 means integers only, and the field
'     then refuses a typed decimal separator outright.
'   StepUp/StepDown are the programmatic door to exactly what the buttons do -- and, being
'     programmatic, they do NOT notify. The buttons call the same arithmetic internally and
'     then notify.
'   SetReadOnly stops TYPING only. The buttons, the arrow keys and the wheel still work: it
'     is "you may not type an arbitrary number here", not "this control is inert" -- that is
'     SetEnabled.
' ----------------------------------------------------------------------------------------
declare function CNumericUpDown_GetValue( byval hCtrl as HWND ) as double
declare sub      CNumericUpDown_SetValue( byval hCtrl as HWND, byval nValue as double )
declare sub      CNumericUpDown_GetRange( byval hCtrl as HWND, byref nMin as double, byref nMax as double )
declare sub      CNumericUpDown_SetRange( byval hCtrl as HWND, byval nMin as double, byval nMax as double )
declare function CNumericUpDown_GetIncrement( byval hCtrl as HWND ) as double
declare sub      CNumericUpDown_SetIncrement( byval hCtrl as HWND, byval nIncrement as double )
declare function CNumericUpDown_GetLargeIncrement( byval hCtrl as HWND ) as double
declare sub      CNumericUpDown_SetLargeIncrement( byval hCtrl as HWND, byval nIncrement as double )
declare function CNumericUpDown_GetDecimalPlaces( byval hCtrl as HWND ) as long
declare sub      CNumericUpDown_SetDecimalPlaces( byval hCtrl as HWND, byval nPlaces as long )
declare sub      CNumericUpDown_StepUp( byval hCtrl as HWND )
declare sub      CNumericUpDown_StepDown( byval hCtrl as HWND )
declare function CNumericUpDown_GetReadOnly( byval hCtrl as HWND ) as boolean
declare sub      CNumericUpDown_SetReadOnly( byval hCtrl as HWND, byval bReadOnly as boolean )

' ----------------------------------------------------------------------------------------
' State and behavior.
'
'   SetEnabled(false) greys the whole control, blocks hover, and swallows clicks and keys. It
'     goes through EnableWindow -- on this container AND on the CTextBox child -- rather than
'     setting a cosmetic flag, so the disable is enforced by the system and no SetFocus can
'     sneak into a control that looks dead. WM_ENABLE syncs the flag back, so a host that
'     reaches for EnableWindow directly still gets the greyed rendering.
'   SetAutoRepeat: milliseconds. Either value <= 0 disables repeating (one click = one step).
'     Repeating stops on its own the moment the value reaches a limit.
' ----------------------------------------------------------------------------------------
declare function CNumericUpDown_GetEnabled( byval hCtrl as HWND ) as boolean
declare sub      CNumericUpDown_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
declare sub      CNumericUpDown_GetAutoRepeat( byval hCtrl as HWND, byref nDelayMs as long, byref nIntervalMs as long )
declare sub      CNumericUpDown_SetAutoRepeat( byval hCtrl as HWND, byval nDelayMs as long, byval nIntervalMs as long )
declare sub      CNumericUpDown_Refresh( byval hCtrl as HWND )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). The BORDER and DIVIDER thicknesses should not be
' scaled at all -- a hairline stays a hairline -- but the GLYPH thickness should be, because
' it is an icon stroke rather than a rule. See the defines at the top of this file.
'
'   SetFont is CALLER-OWNED -- the control never deletes the HFONT. It is handed to the
'     CTextBox, which converts it to a CHARFORMATW internally.
'   SetValuePadding: nLeft/nRight are the text margins inside the value cell; nVert is used
'     only by GetIdealSize, to decide the height around one line of text.
'   SetGlyphSize: the length of the "-" bar (and of each "+" bar) and its thickness.
'
'   GetIdealSize measures the WIDEST value the range can produce at the current decimal
'   places, in the current font, and adds the padding, both buttons, both dividers and the
'   border. This is the only place the control measures anything, and it opens its own DC to
'   do it -- so it is valid BEFORE the control has ever been sized.
'
'   The rect queries force a pending layout, so results are always current. They return
'   FALSE if the control has no geometry yet (created but never sized).
' ----------------------------------------------------------------------------------------
declare function CNumericUpDown_GetFont( byval hCtrl as HWND ) as HFONT
declare sub      CNumericUpDown_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function CNumericUpDown_GetButtonWidth( byval hCtrl as HWND ) as long
declare sub      CNumericUpDown_SetButtonWidth( byval hCtrl as HWND, byval nWidth as long )
declare sub      CNumericUpDown_GetValuePadding( byval hCtrl as HWND, byref nLeft as long, byref nRight as long, byref nVert as long )
declare sub      CNumericUpDown_SetValuePadding( byval hCtrl as HWND, byval nLeft as long, byval nRight as long, byval nVert as long )
declare function CNumericUpDown_GetCornerRadius( byval hCtrl as HWND ) as long
declare sub      CNumericUpDown_SetCornerRadius( byval hCtrl as HWND, byval nRadius as long )
declare function CNumericUpDown_GetBorderThickness( byval hCtrl as HWND ) as long
declare sub      CNumericUpDown_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
declare function CNumericUpDown_GetDividerThickness( byval hCtrl as HWND ) as long
declare sub      CNumericUpDown_SetDividerThickness( byval hCtrl as HWND, byval nThickness as long )
declare sub      CNumericUpDown_GetGlyphSize( byval hCtrl as HWND, byref nLength as long, byref nThickness as long )
declare sub      CNumericUpDown_SetGlyphSize( byval hCtrl as HWND, byval nLength as long, byval nThickness as long )
declare sub      CNumericUpDown_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
declare function CNumericUpDown_GetFrameRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function CNumericUpDown_GetMinusRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function CNumericUpDown_GetValueRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function CNumericUpDown_GetPlusRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
' Which part is at pt (CLIENT coordinates)? Public so a host can special-case a region, and
' so the self-test can assert that hit-testing and the layout agree.
declare function CNumericUpDown_HitTest( byval hCtrl as HWND, byval pt as POINT ) as long

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct and pushes the value cell's two colors into
' the embedded CTextBox. GetColors fills one out, so the read-modify-write "change one color"
' idiom is Get, assign, Set.
' ----------------------------------------------------------------------------------------
declare sub      CNumericUpDown_GetColors( byval hCtrl as HWND, byval pColors as CNUMERICUPDOWN_COLORS ptr )
declare sub      CNumericUpDown_SetColors( byval hCtrl as HWND, byval pColors as CNUMERICUPDOWN_COLORS ptr )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback        - draw the frame, cells, dividers and glyphs INSTEAD OF the
'                          built-in painter. It never draws the number.
'   MessageCallback      - observe the container's mouse/timer messages AND the value
'                          field's relayed key/wheel/focus messages; return TRUE to suppress
'                          default handling (IGNORED for WM_LBUTTONUP and WM_KILLFOCUS).
'   ValueChangedCallback - the USER changed the value. Programmatic setters are silent.
' ----------------------------------------------------------------------------------------
declare sub      CNumericUpDown_SetPaintCallback( byval hCtrl as HWND, byval usersub as NUD_PaintCallbackSub )
declare sub      CNumericUpDown_SetMessageCallback( byval hCtrl as HWND, byval userfunc as NUD_MessageCallbackFunc )
declare sub      CNumericUpDown_SetValueChangedCallback( byval hCtrl as HWND, byval usersub as NUD_ValueChangedCallbackSub )

' ----------------------------------------------------------------------------------------
' Host message pump.  See THE HOST MESSAGE-PUMP OBLIGATION above -- this is not optional.
' ----------------------------------------------------------------------------------------
declare function CNumericUpDown_FilterMessage( byval pMsg as MSG ptr ) as boolean
