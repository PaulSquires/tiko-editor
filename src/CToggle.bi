''
''  CToggle.bi  --  owner-drawn toggle switch (pill + knob)
''

#pragma once

#include once "CBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
#define IDT_CTOGGLE_HOTTRACK   &hCB70
#define CTOGGLE_HOTTRACK_MS    100

' (CTOGGLE_SUPERSAMPLE is gone. The pill and knob used to be rendered at 4x into an
'  offscreen tile and scaled back down with a HALFTONE StretchBlt to fake antialiasing,
'  because plain GDI RoundRect/Ellipse edges are visibly jagged at this size.
'  CBufferPaint renders through GDI+ now and antialiases the curves directly, so the
'  tile, the constant and the traps that came with them have all been deleted.)

' Defaults. The four SIZE values are DPI-scaled at Create; every setter afterwards takes
' raw pixels and the caller scales (the family rule). The two THICKNESS values are NOT
' DPI-scaled, ever -- a hairline should stay a hairline (CMenuBar's nSeparatorThickness).
#define CTOGGLE_DEFAULT_TRACKW      40
#define CTOGGLE_DEFAULT_TRACKH      20
#define CTOGGLE_DEFAULT_KNOBINSET    2
#define CTOGGLE_DEFAULT_BORDERTHICK  1     ' not DPI-scaled
#define CTOGGLE_DEFAULT_FOCUSGAP     3
#define CTOGGLE_DEFAULT_FOCUSTHICK   1     ' not DPI-scaled

' Where the pill sits horizontally when the control is wider than the pill needs. It is
' ALWAYS vertically centred -- there is no vertical justification, because a toggle in a
' settings row is always centred on its row and nothing wanted otherwise.
enum
    TOG_JUSTIFY_LEFT = 0
    TOG_JUSTIFY_CENTER
    TOG_JUSTIFY_RIGHT
end enum


' Colors for the built-in painter. Copied on Set.
'
' Three drawn parts (track fill, track border, knob) x two states (checked / unchecked) x
' three moods (idle / hot / disabled), plus the control's own background and the focus ring.
' Flat COLORREF fields with defaults, exactly like CSELECTBAR_COLORS -- read-modify-write is
' CToggle_GetColors, assign, CToggle_SetColors.
'
' THE ON PILL LOOKS BORDERLESS BY DEFAULT because TrackBorderColorOn* is defaulted EQUAL to
' the matching TrackColorOn*. The OFF pill is distinguished by an outline instead: its fill
' sits close to the background and TrackBorderColorOff is markedly lighter. That asymmetry
' is the look this control was drawn for; a host that wants a visible ON border just sets
' the field. (Same trick CSelectBar uses defaulting BackColorHot to BackColor.)
'
' THERE ARE NO PRESSED COLORS. A live press renders as hot, which is CSelectBar's rule and
' reads correctly for a control whose whole job is to flip on release. isPressed is still
' handed to the paint callback, so a host that wants a distinct pressed look can draw one.
type CTOGGLE_COLORS
    BackColor                   as COLORREF = BGR( 33, 37, 43)   ' the control's own background
    FocusRingColor              as COLORREF = BGR( 86,156,214)
    ' --- checked (ON) ---
    TrackColorOn                as COLORREF = BGR( 74,108,140)
    TrackColorOnHot             as COLORREF = BGR( 92,132,170)
    TrackColorOnDisabled        as COLORREF = BGR( 56, 70, 84)
    TrackBorderColorOn          as COLORREF = BGR( 74,108,140)   ' = TrackColorOn (see above)
    TrackBorderColorOnHot       as COLORREF = BGR( 92,132,170)   ' = TrackColorOnHot
    TrackBorderColorOnDisabled  as COLORREF = BGR( 56, 70, 84)   ' = TrackColorOnDisabled
    KnobColorOn                 as COLORREF = BGR(216,220,226)
    KnobColorOnHot              as COLORREF = BGR(255,255,255)
    KnobColorOnDisabled         as COLORREF = BGR(120,126,134)
    ' --- unchecked (OFF) ---
    TrackColorOff               as COLORREF = BGR( 51, 55, 63)
    TrackColorOffHot            as COLORREF = BGR( 62, 67, 77)
    TrackColorOffDisabled       as COLORREF = BGR( 44, 48, 55)
    TrackBorderColorOff         as COLORREF = BGR(107,114,128)
    TrackBorderColorOffHot      as COLORREF = BGR(138,146,161)
    TrackBorderColorOffDisabled as COLORREF = BGR( 72, 77, 86)
    KnobColorOff                as COLORREF = BGR(138,144,153)
    KnobColorOffHot             as COLORREF = BGR(176,182,191)
    KnobColorOffDisabled        as COLORREF = BGR( 84, 89, 97)
