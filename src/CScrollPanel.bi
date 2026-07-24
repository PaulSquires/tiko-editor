'    Owner-drawn scrolling panel control
'
'    A self-contained, per-instance control in the same vein as CListBox/CVScrollBar: it
'    owns its own window class and keeps all state per instance in the CWindow UserData
'    area, so any number of instances can coexist.
'
'    WHAT IT IS
'      A viewport onto a taller page of host-owned controls -- the settings dialog whose
'      list of options is longer than one screen. Three windows:
'
'          CScrollPanel container HWND    WS_CLIPCHILDREN -- this is the viewport
'          +-- hPanel (CWindow)           the PAGE. Taller than the viewport; the host
'          |     +-- the host's controls  parents its controls HERE and lays them out in
'          |                              PANEL coordinates, once
'          +-- hScrollBar (CVScrollBar)   a reserved strip on the right edge
'
'      Scrolling MOVES THE PANEL: SetWindowPos( hPanel, 0, -nPos, ... ). The children keep
'      their panel-relative coordinates forever, the container's WS_CLIPCHILDREN does the
'      clipping, and USER32 blits the whole subtree in one go. The host never repositions
'      anything to scroll.
'
'    THE THUMB IS NORMALLY INVISIBLE
'      It appears when the cursor is over the control OR when focus is inside the panel,
'      and goes away otherwise -- the settings-dialog look, where the scrollbar is a hint
'      rather than furniture. The bar's STRIP is reserved either way, so the panel's width
'      never changes and host content never jumps sideways as the mouse comes and goes.
'
'    UNITS
'      Everything scroll-related is in PIXELS: content height, position, wheel step.

#pragma once

#include once "CBufferPaint.bi"
#include once "CVScrollBar.bi"

' Reveal/focus poll timer. Timer ids are per-window, so every instance can share this one.
' (&hCB10/&hCB11 belong to the CVScrollBar this control owns -- do not reuse them here.)
#define IDT_CSCROLLPANEL_REVEAL   &hCB30
#define CSCROLLPANEL_REVEAL_MS    100

#define CSCROLLPANEL_LINE_HEIGHT  20    ' default wheel unit, unscaled (DPI scaled at use)


type SCP_PAINTINFO
    hScrollPanel as HWND                 ' the container, so the callback can query it
    hPanel       as HWND                 ' the window actually being painted
    b            as CBufferPaint ptr     ' the panel's buffer for this repaint (no copy)
    rcClient     as RECT                 ' the WHOLE page, not the visible slice
    nPos         as integer              ' current scroll offset, for content that cares
end type


' Observe a message on either of the control's own windows. hFrom says which.
'
' ONE DEPARTURE FROM THE FAMILY, AND IT IS DELIBERATE: this struct carries lResult. The
' panel is the parent of the host's controls, so WM_CTLCOLORSTATIC/BTN/EDIT/LISTBOX arrive
' HERE and the host cannot see them any other way -- and those messages answer with an
' HBRUSH. A plain "handled" boolean cannot express that. Set m->lResult before returning
' TRUE and it becomes the window procedure's return value.
type SCP_MESSAGEINFO
    hScrollPanel as HWND
    hPanel       as HWND
    hFrom        as HWND                 ' = hScrollPanel or = hPanel
    uMsg         as UINT
    wParam       as WPARAM
    lParam       as LPARAM
    lResult      as LRESULT              ' the value to return when the callback claims it
end type


' Paint the PAGE background. Paint through p->b using p->rcClient -- do not touch the
' screen DC. If unset, a flat fill in BackColor is used; setting a callback replaces that
' default ENTIRELY. Use it for section rules, group boxes, alternating bands and so on.
'
' p->rcClient is the whole page (content height), not the visible slice: the panel window
' IS that tall, and Windows clips the repaint to whatever part of it is on screen.
type SCP_PaintCallbackSub as sub( byval p as SCP_PAINTINFO ptr )

