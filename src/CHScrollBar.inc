''
''  CHScrollBar.inc  -- owner-drawn horizontal scrollbar
''
''  Seeded from CVScrollBar.inc (the vertical sibling); the axis is transposed and the
''  module-level helper names are H-prefixed so both controls can be included into the
''  same host (tiko includes the whole control family).
''

#include once "CHScrollBar.bi"


function GetHScrollBarPointer( byval hSB as HWND ) as CHSCROLLBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hSB)
    if pWindow = 0 then return 0
    return cast( CHSCROLLBAR ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Move by nUnits (signed, in range units), clamped to [0, MaxPos()], notifying the owner.
' Returns TRUE if the position actually changed.
'
' THE one place that clamps, recalculates, invalidates and notifies. Paging and the wheel
' both route through it, so the two can never drift apart.
' ========================================================================================
function CHScrollBar_StepBy( byval pSB as CHSCROLLBAR ptr, byval nUnits as integer ) as boolean
    if pSB = 0 then return false
    dim as integer newPos = pSB->nPos + nUnits
    if newPos < 0 then newPos = 0
    if newPos > pSB->MaxPos() then newPos = pSB->MaxPos()
    if newPos = pSB->nPos then return false
    pSB->nPos = newPos
    pSB->Recalc()
    InvalidateRect( pSB->hWin, NULL, FALSE )
    if pSB->ScrollCallback then pSB->ScrollCallback( pSB->hWin, newPos )
    return true
end function


' ========================================================================================
' Move by one page in the given direction (-1 left / +1 right), notifying the owner.
' Returns TRUE if the position actually changed.
' ========================================================================================
function CHScrollBar_PageStep( byval pSB as CHSCROLLBAR ptr, byval nDir as integer ) as boolean
    if pSB = 0 then return false
    return CHScrollBar_StepBy( pSB, nDir * pSB->nPage )
end function


' ========================================================================================
' Feed the scrollbar a horizontal mouse-wheel delta. THE single implementation of wheel
' scrolling: the control's own WM_MOUSEHWHEEL handler is a thin wrapper around this, and a
' host can call it directly to route a swipe made over the CONTENT into this bar.
'
' nDelta is the ALREADY-EXTRACTED signed delta, not the raw wParam:
'
'     CHScrollBar_HandleWheelDelta( hSB, cast(short, hiword(wParam)) )
'
' The cast matters -- loword/hiword yield UNSIGNED words, so a swipe-left delta read without
' it becomes ~65416 and the direction silently inverts (see C:\dev\Learnings.md).
'
' SIGN -- THE TRAP IN THIS FILE: for WM_MOUSEHWHEEL a POSITIVE delta means "scroll RIGHT",
' which INCREASES nPos. That is the OPPOSITE of CVScrollBar, where a positive WM_MOUSEWHEEL
' delta means the wheel moved away from the user, scrolling UP, DECREASING nPos. The two
' controls are otherwise exact mirrors and both readings look equally plausible in code, so
' the sign is applied here, once, with this comment on it.
'
' High-resolution wheels and precision touchpads send deltas well under one 120 notch, so
' the remainder is accumulated: a slow two-finger swipe moves the bar continuously instead
' of sitting dead until a whole notch arrives.
'
' Returns TRUE if the position actually moved.
' ========================================================================================
function CHScrollBar_HandleWheelDelta( byval hSB as HWND, byval nDelta as integer ) as boolean
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return false
    if nDelta = 0 then return false

    pSB->nWheelAccum += nDelta

    ' Units moved per 120-unit notch: the caller's override, else the system setting. The
    ' system value is re-read per message rather than cached at Create, so changing it in
    ' Control Panel takes effect without restarting the app. Note this is the CHARS
    ' setting, not LINES -- the horizontal analogue, and a separate user preference.
    dim as integer nStep = pSB->nWheelStep
    if nStep <= 0 then
        dim as ulong sysval = 3
        SystemParametersInfo( SPI_GETWHEELSCROLLCHARS, 0, @sysval, 0 )
        ' WHEEL_PAGESCROLL (= UINT_MAX) means "one page per notch". FreeBASIC's winuser.bi
        ' declares the SPI_* constants but NOT this one, so it is spelled out -- the same
        ' workaround CTextBox.inc uses for the identical check.
        if sysval = cast(ulong, &hFFFFFFFF) then
            nStep = pSB->nPage
        else
            nStep = cast(integer, sysval)
        end if
    end if
    if nStep <= 0 then nStep = 1        ' never divide by, or step, zero

    dim as integer nUnits = (pSB->nWheelAccum * nStep) \ 120
    if nUnits = 0 then return false     ' not enough delta yet; the remainder is kept

    ' Consume exactly the delta those units represent, keeping the rest. FB's \ truncates
    ' toward zero, which is what makes this symmetric for negative deltas and stops the
    ' accumulator overshooting past the units actually applied.
    pSB->nWheelAccum -= (nUnits * 120) \ nStep

    return CHScrollBar_StepBy( pSB, nUnits )     ' SIGN: positive delta scrolls right
end function


' ========================================================================================
' Stop the track auto-repeat.
' ========================================================================================
sub CHScrollBar_StopRepeat( byval pSB as CHSCROLLBAR ptr )
    if pSB = 0 then return
    if pSB->repeatTimerOn then
        KillTimer( pSB->hWin, IDT_CHSCROLL_REPEAT )
        pSB->repeatTimerOn = false
    end if
    pSB->repeatDir = 0
end sub


' ========================================================================================
' Clear the thumb hover state and stop the hover poll timer. Repaints only if the
' state actually changes, so this is cheap to call repeatedly.
' ========================================================================================
sub CHScrollBar_ClearHot( byval pSB as CHSCROLLBAR ptr )
    if pSB = 0 then return
    if pSB->hotTimerOn then
        KillTimer( pSB->hWin, IDT_CHSCROLL_HOTTRACK )
        pSB->hotTimerOn = false
    end if
    if pSB->isHot then
        pSB->isHot = false
        InvalidateRect( pSB->hWin, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Process WM_PAINT. The thumb is always recalculated from the current range here, so it
' can never go stale no matter who changed the position.
' ========================================================================================
function CHScrollBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hwnd)
    if pSB = 0 then return 0
    pSB->Recalc()

    dim as clsDoubleBuffer b
    b.BeginDoubleBuffer(hwnd)

    dim as CHSCROLLBAR_PAINTINFO p
    GetClientRect( hwnd, @p.rcClient )
    p.b          = @b
    p.rcThumb    = pSB->rcThumb
    p.isHot      = pSB->isHot
    p.isDragging = pSB->isDragging

    if pSB->PaintCallback then
        pSB->PaintCallback( @p )
    else
        ' Default paint: flat track + flat thumb, brighter while hot/dragging.
        b.SetBackColor( pSB->BackColor )
        b.PaintClientRect()
        if IsRectEmpty( @pSB->rcThumb ) = 0 then
            dim as COLORREF clr = pSB->ForeColor
            if pSB->isHot orelse pSB->isDragging then clr = pSB->ForeColorHot
            b.SetBackColor( clr )
            b.PaintRect( @pSB->rcThumb )
        end if
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' CHScrollBar Window procedure
' ========================================================================================
function CHScrollBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hwnd)

    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT, CHScrollBar_OnPaint)

    case WM_ERASEBKGND
        return true      ' the double buffer paints every pixel

    case WM_SIZE
        if pSB then
            pSB->Recalc()
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_SHOWWINDOW
        ' Hidden (e.g. auto-hide) while the mouse was over the thumb: drop the hover
        ' state, or it would reappear stale the next time we are shown.
        if (pSB <> 0) andalso (wParam = 0) then
            CHScrollBar_ClearHot( pSB )
            CHScrollBar_StopRepeat( pSB )
            pSB->isDragging = false
        end if

    case WM_LBUTTONDOWN
        if pSB = 0 then return 0
        if pSB->NeedsThumb() = false then return 0     ' nothing to scroll
        pSB->Recalc()
        dim as POINT pt
        pt.x = GET_X_LPARAM(lParam) : pt.y = GET_Y_LPARAM(lParam)
        if PtInRect( @pSB->rcThumb, pt ) then
            ' Grab the thumb. Remember where inside the thumb we grabbed it so the
            ' thumb tracks the cursor without jumping.
            pSB->isDragging = true
            pSB->dragOffset = pt.x - pSB->rcThumb.left
            SetCapture( hwnd )
            InvalidateRect( hwnd, NULL, FALSE )
        else
            ' Track click: page once now, then auto-repeat while held.
            pSB->repeatDir = iif( pt.x < pSB->rcThumb.left, -1, 1 )
            CHScrollBar_PageStep( pSB, pSB->repeatDir )
            SetCapture( hwnd )
            SetTimer( hwnd, IDT_CHSCROLL_REPEAT, CHSCROLL_REPEAT_DELAY, NULL )
            pSB->repeatTimerOn = true
        end if
        return 0

    case WM_MOUSEMOVE
        if pSB = 0 then return 0
        dim as POINT pt
        pt.x = GET_X_LPARAM(lParam) : pt.y = GET_Y_LPARAM(lParam)

        if pSB->isDragging then
            ' Derive the position from the cursor, then rebuild the thumb FROM that
            ' position -- the thumb and the owner can never drift apart.
            ' (GET_X_LPARAM is signed: while we hold capture, x < 0 is the NORMAL
            ' case of dragging left of the client, not an edge case.)
            dim as integer newPos = pSB->PosFromThumbLeft( pt.x - pSB->dragOffset )
            if newPos <> pSB->nPos then
                pSB->nPos = newPos
                pSB->Recalc()
                InvalidateRect( hwnd, NULL, FALSE )
                if pSB->ScrollCallback then pSB->ScrollCallback( hwnd, newPos )
            end if
        else
            ' Hover tracking. WM_MOUSELEAVE is not reliably delivered on fast exits,
            ' so arm a poll timer as a safety net (same approach as the siblings).
            dim tme as TRACKMOUSEEVENT
            tme.cbSize    = sizeof(TRACKMOUSEEVENT)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            if pSB->hotTimerOn = false then
                SetTimer( hwnd, IDT_CHSCROLL_HOTTRACK, CHSCROLL_HOTTRACK_MS, NULL )
                pSB->hotTimerOn = true
            end if

            dim as boolean hot = false
            if pSB->NeedsThumb() then
                pSB->Recalc()
                if PtInRect( @pSB->rcThumb, pt ) then hot = true
            end if
            if hot <> pSB->isHot then
                pSB->isHot = hot
                InvalidateRect( hwnd, NULL, FALSE )
            end if
        end if
        return 0

    case WM_LBUTTONUP
        if pSB then
            if pSB->isDragging orelse pSB->repeatTimerOn then ReleaseCapture()
            pSB->isDragging = false
            CHScrollBar_StopRepeat( pSB )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_CAPTURECHANGED
        ' Capture lost (someone else took it): never leave a drag/repeat stranded.
        if pSB then
            pSB->isDragging = false
            CHScrollBar_StopRepeat( pSB )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_MOUSELEAVE
        CHScrollBar_ClearHot( pSB )
        return 0

    case WM_MOUSEHWHEEL
        ' A horizontal two-finger trackpad swipe (or a tilt wheel) with the cursor over the
        ' bar. All the work -- and the inverted sign relative to CVScrollBar -- lives in
        ' HandleWheelDelta, which a host can also call to route a swipe made over the
        ' content. The signed-word cast is not optional: hiword is unsigned (Learnings.md).
        '
        ' Reaching us at all depends on "scroll inactive windows when I hover over them"
        ' (on by default), since this control never takes focus -- see CHScrollBar.bi.
        '
        ' There is deliberately no WM_MOUSEWHEEL case here: a horizontal bar answers
        ' horizontal gestures only.
        if pSB then
            CHScrollBar_HandleWheelDelta( hwnd, cast(short, hiword(wParam)) )
            return 0                    ' we own this gesture, moved or not
        end if
        ' No instance: fall through to DefWindowProc so the message still bubbles up.

    case WM_TIMER
        if pSB = 0 then exit select

        if wParam = IDT_CHSCROLL_HOTTRACK then
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then CHScrollBar_ClearHot( pSB )
            return 0
        end if

        if wParam = IDT_CHSCROLL_REPEAT then
            ' Switch from the initial delay to the fast repeat cadence.
            SetTimer( hwnd, IDT_CHSCROLL_REPEAT, CHSCROLL_REPEAT_MS, NULL )
            dim as POINT pt : GetCursorPos( @pt ) : ScreenToClient( hwnd, @pt )
            pSB->Recalc()
            ' Stop once the thumb has travelled far enough to reach the cursor
            ' (standard scrollbar behavior), or when there is nowhere left to go.
            if PtInRect( @pSB->rcThumb, pt ) then
                CHScrollBar_StopRepeat( pSB )
            elseif CHScrollBar_PageStep( pSB, pSB->repeatDir ) = false then
                CHScrollBar_StopRepeat( pSB )
            end if
            return 0
        end if

    case WM_NCDESTROY
        if pSB then
            CHScrollBar_StopRepeat( pSB )
            if pSB->hotTimerOn then KillTimer( hwnd, IDT_CHSCROLL_HOTTRACK )
            delete(pSB)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' CHScrollBar_Create. The control starts hidden; the owner shows it once it knows