end type


' Everything the painter needs. All three rects are precomputed by LayoutToggle -- never
' re-derive one from another, and in particular never derive the knob from the track by
' repeating the inset arithmetic, because SetKnobInset can change it underneath you.
type CTOGGLE_PAINTINFO
    hToggle    as HWND                 ' the control, so the callback can query it
    b          as CBufferPaint ptr  ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutToggle ---
    rcClient   as RECT                 ' the whole client area
    rcTrack    as RECT                 ' the pill
    rcKnob     as RECT                 ' the circle, already at the end its state calls for
    rcVisual   as RECT                 ' rcTrack inflated by the focus-ring padding
    ' --- State: the control decides these, you only render. ---
    isChecked  as boolean              ' ON: the knob is at the RIGHT end
    isHot      as boolean              ' the mouse is over the control
    isPressed  as boolean              ' a live left press AND the cursor is still inside
                                       '   (slide off and this goes false without ending
                                       '   the gesture)
    isEnabled  as boolean
    isFocused  as boolean              ' draw a focus ring when true
end type

type CTOGGLE_MESSAGEINFO
    hToggle    as HWND
    uMsg       as UINT
    wParam     as WPARAM
    lParam     as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' NOTE what you give up by setting this: the built-in painter is the ONLY thing that runs
' the supersampling pass, so a callback drawing with plain GDI RoundRect/Ellipse gets
' aliased edges. That is a legitimate choice (a host drawing a square switch, or an image,
' may not care), but it is a choice -- it is not free antialiasing you inherit.
'
' The control has already filled the client with BackColor before calling you, so a callback
' that only wants to add something on top does not have to repaint the background.
type TOG_PaintCallbackSub as sub( byval p as CTOGGLE_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' Unlike its siblings this control hands over the FOCUS and KEYBOARD messages as well as the
' mouse ones (WM_SETFOCUS, WM_KILLFOCUS, WM_KEYDOWN), because CToggle is the first focusable
' control in the family and those now carry real state.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the control holds mouse capture across a press and the up-message
'                         is what releases it: a callback that suppressed it would strand
'                         capture and route every subsequent click to this control (the
'                         CListBox bug recorded in Learnings.md). Suppressing
'                         WM_LBUTTONDOWN suppresses the press, never the capture bookkeeping.
'   WM_SETFOCUS         - focus is a FACT the system reports, not an action to veto. The
'   WM_KILLFOCUS          state is already updated by the time you are called; there is
'                         nothing left to suppress. A host that does not want the control
'                         focusable must not make it a tabstop.
' For every other message TRUE suppresses the control's default handling.
type TOG_MessageCallbackFunc as function( byval m as CTOGGLE_MESSAGEINFO ptr ) as boolean

' The checked state changed through USER interaction -- a completed click, or Space/Enter.
' Fired AFTER the control's state is updated, so CToggle_GetChecked() = isChecked. The
' programmatic CToggle_SetChecked does NOT fire this (Win32's BM_SETCHECK / BN_CLICKED
' precedent), which also makes it safe to call SetChecked from inside the handler.
type TOG_CheckChangedCallbackSub as sub( byval hToggle as HWND, byval isChecked as boolean )


type CTOGGLE
    hWin           as HWND
    idc_Toggle     as long = 0
    ' --- State. All single-valued: there is exactly one of this control, so unlike the
    '     collection siblings there are no indices here to be left dangling, and none of
    '     their index fix-up code exists. ---
    isChecked      as boolean = false
    isEnabled      as boolean = true
    isHot          as boolean = false   ' the mouse is over the client
    isFocused      as boolean = false
    isPressed      as boolean = false   ' a live left press (see the capture note below)
    hotTimerOn     as boolean = false   ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press: the
    '     press/cancel gesture (press, slide off the control, release, the state does NOT
    '     flip) consumes the guaranteed down->up pairing, which is the family's test for
    '     taking capture. It pays the full price: snapshot the press BEFORE releasing
    '     (ReleaseCapture sends WM_CAPTURECHANGED synchronously -- Learnings.md), release on
    '     the up-message before any callback runs, WM_CAPTURECHANGED cancels the press,
    '     WM_DESTROY releases, and no callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still inside" is exactly isHot,
    '     and hover tracking already maintains that through the capture. ---
    colors         as CTOGGLE_COLORS
    ' --- Layout. Rects are DERIVED, never set from outside; LayoutToggle() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs
    '     it. There is NO font and NO measuring pass -- alone in the family, LayoutToggle
    '     never takes a DC, because nothing about this control's geometry is measured. ---
    nTrackWidth    as long = CTOGGLE_DEFAULT_TRACKW       ' DPI-scaled at Create
    nTrackHeight   as long = CTOGGLE_DEFAULT_TRACKH       ' DPI-scaled at Create
    nKnobInset     as long = CTOGGLE_DEFAULT_KNOBINSET    ' DPI-scaled at Create
    nBorderThick   as long = CTOGGLE_DEFAULT_BORDERTHICK  ' NOT DPI-scaled
    nFocusGap      as long = CTOGGLE_DEFAULT_FOCUSGAP     ' DPI-scaled at Create
    nFocusThick    as long = CTOGGLE_DEFAULT_FOCUSTHICK   ' NOT DPI-scaled
    nJustify       as long = TOG_JUSTIFY_CENTER
    rcTrack        as RECT               ' derived
    rcKnob         as RECT               ' derived; depends on isChecked
    rcVisual       as RECT               ' derived: rcTrack inflated by the ring padding
    bLayoutDirty   as boolean = true
    PaintCallback        as TOG_PaintCallbackSub          ' optional; replaces the built-in painter
    MessageCallback      as TOG_MessageCallbackFunc       ' optional
    CheckChangedCallback as TOG_CheckChangedCallbackSub   ' optional; user-driven changes only

    declare sub      LayoutToggle()
    declare function RingPad() as long
    declare function KnobDiameter() as long
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' The distance the visual bounds extend beyond the track on every side. The focus ring is
' drawn INSIDE this band, so reserving it unconditionally -- whether or not the control
' currently has focus -- is what stops the pill jumping when focus arrives.
function CTOGGLE.RingPad() as long
    return this.nFocusGap + this.nFocusThick
end function

' The knob is a circle inset from the track on all four sides, so its diameter follows the
' track HEIGHT and the inset, never the width. Clamped at 1: a host that sets an inset
' larger than half the track height gets a degenerate-but-drawable knob rather than an
' Ellipse with a negative extent.
function CTOGGLE.KnobDiameter() as long
    dim as long dia = this.nTrackHeight - (2 * this.nKnobInset)
    if dia < 1 then dia = 1
    return dia
end function


' Assign the three rects. This is the only place toggle geometry is produced; painting,
' invalidation and every public rect query consume it.
'
'   ringPad      = nFocusGap + nFocusThick
'   knobDia      = nTrackHeight - 2*nKnobInset
'   visualW      = nTrackWidth  + 2*ringPad          ( = GetIdealSize's width )
'   visualH      = nTrackHeight + 2*ringPad          ( = GetIdealSize's height )
'
'   rcTrack.top  = clientTop + (clientH - nTrackHeight) \ 2        ALWAYS v-centred
'   rcTrack.left = LEFT   -> clientLeft  + ringPad
'                  CENTER -> clientLeft  + (clientW - nTrackWidth) \ 2
'                  RIGHT  -> clientRight - ringPad - nTrackWidth
'
'   rcKnob.top   = rcTrack.top + nKnobInset                        (w = h = knobDia)
'   rcKnob.left  = OFF -> rcTrack.left  + nKnobInset
'                  ON  -> rcTrack.right - nKnobInset - knobDia
'
'   rcVisual     = rcTrack inflated by ringPad on all four sides
'
' WHY CENTER USES THE TRACK AND NOT THE VISUAL: the ring band is symmetric about the track,
' so centring the track centres the visual too -- and doing it this way keeps the CENTER
' formula independent of the ring settings, which means changing the ring thickness cannot
' nudge a centred pill sideways.
'
' OVERFLOW: when the visual does not fit the client the justification degrades to LEFT and
' the tail clips at the client edge. Rects are computed HONESTLY rather than squeezed
' (CIconPanel's and CTabBar's rule), so the pill keeps its proper shape and only the part
' past the edge is lost. The same applies vertically: a client shorter than the track gives
' a negative top, and the pill is clipped top and bottom rather than being deformed.
'
' NO DC IS TAKEN. There is no text and no font, so nothing here is measured -- every number
' above is authored or derived. This is the only LayoutXxx in the family that can say that.
sub CTOGGLE.LayoutToggle()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects alone and ask to be run again. Note that
    ' CToggle_GetIdealSize deliberately does NOT come through here -- it is computed
    ' straight from the scalars, because a host calls it to decide how big to make the
    ' control in the first place and returning 0 until it had already been sized would be a
    ' chicken-and-egg trap.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    ' NOT named ringPad: FreeBASIC is case-insensitive, so a local with the same name as the
    ' RingPad() member is a duplicate definition inside a member procedure -- and the error
    ' points at the DIM, never at the method it collides with. Same family of trap as the
    ' hFont/HFONT shadowing recorded in C:\dev\Learnings.md.
    dim as long nRingPad = this.RingPad()
    dim as long knobDia  = this.KnobDiameter()
    dim as long visualW  = this.nTrackWidth + (2 * nRingPad)

    ' LEFT, which is also the overflow fallback.
    dim as long x = rcClient.left + nRingPad
    if visualW < clientW then
        select case this.nJustify
        case TOG_JUSTIFY_CENTER
            x = rcClient.left + ((clientW - this.nTrackWidth) \ 2)
        case TOG_JUSTIFY_RIGHT
            x = rcClient.right - nRingPad - this.nTrackWidth
        end select
    end if

    dim as long y = rcClient.top + ((clientH - this.nTrackHeight) \ 2)

    SetRect( @this.rcTrack, x, y, x + this.nTrackWidth, y + this.nTrackHeight )

    dim as long knobX = this.rcTrack.left + this.nKnobInset
    if this.isChecked then knobX = this.rcTrack.right - this.nKnobInset - knobDia
    dim as long knobY = this.rcTrack.top + this.nKnobInset
    SetRect( @this.rcKnob, knobX, knobY, knobX + knobDia, knobY + knobDia )

    this.rcVisual = this.rcTrack
    InflateRect( @this.rcVisual, nRingPad, nRingPad )