' Observe messages on the container and the panel. Return TRUE to suppress the control's
' own handling, setting m->lResult first if the message has a meaningful return value.
'
' WM_COMMAND / WM_NOTIFY / WM_HSCROLL / WM_VSCROLL from the host's own controls arrive at
' the PANEL, since it is their parent. They are offered here first and, if not claimed,
' FORWARDED to the container's parent -- so a host that already handles its controls in its
' own WndProc keeps working unchanged when its page moves onto a CScrollPanel. Claim them
' here only if you want them instead of, not as well as, that.
type SCP_MessageCallbackFunc as function( byval m as SCP_MESSAGEINFO ptr ) as boolean

' The viewport was resized: re-flow the page and say how tall it now is.
'
' cxPanel is the width the page is being given, HANDED TO YOU rather than left to be read
' back -- do not call GetClientRect(hPanel) in here and reason about a width that may be
' one call stale. Return the new content height in pixels, or 0 to leave it unchanged.
'
' Calling CScrollPanel_SetContentHeight from inside this callback is safe: the control is
' in a layout pass and the setter records the value instead of starting another one.
type SCP_LayoutCallbackFunc as function( byval hPanel as HWND, byval cxPanel as integer ) as integer

' The scroll position changed through USER action -- wheel, thumb drag, track paging, or
' the focus-into-view scroll that follows a Tab. NOT fired for CScrollPanel_SetPos /
' SetContentHeight / EnsureVisible called by the host, so a host can call those from inside
' this very callback without re-entering itself.
type SCP_ScrollCallbackSub as sub( byval hScrollPanel as HWND, byval newPos as integer )


type CSCROLLPANEL
    hWin           as HWND               ' the container / viewport
    hPanel         as HWND               ' the page
    hScrollBar     as HWND               ' the CVScrollBar
    ' --- Scroll model. Both in PIXELS; nPos is the page's first visible row of pixels. ---
    nContentHeight as integer = 0
    nPos           as integer = 0
    ' --- Geometry inputs (unscaled; DPI applied at use) ---
    ScrollBarWidth as integer = CVSCROLL_DEFAULT_WIDTH
    LineHeight     as integer = CSCROLLPANEL_LINE_HEIGHT
    ' --- Interaction / policy ---
    bThumbShown    as boolean = false    ' what the reveal rule last decided
    bAutoScrollToFocus as boolean = true
    hLastFocus     as HWND               ' change detector for the focus-into-view scroll
    revealTimerOn  as boolean = false
    bInLayout      as boolean = false    ' re-entrancy guard for the layout callback
    ' --- Appearance (used by the built-in painter and by WM_CTLCOLOR*) ---
    BackColor      as COLORREF = BGR(33,37,43)
    ForeColor      as COLORREF = BGR(215,218,224)
    ' The one GDI resource this control owns: the brush handed back to WM_CTLCOLOR*.
    ' Recreated by SetColors, deleted at WM_NCDESTROY. NOT named hBrush -- FreeBASIC is
    ' case-insensitive, so a member called hBrush would shadow the TYPE name HBRUSH inside
    ' every member procedure of this type (CSplitter's hBarCursor trap; see Learnings.md).
    hBackBrushObj  as HBRUSH
    PaintCallback    as SCP_PaintCallbackSub
    MessageCallback  as SCP_MessageCallbackFunc
    LayoutCallback   as SCP_LayoutCallbackFunc
    ScrollCallback   as SCP_ScrollCallbackSub

    declare function ViewportHeight() as integer
    declare function ViewportWidth() as integer
    declare function ScaledBarWidth() as integer
    declare function PanelWidth() as integer
    declare function PanelHeight() as integer
    declare function MaxPos() as integer
    declare function ClampPos( byval n as integer ) as integer
    declare function NeedsThumb() as boolean
    declare function ScaledLineHeight() as integer
    declare function FocusIsInside() as boolean
    declare function MouseIsOver() as boolean
    declare sub      ApplyScroll()
