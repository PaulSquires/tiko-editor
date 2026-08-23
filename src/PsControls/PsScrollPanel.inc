''
''  PsScrollPanel.inc  -- scrolling panel (viewport + page + auto-hiding scrollbar)
''

#include once "PsScrollPanel.bi"


' The container owns the state. The panel reaches it through GetParent rather than keeping
' a second copy of the pointer -- one owner, nothing to keep in step (PsListBox's pattern
' for its own scrollbar callback).
function GetScrollPanelPointer( byval hScp as HWND ) as PSSCROLLPANEL ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hScp)
    if pWindow = 0 then return 0
    return cast( PSSCROLLPANEL ptr, pWindow->UserData(0) )
end function

function GetScrollPanelPointerFromPanel( byval hPanel as HWND ) as PSSCROLLPANEL ptr
    return GetScrollPanelPointer( GetParent(hPanel) )
end function


' ========================================================================================
' Geometry trace, gated on PSSCROLLPANEL_TRACE=1. The environment is read once.
' ========================================================================================
function PsScrollPanel_TraceOn() as boolean
    static as boolean bChecked = false
    static as boolean bEnabled = false
    if bChecked = false then
        bChecked = true
        bEnabled = (environ("PSSCROLLPANEL_TRACE") = "1")
    end if
    return bEnabled
end function


' ========================================================================================
' The reveal rule. A pure function of three booleans so the truth table is assertable
' without a real mouse, a real focus or a real window -- that is why it is not a method.
' ========================================================================================
function PsScrollPanel_ShouldShow( _
            byval bNeeds as boolean, _
            byval bMouse as boolean, _
            byval bFocus as boolean _
            ) as boolean

    if bNeeds = false then return false        ' nothing to scroll: never show a thumb
    return (bMouse orelse bFocus)              ' either alone is enough
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it, in
' which case lResult carries the value the window procedure must return.
' ========================================================================================
function PsScrollPanel_CallMessage( _
            byval pScp    as PSSCROLLPANEL ptr, _
            byval hFrom   as HWND, _
            byval uMsg    as UINT, _
            byval wParam  as WPARAM, _
            byval lParam  as LPARAM, _
            byref lResult as LRESULT _
            ) as boolean

    lResult = 0
    if pScp = 0 then return false
    if pScp->MessageCallback = 0 then return false

    dim as SCP_MESSAGEINFO m
    m.hScrollPanel = pScp->hWin
    m.hPanel       = pScp->hPanel
    m.hFrom        = hFrom
    m.uMsg         = uMsg
    m.wParam       = wParam
    m.lParam       = lParam
    m.lResult      = 0

    if pScp->MessageCallback( @m ) = false then return false
    lResult = m.lResult
    return true
end function


' ========================================================================================
' Move to nNewPos (already clamped by the caller or not -- this clamps again), apply it to
' the page, and notify only when the move came from the USER.
'
' THE one place nPos changes. Every path -- wheel, thumb, host setter, focus-into-view --
' routes through it, so the page and the bar can never drift apart.
' ========================================================================================
function PsScrollPanel_ScrollTo( _
            byval pScp    as PSSCROLLPANEL ptr, _
            byval nNewPos as integer, _
            byval bNotify as boolean _
            ) as boolean

    if pScp = 0 then return false
    nNewPos = pScp->ClampPos( nNewPos )
    if nNewPos = pScp->nPos then return false

    pScp->nPos = nNewPos
    pScp->ApplyScroll()

    ' Mirror the position back into the bar unless the bar is where it came from. SetPos is
    ' silent there too, so this cannot re-enter.
    if pScp->hScrollBar then
        if PsVScrollBar_GetPos( pScp->hScrollBar ) <> nNewPos then
            PsVScrollBar_SetPos( pScp->hScrollBar, nNewPos )
        end if
    end if

    if bNotify andalso (pScp->ScrollCallback <> 0) then
        pScp->ScrollCallback( pScp->hWin, nNewPos )
    end if
    return true
