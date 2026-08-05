''
''  PsCheckBox.bi  --  owner-drawn checkbox (Fluent box glyph + caption)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "PsBufferPaint.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly
' as it always has; a host opts an instance into PsTooltip with PsCheckBox_SetTooltipMode.
#include once "PsTipHost.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
' Distinct from PsButton's &hCB90 and PsToggle's &hCB70 purely as a debugging courtesy -- nothing
' requires it, since the ids never share a window.
#define IDT_CCHECKBOX_HOTTRACK   &hCBA0
#define PSCHECKBOX_HOTTRACK_MS    100

' The two Segoe Fluent Icons codepoints the control draws by default.
'
'   E739  Checkbox           an empty outlined square
'   E73A  CheckboxComposite  the same square with a tick in it
'
' These are the pair tiko has shipped for years (modDeclares.bi), so they are confirmed by use
' rather than read off a chart. BOTH ARE SETTABLE -- see PsCheckBox_SetGlyphUnchecked. The
' control has no opinion about which font supplies them; it draws the string it is given with
' the HFONT it is handed.
'
' NOTE THE CONSEQUENCE OF ONE COMPOSITE GLYPH PER STATE: the box outline and the tick inside it
' are a single piece of ink and therefore a single colour. An accent-FILLED box with a
' contrasting white tick is not reachable this way -- that would need the box drawn as geometry
' and the tick drawn as a separate glyph over it, which is a different control. This was decided
' deliberately; see the header at the PUBLIC API below.
' Written as \u ESCAPES, not as literal characters: this file has no BOM, and fbc reads a
' BOM-less source as bytes -- a literal U+E739 pasted in here would arrive as its three UTF-8
' bytes and draw three garbage glyphs. tiko writes them the same way for the same reason.
#define PSCHECKBOX_GLYPH_UNCHECKED  !"\uE739"
#define PSCHECKBOX_GLYPH_CHECKED    !"\uE73A"

' Default geometry. Everything except nFocusThick is DPI-scaled once at Create; every setter
' afterwards takes raw pixels and the caller scales (the family rule).
'
' The padding is deliberately SMALL. Unlike PsButton this control draws no chrome, so padding is
' not the visual inset of a button face -- it is only breathing room around the content, and it
' matters mainly to a host that opts into a hover row highlight by setting BackColorHot. There
' is no reference screenshot for this control, so these are a judgement call.
#define PSCHECKBOX_DEFAULT_PADLEFT     4
#define PSCHECKBOX_DEFAULT_PADTOP      2
#define PSCHECKBOX_DEFAULT_PADRIGHT    4
#define PSCHECKBOX_DEFAULT_PADBOTTOM   2
' Gap between the box cell and the caption. Spent ONLY when there is a caption to separate the
' box from -- a caption-less checkbox charges padding + box + padding and nothing else.
#define PSCHECKBOX_DEFAULT_BOXGAP      8
' The box cell. DECLARED, never measured (PsIconPanel's rule): the glyph font is a paint-time
' input, so a glyph too large for the cell simply clips rather than resizing the control.
#define PSCHECKBOX_DEFAULT_BOXWIDTH   16
#define PSCHECKBOX_DEFAULT_BOXHEIGHT  16
#define PSCHECKBOX_DEFAULT_FOCUSGAP    2
#define PSCHECKBOX_DEFAULT_FOCUSCURV   4     ' ellipse DIAMETER, not a radius
#define PSCHECKBOX_DEFAULT_FOCUSTHICK  1     ' not DPI-scaled


' Which side of the caption the box sits on.
'
' THE BOX CELL IS PINNED TO THE PADDING on whichever side it is given, and the caption takes the
' whole SPAN left over -- exactly the way PsButton pins its icon cells and PsComboBox its chevron.
' That single model gives both looks, decided by nothing but how the host sizes the control:
'
'   sized to PsCheckBox_GetIdealSize   [x] Enable dark mode          packed
'   sized to a full row width         Enable dark mode         [x]  settings row
'
' So a column of BOXALIGN_RIGHT checkboxes all sized to the same width line their boxes up for
' free, which is the reason a "pack it next to the caption" model was rejected.
enum
    CHK_BOXALIGN_LEFT = 0
    CHK_BOXALIGN_RIGHT
end enum

' Where the caption sits WITHIN ITS SPAN -- which is the leftover after the box cell and the
' padding, not the control. With BOXALIGN_LEFT and CHK_TEXTALIGN_RIGHT on an over-wide control
' the caption goes to the right-hand end of the span, not to the right-hand end of the window.
' (PsButton's rcText carries the identical caveat and it is the natural thing to get wrong.)
enum
    CHK_TEXTALIGN_LEFT = 0
    CHK_TEXTALIGN_CENTER
    CHK_TEXTALIGN_RIGHT
end enum

' Which of the four colour sets the painter uses. Produced by PsCheckBox_ResolveMood, which is a
' PURE FUNCTION precisely so its truth table can be asserted (PsScrollPanel_ShouldShow's and
' PsButton_ResolveMood's precedent) rather than only inspected inside a paint.
'
' isChecked is deliberately NOT a mood. It selects between two BOX colours within whichever mood
' resolved, so it survives all four for free instead of doubling the matrix.
enum
    CHK_MOOD_IDLE = 0
    CHK_MOOD_HOT
    CHK_MOOD_PRESSED
    CHK_MOOD_DISABLED
end enum

' Which rect the two offscreen probes look at. See PsCheckBox_CountRenderedTones and
' PsCheckBox_HashRenderedPart for why they exist.
enum
    CHK_PART_CONTROL = 0
    CHK_PART_BOX
    CHK_PART_TEXT
end enum


