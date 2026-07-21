''
''  CSplitter.inc  -- owner-drawn splitter bar
''

#include once "CSplitter.bi"


function GetSplitterPointer( byval hSplit as HWND ) as CSPLITTER ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hSplit)
    if pWindow = 0 then return 0
    return cast( CSPLITTER ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
' Callers decide whether to honor that answer -- WM_LBUTTONUP deliberately does not.
' ========================================================================================
function CSplitter_CallMessage( _
            byval pSpl   as CSPLITTER ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pSpl = 0 then return false
    if pSpl->MessageCallback = 0 then return false

    dim as CSPLITTER_MESSAGEINFO m
    m.hSplitter = hwnd
    m.uMsg      = uMsg
    m.wParam    = wParam
    m.lParam    = lParam
    return pSpl->MessageCallback( @m )
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
sub CSplitter_ClearHot( byval pSpl as CSPLITTER ptr )
    if pSpl = 0 then return
    if pSpl->hotTimerOn then
        KillTimer( pSpl->hWin, IDT_CSPLITTER_HOTTRACK )
        pSpl->hotTimerOn = false
    end if
    if pSpl->isHot then
        pSpl->isHot = false
        InvalidateRect( pSpl->hWin, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Arm the hover poll timer if it is not already running. WM_MOUSELEAVE is not reliably
' delivered on fast exits, so this periodic cursor check is the safety net.
' ========================================================================================
sub CSplitter_ArmHotTimer( byval pSpl as CSPLITTER ptr )
    if pSpl = 0 then return
    if pSpl->hotTimerOn then return
    SetTimer( pSpl->hWin, IDT_CSPLITTER_HOTTRACK, CSPLITTER_HOTTRACK_MS, NULL )
    pSpl->hotTimerOn = true
end sub


' ========================================================================================
' End a drag, keeping the position the user dragged to. Shared by WM_LBUTTONUP and
' WM_CAPTURECHANGED -- a stolen capture is treated as an implicit button-up, because the
' host has already applied every intermediate position and has nothing to undo.
'
' No-op when no drag is in progress, so the WM_CAPTURECHANGED that ReleaseCapture() itself
' generates cannot fire a second END notification.
'
' The END notification goes out LAST, after the state is consistent: a host that queries
' CSplitter_IsDragging() from inside the callback must see FALSE.
' ========================================================================================
sub CSplitter_EndDrag( byval pSpl as CSPLITTER ptr )
    if pSpl = 0 then return
    if pSpl->isDragging = false then return
    pSpl->isDragging = false

    ' The hover pin is released with the drag: hover was held true for the whole gesture
    ' (the cursor legitimately roams outside the client under capture), so re-derive it
    ' from where the cursor actually ended up.
    dim as RECT rcClient
    GetClientRect( pSpl->hWin, @rcClient )
    pSpl->isHot = isMouseOverRECT( pSpl->hWin, rcClient )
    if pSpl->isHot then
        CSplitter_ArmHotTimer( pSpl )
    else
        if pSpl->hotTimerOn then
            KillTimer( pSpl->hWin, IDT_CSPLITTER_HOTTRACK )
            pSpl->hotTimerOn = false
        end if
    end if

    InvalidateRect( pSpl->hWin, NULL, FALSE )
    pSpl->Notify( CSPLITTER_PHASE_END )
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
function CSplitter_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hwnd)
    if pSpl = 0 then return 0

    dim as clsDoubleBuffer b
    b.BeginDoubleBuffer(hwnd)

    dim as CSPLITTER_PAINTINFO p
    p.hSplitter    = hwnd
    p.b            = @b
    GetClientRect( hwnd, @p.rcClient )
    p.nOrientation = pSpl->nOrientation
    p.isHot        = pSpl->isHot
    p.isDragging   = pSpl->isDragging

    if pSpl->PaintCallback then
        pSpl->PaintCallback( @p )
    else
        ' Default paint: a flat bar, brighter while hot or dragging.
        dim as COLORREF clr = pSpl->BackColor
        if pSpl->isHot orelse pSpl->isDragging then clr = pSpl->HotColor
        b.SetBackColor( clr )
        b.PaintClientRect()
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' CSplitter Window procedure
' ========================================================================================
function CSplitter_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hwnd)

    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT, CSplitter_OnPaint)

    case WM_ERASEBKGND
        return true      ' the double buffer paints every pixel

    case WM_SIZE
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_WINDOWPOSCHANGED
        ' The HOST moved us as part of its own layout pass (parent resize, pane collapse,
        ' a manual SetWindowPos). Record it: without this, GetPos and the bar's actual
        ' location drift apart. This is an observation, not a change -- nothing is
        ' clamped and nothing is notified. Skipped mid-drag, where WE are the mover.
        if pSpl then
            if pSpl->isDragging = false then
                pSpl->nPos = pSpl->BarEdgeFromWindow()
            end if
        end if
        ' fall through to DefWindowProc so WM_MOVE/WM_SIZE are still generated

    case WM_SETCURSOR
        if pSpl = 0 then exit select
        ' Under capture the hit-test code can be anything (the cursor is over the panes,
        ' or off the window entirely), so mid-drag we own the cursor unconditionally.
        dim as boolean bClient = (loword(lParam) = HTCLIENT)
        if (pSpl->isDragging = false) andalso (bClient = false) then exit select
        if CSplitter_CallMessage( pSpl, hwnd, uMsg, wParam, lParam ) then return true
        if pSpl->hBarCursor then
            SetCursor( pSpl->hBarCursor )
            return true
        end if
        exit select

    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        ' WM_LBUTTONDBLCLK REPLACES the second WM_LBUTTONDOWN (the class carries
        ' CS_DBLCLKS): the sequence is down, up, dblclk, up. So the dblclk must run the
        ' SAME capture bookkeeping as the down, or the trailing up would release a capture
        ' that was never taken. See C:\dev\Learnings.md.
        if pSpl = 0 then return 0
        ' Capture goes first, before any host callback runs, so the down->up pairing is
        ' guaranteed no matter what the host does with the message.
        SetCapture( hwnd )
        if CSplitter_CallMessage( pSpl, hwnd, uMsg, wParam, lParam ) then return 0

        pSpl->isDragging = true
        pSpl->isHot      = true      ' pinned for the whole gesture
        ' Absolute grab offset: the distance from the bar's leading edge to the cursor, in
        ' the parent's coordinate space. Every subsequent move recomputes the position from
        ' the cursor and this offset -- never by accumulating deltas, which would make the
        ' clamps sticky.
        pSpl->dragOffset = pSpl->CursorAxisInParent() - pSpl->nPos
        InvalidateRect( hwnd, NULL, FALSE )
        pSpl->Notify( CSPLITTER_PHASE_BEGIN )
        return 0

    case WM_MOUSEMOVE
        if pSpl = 0 then return 0
        if CSplitter_CallMessage( pSpl, hwnd, uMsg, wParam, lParam ) then return 0

        if pSpl->isDragging then
            dim as integer newPos = _
                pSpl->ClampPos( pSpl->CursorAxisInParent() - pSpl->dragOffset )
            if newPos <> pSpl->nPos then
                pSpl->nPos = newPos
                pSpl->MoveBar()          ' no repaint needed: the content did not change
                pSpl->Notify( CSPLITTER_PHASE_MOVE )
            end if
        else
            ' Hover. The whole client IS the bar, so any move that reaches us (outside a
            ' drag) means the cursor is on it.
            dim tme as TRACKMOUSEEVENT
            tme.cbSize    = sizeof(TRACKMOUSEEVENT)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            CSplitter_ArmHotTimer( pSpl )
            if pSpl->isHot = false then
                pSpl->isHot = true
                InvalidateRect( hwnd, NULL, FALSE )
            end if
        end if
        return 0

    case WM_LBUTTONUP
        if pSpl = 0 then return 0
        ' The callback's answer is deliberately IGNORED here: this up-message is the
        ' capture's exit, and a host allowed to suppress it would strand the capture and
        ' swallow the next click anywhere in the app.
        CSplitter_CallMessage( pSpl, hwnd, uMsg, wParam, lParam )
        ' End the drag BEFORE releasing, so the WM_CAPTURECHANGED that ReleaseCapture
        ' generates finds nothing to do and cannot fire a second END.
        CSplitter_EndDrag( pSpl )
        if GetCapture() = hwnd then ReleaseCapture()
        return 0

    case WM_CAPTURECHANGED
        ' Capture lost to someone else mid-drag. Keep the position the user dragged to and
        ' close the gesture cleanly.
        CSplitter_EndDrag( pSpl )
        return 0

    case WM_MOUSELEAVE
        ' Stand down while dragging: under capture the cursor legitimately leaves the
        ' client, and clearing hover there would strip the bar's active visuals mid-drag.
        if pSpl = 0 then return 0
        if pSpl->isDragging = false then CSplitter_ClearHot( pSpl )
        return 0

    case WM_SHOWWINDOW
        ' Hidden (e.g. a collapsed pane) while the mouse was over us: drop the hover state
        ' or it would reappear stale the next time we are shown.
        if pSpl = 0 then exit select
        if wParam = 0 then
            CSplitter_EndDrag( pSpl )
            CSplitter_ClearHot( pSpl )
        end if
        exit select

    case WM_TIMER
        if pSpl = 0 then exit select
        if wParam = IDT_CSPLITTER_HOTTRACK then
            if pSpl->isDragging = false then
                dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then CSplitter_ClearHot( pSpl )
            end if
            return 0
        end if

    case WM_NCDESTROY
        if pSpl then
            if pSpl->hotTimerOn then KillTimer( hwnd, IDT_CSPLITTER_HOTTRACK )
            if GetCapture() = hwnd then ReleaseCapture()
            delete(pSpl)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' CSplitter_Create. The control starts hidden and zero-sized; the host positions, sizes
