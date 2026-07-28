''
''  PsNumericUpDown.inc  --  owner-drawn numeric up/down (spinner) with an editable value
''

#include once "PsNumericUpDown.bi"


function GetNumericUpDownPointer( byval hCtrl as HWND ) as PSNUMERICUPDOWN ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCtrl)
    if pWindow = 0 then return 0
    return cast( PSNUMERICUPDOWN ptr, pWindow->UserData(0) )
end function


' Forward declarations: the Create routine installs these on the embedded PsTextBox, and the
' WndProc and they call each other.
declare function PsNumericUpDown_WndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
declare sub      NUD_SyncChild( byval pNud as PSNUMERICUPDOWN ptr )
declare function NUD_StepBy( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND, byval delta as double ) as boolean
declare function NUD_HandleWheel( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND, byval nDelta as long ) as boolean


' ========================================================================================
' Format a value the way the control displays it. Deliberately NOT calling PsTextBox's own
' private PsTextBox_FormatNumeric: everything here is textually included into one module, so
' that private helper happens to be reachable, but reaching for it would couple this control
' to an implementation detail of a VENDORED file that is synced from elsewhere.
' ========================================================================================
private function NUD_FormatValue( byval nValue as double, byval nDecimalPlaces as long ) as DWSTRING
    dim as string fmt = "0"
    if nDecimalPlaces > 0 then fmt = fmt & "." & string(nDecimalPlaces, "0")
    return format( nValue, fmt )
end function


' ========================================================================================
' Do these two values differ ENOUGH to count as a change?
'
' Both operands have been through SnapToPlaces, so a real difference is at least one grid
' unit; anything smaller is binary-floating-point residue. A quarter of a grid unit is
' therefore comfortably clear of both -- and using the grid rather than a fixed epsilon is
' what keeps this correct at 0 decimal places (where 0.9 and 1.0 are the SAME displayed
' value would be wrong, and they differ by more than 0.25 so they are not).
' ========================================================================================
private function NUD_ValuesDiffer( _
            byval pNud as PSNUMERICUPDOWN ptr, _
            byval a as double, _
            byval b as double _
            ) as boolean

    if pNud = 0 then return (a <> b)
    dim as double tolerance = 0.25 / (10.0 ^ pNud->nDecimalPlaces)
    return ( abs(a - b) > tolerance )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