' Colors for the built-in painter. Copied on Set.
'
' FOUR MOODS, precedence  disabled > pressed > hot > idle.
'
' THE CONTROL READS COMPLETELY FLAT BY DEFAULT because BackColorHot, BackColorPressed and
' BackColorDisabled are all defaulted EQUAL to BackColor -- the same trick PsSelectBar uses for
' its hot cell and PsComboBox for its border. Hovering therefore changes only the box and the
' caption, which is what a checkbox in a settings pane should do. A host that wants a
' menu-style highlight across the whole row sets ONE field and gets it.
'
' THE BOX HAS TWO COLOUR SETS, one per checked state, because the composite glyph is a single
' piece of ink: an unchecked box wants to read as a muted outline and a checked one as an
' accent. There is no separate "tick colour" -- see the note above PSCHECKBOX_GLYPH_UNCHECKED.
'
' THERE IS NO REFERENCE SCREENSHOT for this control. These defaults are INHERITED from
' PSBUTTON_COLORS and PSTOGGLE_COLORS and then accepted by eye, which is a weaker claim than
' "matched to a design" and is worth keeping distinct.
type PSCHECKBOX_COLORS
    ' --- the control's own background, per mood. All four equal by default (see above). ---
    BackColor               as COLORREF = BGR( 33, 37, 43)
    BackColorHot            as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    BackColorPressed        as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    BackColorDisabled       as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    ' --- the caption, per mood ---
    ForeColor               as COLORREF = BGR(200,205,214)
    ForeColorHot            as COLORREF = BGR(226,230,238)
    ForeColorPressed        as COLORREF = BGR(255,255,255)
    ForeColorDisabled       as COLORREF = BGR( 96,102,112)
    ' --- the box glyph, UNCHECKED: a muted outline ---
    BoxColor                as COLORREF = BGR(138,144,153)
    BoxColorHot             as COLORREF = BGR(176,182,191)
    BoxColorPressed         as COLORREF = BGR(200,205,214)
    BoxColorDisabled        as COLORREF = BGR( 84, 89, 97)
    ' --- the box glyph, CHECKED: the accent ---
    BoxColorChecked         as COLORREF = BGR( 86,156,214)
    BoxColorCheckedHot      as COLORREF = BGR(122,180,230)
    BoxColorCheckedPressed  as COLORREF = BGR(150,200,240)
    BoxColorCheckedDisabled as COLORREF = BGR( 56, 70, 84)
    ' --- everything else ---
    FocusRingColor          as COLORREF = BGR( 86,156,214)
end type


' Everything the painter needs. All four rects are precomputed by LayoutCheckBox -- never
' re-derive one from another, and in particular never re-apply the padding to rcContent to get
' rcText, because the box cell, the box gap and the box ALIGNMENT can all change underneath you.
type PSCHECKBOX_PAINTINFO
    hCheckBox   as HWND              ' the control, so the callback can query it
    b           as PsBufferPaint ptr  ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutCheckBox ---
    rcClient    as RECT              ' the whole client area
    rcContent   as RECT              ' rcClient deflated by the focus-ring band
    rcBox       as RECT              ' the box cell. Never empty -- the box always exists
    ' THE CAPTION SPAN, NOT THE INK. rcText is the whole box between the box cell and the far
    ' padding, so DT_END_ELLIPSIS has somewhere to ellipsize into; the caption is placed inside
    ' it by nTextAlign. Do NOT expect it to be a tight box around the glyphs -- that is the
    ' natural mistake here, and drawing a background behind "the text" using this rect fills the
    ' whole span. EMPTY when there is no caption, and DEGENERATE (left = right) when the client
    ' is too narrow to leave any span at all.
    rcText      as RECT
    rcVisual    as RECT              ' rcContent inflated by the focus-ring band
    ' --- State: the control decides these, you only render. ---
    isChecked   as boolean
    isHot       as boolean           ' the mouse is over the control
    isPressed   as boolean           ' a live left press AND the cursor is still inside
                                     '   (slide off and this goes false without ending the
                                     '   gesture)
    isEnabled   as boolean
    isFocused   as boolean           ' draw a focus ring when true
    ' --- What to draw ---
    wszText     as DWSTRING
    ' ALREADY RESOLVED for isChecked, so a callback does not have to repeat the choice and
    ' cannot disagree with the control about which glyph belongs to which state.
    wszGlyph    as DWSTRING
    nBoxAlign   as long              ' CHK_BOXALIGN_*
    nTextAlign  as long              ' CHK_TEXTALIGN_*; map to DT_LEFT / DT_CENTER / DT_RIGHT
end type

type PSCHECKBOX_MESSAGEINFO
    hCheckBox   as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with the resolved mood's BackColor before calling