end type


' The viewport is the container's client area. Both are read live rather than cached: the
' host resizes us whenever it likes and a stale copy would misplace the page.
function CSCROLLPANEL.ViewportHeight() as integer
    dim as RECT rc : GetClientRect( this.hWin, @rc )
    return rc.bottom - rc.top
end function

function CSCROLLPANEL.ViewportWidth() as integer
    dim as RECT rc : GetClientRect( this.hWin, @rc )
    return rc.right - rc.left
end function

function CSCROLLPANEL.ScaledBarWidth() as integer
    dim pWindow as CWindow ptr = AfxCWindowPtr( this.hWin )
    if pWindow then return pWindow->ScaleX( this.ScrollBarWidth )
    return this.ScrollBarWidth
end function

' The strip is reserved whether or not the thumb is currently shown -- that is what stops
' host content jumping sideways as the mouse comes and goes.
function CSCROLLPANEL.PanelWidth() as integer
    dim as integer w = this.ViewportWidth() - this.ScaledBarWidth()
    if w < 0 then w = 0
    return w
end function

' A page shorter than the viewport is stretched to fill it, so the container's own fill can
' only ever show through in the bar strip.
function CSCROLLPANEL.PanelHeight() as integer
    dim as integer h = this.nContentHeight
    if h < this.ViewportHeight() then h = this.ViewportHeight()
    return h
end function

function CSCROLLPANEL.MaxPos() as integer
    dim as integer m = this.nContentHeight - this.ViewportHeight()
    if m < 0 then m = 0
    return m
end function

function CSCROLLPANEL.ClampPos( byval n as integer ) as integer
    if n < 0 then n = 0
    if n > this.MaxPos() then n = this.MaxPos()
    return n
end function

' Is there anything to scroll? A pure RANGE question, deliberately independent of the
' scrollbar's own geometry -- see the same reasoning in CVScrollBar.NeedsThumb.
function CSCROLLPANEL.NeedsThumb() as boolean
    return (this.MaxPos() > 0)
end function

function CSCROLLPANEL.ScaledLineHeight() as integer
    dim as integer h = this.LineHeight
    dim pWindow as CWindow ptr = AfxCWindowPtr( this.hWin )
    if pWindow then h = pWindow->ScaleY( this.LineHeight )
    if h < 1 then h = 1
    return h
end function

function CSCROLLPANEL.FocusIsInside() as boolean
    dim as HWND hFocus = GetFocus()
    if hFocus = 0 then return false
    if hFocus = this.hPanel then return true
    return (IsChild( this.hPanel, hFocus ) <> 0)
end function

' A geometric test against the container's client rect, so the host's child controls are
' irrelevant -- the cursor sitting on an EDIT box still counts as "over the control".
function CSCROLLPANEL.MouseIsOver() as boolean
    dim as RECT rc : GetClientRect( this.hWin, @rc )
    return isMouseOverRECT( this.hWin, rc )
end function

' The ONE place the page is moved. Width and height come from the model, y is the scroll.
sub CSCROLLPANEL.ApplyScroll()
    if this.hPanel = 0 then exit sub
    SetWindowPos( this.hPanel, NULL, _
                  0, -this.nPos, this.PanelWidth(), this.PanelHeight(), _
                  SWP_NOZORDER or SWP_NOACTIVATE )
end sub