end sub


' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's job,
' and only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it.
sub CTOGGLE.CancelPress()
    this.isPressed = false
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy.
sub CTOGGLE.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking pill is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A TOGGLE SWITCH: a rounded pill whose fill changes with state, and a circular knob that
' sits at the left end when OFF and the right end when ON. No text, no animation, no
' tri-state.
'
' NO TEXT, THEREFORE NO FONT
'   The control draws only the track and the knob. There is no caption, no SetFont/GetFont,
'   and no measuring pass -- a host that wants a label puts one beside the control. This is
'   why LayoutToggle takes no DC.
'
' NO TOOLTIPS
'   No tooltip window is created and there is no TOG_TooltipCallback. A host that wants a
'   tip can add its own tool over the control's HWND.
'
' IT IS FOCUSABLE -- THE ONLY ONE IN THE FAMILY
'   WS_TABSTOP, real focus tracking, a painted focus ring, and Space/Enter activation.
'   Every other control in the family is mouse-only, so this is a deliberate departure and
'   not drift. A host that does not run IsDialogMessage in its pump still gets working mouse
'   and (once the control has focus) keyboard behaviour; only Tab NAVIGATION needs the
'   dialog manager.
'
' ANTIALIASING
'   The built-in painter draws through CBufferPaint, which renders geometry with GDI+,
'   so the pill's shoulders and the knob's rim are antialiased. A paint callback replaces
'   the built-in painter entirely -- but it gets the same buffer, so it inherits the same
'   antialiased primitives rather than having to hand-roll smoothing the way the old
'   supersampled painter did.
'
' THE CONTROL HANDLE
'   Every CToggle_* function takes the handle returned by CToggle_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need to
'   treat the control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed. It owns no host resources.
'
' THERE IS NO CToggle_HitTest
'   The whole client rect is the hit area, so a hit test could only ever be
'   PtInRect(client) -- a function that returns TRUE for exactly the points the caller
'   already knows are inside. Its absence is a decision, not an omission.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID), so hosts can find it with GetDlgItem.
'   There are no child controls. The control is created zero-sized: size it with
'   CToggle_GetIdealSize and position it with SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function CToggle_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' State.
'
'   SetChecked is SILENT -- the change callback fires only for user interaction (a completed
'   click, or Space/Enter), following Win32's BM_SETCHECK / BN_CLICKED precedent. That is
'   also what stops a host calling SetChecked from inside its own handler from recursing.
'
'   SetEnabled(false) greys the control, blocks hover, and swallows clicks and keys. It
'   clears any live press and hot state at once rather than waiting for the next mouse move.
'   It does NOT change the checked state: a disabled ON toggle still reads as ON, which is
'   why the color struct carries separate disabled colors for each state.
' ----------------------------------------------------------------------------------------
declare function CToggle_GetChecked( byval hToggle as HWND ) as boolean
declare sub      CToggle_SetChecked( byval hToggle as HWND, byval isChecked as boolean )
declare function CToggle_GetEnabled( byval hToggle as HWND ) as boolean
declare sub      CToggle_SetEnabled( byval hToggle as HWND, byval isEnabled as boolean )
declare function CToggle_GetFocused( byval hToggle as HWND ) as boolean
declare sub      CToggle_Refresh( byval hToggle as HWND )

