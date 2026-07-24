''
''  CMessageBox.inc  --  owner-drawn modal message box
''

#include once "CMessageBox.bi"


' Forward declarations. The button plumbing is referenced by AddButton, which is defined well
' above the modal machinery that owns it.
declare sub      CMessageBox_OnButtonClick( byval hButton as HWND, byval id as long )
declare sub      CMessageBox_PositionButtons( byval pBox as CMESSAGEBOX ptr )
declare sub      CMessageBox_Dismiss( byval pBox as CMESSAGEBOX ptr, byval nResult as long )


function GetMessageBoxPointer( byval hMsgBox as HWND ) as CMESSAGEBOX ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hMsgBox)
    if pWindow = 0 then return 0
    return cast( CMESSAGEBOX ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not.
' WM_LBUTTONUP is one (the box holds capture across a press on the X and the up-message is what
' releases it); WM_CLOSE and WM_DESTROY are the other two, because a modal box that cannot be
' dismissed is a hang, not a refused action.
' ========================================================================================
private function CMessageBox_CallMessage( _
            byval pBox   as CMESSAGEBOX ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pBox = 0 then return false
    if pBox->MessageCallback = 0 then return false

    dim as MBX_MESSAGEINFO m
    m.hMsgBox = hwnd
    m.uMsg    = uMsg
    m.wParam  = wParam
    m.lParam  = lParam
    return pBox->MessageCallback( @m )
end function


' ========================================================================================
' Resolve the icon actually drawn: the host's override if it set one, else whatever the kind
' maps to. Kept out of CMessageBox_ResolveGlyph / CMessageBox_ResolveIconColor so those stay
' pure and their mapping stays assertable on its own.
' ========================================================================================
private function CMessageBox_EffectiveGlyph( byval pBox as CMESSAGEBOX ptr ) as DWSTRING
    if pBox = 0 then return ""
    if pBox->bGlyphOverride then
        dim as DWSTRING wszOut = pBox->wszGlyph
        return wszOut
    end if
    return CMessageBox_ResolveGlyph( pBox->nIconKind )
end function

private function CMessageBox_EffectiveIconColor( byval pBox as CMESSAGEBOX ptr ) as COLORREF
    if pBox = 0 then return 0
    if pBox->bIconOverride then return pBox->clrIconOverride
    return CMessageBox_ResolveIconColor( pBox->nIconKind, @pBox->colors )
end function


' ========================================================================================
' Draw the box: caption band, close X, footer band and its divider, icon, message, border.
'
' Nothing in here touches a GDI object or a device context -- CBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' THE BUTTONS ARE NOT DRAWN HERE. They are real child windows with their own WM_PAINT.
'
' PRESENCE IS TESTED ON THE RECTS, not on the strings. LayoutBox leaves an absent part's rect
' EMPTY, so IsRectEmpty is the single source of truth about what exists -- and a degenerate
' caption span (the box too narrow to leave any room beside the X) drops out of the same test
' rather than needing its own.
'
' THE BORDER IS DRAWN LAST, over everything, so a body or footer fill can never overpaint it.
' ========================================================================================
private sub CMessageBox_RenderDefault( _
            byval pBox as CMESSAGEBOX ptr, _
            byval b    as CBufferPaint ptr, _
            byval p    as MBX_PAINTINFO ptr _
            )

    if (pBox = 0) orelse (b = 0) orelse (p = 0) then exit sub

    ' The fonts. Paint with the SAME font the measuring pass used -- the caller's if set, else
    ' the stock GUI font. CBufferPaint.SetFont IGNORES a null handle, so leaving it unset would
    ' silently paint in the DC's font while the wrap was measured in another, and the message
    ' would clip for no visible reason.
    dim as HFONT hTextUse = pBox->hTextFont
    if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    ' Both of these fall back to the MESSAGE font rather than to the stock font: a host that set
    ' one font and not the others almost certainly meant that one.
    dim as HFONT hCapUse = pBox->hCaptionFont
    if hCapUse = 0 then hCapUse = hTextUse
    dim as HFONT hGlyphUse = pBox->hGlyphFont
    if hGlyphUse = 0 then hGlyphUse = hTextUse
    ' The X gets its OWN font, falling back to the icon's. See CMESSAGEBOX.hCloseFont for why
    ' one glyph font cannot serve a 32px icon and a 10px caption button.
    dim as HFONT hCloseUse = pBox->hCloseFont
    if hCloseUse = 0 then hCloseUse = hGlyphUse

    ' ---- the caption band ------------------------------------------------------------------
    if IsRectEmpty( @p->rcCaption ) = 0 then
        dim as RECT rcCap = p->rcCaption
        b->SetBackColor( pBox->colors.CaptionBackColor )
        b->PaintRect( @rcCap )
    end if

    ' ---- the close button ------------------------------------------------------------------
    ' Three moods and no disabled one: the X on a modal box is always live. Its idle background
    ' defaults EQUAL to the caption's, so it is invisible until the cursor reaches it -- the
    ' same borderless-by-default trick CButton and CComboBox use.
    dim as COLORREF clrCloseBk = pBox->colors.CloseBackColor
    dim as COLORREF clrCloseFg = pBox->colors.CloseForeColor
    if p->isClosePressed then
        clrCloseBk = pBox->colors.CloseBackColorPressed
        clrCloseFg = pBox->colors.CloseForeColorPressed
    elseif p->isCloseHot then
        clrCloseBk = pBox->colors.CloseBackColorHot
        clrCloseFg = pBox->colors.CloseForeColorHot
    end if

    if IsRectEmpty( @p->rcClose ) = 0 then
        dim as RECT rcX = p->rcClose
        b->SetBackColor( clrCloseBk )
        b->PaintRect( @rcX )
        b->SetFont( hCloseUse )
        b->SetColors( clrCloseFg, clrCloseBk )
        b->PaintText( wchr( CMESSAGEBOX_GLYPH_CLOSE ), @rcX, DT_CENTER )
    end if

    ' ---- the title -------------------------------------------------------------------------
    ' DT_END_ELLIPSIS rather than a clip: rcCaptionText already stops short of the X, so a long
    ' title ellipsizes into its own span instead of sliding under the close button.
    if (IsRectEmpty( @p->rcCaptionText ) = 0) andalso (len( p->wszCaption ) > 0) then
        dim as RECT rcCT = p->rcCaptionText
        b->SetFont( hCapUse )
        b->SetColors( pBox->colors.CaptionForeColor, pBox->colors.CaptionBackColor )
        b->PaintText( p->wszCaption, @rcCT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    ' ---- the footer band and its divider ---------------------------------------------------
    if IsRectEmpty( @p->rcFooter ) = 0 then
        dim as RECT rcF = p->rcFooter
        b->SetBackColor( pBox->colors.FooterBackColor )
        b->PaintRect( @rcF )

        ' A HAIRLINE, drawn with PaintLine. NOT PaintBorderRect -- that fills before it strokes
        ' and would erase the footer it was meant to sit on top of. Thickness is deliberately
        ' not DPI-scaled: a hairline stays a hairline.
        if pBox->nDividerThick > 0 then
            b->SetPenColor( pBox->colors.DividerColor )
            b->PaintLine( pBox->nDividerThick, rcF.left, rcF.top, rcF.right, rcF.top )
        end if
    end if

    ' ---- the icon --------------------------------------------------------------------------
    ' The cell is a LAYOUT rect, not a measurement of the glyph: the control never measures the
    ' icon font, so a glyph too large for the cell clips rather than resizing the box
    ' (CIconPanel's declared-cell rule).
    if (IsRectEmpty( @p->rcIcon ) = 0) andalso (len( p->wszGlyph ) > 0) then
        dim as RECT rcI = p->rcIcon
        b->SetFont( hGlyphUse )
        b->SetColors( p->clrIcon, pBox->colors.BackColor )
        b->PaintText( p->wszGlyph, @rcI, DT_CENTER )
    end if

    ' ---- the message -----------------------------------------------------------------------
    ' PaintTextEx, NOT PaintText: PaintText forces DT_VCENTER or DT_SINGLELINE, which cannot
    ' coexist with DT_WORDBREAK -- it would draw one clipped line inside a tall box. The flags
    ' here MUST match the ones LayoutBox measured with, or the height lies.
    '
    ' DT_TOP, and no vertical centring: rcText's height IS the measured height of the wrapped
    ' text, and the rect itself was already v-centred against the icon by LayoutBox.
    if (IsRectEmpty( @p->rcText ) = 0) andalso (len( p->wszText ) > 0) then
        dim as RECT rcT = p->rcText
        b->SetFont( hTextUse )
        b->SetColors( pBox->colors.ForeColor, pBox->colors.BackColor )
        b->PaintTextEx( p->wszText, @rcT, DT_LEFT or DT_TOP or DT_WORDBREAK or DT_EXPANDTABS )
    end if

    ' ---- the border ------------------------------------------------------------------------
    ' Outline, never filled, and LAST so nothing can paint over it. PaintRoundOutline with a
    ' curvature of 0 is a plain rectangle; CBufferPaint owns the deflate-by-half-pen correction,
    ' so the stroke lands inside the client rather than half off the edge.
    if pBox->nBorderThick > 0 then
        dim as RECT rcB = p->rcClient
        b->SetPenColor( pBox->colors.BorderColor )
        b->PaintRoundOutline( @rcB, 0, pBox->nBorderThick )
    end if
end sub


' ========================================================================================
' Fill a PAINTINFO from the box's current state and geometry. One place, so the WM_PAINT path
' and the offscreen tone probe cannot describe the box differently -- a probe that hand-built
' this struct and left one field zeroed silently skipped the thing it was measuring the first
' time this pattern was written for CButton (Learnings.md).
' ========================================================================================
private sub CMessageBox_FillPaintInfo( _
            byval pBox as CMESSAGEBOX ptr, _
            byval hwnd as HWND, _
            byval b    as CBufferPaint ptr, _
            byval p    as MBX_PAINTINFO ptr _
            )

    if (pBox = 0) orelse (p = 0) then exit sub

    p->hMsgBox = hwnd
    p->b       = b
    GetClientRect( hwnd, @p->rcClient )
    p->rcCaption      = pBox->rcCaption
    p->rcCaptionText  = pBox->rcCaptionText
    p->rcClose        = pBox->rcClose
    p->rcBody         = pBox->rcBody
    p->rcIcon         = pBox->rcIcon
    p->rcText         = pBox->rcText
    p->rcFooter       = pBox->rcFooter
    p->isCloseHot     = pBox->isCloseHot
    p->isClosePressed = pBox->isClosePressed
    p->isActive       = (GetActiveWindow() = hwnd)
    p->wszCaption     = pBox->wszCaption
    p->wszText        = pBox->wszText
    p->wszGlyph       = CMessageBox_EffectiveGlyph( pBox )
    p->clrIcon        = CMessageBox_EffectiveIconColor( pBox )
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function CMessageBox_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hwnd)
    if pBox = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pBox->bLayoutDirty then pBox->LayoutBox()

    dim as CBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' The body colour, not a fixed one, so a paint callback that draws only a foreground still
    ' sits on the right background. The caption and footer bands paint over it.
    b.SetBackColor( pBox->colors.BackColor )
    b.PaintClientRect()

    dim p as MBX_PAINTINFO
    CMessageBox_FillPaintInfo( pBox, hwnd, @b, @p )

    if pBox->PaintCallback then
        pBox->PaintCallback( @p )
    else
        CMessageBox_RenderDefault( pBox, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the close button's hover state and stop the hover poll timer. Repaints only if the
' state actually changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub CMessageBox_ClearCloseHot( byval pBox as CMESSAGEBOX ptr, byval hwnd as HWND )
    if pBox = 0 then exit sub
    if pBox->hotTimerOn then
        KillTimer( hwnd, IDT_CMESSAGEBOX_HOTTRACK )
        pBox->hotTimerOn = false
    end if
    if pBox->isCloseHot then
        pBox->isCloseHot = false
        InvalidateRect( hwnd, @pBox->rcClose, FALSE )
    end if
end sub


' ========================================================================================
' End the modal loop with a result.
'
' THE ONE DISMISSAL PATH. A button click, Esc, the X, Alt+F4 and a host's WM_CLOSE all come
' through here, so no route can end the loop without also setting a result -- which is exactly
' the bug that would show up as DoModal returning 0 for a real click.
'
' It does NOT destroy the window: DoModal owns that, and destroying here would free the state
' out from under the loop that is about to read nResult.
' ========================================================================================
sub CMessageBox_Dismiss( byval pBox as CMESSAGEBOX ptr, byval nResult as long )
    if pBox = 0 then exit sub
    pBox->nResult  = nResult
    pBox->bRunning = false
end sub


' ========================================================================================
' A footer button was clicked. The button reports its HOST id (set by AddButton), which is
' exactly what DoModal returns -- so the box never has to map one id space onto another.
'
' The owning box is found through GetParent rather than a stored back-pointer: the button IS a
' child window of the box, so the relationship is already recorded by the window manager and a
' second copy of it could only ever go stale.
' ========================================================================================
sub CMessageBox_OnButtonClick( byval hButton as HWND, byval id as long )
    dim as HWND hBox = GetParent( hButton )
    if hBox = 0 then exit sub
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer( hBox )
    if pBox = 0 then exit sub
    CMessageBox_Dismiss( pBox, id )
end sub


' ========================================================================================
' Apply the computed button rects to the real child windows, and show them.
'
' SEPARATE FROM LayoutBox ON PURPOSE. LayoutBox runs from WM_PAINT, and moving child windows
' from inside a paint invites a recursive invalidation storm; this runs from WM_SIZE and from
' DoModal, where a SetWindowPos is exactly what is expected.
' ========================================================================================
sub CMessageBox_PositionButtons( byval pBox as CMESSAGEBOX ptr )
    if pBox = 0 then exit sub
    if pBox->bLayoutDirty then pBox->LayoutBox()
    if pBox->bLayoutDirty then exit sub          ' still no client area; nothing to place onto

    for i as long = 0 to pBox->nButtonCount - 1
        dim as RECT rc = pBox->rcButtons(i)
        if IsRectEmpty( @rc ) then continue for
        SetWindowPos( pBox->hButtons(i), 0, rc.left, rc.top, _
                      rc.right - rc.left, rc.bottom - rc.top, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
        ShowWindow( pBox->hButtons(i), SW_SHOW )
    next
end sub


' ========================================================================================
' CMessageBox Window procedure
'
' MOUSE CAPTURE IS TAKEN, but only for the close button. The family's test is "take capture only
' if something consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and the X has
' exactly CButton's press/cancel gesture: press it, slide off, release, nothing happens. So the
' full price is paid -- snapshot the press before releasing, a WM_CAPTURECHANGED handler that
' cancels it, a release in WM_DESTROY, and a callback result that is ignored for the up-message.
'
' The FOOTER buttons take their own capture; nothing here knows or cares.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET. The only double-clickable thing on the box is the X, and
' a rapid second click there is a legitimate second click on a window that may already be gone.
'
' THE BOX ITSELF IS NOT FOCUSABLE. It has no WS_TABSTOP and never calls SetFocus on itself: the
' tabstops are the child buttons, which is why this window carries WS_EX_CONTROLPARENT.
' ========================================================================================
function CMessageBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return CMessageBox_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pBox then
            ' Re-layout EAGERLY rather than waiting for the next paint, because the child
            ' buttons have to be moved and that cannot happen from inside WM_PAINT.
            pBox->bLayoutDirty = true
            pBox->LayoutBox()
            CMessageBox_PositionButtons( pBox )
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_ACTIVATE
        ' Tracked only so a host painter can render an inactive caption; the built-in painter
        ' does not, deliberately -- a modal box is the only thing the user can interact with, so
        ' greying its title says nothing useful.
        if pBox then InvalidateRect( hwnd, @pBox->rcCaption, FALSE )

    case WM_NCHITTEST
        ' HTCAPTION over the caption band is the WHOLE drag implementation: DefWindowProc does
        ' the move for us. The close button must be excluded or the X becomes both undraggable
        ' and unclickable, which is the natural mistake here.
        if pBox = 0 then exit select
        dim as LRESULT ht = DefWindowProc( hwnd, uMsg, wParam, lParam )
        if ht <> HTCLIENT then return ht
        if pBox->bLayoutDirty then pBox->LayoutBox()

        dim as POINT pt
        pt.x = cast( short, loword(lParam) )        ' SCREEN coords for this message
        pt.y = cast( short, hiword(lParam) )
        ScreenToClient( hwnd, @pt )

        if PtInRect( @pBox->rcClose, pt ) then return HTCLIENT
        if PtInRect( @pBox->rcCaption, pt ) then return HTCAPTION
        return HTCLIENT

    case WM_TIMER
        if (pBox <> 0) andalso (wParam = IDT_CMESSAGEBOX_HOTTRACK) then
            ' The cursor may have left the X without us getting a WM_MOUSELEAVE. NOT while a
            ' press is live: during capture the cursor is legitimately allowed to wander off and
            ' come back, and WM_MOUSEMOVE is already maintaining isCloseHot for exactly that.
            if pBox->isClosePressed = false then
                if isMouseOverRECT( hwnd, pBox->rcClose ) = false then
                    CMessageBox_ClearCloseHot( pBox, hwnd )
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pBox = 0 then exit select
        if pBox->bLayoutDirty then pBox->LayoutBox()

        dim as boolean bOver = isMouseOverRECT( hwnd, pBox->rcClose )

        if bOver then
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
            ' quickly, which would strand the X painted red.
            if pBox->hotTimerOn = false then
                SetTimer( hwnd, IDT_CMESSAGEBOX_HOTTRACK, CMESSAGEBOX_HOTTRACK_MS, NULL )
                pBox->hotTimerOn = true
            end if
        end if

        ' This also drives the press-cancel FEEDBACK: with capture held, moves outside the X
        ' still reach us, so sliding off a pressed X un-flashes it before the release decides
        ' that nothing happened.
        if bOver <> pBox->isCloseHot then
            pBox->isCloseHot = bOver
            InvalidateRect( hwnd, @pBox->rcClose, FALSE )
        end if

        if CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pBox then
            if pBox->isClosePressed = false then CMessageBox_ClearCloseHot( pBox, hwnd )
            if CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pBox = 0 then return 0
        if pBox->bLayoutDirty then pBox->LayoutBox()

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam ) then return 0

        if isMouseOverRECT( hwnd, pBox->rcClose ) then
            SetCapture( hwnd )
            pBox->isClosePressed = true
            pBox->isCloseHot     = true
            InvalidateRect( hwnd, @pBox->rcClose, FALSE )
        end if
        return 0

    case WM_LBUTTONUP
        if pBox = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window losing
        ' capture SYNCHRONOUSLY, and our handler for it cancels the press -- so reading
        ' isClosePressed after the release would always find FALSE and the X would never close.
        dim as boolean bWasPressed = pBox->isClosePressed
        ' Release capture unconditionally, before any callback runs. Nothing below gets the
        ' chance to leave it held.
        if GetCapture() = hwnd then ReleaseCapture()

        dim as boolean bInside = isMouseOverRECT( hwnd, pBox->rcClose )

        pBox->CancelClosePress()
        InvalidateRect( hwnd, @pBox->rcClose, FALSE )

        ' The result is deliberately IGNORED here (see MBX_MessageCallbackFunc).
        CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release still on the X closes the box. Press, slide off, release
        ' = nothing happened, which is the whole reason for capture.
        if bWasPressed andalso bInside then
            PostMessage( hwnd, WM_CLOSE, 0, 0 )
        end if
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time. The gesture is over; forget it and repaint.
        if pBox then
            pBox->CancelClosePress()
            InvalidateRect( hwnd, @pBox->rcClose, FALSE )
            return 0
        end if

    case WM_CLOSE
        ' Esc, the X, Alt+F4 and any host WM_CLOSE all land here, and all resolve to the same
        ' answer. The callback's result is IGNORED (see MBX_MessageCallbackFunc): a modal box
        ' that cannot be dismissed leaves the nested loop spinning with the parent disabled,
        ' which is an unrecoverable hang rather than a refused action. A host that does not want
        ' the box dismissable this way should not offer a cancel path.
        if pBox then
            CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam )
            CMessageBox_Dismiss( pBox, pBox->ResolveCancelID() )
        end if
        return 0

    case WM_DESTROY
        if pBox then
            CMessageBox_CallMessage( pBox, hwnd, uMsg, wParam, lParam )
            ' Don't let the hot-tracking timer outlive the window...
            if pBox->hotTimerOn then
                KillTimer( hwnd, IDT_CMESSAGEBOX_HOTTRACK )
                pBox->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pBox->CancelClosePress()
            ' Stop the loop no matter how the window died -- a parent being destroyed takes its
            ' owned windows with it, and the nested pump must not outlive its own window.
            pBox->bRunning = false
        end if

    case WM_NCDESTROY
        ' Delete the stored box data. The child buttons are ordinary child windows and are
        ' destroyed by the system; each one frees its own state in its own WM_NCDESTROY. The
        ' three fonts are the host's and are left alone.
        if pBox then delete(pBox)

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' CMessageBox_Create
' ========================================================================================
function CMessageBox_Create( byval hWndParent as HWND ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_POPUP with NO system caption: the caption band, its title and its X are all drawn by
    ' this control, which is the only way the host's colours and font reach them.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND WS_EX_CONTROLPARENT IS CORRECT HERE -- the
    ' exact OPPOSITE of the CButton fix. CWindow.Create defaults dwExStyle to
    ' WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE; a LEAF control that inherits that silently
    ' declares itself a container and becomes unreachable by Tab. This window genuinely IS a
    ' container -- its tabstops are the child buttons -- so the dialog manager MUST descend into
    ' it. WS_EX_WINDOWEDGE is dropped: it is a raised-border hint that means nothing to a window
    ' drawing its own chrome.
    '
    ' The test is "does this window have children the dialog manager should find?", not "is this
    ' a control?". See the per-sibling table in C:\dev\Learnings.md, and the self-test, which
    ' asserts this window HAS the flag and its buttons do NOT.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @CMessageBox_WndProc, _
            0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_CONTROLPARENT _
            )

    ' Replace the class style like every sibling does (dropping CS_HREDRAW/CS_VREDRAW, which
    ' only add flicker) and add the system drop shadow -- a WS_POPUP with no system caption gets
    ' no DWM shadow of its own, and without one the box reads as painted onto the parent rather
    ' than floating above it. CWindow registers a UNIQUE class per instance, so this cannot leak
    ' to other windows; the setter REPLACES and needs m_hwnd, so it must run after Create and
    ' before the first show. The shadow honors the system setting and degrades silently.
    '
    ' NOTE: CS_DBLCLKS is deliberately NOT included -- see the WndProc header.
    pWindow->ClassStyle = CS_DROPSHADOW

    dim as CMESSAGEBOX ptr pBox = new CMESSAGEBOX
    pBox->hWin    = hCtrl
    pBox->hParent = hWndParent

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). The two THICKNESS values are not scaled at
    ' all, here or anywhere -- a hairline stays a hairline.
    pBox->nCaptionHeight  = pWindow->ScaleY( CMESSAGEBOX_DEFAULT_CAPTIONHEIGHT )
    pBox->nCaptionPadX    = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_CAPTIONPADX )
    pBox->nCloseWidth     = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_CLOSEWIDTH )
    pBox->nBodyPadX       = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_BODYPADX )
    pBox->nBodyPadY       = pWindow->ScaleY( CMESSAGEBOX_DEFAULT_BODYPADY )
    pBox->nFooterPadX     = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_FOOTERPADX )
    pBox->nFooterPadY     = pWindow->ScaleY( CMESSAGEBOX_DEFAULT_FOOTERPADY )
    pBox->nIconWidth      = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_ICONWIDTH )
    pBox->nIconHeight     = pWindow->ScaleY( CMESSAGEBOX_DEFAULT_ICONHEIGHT )
    pBox->nIconGap        = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_ICONGAP )
    pBox->nButtonGap      = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_BUTTONGAP )
    pBox->nButtonMinWidth = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_BUTTONMINWIDTH )
    pBox->nMaxWidth       = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_MAXWIDTH )
    pBox->nMinWidth       = pWindow->ScaleX( CMESSAGEBOX_DEFAULT_MINWIDTH )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pBox)

    function = hCtrl