' The reveal rule, as a PURE function of its three inputs -- deliberately not a method, so
' the truth table can be asserted without a real mouse, a real focus, or a real window.
' That testability is the whole reason it is factored out.
'
'   bNeeds  there is something to scroll at all
'   bMouse  the cursor is over the control
'   bFocus  a control on the page holds the focus
'
' Either of the last two alone reveals the thumb; a keyboard-only user is not left guessing
' where they are on the page.
declare function CScrollPanel_ShouldShow( byval bNeeds as boolean, _
                                          byval bMouse as boolean, _
                                          byval bFocus as boolean ) as boolean


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every CScrollPanel_* function takes the handle returned by CScrollPanel_Create().
'   It is a real HWND on purpose (not an opaque type): callers legitimately need to treat
'   the control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed, including its two child windows
'   and the WM_CTLCOLOR brush. The host's controls are children of the PANEL, so they are
'   destroyed with it -- do not DestroyWindow them yourself afterwards.
'
' ----------------------------------------------------------------------------------------
' Creation. The control is created zero-sized and hidden: position, size and show it with
' SetWindowPos. CtrlID becomes the container's id (GWLP_ID); the panel and the scrollbar
' take CtrlID + 1 and CtrlID + 2.
'
' THE FIRST THING TO DO AFTER Create IS CScrollPanel_GetPanel: the host's controls are
' parented to the PANEL, never to the container. A control created on the container itself
' would sit over the viewport and never scroll.
' ----------------------------------------------------------------------------------------
declare function CScrollPanel_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND
declare function CScrollPanel_GetPanel( byval hScp as HWND ) as HWND
declare function CScrollPanel_GetScrollBar( byval hScp as HWND ) as HWND

' ----------------------------------------------------------------------------------------
' Content and scrolling. All values are PIXELS in PAGE coordinates.
'
'   SetContentHeight   how tall the page is. The host owns this number, because only the
'                      host knows whether a collapsed section counts. Re-clamps the current
'                      position, re-sizes the page and re-syncs the bar -- silently.
'   AutoSizeContent    the convenience: max(bottom of every VISIBLE child of the panel) plus
'                      bottomPadding, pushed through SetContentHeight. Returns what it set.
'                      Hidden children are skipped, so a collapsed section does not leave a
'                      tail of empty page behind it.
'   SetPos / ScrollBy  clamped, applied, silent.
'   EnsureVisible      scroll the MINIMUM distance that brings hChild fully into view; a
'                      child taller than the viewport is top-aligned. Silent, and it never
'                      calls SetFocus (see Learnings.md on layout routines that steal it).
'   Recalc             force a full layout pass -- the layout callback, the page geometry
'                      and the scrollbar sync. Call it after adding or moving children.
' ----------------------------------------------------------------------------------------
declare sub      CScrollPanel_SetContentHeight( byval hScp as HWND, byval nHeight as integer )
declare function CScrollPanel_GetContentHeight( byval hScp as HWND ) as integer
declare function CScrollPanel_AutoSizeContent( byval hScp as HWND, byval bottomPadding as integer = 0 ) as integer
declare function CScrollPanel_GetPos( byval hScp as HWND ) as integer
declare sub      CScrollPanel_SetPos( byval hScp as HWND, byval nPosition as integer )
declare function CScrollPanel_ScrollBy( byval hScp as HWND, byval nDelta as integer ) as boolean
declare function CScrollPanel_EnsureVisible( byval hScp as HWND, byval hChild as HWND, byval nMargin as integer = 0 ) as boolean
declare function CScrollPanel_GetViewportHeight( byval hScp as HWND ) as integer
declare function CScrollPanel_GetPanelWidth( byval hScp as HWND ) as integer
declare sub      CScrollPanel_Recalc( byval hScp as HWND )