' you, so a callback that only wants to add something on top does not have to repaint the
' background.
'
' TWO CONTRACTS WORTH HONOURING:
'   - Draw the caption with the SAME font you handed to PsCheckBox_SetFont. The ideal width was
'     measured with it, and a different font means the width lies.
'   - DO NOT reach for PaintBorderRect to draw a frame or a focus ring. It FILLS
'     unconditionally, so used as an outline it erases everything beneath it -- a mistake this
'     family has now made three separate times (PsToggle, PsComboBox, PsNumericUpDown), every time
'     by copying a sibling's callback. PaintRoundOutline is the one that strokes without
'     filling. PsCheckBox_CountRenderedTones exists so you can ASSERT you have not done it.
type CHK_PaintCallbackSub as sub( byval p as PSCHECKBOX_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' Like PsToggle, PsComboBox and PsButton -- and unlike the mouse-only siblings -- this control hands
' over the FOCUS and KEYBOARD messages as well as the mouse ones, because they carry real state.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the control holds mouse capture across a press and the up-message is
'                         what releases it: a callback that suppressed it would strand capture
'                         and route every subsequent click to this control (the PsListBox bug
'                         recorded in Learnings.md). Suppressing WM_LBUTTONDOWN suppresses the
'                         press, never the capture bookkeeping.
'   WM_SETFOCUS         - focus is a FACT the system reports, not an action to veto. The state
'   WM_KILLFOCUS          is already updated by the time you are called; there is nothing left
'                         to suppress. A host that does not want the control focusable must not
'                         make it a tabstop.
' For every other message TRUE suppresses the control's default handling.
type CHK_MessageCallbackFunc as function( byval m as PSCHECKBOX_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text on demand (only when a tip is about to show). Consulted ONLY when the
' control has no tooltip text of its own. Return "" for no tooltip.
'
' THERE IS NO CAPTION FALLBACK. A checkbox whose caption is already on screen would otherwise pop
' a tip that just repeats what you are looking at, so a control with neither its own text nor a
' callback answer simply shows nothing. (PsStatusBar and PsTabBar do fall back to the caption;
' that is their call, made for controls whose text is often truncated.)
type CHK_TooltipCallbackFunc as function( byval hCheckBox as HWND ) as DWSTRING

' The checked state changed through USER interaction -- a completed click, or Space/Enter, or
' the programmatic PsCheckBox_Toggle (which is an ACTION, not a setter; see that function).
' Fired AFTER the control's state is updated, so PsCheckBox_GetChecked() = isChecked.
'
' The programmatic PsCheckBox_SetChecked does NOT fire this (Win32's BM_SETCHECK / BN_CLICKED
' precedent), which is also what makes it safe to call SetChecked from inside this handler.
type CHK_CheckChangedCallbackSub as sub( byval hCheckBox as HWND, byval isChecked as boolean )


type PSCHECKBOX
    hWin              as HWND
    ' The tooltip, whichever backend it is on. Replaces the old hToolTip + HoverTime
    ' pair; PsCheckBox_GetTooltipHandle still answers the comctl32 handle, and 0 while
    ' this instance is on PsTooltip.
    tip         as PSTIPHOST
    ' Instance-lifetime buffer that TTN_GETDISPINFOW's lpszText is pointed at. It must NOT be
    ' the same field as wszTooltip below: that one is the host's authored text, and overwriting
    ' it with a callback's answer would silently promote a transient string into stored state.
    wszTipBuf         as DWSTRING
    idc_CheckBox      as long = 0
    id                as long = 0        ' host command id, reported by CheckChangedCallback
    itemData          as integer = 0     ' free-form host payload
    ' --- Content ---
    wszText           as DWSTRING        ' the caption. "" = no caption (and no gap spent)
    ' The two box glyphs. Seeded by PsCheckBox_Create, NOT by a field initializer here: a DWSTRING
    ' member cannot carry one (it is a UDT with its own constructors), and the family's
    ' Create-does-the-defaults shape is where every other string default lives anyway.
    wszGlyphUnchecked as DWSTRING
    wszGlyphChecked   as DWSTRING
    wszTooltip        as DWSTRING        ' "" = ask TooltipCallback, then show nothing
    ' --- State. All single-valued: there is exactly one of this control, so unlike the
    '     collection siblings there are no indices here to be left dangling, and none of their
    '     index fix-up code exists. ---
    isChecked         as boolean = false
    isEnabled         as boolean = true
    isHot             as boolean = false  ' the mouse is over the client
    isFocused         as boolean = false
    ' Set when a WM_KEYDOWN we consume will be followed by a WM_CHAR, so that character can be
    ' swallowed too. Without it the character reaches DefWindowProc and the system BEEPS on
    ' every Enter/Space -- the keystroke still works, so the beep is the only symptom.
    bAteKeyDown   as boolean = false
    isPressed         as boolean = false  ' a live left press (see the capture note below)
    hotTimerOn        as boolean = false  ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press: the press/cancel
    '     gesture (press, slide off the control, release, the state does NOT flip) consumes the
    '     guaranteed down->up pairing, which is the family's test for taking capture. It pays
    '     the full price: snapshot the press BEFORE releasing (ReleaseCapture sends
    '     WM_CAPTURECHANGED synchronously -- Learnings.md), release on the up-message before any
    '     callback runs, WM_CAPTURECHANGED cancels the press, WM_DESTROY releases, and no
    '     callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still inside" is exactly isHot, and
    '     hover tracking already maintains that through the capture. ---
    colors            as PSCHECKBOX_COLORS
    ' Caller-supplied fonts (caller owns both). NOT named hFont: FreeBASIC is case-insensitive,
    ' so a member called hFont would shadow the TYPE name HFONT inside every member procedure of
    ' this type, and "dim as HFONT x" there fails with a misleading "error 14: Expected
    ' identifier, found 'HFONT'". See C:\dev\Learnings.md.
    '
    ' TWO fonts, because the caption font is never the icon font. hTextFont is the MEASURING
    ' font and drives the layout; hGlyphFont is a paint-time input only (PsIconPanel's rule) and
    ' changing it repaints without re-laying-out.
    hTextFont         as HFONT
    hGlyphFont        as HFONT
    ' --- Layout inputs. All pixels; DPI-scaled once at Create, raw thereafter. ---
    nBoxAlign         as long = CHK_BOXALIGN_LEFT
    nTextAlign        as long = CHK_TEXTALIGN_LEFT
    bAutoSize         as boolean = false   ' opt-in: the control SetWindowPos's ITSELF
    nPadLeft          as long = PSCHECKBOX_DEFAULT_PADLEFT
    nPadTop           as long = PSCHECKBOX_DEFAULT_PADTOP
    nPadRight         as long = PSCHECKBOX_DEFAULT_PADRIGHT
    nPadBottom        as long = PSCHECKBOX_DEFAULT_PADBOTTOM
    nBoxGap           as long = PSCHECKBOX_DEFAULT_BOXGAP
    ' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
    ' that never name the real problem -- hence nBoxWidth throughout.
    nBoxWidth         as long = PSCHECKBOX_DEFAULT_BOXWIDTH
    nBoxHeight        as long = PSCHECKBOX_DEFAULT_BOXHEIGHT
    nFocusGap         as long = PSCHECKBOX_DEFAULT_FOCUSGAP
    nFocusCurvature   as long = PSCHECKBOX_DEFAULT_FOCUSCURV   ' ellipse DIAMETER
    nFocusThick       as long = PSCHECKBOX_DEFAULT_FOCUSTHICK  ' NOT DPI-scaled
    ' --- Layout outputs. Rects are DERIVED, never set from outside; LayoutCheckBox() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect/size query) runs
    '     it, which coalesces a burst of mutations into ONE measuring pass. ---
    nTextWidth        as long = 0     ' derived: the measured caption width
    nTextHeight       as long = 0     ' derived: one height for the control, from tmHeight
    nIdealW           as long = 0     ' derived
    nIdealH           as long = 0     ' derived
    rcContent         as RECT         ' derived
    rcBox             as RECT         ' derived
    rcText            as RECT         ' derived -- the SPAN, see PSCHECKBOX_PAINTINFO
    rcVisual          as RECT         ' derived
    bLayoutDirty      as boolean = true
    PaintCallback        as CHK_PaintCallbackSub          ' optional; replaces the built-in painter
    MessageCallback      as CHK_MessageCallbackFunc       ' optional
    TooltipCallback      as CHK_TooltipCallbackFunc       ' optional
    CheckChangedCallback as CHK_CheckChangedCallbackSub   ' optional; user-driven changes only

    declare sub      LayoutCheckBox()
    declare function RingPad() as long
    declare function HasText() as boolean
    declare function ResolveGlyph() as DWSTRING
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' The distance the visual bounds extend beyond the content on every side. The focus ring is
' drawn INSIDE this band, and the band is reserved UNCONDITIONALLY -- whether or not the control
' currently has focus -- which is what stops the content jumping when focus arrives. It
' therefore contributes to the IDEAL SIZE (PsToggle's rule).
function PSCHECKBOX.RingPad() as long
    return this.nFocusGap + this.nFocusThick