end function


' ========================================================================================
' Arm / kill the reveal poll timer.
'
' A poll, not TrackMouseEvent, and not for the usual reason. The cursor can enter this
' control directly over one of the HOST's child controls -- an EDIT, a checkbox -- in which
' case neither the container nor the panel ever receives a WM_MOUSEMOVE at all, so there is
' no message to hang a TME_LEAVE request off. A periodic cursor test sees it regardless.
' The same tick carries the focus-change detection.
' ========================================================================================
sub PsScrollPanel_ArmRevealTimer( byval pScp as PSSCROLLPANEL ptr )
    if pScp = 0 then return
    if pScp->revealTimerOn then return
    SetTimer( pScp->hWin, IDT_CSCROLLPANEL_REVEAL, PSSCROLLPANEL_REVEAL_MS, NULL )
    pScp->revealTimerOn = true
end sub

sub PsScrollPanel_KillRevealTimer( byval pScp as PSSCROLLPANEL ptr )
    if pScp = 0 then return
    if pScp->revealTimerOn = false then return
    KillTimer( pScp->hWin, IDT_CSCROLLPANEL_REVEAL )
    pScp->revealTimerOn = false
end sub


' ========================================================================================
' Apply the reveal decision. Shows or hides the BAR WINDOW only -- its strip stays reserved
' either way, so nothing reflows and host content never jumps sideways. ShowWindow is
' called only on a real change, so this is cheap to call from a 100 ms timer.
' ========================================================================================
sub PsScrollPanel_ApplyReveal( byval pScp as PSSCROLLPANEL ptr )
    if pScp = 0 then return
    if pScp->hScrollBar = 0 then return

    dim as boolean bShow = PsScrollPanel_ShouldShow( pScp->NeedsThumb(), _
                                                    pScp->MouseIsOver(), _
                                                    pScp->FocusIsInside() )
    ' Never yank the thumb out from under a drag in progress: the user is holding it, which
    ' the cursor test would deny the moment the pointer left the strip sideways.
    if GetCapture() = pScp->hScrollBar then bShow = true

    if bShow = pScp->bThumbShown then return
    pScp->bThumbShown = bShow
    ShowWindow( pScp->hScrollBar, iif( bShow, SW_SHOWNA, SW_HIDE ) )
end sub


' ========================================================================================
' Push the current model into the scrollbar and re-evaluate the reveal rule. Called from
' every path that can change the content height, the viewport size or the position, which
' is what stops the thumb ever going stale.
' ========================================================================================
sub PsScrollPanel_SyncScrollBar( byval pScp as PSSCROLLPANEL ptr )
    if pScp = 0 then return
    if pScp->hScrollBar = 0 then return

    ' Range units are PIXELS: total = the page, page = the visible slice, pos = the offset.
    PsVScrollBar_SetRange( pScp->hScrollBar, _
                          pScp->nContentHeight, _
                          pScp->ViewportHeight(), _
                          pScp->nPos )

    if pScp->NeedsThumb() then
        PsScrollPanel_ArmRevealTimer( pScp )
    else
        PsScrollPanel_KillRevealTimer( pScp )
    end if
    PsScrollPanel_ApplyReveal( pScp )
end sub