' and shows it with SetWindowPos. The size it gives us along the drag axis IS the grab
' area -- there is no separate hit margin.
' ========================================================================================
function CSplitter_Create( _
            byval hWndParent   as HWND, _
            byval CtrlID       as integer, _
            byval nOrientation as integer = CSPLITTER_VERTICAL _
            ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    dim as HWND hSplit = _
        pWindow->Create( hWndParent, "", @CSplitter_WndProc, _
                         0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN )

    ' Enable double-click messages (WM_LBUTTONDBLCLK) on the control's window class,
    ' preserving the class's existing styles rather than replacing them. (CWindow's
    ' ClassStyle setter REPLACES, so this must be read-modify-write, and it needs
    ' m_hwnd -- i.e. it only works after Create.)
    pWindow->ClassStyle = pWindow->ClassStyle or CS_DBLCLKS

    dim as CSPLITTER ptr pSpl = new CSPLITTER
    pSpl->hWin    = hSplit
    pSpl->hParent = hWndParent

    if nOrientation <> CSPLITTER_HORIZONTAL then nOrientation = CSPLITTER_VERTICAL
    pSpl->nOrientation = nOrientation

    ' Shared system cursors -- never DestroyCursor these.
    if nOrientation = CSPLITTER_VERTICAL then
        pSpl->hBarCursor = LoadCursor( null, IDC_SIZEWE )
    else
        pSpl->hBarCursor = LoadCursor( null, IDC_SIZENS )
    end if

    pWindow->UserData(0) = cast(LONG_PTR, pSpl)
    SetWindowLongPtr( hSplit, GWLP_ID, CtrlID )

    function = hSplit
end function


function CSplitter_GetOrientation( byval hSplit as HWND ) as integer
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return CSPLITTER_VERTICAL
    return pSpl->nOrientation
end function


function CSplitter_GetPos( byval hSplit as HWND ) as integer
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return 0
    return pSpl->nPos
end function


' Set the position without invoking the position callback (this is the host telling us
' where it put the splitter, not the user moving it).
sub CSplitter_SetPos( byval hSplit as HWND, byval nPosition as integer )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    nPosition = pSpl->ClampPos( nPosition )
    if nPosition = pSpl->nPos then return
    pSpl->nPos = nPosition
    pSpl->MoveBar()
    InvalidateRect( hSplit, NULL, FALSE )
end sub


' Set the drag limits, in the same parent-client space as the position. Clamps the current
' position into the new range -- silently, because the host set the range and already knows.
sub CSplitter_SetRange( byval hSplit as HWND, byval nMin as integer, byval nMax as integer )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    pSpl->nMin = nMin
    pSpl->nMax = nMax
    dim as integer nClamped = pSpl->ClampPos( pSpl->nPos )
    if nClamped <> pSpl->nPos then
        pSpl->nPos = nClamped
        pSpl->MoveBar()
        InvalidateRect( hSplit, NULL, FALSE )
    end if
end sub


function CSplitter_GetMin( byval hSplit as HWND ) as integer
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return 0
    return pSpl->nMin
end function


function CSplitter_GetMax( byval hSplit as HWND ) as integer
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return 0
    return pSpl->nMax
end function


function CSplitter_IsHot( byval hSplit as HWND ) as boolean
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return false
    return pSpl->isHot
end function


function CSplitter_IsDragging( byval hSplit as HWND ) as boolean
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return false
    return pSpl->isDragging
end function


function CSplitter_GetBackColor( byval hSplit as HWND ) as COLORREF
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return 0
    return pSpl->BackColor
end function


sub CSplitter_SetColors( byval hSplit as HWND, byval backclr as COLORREF, byval hotclr as COLORREF )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    pSpl->BackColor = backclr
    pSpl->HotColor  = hotclr
    InvalidateRect( hSplit, NULL, FALSE )
end sub


sub CSplitter_SetPaintCallback( byval hSplit as HWND, byval usersub as SPL_PaintCallbackSub )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    pSpl->PaintCallback = usersub
    InvalidateRect( hSplit, NULL, FALSE )
end sub


sub CSplitter_SetMessageCallback( byval hSplit as HWND, byval userfunc as SPL_MessageCallbackFunc )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    pSpl->MessageCallback = userfunc
end sub


sub CSplitter_SetPosChangedCallback( byval hSplit as HWND, byval usersub as SPL_PosChangedCallbackSub )
    dim as CSPLITTER ptr pSpl = GetSplitterPointer(hSplit)
    if pSpl = 0 then return
    pSpl->PosChangedCallback = usersub
end sub