end function

' "Is there a caption at all". A named member rather than an inline len() check because the
' layout, the painter and the ideal-size arithmetic must all agree about it, and three copies of
' `len(x) > 0` is three places to disagree.
function PSCHECKBOX.HasText() as boolean
    return (PsLen( this.wszText ) > 0)
end function

' Which glyph does the current state call for? ONE place, so the painter, the PAINTINFO handed
' to a paint callback and the self-test's render probe cannot pick differently.
function PSCHECKBOX.ResolveGlyph() as DWSTRING
    dim as DWSTRING wszOut
    if this.isChecked then
        wszOut = this.wszGlyphChecked
    else
        wszOut = this.wszGlyphUnchecked
    end if
    return wszOut
end function


' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's job,
' and only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it.
sub PSCHECKBOX.CancelPress()
    this.isPressed = false
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is what
' makes layout lazy: a burst of setters costs one measuring pass, not N.
sub PSCHECKBOX.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking control is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' Measure the caption and assign the four rects. This is the only place checkbox geometry is
' produced; painting, invalidation and every public size query consume it.
'
'   ringPad    = nFocusGap + nFocusThick        reserved ALWAYS, focused or not
'   textW      = the measured caption width, 0 when there is no caption
'   textH      = GetTextMetrics.tmHeight        ONE height for the control
'   textBlock  = hasText ? nBoxGap + textW : 0
'   contentH   = max( hasText ? textH : 0 , nBoxHeight )
'
'   nIdealW    = 2*ringPad + nPadLeft + nBoxWidth + textBlock + nPadRight
'   nIdealH    = 2*ringPad + nPadTop  + contentH + nPadBottom
'
'   rcContent  = rcClient deflated by ringPad on all four sides
'   BOXALIGN_LEFT   rcBox  = at rcContent.left + nPadLeft,  nBoxWidth x nBoxHeight, v-centred
'                   rcText = ( rcBox.right + nBoxGap , rcContent.right - nPadRight )
'   BOXALIGN_RIGHT  rcBox  = at rcContent.right - nPadRight - nBoxWidth, same, v-centred
'                   rcText = ( rcContent.left + nPadLeft , rcBox.left - nBoxGap )
'   rcVisual   = rcContent inflated by ringPad  ( = rcClient when the host sized us ideally )
'
' NOTE nIdealW IS THE SAME IN BOTH ALIGNMENTS. The mirror is exact -- the box cell, the gap and
' the two paddings are all still charged, they have merely swapped ends -- which is what lets a
' host flip the alignment on a laid-out form without re-measuring anything. Asserted, not
' assumed.
'
' THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION TO SEPARATE FROM. A caption-less checkbox charges
' padLeft + box + padRight, so the glyph sits properly centred in its own padding instead of
' being pushed off-centre by a gap with nothing on the other side of it. Same reasoning as
' PsButton's icon-only button and PsComboBox's arrow-only mode.
'
' AN ABSENT CAPTION GIVES AN EMPTY rcText, not a zero-width one tucked against an edge. A paint
' callback can therefore test IsRectEmpty rather than having to re-derive presence from the
' string it was handed. rcBox is NEVER empty: the box is what makes this a checkbox.
'
' CONTENT HEIGHT IGNORES THE FONT WHEN THERE IS NO CAPTION, which means a caption-less checkbox
' in a column of captioned ones has a SHORTER ideal height. That is the honest answer -- nothing
' in it is as tall as a line of text -- and a host laying out a column takes the max itself
' rather than having a phantom text height baked in here.
'
' WHY THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area at all, and PsCheckBox_GetIdealSize is exactly what a host calls to decide how big
' to make the control in the FIRST place. Returning 0 until it had already been sized would be a
' chicken-and-egg trap (PsButton and PsComboBox dodge it by this same ordering).
'
' OVERFLOW: when the client is smaller than ideal the rects are computed HONESTLY rather than
' squeezed (PsIconPanel's and PsTabBar's rule) -- the box keeps its declared size and stays pinned
' to its padding, and rcText is what collapses, clamped to EMPTY rather than going negative. So
' a too-narrow checkbox loses its caption to the ellipsis and keeps its box, which is the useful
' failure. Vertically a client shorter than contentH clips rather than deforming.
sub PSCHECKBOX.LayoutCheckBox()
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

    ' Resolved ONCE for the whole pass rather than re-tested at each of the sites below: two
    ' separate len() tests would be correct today but invite a future edit that changes the
    ' caption mid-layout and produces a control measured one way and placed another.
    dim as boolean bText = this.HasText()

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

    dim as long textBlock = 0
    if bText then textBlock = this.nBoxGap + this.nTextWidth

    dim as long contentH = this.nBoxHeight
    if bText then
        if this.nTextHeight > contentH then contentH = this.nTextHeight
    end if

    this.nIdealW = (2 * nRingPad) + this.nPadLeft + this.nBoxWidth + textBlock + this.nPadRight
    this.nIdealH = (2 * nRingPad) + this.nPadTop + contentH + this.nPadBottom

    ' ---- placement, which does ------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is impossible,
    ' so leave the rects alone and ask to be run again -- but note this bail comes AFTER the
    ' ideal size is computed. See the header.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcContent = rcClient
    InflateRect( @this.rcContent, -nRingPad, -nRingPad )
    dim as long contH = this.rcContent.bottom - this.rcContent.top

    ' The box, pinned to its padding on whichever end it was given, always v-centred.
    dim as long boxY = this.rcContent.top + ((contH - this.nBoxHeight) \ 2)
    dim as long boxX
    if this.nBoxAlign = CHK_BOXALIGN_RIGHT then
        boxX = this.rcContent.right - this.nPadRight - this.nBoxWidth
    else
        boxX = this.rcContent.left + this.nPadLeft
    end if
    SetRect( @this.rcBox, boxX, boxY, boxX + this.nBoxWidth, boxY + this.nBoxHeight )

    if bText then
        dim as long tLeft, tRight
        if this.nBoxAlign = CHK_BOXALIGN_RIGHT then
            tLeft  = this.rcContent.left + this.nPadLeft
            tRight = this.rcBox.left - this.nBoxGap
        else
            tLeft  = this.rcBox.right + this.nBoxGap
            tRight = this.rcContent.right - this.nPadRight
        end if
        ' Degenerate rather than negative when the client is too narrow (see OVERFLOW above).
        if tRight < tLeft then tRight = tLeft
        dim as long tTop = this.rcContent.top + ((contH - this.nTextHeight) \ 2)
        SetRect( @this.rcText, tLeft, tTop, tRight, tTop + this.nTextHeight )
    else
        SetRectEmpty( @this.rcText )
    end if

    this.rcVisual = this.rcContent
    InflateRect( @this.rcVisual, nRingPad, nRingPad )