' whether there is anything to scroll.
' ========================================================================================
function CHScrollBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND
    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hSB = _
        pWindow->Create( hWndParent, "", @CHScrollBar_WndProc, _
                         0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN )

    dim as CHSCROLLBAR ptr pSB = new CHSCROLLBAR
    pSB->hWin = hSB
    pWindow->UserData(0) = cast(LONG_PTR, pSB)
    SetWindowLongPtr( hSB, GWLP_ID, CtrlID )

    function = hSB
end function


' ========================================================================================
' Set the scroll range. total = number of units, page = units visible at once,
' pos = index of the first visible unit. Clamped and repainted.
' ========================================================================================
sub CHScrollBar_SetRange( byval hSB as HWND, byval nTotal as integer, byval nPage as integer, byval nPosition as integer )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    if nTotal < 0 then nTotal = 0
    if nPage  < 0 then nPage  = 0
    pSB->nTotal = nTotal
    pSB->nPage  = nPage
    if nPosition < 0 then nPosition = 0
    if nPosition > pSB->MaxPos() then nPosition = pSB->MaxPos()
    pSB->nPos = nPosition
    pSB->Recalc()
    InvalidateRect( hSB, NULL, FALSE )
end sub


function CHScrollBar_GetPos( byval hSB as HWND ) as integer
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return 0
    return pSB->nPos