' ----------------------------------------------------------------------------------------
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
' Create-time defaults are scaled for you). The two THICKNESS values should not be scaled at
' all: a hairline stays a hairline.
'
'   SetTrackSize changes the pill; the knob diameter follows the height and the inset.
'   SetKnobInset is the gap between the track edge and the knob on all four sides.
'   SetFocusRing sets the gap from the track to the ring, and the ring's own thickness. Both
'     are reserved unconditionally, focused or not, so the pill never jumps when focus
'     arrives -- which means changing either changes the IDEAL SIZE.
'   SetJustify places the pill horizontally when the control is wider than it needs; the
'     pill is always vertically centred.
'
'   GetIdealSize is the visual bounds: (trackW + 2*ringPad) x (trackH + 2*ringPad). Size the
'   control with it and the focus ring is never clipped. It is valid BEFORE the control has
'   ever been sized.
'
'   The rect queries force a pending layout, so results are always current. They return
'   FALSE if the control has no geometry yet (created but never sized).
' ----------------------------------------------------------------------------------------
declare sub      CToggle_GetTrackSize( byval hToggle as HWND, byref nTrackWidth as long, byref nTrackHeight as long )
declare sub      CToggle_SetTrackSize( byval hToggle as HWND, byval nTrackWidth as long, byval nTrackHeight as long )
declare function CToggle_GetKnobInset( byval hToggle as HWND ) as long
declare sub      CToggle_SetKnobInset( byval hToggle as HWND, byval nKnobInset as long )
declare function CToggle_GetBorderThickness( byval hToggle as HWND ) as long
declare sub      CToggle_SetBorderThickness( byval hToggle as HWND, byval nThickness as long )
declare sub      CToggle_GetFocusRing( byval hToggle as HWND, byref nGap as long, byref nThickness as long )
declare sub      CToggle_SetFocusRing( byval hToggle as HWND, byval nGap as long, byval nThickness as long )
declare function CToggle_GetJustify( byval hToggle as HWND ) as long
declare sub      CToggle_SetJustify( byval hToggle as HWND, byval nJustify as long )
declare sub      CToggle_GetIdealSize( byval hToggle as HWND, byref nWidth as long, byref nHeight as long )
declare function CToggle_GetTrackRect( byval hToggle as HWND, byref rc as RECT ) as boolean
declare function CToggle_GetKnobRect( byval hToggle as HWND, byref rc as RECT ) as boolean
declare function CToggle_GetVisualRect( byval hToggle as HWND, byref rc as RECT ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance.  SetColors copies the whole struct. GetColors fills one out, so the
' read-modify-write "change one color" idiom is Get, assign, Set.
' ----------------------------------------------------------------------------------------
declare sub      CToggle_GetColors( byval hToggle as HWND, byval pColors as CTOGGLE_COLORS ptr )
declare sub      CToggle_SetColors( byval hToggle as HWND, byval pColors as CTOGGLE_COLORS ptr )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
'   PaintCallback        - draw the whole control INSTEAD OF the built-in painter (which
'                          also means instead of its supersampling pass).
'   MessageCallback      - observe mouse, focus and key messages; return TRUE to suppress
'                          default handling (IGNORED for WM_LBUTTONUP -- capture-critical).
'   CheckChangedCallback - the USER flipped the toggle. Programmatic SetChecked is silent.
' ----------------------------------------------------------------------------------------
declare sub      CToggle_SetPaintCallback( byval hToggle as HWND, byval usersub as TOG_PaintCallbackSub )
declare sub      CToggle_SetMessageCallback( byval hToggle as HWND, byval userfunc as TOG_MessageCallbackFunc )
declare sub      CToggle_SetCheckChangedCallback( byval hToggle as HWND, byval usersub as TOG_CheckChangedCallbackSub )
