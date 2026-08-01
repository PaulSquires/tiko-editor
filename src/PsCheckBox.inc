''
''  PsCheckBox.inc  --  owner-drawn checkbox (Fluent box glyph + caption)
''

#include once "PsCheckBox.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"


' Forward declaration: the content setters re-apply auto-size, and they are defined well above
' the layout section where it lives.
declare sub PsCheckBox_ApplyAutoSize( byval pChk as PSCHECKBOX ptr )


function GetCheckBoxPointer( byval hCheckBox as HWND ) as PSCHECKBOX ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCheckBox)
    if pWindow = 0 then return 0
    return cast( PSCHECKBOX ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not.
' WM_LBUTTONUP is one of them here, because this control HOLDS MOUSE CAPTURE across a press and
' the up-message is what releases it. (PsComboBox, which takes no capture, honours its callback's
' answer for that message; a note copied in from there would be a lie here.)
' ========================================================================================
private function PsCheckBox_CallMessage( _
            byval pChk   as PSCHECKBOX ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pChk = 0 then return false
    if pChk->MessageCallback = 0 then return false

    dim as PSCHECKBOX_MESSAGEINFO m
    m.hCheckBox = hwnd
    m.uMsg      = uMsg
    m.wParam    = wParam
    m.lParam    = lParam
    return pChk->MessageCallback( @m )
end function


' Resolve the tooltip text: the control's own text if it has any, else the host callback.
' NO CAPTION FALLBACK -- a checkbox's caption is already on screen, so falling back to it would
' pop a tip that repeats what the user is looking at.
private function PsCheckBox_ResolveTooltipText( byval pChk as PSCHECKBOX ptr ) as DWSTRING
    if pChk = 0 then return ""
    if len( pChk->wszTooltip ) then
        dim as DWSTRING wszOut = pChk->wszTooltip
        return wszOut
    end if
    if pChk->TooltipCallback then return pChk->TooltipCallback( pChk->hWin )
    return ""
end function


' ========================================================================================
' Draw the checkbox: box glyph, caption, focus ring.
'
' The BACKGROUND is not drawn here -- PsCheckBox_OnPaint has already flooded the client with the
' resolved mood's BackColor, which is what lets a host paint callback that only wants to add a
' foreground still sit on the right colour. There is no chrome: this control draws no frame, no
' border and no rounded body, which is the whole difference from PsButton.
'
' Nothing in here touches a GDI object or a device context -- PsBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' THE COLOURS COME OUT OF PsCheckBox_ResolveMood + PsCheckBox_ResolveColors, not out of an if-chain
' written inline here. That is what lets the self-test assert the precedence and the
' checked/unchecked box split directly, instead of only being able to look at pixels.
'
' PRESENCE IS TESTED ON THE RECT, not on the string. LayoutCheckBox leaves rcText EMPTY when
' there is no caption, so IsRectEmpty is the single source of truth -- and a degenerate caption
' span (the client too narrow to leave any room) drops out of the same test rather than needing
' its own.
' ========================================================================================
private sub PsCheckBox_RenderDefault( _
            byval pChk as PSCHECKBOX ptr, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSCHECKBOX_PAINTINFO ptr _
            )

    if (pChk = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long cw = p->rcContent.right - p->rcContent.left
    dim as long ch = p->rcContent.bottom - p->rcContent.top
    if (cw <= 0) orelse (ch <= 0) then exit sub

    ' ---- which three colors ------------------------------------------------------------
    dim as long nMood = PsCheckBox_ResolveMood( p->isEnabled, p->isPressed, p->isHot )
    dim as COLORREF clrBack, clrFore, clrBox
    PsCheckBox_ResolveColors( @pChk->colors, nMood, p->isChecked, clrBack, clrFore, clrBox )

    ' ---- the fonts ---------------------------------------------------------------------
    ' Paint the caption with the SAME font the measuring pass used -- the caller's if set, else
    ' the stock GUI font. PsBufferPaint.SetFont IGNORES a null handle, so leaving it unset would
    ' silently paint in the DC's font while the width was measured in another, and long captions
    ' would clip for no visible reason.
    dim as HFONT hTextUse = pChk->hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    ' The glyph font falls back to the caption font rather than to the stock font: a host that
    ' set one font and not the other almost certainly meant that one. (It will draw the Fluent
    ' codepoint as a missing-glyph box, which is a visible failure rather than a silent one.)
    dim as HFONT hGlyphUse = pChk->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = hTextUse

    ' ---- the box ------------------------------------------------------------------------
    ' The cell is a LAYOUT rect, not a measurement of the glyph: the control never measures the
    ' box font, so a glyph too large for the cell clips rather than resizing the control.
    ' p->wszGlyph is ALREADY resolved for isChecked (PSCHECKBOX.ResolveGlyph), so the state test
    ' lives in exactly one place and this painter cannot disagree with a host's.
    dim as RECT rcB = p->rcBox
    b->SetFont( hGlyphUse )
    b->SetColors( clrBox, clrBack )
    b->PaintText( p->wszGlyph, @rcB, DT_CENTER )

    ' ---- the caption -------------------------------------------------------------------
    ' rcText is the SPAN between the box cell and the far padding, which is exactly what
    ' DT_END_ELLIPSIS needs: handed a tight box the ellipsis would have nowhere to happen and the
    ' text would simply clip. DT_NOPREFIX / DT_VCENTER / DT_SINGLELINE are added by PaintText
    ' itself -- and the first of those is what makes "&Enable" draw a literal ampersand, so this
    ' control's no-mnemonics rule is enforced by the renderer rather than merely intended.
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as DWORD dtAlign = DT_LEFT
        select case p->nTextAlign
        case CHK_TEXTALIGN_CENTER
            dtAlign = DT_CENTER
        case CHK_TEXTALIGN_RIGHT
            dtAlign = DT_RIGHT
        end select

        dim as RECT rcT = p->rcText
        b->SetFont( hTextUse )
        b->SetColors( clrFore, clrBack )
        b->PaintText( p->wszText, @rcT, dtAlign or DT_END_ELLIPSIS )
    end if

    ' ---- the focus ring ----------------------------------------------------------------
    ' Outline, never filled: it is drawn OVER the control, and a filled round rect here would
    ' erase everything above it (see PsCheckBox_CountRenderedTones). Its band is reserved in the
    ' layout whether or not we are focused, which is what stops the content jumping when focus
    ' arrives.
    '
    ' The curvature is its own setting rather than derived from a body curvature the way PsButton
    ' derives its ring: there is no body here to stay concentric with.
    if p->isFocused then
        if pChk->nFocusThick > 0 then
            dim as RECT rcRing = p->rcVisual
            b->SetPenColor( pChk->colors.FocusRingColor )
            b->PaintRoundOutline( @rcRing, pChk->nFocusCurvature, pChk->nFocusThick )
        end if
    end if
end sub


' ========================================================================================
' Fill a PAINTINFO from the control's current state and geometry. One place, so the WM_PAINT
' path and the two offscreen probes cannot describe the control differently.
' ========================================================================================
private sub PsCheckBox_FillPaintInfo( _
            byval pChk as PSCHECKBOX ptr, _
            byval hwnd as HWND, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSCHECKBOX_PAINTINFO ptr _
            )

    if (pChk = 0) orelse (p = 0) then exit sub

    p->hCheckBox = hwnd
    p->b         = b
    GetClientRect( hwnd, @p->rcClient )
    p->rcContent  = pChk->rcContent
    p->rcBox      = pChk->rcBox
    p->rcText     = pChk->rcText
    p->rcVisual   = pChk->rcVisual
    p->isChecked  = pChk->isChecked
    p->isHot      = pChk->isHot
    p->isPressed  = pChk->isPressed
    p->isEnabled  = pChk->isEnabled
    p->isFocused  = pChk->isFocused
    p->wszText    = pChk->wszText
    p->wszGlyph   = pChk->ResolveGlyph()
    p->nBoxAlign  = pChk->nBoxAlign
    p->nTextAlign = pChk->nTextAlign
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function PsCheckBox_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hwnd)
    if pChk = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' The client is filled with the resolved mood's own background, not a fixed one, so a paint
    ' callback that draws only a foreground still sits on the right colour. Resolved through the
    ' same two functions the painter uses, so the fill can never disagree with the rest.
    dim as long nMood = PsCheckBox_ResolveMood( pChk->isEnabled, pChk->isPressed, pChk->isHot )
    dim as COLORREF clrFill, clrFore, clrBox
    PsCheckBox_ResolveColors( @pChk->colors, nMood, pChk->isChecked, clrFill, clrFore, clrBox )
    b.SetBackColor( clrFill )
    b.PaintClientRect()

    dim p as PSCHECKBOX_PAINTINFO
    PsCheckBox_FillPaintInfo( pChk, hwnd, @b, @p )

    if pChk->PaintCallback then
        pChk->PaintCallback( @p )
    else
        PsCheckBox_RenderDefault( pChk, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub PsCheckBox_ClearHot( byval pChk as PSCHECKBOX ptr, byval hwnd as HWND )
    if pChk = 0 then exit sub
    if pChk->hotTimerOn then
        KillTimer( hwnd, IDT_CCHECKBOX_HOTTRACK )
        pChk->hotTimerOn = false
    end if
    if pChk->isHot then
        pChk->isHot = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Apply the ideal size to ourselves, preserving the top-left. Only runs when the host has opted
' in with PsCheckBox_SetAutoSize -- the family default is that a control never moves itself and
' the host measures with GetIdealSize.
'
' Called from every mutator that can change the ideal size. It forces the pending layout first
' (that is where the measurement happens), then resizes -- which marks the layout dirty again
' through WM_SIZE, so the rects are re-derived against the new client area.
'
' NOTE WHICH MUTATORS ARE ABSENT FROM THAT LIST: SetChecked, SetBoxAlign and the two glyph
' setters. None of them can move the ideal size -- the two glyphs share one DECLARED cell, and
' flipping the alignment mirrors the layout without changing what it costs.
' ========================================================================================
sub PsCheckBox_ApplyAutoSize( byval pChk as PSCHECKBOX ptr )
    if pChk = 0 then exit sub
    if pChk->bAutoSize = false then exit sub
    if pChk->hWin = 0 then exit sub

    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if (pChk->nIdealW <= 0) orelse (pChk->nIdealH <= 0) then exit sub

    SetWindowPos( pChk->hWin, 0, 0, 0, pChk->nIdealW, pChk->nIdealH, _
                  SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
end sub


' ========================================================================================
' The one activation path. A completed mouse click, Space/Enter, and the programmatic
' PsCheckBox_Toggle all come through here, so keyboard, mouse and host can never drift apart --
' and so there is exactly one place that both flips the state and notifies.
'
' It reads isEnabled again rather than trusting the caller: a message callback may have disabled
' the control mid-gesture.
'
' NO RE-LAYOUT. The two glyphs share one declared cell, so flipping the state cannot move a
' single rect -- an InvalidateRect without the erase flag is the whole cost.
' ========================================================================================
private sub PsCheckBox_FireToggle( byval pChk as PSCHECKBOX ptr, byval hwnd as HWND )
    if pChk = 0 then exit sub
    if pChk->isEnabled = false then exit sub

    pChk->isChecked = not pChk->isChecked
    InvalidateRect( hwnd, NULL, FALSE )

    if pChk->CheckChangedCallback then
        pChk->CheckChangedCallback( hwnd, pChk->isChecked )
    end if
end sub


' ========================================================================================
' PsCheckBox Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is handled
' here and anything unhandled goes to DefWindowProc.
'
' MOUSE CAPTURE IS TAKEN. The family's test is "take capture only if something consumes the
' guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and here it comes out positive: the
' press/cancel gesture (press the checkbox, slide off it, release, the state does NOT flip) is
' exactly that. So the full price is paid -- snapshot the press before releasing, a
' WM_CAPTURECHANGED handler that cancels it, a release in WM_DESTROY, and a callback result that
' is ignored for the up-message.
'
' THE WHOLE CLIENT IS THE HIT AREA, caption included, exactly as a system checkbox behaves.
' There is no hit test and no notion of clicking "the box" specifically.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET, which is PsToggle's and PsIconPanel's call rather than
' PsSelectBar's. With it, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD OF a
' second WM_LBUTTONDOWN -- and on a control that flips once per click, a rapid second click is a
' legitimate second flip that would be silently swallowed.
'
' FOCUS: this is the fourth focusable control in the family, after PsToggle, PsComboBox and
' PsButton. See the WM_GETDLGCODE handler for the one subtlety that needed care, and the note in
' PsCheckBox.bi for why Enter activates here even though a system checkbox ignores it.
' ========================================================================================
function PsCheckBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsCheckBox_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a background
        ' erase here would only add flicker.
        return true

    case WM_SIZE
        if pChk then
            pChk->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_TIMER
        if (pChk <> 0) andalso (wParam = IDT_CCHECKBOX_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot state ourselves.
            '
            ' NOT while a press is live: during capture the cursor is legitimately allowed to
            ' wander off and come back, and the WM_MOUSEMOVE handler is already maintaining isHot
            ' for exactly that. Killing the timer here would also strip the press of its safety
            ' net for the moment the control comes back under the cursor.
            if pChk->isPressed = false then
                dim as RECT rcClient
                GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then
                    PsCheckBox_ClearHot( pChk, hwnd )
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pChk = 0 then exit select
        if pChk->isEnabled = false then exit select

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

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits quickly
            ' (or onto another control), which would strand the control painted hot.
            if pChk->hotTimerOn = false then
                SetTimer( hwnd, IDT_CCHECKBOX_HOTTRACK, PSCHECKBOX_HOTTRACK_MS, NULL )
                pChk->hotTimerOn = true
            end if
        end if

        ' This is also what drives the press-cancel FEEDBACK: with capture held, moves outside the
        ' client still reach us, so sliding off a pressed checkbox clears isHot -- which drops the
        ' mood from PRESSED back to IDLE, before the release decides that nothing happened.
        if bOver <> pChk->isHot then
            pChk->isHot = bOver
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pChk then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for when
            ' this message never arrives). Suppressed during a press for the same reason as in
            ' the timer: the gesture is still live and WM_MOUSEMOVE owns isHot.
            if pChk->isPressed = false then PsCheckBox_ClearHot( pChk, hwnd )
            if PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pChk = 0 then return 0
        ' A disabled window does not receive mouse input at all (PsCheckBox_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some other
        ' route.
        if pChk->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true even
        ' if the host goes on to suppress the press. This runs WM_SETFOCUS synchronously, so
        ' isFocused is already correct by the time we paint below.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam ) then return 0

        SetCapture( hwnd )
        pChk->isPressed = true
        pChk->isHot     = true
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_LBUTTONUP
        if pChk = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window losing
        ' capture SYNCHRONOUSLY, and our handler for it cancels the press -- so reading isPressed
        ' after the release would always find FALSE and nothing would ever toggle.
        dim as boolean bWasPressed = pChk->isPressed
        ' Release capture unconditionally, before any callback runs. Nothing below -- not a
        ' callback, not a host handler -- gets the chance to leave it held.
        if GetCapture() = hwnd then ReleaseCapture()

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bInside = isMouseOverRECT( hwnd, rcClient )

        pChk->CancelPress()
        InvalidateRect( hwnd, NULL, FALSE )

        ' The result is deliberately IGNORED here (see CHK_MessageCallbackFunc).
        PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release with the cursor still inside flips the state. Press, slide
        ' off, release = nothing happened, which is the whole reason for capture.
        if (bWasPressed andalso bInside) then PsCheckBox_FireToggle( pChk, hwnd )
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell). The
        ' gesture is over; forget it and repaint.
        if pChk then
            pChk->CancelPress()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pChk then
            if PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' A pending swallow must not survive a focus change: the WM_CHAR it was armed for is
        ' not coming, and a stale flag would eat the next character instead.
        if pChk then pChk->bAteKeyDown = false
        ' Focus is a FACT reported by the system, not an action a host can veto, so the state is
        ' updated first and the callback's result is IGNORED. A host that wants to refuse focus
        ' must not make the control a tabstop in the first place.
        if pChk then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pChk->isFocused then
                pChk->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the two keys we actually consume, and only when we are being asked about a
        ' specific message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the control
        ' would swallow the very navigation it just opted into by being a tabstop. Claiming
        ' nothing would let a host's IsDialogMessage pump route Enter somewhere else before the
        ' control ever saw it. Per-message is the only answer that gets both right.
        if lParam then
            dim as MSG ptr pMsg = cast( MSG ptr, lParam )
            ' Claim the CHARACTER produced by a key we already consumed. Claiming a key with
            ' DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which manufactures a
            ' matching WM_CHAR; if nothing claims that, it reaches DefWindowProc and beeps.
            if pMsg->message = WM_CHAR then
                if pChk then
                    if pChk->bAteKeyDown then return DLGC_WANTALLKEYS
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
        if pChk = 0 then exit select
        if pChk->bAteKeyDown then
            pChk->bAteKeyDown = false
            return 0
        end if

    case WM_KEYDOWN
        if pChk = 0 then exit select
        ' Arm the character swallow (see WM_CHAR). Every path below that handles one of these
        ' keys consumes it, so the key alone decides -- and only keys that actually produce a
        ' WM_CHAR are armed.
        pChk->bAteKeyDown = (wParam = VK_SPACE) orelse (wParam = VK_RETURN)
        if pChk->isEnabled = false then return 0
        if PsCheckBox_CallMessage( pChk, hwnd, uMsg, wParam, lParam ) then return 0
        select case wParam
        case VK_SPACE, VK_RETURN
            ' Same path a completed click takes, so keyboard and mouse cannot drift apart: both
            ' are user action, both notify.
            '
            ' VK_RETURN is here by choice, not by inheritance -- a system checkbox ignores Enter
            ' and lets it reach the dialog's default button. PsToggle and PsButton both take it, so
            ' taking it here keeps the focusable half of the family consistent. See the API
            ' header in PsCheckBox.bi.
            PsCheckBox_FireToggle( pChk, hwnd )
            return 0
        end select

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsCheckBox_SetEnabled still gets the greyed rendering
        ' rather than a control that looks live but eats input.
        if pChk then
            pChk->isEnabled = (wParam <> 0)
            if pChk->isEnabled = false then
                pChk->isHot = false
                pChk->CancelPress()
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_SETTEXT
        ' The caption has ONE store, reached through two doors: PsCheckBox_SetText and this
        ' message. DefWindowProc is deliberately not called -- letting it keep a second copy in
        ' the window's own text would give GetWindowText an answer that could drift from what is
        ' painted.
        if pChk = 0 then exit select
        if lParam = 0 then
            pChk->wszText = ""
        else
            pChk->wszText = *cast( wstring ptr, lParam )
        end if
        pChk->Refresh()
        PsCheckBox_ApplyAutoSize( pChk )
        return TRUE

    case WM_GETTEXT
        ' wParam = buffer size in CHARACTERS including the terminator; return value excludes it.
        ' Copied by hand rather than through a wstring assignment, because the caller's buffer is
        ' not ours to overrun.
        if pChk = 0 then exit select
        dim as long nMax = cast( long, wParam )
        if nMax <= 0 then return 0
        dim as ushort ptr pDst = cast( ushort ptr, lParam )
        if pDst = 0 then return 0
        dim as long nLen = len( pChk->wszText )
        if nLen > (nMax - 1) then nLen = nMax - 1
        if nLen > 0 then
            dim as ushort ptr pSrc = cast( ushort ptr, pChk->wszText.vptr )
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
        ' Required, not optional: GetWindowText asks this first and allocates from the answer, so
        ' without it a caller gets an empty string no matter what WM_GETTEXT would have said.
        if pChk = 0 then exit select
        return len( pChk->wszText )

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply it.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pChk then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' Store in an instance-lifetime buffer and point lpszText at it. NOT in
                ' wszTooltip: that field is the host's authored text, and a callback's answer
                ' written there would silently become stored state.
                pChk->wszTipBuf = PsCheckBox_ResolveTooltipText( pChk )
                pdi->lpszText = cast( LPWSTR, pChk->wszTipBuf.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pChk then
            ' Don't let the hot-tracking timer outlive the window...
            if pChk->hotTimerOn then
                KillTimer( hwnd, IDT_CCHECKBOX_HOTTRACK )
                pChk->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pChk->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored checkbox data pointer. The two fonts are the host's and are left
        ' alone; the tooltip window is ours and goes with us.
        if pChk then
            PsTipHost_Destroy( @pChk->tip )
            delete(pChk)
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
' This thunk is why PsTipHost never learns the text policy: PsCheckBox_ResolveTooltipText
' stays the ONE place that decides what a tip says, and the system backend reaches the
' same function by the other road (comctl32 asking through TTN_GETDISPINFOW), which is
' why that arm stays in the WndProc.
' ========================================================================================
private function PsCheckBox_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                        byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsCheckBox_ResolveTooltipText( GetCheckBoxPointer(hAttached) )
    return wszOut
end function


' ========================================================================================
' PsCheckBox_Create
' ========================================================================================
function PsCheckBox_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle parameter to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that supplies
    ' only dwStyle silently declares itself a CONTAINER. The dialog manager then descends into
    ' it hunting for tabstops, finds no children, and SKIPS IT: the control becomes unreachable
    ' by Tab. That shipped in PsButton's first commit and was then found in PsToggle and PsComboBox
    ' too; see C:\dev\Learnings.md. It is ASSERTED in the self-test below, not merely remembered.
    '
    ' A leaf control must be a tab ITEM, not a tab container. WS_EX_WINDOWEDGE goes with it -- it
    ' is a raised-border hint that means nothing to a window drawing its own chrome.
    '
    ' DO NOT COPY THIS LINE INTO A WRAP-A-REAL-CHILD CONTROL. PsListBox, PsTextBox and
    ' PsNumericUpDown all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner child
    ' and their container is a real container; passing 0 there would make them unreachable. The
    ' test is "does this window have children the dialog manager should find?", not "is this a
    ' control?".
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsCheckBox_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header. It would turn
    ' every second rapid click into a WM_LBUTTONDBLCLK the control would drop.

    dim as PSCHECKBOX ptr pChk = new PSCHECKBOX
    pChk->hWin         = hCtrl
    pChk->idc_CheckBox = CtrlID
    pChk->id           = CtrlID     ' sensible default; PsCheckBox_SetID overrides it

    ' The two box glyphs. Seeded here rather than as TYPE field initializers -- a DWSTRING member
    ' cannot carry one.
    pChk->wszGlyphUnchecked = PSCHECKBOX_GLYPH_UNCHECKED
    pChk->wszGlyphChecked   = PSCHECKBOX_GLYPH_CHECKED

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). nFocusThick is not scaled at all, here or
    ' anywhere -- a hairline stays a hairline.
    pChk->nPadLeft        = pWindow->ScaleX( PSCHECKBOX_DEFAULT_PADLEFT )
    pChk->nPadTop         = pWindow->ScaleY( PSCHECKBOX_DEFAULT_PADTOP )
    pChk->nPadRight       = pWindow->ScaleX( PSCHECKBOX_DEFAULT_PADRIGHT )
    pChk->nPadBottom      = pWindow->ScaleY( PSCHECKBOX_DEFAULT_PADBOTTOM )
    pChk->nBoxGap         = pWindow->ScaleX( PSCHECKBOX_DEFAULT_BOXGAP )
    pChk->nBoxWidth       = pWindow->ScaleX( PSCHECKBOX_DEFAULT_BOXWIDTH )
    pChk->nBoxHeight      = pWindow->ScaleY( PSCHECKBOX_DEFAULT_BOXHEIGHT )
    pChk->nFocusGap       = pWindow->ScaleY( PSCHECKBOX_DEFAULT_FOCUSGAP )
    pChk->nFocusCurvature = pWindow->ScaleY( PSCHECKBOX_DEFAULT_FOCUSCURV )


    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pChk)

    ' AFTER UserData, not before: PsCheckBox_TipTextCallback resolves the control through
    ' GetCheckBoxPointer(hAttached), which reads exactly that slot.
    PsTipHost_Ensure( @pChk->tip, hCtrl, @PsCheckBox_TipTextCallback )

    function = hCtrl

end function


' ========================================================================================
' Content.  All SILENT -- they change what is drawn, never what was clicked.
' ========================================================================================
function PsCheckBox_GetText( byval hCheckBox as HWND ) as DWSTRING
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return ""
    dim as DWSTRING wszOut = pChk->wszText
    return wszOut
end function

sub PsCheckBox_SetText( byval hCheckBox as HWND, byval Text as DWSTRING )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->wszText = Text
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

function PsCheckBox_GetGlyphUnchecked( byval hCheckBox as HWND ) as DWSTRING
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return ""
    dim as DWSTRING wszOut = pChk->wszGlyphUnchecked
    return wszOut
end function

sub PsCheckBox_SetGlyphUnchecked( byval hCheckBox as HWND, byval Glyph as DWSTRING )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->wszGlyphUnchecked = Glyph
    ' No re-measure and no auto-size: the cell is DECLARED, so a different glyph cannot change
    ' the ideal width. Repaint only.
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

function PsCheckBox_GetGlyphChecked( byval hCheckBox as HWND ) as DWSTRING
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return ""
    dim as DWSTRING wszOut = pChk->wszGlyphChecked
    return wszOut
end function

sub PsCheckBox_SetGlyphChecked( byval hCheckBox as HWND, byval Glyph as DWSTRING )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->wszGlyphChecked = Glyph
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

function PsCheckBox_GetID( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->id
end function

sub PsCheckBox_SetID( byval hCheckBox as HWND, byval id as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->id = id
end sub

function PsCheckBox_GetItemData( byval hCheckBox as HWND ) as integer
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->itemData
end function

sub PsCheckBox_SetItemData( byval hCheckBox as HWND, byval itemData as integer )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->itemData = itemData
end sub


' ========================================================================================
' State.
' ========================================================================================
function PsCheckBox_GetChecked( byval hCheckBox as HWND ) as boolean
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    return pChk->isChecked
end function

' SILENT, by the family rule and by Win32's BM_SETCHECK precedent. The action that notifies is
' PsCheckBox_Toggle. No re-measure: the two glyphs share one declared cell, so nothing moves.
sub PsCheckBox_SetChecked( byval hCheckBox as HWND, byval isChecked as boolean )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if pChk->isChecked = isChecked then exit sub
    pChk->isChecked = isChecked
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

function PsCheckBox_GetEnabled( byval hCheckBox as HWND ) as boolean
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    return pChk->isEnabled
end function

' Through EnableWindow, so the disable is enforced by the system rather than being a cosmetic
' flag. WM_ENABLE (sent synchronously) is what actually updates isEnabled, clears the hover and
' cancels any live press.
sub PsCheckBox_SetEnabled( byval hCheckBox as HWND, byval isEnabled as boolean )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    EnableWindow( hCheckBox, iif( isEnabled, TRUE, FALSE ) )
    PsCheckBox_ClearHot( pChk, hCheckBox )
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

function PsCheckBox_GetFocused( byval hCheckBox as HWND ) as boolean
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    return pChk->isFocused
end function

sub PsCheckBox_Refresh( byval hCheckBox as HWND )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->Refresh()
end sub


' ========================================================================================
' Action.  Fires the change callback -- see the declaration in PsCheckBox.bi for why an action is
' not a setter, and PsCheckBox_SetChecked for the silent version.
' ========================================================================================
sub PsCheckBox_Toggle( byval hCheckBox as HWND )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    PsCheckBox_FireToggle( pChk, hCheckBox )
end sub


' ========================================================================================
' Layout.  Every setter takes RAW PIXELS.
' ========================================================================================
function PsCheckBox_GetBoxAlign( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return CHK_BOXALIGN_LEFT
    return pChk->nBoxAlign
end function

' No auto-size call: the mirror is exact, so flipping sides cannot change the ideal size.
sub PsCheckBox_SetBoxAlign( byval hCheckBox as HWND, byval nBoxAlign as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if (nBoxAlign < CHK_BOXALIGN_LEFT) orelse (nBoxAlign > CHK_BOXALIGN_RIGHT) then exit sub
    pChk->nBoxAlign = nBoxAlign
    pChk->Refresh()
end sub

function PsCheckBox_GetTextAlign( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return CHK_TEXTALIGN_LEFT
    return pChk->nTextAlign
end function

sub PsCheckBox_SetTextAlign( byval hCheckBox as HWND, byval nTextAlign as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if (nTextAlign < CHK_TEXTALIGN_LEFT) orelse (nTextAlign > CHK_TEXTALIGN_RIGHT) then exit sub
    pChk->nTextAlign = nTextAlign
    ' Alignment is a paint-time flag: the SPAN does not move, only the ink inside it.
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

sub PsCheckBox_GetPadding( byval hCheckBox as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
    nLeft = 0 : nTop = 0 : nRight = 0 : nBottom = 0
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    nLeft   = pChk->nPadLeft
    nTop    = pChk->nPadTop
    nRight  = pChk->nPadRight
    nBottom = pChk->nPadBottom
end sub

sub PsCheckBox_SetPadding( byval hCheckBox as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if nLeft   < 0 then nLeft   = 0
    if nTop    < 0 then nTop    = 0
    if nRight  < 0 then nRight  = 0
    if nBottom < 0 then nBottom = 0
    pChk->nPadLeft   = nLeft
    pChk->nPadTop    = nTop
    pChk->nPadRight  = nRight
    pChk->nPadBottom = nBottom
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

function PsCheckBox_GetBoxGap( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->nBoxGap
end function

sub PsCheckBox_SetBoxGap( byval hCheckBox as HWND, byval nBoxGap as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if nBoxGap < 0 then nBoxGap = 0
    pChk->nBoxGap = nBoxGap
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

sub PsCheckBox_GetBoxSize( byval hCheckBox as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
    nBoxWidth = 0 : nBoxHeight = 0
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    nBoxWidth  = pChk->nBoxWidth
    nBoxHeight = pChk->nBoxHeight
end sub

sub PsCheckBox_SetBoxSize( byval hCheckBox as HWND, byval nBoxWidth as long, byval nBoxHeight as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if nBoxWidth  < 0 then nBoxWidth  = 0
    if nBoxHeight < 0 then nBoxHeight = 0
    pChk->nBoxWidth  = nBoxWidth
    pChk->nBoxHeight = nBoxHeight
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

sub PsCheckBox_GetFocusRing( byval hCheckBox as HWND, byref nGap as long, byref nThickness as long )
    nGap = 0 : nThickness = 0
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    nGap       = pChk->nFocusGap
    nThickness = pChk->nFocusThick
end sub

sub PsCheckBox_SetFocusRing( byval hCheckBox as HWND, byval nGap as long, byval nThickness as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if nGap       < 0 then nGap       = 0
    if nThickness < 0 then nThickness = 0
    pChk->nFocusGap   = nGap
    pChk->nFocusThick = nThickness
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

function PsCheckBox_GetFocusCurvature( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->nFocusCurvature
end function

sub PsCheckBox_SetFocusCurvature( byval hCheckBox as HWND, byval nCurvature as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pChk->nFocusCurvature = nCurvature
    ' Paint-time only: the ring band's WIDTH is nFocusGap + nFocusThick, not its curvature.
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub

function PsCheckBox_GetAutoSize( byval hCheckBox as HWND ) as boolean
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    return pChk->bAutoSize
end function

sub PsCheckBox_SetAutoSize( byval hCheckBox as HWND, byval bAutoSize as boolean )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->bAutoSize = bAutoSize
    PsCheckBox_ApplyAutoSize( pChk )
end sub

' Forces a pending layout. Valid BEFORE the control has ever been sized -- the measuring pass in
' LayoutCheckBox runs ahead of the zero-client bail precisely so this is answerable.
sub PsCheckBox_GetIdealSize( byval hCheckBox as HWND, byref nWidth as long, byref nHeight as long )
    nWidth = 0 : nHeight = 0
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    nWidth  = pChk->nIdealW
    nHeight = pChk->nIdealH
end sub

function PsCheckBox_GetContentRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if pChk->bLayoutDirty then return false          ' no client area yet
    rc = pChk->rcContent
    return true
end function

function PsCheckBox_GetBoxRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if pChk->bLayoutDirty then return false
    rc = pChk->rcBox
    return true
end function

function PsCheckBox_GetTextRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if pChk->bLayoutDirty then return false
    rc = pChk->rcText
    return true
end function

function PsCheckBox_GetVisualRect( byval hCheckBox as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if pChk->bLayoutDirty then return false
    rc = pChk->rcVisual
    return true
end function


' ========================================================================================
' Appearance.
' ========================================================================================
sub PsCheckBox_GetColors( byval hCheckBox as HWND, byval pColors as PSCHECKBOX_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    *pColors = pChk->colors
end sub

sub PsCheckBox_SetColors( byval hCheckBox as HWND, byval pColors as PSCHECKBOX_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->colors = *pColors
    InvalidateRect( hCheckBox, NULL, TRUE )
end sub

function PsCheckBox_GetFont( byval hCheckBox as HWND ) as HFONT
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->hTextFont
end function

' The MEASURING font. Changing it re-measures, and can therefore change the ideal size.
sub PsCheckBox_SetFont( byval hCheckBox as HWND, byval hTextFont as HFONT )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->hTextFont = hTextFont
    pChk->Refresh()
    PsCheckBox_ApplyAutoSize( pChk )
end sub

function PsCheckBox_GetGlyphFont( byval hCheckBox as HWND ) as HFONT
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return pChk->hGlyphFont
end function

' A PAINT-TIME input only (PsIconPanel's rule): the box cell is declared rather than measured, so
' this repaints but never re-lays-out and never resizes.
sub PsCheckBox_SetGlyphFont( byval hCheckBox as HWND, byval hGlyphFont as HFONT )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->hGlyphFont = hGlyphFont
    InvalidateRect( hCheckBox, NULL, FALSE )
end sub


' ========================================================================================
' Tooltips.
' ========================================================================================
function PsCheckBox_GetTooltipText( byval hCheckBox as HWND ) as DWSTRING
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return ""
    dim as DWSTRING wszOut = pChk->wszTooltip
    return wszOut
end function

sub PsCheckBox_SetTooltipText( byval hCheckBox as HWND, byval Text as DWSTRING )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->wszTooltip = Text
end sub

' The comctl32 handle, and 0 while on PsTooltip -- the honest answer, since a TTM_* sent
' to a PsTooltip window is silently ignored.
function PsCheckBox_GetTooltipHandle( byval hCheckBox as HWND ) as HWND
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return PsTipHost_SysHandle( @pChk->tip )
end function

' The PsTooltip window, and 0 while on the system backend. This is the door to PsTooltip's
' own SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph, none of which is mirrored
' on this control.
function PsCheckBox_GetPsTooltipHandle( byval hCheckBox as HWND ) as HWND
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return 0
    return PsTipHost_PsHandle( @pChk->tip )
end function

function PsCheckBox_SetTooltipMode( byval hCheckBox as HWND, byval nMode as long ) as boolean
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    return PsTipHost_SetMode( @pChk->tip, nMode )
end function

function PsCheckBox_GetTooltipMode( byval hCheckBox as HWND ) as long
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pChk->tip )
end function

' The three delays go through PsTipHost, which STORES them as well as pushing them, so a
' hover time set before a mode switch is still in force after it. Pushing straight at the
' live tooltip loses it at the switch -- and the tip still appears, just on a different
' beat, with nothing reporting the change.
sub PsCheckBox_SetHoverTime( byval hCheckBox as HWND, byval milliseconds as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    PsTipHost_SetDelay( @pChk->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsCheckBox_SetAutoPopTime( byval hCheckBox as HWND, byval milliseconds as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    PsTipHost_SetDelay( @pChk->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsCheckBox_SetReshowTime( byval hCheckBox as HWND, byval milliseconds as long )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    PsTipHost_SetDelay( @pChk->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsCheckBox_SetPaintCallback( byval hCheckBox as HWND, byval usersub as CHK_PaintCallbackSub )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->PaintCallback = usersub
    InvalidateRect( hCheckBox, NULL, TRUE )
end sub

sub PsCheckBox_SetMessageCallback( byval hCheckBox as HWND, byval userfunc as CHK_MessageCallbackFunc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->MessageCallback = userfunc
end sub

sub PsCheckBox_SetTooltipCallback( byval hCheckBox as HWND, byval userfunc as CHK_TooltipCallbackFunc )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->TooltipCallback = userfunc
end sub

sub PsCheckBox_SetCheckChangedCallback( byval hCheckBox as HWND, byval usersub as CHK_CheckChangedCallbackSub )
    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then exit sub
    pChk->CheckChangedCallback = usersub
end sub


' ========================================================================================
' Which rect does this part name? Shared by the two probes and nothing else, but factored out so
' their bodies stay about pixels rather than about rect selection.
' ========================================================================================
private function PsCheckBox_PartRect( byval pChk as PSCHECKBOX ptr, byval nPart as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    if pChk = 0 then return rc
    select case nPart
    case CHK_PART_BOX
        rc = pChk->rcBox
    case CHK_PART_TEXT
        rc = pChk->rcText
    case else
        rc = pChk->rcContent
    end select
    return rc
end function


' ========================================================================================
' Render the control OFFSCREEN with its current painter and measure one of its rects two ways:
' the number of distinct colours in it (capped at 64) and an FNV-1a hash of its pixels. Both
' public probes are thin wrappers over this, so they cannot disagree about what was rendered.
'
' THREE THINGS HERE ARE LOAD-BEARING, and all three come from the write-ups in Learnings.md:
'
'   1. The PAINTINFO is filled by PsCheckBox_FillPaintInfo -- the SAME routine WM_PAINT uses. The
'      first version of this probe elsewhere in the family hand-built the struct and left
'      rcVisual zeroed; PaintRectFactory bails on a negative extent, so the focus-ring call
'      became a no-op and the A/B came back identical. A probe that skips the code path it exists
'      to measure is worse than no probe.
'   2. isFocused is forced TRUE. The ring is the step the flood defect lives on -- it is drawn
'      last, over the largest rect -- so a probe run on an unfocused control measures nothing.
'   3. The buffer is given a CACHED memDC (the four-argument BeginDoubleBuffer), so
'      EndDoubleBuffer flushes GDI+ and blits WITHOUT deleting the bitmap underneath us. GDI+
'      batches: reading the bits before that flush would sample a surface the shapes have not
'      reached yet.
'
' Returns FALSE (and leaves both outputs at 0) if the control has no geometry yet or the part
' rect is empty.
' ========================================================================================
private function PsCheckBox_ProbePart( _
            byval hCheckBox as HWND, _
            byval nPart     as long, _
            byref nTones    as long, _
            byref nHash     as ulong _
            ) as boolean

    nTones = 0
    nHash  = 0

    dim as PSCHECKBOX ptr pChk = GetCheckBoxPointer(hCheckBox)
    if pChk = 0 then return false
    if pChk->bLayoutDirty then pChk->LayoutCheckBox()
    if pChk->bLayoutDirty then return false          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hCheckBox, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return false

    dim as RECT rcPart = PsCheckBox_PartRect( pChk, nPart )
    if IsRectEmpty( @rcPart ) then return false

    ' Clamp to the client: rcVisual-derived rects can legitimately extend past it when the host
    ' sized the control smaller than ideal, and the DIB only covers the client.
    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return false

    dim as HDC hRef = GetDC( hCheckBox )
    if hRef = 0 then return false
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hCheckBox, hRef )
        return false
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
    ReleaseDC( hCheckBox, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return false
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    dim as boolean bSavedFocus = pChk->isFocused
    pChk->isFocused = true

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hCheckBox, hMem, rcClient, hMem )

        dim as long nMood = PsCheckBox_ResolveMood( pChk->isEnabled, pChk->isPressed, pChk->isHot )
        dim as COLORREF clrFill, clrFore, clrBox
        PsCheckBox_ResolveColors( @pChk->colors, nMood, pChk->isChecked, clrFill, clrFore, clrBox )
        b.SetBackColor( clrFill )
        b.PaintClientRect()

        dim p as PSCHECKBOX_PAINTINFO
        PsCheckBox_FillPaintInfo( pChk, hCheckBox, @b, @p )

        if pChk->PaintCallback then
            pChk->PaintCallback( @p )
        else
            PsCheckBox_RenderDefault( pChk, @b, @p )
        end if

        b.EndDoubleBuffer()
    end scope

    pChk->isFocused = bSavedFocus

    ' Count distinct tones and hash the same pixels in one pass. The alpha byte is masked off:
    ' GDI text leaves it at 0 and GDI+ shapes leave it at 255, so an unmasked compare would
    ' report two tones for one colour -- and would make the hash depend on which renderer last
    ' touched a pixel rather than on what colour it is.
    dim tones(0 to 63) as ulong
    dim as long nSeen = 0
    dim as ulong h = &h811C9DC5          ' FNV-1a 32-bit offset basis
    const FNV_PRIME as ulong = &h01000193
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF

            dim as boolean bFound = false
            for t as long = 0 to nSeen - 1
                if tones(t) = c then bFound = true : exit for
            next
            if bFound = false then
                if nSeen < 64 then
                    tones(nSeen) = c
                    nSeen += 1
                end if
            end if

            h = (h xor (c and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 8) and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 16) and &hFF)) * FNV_PRIME
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )

    nTones = nSeen
    nHash  = h
    return true
end function


' See the header in PsCheckBox.bi. A part wiped by a PaintBorderRect fill is literally ONE tone,
' so the useful signal is the count LEAVING 1 -- not the count being large.
function PsCheckBox_CountRenderedTones( byval hCheckBox as HWND, byval nPart as long ) as long
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsCheckBox_ProbePart( hCheckBox, nPart, nTones, nHash ) = false then return 0
    return nTones
end function

' See the header in PsCheckBox.bi. Compare two hashes of the SAME part taken in two different
' states: equal means the state change never reached the surface. It can only prove difference,
' never correctness. (0 is also the failure return; an FNV-1a of a non-empty region landing
' exactly on 0 is possible in principle and has been accepted rather than worked around.)
function PsCheckBox_HashRenderedPart( byval hCheckBox as HWND, byval nPart as long ) as ulong
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsCheckBox_ProbePart( hCheckBox, nPart, nTones, nHash ) = false then return 0
    return nHash
end function


' ========================================================================================
' Self-test.  Gated on PSCHECKBOX_SELFTEST=1.
' ========================================================================================
dim shared as long gCHK_TestCount
dim shared as long gCHK_FailCount

' The notify counter. Module-level because CHK_CheckChangedCallbackSub is a plain sub with no
' user-data parameter -- the callback has nowhere else to put its answer.
dim shared as long     gCHK_NotifyCount
dim shared as boolean  gCHK_NotifyLast

private sub CHKTest_OnCheckChanged( byval hCheckBox as HWND, byval isChecked as boolean )
    gCHK_NotifyCount += 1
    gCHK_NotifyLast   = isChecked
end sub

private sub CHKTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gCHK_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gCHK_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function CHKTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function


sub PsCheckBox_RunSelfTest( byval hWndParent as HWND )
    if environ("PSCHECKBOX_SELFTEST") <> "1" then exit sub

    gCHK_TestCount = 0
    gCHK_FailCount = 0
    print "=== PsCheckBox self-test ==="

    dim as HWND h = PsCheckBox_Create( hWndParent, 9901 )
    if h = 0 then
        print "FAIL  create   [PsCheckBox_Create returned 0]"
        exit sub
    end if

    dim as PSCHECKBOX ptr pC = GetCheckBoxPointer(h)
    if pC = 0 then
        print "FAIL  pointer   [GetCheckBoxPointer returned 0]"
        DestroyWindow(h)
        exit sub
    end if

    dim as string sD                     ' every trace string is built here first: PRINT on a
                                         ' DWSTRING, or on a string & wstring concat, interleaves
                                         ' nulls (Learnings.md).
    dim as long w, hh
    dim as RECT rcCon, rcBox, rcTxt, rcVis

    ' Pin every layout input to a known value so the arithmetic below is exact rather than
    ' DPI-dependent. Only the CAPTION stays measured, and every assertion that involves it is
    ' written against pC->nTextWidth / nTextHeight rather than against a literal.
    PsCheckBox_SetPadding( h, 10, 5, 10, 5 )
    PsCheckBox_SetBoxGap( h, 8 )
    PsCheckBox_SetBoxSize( h, 16, 16 )
    PsCheckBox_SetFocusRing( h, 2, 1 )              ' ringPad = 3
    PsCheckBox_SetFocusCurvature( h, 8 )
    PsCheckBox_SetBoxAlign( h, CHK_BOXALIGN_LEFT )
    PsCheckBox_SetTextAlign( h, CHK_TEXTALIGN_LEFT )

    ' PLAIN LETTERS, NOT THE FLUENT CODEPOINTS -- and this is NECESSARY, not merely tidy. The
    ' final probe asserts that the two states render DIFFERENTLY; with the default E739/E73A and
    ' no Segoe Fluent Icons available to the test process, both would draw the identical
    ' missing-glyph box and that assertion would fail for a reason that has nothing to do with
    ' the control. Two ordinary letters render under the stock GUI font on any machine.
    const GLYPH_OFF = "o"
    const GLYPH_ON  = "X"
    const CAPTION   = "Test Caption"
    PsCheckBox_SetGlyphUnchecked( h, GLYPH_OFF )
    PsCheckBox_SetGlyphChecked( h, GLYPH_ON )

    dim as long ringPad = pC->RingPad()

    ' ---- 1. ideal size is valid BEFORE the control has ever been sized ------------------
    PsCheckBox_SetText( h, CAPTION )
    PsCheckBox_GetIdealSize( h, w, hh )
    sD = "w=" & str(w) & " h=" & str(hh)
    CHKTest_Check( "ideal size before first size", (w > 0) andalso (hh > 0), sD )

    ' ---- 2. the ideal size is built from the parts we think it is -----------------------
    dim as long expectW = (2 * ringPad) + 10 + 16 + 8 + pC->nTextWidth + 10
    sD = "ideal=" & str(w) & " expect=" & str(expectW) & " textW=" & str(pC->nTextWidth)
    CHKTest_Check( "ideal width = ring+padL+box+gap+text+padR", (w = expectW), sD )

    dim as long contentH = pC->nTextHeight
    if 16 > contentH then contentH = 16
    dim as long expectH = (2 * ringPad) + 5 + contentH + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectH) & " textH=" & str(pC->nTextHeight) & " boxH=16"
    CHKTest_Check( "ideal height = ring+padT+max(textH,boxH)+padB", (hh = expectH), sD )
    dim as long bothW = w
    dim as long bothH = hh

    ' ---- 3. THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION -------------------------------
    ' Direction 1: drop the caption and the gap must go with it, not just the text width.
    PsCheckBox_SetText( h, "" )
    PsCheckBox_GetIdealSize( h, w, hh )
    dim as long expectBoxOnly = (2 * ringPad) + 10 + 16 + 10
    sD = "ideal=" & str(w) & " expect=" & str(expectBoxOnly) & " (no gap charged)"
    CHKTest_Check( "no caption = no gap charged", (w = expectBoxOnly), sD )

    ' Direction 2: put it back and the gap returns with it.
    PsCheckBox_SetText( h, CAPTION )
    PsCheckBox_GetIdealSize( h, w, hh )
    sD = "restored=" & str(w) & " expect=" & str(bothW)
    CHKTest_Check( "caption restored = gap restored", (w = bothW), sD )

    ' ---- 4. contentH is a real max(), exercised in BOTH directions ----------------------
    ' With the default 16px box the text is usually the shorter of the two, so the max() above
    ' only ever proves one branch. Shrink the box below the font and the other branch has to
    ' carry the height -- otherwise "height comes from the box" would be indistinguishable from
    ' "height is always the box".
    PsCheckBox_SetBoxSize( h, 8, 8 )
    PsCheckBox_GetIdealSize( h, w, hh )
    dim as long expectTallText = (2 * ringPad) + 5 + pC->nTextHeight + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectTallText) & " textH=" & str(pC->nTextHeight) & " boxH=8"
    CHKTest_Check( "small box + caption: height comes from the TEXT", _
                   (pC->nTextHeight > 8) andalso (hh = expectTallText), sD )

    PsCheckBox_SetText( h, "" )
    PsCheckBox_GetIdealSize( h, w, hh )
    dim as long expectShortBox = (2 * ringPad) + 5 + 8 + 5
    sD = "ideal=" & str(hh) & " expect=" & str(expectShortBox) & " (no caption, boxH=8)"
    CHKTest_Check( "small box, no caption: height comes from the BOX", (hh = expectShortBox), sD )
    PsCheckBox_SetText( h, CAPTION )
    PsCheckBox_SetBoxSize( h, 16, 16 )

    ' ---- 5. FLIPPING THE BOX SIDE DOES NOT CHANGE THE IDEAL SIZE ------------------------
    ' The mirror is exact: the cell, the gap and both paddings are all still charged, they have
    ' only swapped ends. This is what lets a host flip alignment on a laid-out form.
    PsCheckBox_SetBoxAlign( h, CHK_BOXALIGN_RIGHT )
    dim as long w2, h2
    PsCheckBox_GetIdealSize( h, w2, h2 )
    sD = "left=" & str(bothW) & "x" & str(bothH) & " right=" & str(w2) & "x" & str(h2)
    CHKTest_Check( "box side does not change the ideal size", _
                   (w2 = bothW) andalso (h2 = bothH), sD )
    PsCheckBox_SetBoxAlign( h, CHK_BOXALIGN_LEFT )

    ' ---- 6. BOXALIGN_LEFT placement ----------------------------------------------------
    ' Size the control to ideal first; every rect query below needs a client area.
    SetWindowPos( h, 0, 0, 0, bothW, bothH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    sD = "content=" & CHKTest_RectStr( rcCon ) & " box=" & CHKTest_RectStr( rcBox )
    CHKTest_Check( "LEFT: box pinned at padLeft", (rcBox.left - rcCon.left) = 10, sD )
    sD = "box=" & CHKTest_RectStr( rcBox ) & " text=" & CHKTest_RectStr( rcTxt )
    CHKTest_Check( "LEFT: span starts one gap after the box", (rcTxt.left - rcBox.right) = 8, sD )
    sD = "content=" & CHKTest_RectStr( rcCon ) & " text=" & CHKTest_RectStr( rcTxt )
    CHKTest_Check( "LEFT: span ends at padRight", (rcCon.right - rcTxt.right) = 10, sD )
    sD = "box=" & str(rcBox.right - rcBox.left) & "x" & str(rcBox.bottom - rcBox.top)
    CHKTest_Check( "LEFT: box cell is the declared size", _
                   ((rcBox.right - rcBox.left) = 16) andalso ((rcBox.bottom - rcBox.top) = 16), sD )

    ' ---- 7. BOXALIGN_RIGHT is the exact mirror ------------------------------------------
    PsCheckBox_SetBoxAlign( h, CHK_BOXALIGN_RIGHT )
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    sD = "content=" & CHKTest_RectStr( rcCon ) & " box=" & CHKTest_RectStr( rcBox )
    CHKTest_Check( "RIGHT: box pinned at padRight", (rcCon.right - rcBox.right) = 10, sD )
    sD = "box=" & CHKTest_RectStr( rcBox ) & " text=" & CHKTest_RectStr( rcTxt )
    CHKTest_Check( "RIGHT: span ends one gap before the box", (rcBox.left - rcTxt.right) = 8, sD )
    sD = "content=" & CHKTest_RectStr( rcCon ) & " text=" & CHKTest_RectStr( rcTxt )
    CHKTest_Check( "RIGHT: span starts at padLeft", (rcTxt.left - rcCon.left) = 10, sD )
    sD = "box=" & str(rcBox.right - rcBox.left) & "x" & str(rcBox.bottom - rcBox.top)
    CHKTest_Check( "RIGHT: box cell is the declared size", _
                   ((rcBox.right - rcBox.left) = 16) andalso ((rcBox.bottom - rcBox.top) = 16), sD )

    ' ---- 8. a much longer caption never moves the box, in EITHER alignment ---------------
    ' The control is re-sized to the new ideal each time, which is what a host would do.
    PsCheckBox_SetText( h, "A Very Much Longer Caption Indeed" )
    PsCheckBox_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetBoxRect( h, rcBox )
    sD = "padR=" & str(rcCon.right - rcBox.right)
    CHKTest_Check( "RIGHT: caption length never moves the box", (rcCon.right - rcBox.right) = 10, sD )

    PsCheckBox_SetBoxAlign( h, CHK_BOXALIGN_LEFT )
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetBoxRect( h, rcBox )
    sD = "padL=" & str(rcBox.left - rcCon.left)
    CHKTest_Check( "LEFT: caption length never moves the box", (rcBox.left - rcCon.left) = 10, sD )

    PsCheckBox_SetText( h, CAPTION )
    PsCheckBox_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 9. everything is vertically centred in the content -----------------------------
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    dim as long conH = rcCon.bottom - rcCon.top
    sD = "boxTop=" & str(rcBox.top - rcCon.top) & " expect=" & str((conH - 16) \ 2)
    CHKTest_Check( "box v-centred in the content", (rcBox.top - rcCon.top) = ((conH - 16) \ 2), sD )
    sD = "textTop=" & str(rcTxt.top - rcCon.top) & " expect=" & str((conH - pC->nTextHeight) \ 2)
    CHKTest_Check( "caption v-centred in the content", _
                   (rcTxt.top - rcCon.top) = ((conH - pC->nTextHeight) \ 2), sD )

    ' ---- 10. an absent caption gives an EMPTY rect; the box never does -------------------
    PsCheckBox_SetText( h, "" )
    PsCheckBox_GetTextRect( h, rcTxt )
    PsCheckBox_GetBoxRect( h, rcBox )
    sD = "text=" & CHKTest_RectStr( rcTxt )
    CHKTest_Check( "no caption = empty text rect", IsRectEmpty( @rcTxt ) <> 0, sD )
    sD = "box=" & CHKTest_RectStr( rcBox )
    CHKTest_Check( "the box rect is never empty", IsRectEmpty( @rcBox ) = 0, sD )
    PsCheckBox_SetText( h, CAPTION )
    SetWindowPos( h, 0, 0, 0, bothW, bothH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 11. the focus-ring band is reserved, and is the whole difference ----------------
    PsCheckBox_GetContentRect( h, rcCon )
    PsCheckBox_GetVisualRect( h, rcVis )
    sD = "visual=" & CHKTest_RectStr( rcVis ) & " content=" & CHKTest_RectStr( rcCon ) & _
         " ringPad=" & str(ringPad)
    CHKTest_Check( "visual = content inflated by ringPad", _
                   (rcCon.left - rcVis.left = ringPad) andalso _
                   (rcVis.right - rcCon.right = ringPad) andalso _
                   (rcCon.top - rcVis.top = ringPad) andalso _
                   (rcVis.bottom - rcCon.bottom = ringPad), sD )

    ' THE NO-JUMP GUARANTEE: geometry with focus must be byte-identical to geometry without it.
    ' Asserted rather than assumed, because "the band is reserved unconditionally" is exactly the
    ' kind of claim an optimisation would quietly break.
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    dim as RECT rcBoxUnfocused = rcBox
    dim as RECT rcTxtUnfocused = rcTxt
    dim as boolean bSavedFocus = pC->isFocused
    pC->isFocused = true
    pC->bLayoutDirty = true
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    pC->isFocused = bSavedFocus
    sD = "unfocused=" & CHKTest_RectStr( rcBoxUnfocused ) & " focused=" & CHKTest_RectStr( rcBox )
    CHKTest_Check( "focus does not move anything", _
                   (EqualRect( @rcBox, @rcBoxUnfocused ) <> 0) andalso _
                   (EqualRect( @rcTxt, @rcTxtUnfocused ) <> 0), sD )

    ' ---- 12. overflow is honest: the span collapses, the box does not --------------------
    dim as long narrow = (2 * ringPad) + 10 + 16 + 8 + 10        ' exactly no room for a caption
    SetWindowPos( h, 0, 0, 0, narrow, bothH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsCheckBox_GetBoxRect( h, rcBox )
    PsCheckBox_GetTextRect( h, rcTxt )
    sD = "text=" & CHKTest_RectStr( rcTxt ) & " width=" & str(rcTxt.right - rcTxt.left)
    CHKTest_Check( "too narrow: span degenerates, never goes negative", _
                   (rcTxt.right >= rcTxt.left) andalso ((rcTxt.right - rcTxt.left) = 0), sD )
    sD = "box=" & str(rcBox.right - rcBox.left) & "x" & str(rcBox.bottom - rcBox.top)
    CHKTest_Check( "too narrow: the box keeps its declared size", _
                   ((rcBox.right - rcBox.left) = 16) andalso ((rcBox.bottom - rcBox.top) = 16), sD )

    ' ---- 13. the ideal size does NOT follow the client ------------------------------------
    ' The ellipsis is a paint-time concern only, so a host that sizes with GetIdealSize can never
    ' see one -- and that is only true if starving the control leaves the measurement alone.
    PsCheckBox_GetIdealSize( h, w2, h2 )
    sD = "ideal now=" & str(w2) & "x" & str(h2) & " expect=" & str(bothW) & "x" & str(bothH)
    CHKTest_Check( "ideal size unaffected by a starved client", _
                   (w2 = bothW) andalso (h2 = bothH), sD )
    SetWindowPos( h, 0, 0, 0, bothW, bothH, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    ' ---- 14. mood precedence, as a truth table -------------------------------------------
    ' disabled > pressed > hot > idle. All eight combinations, because the interesting ones are
    ' the contradictory states (disabled AND pressed) that only a race can produce.
    CHKTest_Check( "mood: enabled,idle", _
                   PsCheckBox_ResolveMood( true, false, false ) = CHK_MOOD_IDLE, "" )
    CHKTest_Check( "mood: enabled,hot", _
                   PsCheckBox_ResolveMood( true, false, true ) = CHK_MOOD_HOT, "" )
    CHKTest_Check( "mood: enabled,pressed", _
                   PsCheckBox_ResolveMood( true, true, false ) = CHK_MOOD_PRESSED, "" )
    CHKTest_Check( "mood: pressed beats hot", _
                   PsCheckBox_ResolveMood( true, true, true ) = CHK_MOOD_PRESSED, "" )
    CHKTest_Check( "mood: disabled,idle", _
                   PsCheckBox_ResolveMood( false, false, false ) = CHK_MOOD_DISABLED, "" )
    CHKTest_Check( "mood: disabled beats hot", _
                   PsCheckBox_ResolveMood( false, false, true ) = CHK_MOOD_DISABLED, "" )
    CHKTest_Check( "mood: disabled beats pressed", _
                   PsCheckBox_ResolveMood( false, true, false ) = CHK_MOOD_DISABLED, "" )
    CHKTest_Check( "mood: disabled beats both", _
                   PsCheckBox_ResolveMood( false, true, true ) = CHK_MOOD_DISABLED, "" )

    ' ---- 15. isChecked selects the box colour, and touches NOTHING else ------------------
    dim as PSCHECKBOX_COLORS c
    PsCheckBox_GetColors( h, @c )
    ' Make every field distinguishable, so an assertion cannot pass by two colours happening to
    ' be equal (which the four BackColors ARE by default -- the flat look).
    c.BackColor               = BGR(1,0,0)
    c.BackColorHot            = BGR(2,0,0)
    c.BackColorPressed        = BGR(3,0,0)
    c.BackColorDisabled       = BGR(4,0,0)
    c.ForeColor               = BGR(0,1,0)
    c.ForeColorHot            = BGR(0,2,0)
    c.ForeColorPressed        = BGR(0,3,0)
    c.ForeColorDisabled       = BGR(0,4,0)
    c.BoxColor                = BGR(0,0,1)
    c.BoxColorHot             = BGR(0,0,2)
    c.BoxColorPressed         = BGR(0,0,3)
    c.BoxColorDisabled        = BGR(0,0,4)
    c.BoxColorChecked         = BGR(0,0,11)
    c.BoxColorCheckedHot      = BGR(0,0,12)
    c.BoxColorCheckedPressed  = BGR(0,0,13)
    c.BoxColorCheckedDisabled = BGR(0,0,14)

    dim as long nMoods(0 to 3) = { CHK_MOOD_IDLE, CHK_MOOD_HOT, CHK_MOOD_PRESSED, CHK_MOOD_DISABLED }
    dim as COLORREF cBoxOff(0 to 3) = { BGR(0,0,1), BGR(0,0,2), BGR(0,0,3), BGR(0,0,4) }
    dim as COLORREF cBoxOn(0 to 3)  = { BGR(0,0,11), BGR(0,0,12), BGR(0,0,13), BGR(0,0,14) }

    for i as long = 0 to 3
        dim as COLORREF uBack, uFore, uBox
        PsCheckBox_ResolveColors( @c, nMoods(i), false, uBack, uFore, uBox )
        sD = "mood=" & str(nMoods(i)) & " box=" & hex(uBox) & " expect=" & hex(cBoxOff(i))
        CHKTest_Check( "unchecked box colour follows the mood", (uBox = cBoxOff(i)), sD )

        dim as COLORREF kBack, kFore, kBox
        PsCheckBox_ResolveColors( @c, nMoods(i), true, kBack, kFore, kBox )
        sD = "mood=" & str(nMoods(i)) & " box=" & hex(kBox) & " expect=" & hex(cBoxOn(i))
        CHKTest_Check( "checked box colour follows the mood", (kBox = cBoxOn(i)), sD )

        sD = "mood=" & str(nMoods(i)) & " back " & hex(uBack) & "/" & hex(kBack) & _
             " fore " & hex(uFore) & "/" & hex(kFore)
        CHKTest_Check( "isChecked touches ONLY the box colour", _
                       (kBack = uBack) andalso (kFore = uFore), sD )
    next

    ' Put the defaults back before the render probes, so they measure the shipping look.
    dim as PSCHECKBOX_COLORS cFresh
    PsCheckBox_SetColors( h, @cFresh )

    ' ---- 16. THE NOTIFY CONTRACT, MEASURED -----------------------------------------------
    ' "Setters are silent, actions notify" is the family's most-repeated rule and the easiest to
    ' break by accident, so it is counted rather than reasoned about.
    PsCheckBox_SetCheckChangedCallback( h, @CHKTest_OnCheckChanged )
    PsCheckBox_SetChecked( h, false )
    gCHK_NotifyCount = 0
    gCHK_NotifyLast  = false

    PsCheckBox_SetChecked( h, true )
    sD = "notifications=" & str(gCHK_NotifyCount) & " checked=" & str(PsCheckBox_GetChecked(h))
    CHKTest_Check( "SetChecked is SILENT", _
                   (gCHK_NotifyCount = 0) andalso (PsCheckBox_GetChecked(h) = true), sD )

    PsCheckBox_Toggle( h )
    sD = "notifications=" & str(gCHK_NotifyCount) & " callback saw=" & str(gCHK_NotifyLast) & _
         " getter says=" & str(PsCheckBox_GetChecked(h))
    CHKTest_Check( "Toggle FIRES exactly once", (gCHK_NotifyCount = 1), sD )
    CHKTest_Check( "Toggle flipped the state", (PsCheckBox_GetChecked(h) = false), sD )
    CHKTest_Check( "the callback was handed the NEW state", _
                   (gCHK_NotifyLast = PsCheckBox_GetChecked(h)), sD )

    ' A disabled control refuses the action, exactly as a real click would be refused.
    PsCheckBox_SetEnabled( h, false )
    PsCheckBox_Toggle( h )
    sD = "notifications=" & str(gCHK_NotifyCount) & " checked=" & str(PsCheckBox_GetChecked(h))
    CHKTest_Check( "Toggle is refused on a disabled control", _
                   (gCHK_NotifyCount = 1) andalso (PsCheckBox_GetChecked(h) = false), sD )
    PsCheckBox_SetEnabled( h, true )
    sD = "enabled=" & str(PsCheckBox_GetEnabled(h))
    CHKTest_Check( "SetEnabled round-trips through EnableWindow", _
                   (PsCheckBox_GetEnabled(h) = true) andalso (IsWindowEnabled(h) <> 0), sD )
    PsCheckBox_SetCheckChangedCallback( h, 0 )

    ' ---- 17. the window text really is the caption ---------------------------------------
    ' ASK THE CONTROL. Reading the field back would assert nothing about the message handlers,
    ' which are the whole point of this section -- a SendMessage whose return value is discarded
    ' is an assumption, not an implementation (the PsNumericUpDown lesson).
    dim as wstring * 64 wszSet = "Window Text Test"
    SendMessageW( h, WM_SETTEXT, 0, cast( LPARAM, @wszSet ) )

    dim as long nLen = cast( long, SendMessageW( h, WM_GETTEXTLENGTH, 0, 0 ) )
    sD = "WM_GETTEXTLENGTH=" & str(nLen) & " expect=" & str(len(wszSet))
    CHKTest_Check( "WM_GETTEXTLENGTH reports the caption length", (nLen = len(wszSet)), sD )

    dim as wstring * 64 wszGot
    dim as long nGot = cast( long, SendMessageW( h, WM_GETTEXT, 64, cast( LPARAM, @wszGot ) ) )
    sD = "returned=" & str(nGot) & " text=[" & str(wszGot) & "]"
    CHKTest_Check( "WM_GETTEXT round-trips the caption", _
                   (nGot = len(wszSet)) andalso (wszGot = wszSet), sD )

    ' Truncation: a short buffer must be filled to nMax-1 and terminated, not overrun.
    dim as wstring * 8 wszShort
    dim as long nShort = cast( long, SendMessageW( h, WM_GETTEXT, 5, cast( LPARAM, @wszShort ) ) )
    sD = "returned=" & str(nShort) & " text=[" & str(wszShort) & "] expect 4 chars"
    CHKTest_Check( "WM_GETTEXT honours the buffer size", _
                   (nShort = 4) andalso (len(wszShort) = 4) andalso (wszShort = "Wind"), sD )

    ' And GetWindowText -- the whole reason the two handlers exist -- must agree with the
    ' control's own getter.
    dim as wstring * 64 wszWin
    GetWindowTextW( h, @wszWin, 64 )
    dim as wstring * 64 wszApi
    wszApi = PsCheckBox_GetText( h )
    sD = "GetWindowText=[" & str(wszWin) & "] PsCheckBox_GetText=[" & str(wszApi) & "]"
    CHKTest_Check( "GetWindowText agrees with PsCheckBox_GetText", (wszWin = wszApi), sD )

    ' SetWindowText must go the other way and re-measure.
    dim as long wBefore = 0, hBefore = 0
    PsCheckBox_GetIdealSize( h, wBefore, hBefore )
    SetWindowTextW( h, "Substantially Longer Window Text" )
    PsCheckBox_GetIdealSize( h, w2, h2 )
    sD = "before=" & str(wBefore) & " after=" & str(w2)
    CHKTest_Check( "SetWindowText re-measures the control", (w2 > wBefore), sD )
    PsCheckBox_SetText( h, CAPTION )

    ' ---- 18. the control is a tab ITEM, not a tab CONTAINER ------------------------------
    ' CWindow.Create defaults dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control
    ' that passes only dwStyle gets the container flag by accident -- and the dialog manager then
    ' descends into it, finds no children, and abandons the Tab walk for the WHOLE FORM. That
    ' shipped in PsButton, PsToggle and PsComboBox; it is asserted here rather than remembered.
    dim as ulong exStyle = cast( ulong, GetWindowLongPtr( h, GWL_EXSTYLE ) )
    sD = "exstyle=&h" & hex(exStyle)
    CHKTest_Check( "NOT a control parent (or Tab dies form-wide)", _
                   (exStyle and WS_EX_CONTROLPARENT) = 0, sD )

    dim as ulong wsStyle = cast( ulong, GetWindowLongPtr( h, GWL_STYLE ) )
    sD = "style=&h" & hex(wsStyle)
    CHKTest_Check( "is a tabstop", (wsStyle and WS_TABSTOP) <> 0, sD )

    ' The end-to-end claim, not just the flags: ask the dialog manager to find us. It walks the
    ' parent's children itself, so this fails if either style above is wrong.
    '
    ' The WS_VISIBLE bit is set DIRECTLY rather than with ShowWindow: the walk tests the style
    ' bit, and doing it this way means no paint, no flash, and no dependence on whether the host
    ' happens to call the self-test before or after it shows the form.
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
    CHKTest_Check( "the dialog manager can reach us by Tab", bReachable, sD )

    ' ---- 19. the render probes ------------------------------------------------------------
    ' (a) The control is not a solid block. A part wiped by a PaintBorderRect fill is literally
    '     ONE tone, so the useful signal is the count LEAVING 1 -- not the count being large.
    PsCheckBox_GetIdealSize( h, w, hh )
    SetWindowPos( h, 0, 0, 0, w, hh, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

    dim as long tContent = PsCheckBox_CountRenderedTones( h, CHK_PART_CONTROL )
    sD = "distinct tones in the content=" & str(tContent)
    CHKTest_Check( "built-in painter does not flood the control", (tContent > 1), sD )

    dim as long tBox = PsCheckBox_CountRenderedTones( h, CHK_PART_BOX )
    sD = "distinct tones in the box cell=" & str(tBox)
    CHKTest_Check( "the box glyph actually reached the surface", (tBox > 1), sD )

    dim as long tText = PsCheckBox_CountRenderedTones( h, CHK_PART_TEXT )
    sD = "distinct tones in the caption span=" & str(tText)
    CHKTest_Check( "the caption actually reached the surface", (tText > 1), sD )

    ' (b) CHECKING THE BOX CHANGES WHAT IS DRAWN. Every other assertion in this file can pass
    '     while both states render the identical glyph: the flag flips, the layout is right, the
    '     colours resolve correctly, and the pixels never move. This one asks the pixels.
    PsCheckBox_SetChecked( h, false )
    dim as ulong hashOff = PsCheckBox_HashRenderedPart( h, CHK_PART_BOX )
    PsCheckBox_SetChecked( h, true )
    dim as ulong hashOn  = PsCheckBox_HashRenderedPart( h, CHK_PART_BOX )
    sD = "unchecked=&h" & hex(hashOff) & " checked=&h" & hex(hashOn)
    CHKTest_Check( "the checked state reaches the pixels", _
                   (hashOff <> hashOn) andalso (hashOff <> 0) andalso (hashOn <> 0), sD )

    ' ...and it must be the STATE doing it, not the probe being noisy: the same state twice must
    ' hash identically, or the assertion above proves nothing.
    dim as ulong hashOn2 = PsCheckBox_HashRenderedPart( h, CHK_PART_BOX )
    sD = "first=&h" & hex(hashOn) & " second=&h" & hex(hashOn2)
    CHKTest_Check( "the probe is deterministic for one state", (hashOn = hashOn2), sD )
    PsCheckBox_SetChecked( h, false )

    ' (c) AND IT MUST BE THE GLYPH DOING IT, NOT MERELY THE COLOUR.
    '
    ' The assertion above was A/B'd by freezing PSCHECKBOX.ResolveGlyph to always return the
    ' unchecked glyph -- and IT STILL PASSED, because isChecked also switches the box COLOUR, and
    ' that alone moves the pixels. So it proves "the state reaches the surface", which is true and
    ' worth having, but NOT the narrower claim its name suggests.
    '
    ' Isolating it costs four lines: flatten all eight box colours to one value, so the colour
    ' can no longer account for any difference, and the ONLY thing left that can move a pixel is
    ' the glyph swap. (This is PsMessageBox's lesson -- an assertion that is not isolated measures
    ' the wrong quantity while looking entirely reasonable.)
    scope
        dim as PSCHECKBOX_COLORS cIso
        PsCheckBox_GetColors( h, @cIso )
        dim as COLORREF flat = BGR(210,210,210)
        cIso.BoxColor                = flat
        cIso.BoxColorHot             = flat
        cIso.BoxColorPressed         = flat
        cIso.BoxColorDisabled        = flat
        cIso.BoxColorChecked         = flat
        cIso.BoxColorCheckedHot      = flat
        cIso.BoxColorCheckedPressed  = flat
        cIso.BoxColorCheckedDisabled = flat
        PsCheckBox_SetColors( h, @cIso )

        PsCheckBox_SetChecked( h, false )
        dim as ulong glyphOff = PsCheckBox_HashRenderedPart( h, CHK_PART_BOX )
        PsCheckBox_SetChecked( h, true )
        dim as ulong glyphOn  = PsCheckBox_HashRenderedPart( h, CHK_PART_BOX )
        PsCheckBox_SetChecked( h, false )

        sD = "one box colour for both states: unchecked=&h" & hex(glyphOff) & _
             " checked=&h" & hex(glyphOn)
        CHKTest_Check( "the GLYPH swap alone reaches the pixels", _
                       (glyphOff <> glyphOn) andalso (glyphOff <> 0) andalso (glyphOn <> 0), sD )

        PsCheckBox_SetColors( h, @cFresh )
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
        dim as HWND hBeep = PsCheckBox_Create( hWndParent, 7802 )
        PsCheckBox_SetEnabled( hBeep, false )

        dim as MSG mBeep
        mBeep.hwnd = hBeep : mBeep.message = WM_CHAR : mBeep.wParam = 13

        SendMessageW( hBeep, WM_KEYDOWN, VK_RETURN, 0 )
        dim as LRESULT dc1 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        CHKTest_Check( "Enter arms the WM_CHAR swallow that stops the system beep", _
            (dc1 and DLGC_WANTALLKEYS) <> 0, "dlgcode=&h" & hex(dc1) )

        SendMessageW( hBeep, WM_CHAR, 13, 0 )
        dim as LRESULT dc2 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        ' The disarm is the proof the WM_CHAR was consumed. The return value cannot be:
        ' DefWindowProc also returns 0 for WM_CHAR, so it reads the same when it fell through.
        CHKTest_Check( "...and the WM_CHAR is then swallowed instead of reaching DefWindowProc", _
            (dc2 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc2) )

        ' A key we never claimed must leave ordinary characters alone, or a host accelerator
        ' on a printable key would stop working.
        SendMessageW( hBeep, WM_KEYDOWN, VK_TAB, 0 )
        mBeep.wParam = asc("a")
        dim as LRESULT dc3 = SendMessageW( hBeep, WM_GETDLGCODE, asc("a"), cast(LPARAM, @mBeep) )
        CHKTest_Check( "an unclaimed key leaves ordinary characters alone", _
            (dc3 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc3) )

        DestroyWindow( hBeep )
    end scope


    ' ====================================================================================
    ' TOOLTIP BACKEND -- mode, handles, the delays surviving a switch, and the text wiring
    '
    ' A real dwell is what would exercise this end to end and no self-test can produce one,
    ' so what is asserted is the WIRING. The text assertion in particular resolves through
    ' the callback the tip actually holds rather than by calling our own thunk: on the pilot
    ' the direct-call version stayed green with the wiring deleted, which would have shipped
    ' a control that shows no tooltip at all once switched.
    ' ====================================================================================
    scope
        dim as HWND hTipT = PsCheckBox_Create( hWndParent, 9807 )
        if hTipT = 0 then
            CHKTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            CHKTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsCheckBox_GetTooltipMode( hTipT ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsCheckBox_GetTooltipMode( hTipT )) )
            CHKTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsCheckBox_GetTooltipHandle( hTipT ) <> 0) andalso _
                (IsWindow(PsCheckBox_GetTooltipHandle( hTipT )) <> 0) andalso _
                (PsCheckBox_GetPsTooltipHandle( hTipT ) = 0), _
                "hSys = " & str(cast(integer, PsCheckBox_GetTooltipHandle( hTipT ))) )

            PsCheckBox_SetHoverTime( hTipT, 777 )
            CHKTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeT = PsCheckBox_GetTooltipHandle( hTipT )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedT = PsCheckBox_SetTooltipMode( hTipT, PSTIP_MODE_PS )
            CHKTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedT, _
                "mode = " & str(PsCheckBox_GetTooltipMode( hTipT )) )
            CHKTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeT) = 0) andalso (PsCheckBox_GetTooltipHandle( hTipT ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeT)) )

            dim as HWND hPsT = PsCheckBox_GetPsTooltipHandle( hTipT )
            CHKTest_Check( "tooltip: a PsTooltip window exists and is attached to the control", _
                (hPsT <> 0) andalso (IsWindow(hPsT) <> 0) andalso _
                (PsTooltip_GetAttached( hPsT ) = hTipT), _
                "hPs = " & str(cast(integer, hPsT)) )

            dim as PSTOOLTIP ptr pTipT = GetTooltipPointer( hPsT )
            CHKTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipT <> 0) andalso (pTipT->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipT, pTipT->nInitialDelay, -1)) & " ms" )
            CHKTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipT <> 0) andalso (pTipT->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipT andalso pTipT->TooltipCallback, 1, 0)) )

            PsCheckBox_SetTooltipMode( hTipT, PSTIP_MODE_SYSTEM )
            CHKTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsCheckBox_GetTooltipHandle( hTipT ) <> 0) andalso _
                (PsCheckBox_GetPsTooltipHandle( hTipT ) = 0) andalso _
                (SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            CHKTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsCheckBox_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipT )
        end if
    end scope

    dim as string sSummary = "=== PsCheckBox: " & str(gCHK_TestCount - gCHK_FailCount) & _
                             "/" & str(gCHK_TestCount) & " assertions passed"
    if gCHK_FailCount > 0 then sSummary &= "  (" & str(gCHK_FailCount) & " FAILED)"
    print sSummary
end sub