' ========================================================================================
' Full layout pass. THE ORDERING IS LOAD-BEARING:
'
'   1  the page's WIDTH is applied FIRST, so the layout callback re-flows against a panel
'      that is already the width it was told about
'   2  the callback runs, guarded against re-entering this pass
'   3  the height it returned is applied
'   4  the position is re-clamped (a shorter page can strand it past the end)
'   5  the bar takes its strip
'   6  the model is pushed into the bar and the reveal rule re-evaluated
' ========================================================================================
sub PsScrollPanel_PositionWindows( byval hScp as HWND )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if pScp->bInLayout then return          ' re-entered from inside the layout callback

    dim as integer cxPanel = pScp->PanelWidth()
    dim as integer cyView  = pScp->ViewportHeight()

    ' --- 1: width first -----------------------------------------------------------------
    if pScp->hPanel then
        SetWindowPos( pScp->hPanel, NULL, _
                      0, -pScp->nPos, cxPanel, pScp->PanelHeight(), _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    end if

    ' --- 2: the host re-flows and tells us how tall the page now is ---------------------
    if pScp->LayoutCallback then
        pScp->bInLayout = true
        dim as integer nNewHeight = pScp->LayoutCallback( pScp->hPanel, cxPanel )
        pScp->bInLayout = false
        if nNewHeight > 0 then pScp->nContentHeight = nNewHeight
    end if

    ' --- 3 + 4: apply the height, then re-clamp the position ----------------------------
    pScp->nPos = pScp->ClampPos( pScp->nPos )
    if pScp->hPanel then
        SetWindowPos( pScp->hPanel, NULL, _
                      0, -pScp->nPos, cxPanel, pScp->PanelHeight(), _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    end if

    ' --- 5: the reserved strip ----------------------------------------------------------
    ' Sized whether or not the thumb is currently shown -- a 0x0 bar could not derive its
    ' own thumb geometry, and the reveal rule would then show an empty window.
    if pScp->hScrollBar then
        SetWindowPos( pScp->hScrollBar, NULL, _
                      cxPanel, 0, pScp->ScaledBarWidth(), cyView, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    end if

    ' --- 6 --------------------------------------------------------------------------
    PsScrollPanel_SyncScrollBar( pScp )

    if PsScrollPanel_TraceOn() then
        dim as string s = "PsScrollPanel layout  viewport=" & str(pScp->ViewportWidth()) & _
            "x" & str(cyView) & "  panel=(0," & str(-pScp->nPos) & "," & str(cxPanel) & _
            "x" & str(pScp->PanelHeight()) & ")  bar=(" & str(cxPanel) & ",0," & _
            str(pScp->ScaledBarWidth()) & "x" & str(cyView) & ")  content=" & _
            str(pScp->nContentHeight) & "  pos=" & str(pScp->nPos) & "/" & str(pScp->MaxPos()) & _
            "  thumb=" & str(pScp->bThumbShown)
        print s
    end if
end sub


' ========================================================================================
' The owned scrollbar reports that the USER scrolled: move the page there and pass it on.
' ========================================================================================
sub PsScrollPanel_OnScrollBarScroll( byval hSB as HWND, byval newPos as integer )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer( GetParent(hSB) )
    if pScp = 0 then return

    ' Mirror whatever we actually did back into the bar, so the thumb can never show a
    ' position the page is not at. SetPos does not re-enter this callback.
    dim as integer nWanted = pScp->ClampPos( newPos )
    PsScrollPanel_ScrollTo( pScp, nWanted, true )
    if nWanted <> newPos then PsVScrollBar_SetPos( hSB, nWanted )
end sub


' ========================================================================================
' Feed a wheel delta to the page.
'
' The step conversion lives HERE because this control's range is in pixels while the system
' setting counts LINES. Recomputed per message rather than cached, so a Control Panel change
' takes effect immediately; setting the step to the value it already has is a documented
' no-op in PsVScrollBar, so the sub-notch accumulator survives (without that, a slow trackpad
' swipe would reset its remainder on every message and feel dead).
' ========================================================================================
function PsScrollPanel_HandleWheelDelta( byval hScp as HWND, byval nDelta as integer ) as boolean
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return false
    if pScp->hScrollBar = 0 then return false

    dim as ulong sysval = 3
    SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, @sysval, 0 )

    dim as integer nStep
    ' WHEEL_PAGESCROLL (= UINT_MAX) means "one page per notch". FreeBASIC's winuser.bi
    ' declares the SPI_* constants but NOT this one, so it is spelled out -- the same
    ' workaround PsVScrollBar.inc and PsTextBox.inc use.
    if sysval = cast(ulong, &hFFFFFFFF) then
        nStep = pScp->ViewportHeight()
    else
        nStep = cast(integer, sysval) * pScp->ScaledLineHeight()
    end if
    if nStep < 1 then nStep = 1

    PsVScrollBar_SetWheelStep( pScp->hScrollBar, nStep )
    ' The bar owns the sign, the accumulation and the clamping; it calls our scroll callback
    ' back through PsScrollPanel_OnScrollBarScroll, which moves the page.
    return PsVScrollBar_HandleWheelDelta( pScp->hScrollBar, nDelta )
end function


' ========================================================================================
' Scroll the minimum distance that brings hChild fully into view.
'
' bNotify is FALSE for the public API (a host asking for this already knows) and TRUE for
' the focus-into-view scroll, which a user's Tab caused.
' ========================================================================================
function PsScrollPanel_EnsureVisibleEx( _
            byval pScp    as PSSCROLLPANEL ptr, _
            byval hChild  as HWND, _
            byval nMargin as integer, _
            byval bNotify as boolean _
            ) as boolean

    if pScp = 0 then return false
    if hChild = 0 then return false
    if IsWindow(hChild) = 0 then return false
    if IsChild( pScp->hPanel, hChild ) = 0 then return false

    ' The child's rect in PAGE coordinates -- the same space nPos lives in.
    dim as RECT rc
    GetWindowRect( hChild, @rc )
    MapWindowPoints( HWND_DESKTOP, pScp->hPanel, cast( POINT ptr, @rc ), 2 )

    dim as integer nTop    = rc.top - nMargin
    dim as integer nBottom = rc.bottom + nMargin
    dim as integer cyView  = pScp->ViewportHeight()
    dim as integer nNewPos = pScp->nPos

    if (nBottom - nTop) >= cyView then
        ' Taller than the viewport: it cannot be fully revealed, so top-align it. Tested
        ' FIRST -- the "scroll down to reveal the bottom" branch below would otherwise push
        ' the child's top off the screen, which is the wrong end to lose.
        nNewPos = nTop
    elseif nTop < pScp->nPos then
        nNewPos = nTop
    elseif nBottom > (pScp->nPos + cyView) then
        nNewPos = nBottom - cyView
    end if

    return PsScrollPanel_ScrollTo( pScp, nNewPos, bNotify )
end function


' ========================================================================================
' One reveal-timer tick: the hover/focus decision, and the focus-into-view scroll.
' ========================================================================================
sub PsScrollPanel_OnRevealTick( byval pScp as PSSCROLLPANEL ptr )
    if pScp = 0 then return

    dim as HWND hFocus = GetFocus()
    if hFocus <> pScp->hLastFocus then
        dim as HWND hPrev = pScp->hLastFocus
        pScp->hLastFocus = hFocus
        ' Scroll to the newly focused control -- on the CHANGE only. Doing it every tick
        ' would fight the user: scroll the focused control off screen deliberately and it
        ' would be yanked straight back under the cursor.
        if pScp->bAutoScrollToFocus andalso (hFocus <> 0) andalso (hFocus <> hPrev) then
            if IsChild( pScp->hPanel, hFocus ) then
                PsScrollPanel_EnsureVisibleEx( pScp, hFocus, 0, true )
            end if
        end if
    end if

    PsScrollPanel_ApplyReveal( pScp )
end sub


' ========================================================================================
' Paint the PAGE. The buffer covers the whole page rect; Windows has already clipped the
' repaint to whatever slice of it is on screen.
' ========================================================================================
function PsScrollPanel_OnPanelPaint( byval hwnd as HWND ) as LRESULT
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointerFromPanel(hwnd)
    if pScp = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    dim as SCP_PAINTINFO p
    p.hScrollPanel = pScp->hWin
    p.hPanel       = hwnd
    p.b            = @b
    GetClientRect( hwnd, @p.rcClient )
    p.nPos         = pScp->nPos

    if pScp->PaintCallback then
        pScp->PaintCallback( @p )
    else
        b.SetBackColor( pScp->BackColor )
        b.PaintClientRect()
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Paint the CONTAINER. With WS_CLIPCHILDREN this fill only ever reaches the reserved strip
' -- which is exactly what makes the hidden-thumb state seamless, since the strip then
' matches the page it sits beside.
' ========================================================================================
function PsScrollPanel_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hwnd)
    if pScp = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pScp->BackColor )
    b.PaintClientRect()
    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' The PAGE's window procedure.