end function


' ========================================================================================
' Content.  All SILENT -- they change what is drawn, never what was answered.
' ========================================================================================
function CMessageBox_GetCaption( byval hMsgBox as HWND ) as DWSTRING
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return ""
    dim as DWSTRING wszOut = pBox->wszCaption
    return wszOut
end function

sub CMessageBox_SetCaption( byval hMsgBox as HWND, byval Text as DWSTRING )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->wszCaption = Text
    pBox->Refresh()
end sub

function CMessageBox_GetText( byval hMsgBox as HWND ) as DWSTRING
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return ""
    dim as DWSTRING wszOut = pBox->wszText
    return wszOut
end function

sub CMessageBox_SetText( byval hMsgBox as HWND, byval Text as DWSTRING )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->wszText = Text
    pBox->Refresh()
end sub

function CMessageBox_GetIconKind( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return MBX_ICON_NONE
    return pBox->nIconKind
end function

sub CMessageBox_SetIconKind( byval hMsgBox as HWND, byval nKind as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (nKind < MBX_ICON_NONE) orelse (nKind > MBX_ICON_QUESTION) then exit sub
    pBox->nIconKind = nKind
    pBox->Refresh()
end sub

' A glyph override TURNS THE ICON ON even when the kind is MBX_ICON_NONE (see
' CMESSAGEBOX.HasIcon): setting a glyph is an unambiguous request for one. Passing "" clears the
' override and hands the decision back to the kind.
sub CMessageBox_SetGlyph( byval hMsgBox as HWND, byval Glyph as DWSTRING )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->wszGlyph       = Glyph
    pBox->bGlyphOverride = (len( Glyph ) > 0)
    pBox->Refresh()
end sub

sub CMessageBox_SetIconColor( byval hMsgBox as HWND, byval clr as COLORREF )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->clrIconOverride = clr
    pBox->bIconOverride   = true
    ' Colour only: nothing here changes a measurement, so this repaints without re-laying-out.
    if pBox->hWin then InvalidateRect( pBox->hWin, NULL, FALSE )
end sub

sub CMessageBox_SetBeep( byval hMsgBox as HWND, byval bBeep as boolean )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->bBeep = bBeep
end sub


' ========================================================================================
' Buttons.
' ========================================================================================
function CMessageBox_AddButton( byval hMsgBox as HWND, byval Text as DWSTRING, byval id as long ) as HWND
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    if pBox->nButtonCount >= CMESSAGEBOX_MAXBUTTONS then return 0

    dim as long idx = pBox->nButtonCount

    ' The control id is positional and internal; the HOST's id goes on separately and is what
    ' DoModal returns. Keeping them apart is what stops a host id of 0 or 1 colliding with
    ' anything the dialog manager cares about.
    dim as HWND hBtn = CButton_Create( hMsgBox, IDC_CMESSAGEBOX_BUTTON_BASE + idx )
    if hBtn = 0 then return 0

    CButton_SetText( hBtn, Text )
    CButton_SetID( hBtn, id )
    CButton_SetClickCallback( hBtn, @CMessageBox_OnButtonClick )
    ' Push the box's current appearance into the new button, so Add-then-SetFont and
    ' SetFont-then-Add produce the same result (SetFont/SetButtonColors re-apply to all).
    CButton_SetColors( hBtn, @pBox->btnColors )
    if pBox->hTextFont  then CButton_SetFont( hBtn, pBox->hTextFont )
    if pBox->hGlyphFont then CButton_SetGlyphFont( hBtn, pBox->hGlyphFont )

    pBox->hButtons(idx) = hBtn
    pBox->nButtonCount += 1
    pBox->Refresh()
    return hBtn
end function


' Add a whole set AND set the default button and the cancel id to match. Additive: calling it on
' a box that already has buttons is a host error, not something this polices.
sub CMessageBox_AddPreset( byval hMsgBox as HWND, byval nPreset as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub

    select case nPreset
    case MBX_BTN_OKCANCEL
        CMessageBox_AddButton( hMsgBox, "OK", IDOK )
        CMessageBox_AddButton( hMsgBox, "Cancel", IDCANCEL )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        CMessageBox_SetCancelID( hMsgBox, IDCANCEL )

    case MBX_BTN_YESNO
        CMessageBox_AddButton( hMsgBox, "Yes", IDYES )
        CMessageBox_AddButton( hMsgBox, "No", IDNO )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        ' No Cancel button, so Esc / the X answer NO -- the safe half of a yes/no question.
        CMessageBox_SetCancelID( hMsgBox, IDNO )

    case MBX_BTN_YESNOCANCEL
        CMessageBox_AddButton( hMsgBox, "Yes", IDYES )
        CMessageBox_AddButton( hMsgBox, "No", IDNO )
        CMessageBox_AddButton( hMsgBox, "Cancel", IDCANCEL )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        CMessageBox_SetCancelID( hMsgBox, IDCANCEL )

    case MBX_BTN_RETRYCANCEL
        CMessageBox_AddButton( hMsgBox, "Retry", IDRETRY )
        CMessageBox_AddButton( hMsgBox, "Cancel", IDCANCEL )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        CMessageBox_SetCancelID( hMsgBox, IDCANCEL )

    case MBX_BTN_SAVE_DONTSAVE_CANCEL
        ' The reference screenshot. Note the ids: there is no IDSAVE in Win32, so a save
        ' question is a yes/no/cancel question wearing better captions.
        CMessageBox_AddButton( hMsgBox, "Save", IDYES )
        CMessageBox_AddButton( hMsgBox, "Don't Save", IDNO )
        CMessageBox_AddButton( hMsgBox, "Cancel", IDCANCEL )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        CMessageBox_SetCancelID( hMsgBox, IDCANCEL )

    case else       ' MBX_BTN_OK
        CMessageBox_AddButton( hMsgBox, "OK", IDOK )
        CMessageBox_SetDefaultButton( hMsgBox, 0 )
        ' The only button IS the cancel path: with nothing else to answer, Esc must not be a
        ' silent no-op.
        CMessageBox_SetCancelID( hMsgBox, IDOK )
    end select
end sub

function CMessageBox_GetButtonCount( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->nButtonCount
end function

function CMessageBox_GetButtonHandle( byval hMsgBox as HWND, byval idx as long ) as HWND
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    if (idx < 0) orelse (idx >= pBox->nButtonCount) then return 0
    return pBox->hButtons(idx)
end function

function CMessageBox_GetDefaultButton( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return -1
    return pBox->nDefaultIndex
end function

' Paints the accent border, claims Enter box-wide, AND takes the initial focus -- unless
' SetFocusButton is called AFTER this. See CMESSAGEBOX.ResolveFocusIndex for the precedence.
'
' Clearing the old default matters: CButton_SetDefault is appearance only and does not police
' uniqueness, so without this a host that changed its mind would end up with two accent borders.
sub CMessageBox_SetDefaultButton( byval hMsgBox as HWND, byval idx as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (idx < -1) orelse (idx >= pBox->nButtonCount) then exit sub

    for i as long = 0 to pBox->nButtonCount - 1
        CButton_SetDefault( pBox->hButtons(i), (i = idx) )
    next
    pBox->nDefaultIndex = idx
end sub

function CMessageBox_GetFocusButton( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return -1
    return pBox->ResolveFocusIndex()
end function

sub CMessageBox_SetFocusButton( byval hMsgBox as HWND, byval idx as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (idx < 0) orelse (idx >= pBox->nButtonCount) then exit sub
    pBox->nFocusIndex    = idx
    pBox->bFocusExplicit = true
end sub

function CMessageBox_GetCancelID( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->ResolveCancelID()
end function

sub CMessageBox_SetCancelID( byval hMsgBox as HWND, byval id as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->nCancelID    = id
    pBox->bCancelIDSet = true
end sub


' ========================================================================================
' Layout.  ALL setters take RAW PIXELS -- the caller DPI-scales.
' ========================================================================================
sub CMessageBox_SetMaxWidth( byval hMsgBox as HWND, byval nMaxWidth as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nMaxWidth < 1 then exit sub
    pBox->nMaxWidth = nMaxWidth
    pBox->Refresh()
end sub

sub CMessageBox_SetMinWidth( byval hMsgBox as HWND, byval nMinWidth as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nMinWidth < 0 then exit sub
    pBox->nMinWidth = nMinWidth
    pBox->Refresh()
end sub

sub CMessageBox_SetCaptionHeight( byval hMsgBox as HWND, byval nHeight as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nHeight < 0 then exit sub
    pBox->nCaptionHeight = nHeight
    pBox->Refresh()
end sub

sub CMessageBox_SetBodyPadding( byval hMsgBox as HWND, byval nX as long, byval nY as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (nX < 0) orelse (nY < 0) then exit sub
    pBox->nBodyPadX = nX
    pBox->nBodyPadY = nY
    pBox->Refresh()
end sub

sub CMessageBox_SetFooterPadding( byval hMsgBox as HWND, byval nX as long, byval nY as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (nX < 0) orelse (nY < 0) then exit sub
    pBox->nFooterPadX = nX
    pBox->nFooterPadY = nY
    pBox->Refresh()
end sub

sub CMessageBox_SetIconSize( byval hMsgBox as HWND, byval nIconWidth as long, byval nIconHeight as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if (nIconWidth < 0) orelse (nIconHeight < 0) then exit sub
    pBox->nIconWidth  = nIconWidth
    pBox->nIconHeight = nIconHeight
    pBox->Refresh()
end sub

sub CMessageBox_SetIconGap( byval hMsgBox as HWND, byval nGap as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nGap < 0 then exit sub
    pBox->nIconGap = nGap
    pBox->Refresh()
end sub

sub CMessageBox_SetButtonGap( byval hMsgBox as HWND, byval nGap as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nGap < 0 then exit sub
    pBox->nButtonGap = nGap
    pBox->Refresh()
end sub

sub CMessageBox_SetButtonMinWidth( byval hMsgBox as HWND, byval nMinWidth as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nMinWidth < 0 then exit sub
    pBox->nButtonMinWidth = nMinWidth
    pBox->Refresh()
end sub

sub CMessageBox_SetBorderThickness( byval hMsgBox as HWND, byval nThickness as long )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if nThickness < 0 then exit sub
    pBox->nBorderThick = nThickness
    ' The border is drawn INSIDE the client and takes no space from the layout, so this is a
    ' repaint rather than a re-measure.
    if pBox->hWin then InvalidateRect( pBox->hWin, NULL, FALSE )
end sub

sub CMessageBox_GetIdealSize( byval hMsgBox as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
    nBoxWidth  = 0
    nBoxHeight = 0
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    if pBox->bLayoutDirty then pBox->LayoutBox()
    nBoxWidth  = pBox->nIdealW
    nBoxHeight = pBox->nIdealH
end sub


' The rect queries. Each forces a pending layout, so results are always current. FALSE means the
' box has no geometry yet (created but never sized); an absent part answers TRUE with an EMPTY
' rect, because empty is the honest answer.
private function CMessageBox_QueryRect( _
            byval hMsgBox as HWND, _
            byval nPart   as long, _
            byref rc      as RECT _
            ) as boolean

    SetRectEmpty( @rc )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return false
    if pBox->bLayoutDirty then pBox->LayoutBox()
    if pBox->bLayoutDirty then return false      ' no client area yet

    select case nPart
    case MBX_PART_CAPTION : rc = pBox->rcCaption
    case MBX_PART_CLOSE   : rc = pBox->rcClose
    case MBX_PART_BODY    : rc = pBox->rcBody
    case MBX_PART_ICON    : rc = pBox->rcIcon
    case MBX_PART_TEXT    : rc = pBox->rcText
    case MBX_PART_FOOTER  : rc = pBox->rcFooter
    end select
    return true
end function

function CMessageBox_GetCaptionRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_CAPTION, rc )
end function

function CMessageBox_GetCloseRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_CLOSE, rc )
end function

function CMessageBox_GetBodyRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_BODY, rc )
end function

function CMessageBox_GetIconRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_ICON, rc )
end function

function CMessageBox_GetTextRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_TEXT, rc )
end function

function CMessageBox_GetFooterRect( byval hMsgBox as HWND, byref rc as RECT ) as boolean
    return CMessageBox_QueryRect( hMsgBox, MBX_PART_FOOTER, rc )
end function

function CMessageBox_GetButtonRect( byval hMsgBox as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return false
    if (idx < 0) orelse (idx >= pBox->nButtonCount) then return false
    if pBox->bLayoutDirty then pBox->LayoutBox()
    if pBox->bLayoutDirty then return false
    rc = pBox->rcButtons(idx)
    return true
end function


' ========================================================================================
' Appearance.
' ========================================================================================
sub CMessageBox_GetColors( byval hMsgBox as HWND, byval pColors as MBX_COLORS ptr )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if (pBox = 0) orelse (pColors = 0) then exit sub
    *pColors = pBox->colors
end sub

sub CMessageBox_SetColors( byval hMsgBox as HWND, byval pColors as MBX_COLORS ptr )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if (pBox = 0) orelse (pColors = 0) then exit sub
    pBox->colors = *pColors
    if pBox->hWin then InvalidateRect( pBox->hWin, NULL, FALSE )
end sub

' Pushed into every button that exists NOW and stored for every button added LATER, so the order
' of AddButton and SetButtonColors does not matter.
sub CMessageBox_SetButtonColors( byval hMsgBox as HWND, byval pColors as CBUTTON_COLORS ptr )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if (pBox = 0) orelse (pColors = 0) then exit sub
    pBox->btnColors = *pColors
    for i as long = 0 to pBox->nButtonCount - 1
        CButton_SetColors( pBox->hButtons(i), pColors )
    next
end sub

function CMessageBox_GetFont( byval hMsgBox as HWND ) as HFONT
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->hTextFont
end function

' The MESSAGE font and the MEASURING font. Changing it re-wraps, which changes the box's height.
' Also pushed into the buttons, whose own ideal widths then change too.
sub CMessageBox_SetFont( byval hMsgBox as HWND, byval hTextFont as HFONT )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->hTextFont = hTextFont
    for i as long = 0 to pBox->nButtonCount - 1
        CButton_SetFont( pBox->hButtons(i), hTextFont )
    next
    pBox->Refresh()
end sub

function CMessageBox_GetCaptionFont( byval hMsgBox as HWND ) as HFONT
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->hCaptionFont
end function

sub CMessageBox_SetCaptionFont( byval hMsgBox as HWND, byval hCaptionFont as HFONT )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->hCaptionFont = hCaptionFont
    ' The title contributes a WIDTH FLOOR, so this really can change the box's size.
    pBox->Refresh()
end sub

function CMessageBox_GetGlyphFont( byval hMsgBox as HWND ) as HFONT
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->hGlyphFont
end function

' The ICON font, used for both the message icon and the close X. A paint-time input only
' (CIconPanel's rule): the icon cell is DECLARED by SetIconSize and never measured, so changing
' this repaints without re-laying-out. It is still pushed into the buttons, where it is their
' glyph font and likewise does not re-measure them.
sub CMessageBox_SetGlyphFont( byval hMsgBox as HWND, byval hGlyphFont as HFONT )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->hGlyphFont = hGlyphFont
    for i as long = 0 to pBox->nButtonCount - 1
        CButton_SetGlyphFont( pBox->hButtons(i), hGlyphFont )
    next
    if pBox->hWin then InvalidateRect( pBox->hWin, NULL, FALSE )
end sub


function CMessageBox_GetCloseGlyphFont( byval hMsgBox as HWND ) as HFONT
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    return pBox->hCloseFont
end function

' The close X's font. Not pushed into the buttons -- it is caption chrome, not content -- and a
' paint-time input only, so this repaints without re-laying-out.
sub CMessageBox_SetCloseGlyphFont( byval hMsgBox as HWND, byval hCloseFont as HFONT )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->hCloseFont = hCloseFont
    if pBox->hWin then InvalidateRect( pBox->hWin, @pBox->rcClose, FALSE )
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub CMessageBox_SetPaintCallback( byval hMsgBox as HWND, byval usersub as MBX_PaintCallbackSub )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->PaintCallback = usersub
    if pBox->hWin then InvalidateRect( pBox->hWin, NULL, TRUE )
end sub

sub CMessageBox_SetMessageCallback( byval hMsgBox as HWND, byval userfunc as MBX_MessageCallbackFunc )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub
    pBox->MessageCallback = userfunc
end sub


' ========================================================================================
' Where does the box go?  Centred on the parent's WINDOW rect, or -- with no parent -- on the
' monitor holding the cursor, then clamped into that monitor's work area.
'
' THE CLAMP ORDER MATTERS. right/bottom first, then left/top, so a box taller than the work area
' ends up flush with the TOP rather than pushed off it. Work-area coordinates are freely
' negative on a multi-monitor desktop, which is exactly why the clamp is done against
' MonitorFromPoint's answer instead of against screen metrics (CPopupMenu's arithmetic).
' ========================================================================================
private sub CMessageBox_ComputeOrigin( _
            byval pBox as CMESSAGEBOX ptr, _
            byval nBoxWidth  as long, _
            byval nBoxHeight as long, _
            byref x as long, _
            byref y as long _
            )

    x = 0 : y = 0
    if pBox = 0 then exit sub

    dim as RECT rcAnchor
    if (pBox->hParent <> 0) andalso IsWindow( pBox->hParent ) then
        GetWindowRect( pBox->hParent, @rcAnchor )
    else
        dim as POINT ptCur
        GetCursorPos( @ptCur )
        dim mi as MONITORINFO
        mi.cbSize = sizeof(MONITORINFO)
        if GetMonitorInfo( MonitorFromPoint( ptCur, MONITOR_DEFAULTTONEAREST ), @mi ) then
            rcAnchor = mi.rcWork
        end if
    end if

    x = rcAnchor.left + (((rcAnchor.right - rcAnchor.left) - nBoxWidth) \ 2)
    y = rcAnchor.top  + (((rcAnchor.bottom - rcAnchor.top) - nBoxHeight) \ 2)

    dim as POINT ptCentre
    ptCentre.x = x + (nBoxWidth \ 2)
    ptCentre.y = y + (nBoxHeight \ 2)
    dim mi2 as MONITORINFO
    mi2.cbSize = sizeof(MONITORINFO)
    if GetMonitorInfo( MonitorFromPoint( ptCentre, MONITOR_DEFAULTTONEAREST ), @mi2 ) then
        if (x + nBoxWidth)  > mi2.rcWork.right  then x = mi2.rcWork.right - nBoxWidth
        if (y + nBoxHeight) > mi2.rcWork.bottom then y = mi2.rcWork.bottom - nBoxHeight
        if x < mi2.rcWork.left then x = mi2.rcWork.left
        if y < mi2.rcWork.top  then y = mi2.rcWork.top
    end if
end sub


' ========================================================================================
' The one MessageBeep, mapped from the icon kind. Win32's MessageBox does this too, and the
' sounds are user-configurable, so a host that suppresses it is overriding a system preference
' rather than removing noise.
' ========================================================================================
private sub CMessageBox_Beep( byval nKind as long )
    select case nKind
    case MBX_ICON_ERROR    : MessageBeep( MB_ICONHAND )
    case MBX_ICON_WARNING  : MessageBeep( MB_ICONEXCLAMATION )
    case MBX_ICON_QUESTION : MessageBeep( MB_ICONQUESTION )
    case MBX_ICON_INFO     : MessageBeep( MB_ICONASTERISK )
    case else              : MessageBeep( MB_OK )
    end select
end sub


' ========================================================================================
' Keys the box claims before IsDialogMessage ever sees them.
'
' ESCAPE always. IsDialogMessage does synthesise a WM_COMMAND/IDCANCEL for it, but only in terms
' of a control id we deliberately do not use as a host id -- handling it here keeps the cancel
' answer in ONE place (ResolveCancelID) instead of two.
'
' ENTER only when a button does NOT have the focus. A focused CButton claims Enter itself
' through WM_GETDLGCODE, and clicking THAT button is the right answer -- not the default one.
' Stealing Enter unconditionally here would make the default button fire no matter which button
' the user had tabbed to, which is the natural bug in this area.
' ========================================================================================
private function CMessageBox_PreTranslate( _
            byval pBox    as CMESSAGEBOX ptr, _
            byval hMsgBox as HWND, _
            byval pMsg    as MSG ptr _
            ) as boolean

    if (pBox = 0) orelse (pMsg = 0) then return false
    if pMsg->message <> WM_KEYDOWN then return false
    ' Only keys aimed at the box or one of its descendants. Without this a key sent to some
    ' other window of the host's while the box happened to be up would be swallowed.
    if (pMsg->hwnd <> hMsgBox) andalso (IsChild( hMsgBox, pMsg->hwnd ) = 0) then return false

    select case pMsg->wParam
    case VK_ESCAPE
        PostMessage( hMsgBox, WM_CLOSE, 0, 0 )
        return true

    case VK_RETURN
        dim as HWND hFocus = GetFocus()
        for i as long = 0 to pBox->nButtonCount - 1
            if hFocus = pBox->hButtons(i) then return false
        next
        if (pBox->nDefaultIndex >= 0) andalso (pBox->nDefaultIndex < pBox->nButtonCount) then
            ' CButton_Click FIRES the click callback -- the documented exception to the family's
            ' silent-setter rule, and the reason this works at all.
            CButton_Click( pBox->hButtons( pBox->nDefaultIndex ) )
            return true
        end if
    end select

    return false
end function


' ========================================================================================
' CMessageBox_DoModal -- measure, size, centre, show, PUMP, and return the answer.
'
' THE NESTED LOOP IS THIS CONTROL'S ONE BIG DEPARTURE from the rest of the family, which is
' passive and pumped by its host. Four things in here are the whole risk surface:
'
'   1. THE LOOP CONDITION IS RE-FETCHED EVERY ITERATION, never cached. A host that destroys the
'      parent takes this owned window with it and frees pBox -- so a cached pointer would be a
'      use-after-free on the very next test. IsWindow first, then the pointer.
'
'   2. WM_QUIT IS RE-POSTED, not consumed. GetMessage returning 0 removes the quit from the
'      queue; leaving it consumed would swallow the host's shutdown and the outer pump would
'      spin forever. (AfxInputBox ends its nested loop by letting the quit through, which works
'      only because nothing runs after it.)
'
'   3. THE PARENT IS RE-ENABLED BEFORE THE BOX IS DESTROYED. Destroying an owned window while
'      its owner is still disabled hands activation to another application -- the classic
'      "my app went behind Explorer when the dialog closed" bug.
'
'   4. IsDialogMessage IS OURS. It is what makes Tab walk the buttons, and it is why the host has
'      no pump obligation at all. It only acts once a DESCENDANT has focus, hence the explicit
'      SetFocus before the loop -- the second half of the CButton Tab bug in Learnings.md.
' ========================================================================================
function CMessageBox_DoModal( byval hMsgBox as HWND ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0

    ' Captured now, because pBox may not survive the loop (see 1 above) and the parent still has
    ' to be re-enabled afterwards no matter what happened.
    dim as HWND hParent = pBox->hParent

    ' ---- measure and place ------------------------------------------------------------------
    pBox->bLayoutDirty = true
    pBox->LayoutBox()
    dim as long nBoxWidth  = pBox->nIdealW
    dim as long nBoxHeight = pBox->nIdealH
    if nBoxWidth  < 1 then nBoxWidth  = 1
    if nBoxHeight < 1 then nBoxHeight = 1

    dim as long x, y
    CMessageBox_ComputeOrigin( pBox, nBoxWidth, nBoxHeight, x, y )
    ' This fires WM_SIZE, which re-lays out and positions the child buttons.
    SetWindowPos( hMsgBox, HWND_TOP, x, y, nBoxWidth, nBoxHeight, SWP_NOACTIVATE )

    ' Before the show, not after: a beep that lands after the paint reads as a delayed error.
    if pBox->bBeep then CMessageBox_Beep( pBox->nIconKind )

    ' ---- go modal ---------------------------------------------------------------------------
    ' Disable the parent BEFORE showing, so there is no frame in which both windows are live.
    if hParent then EnableWindow( hParent, FALSE )

    ShowWindow( hMsgBox, SW_SHOW )
    UpdateWindow( hMsgBox )
    SetActiveWindow( hMsgBox )

    ' IsDialogMessage ignores Tab until a DESCENDANT of hMsgBox has focus, so this is not
    ' optional -- without it the first Tab does nothing and it reads as broken tabstops.
    dim as long nFocusIdx = pBox->ResolveFocusIndex()
    if nFocusIdx >= 0 then SetFocus( pBox->hButtons(nFocusIdx) )

    pBox->bRunning = true

    dim uMsg as MSG
    do
        if IsWindow( hMsgBox ) = 0 then exit do
        dim as CMESSAGEBOX ptr pLoop = GetMessageBoxPointer( hMsgBox )
        if pLoop = 0 then exit do
        if pLoop->bRunning = false then exit do

        if GetMessageW( @uMsg, NULL, 0, 0 ) = 0 then
            ' The host is shutting down. Put the quit back so the OUTER pump still sees it.
            PostQuitMessage( cast( long, uMsg.wParam ) )
            exit do
        end if

        if CMessageBox_PreTranslate( pLoop, hMsgBox, @uMsg ) = false then
            if IsDialogMessageW( hMsgBox, @uMsg ) = 0 then
                TranslateMessage( @uMsg )
                DispatchMessageW( @uMsg )
            end if
        end if
    loop

    ' ---- unwind, in this order ---------------------------------------------------------------
    ' Read the answer while the window is still alive...
    dim as long nResult = 0
    if IsWindow( hMsgBox ) then
        dim as CMESSAGEBOX ptr pEnd = GetMessageBoxPointer( hMsgBox )
        if pEnd then nResult = pEnd->nResult
    end if

    ' ...re-enable and re-activate the parent BEFORE destroying us (see 3 above)...
    if hParent andalso IsWindow( hParent ) then
        EnableWindow( hParent, TRUE )
        SetActiveWindow( hParent )
    end if

    ' ...and only then destroy. The handle is dead after this call.
    if IsWindow( hMsgBox ) then DestroyWindow( hMsgBox )

    return nResult
end function


' ========================================================================================
' CMessageBox_Show -- Create + configure + DoModal, for the common case.
'
' Everything it does is reachable through the long form; nothing here is privileged. A host that
' wants custom captions, a third button with its own id, a glyph override or its own colours
' builds the box itself.
' ========================================================================================
function CMessageBox_Show( byval hWndParent as HWND, _
                           byval Text as DWSTRING, _
                           byval Caption as DWSTRING, _
                           byval nIconKind as long = MBX_ICON_NONE, _
                           byval nPreset as long = MBX_BTN_OK ) as long

    dim as HWND hBox = CMessageBox_Create( hWndParent )
    if hBox = 0 then return 0

    CMessageBox_SetCaption( hBox, Caption )
    CMessageBox_SetText( hBox, Text )
    CMessageBox_SetIconKind( hBox, nIconKind )
    CMessageBox_AddPreset( hBox, nPreset )

    ' NOTE: no fonts are set here, so the box measures and draws with the stock GUI font and the
    ' icon codepoints come out as missing-glyph boxes. A host that has loaded Segoe Fluent Icons
    ' wants the long form, or its own wrapper around it. Documented in the README rather than
    ' papered over with a font this control would then have to own.
    return CMessageBox_DoModal( hBox )
end function


' ========================================================================================
' Plumbing.
' ========================================================================================

' Size the box to its ideal size WITHOUT showing it or entering the modal loop, so geometry can
' be asserted and the tone probe can run. There is no other way in: every rect this control owns
' depends on the client area, and the client area is otherwise only established by DoModal.
sub CMessageBox_LayoutForTest( byval hMsgBox as HWND )
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then exit sub

    pBox->bLayoutDirty = true
    pBox->LayoutBox()
    dim as long nBoxWidth  = pBox->nIdealW
    dim as long nBoxHeight = pBox->nIdealH
    if nBoxWidth  < 1 then nBoxWidth  = 1
    if nBoxHeight < 1 then nBoxHeight = 1

    ' SWP_NOACTIVATE and no SW_SHOW: the window stays hidden, which is what makes this safe to
    ' call from a self-test that must not steal focus from the host being tested.
    SetWindowPos( hMsgBox, 0, 0, 0, nBoxWidth, nBoxHeight, _
                  SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
end sub


private function CMessageBox_PartRect( byval pBox as CMESSAGEBOX ptr, byval nPart as long ) as RECT
    dim as RECT rc
    if pBox = 0 then return rc
    select case nPart
    case MBX_PART_CAPTION : rc = pBox->rcCaption
    case MBX_PART_BODY    : rc = pBox->rcBody
    case MBX_PART_FOOTER  : rc = pBox->rcFooter
    case MBX_PART_TEXT    : rc = pBox->rcText
    case MBX_PART_ICON    : rc = pBox->rcIcon
    case MBX_PART_CLOSE   : rc = pBox->rcClose
    end select
    return rc
end function


' Render the box OFFSCREEN with its current painter and count the distinct colours inside one of
' its rects. See the declaration in CMessageBox.bi for what this is for and why it is public.
function CMessageBox_CountRenderedTones( byval hMsgBox as HWND, byval nPart as long ) as long
    dim as CMESSAGEBOX ptr pBox = GetMessageBoxPointer(hMsgBox)
    if pBox = 0 then return 0
    if pBox->bLayoutDirty then pBox->LayoutBox()
    if pBox->bLayoutDirty then return 0          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hMsgBox, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return 0

    dim as RECT rcPart = CMessageBox_PartRect( pBox, nPart )
    if IsRectEmpty( @rcPart ) then return 0

    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return 0

    dim as HDC hRef = GetDC( hMsgBox )
    if hRef = 0 then return 0
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hMsgBox, hRef )
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
    ReleaseDC( hMsgBox, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return 0
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    ' Force the close button HOT so its own fill is actually exercised -- idle, its background
    ' equals the caption's and the probe would be measuring the caption twice.
    dim as boolean bSavedHot = pBox->isCloseHot
    pBox->isCloseHot = true

    scope
        dim as CBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hMsgBox, hMem, rcClient, hMem )

        b.SetBackColor( pBox->colors.BackColor )
        b.PaintClientRect()

        dim p as MBX_PAINTINFO
        CMessageBox_FillPaintInfo( pBox, hMsgBox, @b, @p )

        if pBox->PaintCallback then
            pBox->PaintCallback( @p )
        else
            CMessageBox_RenderDefault( pBox, @b, @p )
        end if

        b.EndDoubleBuffer()
    end scope

    pBox->isCloseHot = bSavedHot

    ' Count distinct tones. The alpha byte is masked off: GDI text leaves it at 0 and GDI+
    ' shapes leave it at 255, so an unmasked compare would report two tones for one colour.
    dim tones(0 to 63) as ulong
    dim as long nTones = 0
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for yy as long = rcPart.top to rcPart.bottom - 1
        for xx as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(yy * cw) + xx] and &h00FFFFFF
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
' Self-test.  Gated on CMESSAGEBOX_SELFTEST=1.
'
' NOTHING HERE ENTERS THE MODAL LOOP. Every assertion runs against a box that has been sized by
' CMessageBox_LayoutForTest and never shown -- a self-test that went modal would block the host
' it was called from and could not report anything.
' ========================================================================================
dim shared as long gMBX_TestCount
dim shared as long gMBX_FailCount

private sub MBXTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gMBX_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gMBX_FailCount += 1
    end if
    ' Built as a plain STRING first: print on a DWSTRING, or on a string & wstring concat,
    ' interleaves nulls (C:\dev\Learnings.md).
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function MBXTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function


sub CMessageBox_RunSelfTest( byval hWndParent as HWND )
    if environ("CMESSAGEBOX_SELFTEST") <> "1" then exit sub

    gMBX_TestCount = 0
    gMBX_FailCount = 0
    print "=== CMessageBox self-test ==="

    dim as string sD

    ' ---- 1. the pure resolvers, asserted on their own --------------------------------------
    ' These take no control pointer and touch no global, which is the whole reason they exist as
    ' separate functions: the mapping is a truth table, not something to eyeball in a paint.
    MBXTest_Check( "glyph: NONE resolves to nothing", _
                   len( CMessageBox_ResolveGlyph( MBX_ICON_NONE ) ) = 0, "" )
    MBXTest_Check( "glyph: INFO", _
                   CMessageBox_ResolveGlyph( MBX_ICON_INFO ) = wchr(CMESSAGEBOX_GLYPH_INFO), "" )
    MBXTest_Check( "glyph: WARNING", _
                   CMessageBox_ResolveGlyph( MBX_ICON_WARNING ) = wchr(CMESSAGEBOX_GLYPH_WARNING), "" )
    MBXTest_Check( "glyph: ERROR", _
                   CMessageBox_ResolveGlyph( MBX_ICON_ERROR ) = wchr(CMESSAGEBOX_GLYPH_ERROR), "" )
    MBXTest_Check( "glyph: QUESTION", _
                   CMessageBox_ResolveGlyph( MBX_ICON_QUESTION ) = wchr(CMESSAGEBOX_GLYPH_QUESTION), "" )

    scope
        dim c as MBX_COLORS
        MBXTest_Check( "icon colour: INFO", _
                       CMessageBox_ResolveIconColor( MBX_ICON_INFO, @c ) = c.IconColorInfo, "" )
        MBXTest_Check( "icon colour: WARNING", _
                       CMessageBox_ResolveIconColor( MBX_ICON_WARNING, @c ) = c.IconColorWarning, "" )
        MBXTest_Check( "icon colour: ERROR", _
                       CMessageBox_ResolveIconColor( MBX_ICON_ERROR, @c ) = c.IconColorError, "" )
        MBXTest_Check( "icon colour: QUESTION", _
                       CMessageBox_ResolveIconColor( MBX_ICON_QUESTION, @c ) = c.IconColorQuestion, "" )
        MBXTest_Check( "icon colour: NONE falls back to the body fore", _
                       CMessageBox_ResolveIconColor( MBX_ICON_NONE, @c ) = c.ForeColor, "" )
    end scope

    ' ---- the box under test ----------------------------------------------------------------
    dim as HWND h = CMessageBox_Create( hWndParent )
    if h = 0 then
        print "FAIL  create   [CMessageBox_Create returned 0]"
        exit sub
    end if
    dim as CMESSAGEBOX ptr pB = GetMessageBoxPointer(h)
    if pB = 0 then
        print "FAIL  create   [no state pointer]"
        DestroyWindow(h)
        exit sub
    end if

    CMessageBox_SetCaption( h, "Warning" )
    CMessageBox_SetText( h, "This buffer contains unsaved edits. Do you want to save it?" )
    CMessageBox_SetIconKind( h, MBX_ICON_WARNING )
    CMessageBox_AddPreset( h, MBX_BTN_SAVE_DONTSAVE_CANCEL )
    CMessageBox_LayoutForTest( h )

    dim as RECT rcClient
    GetClientRect( h, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    sD = "client " & str(cw) & "x" & str(ch)
    MBXTest_Check( "sized before ever being shown", (cw > 0) andalso (ch > 0), sD )

    dim as RECT rcCap, rcClose, rcBody, rcText, rcIcon, rcFoot
    CMessageBox_GetCaptionRect( h, rcCap )
    CMessageBox_GetCloseRect( h, rcClose )
    CMessageBox_GetBodyRect( h, rcBody )
    CMessageBox_GetTextRect( h, rcText )
    CMessageBox_GetIconRect( h, rcIcon )
    CMessageBox_GetFooterRect( h, rcFoot )

    ' ---- 2. the three bands tile the client exactly, with no gap and no overlap -------------
    ' Asserted against each OTHER rather than against the fields they were computed from, so a
    ' band that is placed correctly but sized from the wrong input still fails.
    sD = "cap " & MBXTest_RectStr(rcCap)
    MBXTest_Check( "caption starts at the top-left", _
                   (rcCap.left = 0) andalso (rcCap.top = 0), sD )
    MBXTest_Check( "caption spans the full width", (rcCap.right = cw), sD )
    MBXTest_Check( "caption height is the declared one", _
                   (rcCap.bottom - rcCap.top) = pB->nCaptionHeight, sD )

    sD = "body " & MBXTest_RectStr(rcBody) & " foot " & MBXTest_RectStr(rcFoot)
    MBXTest_Check( "body begins where the caption ends", (rcBody.top = rcCap.bottom), sD )
    MBXTest_Check( "footer begins where the body ends", (rcFoot.top = rcBody.bottom), sD )
    MBXTest_Check( "footer ends at the client bottom", (rcFoot.bottom = ch), sD )
    MBXTest_Check( "body spans the full width", _
                   (rcBody.left = 0) andalso (rcBody.right = cw), sD )
    MBXTest_Check( "footer spans the full width", _
                   (rcFoot.left = 0) andalso (rcFoot.right = cw), sD )

    dim as long bandSum = (rcCap.bottom - rcCap.top) + (rcBody.bottom - rcBody.top) + _
                          (rcFoot.bottom - rcFoot.top)
    sD = "caption+body+footer=" & str(bandSum) & " client=" & str(ch)
    MBXTest_Check( "the three bands tile the client exactly", (bandSum = ch), sD )

    ' ---- 3. the close button ----------------------------------------------------------------
    sD = "close " & MBXTest_RectStr(rcClose)
    MBXTest_Check( "close is pinned to the right edge", (rcClose.right = cw), sD )
    MBXTest_Check( "close is the declared width", _
                   (rcClose.right - rcClose.left) = pB->nCloseWidth, sD )
    MBXTest_Check( "close fills the caption band's height", _
                   (rcClose.top = rcCap.top) andalso (rcClose.bottom = rcCap.bottom), sD )

    dim as RECT rcCapText = pB->rcCaptionText
    sD = "captext " & MBXTest_RectStr(rcCapText)
    MBXTest_Check( "title starts at the caption padding", _
                   (rcCapText.left = pB->nCaptionPadX), sD )
    MBXTest_Check( "title never runs under the close button", _
                   (rcCapText.right <= rcClose.left), sD )

    ' ---- 4. the icon and the text ------------------------------------------------------------
    sD = "icon " & MBXTest_RectStr(rcIcon)
    MBXTest_Check( "icon present for a real kind", IsRectEmpty( @rcIcon ) = 0, sD )
    MBXTest_Check( "icon is the declared cell", _
                   ((rcIcon.right - rcIcon.left) = pB->nIconWidth) andalso _
                   ((rcIcon.bottom - rcIcon.top) = pB->nIconHeight), sD )
    MBXTest_Check( "icon pinned at the body padding", (rcIcon.left = pB->nBodyPadX), sD )

    sD = "text " & MBXTest_RectStr(rcText)
    MBXTest_Check( "text starts after icon + gap", _
                   (rcText.left = pB->nBodyPadX + pB->nIconWidth + pB->nIconGap), sD )
    MBXTest_Check( "text ends at the body padding", (rcText.right = cw - pB->nBodyPadX), sD )
    MBXTest_Check( "text height is the MEASURED wrapped height", _
                   (rcText.bottom - rcText.top) = pB->nTextBlockH, sD )
    MBXTest_Check( "text sits inside the body", _
                   (rcText.top >= rcBody.top) andalso (rcText.bottom <= rcBody.bottom), sD )
    MBXTest_Check( "icon sits inside the body", _
                   (rcIcon.top >= rcBody.top) andalso (rcIcon.bottom <= rcBody.bottom), sD )

    ' The icon and the text are each v-centred on the TALLER of the two, so their centres agree.
    dim as long iconMid = (rcIcon.top + rcIcon.bottom) \ 2
    dim as long textMid = (rcText.top + rcText.bottom) \ 2
    sD = "iconMid=" & str(iconMid) & " textMid=" & str(textMid)
    MBXTest_Check( "icon and text share a vertical centre", abs(iconMid - textMid) <= 1, sD )

    ' ---- 5. the buttons -----------------------------------------------------------------------
    MBXTest_Check( "preset added three buttons", CMessageBox_GetButtonCount(h) = 3, _
                   "count=" & str(CMessageBox_GetButtonCount(h)) )
    MBXTest_Check( "a fourth button is refused", _
                   CMessageBox_AddButton( h, "Extra", 99 ) = 0, "" )

    dim as RECT rb0, rb1, rb2
    CMessageBox_GetButtonRect( h, 0, rb0 )
    CMessageBox_GetButtonRect( h, 1, rb1 )
    CMessageBox_GetButtonRect( h, 2, rb2 )
    sD = MBXTest_RectStr(rb0) & " " & MBXTest_RectStr(rb1) & " " & MBXTest_RectStr(rb2)

    MBXTest_Check( "last button is right-aligned to the footer padding", _
                   (rb2.right = cw - pB->nFooterPadX), sD )
    MBXTest_Check( "gap between buttons 0 and 1 is exact", _
                   (rb1.left - rb0.right) = pB->nButtonGap, sD )
    MBXTest_Check( "gap between buttons 1 and 2 is exact", _
                   (rb2.left - rb1.right) = pB->nButtonGap, sD )
    MBXTest_Check( "buttons are laid out left to right in the order added", _
                   (rb0.left < rb1.left) andalso (rb1.left < rb2.left), sD )
    MBXTest_Check( "all buttons share one height", _
                   ((rb0.bottom - rb0.top) = (rb1.bottom - rb1.top)) andalso _
                   ((rb1.bottom - rb1.top) = (rb2.bottom - rb2.top)), sD )
    MBXTest_Check( "all buttons share one baseline", _
                   (rb0.top = rb1.top) andalso (rb1.top = rb2.top), sD )
    MBXTest_Check( "buttons honour the min-width floor", _
                   ((rb0.right - rb0.left) >= pB->nButtonMinWidth) andalso _
                   ((rb1.right - rb1.left) >= pB->nButtonMinWidth) andalso _
                   ((rb2.right - rb2.left) >= pB->nButtonMinWidth), sD )
    MBXTest_Check( "buttons sit inside the footer band", _
                   (rb0.top >= rcFoot.top) andalso (rb2.bottom <= rcFoot.bottom), sD )
    MBXTest_Check( "first button clears the left footer padding", _
                   (rb0.left >= pB->nFooterPadX), sD )

    ' ---- 6. cancel-id resolution ---------------------------------------------------------------
    MBXTest_Check( "cancel unset resolves to the LAST button's id", _
                   CMessageBox_GetCancelID(h) = IDCANCEL, _
                   "id=" & str(CMessageBox_GetCancelID(h)) )
    CMessageBox_SetCancelID( h, 4242 )
    MBXTest_Check( "an explicit cancel id wins", CMessageBox_GetCancelID(h) = 4242, "" )

    ' ---- 7. default and focus, and the precedence between them ----------------------------------
    ' The preset made button 0 default and nothing has called SetFocusButton, so focus follows.
    MBXTest_Check( "preset made button 0 the default", CMessageBox_GetDefaultButton(h) = 0, _
                   "idx=" & str(CMessageBox_GetDefaultButton(h)) )
    MBXTest_Check( "focus follows the default when not set explicitly", _
                   CMessageBox_GetFocusButton(h) = 0, _
                   "idx=" & str(CMessageBox_GetFocusButton(h)) )
    CMessageBox_SetDefaultButton( h, 1 )
    MBXTest_Check( "focus follows a CHANGED default", CMessageBox_GetFocusButton(h) = 1, _
                   "idx=" & str(CMessageBox_GetFocusButton(h)) )
    MBXTest_Check( "only one button carries the default border", _
                   (CButton_GetDefault( CMessageBox_GetButtonHandle(h,0) ) = false) andalso _
                   (CButton_GetDefault( CMessageBox_GetButtonHandle(h,1) ) = true) andalso _
                   (CButton_GetDefault( CMessageBox_GetButtonHandle(h,2) ) = false), "" )
    CMessageBox_SetFocusButton( h, 2 )
    MBXTest_Check( "an explicit focus button overrides the default", _
                   CMessageBox_GetFocusButton(h) = 2, _
                   "idx=" & str(CMessageBox_GetFocusButton(h)) )
    MBXTest_Check( "...and does NOT move the default border", _
                   CMessageBox_GetDefaultButton(h) = 1, "" )

    ' ---- 8. tab navigability, end to end --------------------------------------------------------
    ' CWindow.Create defaults dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE. For a LEAF
    ' control that flag is fatal (CButton shipped broken with it); for THIS window it is
    ' REQUIRED, because the tabstops are its children. Both halves are asserted, because the flag
    ' is invisible at the call site that causes it.
    dim as ulong exStyle = cast( ulong, GetWindowLongPtr( h, GWL_EXSTYLE ) )
    dim as ulong wsStyle = cast( ulong, GetWindowLongPtr( h, GWL_STYLE ) )
    sD = "exstyle=&h" & hex(exStyle) & " style=&h" & hex(wsStyle)
    MBXTest_Check( "the BOX is a control parent (its children are the tabstops)", _
                   (exStyle and WS_EX_CONTROLPARENT) <> 0, sD )
    MBXTest_Check( "the box itself is NOT a tabstop", (wsStyle and WS_TABSTOP) = 0, sD )

    for i as long = 0 to 2
        dim as HWND hb = CMessageBox_GetButtonHandle( h, i )
        dim as ulong bEx = cast( ulong, GetWindowLongPtr( hb, GWL_EXSTYLE ) )
        dim as ulong bSt = cast( ulong, GetWindowLongPtr( hb, GWL_STYLE ) )
        sD = "button " & str(i) & " exstyle=&h" & hex(bEx) & " style=&h" & hex(bSt)
        MBXTest_Check( "button " & str(i) & " is a tab ITEM, not a container", _
                       ((bSt and WS_TABSTOP) <> 0) andalso ((bEx and WS_EX_CONTROLPARENT) = 0), sD )
    next

    ' End to end: walk the dialog manager's own answer. This is the assertion that would have
    ' caught CButton's bug, and it is the only one here that tests what the USER experiences
    ' rather than what a style bit says.
    scope
        dim as HWND hFirst = GetNextDlgTabItem( h, NULL, FALSE )
        dim as HWND hNext1 = GetNextDlgTabItem( h, hFirst, FALSE )
        dim as HWND hNext2 = GetNextDlgTabItem( h, hNext1, FALSE )
        dim as HWND hWrap  = GetNextDlgTabItem( h, hNext2, FALSE )
        sD = "ids " & str(GetDlgCtrlID(hFirst)) & " " & str(GetDlgCtrlID(hNext1)) & " " & _
             str(GetDlgCtrlID(hNext2)) & " -> " & str(GetDlgCtrlID(hWrap))
        MBXTest_Check( "Tab reaches every button and cycles", _
                       (hFirst = CMessageBox_GetButtonHandle(h,0)) andalso _
                       (hNext1 = CMessageBox_GetButtonHandle(h,1)) andalso _
                       (hNext2 = CMessageBox_GetButtonHandle(h,2)) andalso _
                       (hWrap  = hFirst), sD )
        MBXTest_Check( "Shift+Tab walks the other way", _
                       GetNextDlgTabItem( h, hFirst, TRUE ) = CMessageBox_GetButtonHandle(h,2), sD )
    end scope

    ' ---- 9. the offscreen tone probe -------------------------------------------------------------
    ' THE THRESHOLDS ARE NOT "> 1", AND THAT IS THE POINT. CButton can use "> 1" because a wiped
    ' button really is one flat colour. This control cannot: its bands span the full width, so
    ' the window border strokes through EVERY one of them and a fully flooded box still measures
    ' 2 tones per band. A "> 1" assertion here passes on a completely destroyed render.
    '
    ' Measured, by deliberately swapping the border's PaintRoundOutline for PaintBorderRect
    ' (which fills before it strokes) and rebuilding:
    '
    '            caption   body   footer   close
    '   healthy      50      64        3       3
    '   flooded       2       2        2       2
    '
    ' So the floors below are calibrated to sit between those two rows. The footer's and the
    ' close button's are deliberately EXACT -- 3 is a fill plus its one distinct mark (the
    ' hairline / the glyph), and losing either should fail rather than be rounded away.
    '
    ' One limit worth stating, because it was found the hard way: flooding the FOOTER alone is
    ' invisible to this probe, since the flood colour would be the FooterBackColor the band was
    ' just filled with. The probe catches a flood that crosses bands, not one that repaints a
    ' band in its own colour.
    scope
        dim as long tCap   = CMessageBox_CountRenderedTones( h, MBX_PART_CAPTION )
        dim as long tBody  = CMessageBox_CountRenderedTones( h, MBX_PART_BODY )
        dim as long tFoot  = CMessageBox_CountRenderedTones( h, MBX_PART_FOOTER )
        dim as long tClose = CMessageBox_CountRenderedTones( h, MBX_PART_CLOSE )
        sD = "caption=" & str(tCap) & " body=" & str(tBody) & " footer=" & str(tFoot) & _
             " close=" & str(tClose) & "   (flooded reads 2/2/2/2)"
        MBXTest_Check( "caption band renders its title, not a flood", (tCap >= 10), sD )
        MBXTest_Check( "body renders its message and icon, not a flood", (tBody >= 10), sD )
        MBXTest_Check( "footer renders its fill AND its divider hairline", (tFoot >= 3), sD )
        MBXTest_Check( "the close button renders its own fill and glyph", (tClose >= 3), sD )
    end scope

    DestroyWindow( h )

    ' ---- 10. auto-height, shrink-to-fit, and the no-icon layout ------------------------------------
    ' A SECOND box, because these need different content and the assertions above are all about
    ' the first one's fixed geometry.
    scope
        dim as HWND h2 = CMessageBox_Create( hWndParent )
        if h2 = 0 then
            print "FAIL  create #2   [CMessageBox_Create returned 0]"
        else
            dim as CMESSAGEBOX ptr p2 = GetMessageBoxPointer(h2)
            CMessageBox_SetCaption( h2, "Question" )
            CMessageBox_AddPreset( h2, MBX_BTN_OK )
            CMessageBox_SetMinWidth( h2, 0 )       ' out of the way, so shrink-to-fit is visible

            dim as long w1, hh1, w2, hh2
            CMessageBox_SetText( h2, "Short." )
            CMessageBox_GetIdealSize( h2, w1, hh1 )

            CMessageBox_SetText( h2, "This is a considerably longer message that is certain " & _
                                     "to wrap onto several separate lines once the maximum " & _
                                     "width has been applied to it by the layout pass." )
            CMessageBox_GetIdealSize( h2, w2, hh2 )

            sD = "short " & str(w1) & "x" & str(hh1) & "  long " & str(w2) & "x" & str(hh2)
            MBXTest_Check( "a wrapped message is TALLER than a one-liner", (hh2 > hh1), sD )
            MBXTest_Check( "a short message shrinks the box to fit", (w1 < p2->nMaxWidth), sD )
            MBXTest_Check( "a long message clamps to the wrap width", (w2 <= p2->nMaxWidth), sD )

            ' No icon: the cell AND its gap are both given back, so the text starts at the body
            ' padding and the box is narrower by exactly iconW + iconGap.
            '
            ' THE MESSAGE HERE IS DELIBERATELY LONG ENOUGH TO DOMINATE THE FOOTER. The first
            ' version of this assertion used "Short.", and both widths came back 128 -- because
            ' at that size the box's width is the FOOTER's (2*footerPadX + one button's
            ' min-width), so the icon's 48px never reached the total and the test was measuring
            ' the max() rather than the icon. It failed honestly, which is the only reason the
            ' scaffolding error was visible at all.
            CMessageBox_SetText( h2, "Long enough that the text block, not the footer, decides." )
            CMessageBox_SetIconKind( h2, MBX_ICON_INFO )
            dim as long wIcon, hIcon
            CMessageBox_GetIdealSize( h2, wIcon, hIcon )
            dim as long textHIcon = p2->nTextBlockH
            CMessageBox_SetIconKind( h2, MBX_ICON_NONE )
            dim as long wNone, hNone
            CMessageBox_GetIdealSize( h2, wNone, hNone )
            dim as long textHNone = p2->nTextBlockH
            sD = "with icon " & str(wIcon) & "  without " & str(wNone) & _
                 "  iconW+gap=" & str(p2->nIconWidth + p2->nIconGap)
            ' The precondition, asserted rather than assumed: removing the icon widens the wrap
            ' box, so if the message were near the wrap limit it would re-flow and the delta
            ' would not be the icon block at all.
            MBXTest_Check( "the icon test message does not re-wrap between the two cases", _
                           (textHIcon = textHNone), sD & " textH " & str(textHIcon) & _
                           "/" & str(textHNone) )
            MBXTest_Check( "removing the icon gives back the cell AND the gap", _
                           (wIcon - wNone) = (p2->nIconWidth + p2->nIconGap), sD )

            CMessageBox_SetText( h2, "Short." )
            CMessageBox_SetIconKind( h2, MBX_ICON_NONE )
            CMessageBox_LayoutForTest( h2 )
            dim as RECT rcI2, rcT2
            CMessageBox_GetIconRect( h2, rcI2 )
            CMessageBox_GetTextRect( h2, rcT2 )
            sD = "icon " & MBXTest_RectStr(rcI2) & " text " & MBXTest_RectStr(rcT2)
            MBXTest_Check( "no icon = EMPTY rect, not a zero-width one", _
                           IsRectEmpty( @rcI2 ) <> 0, sD )
            MBXTest_Check( "no icon = text starts at the body padding", _
                           (rcT2.left = p2->nBodyPadX), sD )

            ' A glyph override is an unambiguous request for an icon and must beat ICON_NONE.
            CMessageBox_SetGlyph( h2, wchr(CMESSAGEBOX_GLYPH_INFO) )
            CMessageBox_LayoutForTest( h2 )
            CMessageBox_GetIconRect( h2, rcI2 )
            MBXTest_Check( "a glyph override turns the icon on even with kind NONE", _
                           IsRectEmpty( @rcI2 ) = 0, MBXTest_RectStr(rcI2) )

            ' One button, cancel unset -> that button. The single-button case is the one where
            ' "the last button" and "the only button" have to agree.
            MBXTest_Check( "one button, cancel unset, resolves to it", _
                           CMessageBox_GetCancelID(h2) = IDOK, _
                           "id=" & str(CMessageBox_GetCancelID(h2)) )

            DestroyWindow( h2 )
        end if
    end scope

    ' ---- 11. the footer can force the box WIDER than the wrap width ---------------------------------
    ' nMaxWidth constrains the TEXT, not the window. Three long captions must push the box out
    ' rather than being clipped -- that is the max() in LayoutBox, and it is an easy thing to
    ' write as a clamp by mistake.
    scope
        dim as HWND h3 = CMessageBox_Create( hWndParent )
        if h3 then
            CMessageBox_SetText( h3, "Hi." )
            CMessageBox_SetMaxWidth( h3, 200 )
            CMessageBox_SetMinWidth( h3, 0 )
            CMessageBox_AddButton( h3, "A considerably long caption", 1 )
            CMessageBox_AddButton( h3, "Another very long caption", 2 )
            CMessageBox_AddButton( h3, "And a third long one", 3 )
            CMessageBox_LayoutForTest( h3 )

            dim as long w3, h3h
            CMessageBox_GetIdealSize( h3, w3, h3h )
            dim as RECT rl
            CMessageBox_GetButtonRect( h3, 0, rl )
            sD = "ideal " & str(w3) & " maxWidth=200 firstBtn " & MBXTest_RectStr(rl)
            MBXTest_Check( "long buttons widen the box past the wrap width", (w3 > 200), sD )
            MBXTest_Check( "...and no button is pushed off the left edge", (rl.left >= 0), sD )
            DestroyWindow( h3 )
        end if
    end scope

    dim as string sSummary = "=== CMessageBox self-test: " & str(gMBX_TestCount) & " checks, " & _
                             str(gMBX_FailCount) & " failed ==="
    print sSummary
end sub
