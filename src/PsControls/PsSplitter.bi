'    Owner-drawn splitter bar control
'
'    A self-contained, per-instance control in the same vein as PsVScrollBar/PsStatusBar: it
'    owns its own window class and keeps all state per instance in the CWindow UserData
'    area, so any number of instances can coexist.
'
'    WHAT IT IS NOT: the control does not own, create, size, or even know about the pane
'    windows on either side of it. It maintains exactly one number -- the splitter's
'    position -- and tells the host when the user changes it. The host owns its own layout.
'
'    THE POSITION
'      nPos is the bar's LEADING EDGE expressed in the PARENT's client coordinates:
'          PSSPLITTER_VERTICAL   -> the bar's x (a vertical bar splitting left/right panes)
'          PSSPLITTER_HORIZONTAL -> the bar's y (a horizontal bar splitting top/bottom panes)
'      The orientation is named for the BAR's own axis, not for the panes it separates.
'      Min/max limits are in that same coordinate space.
'
'    THE DRAG
'      Live: on every mouse move the control clamps the new position, moves ITSELF with
'      SetWindowPos, then fires the notification. The host only has to lay out the two
'      panes; re-placing the bar from the host's own layout pass is a harmless no-op.

#pragma once

#include once "PsBufferPaint.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
#define IDT_CSPLITTER_HOTTRACK   &hCB20
#define PSSPLITTER_HOTTRACK_MS    100

' Named for the BAR's own axis: a VERTICAL bar is a vertical line separating a left and a
' right pane, and drags horizontally (IDC_SIZEWE).
enum PSSPLITTER_ORIENTATION
    PSSPLITTER_VERTICAL   = 0     ' vertical bar,   left/right panes -> IDC_SIZEWE, nPos = x
    PSSPLITTER_HORIZONTAL = 1     ' horizontal bar, top/bottom panes -> IDC_SIZENS, nPos = y
end enum

' Phase of the position notification. BEGIN and END bracket a user drag; a host uses them
' for the work that is too expensive to do per-pixel (hiding scrollbars, deferring a
' reflow, committing the new layout to config).
enum PSSPLITTER_PHASE
    PSSPLITTER_PHASE_BEGIN = 0    ' the user just grabbed the bar; newPos = current position
    PSSPLITTER_PHASE_MOVE  = 1    ' the position actually changed
    PSSPLITTER_PHASE_END   = 2    ' the drag ended (button up, or capture lost); final position
end enum

type PSSPLITTER_PAINTINFO
    hSplitter    as HWND                  ' the control, so the callback can query it
    b            as PsBufferPaint ptr   ' the control's buffer for this repaint (no copy)
    rcClient     as RECT                  ' the whole bar, in client coordinates
    nOrientation as integer               ' PSSPLITTER_VERTICAL / PSSPLITTER_HORIZONTAL
    isHot        as boolean               ' the mouse is over the bar
    isDragging   as boolean               ' a drag is in progress (hot stays pinned true)
end type

type PSSPLITTER_MESSAGEINFO
    hSplitter as HWND
    uMsg      as UINT
    wParam    as WPARAM
    lParam    as LPARAM
end type


' Paint the bar. Paint through p->b (the control's double buffer for this repaint) using
' p->rcClient -- do not touch the screen DC. If unset, a built-in default paint using the
' colors below is used; setting a callback replaces that default ENTIRELY.
type SPL_PaintCallbackSub as sub( byval p as PSSPLITTER_PAINTINFO ptr )

' Observe mouse messages (including WM_LBUTTONDBLCLK -- the class carries CS_DBLCLKS -- and
' WM_SETCURSOR). Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' ONE ASYMMETRY, AND IT IS DELIBERATE: the result is IGNORED for WM_LBUTTONUP. The control
' holds the mouse capture for the duration of a drag and that up-message is the capture's
' exit; a callback allowed to suppress it would strand the capture and every subsequent
' click in the app would be routed here. See C:\dev\Learnings.md.
'
' Claiming WM_LBUTTONDBLCLK is the supported way to implement collapse/restore on a
' double-click: no drag starts, and the trailing WM_LBUTTONUP is harmless.
type SPL_MessageCallbackFunc as function( byval m as PSSPLITTER_MESSAGEINFO ptr ) as boolean

