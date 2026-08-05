''
''  PsButton.inc  --  owner-drawn command button (glyph + caption + glyph)
''

#include once "PsButton.bi"
' The image loader's implementation, pulled in here so a host that already includes PsButton.inc
' needs no extra include line (PsColorPicker/PsVScrollBar precedent). #include once guards it if
' the host includes it too.
#include once "PsImage.inc"
' Likewise the tooltip backend switch, which pulls PsTooltip.inc in behind it. Adopting this
' control therefore still costs the host no new include line -- but it DOES add two files to
' the copy list (PsTipHost + PsTooltip), which the README records.
#include once "PsTipHost.inc"


' Forward declaration: the content setters re-apply auto-size, and they are defined well above
' the layout section where it lives.
declare sub PsButton_ApplyAutoSize( byval pBtn as PSBUTTON ptr )


function GetButtonPointer( byval hButton as HWND ) as PSBUTTON ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hButton)
    if pWindow = 0 then return 0
    return cast( PSBUTTON ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not.
' WM_LBUTTONUP is one of them here, because this control HOLDS MOUSE CAPTURE across a press
' and the up-message is what releases it. That reverses the contract in PsComboBox, which this
' file was seeded from: a "no capture, result honoured" note copied in from there would be a
' lie.
' ========================================================================================
private function PsButton_CallMessage( _
            byval pBtn   as PSBUTTON ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pBtn = 0 then return false
    if pBtn->MessageCallback = 0 then return false

    dim as PSBUTTON_MESSAGEINFO m
    m.hButton = hwnd
    m.uMsg    = uMsg
    m.wParam  = wParam
    m.lParam  = lParam
    return pBtn->MessageCallback( @m )
end function


' Resolve the tooltip text: the control's own text if it has any, else the host callback.
' NO CAPTION FALLBACK -- a button's caption is already on screen, so falling back to it would
' pop a tip that repeats what the user is looking at.
private function PsButton_ResolveTooltipText( byval pBtn as PSBUTTON ptr ) as DWSTRING
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
' Nothing in here touches a GDI object or a device context -- PsBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' THE COLOURS COME OUT OF PsButton_ResolveMood + PsButton_ResolveColors, not out of an if-chain
' written inline here. That is what lets the self-test assert the precedence and the isDefault
' border overlay directly, instead of only being able to look at pixels.
'
' PRESENCE IS TESTED ON THE RECTS, not on the strings. LayoutButton leaves an absent part's
' rect EMPTY, so IsRectEmpty is the single source of truth about what exists -- and a
' degenerate caption span (the client too narrow to leave any room) drops out of the same test
' rather than needing its own.
' ========================================================================================
private sub PsButton_RenderDefault( _
            byval pBtn as PSBUTTON ptr, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSBUTTON_PAINTINFO ptr _
            )

    if (pBtn = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long bw = p->rcButton.right - p->rcButton.left
    dim as long bh = p->rcButton.bottom - p->rcButton.top
    if (bw <= 0) orelse (bh <= 0) then exit sub

    ' ---- which four colors -------------------------------------------------------------
    dim as long nMood = PsButton_ResolveMood( p->isEnabled, p->isPressed, p->isHot )
    dim as COLORREF clrBack, clrFore, clrBorder, clrIcon
    PsButton_ResolveColors( @pBtn->colors, nMood, p->isDefault, _
                           clrBack, clrFore, clrBorder, clrIcon )

    ' ---- the chrome --------------------------------------------------------------------
    ' nCurvature is an ellipse DIAMETER (PsBufferPaint keeps GDI's vocabulary and halves it
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
    ' the stock GUI font. PsBufferPaint.SetFont IGNORES a null handle, so leaving it unset would
    ' silently paint in the DC's font while the width was measured in another, and long
    ' captions would clip for no visible reason.
    dim as HFONT hTextUse = pBtn->hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    ' The glyph font falls back to the caption font rather than to the stock font: a host that
    ' set one font and not the other almost certainly meant that one.
    dim as HFONT hGlyphUse = pBtn->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = hTextUse

    ' ---- the left icon: image first, else glyph ----------------------------------------
    ' The cell is a LAYOUT rect, not a measurement of the icon: the control never measures the
    ' icon font, so a glyph too large for the cell clips rather than resizing the button. An
    ' image is fitted into the same cell, aspect preserved.
    if IsRectEmpty( @p->rcIconLeft ) = 0 then
        dim as RECT rcGL = p->rcIconLeft
        if p->pImageLeft then
            b->PaintImage( p->pImageLeft, @rcGL, PS_IMGFIT_ASPECT )
        else
            b->SetFont( hGlyphUse )
            b->SetColors( clrIcon, clrBack )
            b->PaintText( p->wszGlyphLeft, @rcGL, DT_CENTER )
        end if
    end if

    ' ---- the right icon: image first, else glyph ---------------------------------------
    if IsRectEmpty( @p->rcIconRight ) = 0 then
        dim as RECT rcGR = p->rcIconRight
        if p->pImageRight then
            b->PaintImage( p->pImageRight, @rcGR, PS_IMGFIT_ASPECT )
        else
            b->SetFont( hGlyphUse )
            b->SetColors( clrIcon, clrBack )
            b->PaintText( p->wszGlyphRight, @rcGR, DT_CENTER )
        end if
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
private sub PsButton_FillPaintInfo( _
            byval pBtn as PSBUTTON ptr, _
            byval hwnd as HWND, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSBUTTON_PAINTINFO ptr _
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
    ' Resolve the images to their decoded handles (NULL when absent or failed to load), so the
    ' painter and any callback draw the image without reaching back into the control.
    p->pImageLeft  = iif( (pBtn->pImageLeft  <> 0) andalso (pBtn->pImageLeft->IsValid()  <> 0), pBtn->pImageLeft->Image(),  cast( CGpImage ptr, 0 ) )
    p->pImageRight = iif( (pBtn->pImageRight <> 0) andalso (pBtn->pImageRight->IsValid() <> 0), pBtn->pImageRight->Image(), cast( CGpImage ptr, 0 ) )
    p->nTextAlign    = pBtn->nTextAlign
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function PsButton_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hwnd)
    if pBtn = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pBtn->bLayoutDirty then pBtn->LayoutButton()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' The client is filled with the resolved mood's own background, not a fixed one, so a paint
    ' callback that draws only a foreground still sits on the right colour. Resolved through
    ' the same two functions the painter uses, so the fill can never disagree with the chrome.
    dim as long nMood = PsButton_ResolveMood( pBtn->isEnabled, pBtn->isPressed, pBtn->isHot )
    dim as COLORREF clrFill, clrFore, clrBorder, clrIcon
    PsButton_ResolveColors( @pBtn->colors, nMood, pBtn->isDefault, _
                           clrFill, clrFore, clrBorder, clrIcon )
    b.SetBackColor( clrFill )
    b.PaintClientRect()

    dim p as PSBUTTON_PAINTINFO
    PsButton_FillPaintInfo( pBtn, hwnd, @b, @p )

    if pBtn->PaintCallback then
        pBtn->PaintCallback( @p )
    else
        PsButton_RenderDefault( pBtn, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub PsButton_ClearHot( byval pBtn as PSBUTTON ptr, byval hwnd as HWND )
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
' opted in with PsButton_SetAutoSize -- the family default is that a control never moves itself
' and the host measures with GetIdealSize.
'
' Called from every mutator that can change the ideal size. It forces the pending layout first
' (that is where the measurement happens), then resizes -- which marks the layout dirty again
' through WM_SIZE, so the rects are re-derived against the new client area.
' ========================================================================================
sub PsButton_ApplyAutoSize( byval pBtn as PSBUTTON ptr )
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
' PsButton_Click all come through here, so keyboard, mouse and host can never drift apart.
'
' It reads isEnabled again rather than trusting the caller: a message callback may have
' disabled the control mid-gesture.
' ========================================================================================
private sub PsButton_FireClick( byval pBtn as PSBUTTON ptr, byval hwnd as HWND )
    if pBtn = 0 then exit sub
    if pBtn->isEnabled = false then exit sub
    if pBtn->ClickCallback = 0 then exit sub
    pBtn->ClickCallback( hwnd, pBtn->id )
end sub


' ========================================================================================
' PsButton Window procedure
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
' NOTE this control fires on the UP, unlike PsComboBox (which opens its list on the DOWN) and
' PsNumericUpDown (which steps on the DOWN and uses capture to bound an auto-repeat). Same
' obligation, a third distinct justification.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET, which is PsToggle's and PsIconPanel's call rather than
' PsSelectBar's. With it, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD OF
' a second WM_LBUTTONDOWN -- and on a momentary command button a rapid second click is a
' legitimate second click, which would be silently swallowed.
'
' FOCUS: this is the third focusable control in the family, after PsToggle and PsComboBox. See
' the WM_GETDLGCODE handler for the one subtlety that needed care.
' ========================================================================================
function PsButton_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSBUTTON ptr pBtn = GetButtonPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsButton_OnPaint( hwnd )

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
                    PsButton_ClearHot( pBtn, hwnd )
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
                SetTimer( hwnd, IDT_CBUTTON_HOTTRACK, PSBUTTON_HOTTRACK_MS, NULL )
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

        if PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pBtn then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for when
            ' this message never arrives). Suppressed during a press for the same reason as in
            ' the timer: the gesture is still live and WM_MOUSEMOVE owns isHot.
            if pBtn->isPressed = false then PsButton_ClearHot( pBtn, hwnd )
            if PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pBtn = 0 then return 0
        ' A disabled window does not receive mouse input at all (PsButton_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some
        ' other route.
        if pBtn->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true
        ' even if the host goes on to suppress the press. This runs WM_SETFOCUS synchronously,
        ' so isFocused is already correct by the time we paint below.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0

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
        PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release with the cursor still inside is a click. Press, slide
        ' off, release = nothing happened, which is the whole reason for capture.
        dim as boolean bClick = (bWasPressed andalso bInside)
        if bClick then PsButton_FireClick( pBtn, hwnd )
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
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pBtn then
            if PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' A pending swallow must not survive a focus change: the WM_CHAR it was armed for is
        ' not coming, and a stale flag would eat the next character instead.
        if pBtn then pBtn->bAteKeyDown = false
        ' Focus is a FACT reported by the system, not an action a host can veto, so the state
        ' is updated first and the callback's result is IGNORED. A host that wants to refuse
        ' focus must not make the control a tabstop in the first place.
        if pBtn then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pBtn->isFocused then
                pBtn->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam )
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
            ' Claim the CHARACTER produced by a key we already consumed. Claiming a key with
            ' DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which manufactures a
            ' matching WM_CHAR; if nothing claims that, it reaches DefWindowProc and beeps.
            if pMsg->message = WM_CHAR then
                if pBtn then
                    if pBtn->bAteKeyDown then return DLGC_WANTALLKEYS
                end if
            end if
            if pMsg->message = WM_KEYDOWN then
                select case pMsg->wParam
                case VK_SPACE, VK_RETURN
                    return DLGC_WANTALLKEYS
                end select
            end if
        end if
        return 0

    case WM_CHAR
        ' Swallow the character generated by a key WM_KEYDOWN already acted on, so it never
        ' reaches DefWindowProc -- which beeps at a character no control wants. The keystroke
        ' works either way, so the beep is the ONLY symptom and no assertion can hear it.
        '
        ' Anything we did not consume falls through untouched. WM_SYSCHAR is deliberately not
        ' touched either: swallowing it would break Alt+F4 and the host's menu mnemonics.
        if pBtn = 0 then exit select
        if pBtn->bAteKeyDown then
            pBtn->bAteKeyDown = false
            return 0
        end if

    case WM_KEYDOWN
        if pBtn = 0 then exit select
        ' Arm the character swallow (see WM_CHAR). Every path below that handles one of these
        ' keys consumes it, so the key alone decides -- and only keys that actually produce a
        ' WM_CHAR are armed.
        pBtn->bAteKeyDown = (wParam = VK_SPACE) orelse (wParam = VK_RETURN)
        if pBtn->isEnabled = false then return 0
        if PsButton_CallMessage( pBtn, hwnd, uMsg, wParam, lParam ) then return 0
        select case wParam
        case VK_SPACE, VK_RETURN
            ' Same path a completed click takes, so keyboard and mouse cannot drift apart: both
            ' are user action, both notify.
            '
            ' Note we claim Enter only while WE are focused (that is all WM_GETDLGCODE above
            ' answers for). isDefault does NOT make this control grab Enter from elsewhere in
            ' the dialog -- it is appearance only, and routing the dialog's default key is the
            ' host's job.
            PsButton_FireClick( pBtn, hwnd )
            return 0
        end select

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsButton_SetEnabled still gets the greyed rendering
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
        ' The caption has ONE store, reached through two doors: PsButton_SetText and this
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
        PsButton_ApplyAutoSize( pBtn )
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
                pBtn->wszTipBuf = PsButton_ResolveTooltipText( pBtn )
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
        ' alone; the tooltip window and the two PsImages are ours and go with us.
        if pBtn then
            PsTipHost_Destroy( @pBtn->tip )
            if pBtn->pImageLeft  then delete pBtn->pImageLeft  : pBtn->pImageLeft  = 0
            if pBtn->pImageRight then delete pBtn->pImageRight : pBtn->pImageRight = 0
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
' Supply the tooltip text to the PsTooltip backend.
'
' This thunk exists so PsTipHost never learns the text policy: it hands PsTooltip a callback
' and PsTooltip_ResolveTooltipText above stays the ONE place that decides what a button's tip
' says. The system backend reaches the same function by a different road -- comctl32 asks the
' control through TTN_GETDISPINFOW -- which is why that arm stays in the WndProc.
'
' idx is ignored: a button is a whole-control tip and has no items.
' ========================================================================================
private function PsButton_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                           byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsButton_ResolveTooltipText( GetButtonPointer(hAttached) )
    return wszOut
end function


' ========================================================================================
' PsButton_Create
' ========================================================================================
function PsButton_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle parameter to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that supplies
    ' only dwStyle -- which is what every sibling does -- silently declares itself a CONTAINER.
    ' The dialog manager then descends into it hunting for tabstops, finds no children, and
    ' SKIPS IT: the control becomes unreachable by Tab. (It does not break the rest of the form
    ' -- that was measured. PsButton's demo merely looked completely dead because every control
    ' on it was a PsButton.) Measured, not theorised; see REFACTOR_PLAN.md.
    '
    ' A leaf control must be a tab ITEM, not a tab container. WS_EX_WINDOWEDGE goes with it --
    ' it is a raised-border hint that means nothing to a window drawing its own chrome.
    '
    ' DO NOT COPY THIS LINE INTO A WRAP-A-REAL-CHILD CONTROL. PsListBox, PsTextBox and
    ' PsNumericUpDown all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner child
    ' and their container is a real container; passing 0 there would make them unreachable. The
    ' test is "does this window have children the dialog manager should find?", not "is this a
    ' control?".
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsButton_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header. It would
    ' turn every second rapid click into a WM_LBUTTONDBLCLK the control would drop.

    dim as PSBUTTON ptr pBtn = new PSBUTTON
    pBtn->hWin       = hCtrl
    pBtn->idc_Button = CtrlID
    pBtn->id         = CtrlID     ' sensible default; PsButton_SetID overrides it

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). The two THICKNESS values are not scaled
    ' at all, here or anywhere -- a hairline stays a hairline.
    pBtn->nPadLeft    = pWindow->ScaleX( PSBUTTON_DEFAULT_PADLEFT )
    pBtn->nPadRight   = pWindow->ScaleX( PSBUTTON_DEFAULT_PADRIGHT )
    pBtn->nPadTop     = pWindow->ScaleY( PSBUTTON_DEFAULT_PADTOP )
    pBtn->nPadBottom  = pWindow->ScaleY( PSBUTTON_DEFAULT_PADBOTTOM )
    pBtn->nIconGap    = pWindow->ScaleX( PSBUTTON_DEFAULT_ICONGAP )
    pBtn->nIconWidth  = pWindow->ScaleX( PSBUTTON_DEFAULT_ICONWIDTH )
    pBtn->nIconHeight = pWindow->ScaleY( PSBUTTON_DEFAULT_ICONHEIGHT )
    pBtn->nCurvature  = pWindow->ScaleY( PSBUTTON_DEFAULT_CURVATURE )
    pBtn->nFocusGap   = pWindow->ScaleY( PSBUTTON_DEFAULT_FOCUSGAP )

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pBtn)

    ' AFTER UserData, not before. PsButton_TipTextCallback resolves the button through
    ' GetButtonPointer(hAttached), which reads exactly that slot -- so building the tip first
    ' would leave a window in which a resolve returns "" for a button that has text.
    PsTipHost_Ensure( @pBtn->tip, hCtrl, @PsButton_TipTextCallback )

    function = hCtrl

end function


' ========================================================================================
' Content.  All SILENT -- they change what is drawn, never what was clicked.
' ========================================================================================
function PsButton_GetText( byval hButton as HWND ) as DWSTRING
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszText
    return wszOut
end function

sub PsButton_SetText( byval hButton as HWND, byval Text as DWSTRING )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszText = Text
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetGlyphLeft( byval hButton as HWND ) as DWSTRING
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszGlyphLeft
    return wszOut
end function

sub PsButton_SetGlyphLeft( byval hButton as HWND, byval Glyph as DWSTRING )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszGlyphLeft = Glyph
    ' Glyph OR image: setting an actual glyph frees any image on this side.
    if (PsLen( Glyph ) > 0) andalso (pBtn->pImageLeft <> 0) then
        delete pBtn->pImageLeft : pBtn->pImageLeft = 0
    end if
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetGlyphRight( byval hButton as HWND ) as DWSTRING
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszGlyphRight
    return wszOut
end function

sub PsButton_SetGlyphRight( byval hButton as HWND, byval Glyph as DWSTRING )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszGlyphRight = Glyph
    if (PsLen( Glyph ) > 0) andalso (pBtn->pImageRight <> 0) then
        delete pBtn->pImageRight : pBtn->pImageRight = 0
    end if
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

' ========================================================================================
' Load an image file into an icon cell, replacing the glyph on that side. "" removes it.
' Returns non-zero on a successful load; on failure the (empty) image is dropped so
' HasIconLeft/Right stays honest. The image honours the existing icon cell size.
' ========================================================================================
function PsButton_SetImageLeft( byval hButton as HWND, byval Path as DWSTRING ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0

    if PsLen( Path ) = 0 then
        if pBtn->pImageLeft then delete pBtn->pImageLeft : pBtn->pImageLeft = 0
        pBtn->Refresh()
        PsButton_ApplyAutoSize( pBtn )
        return 0
    end if

    if pBtn->pImageLeft = 0 then pBtn->pImageLeft = new PsImage
    dim as long bOK = pBtn->pImageLeft->Load( Path )
    if bOK = 0 then
        delete pBtn->pImageLeft : pBtn->pImageLeft = 0
    else
        pBtn->wszGlyphLeft = ""                 ' glyph OR image
    end if
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
    function = bOK
end function

function PsButton_SetImageRight( byval hButton as HWND, byval Path as DWSTRING ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0

    if PsLen( Path ) = 0 then
        if pBtn->pImageRight then delete pBtn->pImageRight : pBtn->pImageRight = 0
        pBtn->Refresh()
        PsButton_ApplyAutoSize( pBtn )
        return 0
    end if

    if pBtn->pImageRight = 0 then pBtn->pImageRight = new PsImage
    dim as long bOK = pBtn->pImageRight->Load( Path )
    if bOK = 0 then
        delete pBtn->pImageRight : pBtn->pImageRight = 0
    else
        pBtn->wszGlyphRight = ""
    end if
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
    function = bOK
end function

function PsButton_GetID( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->id
end function

sub PsButton_SetID( byval hButton as HWND, byval id as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->id = id
end sub

function PsButton_GetItemData( byval hButton as HWND ) as integer
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->itemData
end function

sub PsButton_SetItemData( byval hButton as HWND, byval itemData as integer )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->itemData = itemData
end sub


' ========================================================================================
' State.
' ========================================================================================
function PsButton_GetEnabled( byval hButton as HWND ) as boolean
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isEnabled
end function

' Goes through EnableWindow so the disable is enforced by the SYSTEM rather than being a
' cosmetic flag: a disabled window gets no mouse input and the dialog manager's Tab skips it.
' WM_ENABLE syncs our own flag and clears hover and any live press.
sub PsButton_SetEnabled( byval hButton as HWND, byval isEnabled as boolean )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->isEnabled = isEnabled then exit sub
    EnableWindow( hButton, iif( isEnabled, TRUE, FALSE ) )
end sub

function PsButton_GetFocused( byval hButton as HWND ) as boolean
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isFocused
end function

function PsButton_GetDefault( byval hButton as HWND ) as boolean
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->isDefault
end function

' Appearance only -- it swaps one colour and nothing else, so it never re-measures and the
' repaint does not need a background erase. Nothing stops a host marking two buttons default;
' the control does not police it.
sub PsButton_SetDefault( byval hButton as HWND, byval isDefault as boolean )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->isDefault = isDefault then exit sub
    pBtn->isDefault = isDefault
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

sub PsButton_Refresh( byval hButton as HWND )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->Refresh()
end sub


' ========================================================================================
' Action.  This one DOES notify -- see the header. It is refused on a disabled button, exactly
' as a real click would be, and it does not paint a press flash: it reports an action, it does
' not animate one.
' ========================================================================================
sub PsButton_Click( byval hButton as HWND )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    PsButton_FireClick( pBtn, hButton )
end sub


' ========================================================================================
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales.
' ========================================================================================
function PsButton_GetTextAlign( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return BTN_ALIGN_CENTER
    return pBtn->nTextAlign
end function

' Alignment moves the caption INSIDE the span, never the span itself, so it cannot change the
' ideal size and does not need an auto-size pass. An out-of-range value is ignored rather than
' clamped: silently turning a typo into LEFT would hide it.
sub PsButton_SetTextAlign( byval hButton as HWND, byval nTextAlign as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    select case nTextAlign
    case BTN_ALIGN_LEFT, BTN_ALIGN_CENTER, BTN_ALIGN_RIGHT
        if pBtn->nTextAlign = nTextAlign then exit sub
        pBtn->nTextAlign = nTextAlign
        if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
    end select
end sub

sub PsButton_GetPadding( byval hButton as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
    nLeft = 0 : nTop = 0 : nRight = 0 : nBottom = 0
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nLeft   = pBtn->nPadLeft
    nTop    = pBtn->nPadTop
    nRight  = pBtn->nPadRight
    nBottom = pBtn->nPadBottom
end sub

sub PsButton_SetPadding( byval hButton as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
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
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetIconGap( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nIconGap
end function

sub PsButton_SetIconGap( byval hButton as HWND, byval nIconGap as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nIconGap < 0 then nIconGap = 0
    pBtn->nIconGap = nIconGap
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

sub PsButton_GetIconSize( byval hButton as HWND, byref nIconWidth as long, byref nIconHeight as long )
    nIconWidth = 0 : nIconHeight = 0
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nIconWidth  = pBtn->nIconWidth
    nIconHeight = pBtn->nIconHeight
end sub

sub PsButton_SetIconSize( byval hButton as HWND, byval nIconWidth as long, byval nIconHeight as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nIconWidth  < 0 then nIconWidth  = 0
    if nIconHeight < 0 then nIconHeight = 0
    pBtn->nIconWidth  = nIconWidth
    pBtn->nIconHeight = nIconHeight
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetCornerCurvature( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nCurvature
end function

sub PsButton_SetCornerCurvature( byval hButton as HWND, byval nCurvature as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pBtn->nCurvature = nCurvature
    ' Chrome only -- no size change, so no auto-size pass.
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

function PsButton_GetBorderThickness( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->nBorderThick
end function

' The border is drawn INSIDE the chrome rect, so its thickness does not change the ideal size.
' It is not DPI-scaled anywhere: a hairline stays a hairline.
sub PsButton_SetBorderThickness( byval hButton as HWND, byval nThickness as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pBtn->nBorderThick = nThickness
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

sub PsButton_GetFocusRing( byval hButton as HWND, byref nGap as long, byref nThickness as long )
    nGap = 0 : nThickness = 0
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    nGap       = pBtn->nFocusGap
    nThickness = pBtn->nFocusThick
end sub

' The ring band is reserved UNCONDITIONALLY, focused or not, so the button never jumps when
' focus arrives -- which is exactly why changing either value changes the IDEAL SIZE.
sub PsButton_SetFocusRing( byval hButton as HWND, byval nGap as long, byval nThickness as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if nGap       < 0 then nGap       = 0
    if nThickness < 0 then nThickness = 0
    pBtn->nFocusGap   = nGap
    pBtn->nFocusThick = nThickness
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetAutoSize( byval hButton as HWND ) as boolean
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return pBtn->bAutoSize
end function

sub PsButton_SetAutoSize( byval hButton as HWND, byval bAutoSize as boolean )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->bAutoSize = bAutoSize
    ' Turning it ON applies immediately rather than waiting for the next content change.
    PsButton_ApplyAutoSize( pBtn )
end sub

sub PsButton_GetIdealSize( byval hButton as HWND, byref nWidth as long, byref nHeight as long )
    nWidth  = 0
    nHeight = 0
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    nWidth  = pBtn->nIdealW
    nHeight = pBtn->nIdealH
end sub

function PsButton_GetButtonRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false        ' still no client area
    rc = pBtn->rcButton
    return true
end function

function PsButton_GetTextRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcText
    return true
end function

function PsButton_GetIconLeftRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcIconLeft
    return true
end function

function PsButton_GetIconRightRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcIconRight
    return true
end function

function PsButton_GetVisualRect( byval hButton as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return false
    rc = pBtn->rcVisual
    return true
end function


' ========================================================================================
' Appearance.  Both fonts are BORROWED, never owned.
' ========================================================================================
sub PsButton_GetColors( byval hButton as HWND, byval pColors as PSBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    *pColors = pBtn->colors
end sub

sub PsButton_SetColors( byval hButton as HWND, byval pColors as PSBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->colors = *pColors
    ' Colours only -- nothing measured changes, so no layout and no auto-size.
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub

function PsButton_GetFont( byval hButton as HWND ) as HFONT
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->hTextFont
end function

' The CAPTION font, and therefore the MEASURING font: changing it re-measures and can change
' the ideal size.
sub PsButton_SetFont( byval hButton as HWND, byval hTextFont as HFONT )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->hTextFont = hTextFont
    pBtn->Refresh()
    PsButton_ApplyAutoSize( pBtn )
end sub

function PsButton_GetGlyphFont( byval hButton as HWND ) as HFONT
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return pBtn->hGlyphFont
end function

' The ICON font. A PAINT-TIME INPUT ONLY: the icon cell is declared by PsButton_SetIconSize and
' is never measured, so changing this repaints but never re-lays-out and never resizes.
sub PsButton_SetGlyphFont( byval hButton as HWND, byval hGlyphFont as HFONT )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->hGlyphFont = hGlyphFont
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, FALSE )
end sub


' ========================================================================================
' Tooltips.
' ========================================================================================
function PsButton_GetTooltipText( byval hButton as HWND ) as DWSTRING
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return ""
    dim as DWSTRING wszOut = pBtn->wszTooltip
    return wszOut
end function

sub PsButton_SetTooltipText( byval hButton as HWND, byval Text as DWSTRING )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->wszTooltip = Text
end sub

' The comctl32 handle, and 0 while this instance is on PsTooltip -- which is the honest
' answer, not a gap: a host reaching through here is about to send a TTM_* message, and one
' sent to a PsTooltip window would be silently ignored.
function PsButton_GetTooltipHandle( byval hButton as HWND ) as HWND
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return PsTipHost_SysHandle( @pBtn->tip )
end function

function PsButton_GetPsTooltipHandle( byval hButton as HWND ) as HWND
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    return PsTipHost_PsHandle( @pBtn->tip )
end function

function PsButton_SetTooltipMode( byval hButton as HWND, byval nMode as long ) as boolean
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return false
    return PsTipHost_SetMode( @pBtn->tip, nMode )
end function

function PsButton_GetTooltipMode( byval hButton as HWND ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pBtn->tip )
end function

' The three delays. All of them go through PsTipHost, which STORES them as well as pushing
' them -- so a host that sets a hover time and later switches backend keeps the hover time it
' asked for. Pushing straight at the live tooltip would lose it at the switch, and the tip
' would still appear, just on a different beat, with nothing reporting the change.
sub PsButton_SetHoverTime( byval hButton as HWND, byval milliseconds as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    PsTipHost_SetDelay( @pBtn->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsButton_SetAutoPopTime( byval hButton as HWND, byval milliseconds as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    PsTipHost_SetDelay( @pBtn->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsButton_SetReshowTime( byval hButton as HWND, byval milliseconds as long )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    PsTipHost_SetDelay( @pBtn->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsButton_SetPaintCallback( byval hButton as HWND, byval usersub as BTN_PaintCallbackSub )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->PaintCallback = usersub
    if pBtn->hWin then InvalidateRect( pBtn->hWin, NULL, TRUE )
end sub

sub PsButton_SetMessageCallback( byval hButton as HWND, byval userfunc as BTN_MessageCallbackFunc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->MessageCallback = userfunc
end sub

sub PsButton_SetTooltipCallback( byval hButton as HWND, byval userfunc as BTN_TooltipCallbackFunc )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->TooltipCallback = userfunc
end sub

sub PsButton_SetClickCallback( byval hButton as HWND, byval usersub as BTN_ClickCallbackSub )
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then exit sub
    pBtn->ClickCallback = usersub
end sub


' ========================================================================================
' Which rect does this part name? Shared by the tone probe and nothing else, but factored out
' so the probe body stays about pixels rather than about rect selection.
' ========================================================================================
private function PsButton_PartRect( byval pBtn as PSBUTTON ptr, byval nPart as long ) as RECT
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
' one of its rects. See the header in PsButton.bi for what this is for and why it is public.
'
' THREE THINGS HERE ARE LOAD-BEARING, and all three come from the write-up in Learnings.md:
'
'   1. The PAINTINFO is filled by PsButton_FillPaintInfo -- the SAME routine WM_PAINT uses. The
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
function PsButton_CountRenderedTones( byval hButton as HWND, byval nPart as long ) as long
    dim as PSBUTTON ptr pBtn = GetButtonPointer(hButton)
    if pBtn = 0 then return 0
    if pBtn->bLayoutDirty then pBtn->LayoutButton()
    if pBtn->bLayoutDirty then return 0          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hButton, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return 0

    dim as RECT rcPart = PsButton_PartRect( pBtn, nPart )
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

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels we read are
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
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hButton, hMem, rcClient, hMem )

        dim as long nMood = PsButton_ResolveMood( pBtn->isEnabled, pBtn->isPressed, pBtn->isHot )
        dim as COLORREF clrFill, clrFore, clrBorder, clrIcon
        PsButton_ResolveColors( @pBtn->colors, nMood, pBtn->isDefault, _
                               clrFill, clrFore, clrBorder, clrIcon )
        b.SetBackColor( clrFill )
        b.PaintClientRect()

        dim p as PSBUTTON_PAINTINFO
        PsButton_FillPaintInfo( pBtn, hButton, @b, @p )

        if pBtn->PaintCallback then
            pBtn->PaintCallback( @p )
        else
            PsButton_RenderDefault( pBtn, @b, @p )
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
' Self-test.  Gated on PSBUTTON_SELFTEST=1.
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


sub PsButton_RunSelfTest( byval hWndParent as HWND )
    if environ("PSBUTTON_SELFTEST") <> "1" then exit sub

    gBTN_TestCount = 0
    gBTN_FailCount = 0
    print "=== PsButton self-test ==="

    dim as HWND h = PsButton_Create( hWndParent, 9801 )
    if h = 0 then
        print "FAIL  create   [PsButton_Create returned 0]"
        exit sub
    end if

    dim as PSBUTTON ptr pB = GetButtonPointer(h)
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
    PsButton_SetPadding( h, 10, 5, 10, 5 )
    PsButton_SetIconGap( h, 8 )
    PsButton_SetIconSize( h, 16, 16 )
    PsButton_SetFocusRing( h, 2, 1 )              ' ringPad = 3
    PsButton_SetBorderThickness( h, 1 )
    PsButton_SetCornerCurvature( h, 8 )

    ' Plain letters, not Segoe Fluent Icons codepoints: the control does not care what the
    ' glyph string is, and a letter renders under the stock GUI font, so the tone probe at the
    ' end cannot fail for want of an icon font being installed.
    const GLYPH_L = "A"
    const GLYPH_R = "B"
    const CAPTION = "Test Caption"

    dim as long ringPad = pB->RingPad()

    ' ---- 1. ideal size is valid BEFORE the control has ever been sized ------------------
    PsButton_SetText( h, CAPTION )
    PsButton_GetIdealSize( h, w, hh )
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
    PsButton_SetGlyphLeft( h, GLYPH_L )
    PsButton_GetIdealSize( h, w, hh )
    sD = "before=" & str(textOnlyW) & " after=" & str(w) & " delta=" & str(w - textOnlyW)
    BTNTest_Check( "left glyph adds iconW + gap", (w - textOnlyW) = (16 + 8), sD )
    dim as long leftOnlyW = w

    ' ---- 4. a right glyph costs the same again ------------------------------------------
    PsButton_SetGlyphRight( h, GLYPH_R )
    PsButton_GetIdealSize( h, w, hh )
    sD = "before=" & str(leftOnlyW) & " after=" & str(w) & " delta=" & str(w - leftOnlyW)
    BTNTest_Check( "right glyph adds iconW + gap", (w - leftOnlyW) = (16 + 8), sD )
    dim as long bothW = w

    ' ---- 5. THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION -------------------------------
    ' Direction 1: drop the caption and both gaps must go with it, not just the text width.
    PsButton_SetText( h, "" )
    PsButton_GetIdealSize( h, w, hh )
    dim as long expectIconOnly = (2 * ringPad) + 10 + 16 + 16 + 10
    sD = "ideal=" & str(w) & " expect=" & str(expectIconOnly) & " (no gaps charged)"
    BTNTest_Check( "no caption = no gap on either side", (w = expectIconOnly), sD )

    ' ...and the content height falls back to the ICON, not to the font.
    dim as long expectIconH = (2 * ringPad) + 5 + 16 + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectIconH) & " textH=" & str(pB->nTextHeight)
    BTNTest_Check( "no caption = height comes from the icon", (hh = expectIconH), sD )

    ' Direction 2: put it back and both gaps return with it.
    PsButton_SetText( h, CAPTION )
    PsButton_GetIdealSize( h, w, hh )
    sD = "restored=" & str(w) & " expect=" & str(bothW)
    BTNTest_Check( "caption restored = gaps restored", (w = bothW), sD )

    ' ---- 6. absent parts give EMPTY rects, not zero-width ones at an edge ---------------
    ' Size the control to ideal first; every rect query below needs a client area.
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    PsButton_SetGlyphRight( h, "" )
    PsButton_GetIconRightRect( h, rcIR )
    sD = "rcIconRight=" & BTNTest_RectStr( rcIR )
    BTNTest_Check( "no right glyph = empty rect", IsRectEmpty( @rcIR ) <> 0, sD )
    PsButton_SetGlyphRight( h, GLYPH_R )

    PsButton_SetText( h, "" )
    PsButton_GetTextRect( h, rcTxt )
    sD = "rcText=" & BTNTest_RectStr( rcTxt )
    BTNTest_Check( "no caption = empty text rect", IsRectEmpty( @rcTxt ) <> 0, sD )
    PsButton_SetText( h, CAPTION )

    ' ---- 7. the icons are PINNED to the padding ----------------------------------------
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsButton_GetButtonRect( h, rcBtn )
    PsButton_GetIconLeftRect( h, rcIL )
    PsButton_GetIconRightRect( h, rcIR )
    sD = "btn=" & BTNTest_RectStr( rcBtn ) & " iconL=" & BTNTest_RectStr( rcIL )
    BTNTest_Check( "left icon pinned at padLeft", (rcIL.left - rcBtn.left) = 10, sD )
    sD = "btn=" & BTNTest_RectStr( rcBtn ) & " iconR=" & BTNTest_RectStr( rcIR )
    BTNTest_Check( "right icon pinned at padRight", (rcBtn.right - rcIR.right) = 10, sD )
    sD = "w=" & str(rcIL.right - rcIL.left) & "x" & str(rcIL.bottom - rcIL.top)
    BTNTest_Check( "icon cell is the declared size", _
                   ((rcIL.right - rcIL.left) = 16) andalso ((rcIL.bottom - rcIL.top) = 16), sD )

    ' ...and a much longer caption does not move them (the control is re-sized to the new
    ' ideal, which is what a host would do).
    PsButton_SetText( h, "A Very Much Longer Caption Indeed" )
    PsButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsButton_GetButtonRect( h, rcBtn )
    PsButton_GetIconLeftRect( h, rcIL )
    PsButton_GetIconRightRect( h, rcIR )
    sD = "padL=" & str(rcIL.left - rcBtn.left) & " padR=" & str(rcBtn.right - rcIR.right)
    BTNTest_Check( "caption length never moves the icons", _
                   ((rcIL.left - rcBtn.left) = 10) andalso ((rcBtn.right - rcIR.right) = 10), sD )
    PsButton_SetText( h, CAPTION )
    PsButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 8. the caption SPAN sits exactly between the two icon blocks -------------------
    PsButton_GetButtonRect( h, rcBtn )
    PsButton_GetTextRect( h, rcTxt )
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
    PsButton_GetVisualRect( h, rcVis )
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
    PsButton_GetButtonRect( h, rcBtn )
    PsButton_GetTextRect( h, rcTxt )
    pB->isFocused = bSavedFocus
    sD = "unfocused=" & BTNTest_RectStr( rcBtnUnfocused ) & " focused=" & BTNTest_RectStr( rcBtn )
    BTNTest_Check( "focus does not move anything", _
                   (EqualRect( @rcBtn, @rcBtnUnfocused ) <> 0) andalso _
                   (EqualRect( @rcTxt, @rcTxtUnfocused ) <> 0), sD )

    ' ---- 11. overflow is honest: the span collapses, the icons do not -------------------
    dim as long narrow = 2 * ringPad + 10 + 16 + 16 + 10        ' exactly no room for a caption
    SetWindowPos( h, 0, 0, 0, narrow, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsButton_GetButtonRect( h, rcBtn )
    PsButton_GetTextRect( h, rcTxt )
    PsButton_GetIconLeftRect( h, rcIL )
    PsButton_GetIconRightRect( h, rcIR )
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
    PsButton_GetIdealSize( h, w2, h2 )
    sD = "ideal now=" & str(w2) & "x" & str(h2) & " expect=" & str(bothW) & "x" & str(hh)
    BTNTest_Check( "ideal size unaffected by a starved client", _
                   (w2 = bothW) andalso (h2 = hh), sD )
    SetWindowPos( h, 0, 0, 0, bothW, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 13. mood precedence, as a truth table ------------------------------------------
    ' disabled > pressed > hot > idle. All eight combinations, because the interesting ones are
    ' the contradictory states (disabled AND pressed) that only a race can produce.
    BTNTest_Check( "mood: enabled,idle", _
                   PsButton_ResolveMood( true, false, false ) = BTN_MOOD_IDLE, "" )
    BTNTest_Check( "mood: enabled,hot", _
                   PsButton_ResolveMood( true, false, true ) = BTN_MOOD_HOT, "" )
    BTNTest_Check( "mood: enabled,pressed", _
                   PsButton_ResolveMood( true, true, false ) = BTN_MOOD_PRESSED, "" )
    BTNTest_Check( "mood: pressed beats hot", _
                   PsButton_ResolveMood( true, true, true ) = BTN_MOOD_PRESSED, "" )
    BTNTest_Check( "mood: disabled,idle", _
                   PsButton_ResolveMood( false, false, false ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats hot", _
                   PsButton_ResolveMood( false, false, true ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats pressed", _
                   PsButton_ResolveMood( false, true, false ) = BTN_MOOD_DISABLED, "" )
    BTNTest_Check( "mood: disabled beats both", _
                   PsButton_ResolveMood( false, true, true ) = BTN_MOOD_DISABLED, "" )

    ' ---- 14. isDefault overlays the border in EVERY mood --------------------------------
    dim as PSBUTTON_COLORS c
    PsButton_GetColors( h, @c )
    ' Make every field distinguishable, so an assertion cannot pass by two colours happening
    ' to be equal (which they ARE by default -- the borderless look).
    c.BorderColor         = BGR(1,1,1)
    c.BorderColorHot      = BGR(2,2,2)
    c.BorderColorPressed  = BGR(3,3,3)
    c.BorderColorDisabled = BGR(4,4,4)
    c.DefaultBorderColor  = BGR(9,9,9)
    PsButton_SetColors( h, @c )

    dim as COLORREF cBack, cFore, cBorder, cIcon
    dim as long nMoods(0 to 3) = { BTN_MOOD_IDLE, BTN_MOOD_HOT, BTN_MOOD_PRESSED, BTN_MOOD_DISABLED }
    dim as COLORREF cExpect(0 to 3) = { BGR(1,1,1), BGR(2,2,2), BGR(3,3,3), BGR(4,4,4) }

    for i as long = 0 to 3
        PsButton_ResolveColors( @c, nMoods(i), false, cBack, cFore, cBorder, cIcon )
        sD = "mood=" & str(nMoods(i)) & " border=" & hex(cBorder) & " expect=" & hex(cExpect(i))
        BTNTest_Check( "border follows the mood when not default", (cBorder = cExpect(i)), sD )

        ' The same mood, with the flag on: only the border may move.
        dim as COLORREF dBack, dFore, dBorder, dIcon
        PsButton_ResolveColors( @c, nMoods(i), true, dBack, dFore, dBorder, dIcon )
        sD = "mood=" & str(nMoods(i)) & " border=" & hex(dBorder) & " expect=" & hex(c.DefaultBorderColor)
        BTNTest_Check( "isDefault overlays the border in this mood", _
                       (dBorder = c.DefaultBorderColor), sD )
        sD = "back/fore/icon unchanged in mood " & str(nMoods(i))
        BTNTest_Check( "isDefault touches ONLY the border", _
                       (dBack = cBack) andalso (dFore = cFore) andalso (dIcon = cIcon), sD )
    next

    ' Put the defaults back before the render probe, so it measures the shipping look.
    dim as PSBUTTON_COLORS cFresh
    PsButton_SetColors( h, @cFresh )

    ' ---- 15. the window text really is the caption --------------------------------------
    ' ASK THE CONTROL. Reading the field back would assert nothing about the message handlers,
    ' which are the whole point of this section -- a SendMessage whose result is discarded is
    ' an assumption, not an implementation (the PsNumericUpDown lesson).
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
    wszApi = PsButton_GetText( h )
    sD = "GetWindowText=[" & str(wszWin) & "] PsButton_GetText=[" & str(wszApi) & "]"
    BTNTest_Check( "GetWindowText agrees with PsButton_GetText", (wszWin = wszApi), sD )

    ' SetWindowText must go the other way and re-measure.
    dim as long wBefore = 0, hBefore = 0
    PsButton_GetIdealSize( h, wBefore, hBefore )
    SetWindowTextW( h, "Substantially Longer Window Text" )
    PsButton_GetIdealSize( h, w2, h2 )
    sD = "before=" & str(wBefore) & " after=" & str(w2)
    BTNTest_Check( "SetWindowText re-measures the button", (w2 > wBefore), sD )
    PsButton_SetText( h, CAPTION )

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
    ' See PsButton_CountRenderedTones. A part wiped by a PaintBorderRect fill is literally ONE
    ' tone, so the useful signal is the count LEAVING 1 -- not the count being large.
    PsButton_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    dim as long tButton = PsButton_CountRenderedTones( h, BTN_PART_BUTTON )
    sD = "distinct tones in the chrome=" & str(tButton)
    BTNTest_Check( "built-in painter does not flood the button", (tButton > 1), sD )

    dim as long tText = PsButton_CountRenderedTones( h, BTN_PART_TEXT )
    sD = "distinct tones in the caption span=" & str(tText)
    BTNTest_Check( "the caption actually reached the surface", (tText > 1), sD )

    dim as long tIcon = PsButton_CountRenderedTones( h, BTN_PART_ICONLEFT )
    sD = "distinct tones in the left icon cell=" & str(tIcon)
    BTNTest_Check( "the left glyph actually reached the surface", (tIcon > 1), sD )

    ' ---- 18. an IMAGE reserves the icon cell, replaces the glyph, and reaches the surface --
    ' Build a small multi-tone opaque bitmap, save it, load it as the left image. Multi-tone on
    ' purpose: a solid image scaled to exactly fill the 16x16 cell would read as ONE tone and the
    ' render probe below could not tell it from a wiped cell.
    scope
        dim as wstring * 260 wszTmp = "__psbtn_st.png"
        dim as BITMAPINFO bi
        bi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER )
        bi.bmiHeader.biWidth       = 24
        bi.bmiHeader.biHeight      = -24
        bi.bmiHeader.biPlanes      = 1
        bi.bmiHeader.biBitCount    = 32
        bi.bmiHeader.biCompression = BI_RGB
        dim as ulong ptr pB
        dim as HBITMAP hbm = CreateDIBSection( null, @bi, DIB_RGB_COLORS, cast(any ptr ptr, @pB), null, 0 )
        if hbm andalso pB then
            for yy as long = 0 to 23
                for xx as long = 0 to 23
                    dim as ulong c = &hFF3399EE
                    if ((xx < 12) xor (yy < 12)) then c = &hFFEE9933   ' 2x2 checker -> >=2 tones
                    pB[yy * 24 + xx] = c
                next
            next
            scope
                dim as PsImage tmp
                tmp.LoadFromBitmap( hbm )
                if tmp.IsValid() then AfxGdipSaveImageToPng( tmp.Image()->m_pImage, wszTmp )
            end scope
        end if
        if hbm then DeleteObject( hbm )

        ' Baseline: caption only, no icons at all.
        PsButton_SetGlyphLeft( h, "" )
        PsButton_SetGlyphRight( h, "" )
        PsButton_SetText( h, CAPTION )
        dim as long baseW, baseH
        PsButton_GetIdealSize( h, baseW, baseH )

        dim as long okLoad = PsButton_SetImageLeft( h, wszTmp )
        BTNTest_Check( "SetImageLeft loads the file", okLoad <> 0, "returned " & str(okLoad) )

        PsButton_GetIdealSize( h, w, hh )
        sD = "base=" & str(baseW) & " withImage=" & str(w) & " delta=" & str(w - baseW)
        BTNTest_Check( "an image reserves iconW + gap (same as a glyph)", (w - baseW) = (16 + 8), sD )

        ' Either/or: the glyph on that side was cleared by setting an image.
        dim as wstring * 16 wgl = PsButton_GetGlyphLeft( h )
        BTNTest_Check( "setting an image clears the glyph on that side", (len(wgl) = 0), "glyph=[" & str(wgl) & "]" )

        ' Render: the image reached the left icon cell.
        SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
        dim as long tImg = PsButton_CountRenderedTones( h, BTN_PART_ICONLEFT )
        BTNTest_Check( "the image reached the left icon cell", (tImg > 1), "tones=" & str(tImg) )

        ' Removing the image gives the cell (and its gap) back.
        PsButton_SetImageLeft( h, "" )
        PsButton_GetIdealSize( h, w, hh )
        BTNTest_Check( "removing the image reclaims the cell", (w = baseW), "now=" & str(w) & " base=" & str(baseW) )

        DeleteFileW( wszTmp )
    end scope

    DestroyWindow(h)

    ' ------------------------------------------------------------------------------------
    ' The Enter/Space BEEP.
    '
    ' Claiming a key with DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which
    ' manufactures a matching WM_CHAR. Nothing consuming that character means it reaches
    ' DefWindowProc, and the system beeps -- while the keystroke itself works perfectly. The
    ' beep is the ONLY symptom, so it cannot be heard by any assertion; what is asserted here
    ' is the mechanism that suppresses it, entirely through messages.
    '
    ' The control is DISABLED on purpose: the arm happens before the enabled check, so the
    ' mechanism is identical while nothing fires a callback or opens a dropdown -- and a
    ' disabled control that swallows the key owes the character too.
    scope
        dim as HWND hBeep = PsButton_Create( hWndParent, 7801 )
        PsButton_SetEnabled( hBeep, false )

        dim as MSG mBeep
        mBeep.hwnd = hBeep : mBeep.message = WM_CHAR : mBeep.wParam = 13

        SendMessageW( hBeep, WM_KEYDOWN, VK_RETURN, 0 )
        dim as LRESULT dc1 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        BTNTest_Check( "Enter arms the WM_CHAR swallow that stops the system beep", _
            (dc1 and DLGC_WANTALLKEYS) <> 0, "dlgcode=&h" & hex(dc1) )

        SendMessageW( hBeep, WM_CHAR, 13, 0 )
        dim as LRESULT dc2 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        ' The disarm is the proof the WM_CHAR was consumed. The return value cannot be:
        ' DefWindowProc also returns 0 for WM_CHAR, so it reads the same when it fell through.
        BTNTest_Check( "...and the WM_CHAR is then swallowed instead of reaching DefWindowProc", _
            (dc2 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc2) )

        ' A key we never claimed must leave ordinary characters alone, or a host accelerator
        ' on a printable key would stop working.
        SendMessageW( hBeep, WM_KEYDOWN, VK_TAB, 0 )
        mBeep.wParam = asc("a")
        dim as LRESULT dc3 = SendMessageW( hBeep, WM_GETDLGCODE, asc("a"), cast(LPARAM, @mBeep) )
        BTNTest_Check( "an unclaimed key leaves ordinary characters alone", _
            (dc3 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc3) )

        DestroyWindow( hBeep )
    end scope

    ' ====================================================================================
    ' TOOLTIP BACKEND -- mode, handles, and the delays surviving a switch
    '
    ' What is worth asserting here is NOT that the setters store what they were handed. It is
    ' that (a) the control still ships on the system backend, (b) a switch actually swaps the
    ' windows rather than leaving both alive, and (c) a hover time set on one backend is still
    ' in force after a switch to the other. (c) is the one with a real failure mode behind it:
    ' pushing a delay straight at the live tooltip loses it silently at the switch, and the tip
    ' still appears -- just on a different beat, with nothing to say so.
    '
    ' The text policy is asserted ACROSS the switch too, because the whole claim of this
    ' feature is that the backend changes how a tip is drawn and never what it says.
    ' ====================================================================================
    scope
        dim as HWND hT = PsButton_Create( hWndParent, 9807 )
        if hT = 0 then
            BTNTest_Check( "tooltip: could not create a test button", false, "Create returned 0" )
        else
            BTNTest_Check( "tooltip: the control still ships on the SYSTEM backend", _
                (PsButton_GetTooltipMode( hT ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsButton_GetTooltipMode( hT )) )
            BTNTest_Check( "tooltip: a comctl32 window exists on the system backend", _
                (PsButton_GetTooltipHandle( hT ) <> 0) andalso IsWindow(PsButton_GetTooltipHandle( hT )), _
                "hSys = " & str(cast(integer, PsButton_GetTooltipHandle( hT ))) )
            BTNTest_Check( "tooltip: and the PsTooltip handle is 0 while on the system backend", _
                (PsButton_GetPsTooltipHandle( hT ) = 0), _
                "hPs = " & str(cast(integer, PsButton_GetPsTooltipHandle( hT ))) )

            ' Set the hover time on the SYSTEM backend, then switch. comctl32 can be asked what
            ' it stored, so this half is verified against the tooltip itself rather than
            ' against our own copy of the number.
            PsButton_SetHoverTime( hT, 777 )
            dim as long nSysInitial = SendMessageW( PsButton_GetTooltipHandle( hT ), _
                                                    TTM_GETDELAYTIME, TTDT_INITIAL, 0 )
            BTNTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (nSysInitial = 777), "TTM_GETDELAYTIME = " & str(nSysInitial) & " ms" )

            dim as HWND hSysBefore = PsButton_GetTooltipHandle( hT )
            dim as boolean bSwitched = PsButton_SetTooltipMode( hT, PSTIP_MODE_PS )
            BTNTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitched, _
                "mode = " & str(PsButton_GetTooltipMode( hT )) )
            BTNTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBefore) = 0) andalso (PsButton_GetTooltipHandle( hT ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBefore)) )

            dim as HWND hPs = PsButton_GetPsTooltipHandle( hT )
            BTNTest_Check( "tooltip: a PsTooltip window exists after the switch", _
                (hPs <> 0) andalso IsWindow(hPs), "hPs = " & str(cast(integer, hPs)) )
            BTNTest_Check( "tooltip: the PsTooltip is ATTACHED to the button, not to its parent", _
                (PsTooltip_GetAttached( hPs ) = hT), _
                "attached = " & str(cast(integer, PsTooltip_GetAttached( hPs ))) )

            ' THE ASSERTION THIS BLOCK EXISTS FOR.
            dim as PSTOOLTIP ptr pTip = GetTooltipPointer( hPs )
            BTNTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTip <> 0) andalso (pTip->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTip, pTip->nInitialDelay, -1)) & " ms" )

            ' Text policy: same rule, both backends -- resolved THROUGH THE CALLBACK THE TIP
            ' ACTUALLY HOLDS, not by calling our thunk directly. That distinction is not
            ' pedantry: the first version of this assertion called PsButton_TipTextCallback
            ' itself, and an A/B that removed the SetTooltipCallback wiring entirely left the
            ' whole suite at 77/77 green -- a switched button would have shown no tip at all.
            ' A real dwell is what would exercise this end to end, and no self-test can produce
            ' one, so the wiring is what has to be asserted.
            PsButton_SetTooltipText( hT, "authored" )
            BTNTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTip <> 0) andalso (pTip->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTip andalso pTip->TooltipCallback, 1, 0)) )
            dim as DWSTRING wszA
            if (pTip <> 0) andalso (pTip->TooltipCallback <> 0) then
                wszA = pTip->TooltipCallback( hPs, hT, -1 )
            end if
            BTNTest_Check( "tooltip: authored text wins on the PsTooltip backend too", _
                (wszA = "authored"), "resolved = " & str(wszA) )

            ' A whole-control tip has no items, so the index must not disturb it. This is the
            ' call site that sends TTM_POP on the system backend.
            dim as PSBUTTON ptr pBT = GetButtonPointer(hT)
            PsTipHost_SetToolIndex( @pBT->tip, -1 )
            BTNTest_Check( "tooltip: SetToolIndex(-1) leaves the whole-control tip alive", _
                IsWindow(PsButton_GetPsTooltipHandle( hT )) <> 0, "hPs still a window" )

            ' ...and back. A mode switch is not one-way, and the delay has to survive the
            ' return trip as well or "honoured by both" is only half true.
            PsButton_SetTooltipMode( hT, PSTIP_MODE_SYSTEM )
            BTNTest_Check( "tooltip: switching back rebuilds the comctl32 tip", _
                (PsButton_GetTooltipHandle( hT ) <> 0) andalso _
                (IsWindow(PsButton_GetTooltipHandle( hT )) <> 0) andalso _
                (PsButton_GetPsTooltipHandle( hT ) = 0), _
                "hSys = " & str(cast(integer, PsButton_GetTooltipHandle( hT ))) )
            dim as long nBack = SendMessageW( PsButton_GetTooltipHandle( hT ), _
                                              TTM_GETDELAYTIME, TTDT_INITIAL, 0 )
            BTNTest_Check( "tooltip: ...carrying the hover time back with it", _
                (nBack = 777), "TTM_GETDELAYTIME = " & str(nBack) & " ms" )

            ' An unset delay must NOT be forced to zero by the switch machinery: comctl32's own
            ' default is what a host that expressed no opinion is entitled to.
            BTNTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsButton_GetTooltipHandle( hT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsButton_GetTooltipHandle( hT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hT )
        end if
    end scope

    ' The process-wide defaults are PsTooltip's, but the reason they exist is THIS: a control
    ' builds its own tip and the host never sees the handle, so an armed default is the only
    ' way a host theme can reach it. Asserted here, from the host side, rather than only in
    ' PsTooltip's own suite where no control is involved.
    scope
        PsTooltip_SetDefaultMaxWidth( 246 )
        dim as HWND hT2 = PsButton_Create( hWndParent, 9808 )
        if hT2 <> 0 then
            PsButton_SetTooltipMode( hT2, PSTIP_MODE_PS )
            dim as HWND hPs2 = PsButton_GetPsTooltipHandle( hT2 )
            BTNTest_Check( "tooltip: a host's process-wide default reaches a tip the control built itself", _
                (hPs2 <> 0) andalso (PsTooltip_GetMaxWidth( hPs2 ) = 246), _
                "maxwidth = " & str(iif(hPs2, PsTooltip_GetMaxWidth( hPs2 ), -1)) )
            DestroyWindow( hT2 )
        end if
        ' Leave the process as it was found, or every later tip in this run inherits it.
        PsTooltip_ClearDefaults()
    end scope

    dim as string sSummary = "=== PsButton: " & str(gBTN_TestCount - gBTN_FailCount) & _
                             "/" & str(gBTN_TestCount) & " assertions passed"
    if gBTN_FailCount > 0 then sSummary &= "  (" & str(gBTN_FailCount) & " FAILED)"
    print sSummary
end sub