end sub


' ========================================================================================
' Which colour set does this state call for?  disabled > pressed > hot > idle.
'
' A PURE FUNCTION on purpose: it takes no control pointer and touches no global, so its whole
' truth table can be asserted directly (PsScrollPanel_ShouldShow's precedent) instead of only
' being observable from inside a WM_PAINT. The painter and the self-test call the same one.
' ========================================================================================
function PsCheckBox_ResolveMood( _
            byval isEnabled as boolean, _
            byval isPressed as boolean, _
            byval isHot     as boolean _
            ) as long

    if isEnabled = false then return CHK_MOOD_DISABLED
    if isPressed then return CHK_MOOD_PRESSED
    if isHot then return CHK_MOOD_HOT
    return CHK_MOOD_IDLE
end function


' ========================================================================================
' Turn a mood plus the checked flag into the three colours the painter uses.
'
' Split out of the painter for the same reason as PsCheckBox_ResolveMood: "isChecked changes the
' BOX colour and nothing else" has to hold in EVERY mood, and that is a claim worth asserting
' rather than eyeballing in eight screenshots. The checked test is applied INSIDE each mood arm
' rather than as a fifth mood, which is what keeps it one rule rather than four.
' ========================================================================================
sub PsCheckBox_ResolveColors( _
            byval pColors   as PSCHECKBOX_COLORS ptr, _
            byval nMood     as long, _
            byval isChecked as boolean, _
            byref clrBack   as COLORREF, _
            byref clrFore   as COLORREF, _
            byref clrBox    as COLORREF _
            )

    if pColors = 0 then exit sub

    with *pColors
        select case nMood
        case CHK_MOOD_DISABLED
            clrBack = .BackColorDisabled
            clrFore = .ForeColorDisabled
            if isChecked then
                clrBox = .BoxColorCheckedDisabled
            else
                clrBox = .BoxColorDisabled
            end if
        case CHK_MOOD_PRESSED
            clrBack = .BackColorPressed
            clrFore = .ForeColorPressed
            if isChecked then
                clrBox = .BoxColorCheckedPressed
            else
                clrBox = .BoxColorPressed
            end if
        case CHK_MOOD_HOT
            clrBack = .BackColorHot
            clrFore = .ForeColorHot
            if isChecked then
                clrBox = .BoxColorCheckedHot
            else
                clrBox = .BoxColorHot
            end if
        case else
            clrBack = .BackColor
            clrFore = .ForeColor
            if isChecked then
                clrBox = .BoxColorChecked
            else
                clrBox = .BoxColor
            end if
        end select
    end with
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A CHECKBOX: a Fluent box glyph and an optional caption, on either side of each other, with a
' focus ring around the pair. Two states, no chrome, no animation.
'
' THE BOX IS ONE COMPOSITE GLYPH PER STATE
'   E739 (empty square) and E73A (square with a tick), both settable. That means the box outline
'   and the tick are ONE piece of ink in ONE colour -- an accent-filled box with a contrasting
'   white tick is not reachable, and getting it would mean drawing the box as geometry and the
'   tick as a separate glyph over it. That was decided deliberately: it keeps the whole
'   appearance font-driven and swappable, and it keeps the colour struct to two box colours
'   instead of six. Don't "fix" it without reopening the decision.
'
' TWO STATES ONLY -- THERE IS NO INDETERMINATE
'   isChecked is a boolean throughout, including in CHK_CheckChangedCallbackSub. Adding a third
'   state later would change that callback's signature and break every host, so it is a decision
'   rather than a gap. A host that needs tri-state today can fake it with SetGlyphChecked and a
'   custom paint callback, but the CONTROL will still only track two.
'
' NO MNEMONICS
'   "&Enable" draws a literal ampersand -- PsBufferPaint.PaintText forces DT_NOPREFIX, so this is
'   enforced by the renderer rather than merely intended, and PaintTextEx forces it too. A host
'   wanting Alt+E wires its own accelerator and calls PsCheckBox_Toggle().
'
' NO MULTILINE
'   One line, ellipsized into the caption span. Wrapping would make the ideal HEIGHT depend on
'   the assigned WIDTH, which would cost GetIdealSize its "valid before the control is sized"
'   guarantee -- the property the whole family's layout leans on.
'
' IT IS FOCUSABLE
'   WS_TABSTOP, real focus tracking, a painted focus ring, and Space/Enter activation. PsToggle,
'   PsComboBox and PsButton are the other focusable siblings; the rest of the family is mouse-only.
'   Tab NAVIGATION needs IsDialogMessage in the host pump -- and the host must SetFocus its first
'   control, because IsDialogMessage ignores Tab until a child already has focus. Without either,
'   mouse and (once the control has focus) keyboard both still work.
'
'   ENTER ACTIVATES, WHICH DEPARTS FROM REAL WIN32. A system checkbox toggles on Space only and
'   lets Enter fall through to the dialog's default button. This control takes both, matching
'   PsToggle and PsButton, so that the family behaves one way rather than three. It is a deliberate
'   choice, not drift: a host that wants Enter to reach its OK button should not put focus on a
'   checkbox, or should intercept WM_KEYDOWN through the message callback.
'
' THERE IS NO PUMP OBLIGATION
'   Unlike PsComboBox, PsNumericUpDown and PsTextBox there is no PsCheckBox_FilterMessage: this
'   control owns no second top-level window and no popup, so nothing needs intercepting ahead of
'   the dispatch.
'
' THE CONTROL HANDLE
'   Every PsCheckBox_* function takes the handle returned by PsCheckBox_Create(). It is a real HWND
'   on purpose (not an opaque type): callers legitimately need to treat the control as a window,
'   e.g. SetWindowPos() to place and size it.
'
' THERE IS NO PsCheckBox_HitTest
'   The whole client rect is the hit area -- clicking the caption toggles, exactly as a system
'   checkbox does -- so a hit test could only ever be PtInRect(client), a function that returns
'   TRUE for exactly the points the caller already knows are inside. Its absence is a decision,
'   not an omission (PsToggle's and PsButton's reasoning).
'
' LIFETIME
'   The control frees itself when its window is destroyed, and destroys its tooltip with it. The
'   two fonts you pass in stay yours.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID), so hosts can find it with GetDlgItem.
'   There are no child controls. The control is created zero-sized: size it with
'   PsCheckBox_GetIdealSize and position it with SetWindowPos() -- or turn on
'   PsCheckBox_SetAutoSize and let it size itself.
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Content.  All of these are SILENT: they change what is drawn, never what was clicked.
'
'   SetText is also reachable as the WINDOW TEXT -- the control handles WM_SETTEXT, WM_GETTEXT
'     and WM_GETTEXTLENGTH against this same store, so SetWindowText / GetWindowText work and
'     generic code that walks children and reads their text sees a real caption. One store, two
'     doors. (Only PsButton does this too; it is here because a checkbox meets generic dialog
'     code just as often.)
'   SetGlyphUnchecked / SetGlyphChecked take the codepoint(s) to draw for each state. Passing ""
'     draws nothing for that state, which is legal but leaves the control looking broken -- the
'     box cell and its gap are still charged, because the cell is DECLARED, not measured.
'   SetID sets the value stored for the host's convenience. It is a HOST PAYLOAD and is never
'     used as a command id by this control, so it cannot collide with anything.
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_GetText( byval hCheckBox as HWND ) as DWSTRING
declare sub      PsCheckBox_SetText( byval hCheckBox as HWND, byval Text as DWSTRING )
declare function PsCheckBox_GetGlyphUnchecked( byval hCheckBox as HWND ) as DWSTRING
declare sub      PsCheckBox_SetGlyphUnchecked( byval hCheckBox as HWND, byval Glyph as DWSTRING )
declare function PsCheckBox_GetGlyphChecked( byval hCheckBox as HWND ) as DWSTRING
declare sub      PsCheckBox_SetGlyphChecked( byval hCheckBox as HWND, byval Glyph as DWSTRING )
declare function PsCheckBox_GetID( byval hCheckBox as HWND ) as long
declare sub      PsCheckBox_SetID( byval hCheckBox as HWND, byval id as long )
declare function PsCheckBox_GetItemData( byval hCheckBox as HWND ) as integer
declare sub      PsCheckBox_SetItemData( byval hCheckBox as HWND, byval itemData as integer )

