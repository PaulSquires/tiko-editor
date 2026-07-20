''
''  CVScrollBar.inc  -- owner-drawn vertical scrollbar
''

#include once "CVScrollBar.bi"


function GetScrollBarPointer( byval hSB as HWND ) as CVSCROLLBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hSB)
    if pWindow = 0 then return 0
    return cast( CVSCROLLBAR ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Move by one page in the given direction (-1 up / +1 down), notifying the owner.
' Returns TRUE if the position actually changed.
' ========================================================================================
function CVScrollBar_PageStep( byval pSB as CVSCROLLBAR ptr, byval nDir as integer ) as boolean
    if pSB = 0 then return false
    dim as integer newPos = pSB->nPos + (nDir * pSB->nPage)
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
' Stop the track auto-repeat.
' ========================================================================================
sub CVScrollBar_StopRepeat( byval pSB as CVSCROLLBAR ptr )
    if pSB = 0 then return
    if pSB->repeatTimerOn then
        KillTimer( pSB->hWin, IDT_CVSCROLL_REPEAT )
        pSB->repeatTimerOn = false
    end if
    pSB->repeatDir = 0
end sub


' ========================================================================================
' Clear the thumb hover state and stop the hover poll timer. Repaints only if the
' state actually changes, so this is cheap to call repeatedly.
' ========================================================================================
sub CVScrollBar_ClearHot( byval pSB as CVSCROLLBAR ptr )
    if pSB = 0 then return
    if pSB->hotTimerOn then
        KillTimer( pSB->hWin, IDT_CVSCROLL_HOTTRACK )
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
function CVScrollBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hwnd)
    if pSB = 0 then return 0
    pSB->Recalc()

    dim as clsDoubleBuffer b
    b.BeginDoubleBuffer(hwnd)

    dim as CVSCROLLBAR_PAINTINFO p
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
' CVScrollBar Window procedure
' ========================================================================================
function CVScrollBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hwnd)

    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT, CVScrollBar_OnPaint)

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
            CVScrollBar_ClearHot( pSB )
            CVScrollBar_StopRepeat( pSB )
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
            pSB->dragOffset = pt.y - pSB->rcThumb.top
            SetCapture( hwnd )
            InvalidateRect( hwnd, NULL, FALSE )
        else
            ' Track click: page once now, then auto-repeat while held.
            pSB->repeatDir = iif( pt.y < pSB->rcThumb.top, -1, 1 )
            CVScrollBar_PageStep( pSB, pSB->repeatDir )
            SetCapture( hwnd )
            SetTimer( hwnd, IDT_CVSCROLL_REPEAT, CVSCROLL_REPEAT_DELAY, NULL )
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
            dim as integer newPos = pSB->PosFromThumbTop( pt.y - pSB->dragOffset )
            if newPos <> pSB->nPos then
                pSB->nPos = newPos
                pSB->Recalc()
                InvalidateRect( hwnd, NULL, FALSE )
                if pSB->ScrollCallback then pSB->ScrollCallback( hwnd, newPos )
            end if
        else
            ' Hover tracking. WM_MOUSELEAVE is not reliably delivered on fast exits,
            ' so arm a poll timer as a safety net (same approach as CListBox).
            dim tme as TRACKMOUSEEVENT
            tme.cbSize    = sizeof(TRACKMOUSEEVENT)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            if pSB->hotTimerOn = false then
                SetTimer( hwnd, IDT_CVSCROLL_HOTTRACK, CVSCROLL_HOTTRACK_MS, NULL )
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
            CVScrollBar_StopRepeat( pSB )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_CAPTURECHANGED
        ' Capture lost (someone else took it): never leave a drag/repeat stranded.
        if pSB then
            pSB->isDragging = false
            CVScrollBar_StopRepeat( pSB )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_MOUSELEAVE
        CVScrollBar_ClearHot( pSB )
        return 0

    case WM_TIMER
        if pSB = 0 then exit select

        if wParam = IDT_CVSCROLL_HOTTRACK then
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then CVScrollBar_ClearHot( pSB )
            return 0
        end if

        if wParam = IDT_CVSCROLL_REPEAT then
            ' Switch from the initial delay to the fast repeat cadence.
            SetTimer( hwnd, IDT_CVSCROLL_REPEAT, CVSCROLL_REPEAT_MS, NULL )
            dim as POINT pt : GetCursorPos( @pt ) : ScreenToClient( hwnd, @pt )
            pSB->Recalc()
            ' Stop once the thumb has travelled far enough to reach the cursor
            ' (standard scrollbar behavior), or when there is nowhere left to go.
            if PtInRect( @pSB->rcThumb, pt ) then
                CVScrollBar_StopRepeat( pSB )
            elseif CVScrollBar_PageStep( pSB, pSB->repeatDir ) = false then
                CVScrollBar_StopRepeat( pSB )
            end if
            return 0
        end if

    case WM_NCDESTROY
        if pSB then
            CVScrollBar_StopRepeat( pSB )
            if pSB->hotTimerOn then KillTimer( hwnd, IDT_CVSCROLL_HOTTRACK )
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
' CVScrollBar_Create. The control starts hidden; the owner shows it once it knows
' whether there is anything to scroll.
' ========================================================================================
function CVScrollBar_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND
    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hSB = _
        pWindow->Create( hWndParent, "", @CVScrollBar_WndProc, _
                         0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN )

    dim as CVSCROLLBAR ptr pSB = new CVSCROLLBAR
    pSB->hWin = hSB
    pWindow->UserData(0) = cast(LONG_PTR, pSB)
    SetWindowLongPtr( hSB, GWLP_ID, CtrlID )

    function = hSB
end function


' ========================================================================================
' Set the scroll range. total = number of units, page = units visible at once,
' pos = index of the first visible unit. Clamped and repainted.
' ========================================================================================
sub CVScrollBar_SetRange( byval hSB as HWND, byval nTotal as integer, byval nPage as integer, byval nPosition as integer )
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
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


function CVScrollBar_GetPos( byval hSB as HWND ) as integer
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return 0
    return pSB->nPos
end function


' Set the position without invoking the scroll callback (this is the owner telling us
' where it scrolled to, not the user scrolling).
sub CVScrollBar_SetPos( byval hSB as HWND, byval nPosition as integer )
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return
    if nPosition < 0 then nPosition = 0
    if nPosition > pSB->MaxPos() then nPosition = pSB->MaxPos()
    if nPosition = pSB->nPos then return
    pSB->nPos = nPosition
    pSB->Recalc()
    InvalidateRect( hSB, NULL, FALSE )
end sub


function CVScrollBar_NeedsThumb( byval hSB as HWND ) as boolean
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return false
    return pSB->NeedsThumb()
end function


sub CVScrollBar_SetColors( byval hSB as HWND, byval backclr as COLORREF, byval foreclr as COLORREF, byval foreclrhot as COLORREF )
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->BackColor    = backclr
    pSB->ForeColor    = foreclr
    pSB->ForeColorHot = foreclrhot
    InvalidateRect( hSB, NULL, FALSE )
end sub


sub CVScrollBar_SetPaintCallback( byval hSB as HWND, byval usersub as VScrollPaintCallbackSub )
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->PaintCallback = usersub
    InvalidateRect( hSB, NULL, FALSE )
end sub


sub CVScrollBar_SetScrollCallback( byval hSB as HWND, byval usersub as VScrollCallbackSub )
    dim as CVSCROLLBAR ptr pSB = GetScrollBarPointer(hSB)
    if pSB = 0 then return
    pSB->ScrollCallback = usersub
end sub