' The user moved the splitter. newPos is the bar's new leading edge in PARENT client
' coordinates; nPhase is one of the PSSPLITTER_PHASE_* values. The control has already moved
' itself -- lay out the two panes and nothing else.
'
' NOT fired for PsSplitter_SetPos / PsSplitter_SetRange: programmatic setters are silent, so a
' host can call them from inside this very callback without re-entering itself.
type SPL_PosChangedCallbackSub as sub( byval hSplitter as HWND, byval newPos as integer, _
                                       byval nPhase as integer )


type PSSPLITTER
    hWin           as HWND
    hParent        as HWND                ' the coordinate space nPos lives in
    nOrientation   as integer = PSSPLITTER_VERTICAL
    ' --- Position model. nPos is the bar's leading edge in hParent's client coords. ---
    nPos           as integer = 0
    nMin           as integer = 0
    nMax           as integer = &h7FFFFFFF   ' effectively unbounded until SetRange is called
    ' --- Interaction ---
    isHot          as boolean = false
    isDragging     as boolean = false
    dragOffset     as integer = 0         ' cursor offset from the bar's leading edge at grab
    hotTimerOn     as boolean = false
    ' Shared system cursor (IDC_SIZEWE / IDC_SIZENS), loaded once at Create. NOT named
    ' hCursor: FreeBASIC is case-insensitive, so a member called hCursor would shadow the
    ' TYPE name HCURSOR inside every member procedure of this type, and "dim as HCURSOR x"
    ' there fails with a misleading "error 14: Expected identifier". Same trap as
    ' PsStatusBar's hPanelFont/HFONT. See C:\dev\Learnings.md.
    hBarCursor     as HCURSOR
    ' --- Appearance (used by the default paint; a PaintCallback overrides entirely) ---
    BackColor      as COLORREF = BGR(53,59,69)
    HotColor       as COLORREF = BGR(90,98,112)
    PaintCallback      as SPL_PaintCallbackSub
    MessageCallback    as SPL_MessageCallbackFunc
    PosChangedCallback as SPL_PosChangedCallbackSub

    declare function ClampPos( byval n as integer ) as integer
    declare function WindowRectInParent() as RECT
    declare function BarEdgeFromWindow() as integer
    declare function CursorAxisInParent() as integer
    declare sub      MoveBar()
    declare sub      Notify( byval nPhase as integer )
end type


' Clamp into [nMin, nMax]. An inverted range (max < min) collapses to min rather than
' rejecting the call -- a host recomputing limits on WM_SIZE legitimately produces one for
' a window too small to hold both panes, and pinning the bar is the sane result.
function PSSPLITTER.ClampPos( byval n as integer ) as integer
    dim as integer lo = this.nMin
    dim as integer hi = this.nMax
    if hi < lo then hi = lo
    if n < lo then n = lo
    if n > hi then n = hi
    return n
end function

' The control's own window rect, converted to the parent's client coordinates -- the space
' nPos is measured in.
function PSSPLITTER.WindowRectInParent() as RECT
    dim as RECT rc
    if this.hWin = 0 then return rc
    GetWindowRect( this.hWin, @rc )
    if this.hParent then
        MapWindowPoints( HWND_DESKTOP, this.hParent, cast( POINT ptr, @rc ), 2 )
    end if
    return rc
end function

' Where the bar ACTUALLY is, as opposed to where nPos says it is. Used to re-sync after the
' host moves us itself.
function PSSPLITTER.BarEdgeFromWindow() as integer
    dim as RECT rc = this.WindowRectInParent()
    if this.nOrientation = PSSPLITTER_VERTICAL then return rc.left
    return rc.top
end function

' The cursor's position along the drag axis, in parent client coordinates. Read from
' GetCursorPos rather than from a message's lParam on purpose: lParam is relative to a
' window we are moving mid-gesture, and applying it as an incremental delta makes clamping
' "sticky" (the bar starts moving again before the cursor has travelled back to the grab
' point). An absolute parent-relative read has neither problem.
function PSSPLITTER.CursorAxisInParent() as integer
    dim as POINT pt
    GetCursorPos( @pt )
    if this.hParent then ScreenToClient( this.hParent, @pt )
    if this.nOrientation = PSSPLITTER_VERTICAL then return pt.x
    return pt.y
end function