'
' It is the parent of every control the host puts on the page, which makes it the delivery
' point for two families of message the host would otherwise never see:
'   WM_CTLCOLOR*        answered here by default, so a plain settings page is themed with
'                       one SetColors call and no host code at all
'   WM_COMMAND/WM_NOTIFY offered to the message callback, then FORWARDED to the container's
'                       parent -- so a host that already handles its controls in its own
'                       WndProc keeps working when its page moves onto a PsScrollPanel.
' ========================================================================================
function PsScrollPanel_PanelProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointerFromPanel(hwnd)
    dim as LRESULT lResult

    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT, PsScrollPanel_OnPanelPaint)

    case WM_ERASEBKGND
        return true      ' the double buffer paints every pixel

    case WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORLISTBOX
        if pScp = 0 then exit select
        ' The host gets first refusal, because "every EDIT on this page has a lighter
        ' background" is a legitimate thing to want and only the host knows which is which.
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        SetBkColor( cast(HDC, wParam), pScp->BackColor )
        SetTextColor( cast(HDC, wParam), pScp->ForeColor )
        return cast( LRESULT, pScp->hBackBrushObj )

    case WM_COMMAND, WM_NOTIFY, WM_HSCROLL, WM_VSCROLL
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        ' Not claimed: send it where it would have gone if the page were not here.
        dim as HWND hOwner = GetParent( pScp->hWin )
        if hOwner then return SendMessage( hOwner, uMsg, wParam, lParam )
        exit select

    case WM_MOUSEWHEEL
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        ' cast(short, ...): hiword is UNSIGNED, and a delta read without this turns ~-120
        ' into ~65416, silently inverting the direction. See C:\dev\Learnings.md.
        PsScrollPanel_HandleWheelDelta( pScp->hWin, cast(short, hiword(wParam)) )
        return 0

    case WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP, _
         WM_MOUSEMOVE, WM_SETCURSOR
        ' Observation only -- this control takes no capture and tracks no press state, so
        ' there is no invariant a host callback could strand by claiming any of these.
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        exit select

    case WM_NCDESTROY
        ' Only this window's CWindow. The PSSCROLLPANEL belongs to the container, whose own
        ' WM_NCDESTROY follows this one (children are torn down first).
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' The CONTAINER's window procedure (the viewport).
' ========================================================================================
function PsScrollPanel_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hwnd)
    dim as LRESULT lResult

    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT, PsScrollPanel_OnPaint)

    case WM_ERASEBKGND
        return true      ' the double buffer paints every pixel

    case WM_SIZE
        PsScrollPanel_PositionWindows( hwnd )
        return 0

    case WM_MOUSEWHEEL
        ' Reachable over the reserved strip while the thumb is hidden.
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        PsScrollPanel_HandleWheelDelta( hwnd, cast(short, hiword(wParam)) )
        return 0

    case WM_COMMAND, WM_NOTIFY
        ' Rare (the host's controls live on the PAGE, not here), but a host that parents
        ' something to the container directly should still be heard.
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        exit select

    case WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP, _
         WM_MOUSEMOVE, WM_SETCURSOR
        if pScp = 0 then exit select
        if PsScrollPanel_CallMessage( pScp, hwnd, uMsg, wParam, lParam, lResult ) then
            return lResult
        end if
        exit select

    case WM_TIMER
        if pScp = 0 then exit select
        if wParam = IDT_CSCROLLPANEL_REVEAL then
            PsScrollPanel_OnRevealTick( pScp )
            return 0
        end if

    case WM_SHOWWINDOW
        if pScp = 0 then exit select
        if wParam = 0 then
            ' Hidden with the mouse over us: drop the poll and the reveal state, or the
            ' thumb would reappear stale the next time we are shown.
            PsScrollPanel_KillRevealTimer( pScp )
            if pScp->bThumbShown then
                pScp->bThumbShown = false
                ShowWindow( pScp->hScrollBar, SW_HIDE )
            end if
        else
            PsScrollPanel_SyncScrollBar( pScp )
        end if
        exit select

    case WM_NCDESTROY
        if pScp then
            PsScrollPanel_KillRevealTimer( pScp )
            if pScp->hBackBrushObj then DeleteObject( pScp->hBackBrushObj )
            delete(pScp)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' PsScrollPanel_Create. The control starts hidden and zero-sized; the host positions, sizes
