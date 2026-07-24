''
''  CToggle.inc  --  owner-drawn toggle switch (pill + knob)
''

#include once "CToggle.bi"


function GetTogglePointer( byval hToggle as HWND ) as CTOGGLE ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hToggle)
    if pWindow = 0 then return 0
    return cast( CTOGGLE ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
' Callers decide whether to honor that answer -- WM_LBUTTONUP and the focus messages
' deliberately do not.
' ========================================================================================
private function CToggle_CallMessage( _
            byval pTog   as CTOGGLE ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pTog = 0 then return false
    if pTog->MessageCallback = 0 then return false

    dim as CTOGGLE_MESSAGEINFO m
    m.hToggle = hwnd
    m.uMsg    = uMsg
    m.wParam  = wParam
    m.lParam  = lParam
    return pTog->MessageCallback( @m )
end function


' ========================================================================================
' Draw the pill and the knob.
'
' This used to supersample: the whole visual was rendered into a 4x offscreen 32bpp DIB and
' averaged back down with a HALFTONE StretchBlt, because plain GDI has no antialiasing and
' RoundRect's shoulders and Ellipse's rim came out visibly stair-stepped on a ~40x20 switch.
' CBufferPaint draws through GDI+ now, so the curves are antialiased directly and the
' whole tile -- roughly 145 lines, an offscreen DC, a DIB section, and the two traps below
' -- is gone.
'
' The two traps went with it, and they are recorded here because they are what the old code
' was spending its complexity on:
'
'   1. THE TILE HAD TO BE PRE-FILLED WITH BackColor. A fresh DIB section is zeroed, i.e.
'      black, and the downscale averaged edge blocks against whatever was underneath -- so
'      an unfilled tile blended every curve toward black and the pill wore a dark fringe
'      that read as a drop shadow. There is no tile now, so there is no blend partner to
'      get wrong.
'
'   2. GDI PENS ARE CENTRED ON THE PATH, so a border of width W straddled the track edge
'      with half of it outside, and the RoundRect had to be deflated by W. GDI+ pens are
'      centred too -- but CBufferPaint now owns that correction for every control, so it
'      is applied once there instead of hand-rolled here.
'
' What is left is three primitive calls. Nothing in this routine touches a GDI object or a
' device context, which is what puts CToggle back on the family contract: the control owns
' geometry and state, CBufferPaint owns rendering.
' ========================================================================================
private sub CToggle_RenderDefault( _
            byval pTog as CTOGGLE ptr, _
            byval b    as CBufferPaint ptr, _
            byval p    as CTOGGLE_PAINTINFO ptr _
            )

    if (pTog = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long vw = p->rcVisual.right - p->rcVisual.left
    dim as long vh = p->rcVisual.bottom - p->rcVisual.top
    if (vw <= 0) orelse (vh <= 0) then exit sub

    ' ---- which three colors ------------------------------------------------------------
    ' Precedence: disabled > hot/pressed > idle. A live press renders as hot; there are no
    ' pressed colors (see CTOGGLE_COLORS).
    dim as COLORREF clrTrack, clrBorder, clrKnob
    with pTog->colors
        if p->isEnabled = false then
            if p->isChecked then
                clrTrack = .TrackColorOnDisabled
                clrBorder = .TrackBorderColorOnDisabled
                clrKnob  = .KnobColorOnDisabled
            else
                clrTrack = .TrackColorOffDisabled
                clrBorder = .TrackBorderColorOffDisabled
                clrKnob  = .KnobColorOffDisabled
            end if
        elseif p->isHot orelse p->isPressed then
            if p->isChecked then
                clrTrack = .TrackColorOnHot
                clrBorder = .TrackBorderColorOnHot
                clrKnob  = .KnobColorOnHot
            else
                clrTrack = .TrackColorOffHot
                clrBorder = .TrackBorderColorOffHot
                clrKnob  = .KnobColorOffHot
            end if
        else
            if p->isChecked then
                clrTrack = .TrackColorOn
                clrBorder = .TrackBorderColorOn
                clrKnob  = .KnobColorOn
            else
                clrTrack = .TrackColorOff
                clrBorder = .TrackBorderColorOff
                clrKnob  = .KnobColorOff
            end if
        end if
    end with

    ' ---- the track ---------------------------------------------------------------------
    ' A corner ellipse as wide and tall as the rect's HEIGHT makes both ends exact
    ' semicircles -- that, and only that, is what turns a rounded rectangle into a pill.
    ' Curvature is a DIAMETER here (CBufferPaint halves it to a radius internally), so
    ' the track height goes in unmodified.
    dim as long ell = p->rcTrack.bottom - p->rcTrack.top
    dim as RECT rcTrack = p->rcTrack
    b->SetBackColor( clrTrack )
    if pTog->nBorderThick > 0 then
        b->SetPenColor( clrBorder )
        b->PaintRoundBorderRect( @rcTrack, ell, pTog->nBorderThick )
    else
        b->PaintRoundRect( @rcTrack, ell )
    end if

    ' ---- the knob ----------------------------------------------------------------------
    dim as RECT rcKnob = p->rcKnob
    b->SetBackColor( clrKnob )
    b->PaintEllipse( @rcKnob, 0 )

    ' ---- the focus ring ----------------------------------------------------------------
    ' Outline, never filled: it is drawn OVER the pill, and a filled round rect here would
    ' erase everything above. Its band is reserved in the layout whether or not we are
    ' focused, which is what stops the pill jumping when focus arrives.
    if p->isFocused andalso (pTog->nFocusThick > 0) then
        dim as RECT rcRing = p->rcVisual
        dim as long fell = rcRing.bottom - rcRing.top
        b->SetPenColor( pTog->colors.FocusRingColor )
        b->PaintRoundOutline( @rcRing, fell, pTog->nFocusThick )
    end if
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function CToggle_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CTOGGLE ptr pTog = GetTogglePointer(hwnd)
    if pTog = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pTog->bLayoutDirty then pTog->LayoutToggle()

    dim as CBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pTog->colors.BackColor )
    b.PaintClientRect()

    dim p as CTOGGLE_PAINTINFO
    p.hToggle  = hwnd
    p.b        = @b
    GetClientRect( hwnd, @p.rcClient )
    p.rcTrack  = pTog->rcTrack
    p.rcKnob   = pTog->rcKnob
    p.rcVisual = pTog->rcVisual
    p.isChecked = pTog->isChecked
    p.isHot     = pTog->isHot
    ' "The press is still armed" is exactly "pressed AND the cursor is still inside" --
    ' hover tracking maintains isHot through the capture, so there is no second flag.
    p.isPressed = pTog->isPressed andalso pTog->isHot
    p.isEnabled = pTog->isEnabled
    p.isFocused = pTog->isFocused

    if pTog->PaintCallback then
        pTog->PaintCallback( @p )
    else
        CToggle_RenderDefault( pTog, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
'
' Note this also changes how a live press LOOKS (the painter derives isPressed from isHot),
' which is exactly right: the cursor left, so the press must stop looking armed. The gesture
' itself survives -- only a release or a capture loss ends it.
' ========================================================================================
private sub CToggle_ClearHot( byval pTog as CTOGGLE ptr, byval hwnd as HWND )
    if pTog = 0 then exit sub
    if pTog->hotTimerOn then
        KillTimer( hwnd, IDT_CTOGGLE_HOTTRACK )
        pTog->hotTimerOn = false
    end if
    if pTog->isHot then
        pTog->isHot = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Flip the state because the USER asked for it -- a completed click, or Space/Enter. The one
' place that fires the change callback, so click and keyboard cannot drift apart.
'
' The layout is marked dirty because the KNOB MOVED: rcKnob depends on isChecked, and it is
' LayoutToggle that positions it. Setting isChecked without this would leave the knob drawn
' at the old end.
'
' The callback goes out LAST, after the state is consistent, so a host that queries
' CToggle_GetChecked() from inside it sees the new value.
' ========================================================================================
private sub CToggle_ApplyUserToggle( byval pTog as CTOGGLE ptr, byval hwnd as HWND )
    if pTog = 0 then exit sub
    pTog->isChecked = not pTog->isChecked
    pTog->bLayoutDirty = true
    InvalidateRect( hwnd, NULL, FALSE )
    if pTog->CheckChangedCallback then pTog->CheckChangedCallback( hwnd, pTog->isChecked )
end sub


' ========================================================================================
' CToggle Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc.
'
' THE CONTROL TAKES MOUSE CAPTURE on a press. Something consumes the guaranteed
' WM_LBUTTONDOWN -> WM_LBUTTONUP pairing: the press/cancel gesture. Press the toggle, slide
' off it, release -- the state does NOT flip. The full price is paid, in the order the
' family learned to pay it:
'   - WM_LBUTTONUP SNAPSHOTS isPressed, THEN releases capture. ReleaseCapture sends
'     WM_CAPTURECHANGED synchronously and that handler cancels the press, so reading the
'     flag afterwards would always find FALSE and no click would ever flip anything
'     (Learnings.md);
'   - the release happens before any callback runs;
'   - the message callback's result is IGNORED for WM_LBUTTONUP, so it cannot strand it;
'   - WM_CAPTURECHANGED cancels the press (anything can steal capture at any time);
'   - WM_DESTROY releases it if we still hold it.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET -- the opposite call from CSelectBar and CSplitter,
' and the reason matters. With CS_DBLCLKS the second of two rapid clicks arrives as
' WM_LBUTTONDBLCLK INSTEAD OF a second WM_LBUTTONDOWN. For a control whose entire job is to
' flip on every click, that would silently swallow every second rapid click and the toggle
' would appear to ignore half the user's input. CIconPanel omits it for exactly this reason
' on its toggle items; here the control IS a toggle, so the reasoning is stronger still.
' Without the class style, rapid clicking produces a plain down/up pair every time.
'
' FOCUS: this is the first focusable control in the family (WS_TABSTOP, Space/Enter). See
' the WM_GETDLGCODE handler for the one subtlety that needed care.
' ========================================================================================
function CToggle_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CTOGGLE ptr pTog = GetTogglePointer( hwnd )

    select case uMsg

    case WM_PAINT
        return CToggle_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pTog then pTog->Refresh()
        return 0

    case WM_TIMER
        if (pTog <> 0) andalso (wParam = IDT_CTOGGLE_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no
            ' longer over our client area, clear the hot state ourselves. Skipped while a
            ' press is live: under capture the cursor legitimately roams outside, and
            ' WM_MOUSEMOVE is already maintaining isHot correctly for that case.
            if pTog->isPressed = false then
                dim as RECT rcClient
                GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then
                    CToggle_ClearHot( pTog, hwnd )
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pTog = 0 then exit select
        if pTog->isEnabled = false then exit select

        ' Under capture we receive moves from anywhere on the desktop, so "is the mouse over
        ' us" must be asked of the cursor position and never assumed from the message.
        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bOver = isMouseOverRECT( hwnd, rcClient )

        if bOver then
            ' Track so we get the eventual WM_MOUSELEAVE.
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
            ' quickly (or onto another control), which would strand the control painted hot.
            if pTog->hotTimerOn = false then
                SetTimer( hwnd, IDT_CTOGGLE_HOTTRACK, CTOGGLE_HOTTRACK_MS, NULL )
                pTog->hotTimerOn = true
            end if
        end if

        if bOver <> pTog->isHot then
            pTog->isHot = bOver
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pTog then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for
            ' when this message never arrives).
            if pTog->isPressed = false then CToggle_ClearHot( pTog, hwnd )
            if CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pTog = 0 then return 0
        ' A disabled window does not receive mouse input at all (CToggle_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some
        ' other route.
        if pTog->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true
        ' even if the host goes on to suppress the press. Note this runs WM_SETFOCUS
        ' synchronously, so isFocused is already correct by the time we paint below.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam ) then return 0

        SetCapture( hwnd )
        pTog->isPressed = true
        pTog->isHot     = true
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_LBUTTONUP
        if pTog = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window
        ' losing capture SYNCHRONOUSLY, and our handler for it cancels the press -- so
        ' reading isPressed after the release would always find FALSE and nothing would ever
        ' flip.
        dim as boolean bWasPressed = pTog->isPressed
        ' Release capture unconditionally, before any callback runs. Nothing below -- not a
        ' callback, not a host handler -- gets the chance to leave it held.
        if GetCapture() = hwnd then ReleaseCapture()

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bInside = isMouseOverRECT( hwnd, rcClient )

        pTog->CancelPress()
        InvalidateRect( hwnd, NULL, FALSE )

        ' The result is deliberately IGNORED here (see TOG_MessageCallbackFunc).
        CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release with the cursor still inside flips the state. Press,
        ' slide off, release = nothing happened, which is the whole reason for capture.
        ' isEnabled is re-read because a callback may have disabled us mid-gesture.
        dim as boolean bFlip = bWasPressed andalso bInside andalso pTog->isEnabled
        if bFlip then CToggle_ApplyUserToggle( pTog, hwnd )
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell).
        ' The gesture is over; forget it and repaint.
        if pTog then
            pTog->CancelPress()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (CPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pTog then
            if CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' Focus is a FACT reported by the system, not an action a host can veto, so the
        ' state is updated first and the callback's result is IGNORED. A host that wants to
        ' refuse focus must not make the control a tabstop in the first place.
        if pTog then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pTog->isFocused then
                pTog->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the two keys we actually consume, and only when we are being asked
        ' about a specific message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the
        ' control would swallow the very navigation it just opted into by being a tabstop.
        ' Claiming nothing would let a host's IsDialogMessage pump route Enter to the
        ' dialog's default button before the control ever saw it. Per-message is the only
        ' answer that gets both right.
        if lParam then
            dim as MSG ptr pMsg = cast( MSG ptr, lParam )
            if pMsg->message = WM_KEYDOWN then
                select case pMsg->wParam
                case VK_SPACE, VK_RETURN
                    return DLGC_WANTALLKEYS
                end select
            end if
        end if
        return 0

    case WM_KEYDOWN
        if pTog = 0 then exit select
        if pTog->isEnabled = false then return 0
        if CToggle_CallMessage( pTog, hwnd, uMsg, wParam, lParam ) then return 0
        select case wParam
        case VK_SPACE, VK_RETURN
            ' Same path a completed click takes, so keyboard and mouse cannot drift apart:
            ' both are user action, both notify.
            CToggle_ApplyUserToggle( pTog, hwnd )
            return 0
        end select
        ' Arrow keys are deliberately NOT handled. Left=off / Right=on is a common toggle
        ' idiom but was not asked for, and adding it would mean claiming the arrows in
        ' WM_GETDLGCODE too and taking them away from the dialog manager's navigation.

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of CToggle_SetEnabled still gets the greyed
        ' rendering rather than a control that looks live but eats input.
        if pTog then
            pTog->isEnabled = (wParam <> 0)
            if pTog->isEnabled = false then
                pTog->isHot = false
                pTog->CancelPress()
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_DESTROY
        if pTog then
            ' Don't let the hot-tracking timer outlive the window...
            if pTog->hotTimerOn then
                KillTimer( hwnd, IDT_CTOGGLE_HOTTRACK )
                pTog->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pTog->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored toggle data pointer. There is no tooltip window and no font to
        ' release: the control owns no host resources.
        if pTog then delete(pTog)

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' CToggle_Create
' ========================================================================================
function CToggle_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus, unlike most of its siblings. There is no
    ' WS_EX_CONTROLPARENT because there is no child to see through to -- the tabstop is on
    ' this window itself.
    '
    ' THE TRAILING 0 IS THAT SENTENCE'S IMPLEMENTATION, AND IT WAS MISSING UNTIL 2026-07-23.
    ' CWindow.Create defaults its dwExStyle parameter to WS_EX_CONTROLPARENT OR
    ' WS_EX_WINDOWEDGE, so passing only dwStyle -- as this call did -- silently declared the
    ' control a CONTAINER. The dialog manager then descended into it looking for tabstops,
    ' found no children, and SKIPPED it: Tab never landed on a CToggle, in this repo's demo
    ' or in any host. The comment above was written as an intention and read for months as a
    ' statement of fact; nothing at the call site showed an extended style being chosen at all.
    '
    ' DO NOT COPY THIS INTO A WRAP-A-REAL-CHILD CONTROL. CListBox, CTextBox, CNumericUpDown
    ' and CScrollPanel all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner
    ' child; passing 0 there would make them unreachable instead. The test is "does this
    ' window have children the dialog manager should find?", not "is this a control?".
    ' Full write-up in C:\dev\Learnings.md.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @CToggle_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header. It would
    ' turn every second rapid click into a WM_LBUTTONDBLCLK and the toggle would drop it.

    dim as CTOGGLE ptr pTog = new CTOGGLE
    pTog->hWin       = hCtrl
    pTog->idc_Toggle = CtrlID

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). The inset and the focus gap take
    ' ScaleY because they are subtracted from the track HEIGHT (they apply on both axes, and
    ' ScaleX and ScaleY agree in practice, but the height arithmetic is the one that has to
    ' stay consistent). nBorderThick and nFocusThick are NOT scaled at all -- a hairline
    ' should stay a hairline, which is CMenuBar's nSeparatorThickness rule.
    pTog->nTrackWidth  = pWindow->ScaleX( CTOGGLE_DEFAULT_TRACKW )
    pTog->nTrackHeight = pWindow->ScaleY( CTOGGLE_DEFAULT_TRACKH )
    pTog->nKnobInset   = pWindow->ScaleY( CTOGGLE_DEFAULT_KNOBINSET )
    pTog->nFocusGap    = pWindow->ScaleY( CTOGGLE_DEFAULT_FOCUSGAP )

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the toggle window
    pWindow->UserData(0) = cast(LONG_PTR, pTog)

    function = hCtrl

end function


' ========================================================================================
' State
' ========================================================================================
function CToggle_GetChecked( byval hToggle as HWND ) as boolean
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    return pTog->isChecked
end function

' ========================================================================================
' SILENT: the change callback fires only for user interaction. See the header.
' ========================================================================================
sub CToggle_SetChecked( byval hToggle as HWND, byval isChecked as boolean )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    if pTog->isChecked = isChecked then exit sub
    pTog->isChecked = isChecked
    pTog->Refresh()          ' the knob moved, so the layout is stale
end sub

function CToggle_GetEnabled( byval hToggle as HWND ) as boolean
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    return pTog->isEnabled
end function

' ========================================================================================
' Greys the control AND disables the window. Going through EnableWindow rather than just
' painting differently is what actually stops the input: a disabled window receives no mouse
' messages and the dialog manager's Tab skips it, so there is no way for a click or a
' keystroke to sneak past the cosmetic check. WM_ENABLE does the state clearing (and covers
' a host that calls EnableWindow itself); the work is repeated here because Windows sends no
' WM_ENABLE when the state was already what we asked for.
' ========================================================================================
sub CToggle_SetEnabled( byval hToggle as HWND, byval isEnabled as boolean )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->isEnabled = isEnabled
    if isEnabled then
        EnableWindow( hToggle, TRUE )
    else
        EnableWindow( hToggle, FALSE )
        pTog->isHot = false
        pTog->CancelPress()
    end if
    InvalidateRect( hToggle, NULL, FALSE )
end sub

function CToggle_GetFocused( byval hToggle as HWND ) as boolean
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    return pTog->isFocused
end function

sub CToggle_Refresh( byval hToggle as HWND )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->Refresh()
end sub


' ========================================================================================
' Layout.  All setters take RAW PIXELS -- the caller DPI-scales.
' ========================================================================================
sub CToggle_GetTrackSize( byval hToggle as HWND, byref nTrackWidth as long, byref nTrackHeight as long )
    nTrackWidth  = 0
    nTrackHeight = 0
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    nTrackWidth  = pTog->nTrackWidth
    nTrackHeight = pTog->nTrackHeight
end sub

sub CToggle_SetTrackSize( byval hToggle as HWND, byval nTrackWidth as long, byval nTrackHeight as long )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    if nTrackWidth  < 1 then nTrackWidth  = 1
    if nTrackHeight < 1 then nTrackHeight = 1
    pTog->nTrackWidth  = nTrackWidth
    pTog->nTrackHeight = nTrackHeight
    pTog->Refresh()
end sub

function CToggle_GetKnobInset( byval hToggle as HWND ) as long
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return 0
    return pTog->nKnobInset
end function

sub CToggle_SetKnobInset( byval hToggle as HWND, byval nKnobInset as long )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    if nKnobInset < 0 then nKnobInset = 0
    pTog->nKnobInset = nKnobInset
    pTog->Refresh()
end sub

function CToggle_GetBorderThickness( byval hToggle as HWND ) as long
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return 0
    return pTog->nBorderThick
end function

' Thickness only, no layout consequence: the border is drawn INSIDE the track, so changing
' it repaints but never moves anything. Zero means no border at all.
sub CToggle_SetBorderThickness( byval hToggle as HWND, byval nThickness as long )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pTog->nBorderThick = nThickness
    if pTog->hWin then InvalidateRect( pTog->hWin, NULL, FALSE )
end sub

sub CToggle_GetFocusRing( byval hToggle as HWND, byref nGap as long, byref nThickness as long )
    nGap       = 0
    nThickness = 0
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    nGap       = pTog->nFocusGap
    nThickness = pTog->nFocusThick
end sub

' BOTH are layout inputs, focused or not: the ring's band is reserved unconditionally so the
' pill does not jump when focus arrives, which means either value changes the ideal size.
sub CToggle_SetFocusRing( byval hToggle as HWND, byval nGap as long, byval nThickness as long )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    if nGap < 0 then nGap = 0
    if nThickness < 0 then nThickness = 0
    pTog->nFocusGap   = nGap
    pTog->nFocusThick = nThickness
    pTog->Refresh()
end sub

function CToggle_GetJustify( byval hToggle as HWND ) as long
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return TOG_JUSTIFY_CENTER
    return pTog->nJustify
end function

sub CToggle_SetJustify( byval hToggle as HWND, byval nJustify as long )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    select case nJustify
    case TOG_JUSTIFY_LEFT, TOG_JUSTIFY_CENTER, TOG_JUSTIFY_RIGHT
    case else
        exit sub                 ' ignore garbage rather than laying out somewhere strange
    end select
    pTog->nJustify = nJustify
    pTog->Refresh()
end sub

' ========================================================================================
' The visual bounds: the pill plus the focus-ring band on every side. Computed straight from
' the scalars and NOT from the layout, so it is valid before the control has ever been
' sized -- which is the whole point, since a host calls this to decide how big to make it.
' ========================================================================================
sub CToggle_GetIdealSize( byval hToggle as HWND, byref nWidth as long, byref nHeight as long )
    nWidth  = 0
    nHeight = 0
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    dim as long ringPad = pTog->RingPad()
    nWidth  = pTog->nTrackWidth  + (2 * ringPad)
    nHeight = pTog->nTrackHeight + (2 * ringPad)
end sub

' ========================================================================================
' Rect queries. Each forces a pending layout, so results are always current. FALSE means the
' control has no geometry yet (created but never sized).
' ========================================================================================
function CToggle_GetTrackRect( byval hToggle as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    if pTog->bLayoutDirty then pTog->LayoutToggle()
    if pTog->bLayoutDirty then return false        ' still no client area
    rc = pTog->rcTrack
    return true
end function

function CToggle_GetKnobRect( byval hToggle as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    if pTog->bLayoutDirty then pTog->LayoutToggle()
    if pTog->bLayoutDirty then return false
    rc = pTog->rcKnob
    return true
end function

function CToggle_GetVisualRect( byval hToggle as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then return false
    if pTog->bLayoutDirty then pTog->LayoutToggle()
    if pTog->bLayoutDirty then return false
    rc = pTog->rcVisual
    return true
end function


' ========================================================================================
' Appearance
' ========================================================================================
sub CToggle_GetColors( byval hToggle as HWND, byval pColors as CTOGGLE_COLORS ptr )
    if pColors = 0 then exit sub
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    *pColors = pTog->colors
end sub

sub CToggle_SetColors( byval hToggle as HWND, byval pColors as CTOGGLE_COLORS ptr )
    if pColors = 0 then exit sub
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->colors = *pColors
    if pTog->hWin then InvalidateRect( pTog->hWin, NULL, TRUE )
end sub


' ========================================================================================
' Callbacks
' ========================================================================================
sub CToggle_SetPaintCallback( byval hToggle as HWND, byval usersub as TOG_PaintCallbackSub )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->PaintCallback = usersub
    if pTog->hWin then InvalidateRect( pTog->hWin, NULL, TRUE )
end sub

sub CToggle_SetMessageCallback( byval hToggle as HWND, byval userfunc as TOG_MessageCallbackFunc )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->MessageCallback = userfunc
end sub

sub CToggle_SetCheckChangedCallback( byval hToggle as HWND, byval usersub as TOG_CheckChangedCallbackSub )
    dim as CTOGGLE ptr pTog = GetTogglePointer(hToggle)
    if pTog = 0 then exit sub
    pTog->CheckChangedCallback = usersub
end sub