end function


' Set the position without invoking the scroll callback (this is the owner telling us
' where it scrolled to, not the user scrolling).
sub CHScrollBar_SetPos( byval hSB as HWND, byval nPosition as integer )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    if nPosition < 0 then nPosition = 0
    if nPosition > pSB->MaxPos() then nPosition = pSB->MaxPos()
    if nPosition = pSB->nPos then return
    pSB->nPos = nPosition
    pSB->Recalc()
    InvalidateRect( hSB, NULL, FALSE )
end sub


function CHScrollBar_NeedsThumb( byval hSB as HWND ) as boolean
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return false
    return pSB->NeedsThumb()
end function


' Is the user actively working the bar with the mouse captured (thumb drag or track
' auto-repeat)? Hosts that auto-hide the bar on mouse-leave need this: with capture
' held the cursor legitimately roams outside the bar mid-gesture, and hiding it then
' would cancel the drag under the user's finger.
function CHScrollBar_IsDragging( byval hSB as HWND ) as boolean
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return false
    return (pSB->isDragging = true) orelse (pSB->repeatTimerOn = true)
end function


sub CHScrollBar_SetColors( byval hSB as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->BackColor    = backclr
    pSB->ForeColor    = foreclr
    pSB->ForeColorHot = foreclrhot
    InvalidateRect( hSB, NULL, FALSE )
end sub


sub CHScrollBar_SetPaintCallback( byval hSB as HWND, byval usersub as HScrollPaintCallbackSub )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->PaintCallback = usersub
    InvalidateRect( hSB, NULL, FALSE )
end sub


sub CHScrollBar_SetScrollCallback( byval hSB as HWND, byval usersub as HScrollCallbackSub )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->ScrollCallback = usersub
end sub


' ========================================================================================
' How many range units one 120-unit wheel notch scrolls. 0 = follow the system setting.
'
' CHANGING it resets any part-accumulated delta: the remainder was measured against the old
' step and would translate to the wrong distance under the new one.
'
' Setting it to the value it ALREADY HAS is a no-op that keeps the remainder -- and that
' matters more than it looks. A host whose units are neither lines nor characters has to
' convert (tiko turns "3 characters" into pixels), and the natural home for that is its
' per-layout range-push, which re-runs constantly -- including while a slow trackpad swipe
' is still accumulating. Resetting on every such call would discard every sub-notch
' remainder and put the dead-then-jump behaviour straight back.
' ========================================================================================
sub CHScrollBar_SetWheelStep( byval hSB as HWND, byval nUnitsPerNotch as integer )
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return
    if nUnitsPerNotch < 0 then nUnitsPerNotch = 0
    if pSB->nWheelStep = nUnitsPerNotch then return   ' no-op: keep the remainder
    pSB->nWheelStep  = nUnitsPerNotch
    pSB->nWheelAccum = 0
end sub


function CHScrollBar_GetWheelStep( byval hSB as HWND ) as integer
    dim as CHSCROLLBAR ptr pSB = GetHScrollBarPointer(hSB)
    if pSB = 0 then return 0
    return pSB->nWheelStep
end function