' Move the window to nPos along the drag axis ONLY. The cross-axis position and the size
' are read back from the window itself and preserved -- the control never invents geometry
' the host owns.
sub PSSPLITTER.MoveBar()
    if this.hWin = 0 then exit sub
    dim as RECT rc = this.WindowRectInParent()
    dim as integer x = rc.left
    dim as integer y = rc.top
    if this.nOrientation = PSSPLITTER_VERTICAL then
        x = this.nPos
    else
        y = this.nPos
    end if
    if (x = rc.left) andalso (y = rc.top) then exit sub
    SetWindowPos( this.hWin, NULL, x, y, 0, 0, _
                  SWP_NOSIZE or SWP_NOZORDER or SWP_NOACTIVATE )
end sub

sub PSSPLITTER.Notify( byval nPhase as integer )
    if this.PosChangedCallback then
        this.PosChangedCallback( this.hWin, this.nPos, nPhase )
    end if
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' THE CONTROL HANDLE
'   Every PsSplitter_* function takes the handle returned by PsSplitter_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need to
'   treat the control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed. It loads no resource that needs
'   freeing (the resize cursors are shared system cursors -- never DestroyCursor them).
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). There are no child controls.
'   The control is created zero-sized and hidden: position, size and show it with
'   SetWindowPos(). The width (or height) you give it IS the grab area -- there is no
'   separate hit margin; paint a thinner line inside it if you want a thin look.
'
'   The orientation is fixed for the control's lifetime. A host that switches split modes
'   destroys the control and creates the other one; that is cheaper than reasoning about an
'   nPos whose meaning changed axis underneath a stale range.
' ----------------------------------------------------------------------------------------
declare function PsSplitter_Create( byval hWndParent as HWND, byval CtrlID as integer, _
                                   byval nOrientation as integer = PSSPLITTER_VERTICAL ) as HWND
declare function PsSplitter_GetOrientation( byval hSplit as HWND ) as integer

' ----------------------------------------------------------------------------------------
' Position and limits. All values are the bar's LEADING EDGE in the PARENT's client
' coordinates (x for a vertical bar, y for a horizontal one).
'
'   PsSplitter_SetPos    clamps, moves the bar, repaints -- and fires NOTHING. This is the
'                       host telling the control where it put the splitter, not the user
'                       moving it.
'   PsSplitter_SetRange  the limits the drag clamps to. The control does NOT derive them
'                       from the parent's client rect, because it cannot know what else
'                       lives there (tiko's help viewer parks a scrollbar between the pane
'                       and the bar). Recompute and re-set them from your own WM_SIZE.
'                       Also clamps the current position into the new range, silently.
'   PsSplitter_GetPos    always agrees with where the bar actually is: the control re-syncs
'                       from its own window rect whenever the HOST moves it.
' ----------------------------------------------------------------------------------------
declare function PsSplitter_GetPos( byval hSplit as HWND ) as integer
declare sub      PsSplitter_SetPos( byval hSplit as HWND, byval nPosition as integer )
declare sub      PsSplitter_SetRange( byval hSplit as HWND, byval nMin as integer, byval nMax as integer )
declare function PsSplitter_GetMin( byval hSplit as HWND ) as integer
declare function PsSplitter_GetMax( byval hSplit as HWND ) as integer

' ----------------------------------------------------------------------------------------
' State queries.
' ----------------------------------------------------------------------------------------
declare function PsSplitter_IsHot( byval hSplit as HWND ) as boolean
declare function PsSplitter_IsDragging( byval hSplit as HWND ) as boolean

' ----------------------------------------------------------------------------------------
' Appearance. Used by the built-in painter only; a PaintCallback ignores them.
' ----------------------------------------------------------------------------------------
declare function PsSplitter_GetBackColor( byval hSplit as HWND ) as COLORREF
declare sub      PsSplitter_SetColors( byval hSplit as HWND, byval backclr as COLORREF, byval hotclr as COLORREF )

' ----------------------------------------------------------------------------------------
' Callbacks. See the type declarations above for each signature and contract.
'   PaintCallback       - draw the bar. Optional; replaces the built-in painter.
'   MessageCallback     - observe mouse messages; return TRUE to suppress default handling
'                         (ignored for WM_LBUTTONUP).
'   PosChangedCallback  - the user moved the splitter (BEGIN / MOVE / END).
' ----------------------------------------------------------------------------------------
declare sub      PsSplitter_SetPaintCallback( byval hSplit as HWND, byval usersub as SPL_PaintCallbackSub )
declare sub      PsSplitter_SetMessageCallback( byval hSplit as HWND, byval userfunc as SPL_MessageCallbackFunc )
declare sub      PsSplitter_SetPosChangedCallback( byval hSplit as HWND, byval usersub as SPL_PosChangedCallbackSub )