' Callers decide whether to honor that answer -- WM_LBUTTONUP and WM_KILLFOCUS deliberately
' do not (see NUD_MessageCallbackFunc).
' ========================================================================================
private function NUD_CallMessage( _
            byval pNud   as PSNUMERICUPDOWN ptr, _
            byval hCtrl  as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pNud = 0 then return false
    if pNud->MessageCallback = 0 then return false

    dim as PSNUMERICUPDOWN_MESSAGEINFO m
    m.hCtrl  = hCtrl
    m.uMsg   = uMsg
    m.wParam = wParam
    m.lParam = lParam
    return pNud->MessageCallback( @m )
end function


' ========================================================================================
' Push the value cell's colors into the PsTextBox for the control's current enabled state.
'
' The RichEdit draws the number, so these two colors have to live on the child -- but they
' are authored in OUR color struct, because a host should set every color of the control in
' one place. This is the one routine that bridges the two.
' ========================================================================================
private sub NUD_ApplyStateColors( byval pNud as PSNUMERICUPDOWN ptr )
    if pNud = 0 then exit sub
    if pNud->hTextBox = 0 then exit sub

    if pNud->isEnabled then
        PsTextBox_SetBackColor( pNud->hTextBox, pNud->colors.ValueBackColor )
        PsTextBox_SetForeColor( pNud->hTextBox, pNud->colors.ValueForeColor )
    else
        PsTextBox_SetBackColor( pNud->hTextBox, pNud->colors.ValueBackColorDisabled )
        PsTextBox_SetForeColor( pNud->hTextBox, pNud->colors.ValueForeColorDisabled )
    end if
end sub


' ========================================================================================
' Display a value and record it as the control's committed value.
'
' THE ORDER MATTERS. nValue is updated first so that anything the writes below trigger --
' a repaint deciding whether a button is at its limit, a host querying us from a callback --
' already sees the new value. PsTextBox_SetValue is silent (a programmatic setter), so this
' never notifies; the caller decides whether a change is worth reporting.
' ========================================================================================
private sub NUD_DisplayValue( byval pNud as PSNUMERICUPDOWN ptr, byval nValue as double )
    if pNud = 0 then exit sub

    pNud->nValue = nValue
    if pNud->hTextBox then
        ' No alignment work here. The centring is set ONCE at Create and survives every
        ' subsequent write -- PsTextBox's own harness asserts exactly that. An earlier version
        ' of this routine re-stamped it after each value change, which was both unnecessary
        ' and (as it turned out) ineffective: it was sending EM_SETPARAFORMAT itself, which
        ' TM_PLAINTEXT silently refuses.
        PsTextBox_SetValue( pNud->hTextBox, nValue )
    end if
    ' The buttons' at-a-limit appearance is derived from the value, so a repaint is not
    ' optional here even though the number itself is drawn by the child.
    if pNud->hWin then InvalidateRect( pNud->hWin, NULL, FALSE )
end sub


' ========================================================================================
' What is in the field RIGHT NOW, normalised.
'
' Read from the PsTextBox rather than from nValue, because the user may have typed something
' that has not been committed yet -- and stepping from what is on screen is what a user
' expects when they type 50 and then click "+". Out-of-range typing is clamped here, so a
' step always starts from a legal value.
' ========================================================================================
private function NUD_CurrentValue( byval pNud as PSNUMERICUPDOWN ptr ) as double
    if pNud = 0 then return 0
    if pNud->hTextBox = 0 then return pNud->nValue
    return pNud->ClampToRange( pNud->SnapToPlaces( PsTextBox_GetValue( pNud->hTextBox ) ) )
end function


' ========================================================================================
' Is the value hard against a limit? Used only to paint a button disabled.
'
' Answered from nValue -- the committed value -- and not from the live text, so painting
' never sends a WM_GETTEXT to a child window. The consequence is honest and small: while the
' user is mid-typing, the buttons reflect the last committed value rather than the digits on
' screen. They still step correctly from what was typed (see NUD_CurrentValue).
' ========================================================================================
private function NUD_AtMin( byval pNud as PSNUMERICUPDOWN ptr ) as boolean
    if pNud = 0 then return false
    return ( NUD_ValuesDiffer( pNud, pNud->nValue, pNud->nMin ) = false ) orelse (pNud->nValue < pNud->nMin)
end function

private function NUD_AtMax( byval pNud as PSNUMERICUPDOWN ptr ) as boolean
    if pNud = 0 then return false
    return ( NUD_ValuesDiffer( pNud, pNud->nValue, pNud->nMax ) = false ) orelse (pNud->nValue > pNud->nMax)
end function


' ========================================================================================
' Add delta to whatever is in the field, clamp, snap, display, and report the change.
'
' Returns TRUE if the value actually MOVED, which is what stops auto-repeat dead at a limit.
' Note the two different comparisons, and that they are asking different questions:
'   - "did it move"      compares against what was in the FIELD (so a step off uncommitted
'                        typed text is a move);
'   - "should we notify" compares against nValue, the last value the host was told about
'                        (so typing 50 and clicking "+" reports 51 once, not twice).
' The display is refreshed either way, which is also what normalises "5." to "5.00".
' ========================================================================================
private function NUD_StepBy( _
            byval pNud  as PSNUMERICUPDOWN ptr, _
            byval hCtrl as HWND, _
            byval delta as double _
            ) as boolean

    if pNud = 0 then return false

    dim as double nCurrent = NUD_CurrentValue( pNud )
    dim as double nNew = pNud->ClampToRange( pNud->SnapToPlaces( nCurrent + delta ) )
    dim as double nOld = pNud->nValue

    dim as boolean bMoved = NUD_ValuesDiffer( pNud, nNew, nCurrent )
    dim as boolean bNotify = NUD_ValuesDiffer( pNud, nNew, nOld )

    NUD_DisplayValue( pNud, nNew )

    if bNotify andalso (pNud->ValueChangedCallback <> 0) then
        pNud->ValueChangedCallback( hCtrl, nNew )
    end if

    return bMoved
end function


' ========================================================================================
' Commit whatever the user typed: clamp it, snap it, reformat it, and report it once.
'
' Reached from ENTER and from focus loss. By the time PsTextBox's FocusCallback runs, its
' numeric mode has already rewritten "5." as "5.00" -- but it knows nothing about our range,
' so the clamp is ours and has to happen here.
' ========================================================================================
private sub NUD_CommitTypedValue( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND )
    if pNud = 0 then exit sub

    dim as double nOld = pNud->nValue
    ' An empty field reads as 0 and is then clamped into the range, so the control can never
    ' be left blank -- there is no "no value" state to represent.
    dim as double nNew = NUD_CurrentValue( pNud )

    NUD_DisplayValue( pNud, nNew )

    if NUD_ValuesDiffer( pNud, nNew, nOld ) andalso (pNud->ValueChangedCallback <> 0) then
        pNud->ValueChangedCallback( hCtrl, nNew )
    end if
end sub


' ========================================================================================
' Wheel: one increment per notch, accumulating sub-notch deltas.
'
' The accumulation is the scrollbar siblings' rule -- a precision touchpad sends deltas well
' under one 120 notch and truncating each one would ignore a slow swipe entirely.
'
' SPI_GETWHEELSCROLLLINES is deliberately NOT consulted, which is where this parts company
' with PsVScrollBar/PsHScrollBar: "how many LINES does one notch scroll" is a question about
' scrolling a view, and this is a value, not a view. One notch = one increment, and the host
' controls the feel through SetIncrement.
' ========================================================================================
private function NUD_HandleWheel( _
            byval pNud   as PSNUMERICUPDOWN ptr, _
            byval hCtrl  as HWND, _
            byval nDelta as long _
            ) as boolean

    if pNud = 0 then return false
    if pNud->isEnabled = false then return false

    pNud->nWheelAccum += nDelta
    dim as long nNotches = pNud->nWheelAccum \ WHEEL_DELTA
    if nNotches = 0 then return true          ' consumed, but not yet a whole notch
    pNud->nWheelAccum -= nNotches * WHEEL_DELTA

    ' Positive delta = wheel away from the user = increase.
    NUD_StepBy( pNud, hCtrl, nNotches * pNud->nIncrement )
    return true
end function


' ========================================================================================
' Which part is under a CLIENT-coordinate point?
'
' The value cell is reported for completeness, but the control never acts on it: that area
' is covered by the PsTextBox child, which gets its own mouse messages and never routes them
' here. So in practice only MINUS and PLUS are ever reachable through a real mouse message.
' ========================================================================================
private function NUD_HitTestPart( byval pNud as PSNUMERICUPDOWN ptr, byval pt as POINT ) as long
    if pNud = 0 then return NUD_PART_NONE
    if pNud->bLayoutDirty then pNud->LayoutControl()

    if PtInRect( @pNud->rcMinus, pt ) then return NUD_PART_MINUS
    if PtInRect( @pNud->rcPlus, pt ) then return NUD_PART_PLUS
    if PtInRect( @pNud->rcValue, pt ) then return NUD_PART_VALUE
    return NUD_PART_NONE
end function


' ========================================================================================
' Auto-repeat control. The delay/interval transition needs no extra state: every tick
' re-arms the timer at the INTERVAL, and SetTimer with an existing id replaces the timer
' rather than adding a second one. So the first tick arrives after the delay and all the
' rest after the interval.
' ========================================================================================
private sub NUD_StartRepeat( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND )
    if pNud = 0 then exit sub
    if (pNud->nRepeatDelayMs <= 0) orelse (pNud->nRepeatIntervalMs <= 0) then exit sub
    SetTimer( hCtrl, IDT_CNUMERICUPDOWN_REPEAT, pNud->nRepeatDelayMs, NULL )
    pNud->repeatTimerOn = true
end sub

private sub NUD_StopRepeat( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND )
    if pNud = 0 then exit sub
    if pNud->repeatTimerOn then
        KillTimer( hCtrl, IDT_CNUMERICUPDOWN_REPEAT )
        pNud->repeatTimerOn = false
    end if
end sub


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub NUD_ClearHot( byval pNud as PSNUMERICUPDOWN ptr, byval hCtrl as HWND )
    if pNud = 0 then exit sub
    if pNud->hotTimerOn then
        KillTimer( hCtrl, IDT_CNUMERICUPDOWN_HOTTRACK )
        pNud->hotTimerOn = false
    end if
    if pNud->hotPart <> NUD_PART_NONE then
        pNud->hotPart = NUD_PART_NONE
        InvalidateRect( hCtrl, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Keep the PsTextBox child sitting exactly on rcValue.
'
' Guarded by a comparison so it is a no-op in steady state: this runs from WM_PAINT as well
' as WM_SIZE, and an unconditional SetWindowPos from inside a paint would invite a repaint
' loop. In practice WM_SIZE has already done the work and the comparison short-circuits.
'
' A client too narrow to hold both buttons collapses rcValue to zero width; the child is
' hidden rather than handed a degenerate rect, because a zero-width RichEdit is a different
' kind of problem from an absent one.
' ========================================================================================
private sub NUD_SyncChild( byval pNud as PSNUMERICUPDOWN ptr )
    if pNud = 0 then exit sub
    if pNud->hTextBox = 0 then exit sub
    if pNud->bLayoutDirty then pNud->LayoutControl()

    dim as RECT rc = pNud->rcValue
    dim as long cx = rc.right - rc.left
    dim as long cy = rc.bottom - rc.top

    if (cx <= 0) orelse (cy <= 0) then
        ShowWindow( pNud->hTextBox, SW_HIDE )
        exit sub
    end if

    dim as RECT rcNow
    GetWindowRect( pNud->hTextBox, @rcNow )
    MapWindowPoints( HWND_DESKTOP, pNud->hWin, cast(POINT ptr, @rcNow), 2 )
    if EqualRect( @rcNow, @rc ) = 0 then
        SetWindowPos( pNud->hTextBox, 0, rc.left, rc.top, cx, cy, SWP_NOZORDER or SWP_NOACTIVATE )
    end if
    ShowWindow( pNud->hTextBox, SW_SHOW )
end sub


' ========================================================================================
' Draw the frame, the two button cells, the dividers and the two glyphs.
'
' IT NEVER DRAWS THE NUMBER. The RichEdit inside the PsTextBox paints that, over rcValue,
' after we are done -- and because the container is WS_CLIPCHILDREN, the child's rect is
' excluded from our DC anyway, so anything drawn there would be discarded rather than
' fighting for the pixels.
'
' THE PAINT ORDER IS LOad-BEARING, and it is what keeps the corners round:
'
'   1. the client, in BackColor                 -- shows through OUTSIDE the rounded corners
'   2. the whole frame, as a filled ROUND rect  -- this is what makes the corners round; it
'                                                  is also the value cell's background, for
'                                                  the strip above and below the child
'   3. each button cell, as a filled ROUND rect EXTENDED INWARD past the divider. A plain
'      rectangular fill would square off the two corners it covers. Extending it inward puts
'      its own inner rounding somewhere harmless -- past the divider, inside the child's
'      clipped-away rect -- so only the outer, wanted rounding survives.
'   4. the dividers, then the glyphs, as filled RECTANGLES. Learnings.md: an antialiased 1px
'      axis-aligned rule comes out grey and blurry, and a stated extent sidesteps both the
'      smoothing and the endpoint-inclusion question. Reserve AA for curves.
'   5. the frame OUTLINE last, over everything -- PaintRoundOutline, not
'      PaintRoundBorderRect, because a filled round rect here would erase steps 2-4.
'
' Curvature is a DIAMETER in this API (PsBufferPaint halves it to a GDI+ radius internally),
' so nCornerRadius is doubled at every call site here.
' ========================================================================================
private sub PsNumericUpDown_RenderDefault( _
            byval pNud as PSNUMERICUPDOWN ptr, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSNUMERICUPDOWN_PAINTINFO ptr _
            )

    if (pNud = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long cw = p->rcFrame.right - p->rcFrame.left
    dim as long ch = p->rcFrame.bottom - p->rcFrame.top
    if (cw <= 0) orelse (ch <= 0) then exit sub

    dim as long ell = pNud->nCornerRadius * 2

    ' ---- 2. the frame's fill ------------------------------------------------------------
    dim as COLORREF clrBase = pNud->colors.ValueBackColor
    if p->isEnabled = false then clrBase = pNud->colors.ValueBackColorDisabled
    dim as RECT rcFrame = p->rcFrame
    b->SetBackColor( clrBase )
    b->PaintRoundRect( @rcFrame, ell )

    ' ---- 3. the two button cells --------------------------------------------------------
    ' Precedence: disabled (control-wide, or this button at its limit) > pressed > hot > idle.
    ' A button at a limit is drawn disabled while the rest of the control stays live, which is
    ' the only per-part state in here.
    dim as long ext = pNud->nCornerRadius + pNud->nDividerThick + 1

    for iPart as long = NUD_PART_MINUS to NUD_PART_PLUS step (NUD_PART_PLUS - NUD_PART_MINUS)
        dim as RECT rcCell, rcFill
        dim as boolean bAtLimit
        if iPart = NUD_PART_MINUS then
            rcCell = p->rcMinus
            bAtLimit = p->bAtMin
            SetRect( @rcFill, rcCell.left, rcCell.top, rcCell.right + ext, rcCell.bottom )
        else
            rcCell = p->rcPlus
            bAtLimit = p->bAtMax
            SetRect( @rcFill, rcCell.left - ext, rcCell.top, rcCell.right, rcCell.bottom )
        end if

        dim as COLORREF clrBack, clrGlyph
        if (p->isEnabled = false) orelse bAtLimit then
            clrBack  = pNud->colors.ButtonBackColorDisabled
            clrGlyph = pNud->colors.GlyphColorDisabled
        elseif p->pressedPart = iPart then
            clrBack  = pNud->colors.ButtonBackColorPressed
            clrGlyph = pNud->colors.GlyphColorPressed
        elseif p->hotPart = iPart then
            clrBack  = pNud->colors.ButtonBackColorHot
            clrGlyph = pNud->colors.GlyphColorHot
        else
            clrBack  = pNud->colors.ButtonBackColor
            clrGlyph = pNud->colors.GlyphColor
        end if

        b->SetBackColor( clrBack )
        b->PaintRoundRect( @rcFill, ell )

        ' ---- 4b. the glyph ---------------------------------------------------------------
        b->SetBackColor( clrGlyph )
        if iPart = NUD_PART_MINUS then
            dim as RECT rcBar = p->rcMinusBar
            b->PaintRect( @rcBar )
        else
            dim as RECT rcBarH = p->rcPlusBarH
            dim as RECT rcBarV = p->rcPlusBarV
            b->PaintRect( @rcBarH )
            b->PaintRect( @rcBarV )
        end if
    next

    ' ---- 4a. the dividers ---------------------------------------------------------------
    if pNud->nDividerThick > 0 then
        dim as COLORREF clrDiv = pNud->colors.DividerColor
        if p->isEnabled = false then clrDiv = pNud->colors.DividerColorDisabled
        dim as RECT rcD1 = p->rcDiv1
        dim as RECT rcD2 = p->rcDiv2
        b->SetBackColor( clrDiv )
        b->PaintRect( @rcD1 )
        b->PaintRect( @rcD2 )
    end if

    ' ---- 5. the frame outline -----------------------------------------------------------
    ' There is no separate focus ring and therefore no reserved band: focus simply recolors
    ' this stroke, which is why the control's ideal size does not change when focus arrives.
    if pNud->nBorderThick > 0 then
        dim as COLORREF clrBorder = pNud->colors.BorderColor
        if p->isEnabled = false then
            clrBorder = pNud->colors.BorderColorDisabled
        elseif p->isFocused then
            clrBorder = pNud->colors.FocusBorderColor
        end if
        dim as RECT rcOutline = p->rcFrame
        b->SetPenColor( clrBorder )
        b->PaintRoundOutline( @rcOutline, ell, pNud->nBorderThick )
    end if
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function PsNumericUpDown_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hwnd)
    if pNud = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy -- and
    ' make sure the child is standing where the layout says it should.
    if pNud->bLayoutDirty then pNud->LayoutControl()
    NUD_SyncChild( pNud )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pNud->colors.BackColor )
    b.PaintClientRect()

    dim p as PSNUMERICUPDOWN_PAINTINFO
    p.hCtrl       = hwnd
    p.b           = @b
    GetClientRect( hwnd, @p.rcClient )
    p.rcFrame     = pNud->rcFrame
    p.rcMinus     = pNud->rcMinus
    p.rcDiv1      = pNud->rcDiv1
    p.rcValue     = pNud->rcValue
    p.rcDiv2      = pNud->rcDiv2
    p.rcPlus      = pNud->rcPlus
    p.rcMinusBar  = pNud->rcMinusBar
    p.rcPlusBarH  = pNud->rcPlusBarH
    p.rcPlusBarV  = pNud->rcPlusBarV
    p.hotPart     = pNud->hotPart
    ' "The press is still armed" is exactly "pressed AND the cursor is still on that button"
    ' -- hover tracking maintains hotPart through the capture, so there is no second flag.
    p.pressedPart = NUD_PART_NONE
    if (pNud->pressedPart <> NUD_PART_NONE) andalso (pNud->pressedPart = pNud->hotPart) then
        p.pressedPart = pNud->pressedPart
    end if
    p.isEnabled   = pNud->isEnabled
    p.isFocused   = pNud->isFocused
    p.bAtMin      = NUD_AtMin( pNud )
    p.bAtMax      = NUD_AtMax( pNud )
    p.nValue      = pNud->nValue

    if pNud->PaintCallback then
        pNud->PaintCallback( @p )
    else
        PsNumericUpDown_RenderDefault( pNud, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' THE THREE HOOKS INTO THE EMBEDDED PsTextBox.
'
' PsTextBox reports events against ITS OWN handle, so each of these starts by walking up one
' level: GetParent(hTextBox) is this control. That is the whole mapping -- there is no
' registry and no back-pointer to keep in sync.
' ========================================================================================

' The value field gained or lost keyboard focus.
'
' FOCUS LOSS IS WHAT COMMITS A TYPED VALUE, and by the time this runs PsTextBox's numeric mode
' has already reformatted the buffer ("5." -> "5.00"). It knows nothing about our range,
' though, so the clamp happens here.
private sub NUD_OnTextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    dim as HWND hCtrl = GetParent( hTextBox )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer( hCtrl )
    if pNud = 0 then exit sub

    pNud->isFocused = bGotFocus
    InvalidateRect( hCtrl, NULL, FALSE )

    if bGotFocus = false then NUD_CommitTypedValue( pNud, hCtrl )
end sub


' ENTER in the value field. PsTextBox swallows the keystroke itself (no newline, no beep) and
' reports it here; committing is the natural meaning of ENTER in a spinner.
private sub NUD_OnTextBoxEnter( byval hTextBox as HWND )
    dim as HWND hCtrl = GetParent( hTextBox )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer( hCtrl )
    if pNud = 0 then exit sub

    NUD_CommitTypedValue( pNud, hCtrl )
end sub


' Every key, wheel, mouse and focus message the RichEdit sees passes through here BEFORE
' PsTextBox acts on it. Two jobs:
'
'   1. RELAY it to our own host as if it had arrived at this control, so a host sees one
'      message stream and never has to know a RichEdit is in there.
'   2. CLAIM the four navigation keys and the wheel. They are free to take: a single-line
'      RichEdit does nothing useful with Up/Down/PgUp/PgDn, and PsTextBox only handles
'      WM_MOUSEWHEEL in multiline mode.
'
' Returning TRUE makes PsTextBox's subclass return 0 without calling the RichEdit, which is
' exactly "consumed". Home and End are deliberately NOT claimed -- they move the caret to the
' start and end of the text, and stealing them for min/max would break ordinary editing in a
' field the user can type into.
private function NUD_OnTextBoxMessage( byval m as PSTEXTBOX_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false

    dim as HWND hCtrl = GetParent( m->hTextBox )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer( hCtrl )
    if pNud = 0 then return false

    ' The host sees it first. WM_KILLFOCUS is relayed but NOT vetoable: it is what commits a
    ' typed value, and a suppressed commit would leave the control displaying a number it has
    ' not accepted.
    dim as boolean bClaimed = NUD_CallMessage( pNud, hCtrl, m->uMsg, m->wParam, m->lParam )
    if bClaimed andalso (m->uMsg <> WM_KILLFOCUS) then return true

    if pNud->isEnabled = false then return false

    select case m->uMsg

    case WM_KEYDOWN
        select case m->wParam
        case VK_UP
            NUD_StepBy( pNud, hCtrl, pNud->nIncrement )
            return true
        case VK_DOWN
            NUD_StepBy( pNud, hCtrl, -pNud->nIncrement )
            return true
        case VK_PRIOR
            NUD_StepBy( pNud, hCtrl, pNud->nLargeIncrement )
            return true
        case VK_NEXT
            NUD_StepBy( pNud, hCtrl, -pNud->nLargeIncrement )
            return true
        end select

    case WM_MOUSEWHEEL
        ' The delta is a SIGNED word in the high word (Learnings.md).
        return NUD_HandleWheel( pNud, hCtrl, cast(short, hiword(m->wParam)) )

    end select

    return false
end function


' ========================================================================================
' PsNumericUpDown container window procedure.
'
' It owns the chrome, the two buttons and the child's geometry. The value field's own
' messages never reach here -- they go to the RichEdit and come back through
' NUD_OnTextBoxMessage.
'
' THE CONTROL TAKES MOUSE CAPTURE on a button press, and pays the family's full price for it:
' snapshot before releasing (ReleaseCapture sends WM_CAPTURECHANGED synchronously, so reading
' the flag afterwards always finds it cleared -- Learnings.md), release on the up-message
' before any callback runs, WM_CAPTURECHANGED cancels the press, WM_DESTROY releases, and the
' message callback's result is IGNORED for WM_LBUTTONUP so nothing can strand it.
'
' BUT NOTE WHAT THE CAPTURE IS FOR HERE, because it is NOT the press/cancel gesture its
' siblings take it for. A SPINNER STEPS ON THE BUTTON-DOWN, not on the release -- it has to,
' because auto-repeat has to start from somewhere and a first step deferred to the release
' would arrive after the repeats it precedes. So sliding off a button does not undo the step
' that already happened; what capture buys is (a) the repeat stops the moment the cursor
' leaves the button and resumes if it comes back, and (b) the up-message is guaranteed to
' arrive here, wherever the cursor has wandered to, so the repeat timer can never outlive the
' gesture. Every real spinner behaves this way.
'
' CS_DBLCLKS IS DELIBERATELY NOT SET, which is PsToggle's and PsIconPanel's call rather than
' PsSelectBar's: with it, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD
' OF a second WM_LBUTTONDOWN, and a user double-clicking "+" to add two would get one.
' ========================================================================================
function PsNumericUpDown_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsNumericUpDown_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pNud then
            pNud->Refresh()
            NUD_SyncChild( pNud )
        end if
        return 0

    case WM_SETFOCUS
        ' A host that reaches for SetFocus(hCtrl) means "put the caret in the field". The
        ' container itself is not a tabstop and has nothing to do with focus, so hand it
        ' straight down rather than swallowing it.
        if pNud andalso pNud->hTextBox then
            dim as HWND hEdit = PsTextBox_GetRichEditHandle( pNud->hTextBox )
            if hEdit then
                SetFocus( hEdit )
                return 0
            end if
        end if

    case WM_TIMER
        if pNud = 0 then exit select

        if wParam = IDT_CNUMERICUPDOWN_HOTTRACK then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot state ourselves. Note this poll runs even
            ' during a press: under capture the cursor legitimately roams outside, and the
            ' pressed button's appearance follows hotPart.
            dim as RECT rcClient
            GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                if pNud->pressedPart = NUD_PART_NONE then
                    NUD_ClearHot( pNud, hwnd )
                elseif pNud->hotPart <> NUD_PART_NONE then
                    pNud->hotPart = NUD_PART_NONE
                    NUD_StopRepeat( pNud, hwnd )
                    InvalidateRect( hwnd, NULL, FALSE )
                end if
            end if
            return 0
        end if

        if wParam = IDT_CNUMERICUPDOWN_REPEAT then
            ' Re-arm at the INTERVAL. The first tick came after the delay; SetTimer with an
            ' existing id replaces the timer, so this is the whole delay->interval
            ' transition and it needs no extra state.
            SetTimer( hwnd, IDT_CNUMERICUPDOWN_REPEAT, pNud->nRepeatIntervalMs, NULL )

            ' Repeat only while the button is still held AND the cursor is still on it.
            if (pNud->pressedPart = NUD_PART_NONE) orelse (pNud->pressedPart <> pNud->hotPart) then
                NUD_StopRepeat( pNud, hwnd )
                return 0
            end if

            dim as double delta = pNud->nIncrement
            if pNud->pressedPart = NUD_PART_MINUS then delta = -delta
            ' Reaching a limit stops the repeat dead rather than spinning against the clamp.
            if NUD_StepBy( pNud, hwnd, delta ) = false then NUD_StopRepeat( pNud, hwnd )
            return 0
        end if

    case WM_MOUSEMOVE
        if pNud = 0 then exit select
        if pNud->isEnabled = false then exit select

        ' Under capture we receive moves from anywhere on the desktop, so "where is the
        ' mouse" must be asked of the cursor position and never assumed from the message.
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        dim as long newHot = NUD_HitTestPart( pNud, pt )
        ' Only the buttons light up; the value cell belongs to the child.
        if newHot = NUD_PART_VALUE then newHot = NUD_PART_NONE

        if newHot <> NUD_PART_NONE then
            ' Track so we get the eventual WM_MOUSELEAVE.
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits
            ' quickly (or onto the child in the middle), which would strand a button hot.
            if pNud->hotTimerOn = false then
                SetTimer( hwnd, IDT_CNUMERICUPDOWN_HOTTRACK, PSNUMERICUPDOWN_HOTTRACK_MS, NULL )
                pNud->hotTimerOn = true
            end if
        end if

        if newHot <> pNud->hotPart then
            pNud->hotPart = newHot
            ' Sliding off the held button suspends the repeat; sliding back on resumes it,
            ' after the full delay again so a wobbling cursor cannot machine-gun the value.
            if pNud->pressedPart <> NUD_PART_NONE then
                if pNud->pressedPart = newHot then
                    NUD_StartRepeat( pNud, hwnd )
                else
                    NUD_StopRepeat( pNud, hwnd )
                end if
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pNud then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for
            ' when this message never arrives).
            if pNud->pressedPart = NUD_PART_NONE then
                NUD_ClearHot( pNud, hwnd )
            else
                if pNud->hotPart <> NUD_PART_NONE then
                    pNud->hotPart = NUD_PART_NONE
                    NUD_StopRepeat( pNud, hwnd )
                    InvalidateRect( hwnd, NULL, FALSE )
                end if
            end if
            if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pNud = 0 then return 0
        ' A disabled window receives no mouse input at all (SetEnabled calls EnableWindow),
        ' so this is belt-and-braces against a host that disabled us by some other route.
        if pNud->isEnabled = false then return 0

        ' Asked of the cursor rather than unpacked from lParam, so this reads identically to
        ' the WM_MOUSEMOVE handler and needs no windowsx.bi macro.
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        dim as long part = NUD_HitTestPart( pNud, pt )
        if (part <> NUD_PART_MINUS) andalso (part <> NUD_PART_PLUS) then
            if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0
            exit select
        end if

        ' Put the caret in the value field so the arrows and the wheel work straight away and
        ' the frame lights up -- but WITHOUT selecting the text, which would leave the number
        ' highlighted after every click of "+". A Tab arrival still selects.
        '
        ' Toggling the PsTextBox flag around the SetFocus is safe (rather than a timing
        ' gamble) because PsTextBox applies select-on-focus synchronously inside its
        ' WM_SETFOCUS handler -- the whole effect is over before SetFocus returns.
        if pNud->hTextBox then
            dim as HWND hEdit = PsTextBox_GetRichEditHandle( pNud->hTextBox )
            if hEdit then
                PsTextBox_SetSelectOnFocus( pNud->hTextBox, false )
                SetFocus( hEdit )
                PsTextBox_SetSelectOnFocus( pNud->hTextBox, true )
            end if
        end if

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0

        SetCapture( hwnd )
        pNud->pressedPart = part
        pNud->hotPart     = part
        InvalidateRect( hwnd, NULL, FALSE )

        ' Step immediately, then let the repeat timer take over after the delay.
        dim as double delta = pNud->nIncrement
        if part = NUD_PART_MINUS then delta = -delta
        if NUD_StepBy( pNud, hwnd, delta ) then NUD_StartRepeat( pNud, hwnd )
        return 0

    case WM_LBUTTONUP
        if pNud = 0 then return 0

        ' Release capture unconditionally, before any callback runs. Nothing below -- not a
        ' callback, not a host handler -- gets the chance to leave it held. (There is no
        ' "was it pressed" snapshot to take here, unlike the press/cancel siblings: the step
        ' already happened on the down-message, so the up has no value decision to make.)
        if GetCapture() = hwnd then ReleaseCapture()

        NUD_StopRepeat( pNud, hwnd )
        pNud->CancelPress()
        InvalidateRect( hwnd, NULL, FALSE )

        ' The result is deliberately IGNORED here (see NUD_MessageCallbackFunc).
        NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam )
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell).
        ' The gesture is over; forget it, stop repeating and repaint.
        if pNud then
            pNud->CancelPress()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_MOUSEWHEEL
        ' Wheel messages go to the FOCUSED window, so most of them arrive at the RichEdit and
        ' come back through NUD_OnTextBoxMessage. This path catches the other case: Windows'
        ' "scroll inactive windows" setting (on by default) delivers the wheel to whatever is
        ' under the cursor, which is this container when the pointer is over a button.
        if pNud then
            if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0
            if NUD_HandleWheel( pNud, hwnd, cast(short, hiword(wParam)) ) then return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on. The value field has its own context menu (PsTextBox's);
        ' the buttons have none, and inventing one here is the host's business.
        if pNud then
            if NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsNumericUpDown_SetEnabled still gets the greyed
        ' rendering rather than a control that looks live but eats input.
        if pNud then
            pNud->isEnabled = (wParam <> 0)
            if pNud->isEnabled = false then
                pNud->hotPart = NUD_PART_NONE
                NUD_StopRepeat( pNud, hwnd )
                pNud->CancelPress()
            end if
            NUD_ApplyStateColors( pNud )
            InvalidateRect( hwnd, NULL, TRUE )
            NUD_CallMessage( pNud, hwnd, uMsg, wParam, lParam )
        end if

    case WM_DESTROY
        if pNud then
            ' Don't let either timer outlive the window...
            if pNud->hotTimerOn then
                KillTimer( hwnd, IDT_CNUMERICUPDOWN_HOTTRACK )
                pNud->hotTimerOn = false
            end if
            NUD_StopRepeat( pNud, hwnd )
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pNud->CancelPress()
        end if

    case WM_NCDESTROY
        ' Delete the stored control data. The PsTextBox child destroys itself with its own
        ' window (it is our child, so it goes first); the font is caller-owned and is never
        ' deleted here.
        if pNud then delete(pNud)

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' PsNumericUpDown_Create
' ========================================================================================
function PsNumericUpDown_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_EX_CONTROLPARENT is LOAD-BEARING, not decoration. PsTextBox handles VK_TAB itself by
    ' walking GetAncestor(GA_ROOT) + GetNextDlgTabItem, and that walk only sees through a
    ' container that declares itself one. Without it, tabbing into or out of the value field
    ' stops working the moment the textbox is nested inside this control.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsNumericUpDown_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_CONTROLPARENT _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header.

    dim as PSNUMERICUPDOWN ptr pNud = new PSNUMERICUPDOWN
    pNud->hWin     = hCtrl
    pNud->idc_Ctrl = CtrlID

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule).
    '
    ' nBorderThick and nDividerThick are NOT scaled -- a hairline should stay a hairline,
    ' PsMenuBar's nSeparatorThickness rule. nGlyphThick IS scaled, and that asymmetry is
    ' deliberate: the border and divider are rules, while the glyph bars are the strokes of an
    ' icon, and scaling an icon's length but not its weight makes it thinner relative to
    ' itself on every step up in DPI. At 1.75x the unscaled version was 18 pixels long and 1
    ' thick, which read as a thread rather than a minus sign.
    '
    ' ScaleY for the thickness because the "-" bar's thickness is a vertical extent; ScaleX
    ' and ScaleY agree in practice, and picking the axis the primary glyph is measured on
    ' keeps the choice explicable. Floored at 1 so a fractional scale can never round a
    ' visible bar away to nothing.
    pNud->nButtonWidth   = pWindow->ScaleX( CNUD_DEFAULT_BUTTONW )
    pNud->nValuePadLeft  = pWindow->ScaleX( CNUD_DEFAULT_VALUEPAD )
    pNud->nValuePadRight = pWindow->ScaleX( CNUD_DEFAULT_VALUEPAD )
    pNud->nVertPadding   = pWindow->ScaleY( CNUD_DEFAULT_VERTPAD )
    pNud->nCornerRadius  = pWindow->ScaleX( CNUD_DEFAULT_CORNERRADIUS )
    pNud->nGlyphLength   = pWindow->ScaleX( CNUD_DEFAULT_GLYPHLENGTH )
    pNud->nGlyphThick    = pWindow->ScaleY( CNUD_DEFAULT_GLYPHTHICK )
    if pNud->nGlyphThick < 1 then pNud->nGlyphThick = 1

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the pointer BEFORE creating the child: the child's creation sends messages to
    ' this window, and a handler that found a null pointer would silently do nothing.
    pWindow->UserData(0) = cast(LONG_PTR, pNud)

    ' ---- the value field ----------------------------------------------------------------
    ' Borderless, because THIS control owns every pixel of chrome; numeric, because that is
    ' the whole reason for embedding a PsTextBox rather than drawing the number ourselves.
    pNud->hTextBox = PsTextBox_Create( hCtrl, CtrlID )
    if pNud->hTextBox then
        PsTextBox_SetBorderWidth( pNud->hTextBox, 0 )
        PsTextBox_SetNumericMode( pNud->hTextBox, true )
        PsTextBox_SetDecimalPlaces( pNud->hTextBox, pNud->nDecimalPlaces )
        ' Never show an empty box: with a range there is no "no value" state to represent,
        ' so an empty buffer displays the formatted zero instead of the (unset) cue banner.
        PsTextBox_SetZeroWhenEmpty( pNud->hTextBox, true )
        ' Tab in and the whole number is selected, so the first keystroke replaces it. A
        ' button click deliberately bypasses this -- see WM_LBUTTONDOWN.
        PsTextBox_SetSelectOnFocus( pNud->hTextBox, true )
        PsTextBox_SetMargins( pNud->hTextBox, pNud->nValuePadLeft, pNud->nValuePadRight )
        ' Centred, which is the whole reason PsTextBox grew an alignment setter. It is set
        ' HERE, while the buffer is still empty, and that timing is not incidental: applying
        ' alignment rewrites the buffer and discards the undo history, so a control that has
        ' been typed into pays for it. At Create there is nothing to pay.
        PsTextBox_SetTextAlign( pNud->hTextBox, TXT_ALIGN_CENTER )
        NUD_ApplyStateColors( pNud )

        PsTextBox_SetFocusCallback( pNud->hTextBox, @NUD_OnTextBoxFocus )
        PsTextBox_SetEnterPressedCallback( pNud->hTextBox, @NUD_OnTextBoxEnter )
        PsTextBox_SetMessageCallback( pNud->hTextBox, @NUD_OnTextBoxMessage )

        PsTextBox_SetValue( pNud->hTextBox, pNud->nValue )
        ShowWindow( pNud->hTextBox, SW_SHOW )
    end if

    function = hCtrl

end function


function PsNumericUpDown_GetTextBoxHandle( byval hCtrl as HWND ) as HWND
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->hTextBox
end function

' Focus sits on the RichEdit two levels down, so a plain GetFocus() = hCtrl is always false.
function PsNumericUpDown_HasFocus( byval hCtrl as HWND ) as boolean
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return false
    if pNud->hTextBox = 0 then return false
    return PsTextBox_HasFocus( pNud->hTextBox )
end function


' ========================================================================================
' Value.  Every setter here is SILENT -- see NUD_ValueChangedCallbackSub.
' ========================================================================================
function PsNumericUpDown_GetValue( byval hCtrl as HWND ) as double
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nValue
end function

sub PsNumericUpDown_SetValue( byval hCtrl as HWND, byval nValue as double )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    NUD_DisplayValue( pNud, pNud->ClampToRange( pNud->SnapToPlaces( nValue ) ) )
end sub

sub PsNumericUpDown_GetRange( byval hCtrl as HWND, byref nMin as double, byref nMax as double )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then
        nMin = 0 : nMax = 0
        exit sub
    end if
    nMin = pNud->nMin
    nMax = pNud->nMax
end sub

' An inverted pair is swapped rather than rejected: the alternative is a control whose
' ClampToRange can never be satisfied, and every subsequent step would pin to nMin.
sub PsNumericUpDown_SetRange( byval hCtrl as HWND, byval nMin as double, byval nMax as double )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub

    if nMin > nMax then swap nMin, nMax
    pNud->nMin = nMin
    pNud->nMax = nMax
    ' Re-clamp what is already displayed -- silently, like every setter here.
    NUD_DisplayValue( pNud, pNud->ClampToRange( pNud->nValue ) )
end sub

function PsNumericUpDown_GetIncrement( byval hCtrl as HWND ) as double
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nIncrement
end function

sub PsNumericUpDown_SetIncrement( byval hCtrl as HWND, byval nIncrement as double )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->nIncrement = abs(nIncrement)
end sub

function PsNumericUpDown_GetLargeIncrement( byval hCtrl as HWND ) as double
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nLargeIncrement
end function

sub PsNumericUpDown_SetLargeIncrement( byval hCtrl as HWND, byval nIncrement as double )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->nLargeIncrement = abs(nIncrement)
end sub

function PsNumericUpDown_GetDecimalPlaces( byval hCtrl as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nDecimalPlaces
end function

' Changing the grid re-snaps the current value onto it, so 16.25 at 2 places becomes 16 at 0.
' The PsTextBox is told too: it is what refuses a typed decimal separator at 0 places.
sub PsNumericUpDown_SetDecimalPlaces( byval hCtrl as HWND, byval nPlaces as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nPlaces < 0 then nPlaces = 0

    pNud->nDecimalPlaces = nPlaces
    if pNud->hTextBox then PsTextBox_SetDecimalPlaces( pNud->hTextBox, nPlaces )
    NUD_DisplayValue( pNud, pNud->ClampToRange( pNud->SnapToPlaces( pNud->nValue ) ) )
end sub

' The programmatic door to exactly what the buttons do -- minus the notification, because
' this is a programmatic setter (the family rule).
sub PsNumericUpDown_StepUp( byval hCtrl as HWND )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    NUD_DisplayValue( pNud, pNud->ClampToRange( pNud->SnapToPlaces( NUD_CurrentValue(pNud) + pNud->nIncrement ) ) )
end sub

sub PsNumericUpDown_StepDown( byval hCtrl as HWND )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    NUD_DisplayValue( pNud, pNud->ClampToRange( pNud->SnapToPlaces( NUD_CurrentValue(pNud) - pNud->nIncrement ) ) )
end sub

function PsNumericUpDown_GetReadOnly( byval hCtrl as HWND ) as boolean
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return false
    return pNud->bReadOnly
end function

' Stops TYPING only. The buttons, the arrows and the wheel keep working -- "you may not type
' an arbitrary number here" is a different statement from "this control is inert".
sub PsNumericUpDown_SetReadOnly( byval hCtrl as HWND, byval bReadOnly as boolean )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->bReadOnly = bReadOnly
    if pNud->hTextBox then PsTextBox_SetReadOnly( pNud->hTextBox, bReadOnly )
end sub


' ========================================================================================
' State and behavior
' ========================================================================================
function PsNumericUpDown_GetEnabled( byval hCtrl as HWND ) as boolean
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return false
    return pNud->isEnabled
end function

' EnableWindow on BOTH windows, not a cosmetic flag. Disabling only the container would leave
' the PsTextBox itself enabled, and a programmatic SetFocus could still drop a caret into a
' control that looks dead. WM_ENABLE (above) does the state and color work for both calls.
sub PsNumericUpDown_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub

    if pNud->hTextBox then EnableWindow( pNud->hTextBox, isEnabled )
    EnableWindow( hCtrl, isEnabled )
end sub

sub PsNumericUpDown_GetAutoRepeat( byval hCtrl as HWND, byref nDelayMs as long, byref nIntervalMs as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then
        nDelayMs = 0 : nIntervalMs = 0
        exit sub
    end if
    nDelayMs    = pNud->nRepeatDelayMs
    nIntervalMs = pNud->nRepeatIntervalMs
end sub

' Either value <= 0 disables repeating: one click = one step.
sub PsNumericUpDown_SetAutoRepeat( byval hCtrl as HWND, byval nDelayMs as long, byval nIntervalMs as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->nRepeatDelayMs    = nDelayMs
    pNud->nRepeatIntervalMs = nIntervalMs
    if (nDelayMs <= 0) orelse (nIntervalMs <= 0) then NUD_StopRepeat( pNud, hCtrl )
end sub

sub PsNumericUpDown_Refresh( byval hCtrl as HWND )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->Refresh()
    NUD_SyncChild( pNud )
end sub


' ========================================================================================
' Layout
' ========================================================================================
function PsNumericUpDown_GetFont( byval hCtrl as HWND ) as HFONT
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->hFont
end function

' CALLER-OWNED: the control converts nothing and deletes nothing; the PsTextBox turns it into
' a CHARFORMATW internally and also never takes ownership.
sub PsNumericUpDown_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->hFont = hFont
    ' A font change re-applies the CHARFORMAT but leaves the paragraph format alone, so the
    ' centring set at Create is untouched -- and re-applying it here would be actively
    ' harmful, since doing so rewrites the buffer and drops the undo history.
    if pNud->hTextBox then PsTextBox_SetFont( pNud->hTextBox, hFont )
    pNud->Refresh()
end sub

function PsNumericUpDown_GetButtonWidth( byval hCtrl as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nButtonWidth
end function

sub PsNumericUpDown_SetButtonWidth( byval hCtrl as HWND, byval nWidth as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nWidth < 0 then nWidth = 0
    pNud->nButtonWidth = nWidth
    pNud->Refresh()
    NUD_SyncChild( pNud )
end sub

sub PsNumericUpDown_GetValuePadding( byval hCtrl as HWND, byref nLeft as long, byref nRight as long, byref nVert as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then
        nLeft = 0 : nRight = 0 : nVert = 0
        exit sub
    end if
    nLeft  = pNud->nValuePadLeft
    nRight = pNud->nValuePadRight
    nVert  = pNud->nVertPadding
end sub

' nLeft/nRight are real text margins inside the value cell (EM_SETMARGINS on the RichEdit);
' nVert is the vertical clearance inside the value cell: it insets rcValue top and bottom (so
' the borderless RichEdit's line clears the frame) AND is reserved by GetIdealSize when sizing
' the height around one line of text -- the two agree, so an ideal-height control has a value
' cell exactly one text-line tall.
sub PsNumericUpDown_SetValuePadding( byval hCtrl as HWND, byval nLeft as long, byval nRight as long, byval nVert as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nLeft < 0 then nLeft = 0
    if nRight < 0 then nRight = 0
    if nVert < 0 then nVert = 0
    pNud->nValuePadLeft  = nLeft
    pNud->nValuePadRight = nRight
    pNud->nVertPadding   = nVert
    if pNud->hTextBox then PsTextBox_SetMargins( pNud->hTextBox, nLeft, nRight )
    pNud->Refresh()
end sub

function PsNumericUpDown_GetCornerRadius( byval hCtrl as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nCornerRadius
end function

sub PsNumericUpDown_SetCornerRadius( byval hCtrl as HWND, byval nRadius as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nRadius < 0 then nRadius = 0
    pNud->nCornerRadius = nRadius
    pNud->Refresh()
end sub

function PsNumericUpDown_GetBorderThickness( byval hCtrl as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nBorderThick
end function

sub PsNumericUpDown_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pNud->nBorderThick = nThickness
    pNud->Refresh()
    NUD_SyncChild( pNud )
end sub

function PsNumericUpDown_GetDividerThickness( byval hCtrl as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return 0
    return pNud->nDividerThick
end function

sub PsNumericUpDown_SetDividerThickness( byval hCtrl as HWND, byval nThickness as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pNud->nDividerThick = nThickness
    pNud->Refresh()
    NUD_SyncChild( pNud )
end sub

sub PsNumericUpDown_GetGlyphSize( byval hCtrl as HWND, byref nLength as long, byref nThickness as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then
        nLength = 0 : nThickness = 0
        exit sub
    end if
    nLength    = pNud->nGlyphLength
    nThickness = pNud->nGlyphThick
end sub

sub PsNumericUpDown_SetGlyphSize( byval hCtrl as HWND, byval nLength as long, byval nThickness as long )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    if nLength < 1 then nLength = 1
    if nThickness < 1 then nThickness = 1
    pNud->nGlyphLength = nLength
    pNud->nGlyphThick  = nThickness
    pNud->Refresh()
end sub

' ========================================================================================
' The ideal size: what the control needs to show the WIDEST value its range can produce.
'
' Both ends of the range are measured, not just the max: -1000 is wider than 999, and a range
' that never goes negative should not pay for a minus sign it will never draw.
'
' THIS IS THE ONLY PLACE THIS CONTROL MEASURES ANYTHING, and it opens its own DC to do it --
' which is what makes it valid BEFORE the control has ever been sized, when a host is calling
' it precisely to decide how big to make the thing. (LayoutControl, by contrast, takes no DC
' at all: every cell is derived from the client rect and authored scalars.)
' ========================================================================================
sub PsNumericUpDown_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
    nWidth = 0 : nHeight = 0
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub

    dim as HDC hdc = GetDC( hCtrl )
    if hdc = 0 then exit sub

    dim as HFONT hFontUse = pNud->hFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject(DEFAULT_GUI_FONT) )
    dim as HFONT hFontOld = cast( HFONT, SelectObject( hdc, hFontUse ) )

    dim as DWSTRING wszMin = NUD_FormatValue( pNud->nMin, pNud->nDecimalPlaces )
    dim as DWSTRING wszMax = NUD_FormatValue( pNud->nMax, pNud->nDecimalPlaces )

    dim as SIZE szMin, szMax
    GetTextExtentPoint32W( hdc, wszMin.vptr, len(wszMin), @szMin )
    GetTextExtentPoint32W( hdc, wszMax.vptr, len(wszMax), @szMax )
    dim as long textW = szMin.cx
    if szMax.cx > textW then textW = szMax.cx

    dim as TEXTMETRICW tm
    GetTextMetricsW( hdc, @tm )

    SelectObject( hdc, hFontOld )
    ReleaseDC( hCtrl, hdc )

    nWidth = (pNud->nButtonWidth * 2) + (pNud->nDividerThick * 2) + _
             pNud->nValuePadLeft + pNud->nValuePadRight + textW
    nHeight = tm.tmHeight + (pNud->nVertPadding * 2) + (pNud->nBorderThick * 2)
end sub


' The rect queries force a pending layout, so results are always current. FALSE means the
' control has no geometry yet (created but never sized).
private function NUD_GetRect( byval hCtrl as HWND, byref rc as RECT, byval iPart as long ) as boolean
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    SetRectEmpty( @rc )
    if pNud = 0 then return false
    if pNud->bLayoutDirty then pNud->LayoutControl()
    if pNud->bLayoutDirty then return false      ' still no geometry: never been sized

    select case iPart
    case NUD_PART_MINUS : rc = pNud->rcMinus
    case NUD_PART_VALUE : rc = pNud->rcValue
    case NUD_PART_PLUS  : rc = pNud->rcPlus
    case else           : rc = pNud->rcFrame
    end select
    return true
end function

function PsNumericUpDown_GetFrameRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    return NUD_GetRect( hCtrl, rc, NUD_PART_NONE )
end function

function PsNumericUpDown_GetMinusRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    return NUD_GetRect( hCtrl, rc, NUD_PART_MINUS )
end function

function PsNumericUpDown_GetValueRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    return NUD_GetRect( hCtrl, rc, NUD_PART_VALUE )
end function

function PsNumericUpDown_GetPlusRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    return NUD_GetRect( hCtrl, rc, NUD_PART_PLUS )
end function

function PsNumericUpDown_HitTest( byval hCtrl as HWND, byval pt as POINT ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then return NUD_PART_NONE
    return NUD_HitTestPart( pNud, pt )
end function


' ========================================================================================
' Appearance
' ========================================================================================
sub PsNumericUpDown_GetColors( byval hCtrl as HWND, byval pColors as PSNUMERICUPDOWN_COLORS ptr )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if (pNud = 0) orelse (pColors = 0) then exit sub
    *pColors = pNud->colors
end sub

' Copies the whole struct, then pushes the value cell's two colors into the PsTextBox -- the
' RichEdit draws the number, so those two have to live on the child even though they are
' authored here.
sub PsNumericUpDown_SetColors( byval hCtrl as HWND, byval pColors as PSNUMERICUPDOWN_COLORS ptr )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if (pNud = 0) orelse (pColors = 0) then exit sub
    pNud->colors = *pColors
    NUD_ApplyStateColors( pNud )
    pNud->Refresh()
end sub


' ========================================================================================
' Callbacks
' ========================================================================================
sub PsNumericUpDown_SetPaintCallback( byval hCtrl as HWND, byval usersub as NUD_PaintCallbackSub )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->PaintCallback = usersub
    if pNud->hWin then InvalidateRect( pNud->hWin, NULL, TRUE )
end sub

sub PsNumericUpDown_SetMessageCallback( byval hCtrl as HWND, byval userfunc as NUD_MessageCallbackFunc )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->MessageCallback = userfunc
end sub

sub PsNumericUpDown_SetValueChangedCallback( byval hCtrl as HWND, byval usersub as NUD_ValueChangedCallbackSub )
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(hCtrl)
    if pNud = 0 then exit sub
    pNud->ValueChangedCallback = usersub
end sub


' ========================================================================================
' Host message pump.
'
' One line of forwarding, and it exists so a host adopting this control has ONE call to add
' rather than having to know that there is a PsTextBox inside it with a PsPopupMenu inside
' that. An app already calling PsTextBox_FilterMessage is covered either way, and calling both
' is harmless -- the filter finds the single open menu chain, or finds none.
' ========================================================================================
function PsNumericUpDown_FilterMessage( byval pMsg as MSG ptr ) as boolean
    return PsTextBox_FilterMessage( pMsg )
end function