' and shows it with SetWindowPos. Its two children are created here and are the control's
' to destroy.
' ========================================================================================
function PsScrollPanel_Create( byval hWndParent as HWND, byval CtrlID as integer ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_EX_CONTROLPARENT on BOTH our windows, and it is not decoration: IsDialogMessage
    ' only recurses into a child container that carries it. Without it a host's Tab key
    ' stops dead at the viewport and never reaches the page's controls -- which would take
    ' the focus-into-view scroll with it, since nothing would ever change focus.
    dim as HWND hScp = _
        pWindow->Create( hWndParent, "", @PsScrollPanel_WndProc, _
                         0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                         WS_EX_CONTROLPARENT )

    dim as PSSCROLLPANEL ptr pScp = new PSSCROLLPANEL
    pScp->hWin = hScp
    pScp->hBackBrushObj = CreateSolidBrush( pScp->BackColor )

    pWindow->UserData(0) = cast(LONG_PTR, pScp)
    SetWindowLongPtr( hScp, GWLP_ID, CtrlID )

    ' ---- the PAGE ----------------------------------------------------------------------
    ' WS_CLIPCHILDREN so the page's own background fill never paints over the host's
    ' controls on its way past them.
    dim pPanelWin as CWindow ptr = New CWindow
    pScp->hPanel = _
        pPanelWin->Create( hScp, "", @PsScrollPanel_PanelProc, _
                           0, 0, 0, 0, _
                           WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                           WS_EX_CONTROLPARENT )
    SetWindowLongPtr( pScp->hPanel, GWLP_ID, CtrlID + 1 )

    ' CWindow's default class style carries CS_HREDRAW or CS_VREDRAW, and its own header
    ' warns those cause heavy flicker on a window full of controls -- which is precisely
    ' what a page of settings is. Drop them and invalidate deliberately instead.
    ' Read-modify-write: the ClassStyle setter REPLACES, and it needs the window to exist.
    pPanelWin->ClassStyle = pPanelWin->ClassStyle and (not (CS_HREDRAW or CS_VREDRAW))

    ' ---- the BAR -----------------------------------------------------------------------
    ' Starts hidden; the reveal rule decides when it appears. Its BACK color is set to the
    ' page's, so the reserved strip is indistinguishable from the page while the thumb is
    ' away -- the whole point of reserving it rather than reclaiming the width.
    pScp->hScrollBar = PsVScrollBar_Create( hScp, CtrlID + 2 )
    PsVScrollBar_SetScrollCallback( pScp->hScrollBar, @PsScrollPanel_OnScrollBarScroll )
    PsVScrollBar_SetColors( pScp->hScrollBar, pScp->BackColor, BGR(53,59,69), BGR(90,98,112) )

    function = hScp
end function


function PsScrollPanel_GetPanel( byval hScp as HWND ) as HWND
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->hPanel
end function


function PsScrollPanel_GetScrollBar( byval hScp as HWND ) as HWND
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->hScrollBar
end function


' ========================================================================================
' Content height. Silent: the host set it and already knows.
' ========================================================================================
sub PsScrollPanel_SetContentHeight( byval hScp as HWND, byval nHeight as integer )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if nHeight < 0 then nHeight = 0
    if nHeight = pScp->nContentHeight then return
    pScp->nContentHeight = nHeight

    ' Called from inside the layout callback: record it and let that pass finish the job,
    ' rather than starting a second one underneath it.
    if pScp->bInLayout then return

    pScp->nPos = pScp->ClampPos( pScp->nPos )
    pScp->ApplyScroll()
    PsScrollPanel_SyncScrollBar( pScp )
    InvalidateRect( pScp->hPanel, NULL, FALSE )
end sub


function PsScrollPanel_GetContentHeight( byval hScp as HWND ) as integer
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->nContentHeight
end function


' ========================================================================================
' Measure the page from its children: the lowest bottom edge, plus a padding.
'
' HIDDEN children are skipped, so a collapsed section does not leave a tail of empty page
' behind it -- which is the case that makes this worth having rather than the host summing
' its own row heights.
' ========================================================================================
function PsScrollPanel_AutoSizeContent( _
            byval hScp as HWND, _
            byval bottomPadding as integer = 0 _
            ) as integer

    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    if pScp->hPanel = 0 then return 0

    dim as integer nBottom = 0
    dim as HWND hChild = GetWindow( pScp->hPanel, GW_CHILD )
    do while hChild
        ' The child's OWN WS_VISIBLE bit, NOT IsWindowVisible: that walks the whole ancestor
        ' chain, so with the control created-but-not-yet-shown -- exactly when a host builds
        ' its page -- every child reads invisible and this would measure the page as zero.
        ' The question here is "did the host hide this row", which is a per-window property.
        if (GetWindowLongPtr( hChild, GWL_STYLE ) and WS_VISIBLE) <> 0 then
            dim as RECT rc
            GetWindowRect( hChild, @rc )
            MapWindowPoints( HWND_DESKTOP, pScp->hPanel, cast( POINT ptr, @rc ), 2 )
            if rc.bottom > nBottom then nBottom = rc.bottom
        end if
        hChild = GetWindow( hChild, GW_HWNDNEXT )
    loop

    if nBottom > 0 then nBottom += bottomPadding
    PsScrollPanel_SetContentHeight( hScp, nBottom )
    return nBottom
end function


function PsScrollPanel_GetPos( byval hScp as HWND ) as integer
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->nPos
end function


' Silent -- this is the host telling the page where to sit, not the user scrolling it.
sub PsScrollPanel_SetPos( byval hScp as HWND, byval nPosition as integer )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    PsScrollPanel_ScrollTo( pScp, nPosition, false )
end sub


function PsScrollPanel_ScrollBy( byval hScp as HWND, byval nDelta as integer ) as boolean
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return false
    return PsScrollPanel_ScrollTo( pScp, pScp->nPos + nDelta, false )
end function


function PsScrollPanel_EnsureVisible( _
            byval hScp    as HWND, _
            byval hChild  as HWND, _
            byval nMargin as integer = 0 _
            ) as boolean

    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return false
    return PsScrollPanel_EnsureVisibleEx( pScp, hChild, nMargin, false )
end function


function PsScrollPanel_GetViewportHeight( byval hScp as HWND ) as integer
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->ViewportHeight()
end function


function PsScrollPanel_GetPanelWidth( byval hScp as HWND ) as integer
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return 0
    return pScp->PanelWidth()
end function


sub PsScrollPanel_Recalc( byval hScp as HWND )
    PsScrollPanel_PositionWindows( hScp )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp then InvalidateRect( pScp->hPanel, NULL, FALSE )
end sub


' ========================================================================================
' Appearance.
' ========================================================================================
sub PsScrollPanel_SetColors( byval hScp as HWND, byval backclr as COLORREF, byval foreclr as COLORREF )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->ForeColor = foreclr

    if pScp->BackColor <> backclr then
        pScp->BackColor = backclr
        ' The WM_CTLCOLOR brush is derived from the back color, so it is rebuilt here and
        ' nowhere else. Delete AFTER creating the replacement: a control repainting between
        ' the two would otherwise be handed a freed handle.
        dim as HBRUSH hOld = pScp->hBackBrushObj
        pScp->hBackBrushObj = CreateSolidBrush( backclr )
        if hOld then DeleteObject( hOld )
        ' The bar's own background follows the page, which is what keeps the reserved strip
        ' invisible while the thumb is away. Its thumb colors are the host's to set.
        if pScp->hScrollBar then
            dim as PSVSCROLLBAR ptr pSB = GetVScrollBarPointer( pScp->hScrollBar )
            if pSB then PsVScrollBar_SetColors( pScp->hScrollBar, backclr, pSB->ForeColor, pSB->ForeColorHot )
        end if
    end if

    InvalidateRect( pScp->hWin, NULL, FALSE )
    InvalidateRect( pScp->hPanel, NULL, TRUE )   ' TRUE: the host's controls repaint too
end sub


sub PsScrollPanel_SetScrollBarColors( _
            byval hScp       as HWND, _
            byval backclr    as COLORREF, _
            byval foreclr    as COLORREF, _
            byval foreclrhot as COLORREF _
            )

    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if pScp->hScrollBar then PsVScrollBar_SetColors( pScp->hScrollBar, backclr, foreclr, foreclrhot )
end sub


sub PsScrollPanel_SetScrollBarWidth( byval hScp as HWND, byval nWidth as integer )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if nWidth < 0 then nWidth = 0
    if nWidth = pScp->ScrollBarWidth then return
    pScp->ScrollBarWidth = nWidth
    PsScrollPanel_PositionWindows( hScp )    ' the page's width changed with the strip
end sub


sub PsScrollPanel_SetLineHeight( byval hScp as HWND, byval nHeight as integer )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if nHeight < 1 then nHeight = 1
    pScp->LineHeight = nHeight
end sub


' Applied to every existing child of the page. The HOST keeps ownership of the HFONT -- this
' control never deletes it, and a font passed here must outlive the page.
sub PsScrollPanel_SetFont( byval hScp as HWND, byval hFont as HFONT )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    if pScp->hPanel = 0 then return

    dim as HWND hChild = GetWindow( pScp->hPanel, GW_CHILD )
    do while hChild
        SendMessage( hChild, WM_SETFONT, cast(WPARAM, hFont), MAKELPARAM(TRUE, 0) )
        hChild = GetWindow( hChild, GW_HWNDNEXT )
    loop
end sub


' ========================================================================================
' Behaviour and state.
' ========================================================================================
sub PsScrollPanel_SetAutoScrollToFocus( byval hScp as HWND, byval bEnable as boolean )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->bAutoScrollToFocus = bEnable
end sub


function PsScrollPanel_GetAutoScrollToFocus( byval hScp as HWND ) as boolean
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return false
    return pScp->bAutoScrollToFocus
end function


function PsScrollPanel_IsThumbVisible( byval hScp as HWND ) as boolean
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return false
    return pScp->bThumbShown
end function


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsScrollPanel_SetPaintCallback( byval hScp as HWND, byval usersub as SCP_PaintCallbackSub )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->PaintCallback = usersub
    InvalidateRect( pScp->hPanel, NULL, FALSE )
end sub


sub PsScrollPanel_SetMessageCallback( byval hScp as HWND, byval userfunc as SCP_MessageCallbackFunc )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->MessageCallback = userfunc
end sub


sub PsScrollPanel_SetLayoutCallback( byval hScp as HWND, byval userfunc as SCP_LayoutCallbackFunc )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->LayoutCallback = userfunc
end sub


sub PsScrollPanel_SetScrollCallback( byval hScp as HWND, byval usersub as SCP_ScrollCallbackSub )
    dim as PSSCROLLPANEL ptr pScp = GetScrollPanelPointer(hScp)
    if pScp = 0 then return
    pScp->ScrollCallback = usersub
end sub