' ----------------------------------------------------------------------------------------
' State.
'
'   SetChecked is SILENT -- the change callback fires only for user interaction (a completed
'     click, or Space/Enter) and for PsCheckBox_Toggle below, following Win32's BM_SETCHECK /
'     BN_CLICKED precedent. That is also what stops a host calling SetChecked from inside its own
'     handler from recursing. It never re-measures: the two glyphs share one declared cell.
'
'   SetEnabled(false) goes through EnableWindow, so the disable is enforced by the system rather
'     than being a cosmetic flag: a disabled window gets no mouse input and the dialog manager's
'     Tab skips it. It clears any live press and hover at once rather than waiting for the next
'     mouse move. It does NOT change the checked state -- a disabled ticked box still reads as
'     ticked, which is why the colour struct carries a disabled colour for each state.
'
'   GetFocused answers for this window directly (there is no child to hold focus, unlike
'     PsNumericUpDown where GetFocus() = hCtrl is never true).
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_GetChecked( byval hCheckBox as HWND ) as boolean
declare sub      PsCheckBox_SetChecked( byval hCheckBox as HWND, byval isChecked as boolean )
declare function PsCheckBox_GetEnabled( byval hCheckBox as HWND ) as boolean
declare sub      PsCheckBox_SetEnabled( byval hCheckBox as HWND, byval isEnabled as boolean )
declare function PsCheckBox_GetFocused( byval hCheckBox as HWND ) as boolean
declare sub      PsCheckBox_Refresh( byval hCheckBox as HWND )