' ----------------------------------------------------------------------------------------
' Appearance.
'   SetColors           the page fill AND the text/background handed to the host's controls
'                       through WM_CTLCOLOR*. One call themes a whole settings page of
'                       plain labels and checkboxes.
'   SetScrollBarColors  forwarded straight to the owned CVScrollBar. Its BACK color defaults
'                       to the panel's, which is what makes the hidden state seamless.
'   SetScrollBarWidth   unscaled; this is the width of the reserved strip.
'   SetLineHeight       unscaled; the pixel value of one "line" for the wheel (see below).
'   SetFont             applied to every existing child of the panel (WM_SETFONT). The HOST
'                       keeps ownership of the HFONT -- this control never deletes it.
' ----------------------------------------------------------------------------------------
declare sub      CScrollPanel_SetColors( byval hScp as HWND, byval backclr as COLORREF, byval foreclr as COLORREF )
declare sub      CScrollPanel_SetScrollBarColors( byval hScp as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
declare sub      CScrollPanel_SetScrollBarWidth( byval hScp as HWND, byval nWidth as integer )
declare sub      CScrollPanel_SetLineHeight( byval hScp as HWND, byval nHeight as integer )
declare sub      CScrollPanel_SetFont( byval hScp as HWND, byval hFont as HFONT )

' ----------------------------------------------------------------------------------------
' Behaviour and state.
'
'   SetAutoScrollToFocus  ON by default. With it on, focus landing on an off-screen child --
'                         a Tab, or a host SetFocus -- scrolls that child into view. The
'                         scroll happens on a focus CHANGE only, so a user who deliberately
'                         scrolls the focused control off screen does not have it yanked
'                         back under them.
'   IsThumbVisible        what the reveal rule currently says.
' ----------------------------------------------------------------------------------------
declare sub      CScrollPanel_SetAutoScrollToFocus( byval hScp as HWND, byval bEnable as boolean )
declare function CScrollPanel_GetAutoScrollToFocus( byval hScp as HWND ) as boolean
declare function CScrollPanel_IsThumbVisible( byval hScp as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' MOUSE WHEEL / TRACKPAD
'
' Handled on the container and on the panel, so a wheel gesture anywhere over the page
' scrolls it. Host controls that ignore the wheel bubble it up through DefWindowProc, which
' is exactly the parent direction, so an EDIT under the cursor costs nothing.
'
' HandleWheelDelta is the same implementation exposed, for a host that needs to route a
' gesture it received itself. Pass the ALREADY-EXTRACTED signed delta:
'
'     case WM_MOUSEWHEEL
'         CScrollPanel_HandleWheelDelta( hScp, cast(short, hiword(wParam)) )
'
' The cast matters: loword/hiword yield UNSIGNED words, so a delta read without it turns
' ~-120 into ~65416 and the direction silently inverts (see C:\dev\Learnings.md).
'
' THE UNIT. This control's range is in PIXELS, but the system's wheel setting
' (SPI_GETWHEELSCROLLLINES) counts LINES. Feeding that number to a pixel range gives three
' pixels a notch and a gesture that feels dead -- the exact CHScrollBar bug in Learnings.md.
' So the conversion happens here: one notch = system lines x SetLineHeight (default 20,
' DPI-scaled). Set LineHeight to your settings row's pitch and a notch moves whole rows.
' ----------------------------------------------------------------------------------------
declare function CScrollPanel_HandleWheelDelta( byval hScp as HWND, byval nDelta as integer ) as boolean

' ----------------------------------------------------------------------------------------
' Callbacks. See the type declarations above for each signature and contract.
'   PaintCallback     - draw the page background. Optional; replaces the built-in fill.
'   MessageCallback   - observe container/panel messages; return TRUE to suppress default
'                       handling, having set m->lResult if the message returns a value.
'   LayoutCallback    - the viewport resized: re-flow, return the new content height.
'   ScrollCallback    - the USER scrolled.
' ----------------------------------------------------------------------------------------
declare sub      CScrollPanel_SetPaintCallback( byval hScp as HWND, byval usersub as SCP_PaintCallbackSub )
declare sub      CScrollPanel_SetMessageCallback( byval hScp as HWND, byval userfunc as SCP_MessageCallbackFunc )
declare sub      CScrollPanel_SetLayoutCallback( byval hScp as HWND, byval userfunc as SCP_LayoutCallbackFunc )
declare sub      CScrollPanel_SetScrollCallback( byval hScp as HWND, byval usersub as SCP_ScrollCallbackSub )
