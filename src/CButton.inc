''
''  CButton.inc  --  owner-drawn command button (glyph + caption + glyph)
''

#include once "CButton.bi"


' Forward declaration: the content setters re-apply auto-size, and they are defined well above
' the layout section where it lives.
declare sub CButton_ApplyAutoSize( byval pBtn as CBUTTON ptr )


function GetButtonPointer( byval hButton as HWND ) as CBUTTON ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hButton)
    if pWindow = 0 then return 0
    return cast( CBUTTON ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not.
' WM_LBUTTONUP is one of them here, because this control HOLDS MOUSE CAPTURE across a press
' and the up-message is what releases it. That reverses the contract in CComboBox, which this
' file was seeded from: a "no capture, result honoured" note copied in from there would be a
' lie.
' ========================================================================================
private function CButton_CallMessage( _
            byval pBtn   as CBUTTON ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pBtn = 0 then return false
    if pBtn->MessageCallback = 0 then return false

    dim as CBUTTON_MESSAGEINFO m
    m.hButton = hwnd
    m.uMsg    = uMsg
    m.wParam  = wParam
    m.lParam  = lParam
    return pBtn->MessageCallback( @m )
end function


' Resolve the tooltip text: the control's own text if it has any, else the host callback.
' NO CAPTION FALLBACK -- a button's caption is already on screen, so falling back to it would
' pop a tip that repeats what the user is looking at.
private function CButton_ResolveTooltipText( byval pBtn as CBUTTON ptr ) as DWSTRING
    if pBtn = 0 then return ""
    if len( pBtn->wszTooltip ) then
        dim as DWSTRING wszOut = pBtn->wszTooltip
        return wszOut
    end if
    if pBtn->TooltipCallback then return pBtn->TooltipCallback( pBtn->hWin )
    return ""
end function


' ========================================================================================
' Draw the button: chrome, left glyph, caption, right glyph, focus ring.
'
' Nothing in here touches a GDI object or a device context -- CBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' THE COLOURS COME OUT OF CButton_ResolveMood + CButton_ResolveColors, not out of an if-chain
' written inline here. That is what lets the self-test assert the precedence and the isDefault
' border overlay directly, instead of only being able to look at pixels.
'
' PRESENCE IS TESTED ON THE RECTS, not on the strings. LayoutButton leaves an absent part's
' rect EMPTY, so IsRectEmpty is the single source of truth about what exists -- and a
' degenerate caption span (the client too narrow to leave any room) drops out of the same test
' rather than needing its own.
' ========================================================================================
private sub CButton_RenderDefault( _
            byval pBtn as CBUTTON ptr, _
            byval b    as CBufferPaint ptr, _
            byval p    as CBUTTON_PAINTINFO ptr _
            )

    if (pBtn = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long bw = p->rcButton.right - p->rcButton.left
    dim as long bh = p->rcButton.bottom - p->rcButton.top
    if (bw <= 0) orelse (bh <= 0) then exit sub

    ' ---- which four colors -------------------------------------------------------------
    dim as long nMood = CButton_ResolveMood( p->isEnabled, p->isPressed, p->isHot )
    dim as COLORREF clrBack, clrFore, clrBorder, clrIcon
    CButton_ResolveColors( @pBtn->colors, nMood, p->isDefault, _
                           clrBack, clrFore, clrBorder, clrIcon )

    ' ---- the chrome --------------------------------------------------------------------
    ' nCurvature is an ellipse DIAMETER (CBufferPaint keeps GDI's vocabulary and halves it
    ' internally). Zero gives square corners through the same call.
    dim as RECT rcBtn = p->rcButton
    b->SetBackColor( clrBack )
    if pBtn->nBorderThick > 0 then
        b->SetPenColor( clrBorder )
        b->PaintRoundBorderRect( @rcBtn, pBtn->nCurvature, pBtn->nBorderThick )
    else
        b->PaintRoundRect( @rcBtn, pBtn->nCurvature )
    end if

    ' ---- the fonts ---------------------------------------------------------------------
    ' Paint the caption with the SAME font the measuring pass used -- the caller's if set, else
    ' the stock GUI font. CBufferPaint.SetFont IGNORES a null handle, so leaving it unset would
    ' silently paint in the DC's font while the width was measured in another, and long
    ' captions would clip for no visible reason.
    dim as HFONT hTextUse = pBtn->hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    ' The glyph font falls back to the caption font rather than to the stock font: a host that
    ' set one font and not the other almost certainly meant that one.
    dim as HFONT hGlyphUse = pBtn->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = hTextUse

    ' ---- the left glyph ----------------------------------------------------------------
    ' The cell is a LAYOUT rect, not a measurement of the glyph: the control never measures the
    ' icon font, so a glyph too large for the cell clips rather than resizing the button.
    if IsRectEmpty( @p->rcIconLeft ) = 0 then
        dim as RECT rcGL = p->rcIconLeft
        b->SetFont( hGlyphUse )
        b->SetColors( clrIcon, clrBack )
        b->PaintText( p->wszGlyphLeft, @rcGL, DT_CENTER )
    end if

    ' ---- the right glyph ---------------------------------------------------------------
    if IsRectEmpty( @p->rcIconRight ) = 0 then
        dim as RECT rcGR = p->rcIconRight
        b->SetFont( hGlyphUse )
        b->SetColors( clrIcon, clrBack )
        b->PaintText( p->wszGlyphRight, @rcGR, DT_CENTER )
    end if

    ' ---- the caption -------------------------------------------------------------------
    ' rcText is the SPAN between the icon blocks, which is exactly what DT_END_ELLIPSIS needs:
    ' handed a tight box the ellipsis would have nowhere to happen and the text would simply
    ' clip. DT_NOPREFIX / DT_VCENTER / DT_SINGLELINE are added by PaintText itself -- and the
    ' first of those is what makes "&Save" draw a literal ampersand, so this control's
    ' no-mnemonics rule is enforced by the renderer rather than merely intended.
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as DWORD dtAlign = DT_CENTER
        select case p->nTextAlign
        case BTN_ALIGN_LEFT
            dtAlign = DT_LEFT
        case BTN_ALIGN_RIGHT
            dtAlign = DT_RIGHT
        end select

        dim as RECT rcT = p->rcText
        b->SetFont( hTextUse )
        b->SetColors( clrFore, clrBack )
        b->PaintText( p->wszText, @rcT, dtAlign or DT_END_ELLIPSIS )
    end if

    ' ---- the focus ring ----------------------------------------------------------------
    ' Outline, never filled: it is drawn OVER the button, and a filled round rect here would
    ' erase everything above it. Its band is reserved in the layout whether or not we are
    ' focused, which is what stops the button jumping when focus arrives.
    '
    ' The ring's curvature is the button's plus the band on both sides, which is what keeps the
    ' two curves concentric -- reusing nCurvature would leave the ring visibly squarer than the
    ' corner it surrounds.
    if p->isFocused then
        if pBtn->nFocusThick > 0 then
            dim as RECT rcRing = p->rcVisual
            dim as long fcurv = pBtn->nCurvature + (2 * pBtn->RingPad())
            b->SetPenColor( pBtn->colors.FocusRingColor )
            b->PaintRoundOutline( @rcRing, fcurv, pBtn->nFocusThick )
        end if
    end if
end sub


' ========================================================================================
' Fill a PAINTINFO from the control's current state and geometry. One place, so the WM_PAINT
' path and the self-test's offscreen probe cannot describe the control differently.
' ========================================================================================
private sub CButton_FillPaintInfo( _
            byval pBtn as CBUTTON ptr, _
            byval hwnd as HWND, _
            byval b    as CBufferPaint ptr, _
            byval p    as CBUTTON_PAINTINFO ptr _
            )

    if (pBtn = 0) orelse (p = 0) then exit sub

    p->hButton = hwnd
    p->b       = b
    GetClientRect( hwnd, @p->rcClient )
    p->rcButton      = pBtn->rcButton
    p->rcIconLeft    = pBtn->rcIconLeft
    p->rcText        = pBtn->rcText
    p->rcIconRight   = pBtn->rcIconRight
    p->rcVisual      = pBtn->rcVisual
    p->isHot         = pBtn->isHot
    p->isPressed     = pBtn->isPressed
    p->isEnabled     = pBtn->isEnabled
    p->isFocused     = pBtn->isFocused
    p->isDefault     = pBtn->isDefault
    p->wszText       = pBtn->wszText
    p->wszGlyphLeft  = pBtn->wszGlyphLeft
    p->wszGlyphRight = pBtn->wszGlyphRight
    p->nTextAlign    = pBtn->nTextAlign
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function CButton_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CBUTTON ptr pBtn = GetButtonPointer(hwnd)
    if pBtn = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pBtn->bLayoutDirty then pBtn->LayoutButton()

    dim as CBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' The client is filled with the resolved mood's own background, not a fixed one, so a paint
    ' callback that draws only a foreground still sits on the right colour. Resolved through
    ' the same two functions the painter uses, so the fill can never disagree with the chrome.
    dim as long nMood = CButton_ResolveMood( pBtn->isEnabled, pBtn->isPressed, pBtn->isHot )
    dim as COLORREF clrFill, clrFore, clrBorder, clrIcon
    CButton_ResolveColors( @pBtn->colors, nMood, pBtn->isDefault, _
                           clrFill, clrFore, clrBorder, clrIcon )
    b.SetBackColor( clrFill )
    b.PaintClientRect()

    dim p as CBUTTON_PAINTINFO
    CButton_FillPaintInfo( pBtn, hwnd, @b, @p )

    if pBtn->PaintCallback then
        pBtn->PaintCallback( @p )
    else
        CButton_RenderDefault( pBtn, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub CButton_ClearHot( byval pBtn as CBUTTON ptr, byval hwnd as HWND )
    if pBtn = 0 then exit sub
    if pBtn->hotTimerOn then
        KillTimer( hwnd, IDT_CBUTTON_HOTTRACK )
        pBtn->hotTimerOn = false
    end if
    if pBtn->isHot then
        pBtn->isHot = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Apply the ideal size to ourselves, preserving the top-left. Only runs when the host has
' opted in with CButton_SetAutoSize -- the family default is that a control never moves itself
' and the host measures with GetIdealSize.
'
' Called from every mutator that can change the ideal size. It forces the pending layout first
' (that is where the measurement happens), then resizes -- which marks the layout dirty again
' through WM_SIZE, so the rects are re-derived against the new client area.
' ========================================================================================
sub CButton_ApplyAutoSize( byval pBtn as CBUTTON ptr )
    if pBtn = 0 then exit sub
    if pBtn->bAutoSize = false then exit sub
    if pBtn->hWin = 0 then exit sub

    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if (pBtn->nIdealW <= 0) orelse (pBtn->nIdealH <= 0) then exit sub

    SetWindowPos( pBtn->hWin, 0, 0, 0, pBtn->nIdealW, pBtn->nIdealH, _
                  SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
end sub


' ========================================================================================
' The one activation path. A completed mouse click, Space/Enter, and the programmatic
' CButton_Click all come through here, so keyboard, mouse and host can never drift apart.
'
' It reads isEnabled again rather than trusting the caller: a message callback may have
' disabled the control mid-gesture.
' ========================================================================================
private sub CButton_FireClick( byval pBtn as CBUTTON ptr, byval hwnd as HWND )
    if pBtn = 0 then exit sub
    if pBtn->isEnabled = false then exit sub
    if pBtn->ClickCallback = 0 then exit sub
    pBtn->ClickCallback( hwnd, pBtn->id )
end sub


' ========================================================================================
' CButton Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is handled
' here and anything unhandled goes to DefWindowProc.
'
' MOUSE CAPTURE IS TAKEN. The family's test is "take capture only if something consumes the
' guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and here it comes out positive: the
' press/cancel gesture (press the button, slide off it, release, nothing happens) is exactly
' that. So the full price is paid -- snapshot the press before releasing, a WM_CAPTURECHANGED
' handler that cancels it, a release in WM_DESTROY, and a callback result that is ignored for
' the up-message.
'
' NOTE this control fires on the UP, unlike CComboBox (which opens its list on the DOWN) and
' CNumericUpDown (which steps on the DOWN and uses capture to bound an auto-repeat). Same
' obligation, a third distinct justification.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET, which is CToggle's and CIconPanel's call rather than
' CSelectBar's. With it, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD OF
' a second WM_LBUTTONDOWN -- and on a momentary command button a rapid second click is a
' legitimate second click, which would be silently swallowed.
'
' FOCUS: this is the third focusable control in the family, after CToggle and CComboBox. See
' the WM_GETDLGCODE handler for the one subtlety that needed care.
' ========================================================================================
function CButton_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CBUTTON ptr pBtn = GetButtonPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return CButton_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pBtn then
            pBtn->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_TIMER
        if (pBtn <> 0) andalso (wParam = IDT_CBUTTON_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot state ourselves.
            '
            ' NOT while a press is live: during capture the cursor is legitimately allowed to
            ' wander off and come back, and the WM_MOUSEMOVE handler is already maintaining
            ' isHot for exactly that. Killing the timer here would also strip the press of its
            ' safety net for the moment the button comes back under the cursor.
            if pBtn->isPressed = false then
                dim as RECT rcClient
                GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then
                    CButton_ClearHot( pBtn, hwnd )
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pBtn = 0 then exit select
        if pBtn->isEnabled = false then exit select

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
            if pBtn->hotTimerOn = false then
                SetTimer( hwnd, IDT_CBUTTON_HOTTRACK, CBUTTON_HOTTRACK_MS, NULL )
                pBtn->hotTimerOn = true
            end if
        end if

        ' This is also what drives the press-cancel FEEDBACK: with capture held, moves outside
        ' the client still reach us, so sliding off a pressed button clears isHot -- which
        ' drops the mood from PRESSED back to IDLE and un-flashes it, before the release
        ' decides that nothing happened.
        if bOver <> pBtn->isHot then
            pBtn->isHot = bOver
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pBtn then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for when
            ' this message never arrives). Suppressed during a press for the same reason as in
            ' the timer: the gesture is still live and WM_MOUSEMOVE owns isHot.
            if pBtn->isPressed = false then CButton_ClearHot( pBtn, hwnd )
            if CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pBtn = 0 then return 0
        ' A disabled window does not receive mouse input at all (CButton_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some
        ' other route.
        if pBtn->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true
        ' even if the host goes on to suppress the press. This runs WM_SETFOCUS synchronously,
        ' so isFocused is already correct by the time we paint below.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0

        SetCapture( hwnd )
        pBtn->isPressed = true
        pBtn->isHot     = true
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_LBUTTONUP
        if pBtn = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window
        ' losing capture SYNCHRONOUSLY, and our handler for it cancels the press -- so reading
        ' isPressed after the release would always find FALSE and nothing would ever click.
        dim as boolean bWasPressed = pBtn->isPressed
        ' Release capture unconditionally, before any callback runs. Nothing below -- not a
        ' callback, not a host handler -- gets the chance to leave it held.
        if GetCapture() = hwnd then ReleaseCapture()

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bInside = isMouseOverRECT( hwnd, rcClient )

        pBtn->CancelPress()
        InvalidateRect( hwnd, NULL, FALSE )

        ' The result is deliberately IGNORED here (see BTN_MessageCallbackFunc).
        CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release with the cursor still inside is a click. Press, slide
        ' off, release = nothing happened, which is the whole reason for capture.
        dim as boolean bClick = (bWasPressed andalso bInside)
        if bClick then CButton_FireClick( pBtn, hwnd )
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell). The
        ' gesture is over; forget it and repaint.
        if pBtn then
            pBtn->CancelPress()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (CPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pBtn then
            if CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' Focus is a FACT reported by the system, not an action a host can veto, so the state
        ' is updated first and the callback's result is IGNORED. A host that wants to refuse
        ' focus must not make the control a tabstop in the first place.
        if pBtn then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pBtn->isFocused then
                pBtn->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the two keys we actually consume, and only when we are being asked about a
        ' specific message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the
        ' control would swallow the very navigation it just opted into by being a tabstop.
        ' Claiming nothing would let a host's IsDialogMessage pump route Enter somewhere else
        ' before the control ever saw it. Per-message is the only answer that gets both right.
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
        if pBtn = 0 then exit select
        if pBtn->isEnabled = false then return 0
        if CButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        select case wParam
        case VK_SPACE, VK_RETURN
            ' Same path a completed click takes, so keyboard and mouse cannot drift apart: both
            ' are user action, both notify.
            '
            ' Note we claim Enter only while WE are focused (that is all WM_GETDLGCODE above
            ' answers for). isDefault does NOT make this control grab Enter from elsewhere in
            ' the dialog -- it is appearance only, and routing the dialog's default key is the
            ' host's job.
            CButton_FireClick( pBtn, hwnd )
            return 0
        end select

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of CButton_SetEnabled still gets the greyed rendering
        ' rather than a control that looks live but eats input.
        if pBtn then
            pBtn->isEnabled = (wParam <> 0)
            if pBtn->isEnabled = false then
                pBtn->isHot = false
                pBtn->CancelPress()
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_SETTEXT
        ' The caption has ONE store, reached through two doors: CButton_SetText and this
        ' message. DefWindowProc is deliberately not called -- letting it keep a second copy in
        ' the window's own text would give GetWindowText an answer that could drift from what
        ' is painted.
        if pBtn = 0 then exit select
        if lParam = 0 then
            pBtn->wszText = ""
        else
            pBtn->wszText = *cast( wstring ptr, lParam )
        end if
        pBtn->Refresh()
        CButton_ApplyAutoSize( pBtn )
        return TRUE

    case WM_GETTEXT
        ' wParam = buffer size in CHARACTERS including the terminator; return value excludes
        ' it. Copied by hand rather than through a wstring assignment, because the caller's
        ' buffer is not ours to overrun.
        if pBtn = 0 then exit select
        dim as long nMax = cast( long, wParam )
        if nMax <= 0 then return 0
        dim as ushort ptr pDst = cast( ushort ptr, lParam )
        if pDst = 0 then return 0
        dim as long nLen = len( pBtn->wszText )
        if nLen > (nMax - 1) then nLen = nMax - 1
        if nLen > 0 then
            dim as ushort ptr pSrc = cast( ushort ptr, pBtn->wszText.vptr )
            if pSrc then
                for i as long = 0 to nLen - 1
                    pDst[i] = pSrc[i]
                next
            else
                nLen = 0
            end if
        end if
        pDst[nLen] = 0
        return nLen

    case WM_GETTEXTLENGTH
        ' Required, not optional: GetWindowText asks this first and allocates from the answer,
        ' so without it a caller gets an empty string no matter what WM_GETTEXT would have said.
        if pBtn = 0 then exit select
        return len( pBtn->wszText )

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply it.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pBtn then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' Store in an instance-lifetime buffer and point lpszText at it. NOT in
                ' wszTooltip: that field is the host's authored text, and a callback's answer
                ' written there would silently become stored state.
                pBtn->wszTipBuf = CButton_ResolveTooltipText( pBtn )
                pdi->lpszText = cast( LPWSTR, pBtn->wszTipBuf.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pBtn then
            ' Don't let the hot-tracking timer outlive the window...
            if pBtn->hotTimerOn then
                KillTimer( hwnd, IDT_CBUTTON_HOTTRACK )
                pBtn->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pBtn->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored button data pointer. The two fonts are the host's and are left
        ' alone; the tooltip window is ours and goes with us.
        if pBtn then
            if IsWindow(pBtn->hToolTip) then DestroyWindow(pBtn->hToolTip)
            delete(pBtn)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Create a tooltip that pulls its text on demand (LPSTR_TEXTCALLBACK) for the whole control.
' TTN_GETDISPINFOW therefore goes to TOOLINFO.hwnd, which is the control itself (NOT the parent
' form), and CButton_WndProc answers it.
' ========================================================================================
private function CButton_AddTooltip( byval hCtrl as HWND ) as HWND
    if IsWindow(hCtrl) = 0 then return 0
    dim as HWND hTooltip = CreateWindowExW( 0, "tooltips_class32", "", _
                                            WS_POPUP or TTS_ALWAYSTIP, _
                                            0, 0, 0, 0, NULL, NULL, NULL, NULL )
    if hTooltip = 0 then return 0
    SetWindowPos( hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_NOACTIVATE )

    dim tti as TOOLINFOW
    ' cbSize must match the running comctl32 version (see AfxNova AfxAddTooltip).
    #ifdef __FB_64BIT__
        if AfxComCtlVersion < 600 then tti.cbSize = 64 else tti.cbSize = 72
    #else
        if AfxComCtlVersion < 600 then tti.cbSize = 44 else tti.cbSize = 48
    #endif
    tti.uFlags   = TTF_IDISHWND or TTF_SUBCLASS
    tti.hwnd     = hCtrl
    tti.uId      = cast(UINT_PTR, hCtrl)
    tti.hinst    = GetModuleHandleW(NULL)
    tti.lpszText = LPSTR_TEXTCALLBACK
    SendMessageW( hTooltip, TTM_ADDTOOLW, 0, cast(LPARAM, @tti) )
    return hTooltip
end function


' ========================================================================================
' CButton_Create
' ========================================================================================
function CButton_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle parameter to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that supplies
    ' only dwStyle -- which is what every sibling does -- silently declares itself a CONTAINER.
    ' The dialog manager then descends into it hunting for tabstops, finds no children, and
    ' SKIPS IT: the control becomes unreachable by Tab. (It does not break the rest of the form
    ' -- that was measured. CButton's demo merely looked completely dead because every control
    ' on it was a CButton.) Measured, not theorised; see REFACTOR_PLAN.md.
    '
    ' A leaf control must be a tab ITEM, not a tab container. WS_EX_WINDOWEDGE goes with it --
    ' it is a raised-border hint that means nothing to a window drawing its own chrome.
    '
    ' DO NOT COPY THIS LINE INTO A WRAP-A-REAL-CHILD CONTROL. CListBox, CTextBox and
    ' CNumericUpDown all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner child
    ' and their container is a real container; passing 0 there would make them unreachable. The
    ' test is "does this window have children the dialog manager should find?", not "is this a
    ' control?".
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @CButton_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header. It would
    ' turn every second rapid click into a WM_LBUTTONDBLCLK the control would drop.

    dim as CBUTTON ptr pBtn = new CBUTTON
    pBtn->hWin       = hCtrl
    pBtn->idc_Button = CtrlID
    pBtn->id         = CtrlID     ' sensible default; CButton_SetID overrides it

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). The two THICKNESS values are not scaled
    ' at all, here or anywhere -- a hairline stays a hairline.
    pBtn->nPadLeft    = pWindow->ScaleX( CBUTTON_DEFAULT_PADLEFT )
    pBtn->nPadRight   = pWindow->ScaleX( CBUTTON_DEFAULT_PADRIGHT )
    pBtn->nPadTop     = pWindow->ScaleY( CBUTTON_DEFAULT_PADTOP )
    pBtn->nPadBottom  = pWindow->ScaleY( CBUTTON_DEFAULT_PADBOTTOM )
    pBtn->nIconGap    = pWindow->ScaleX( CBUTTON_DEFAULT_ICONGAP )
    pBtn->nIconWidth  = pWindow->ScaleX( CBUTTON_DEFAULT_ICONWIDTH )
    pBtn->nIconHeight = pWindow->ScaleY( CBUTTON_DEFAULT_ICONHEIGHT )
    pBtn->nCurvature  = pWindow->ScaleY( CBUTTON_DEFAULT_CURVATURE )
    pBtn->nFocusGap   = pWindow->ScaleY( CBUTTON_DEFAULT_FOCUSGAP )

    pBtn->hToolTip = CButton_AddTooltip( hCtrl )

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pBtn)

    function = hCtrl

end function


' ========================================================================================
' Content.  All SILENT -- they change what is drawn, never what was clicked.
' ========================================================================================
function CButton_GetText( byval hButton as HWND ) as DWSTRING
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszText
    return wszOut
end function

sub CButton_SetText( byval hButton as HWND, byval Text as DWSTRING )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszText = Text
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetGlyphLeft( byval hButton as HWND ) as DWSTRING
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszGlyphLeft
    return wszOut
end function

sub CButton_SetGlyphLeft( byval hButton as HWND, byval Glyph as DWSTRING )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszGlyphLeft = Glyph
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetGlyphRight( byval hButton as HWND ) as DWSTRING
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszGlyphRight
    return wszOut
end function

sub CButton_SetGlyphRight( byval hButton as HWND, byval Glyph as DWSTRING )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszGlyphRight = Glyph
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetID( byval hButton as HWND ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->id
end function

sub CButton_SetID( byval hButton as HWND, byval id as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->id = id
end sub

function CButton_GetItemData( byval hButton as HWND ) as integer
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->itemData
end function

sub CButton_SetItemData( byval hButton as HWND, byval itemData as integer )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->itemData = itemData
end sub


' ========================================================================================
' State.
' ========================================================================================
function CButton_GetEnabled( byval hButton as HWND ) as boolean
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isEnabled
end function

' Goes through EnableWindow so the disable is enforced by the SYSTEM rather than being a
' cosmetic flag: a disabled window gets no mouse input and the dialog manager's Tab skips it.
' WM_ENABLE syncs our own flag and clears hover and any live press.
sub CButton_SetEnabled( byval hButton as HWND, byval isEnabled as boolean )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->isEnabled = isEnabled then exit sub
    EnableWindow( hButton, iif( isEnabled, TRUE, FALSE ) )
end sub

function CButton_GetFocused( byval hButton as HWND ) as boolean
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isFocused
end function

function CButton_GetDefault( byval hButton as HWND ) as boolean
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isDefault
end function

' Appearance only -- it swaps one colour and nothing else, so it never re-measures and the
' repaint does not need a background erase. Nothing stops a host marking two buttons default;
' the control does not police it.
sub CButton_SetDefault( byval hButton as HWND, byval isDefault as boolean )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->isDefault = isDefault then exit sub
    pBtn->isDefault = isDefault
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

sub CButton_Refresh( byval hButton as HWND )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->Refresh()
end sub


' ========================================================================================
' Action.  This one DOES notify -- see the header. It is refused on a disabled button, exactly
' as a real click would be, and it does not paint a press flash: it reports an action, it does
' not animate one.
' ========================================================================================
sub CButton_Click( byval hButton as HWND )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    CButton_FireClick( pBtn, hButton )
end sub


' ========================================================================================
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales.
' ========================================================================================
function CButton_GetTextAlign( byval hButton as HWND ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return BTN_ALIGN_CENTER
    return pBtn->nTextAlign
end function

' Alignment moves the caption INSIDE the span, never the span itself, so it cannot change the
' ideal size and does not need an auto-size pass. An out-of-range value is ignored rather than
' clamped: silently turning a typo into LEFT would hide it.
sub CButton_SetTextAlign( byval hButton as HWND, byval nTextAlign as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    select case nTextAlign
    case BTN_ALIGN_LEFT, BTN_ALIGN_CENTER, BTN_ALIGN_RIGHT
        if pBtn->nTextAlign = nTextAlign then exit sub
        pBtn->nTextAlign = nTextAlign
        if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
    end select
end sub

sub CButton_GetPadding( byval hButton as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
    nLeft = 0 : nTop = 0 : nRight = 0 : nBottom = 0
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nLeft   = pBtn->nPadLeft
    nTop    = pBtn->nPadTop
    nRight  = pBtn->nPadRight
    nBottom = pBtn->nPadBottom
end sub

sub CButton_SetPadding( byval hButton as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nLeft   < 0 then nLeft   = 0
    if nTop    < 0 then nTop    = 0
    if nRight  < 0 then nRight  = 0
    if nBottom < 0 then nBottom = 0
    pBtn->nPadLeft   = nLeft
    pBtn->nPadTop    = nTop
    pBtn->nPadRight  = nRight
    pBtn->nPadBottom = nBottom
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetIconGap( byval hButton as HWND ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nIconGap
end function

sub CButton_SetIconGap( byval hButton as HWND, byval nIconGap as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nIconGap < 0 then nIconGap = 0
    pBtn->nIconGap = nIconGap
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

sub CButton_GetIconSize( byval hButton as HWND, byref nIconWidth as long, byref nIconHeight as long )
    nIconWidth = 0 : nIconHeight = 0
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nIconWidth  = pBtn->nIconWidth
    nIconHeight = pBtn->nIconHeight
end sub

sub CButton_SetIconSize( byval hButton as HWND, byval nIconWidth as long, byval nIconHeight as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nIconWidth  < 0 then nIconWidth  = 0
    if nIconHeight < 0 then nIconHeight = 0
    pBtn->nIconWidth  = nIconWidth
    pBtn->nIconHeight = nIconHeight
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetCornerCurvature( byval hButton as HWND ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nCurvature
end function

sub CButton_SetCornerCurvature( byval hButton as HWND, byval nCurvature as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pBtn->nCurvature = nCurvature
    ' Chrome only -- no size change, so no auto-size pass.
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

function CButton_GetBorderThickness( byval hButton as HWND ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nBorderThick
end function

' The border is drawn INSIDE the chrome rect, so its thickness does not change the ideal size.
' It is not DPI-scaled anywhere: a hairline stays a hairline.
sub CButton_SetBorderThickness( byval hButton as HWND, byval nThickness as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pBtn->nBorderThick = nThickness
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

sub CButton_GetFocusRing( byval hButton as HWND, byref nGap as long, byref nThickness as long )
    nGap = 0 : nThickness = 0
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nGap       = pBtn->nFocusGap
    nThickness = pBtn->nFocusThick
end sub

' The ring band is reserved UNCONDITIONALLY, focused or not, so the button never jumps when
' focus arrives -- which is exactly why changing either value changes the IDEAL SIZE.
sub CButton_SetFocusRing( byval hButton as HWND, byval nGap as long, byval nThickness as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nGap       < 0 then nGap       = 0
    if nThickness < 0 then nThickness = 0
    pBtn->nFocusGap   = nGap
    pBtn->nFocusThick = nThickness
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetAutoSize( byval hButton as HWND ) as boolean
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->bAutoSize
end function

sub CButton_SetAutoSize( byval hButton as HWND, byval bAutoSize as boolean )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->bAutoSize = bAutoSize
    ' Turning it ON applies immediately rather than waiting for the next content change.
    CButton_ApplyAutoSize( pBtn )
end sub

sub CButton_GetIdealSize( byval hButton as HWND, byref nWidth as long, byref nHeight as long )
    nWidth  = 0
    nHeight = 0
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    nWidth  = pBtn->nIdealW
    nHeight = pBtn->nIdealH
end sub

function CButton_GetButtonRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false        ' still no client area
    rc = pBtn->rcButton
    return true
end function

function CButton_GetTextRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcText
    return true
end function

function CButton_GetIconLeftRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcIconLeft
    return true
end function

function CButton_GetIconRightRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcIconRight
    return true
end function

function CButton_GetVisualRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcVisual
    return true
end function


' ========================================================================================
' Appearance.  Both fonts are BORROWED, never owned.
' ========================================================================================
sub CButton_GetColors( byval hButton as HWND, byval pColors as CBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    *pColors = pBtn->colors
end sub

sub CButton_SetColors( byval hButton as HWND, byval pColors as CBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->colors = *pColors
    ' Colours only -- nothing measured changes, so no layout and no auto-size.
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

function CButton_GetFont( byval hButton as HWND ) as HFONT
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->hTextFont
end function

' The CAPTION font, and therefore the MEASURING font: changing it re-measures and can change
' the ideal size.
sub CButton_SetFont( byval hButton as HWND, byval hTextFont as HFONT )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->hTextFont = hTextFont
    pBtn->Refresh()
    CButton_ApplyAutoSize( pBtn )
end sub

function CButton_GetGlyphFont( byval hButton as HWND ) as HFONT
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->hGlyphFont
end function

' The ICON font. A PAINT-TIME INPUT ONLY: the icon cell is declared by CButton_SetIconSize and
' is never measured, so changing this repaints but never re-lays-out and never resizes.
sub CButton_SetGlyphFont( byval hButton as HWND, byval hGlyphFont as HFONT )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->hGlyphFont = hGlyphFont
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub


' ========================================================================================
' Tooltips.
' ========================================================================================
function CButton_GetTooltipText( byval hButton as HWND ) as DWSTRING
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszTooltip
    return wszOut
end function

sub CButton_SetTooltipText( byval hButton as HWND, byval Text as DWSTRING )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszTooltip = Text
end sub

function CButton_GetTooltipHandle( byval hButton as HWND ) as HWND
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->hToolTip
end function

sub CButton_SetHoverTime( byval hButton as HWND, byval milliseconds as long )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->HoverTime = milliseconds
    if pBtn->hToolTip then
        SendMessageW( pBtn->hToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, cast(LPARAM, milliseconds) )
    end if
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub CButton_SetPaintCallback( byval hButton as HWND, byval usersub as BTN_PaintCallbackSub )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->PaintCallback = usersub
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, TRUE )
end sub

sub CButton_SetMessageCallback( byval hButton as HWND, byval userfunc as BTN_MessageCallbackFunc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->MessageCallback = userfunc
end sub

sub CButton_SetTooltipCallback( byval hButton as HWND, byval userfunc as BTN_TooltipCallbackFunc )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->TooltipCallback = userfunc
end sub

sub CButton_SetClickCallback( byval hButton as HWND, byval usersub as BTN_ClickCallbackSub )
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->ClickCallback = usersub
end sub


' ========================================================================================
' Which rect does this part name? Shared by the tone probe and nothing else, but factored out
' so the probe body stays about pixels rather than about rect selection.
' ========================================================================================
private function CButton_PartRect( byval pBtn as CBUTTON ptr, byval nPart as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    if pBtn = 0 then return rc
    select case nPart
    case BTN_PART_TEXT
        rc = pBtn->rcText
    case BTN_PART_ICONLEFT
        rc = pBtn->rcIconLeft
    case BTN_PART_ICONRIGHT
        rc = pBtn->rcIconRight
    case else
        rc = pBtn->rcButton
    end select
    return rc
end function


' ========================================================================================
' Render the control OFFSCREEN with its current painter and count the distinct colours inside
' one of its rects. See the header in CButton.bi for what this is for and why it is public.
'
' THREE THINGS HERE ARE LOAD-BEARING, and all three come from the write-up in Learnings.md:
'
'   1. The PAINTINFO is filled by CButton_FillPaintInfo -- the SAME routine WM_PAINT uses. The
'      first version of this probe elsewhere in the family hand-built the struct and left
'      rcVisual zeroed; PaintRectFactory bails on a negative extent, so the focus-ring call
'      became a no-op and the A/B came back identical. A probe that skips the code path it
'      exists to measure is worse than no probe.
'   2. isFocused is forced TRUE. The ring is the step the defect lives on -- it is drawn last,
'      over the largest rect -- so a probe run on an unfocused control measures nothing.
'   3. The buffer is given a CACHED memDC (the four-argument BeginDoubleBuffer), so
'      EndDoubleBuffer flushes GDI+ and blits WITHOUT deleting the bitmap underneath us. GDI+
'      batches: reading the bits before that flush would sample a surface the shapes have not
'      reached yet.
' ========================================================================================
function CButton_CountRenderedTones( byval hButton as HWND, byval nPart as long ) as long
    dim as CBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return 0          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hButton, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return 0

    dim as RECT rcPart = CButton_PartRect( pBtn, nPart )
    if IsRectEmpty( @rcPart ) then return 0

    ' Clamp to the client: rcVisual can legitimately extend past it when the host sized the
    ' control smaller than ideal, and the DIB only covers the client.
    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return 0

    dim as HDC hRef = GetDC( hButton )
    if hRef = 0 then return 0
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hButton, hRef )
        return 0
    end if

    ' 32bpp top-down, matching what CBufferPaint.SetupBitmap builds, so the pixels we read are
    ' laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( hButton, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return 0
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    dim as boolean bSavedFocus = pBtn->isFocused
    pBtn->isFocused = true

    scope
        dim as CBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hButton, hMem, rcClient, hMem )

        dim as long nMood = CButton_ResolveMood( pBtn->isEnabled, pBtn->isPressed, pBtn->isHot )
        dim as COLORREF clrFill, clrFore, clrBorder, clrIcon
        CButton_ResolveColors( @pBtn->colors, nMood, pBtn->isDefault, _
                               clrFill, clrFore, clrBorder, clrIcon )
        b.SetBackColor( clrFill )
        b.PaintClientRect()

        dim p as CBUTTON_PAINTINFO
        CButton_FillPaintInfo( pBtn, hButton, @b, @p )

        if pBtn->PaintCallback then
            pBtn->PaintCallback( @p )
        else
            CButton_RenderDefault( pBtn, @b, @p )
        end if

        b.EndDoubleBuffer()
    end scope

    pBtn->isFocused = bSavedFocus

    ' Count distinct tones. The alpha byte is masked off: GDI text leaves it at 0 and GDI+
    ' shapes leave it at 255, so an unmasked compare would report two tones for one colour.
    dim tones(0 to 63) as ulong
    dim as long nTones = 0
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF
            dim as boolean bFound = false
            for t as long = 0 to nTones - 1
                if tones(t) = c then bFound = true : exit for
            next
            if bFound = false then
                if nTones < 64 then
                    tones(nTones) = c
                    nTones += 1
                end if
            end if
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )
    return nTones
end function


' ========================================================================================
' Self-test.  Gated on CBUTTON_SELFTEST=1.
' ========================================================================================
dim shared as long gBTN_TestCount
dim shared as long gBTN_FailCount

private sub BTNTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gBTN_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gBTN_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function BTNTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function


sub CButton_RunSelfTest( byval hWndParent as HWND )
    if environ("CBUTTON_SELFTEST") <> "1" then exit sub

    gBTN_TestCount = 0
    gBTN_FailCount = 0
    print "=== CButton self-test ==="

    dim as HWND h = CButton_Create( hWndParent, 9801 )
    if h = 0 then
        print "FAIL  create   [CButton_Create returned 0]"
        exit sub
    end if

    dim as CBUTTON ptr pB = GetButtonPointer(h)
    if pB = 0 then
        print "FAIL  pointer   [GetButtonPointer returned 0]"
        DestroyWindow(h)
        exit sub
    end if

    dim as string sD                     ' every trace string is built here first: PRINT on a
                                         ' DWSTRING, or on a string & wstring concat,
                                         ' interleaves nulls (Learnings.md).
    dim as long w, hh
    dim as RECT rcBtn, rcTxt, rcIL, rcIR, rcVis

    ' Pin every layout input to a known value so the arithmetic below is exact rather than
    ' DPI-dependent. Only the CAPTION stays measured, and every assertion that involves it is
    ' written against pB->nTextWidth rather than against a literal.
    CButton_SetPadding( h, 10, 5, 10, 5 )
    CButton_SetIconGap( h, 8 )
    CButton_SetIconSize( h, 16, 16 )
    CButton_SetFocusRing( h, 2, 1 )              ' ringPad = 3
    CButton_SetBorderThickness( h, 1 )
    CButton_SetCornerCurvature( h, 8 )

    ' Plain letters, not Segoe Fluent Icons codepoints: the control does not care what the
    ' glyph string is, and a letter renders under the stock GUI font, so the tone probe at the
    ' end cannot fail for want of an icon font being installed.
    const GLYPH_L = "A"
    const GLYPH_R = "B"
    const CAPTION = "Test Caption"

    dim as long ringPad = pB->RingPad()

    ' ---- 1. ideal size is valid BEFORE the control has ever been sized ------------------
    CButton_SetText( h, CAPTION )
    CButton_GetIdealSize( h, w, hh )
    sD = "w=" & str(w) & " h=" & str(hh)
    BTNTest_Check( "ideal size before first size", (w > 0) andalso (hh > 0), sD )

    ' ---- 2. the ideal size is built from the parts we think it is -----------------------
    dim as long textOnlyW = w
    dim as long expectW = (2 * ringPad) + 10 + pB->nTextWidth + 10
    sD = "ideal=" & str(w) & " expect=" & str(expectW) & " textW=" & str(pB->nTextWidth)
    BTNTest_Check( "ideal width = ring+padL+text+padR", (w = expectW), sD )

    dim as long expectH = (2 * ringPad) + 5 + pB->nTextHeight + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectH) & " textH=" & str(pB->nTextHeight)
    BTNTest_Check( "ideal height = ring+padT+textH+padB", (hh = expectH), sD )

    ' ---- 3. a left glyph costs exactly one cell plus one gap ----------------------------
    CButton_SetGlyphLeft( h, GLYPH_L )
    CButton_GetIdealSize( h, w, hh )
    sD = "before=" & str(textOnlyW) & " after=" & str(w) & " delta=" & str(w - textOnlyW)
    BTNTest_Check( "left glyph adds iconW + gap", (w - textOnlyW) = (16 + 8), sD )
    dim as long leftOnlyW = w

    ' ---- 4. a right glyph costs the same again ------------------------------------------
    CButton_SetGlyphRight( h, GLYPH_R )
    CButton_GetIdealSize( h, w, hh )
    sD = "before=" & str(leftOnlyW) & " after=" & str(w) & " delta=" & str(w - leftOnlyW)
    BTNTest_Check( "right glyph adds iconW + gap", (w - leftOnlyW) = (16 + 8), sD )
    dim as long bothW = w

    ' ---- 5. THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION -------------------------------
    ' Direction 1: drop the caption and both gaps must go with it, not just the text width.
    CButton_SetText( h, "" )
    CButton_GetIdealSize( h, w, hh )
    dim as long expectIconOnly = (2 * ringPad) + 10 + 16 + 16 + 10
    sD = "ideal=" & str(w) & " expect=" & str(expectIconOnly) & " (no gaps charged)"
    BTNTest_Check( "no caption = no gap on either side", (w = expectIconOnly), sD )

    ' ...and the content height falls back to the ICON, not to the font.
    dim as long expectIconH = (2 * ringPad) + 5 + 16 + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectIconH) & " textH=" & str(pB->nTextHeight)
    BTNTest_Check( "no caption = height comes from the icon", (hh = expectIconH), sD )

    ' Direction 2: put it back and both gaps return with it.
    CButton_SetText( h, CAPTION )
    CButton_GetIdealSize( h, w, hh )
    sD = "restored=" & str(w) & " expect=" & str(bothW)
    BTNTest_Check( "caption restored = gaps restored", (w = bothW), sD )

    ' ---- 6. absent parts give EMPTY rects, not zero-width ones at an edge ---------------
    ' Size the control to ideal first; every rect query below needs a client area.
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    CButton_SetGlyphRight( h, "" )
    CButton_GetIconRightRect( h, rcIR )
    sD = "rcIconRight=" & BTNTest_RectStr( rcIR )
    BTNTest_Check( "no right glyph = empty rect", IsRectEmpty( @rcIR ) <> 0, sD )
    CButton_SetGlyphRight( h, GLYPH_R )

    CButton_SetText( h, "" )
    CButton_GetTextRect( h, rcTxt )
    sD = "rcText=" & BTNTest_RectStr( rcTxt )
    BTNTest_Check( "no caption = empty text rect", IsRectEmpty( @rcTxt ) <> 0, sD )
    CButton_SetText( h, CAPTION )

    ' ---- 7. the icons are PINNED to the padding ----------------------------------------
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    CButton_GetButtonRect( h, rcBtn )
    CButton_GetIconLeftRect( h, rcIL )
    CButton_GetIconRightRect( h, rcIR )
    sD = "btn=" & BTNTest_RectStr( rcBtn ) & " iconL=" & BTNTest_RectStr( rcIL )
    BTNTest_Check( "left icon pinned at padLeft", (rcIL.left - rcBtn.left) = 10, sD )
    sD = "btn=" & BTNTest_RectStr( rcBtn ) & " iconR=" & BTNTest_RectStr( rcIR )
    BTNTest_Check( "right icon pinned at padRight", (rcBtn.right - rcIR.right) = 10, sD )
    sD = "w=" & str(rcIL.right - rcIL.left) & "x" & str(rcIL.bottom - rcIL.top)
    BTNTest_Check( "icon cell is the declared size", _
                   ((rcIL.right - rcIL.left) = 16) andalso ((rcIL.bottom - rcIL.top) = 16), sD )

    ' ...and a much longer caption does not move them (the control is re-sized to the new
    ' ideal, which is what a host would do).
    CButton_SetText( h, "A Very Much Longer Caption Indeed" )
    CButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    CButton_GetButtonRect( h, rcBtn )
    CButton_GetIconLeftRect( h, rcIL )
    CButton_GetIconRightRect( h, rcIR )
    sD = "padL=" & str(rcIL.left - rcBtn.left) & " padR=" & str(rcBtn.right - rcIR.right)
    BTNTest_Check( "caption length never moves the icons", _
                   ((rcIL.left - rcBtn.left) = 10) andalso ((rcBtn.right - rcIR.right) = 10), sD )
    CButton_SetText( h, CAPTION )
    CButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 8. the caption SPAN sits exactly between the two icon blocks -------------------
    CButton_GetButtonRect( h, rcBtn )
    CButton_GetTextRect( h, rcTxt )
    sD = "text=" & BTNTest_RectStr( rcTxt ) & " btn=" & BTNTest_RectStr( rcBtn )
    BTNTest_Check( "span starts after padLeft + leftBlock", _
                   (rcTxt.left - rcBtn.left) = (10 + 16 + 8), sD )
    BTNTest_Check( "span ends before rightBlock + padRight", _
                   (rcBtn.right - rcTxt.right) = (10 + 16 + 8), sD )

    ' ---- 9. everything is vertically centred in the chrome ------------------------------
    dim as long btnH = rcBtn.bottom - rcBtn.top
    sD = "iconTop=" & str(rcIL.top - rcBtn.top) & " expect=" & str((btnH - 16) \ 2)
    BTNTest_Check( "icon v-centred in the chrome", (rcIL.top - rcBtn.top) = ((btnH - 16) \ 2), sD )
    sD = "textTop=" & str(rcTxt.top - rcBtn.top) & " expect=" & str((btnH - pB->nTextHeight) \ 2)
    BTNTest_Check( "caption v-centred in the chrome", _
                   (rcTxt.top - rcBtn.top) = ((btnH - pB->nTextHeight) \ 2), sD )

    ' ---- 10. the focus-ring band is reserved, and is the whole difference ---------------
    CButton_GetVisualRect( h, rcVis )
    sD = "visual=" & BTNTest_RectStr( rcVis ) & " btn=" & BTNTest_RectStr( rcBtn ) & _
         " ringPad=" & str(ringPad)
    BTNTest_Check( "visual = chrome inflated by ringPad", _
                   (rcBtn.left - rcVis.left = ringPad) andalso _
                   (rcVis.right - rcBtn.right = ringPad) andalso _
                   (rcBtn.top - rcVis.top = ringPad) andalso _
                   (rcVis.bottom - rcBtn.bottom = ringPad), sD )

    ' THE NO-JUMP GUARANTEE: geometry with focus must be byte-identical to geometry without it.
    ' Asserted rather than assumed, because "the band is reserved unconditionally" is exactly
    ' the kind of claim an optimisation would quietly break.
    dim as RECT rcBtnUnfocused = rcBtn
    dim as RECT rcTxtUnfocused = rcTxt
    dim as boolean bSavedFocus = pB->isFocused
    pB->isFocused = true
    pB->bLayoutDirty = true
    CButton_GetButtonRect( h, rcBtn )
    CButton_GetTextRect( h, rcTxt )
    pB->isFocused = bSavedFocus
    sD = "unfocused=" & BTNTest_RectStr( rcBtnUnfocused ) & " focused=" & BTNTest_RectStr( rcBtn )
    BTNTest_Check( "focus does not move anything", _
                   (EqualRect( @rcBtn, @rcBtnUnfocused ) <> 0) andalso _
                   (EqualRect( @rcTxt, @rcTxtUnfocused ) <> 0), sD )

    ' ---- 11. overflow is honest: the span collapses, the icons do not -------------------
    dim as long narrow = 2 * ringPad + 10 + 16 + 16 + 10        ' exactly no room for a caption
    SetWindowPos( h, 0, 0, 0, narrow, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    CButton_GetButtonRect( h, rcBtn )
    CButton_GetTextRect( h, rcTxt )
    CButton_GetIconLeftRect( h, rcIL )
    CButton_GetIconRightRect( h, rcIR )
    sD = "text=" & BTNTest_RectStr( rcTxt ) & " width=" & str(rcTxt.right - rcTxt.left)
    BTNTest_Check( "too narrow: span degenerates, never goes negative", _
                   (rcTxt.right >= rcTxt.left) andalso ((rcTxt.right - rcTxt.left) = 0), sD )
    sD = "iconL=" & str(rcIL.right - rcIL.left) & " iconR=" & str(rcIR.right - rcIR.left)
    BTNTest_Check( "too narrow: icons keep their size", _
                   ((rcIL.right - rcIL.left) = 16) andalso ((rcIR.right - rcIR.left) = 16), sD )

    ' ---- 12. the ideal size does NOT follow the client -----------------------------------
    ' The ellipsis is a paint-time concern only, so a host that sizes with GetIdealSize can
    ' never see one -- and that is only true if starving the control leaves the measurement
    ' alone.
    dim as long w2, h2
    CButton_GetIdealSize( h, w2, h2 )
    sD = "ideal now=" & str(w2) & "x" & str(h2) & " expect=" & str(bothW) & "x" & str(hh)
    BTNTest_Check( "ideal size unaffected by a starved client", _
                   (w2 = bothW) andalso (h2 = hh), sD )
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 13. mood precedence, as a truth table ------------------------------------------
    ' disabled > pressed > hot > idle. All eight combinations, because the interesting ones are
    ' the contradictory states (disabled AND pressed) that only a race can produce.
    BTNTest_Check( "mood: enabled,idle", _
                   CButton_ResolveMood( true, false, false ) = BTN_MOOD_IDLE, "" )
    BTNTest_Check( "mood: enabled,hot", _
                   CButton_ResolveMood( true, false, true ) = BTN_MOOD_HOT, "" )
    BTNTest_Check( "mood: enabled,pressed", _
                   CButton_ResolveMood( true, true, false ) = BTN_MOOD_PRESSED, "" )
    BTNTest_Check( "mood: pressed beats hot", _
                   CButton_ResolveMood( true, true, true ) = BTN_MOOD_PRESSED, "" )
    BTNTest_Check( "mood: disabled,idle", _
                   CButton_ResolveMood( false, false, false ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats hot", _
                   CButton_ResolveMood( false, false, true ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats pressed", _
                   CButton_ResolveMood( false, true, false ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats both", _
                   CButton_ResolveMood( false, true, true ) = BTN_MOOD_DISABLED, "" )

    ' ---- 14. isDefault overlays the border in EVERY mood --------------------------------
    dim as CBUTTON_COLORS c
    CButton_GetColors( h, @c )
    ' Make every field distinguishable, so an assertion cannot pass by two colours happening
    ' to be equal (which they ARE by default -- the borderless look).
    c.BorderColor         = BGR(1,1,1)
    c.BorderColorHot      = BGR(2,2,2)
    c.BorderColorPressed  = BGR(3,3,3)
    c.BorderColorDisabled = BGR(4,4,4)
    c.DefaultBorderColor  = BGR(9,9,9)
    CButton_SetColors( h, @c )

    dim as COLORREF cBack, cFore, cBorder, cIcon
    dim as long nMoods(0 to 3) = { BTN_MOOD_IDLE, BTN_MOOD_HOT, BTN_MOOD_PRESSED, BTN_MOOD_DISABLED }
    dim as COLORREF cExpect(0 to 3) = { BGR(1,1,1), BGR(2,2,2), BGR(3,3,3), BGR(4,4,4) }

    for i as long = 0 to 3
        CButton_ResolveColors( @c, nMoods(i), false, cBack, cFore, cBorder, cIcon )
        sD = "mood=" & str(nMoods(i)) & " border=" & hex(cBorder) & " expect=" & hex(cExpect(i))
        BTNTest_Check( "border follows the mood when not default", (cBorder = cExpect(i)), sD )

        ' The same mood, with the flag on: only the border may move.
        dim as COLORREF dBack, dFore, dBorder, dIcon
        CButton_ResolveColors( @c, nMoods(i), true, dBack, dFore, dBorder, dIcon )
        sD = "mood=" & str(nMoods(i)) & " border=" & hex(dBorder) & " expect=" & hex(c.DefaultBorderColor)
        BTNTest_Check( "isDefault overlays the border in this mood", _
                       (dBorder = c.DefaultBorderColor), sD )
        sD = "back/fore/icon unchanged in mood " & str(nMoods(i))
        BTNTest_Check( "isDefault touches ONLY the border", _
                       (dBack = cBack) andalso (dFore = cFore) andalso (dIcon = cIcon), sD )
    next

    ' Put the defaults back before the render probe, so it measures the shipping look.
    dim as CBUTTON_COLORS cFresh
    CButton_SetColors( h, @cFresh )

    ' ---- 15. the window text really is the caption --------------------------------------
    ' ASK THE CONTROL. Reading the field back would assert nothing about the message handlers,
    ' which are the whole point of this section -- a SendMessage whose result is discarded is
    ' an assumption, not an implementation (the CNumericUpDown lesson).
    dim as wstring * 64 wszSet = "Window Text Test"
    SendMessageW( h, WM_SETTEXT, 0, cast( LPARAM, @wszSet ) )

    dim as long nLen = cast( long, SendMessageW( h, WM_GETTEXTLENGTH, 0, 0 ) )
    sD = "WM_GETTEXTLENGTH=" & str(nLen) & " expect=" & str(len(wszSet))
    BTNTest_Check( "WM_GETTEXTLENGTH reports the caption length", (nLen = len(wszSet)), sD )

    dim as wstring * 64 wszGot
    dim as long nGot = cast( long, SendMessageW( h, WM_GETTEXT, 64, cast( LPARAM, @wszGot ) ) )
    sD = "returned=" & str(nGot) & " text=[" & str(wszGot) & "]"
    BTNTest_Check( "WM_GETTEXT round-trips the caption", _
                   (nGot = len(wszSet)) andalso (wszGot = wszSet), sD )

    ' Truncation: a short buffer must be filled to nMax-1 and terminated, not overrun.
    dim as wstring * 8 wszShort
    dim as long nShort = cast( long, SendMessageW( h, WM_GETTEXT, 5, cast( LPARAM, @wszShort ) ) )
    sD = "returned=" & str(nShort) & " text=[" & str(wszShort) & "] expect 4 chars"
    BTNTest_Check( "WM_GETTEXT honours the buffer size", _
                   (nShort = 4) andalso (len(wszShort) = 4) andalso (wszShort = "Wind"), sD )

    ' And GetWindowText -- the whole reason the two handlers exist -- must agree with the
    ' control's own getter.
    dim as wstring * 64 wszWin
    GetWindowTextW( h, @wszWin, 64 )
    dim as wstring * 64 wszApi
    wszApi = CButton_GetText( h )
    sD = "GetWindowText=[" & str(wszWin) & "] CButton_GetText=[" & str(wszApi) & "]"
    BTNTest_Check( "GetWindowText agrees with CButton_GetText", (wszWin = wszApi), sD )

    ' SetWindowText must go the other way and re-measure.
    dim as long wBefore = 0, hBefore = 0
    CButton_GetIdealSize( h, wBefore, hBefore )
    SetWindowTextW( h, "Substantially Longer Window Text" )
    CButton_GetIdealSize( h, w2, h2 )
    sD = "before=" & str(wBefore) & " after=" & str(w2)
    BTNTest_Check( "SetWindowText re-measures the button", (w2 > wBefore), sD )
    CButton_SetText( h, CAPTION )

    ' ---- 16. the control is a tab ITEM, not a tab CONTAINER -----------------------------
    ' CWindow.Create defaults dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a
    ' control that passes only dwStyle gets the container flag by accident -- and the dialog
    ' manager then descends into it, finds no children, and abandons the Tab walk for the
    ' WHOLE FORM. That shipped here and is asserted rather than remembered.
    dim as ulong exStyle = cast( ulong, GetWindowLongPtr( h, GWL_EXSTYLE ) )
    sD = "exstyle=&h" & hex(exStyle)
    BTNTest_Check( "NOT a control parent (or Tab dies form-wide)", _
                   (exStyle and WS_EX_CONTROLPARENT) = 0, sD )

    dim as ulong wsStyle = cast( ulong, GetWindowLongPtr( h, GWL_STYLE ) )
    sD = "style=&h" & hex(wsStyle)
    BTNTest_Check( "is a tabstop", (wsStyle and WS_TABSTOP) <> 0, sD )

    ' The end-to-end claim, not just the flag: ask the dialog manager to find us. It walks the
    ' parent's children itself, so this fails if either style above is wrong.
    '
    ' The WS_VISIBLE bit is set DIRECTLY rather than with ShowWindow: the walk tests the style
    ' bit, and doing it this way means no paint, no flash, and no dependence on whether the
    ' host happens to call the self-test before or after it shows the form.
    dim as ulong wsSaved = wsStyle
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsStyle or WS_VISIBLE ) )
    dim as HWND hFound = GetNextDlgTabItem( hWndParent, 0, FALSE )
    dim as boolean bReachable = false
    dim as HWND hScan = hFound
    for i as long = 0 to 63
        if hScan = 0 then exit for
        if hScan = h then
            bReachable = true
            exit for
        end if
        hScan = GetNextDlgTabItem( hWndParent, hScan, FALSE )
        if hScan = hFound then exit for          ' wrapped all the way round
    next
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsSaved ) )
    sD = "first tab item id=" & str(GetDlgCtrlID(hFound)) & " reached us=" & str(bReachable)
    BTNTest_Check( "the dialog manager can reach us by Tab", bReachable, sD )

    ' ---- 17. the render probe: the control is not a solid block -------------------------
    ' See CButton_CountRenderedTones. A part wiped by a PaintBorderRect fill is literally ONE
    ' tone, so the useful signal is the count LEAVING 1 -- not the count being large.
    CButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    dim as long tButton = CButton_CountRenderedTones( h, BTN_PART_BUTTON )
    sD = "distinct tones in the chrome=" & str(tButton)
    BTNTest_Check( "built-in painter does not flood the button", (tButton > 1), sD )

    dim as long tText = CButton_CountRenderedTones( h, BTN_PART_TEXT )
    sD = "distinct tones in the caption span=" & str(tText)
    BTNTest_Check( "the caption actually reached the surface", (tText > 1), sD )

    dim as long tIcon = CButton_CountRenderedTones( h, BTN_PART_ICONLEFT )
    sD = "distinct tones in the left icon cell=" & str(tIcon)
    BTNTest_Check( "the left glyph actually reached the surface", (tIcon > 1), sD )

    DestroyWindow(h)

    dim as string sSummary = "=== CButton: " & str(gBTN_TestCount - gBTN_FailCount) & _
                             "/" & str(gBTN_TestCount) & " assertions passed"
    if gBTN_FailCount > 0 then sSummary &= "  (" & str(gBTN_FailCount) & " FAILED)"
    print sSummary
end sub