' ----------------------------------------------------------------------------------------
' Action.
'
'   PsCheckBox_Toggle FLIPS the state and FIRES CheckChangedCallback, and that is a DELIBERATE
'   EXCEPTION to the family's "programmatic setters are silent" rule. It is an ACTION, not a
'   state setter -- Win32's BM_CLICK sends BN_CLICKED too -- and with no mnemonics parsed by this
'   control it is the door a host's accelerator uses. A silent version already exists, and is
'   called PsCheckBox_SetChecked.
'
'   The call is refused on a disabled control, exactly as a real click would be. Note it does NOT
'   paint a press flash: it reports an action, it does not animate one.
' ----------------------------------------------------------------------------------------
declare sub      PsCheckBox_Toggle( byval hCheckBox as HWND )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). nFocusThick should not be scaled at all: a hairline
' stays a hairline.
'
'   SetBoxAlign puts the box on the left or the right. The cell is PINNED to the padding on that
'     side and the caption takes the span -- see the CHK_BOXALIGN_* enum, which is where the two
'     looks this gives you are drawn out. It does NOT change the ideal size.
'   SetTextAlign places the caption in its SPAN, not in the control -- see CHK_TEXTALIGN_*.
'   SetBoxGap is the space between the box cell and the caption. It is charged only when there IS
'     a caption.
'   SetBoxSize is the declared cell for the glyph. Nothing is measured, so this is the only thing
'     that decides how much room the glyph gets.
'   SetFocusRing sets the gap from the content to the ring and the ring's own thickness. Both are
'     reserved unconditionally, focused or not, so nothing jumps when focus arrives -- which
'     means changing either changes the IDEAL SIZE.
'   SetFocusCurvature takes an ellipse DIAMETER, not a radius: PsBufferPaint keeps GDI's
'     vocabulary and halves it internally. 8 draws a 4px radius. 0 = square corners.
'   SetAutoSize(true) makes the control SetWindowPos ITSELF (preserving its top-left) whenever
'     something that changes the ideal size changes: caption, font, padding, box gap, box size,
'     focus ring. Default FALSE, which is the strict family rule -- the host measures with
'     GetIdealSize and sizes.
'
'   GetIdealSize forces a pending layout and returns the measured size. It is valid BEFORE the
'   control has ever been sized (the measuring pass runs ahead of the zero-client bail), which is
'   the whole point -- a host calls it to decide how big to make the control.
'
'   The rect queries force a pending layout, so results are always current. They return FALSE if
'   the control has no geometry yet (created but never sized). rcText is EMPTY when there is no
'   caption and the query still returns TRUE -- empty is the honest answer.
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_GetBoxAlign( byval hCheckBox as HWND ) as long
declare sub      PsCheckBox_SetBoxAlign( byval hCheckBox as HWND, byval nBoxAlign as long )
declare function PsCheckBox_GetTextAlign( byval hCheckBox as HWND ) as long
declare sub      PsCheckBox_SetTextAlign( byval hCheckBox as HWND, byval nTextAlign as long )
declare sub      PsCheckBox_GetPadding( byval hCheckBox as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
declare sub      PsCheckBox_SetPadding( byval hCheckBox as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
declare function PsCheckBox_GetBoxGap( byval hCheckBox as HWND ) as long
declare sub      PsCheckBox_SetBoxGap( byval hCheckBox as HWND, byval nBoxGap as long )
declare sub      PsCheckBox_GetBoxSize( byval hCheckBox as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
declare sub      PsCheckBox_SetBoxSize( byval hCheckBox as HWND, byval nBoxWidth as long, byval nBoxHeight as long )
declare sub      PsCheckBox_GetFocusRing( byval hCheckBox as HWND, byref nGap as long, byref nThickness as long )
declare sub      PsCheckBox_SetFocusRing( byval hCheckBox as HWND, byval nGap as long, byval nThickness as long )
declare function PsCheckBox_GetFocusCurvature( byval hCheckBox as HWND ) as long
declare sub      PsCheckBox_SetFocusCurvature( byval hCheckBox as HWND, byval nCurvature as long )
declare function PsCheckBox_GetAutoSize( byval hCheckBox as HWND ) as boolean
declare sub      PsCheckBox_SetAutoSize( byval hCheckBox as HWND, byval bAutoSize as boolean )
declare sub      PsCheckBox_GetIdealSize( byval hCheckBox as HWND, byref nWidth as long, byref nHeight as long )
declare function PsCheckBox_GetContentRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
declare function PsCheckBox_GetBoxRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
declare function PsCheckBox_GetTextRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
declare function PsCheckBox_GetVisualRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct. GetColors fills one out, so the
' read-modify-write "change one color" idiom is Get, assign, Set.
'
' BOTH fonts are BORROWED, never owned: keep them alive and destroy them yourself.
'   SetFont      - the CAPTION font, and the MEASURING font. Changing it re-measures.
'   SetGlyphFont - the BOX font (normally Segoe Fluent Icons). A paint-time input only: changing
'                  it repaints but never re-lays-out, because the box cell is declared rather
'                  than measured. Unset falls back to the caption font -- which will draw the
'                  codepoint as a missing-glyph box, so this one is worth setting.
' ----------------------------------------------------------------------------------------
declare sub      PsCheckBox_GetColors( byval hCheckBox as HWND, byval pColors as PSCHECKBOX_COLORS ptr )
declare sub      PsCheckBox_SetColors( byval hCheckBox as HWND, byval pColors as PSCHECKBOX_COLORS ptr )
declare function PsCheckBox_GetFont( byval hCheckBox as HWND ) as HFONT
declare sub      PsCheckBox_SetFont( byval hCheckBox as HWND, byval hTextFont as HFONT )
declare function PsCheckBox_GetGlyphFont( byval hCheckBox as HWND ) as HFONT
declare sub      PsCheckBox_SetGlyphFont( byval hCheckBox as HWND, byval hGlyphFont as HFONT )

' ----------------------------------------------------------------------------------------
' Tooltips.  The control's own text wins; the callback is consulted only when it is empty; with
' neither, no tip is shown -- there is no caption fallback (see CHK_TooltipCallbackFunc).
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_GetTooltipText( byval hCheckBox as HWND ) as DWSTRING
declare sub      PsCheckBox_SetTooltipText( byval hCheckBox as HWND, byval Text as DWSTRING )
declare function PsCheckBox_GetTooltipHandle( byval hCheckBox as HWND ) as HWND
declare sub      PsCheckBox_SetHoverTime( byval hCheckBox as HWND, byval milliseconds as long )
declare function PsCheckBox_SetTooltipMode( byval hCheckBox as HWND, byval nMode as long ) as boolean
declare function PsCheckBox_GetTooltipMode( byval hCheckBox as HWND ) as long
' The PsTooltip window, or 0 on the system backend. The door to PsTooltip's own
' SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph -- deliberately not mirrored
' here, since thirteen controls x twenty setters is 260 wrappers to keep in step.
declare function PsCheckBox_GetPsTooltipHandle( byval hCheckBox as HWND ) as HWND
' Honoured by BOTH backends. A delay never set keeps the backend's own derivation from
' the system double-click time.
declare sub      PsCheckBox_SetAutoPopTime( byval hCheckBox as HWND, byval milliseconds as long )
declare sub      PsCheckBox_SetReshowTime( byval hCheckBox as HWND, byval milliseconds as long )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback        - draw the whole control INSTEAD OF the built-in painter.
'   MessageCallback      - observe mouse, focus and key messages; TRUE suppresses the control's
'                          default handling (IGNORED for WM_LBUTTONUP and the two focus messages).
'   TooltipCallback      - supply tooltip text on demand when the control has none of its own.
'   CheckChangedCallback - the USER flipped the box (or the host called PsCheckBox_Toggle).
'                          Programmatic SetChecked is silent.
' ----------------------------------------------------------------------------------------
declare sub      PsCheckBox_SetPaintCallback( byval hCheckBox as HWND, byval usersub as CHK_PaintCallbackSub )
declare sub      PsCheckBox_SetMessageCallback( byval hCheckBox as HWND, byval userfunc as CHK_MessageCallbackFunc )
declare sub      PsCheckBox_SetTooltipCallback( byval hCheckBox as HWND, byval userfunc as CHK_TooltipCallbackFunc )
declare sub      PsCheckBox_SetCheckChangedCallback( byval hCheckBox as HWND, byval usersub as CHK_CheckChangedCallbackSub )

' ----------------------------------------------------------------------------------------
' Plumbing.  Two offscreen probes and the self-test. Nothing in the control calls the probes;
' they exist to make two specific defects MEASURABLE, because both of them produce a control
' that compiles, runs, and draws a plausible result.
'
'   CountRenderedTones - render the control OFFSCREEN with its current painter and return how
'                 many distinct colours land inside one of its rects (capped at 64).
'
'                 THE DEFECT: PsBufferPaint.PaintBorderRect FILLS before it strokes, so a paint
'                 callback that reaches for it to draw a frame or a focus ring floods everything
'                 beneath and the control renders as one solid block. That has shipped three
'                 times in this family (PsToggle, PsComboBox, PsNumericUpDown), every time from a
'                 copied callback, and it survives a glance because the flood colour is a real
'                 colour from the control. A wiped part is literally ONE tone.
'
'                 A HOST THAT WRITES ITS OWN PaintCallback SHOULD ASSERT THIS. That is the whole
'                 reason it is public rather than private to the self-test.
'
'   HashRenderedPart - the same offscreen render, returning a 32-bit FNV-1a hash of the pixels in
'                 one rect instead of a tone count.
'
'                 THE DEFECT IT CATCHES: a state change that never reaches the surface. Every
'                 numeric assertion about a checkbox can pass while the box draws the same glyph
'                 in both states -- the stored flag flips, the layout is right, the colours
'                 resolve correctly, and the pixels never move. PsNumericUpDown shipped exactly
'                 this class of bug (a SendMessage whose failure was never checked), so here the
'                 claim "checking the box changes what is drawn" is asserted against the pixels:
'                 hash(rcBox, unchecked) <> hash(rcBox, checked).
'
'                 Being a hash it can only ever prove DIFFERENCE, never correctness -- it says
'                 the render moved, not that it moved to the right thing. That is what the
'                 interactive pass is for.
'
'                 AND IT ONLY MEASURES WHAT YOU ISOLATE. The self-test's first version of this
'                 asserted hash(unchecked) <> hash(checked) and called it "the glyph swap reached
'                 the surface" -- then survived an A/B that froze glyph resolution outright,
'                 because isChecked ALSO switches the box colour and that alone moves the pixels.
'                 The assertion was true; its name was not. The self-test now runs it twice, the
'                 second time with all eight box colours flattened to one value so that nothing
'                 but the glyph can account for a difference. A host comparing two hashes should
'                 ask the same question: what else changed between these two renders?
'
'   Both probes force isFocused TRUE (restored afterwards) so the focus-ring path -- the one the
'   flood defect lives on -- is actually exercised, and both build their PAINTINFO with the same
'   routine WM_PAINT uses, because a probe that hand-builds that struct and leaves one field
'   zeroed silently skips the thing it is measuring; that produced a near-miss false PASS the
'   first time this was written elsewhere in the family (Learnings.md). Both return 0 if the
'   control has no geometry yet.
'
'   RunSelfTest - geometry, colour-resolution, notify-contract, window-text, Tab-reachability and
'                 render assertions, gated on the PSCHECKBOX_SELFTEST env var. Call it from the
'                 host before the message loop.
' ----------------------------------------------------------------------------------------
declare function PsCheckBox_CountRenderedTones( byval hCheckBox as HWND, byval nPart as long ) as long
declare function PsCheckBox_HashRenderedPart( byval hCheckBox as HWND, byval nPart as long ) as ulong
declare sub      PsCheckBox_RunSelfTest( byval hWndParent as HWND )
