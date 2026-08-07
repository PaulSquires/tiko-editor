''
''  PsComboBox.inc  --  owner-drawn dropdown selector (button + checkmarked list)
''

#include once "PsComboBox.bi"


' Forward declaration: the selection setters need to re-apply auto-size when a
' CBO_TEXT_WHENSELECTED button crosses the -1 boundary, and they are defined well above the
' layout section where this lives.
declare sub PsComboBox_ApplyAutoSize( byval pCombo as PSCOMBOBOX ptr )


function GetComboBoxPointer( byval hCombo as HWND ) as PSCOMBOBOX ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCombo)
    if pWindow = 0 then return 0
    return cast( PSCOMBOBOX ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- the two focus messages deliberately do not.
' Note that WM_LBUTTONUP is NOT on that list here, unlike in every capture-taking sibling:
' this control takes no capture, so an up-message carries no bookkeeping to strand.
' ========================================================================================
private function PsComboBox_CallMessage( _
            byval pCombo as PSCOMBOBOX ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pCombo = 0 then return false
    if pCombo->MessageCallback = 0 then return false

    dim as PSCOMBOBOX_MESSAGEINFO m
    m.hCombo = hwnd
    m.uMsg   = uMsg
    m.wParam = wParam
    m.lParam = lParam
    return pCombo->MessageCallback( @m )
end function


' ========================================================================================
' Re-derive the dropdown's colors from the button's, unless the host has claimed them.
'
' PSPOPUPMENU_COLORS carries no defaults except SeparatorColor, so a list that is never given
' colors renders black on black. This is what stops that being the out-of-box experience, and
' it is also why theming the button themes the list: a host sets PSCOMBOBOX_COLORS once.
'
' The border is taken from BorderColorHot rather than BorderColor because the latter defaults
' EQUAL to BackColor (the button reads borderless) -- which would give the popup an invisible
' border and no edge at all against the host window behind it.
' ========================================================================================
private sub PsComboBox_ApplyDerivedListColors( byval pCombo as PSCOMBOBOX ptr )
    if pCombo = 0 then exit sub
    if pCombo->hList = 0 then exit sub
    if pCombo->bListColorsCustom then exit sub

    dim as PSPOPUPMENU_COLORS c
    c.BackColor         = pCombo->colors.BackColor
    c.ForeColor         = pCombo->colors.ForeColor
    c.BackColorHot      = pCombo->colors.BackColorHot
    c.ForeColorHot      = pCombo->colors.ForeColorHot
    c.ForeColorDisabled = pCombo->colors.ForeColorDisabled
    c.BorderColor       = pCombo->colors.BorderColorHot
    c.SeparatorColor    = pCombo->colors.ForeColorDisabled
    PsPopupMenu_SetColors( pCombo->hList, @c )
end sub


' ========================================================================================
' Create the dropdown window on demand, and hand it everything that must never drift from the
' button: the owner, the notify target, the font, and the derived colors.
'
' Lazy on purpose. A form with a dozen comboboxes on tabs the user never visits would
' otherwise create a dozen WS_POPUP windows at startup for nothing. Nothing about the button
' needs the list to exist.
'
' hOwner is the host's TOP-LEVEL window, not our parent: PsPopupMenu uses it for z-order and
' for its foreground watch ("is the app still ours?"), and a child control is neither.
' ========================================================================================
function PsComboBox_EnsureList( byval hCombo as HWND ) as HWND
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    if pCombo->hList then return pCombo->hList

    dim as HWND hOwner = GetAncestor( hCombo, GA_ROOT )
    if hOwner = 0 then hOwner = GetParent( hCombo )

    dim as HWND hList = PsPopupMenu_Create( hOwner, pCombo->idc_ComboBox )
    if hList = 0 then return 0

    pCombo->hList = hList
    ' We are our own notify target: CPM_NOTIFY_COMMAND and CPM_NOTIFY_CLOSED are handled in
    ' our WndProc. One door -- no PM_SelectCallback plumbing on top of it.
    PsPopupMenu_SetNotifyWindow( hList, hCombo )
    PsPopupMenu_SetFonts( hList, pCombo->hTextFont, 0 )
    PsComboBox_ApplyDerivedListColors( pCombo )
    return hList
end function


' ========================================================================================
' Make the dropdown at least as wide as the button.
'
' PsPopupMenu_SetMinWidth floors the CONTENT width, but the window it reports adds padding and
' a border on top -- and PsPopupMenu exposes no getter for either, so the overhead cannot
' simply be subtracted. It CAN be measured, which is what this does:
'
'   pass 1: floor = buttonW, measure -> w.
'           w <= buttonW  ->  done. Either the floor landed exactly, or the natural content
'                             is already wider and the floor never bound.
'   pass 2: the overshoot is exactly the overhead, so floor = buttonW - overshoot and
'           measure again -> w2.
'           w2 >= buttonW ->  keep it. (Either it landed exactly, or the floor stopped
'                             binding and the natural width is wider than the button, which
'                             is fine.)
'           w2 <  buttonW ->  the lower floor stopped binding and the natural width sits
'                             between the two, so REVERT to pass 1 and be a few pixels wide
'                             rather than narrower than the button. Never narrower is the
'                             contract; a small overshoot is not worth a third iteration.
'
' At most three measures, fully deterministic, and asserted in the self-test rather than
' eyeballed.
' ========================================================================================
private sub PsComboBox_SyncListWidth( byval hList as HWND, byval nButtonW as long )
    if hList = 0 then exit sub
    if nButtonW <= 0 then exit sub

    dim as long w, h
    PsPopupMenu_SetMinWidth( hList, nButtonW )
    if PsPopupMenu_GetIdealSize( hList, w, h ) = false then exit sub
    if w <= nButtonW then exit sub

    dim as long nMin2 = nButtonW - (w - nButtonW)
    if nMin2 < 0 then nMin2 = 0

    dim as long w2, h2
    PsPopupMenu_SetMinWidth( hList, nMin2 )
    if PsPopupMenu_GetIdealSize( hList, w2, h2 ) = false then
        PsPopupMenu_SetMinWidth( hList, nButtonW )     ' restore pass 1
        exit sub
    end if
    if w2 < nButtonW then PsPopupMenu_SetMinWidth( hList, nButtonW )
end sub


' ========================================================================================
' Re-fill the dropdown's rows from items(). Runs on EVERY open, which is what makes the two
' incapable of drifting: there is no incremental path to get wrong.
'
' THE ROW ID IS (model index + 1), NOT the item's own id. Two reasons, and neither is
' cosmetic. PsPopupMenu reserves id 0 for separators, so a zero-based index cannot be used
' directly; and the host's `id` field is a free-form payload that would otherwise enter the
' command stream -- exactly the collision recorded in Learnings.md under "Replacing
' TrackPopupMenu with PsPopupMenu", where menu ids that had only ever been return values
' started re-triggering the command that opened the menu.
'
' Exactly one row is checked. PsPopupMenu has no CheckMenuRadioItem equivalent, so the mutual
' exclusion is ours -- and it falls out for free because we rebuild from a single nCurSel.
' ========================================================================================
private sub PsComboBox_RebuildListItems( byval pCombo as PSCOMBOBOX ptr )
    if pCombo = 0 then exit sub
    if pCombo->hList = 0 then exit sub

    PsPopupMenu_Clear( pCombo->hList )
    for i as long = 0 to pCombo->itemCount - 1
        dim as boolean bDisabled = (pCombo->items(i).isEnabled = false)
        dim as boolean bChecked  = (i = pCombo->nCurSel)
        PsPopupMenu_AddItem( pCombo->hList, i + 1, pCombo->items(i).wszText, "", bDisabled, bChecked )
    next
end sub


' ========================================================================================
' React to a selection change that may have crossed the -1 <-> selected boundary.
'
' Under CBO_TEXT_WHENSELECTED the button collapses to just the chevron while nothing is
' selected and expands to a captioned combobox once something is -- so that one transition
' changes the IDEAL WIDTH, and the layout has to be re-measured and (if the host opted in)
' the control re-sized.
'
' It is driven off IsTextVisible() sampled BEFORE and AFTER the change rather than off
' "was it -1", so it stays correct for any future mode without anyone having to remember
' this function exists. In CBO_TEXT_ALWAYS and CBO_TEXT_NEVER the two samples are always
' equal and this is a no-op -- which is what keeps an ordinary pick from re-measuring every
' item, the optimisation ApplyUserSel's header depends on.
'
' Call it AFTER the new selection is stored, with the pre-change sample.
' ========================================================================================
private sub PsComboBox_AfterSelectionChange( _
            byval pCombo      as PSCOMBOBOX ptr, _
            byval bTextBefore as boolean _
            )

    if pCombo = 0 then exit sub
    if pCombo->IsTextVisible() = bTextBefore then exit sub
    pCombo->bLayoutDirty = true
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, TRUE )
    PsComboBox_ApplyAutoSize( pCombo )
end sub


' ========================================================================================
' Adopt a selection because the USER asked for it -- a pick from the list, or an arrow / Home
' / End key. The one place that fires the change callback, so mouse and keyboard cannot drift
' apart. Picking what is already current is not a change and fires nothing.
'
' THE LAYOUT IS NOT MARKED DIRTY FOR AN ORDINARY PICK. The button's width comes from the
' WIDEST item, never from the selected one, so a selection change cannot move a single rect --
' only the string being drawn into rcText changes. Marking it dirty here unconditionally would
' force a needless re-measure of every item on every pick.
'
' The ONE exception is the -1 <-> selected boundary under CBO_TEXT_WHENSELECTED, and it is
' handled by PsComboBox_AfterSelectionChange rather than by a test here -- see its header for
' why it samples the resolver instead of testing for -1.
'
' The callback goes out LAST, after the state is consistent, so a host that queries
' PsComboBox_GetCurSel() from inside it sees the new value.
' ========================================================================================
private sub PsComboBox_ApplyUserSel( _
            byval pCombo as PSCOMBOBOX ptr, _
            byval hwnd   as HWND, _
            byval idxNew as long _
            )

    if pCombo = 0 then exit sub
    if idxNew = pCombo->nCurSel then exit sub
    if idxNew <> -1 then
        if pCombo->IsValidItem(idxNew) = false then exit sub
        if pCombo->items(idxNew).isEnabled = false then exit sub
    end if

    dim as boolean bTextBefore = pCombo->IsTextVisible()
    dim as long idxOld = pCombo->nCurSel
    pCombo->nCurSel = idxNew
    InvalidateRect( hwnd, NULL, FALSE )
    ' Before the callback: a host that re-places the control from its SelChange handler (which
    ' is what CBO_TEXT_WHENSELECTED asks of a host that has not turned on auto-size) must find
    ' GetIdealSize already reporting the NEW width.
    PsComboBox_AfterSelectionChange( pCombo, bTextBefore )
    if pCombo->SelChangeCallback then pCombo->SelChangeCallback( hwnd, idxOld, idxNew )
end sub


' ========================================================================================
' Re-sync isOpen with what the dropdown window is actually doing.
'
' THIS EXISTS FOR ONE CASE and it is a genuine hole in PsPopupMenu's contract rather than a
' defensive habit: only ONE popup chain may be open at a time (gPM_OpenRoot is module-shared),
' and PM_ShowCore closes any other chain with bNotify = FALSE. So when combobox B is opened
' while combobox A's list is up, A's list is closed SILENTLY -- no CPM_NOTIFY_CLOSED ever
' reaches A, and A would otherwise sit there painted "open" forever.
'
' bNotify = FALSE from the paint path. Running host callbacks from inside WM_PAINT is a rule
' this family does not break, so a repaint corrects the flag silently and the callback is
' deferred to the next non-paint touch (a mouse move, a focus change, the hover poll, or any
' query). That deferral is the residual cost of the hole and is documented rather than hidden.
' ========================================================================================
private sub PsComboBox_SyncOpenState( _
            byval pCombo  as PSCOMBOBOX ptr, _
            byval hwnd    as HWND, _
            byval bNotify as boolean _
            )

    if pCombo = 0 then exit sub
    if pCombo->isOpen = false then exit sub
    if pCombo->hList <> 0 then
        if PsPopupMenu_IsOpen( pCombo->hList ) then exit sub
    end if

    pCombo->isOpen = false
    InvalidateRect( hwnd, NULL, FALSE )
    if bNotify then
        if pCombo->DropDownCallback then pCombo->DropDownCallback( hwnd, false )
    end if
end sub


' ========================================================================================
' Draw the button: chrome, caption, chevron, focus ring.
'
' Nothing in here touches a GDI object or a device context -- PsBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' THE CHEVRON IS GEOMETRY, NOT A GLYPH. Two strokes up and two strokes down, drawn as diagonal
' lines so GDI+ antialiases them. That buys independence from any icon font being installed,
' and it scales cleanly because the cell is a layout input rather than a character cell.
'
' One asymmetry worth knowing about the stroke weight: PsBufferPaint.PaintLine DPI-scales the
' pen width it is handed, so nChevronThick DOES grow on a high-DPI display -- unlike
' nBorderThick and nFocusThick, which are scaled nowhere. That is deliberate: a divider
' hairline should stay a hairline, but a glyph's stroke should grow with the glyph.
' ========================================================================================
private sub PsComboBox_RenderDefault( _
            byval pCombo as PSCOMBOBOX ptr, _
            byval b      as PsBufferPaint ptr, _
            byval p      as PSCOMBOBOX_PAINTINFO ptr _
            )

    if (pCombo = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long bw = p->rcButton.right - p->rcButton.left
    dim as long bh = p->rcButton.bottom - p->rcButton.top
    if (bw <= 0) orelse (bh <= 0) then exit sub

    ' ---- which four colors -------------------------------------------------------------
    ' Precedence: disabled > open > hot > idle. OPEN BEATS HOT because while the list is up
    ' the cursor is normally over the LIST, not over us, and the button must stay lit.
    dim as COLORREF clrBack, clrFore, clrBorder, clrChev
    with pCombo->colors
        if p->isEnabled = false then
            clrBack   = .BackColorDisabled
            clrFore   = .ForeColorDisabled
            clrBorder = .BorderColorDisabled
            clrChev   = .ChevronColorDisabled
        elseif p->isOpen then
            clrBack   = .BackColorOpen
            clrFore   = .ForeColorOpen
            clrBorder = .BorderColorOpen
            clrChev   = .ChevronColorOpen
        elseif p->isHot then
            clrBack   = .BackColorHot
            clrFore   = .ForeColorHot
            clrBorder = .BorderColorHot
            clrChev   = .ChevronColorHot
        else
            clrBack   = .BackColor
            clrFore   = .ForeColor
            clrBorder = .BorderColor
            clrChev   = .ChevronColor
        end if
    end with

    ' ---- the chrome --------------------------------------------------------------------
    ' nCurvature is an ellipse DIAMETER (PsBufferPaint keeps GDI's vocabulary and halves it
    ' internally). Zero gives square corners through the same call.
    dim as RECT rcBtn = p->rcButton
    b->SetBackColor( clrBack )
    if pCombo->nBorderThick > 0 then
        b->SetPenColor( clrBorder )
        b->PaintRoundBorderRect( @rcBtn, pCombo->nCurvature, pCombo->nBorderThick )
    else
        b->PaintRoundRect( @rcBtn, pCombo->nCurvature )
    end if

    ' ---- the caption -------------------------------------------------------------------
    ' p->isTextVisible, NOT pCombo->nTextMode: under CBO_TEXT_WHENSELECTED the answer depends
    ' on the selection, and rcText is empty whenever this is false.
    if p->isTextVisible then
        if PsLen( p->wszText ) > 0 then
            dim as COLORREF clrText = clrFore
            if p->isPlaceholder then clrText = pCombo->colors.ForeColorPlaceholder

            ' Paint with the SAME font the measuring pass used -- the caller's if set, else
            ' the stock GUI font. PsBufferPaint.SetFont IGNORES a null handle, so leaving it
            ' unset here would silently paint in the DC's font while the widths were measured
            ' in another, and long captions would clip for no visible reason.
            dim as HFONT hFontUse = pCombo->hTextFont
            if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
            b->SetFont( hFontUse )
            b->SetColors( clrText, clrBack )

            dim as RECT rcT = p->rcText
            ' DT_NOPREFIX / DT_VCENTER / DT_SINGLELINE are added by PaintText itself.
            b->PaintText( p->wszText, @rcT, DT_LEFT or DT_END_ELLIPSIS )
        end if
    end if

    ' ---- the chevron -------------------------------------------------------------------
    ' An up chevron in the top half and a down chevron in the bottom half, separated by
    ' nChevronGap. Both apexes point AWAY from the centre, which is the "this opens a list"
    ' idiom rather than the "expand/collapse" single arrow.
    dim as long cl = p->rcChevron.left
    dim as long cr = p->rcChevron.right
    dim as long ct = p->rcChevron.top
    dim as long cb = p->rcChevron.bottom
    dim as long cx = cl + ((cr - cl) \ 2)
    dim as long ch = ((cb - ct) - pCombo->nChevronGap) \ 2

    ' Degenerate cells are skipped rather than drawn wrong: a zero-height arm would make
    ' PaintLine take its HORIZONTAL branch, and a zero-width one its VERTICAL branch, either
    ' of which draws a stray bar instead of a chevron.
    if (ch >= 1) andalso (cx > cl) andalso (cr > cx) then
        b->SetPenColor( clrChev )
        dim as long thick = pCombo->nChevronThick
        if thick < 1 then thick = 1
        ' up
        b->PaintLine( thick, cl, ct + ch, cx, ct )
        b->PaintLine( thick, cx, ct, cr, ct + ch )
        ' down
        b->PaintLine( thick, cl, cb - ch, cx, cb )
        b->PaintLine( thick, cx, cb, cr, cb - ch )
    end if

    ' ---- the focus ring ----------------------------------------------------------------
    ' Outline, never filled: it is drawn OVER the button, and a filled round rect here would
    ' erase everything above. Its band is reserved in the layout whether or not we are
    ' focused, which is what stops the button jumping when focus arrives.
    '
    ' The ring's curvature is the button's plus the band on both sides, which is what keeps
    ' the two curves concentric -- reusing nCurvature would leave the ring visibly squarer
    ' than the corner it surrounds.
    if p->isFocused then
        if pCombo->nFocusThick > 0 then
            dim as RECT rcRing = p->rcVisual
            dim as long fcurv = pCombo->nCurvature + (2 * pCombo->RingPad())
            b->SetPenColor( pCombo->colors.FocusRingColor )
            b->PaintRoundOutline( @rcRing, fcurv, pCombo->nFocusThick )
        end if
    end if
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function PsComboBox_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hwnd)
    if pCombo = null then return 0

    ' Correct a stale "open" flag before deciding colors, but WITHOUT running host callbacks
    ' from inside a paint. See PsComboBox_SyncOpenState.
    PsComboBox_SyncOpenState( pCombo, hwnd, false )

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    ' The client is filled with the mood's own background, not a fixed one, so a paint
    ' callback that draws only a foreground still sits on the right colour.
    dim as COLORREF clrFill = pCombo->colors.BackColor
    if pCombo->isEnabled = false then
        clrFill = pCombo->colors.BackColorDisabled
    elseif pCombo->isOpen then
        clrFill = pCombo->colors.BackColorOpen
    elseif pCombo->isHot then
        clrFill = pCombo->colors.BackColorHot
    end if
    b.SetBackColor( clrFill )
    b.PaintClientRect()

    dim p as PSCOMBOBOX_PAINTINFO
    p.hCombo = hwnd
    p.b      = @b
    GetClientRect( hwnd, @p.rcClient )
    p.rcButton  = pCombo->rcButton
    p.rcText    = pCombo->rcText
    p.rcChevron = pCombo->rcChevron
    p.rcVisual  = pCombo->rcVisual
    p.isHot     = pCombo->isHot
    p.isOpen    = pCombo->isOpen
    p.isEnabled = pCombo->isEnabled
    p.isFocused = pCombo->isFocused
    p.nCurSel   = pCombo->nCurSel
    dim as boolean bPlaceholder = false
    p.wszText       = pCombo->DisplayText( bPlaceholder )
    p.isPlaceholder = bPlaceholder
    p.isTextVisible = pCombo->IsTextVisible()

    if pCombo->PaintCallback then
        pCombo->PaintCallback( @p )
    else
        PsComboBox_RenderDefault( pCombo, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
' ========================================================================================
private sub PsComboBox_ClearHot( byval pCombo as PSCOMBOBOX ptr, byval hwnd as HWND )
    if pCombo = 0 then exit sub
    if pCombo->hotTimerOn then
        KillTimer( hwnd, IDT_CCOMBOBOX_HOTTRACK )
        pCombo->hotTimerOn = false
    end if
    if pCombo->isHot then
        pCombo->isHot = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Apply the ideal size to ourselves, preserving the top-left. Only runs when the host has
' opted in with PsComboBox_SetAutoSize -- the family default is that a control never moves
' itself and the host measures with GetIdealSize.
'
' Called from every mutator that can change the ideal width. It forces the pending layout
' first (that is where the measurement happens), then resizes -- which marks the layout dirty
' again through WM_SIZE, so the rects are re-derived against the new client area.
' ========================================================================================
private sub PsComboBox_ApplyAutoSize( byval pCombo as PSCOMBOBOX ptr )
    if pCombo = 0 then exit sub
    if pCombo->bAutoSize = false then exit sub
    if pCombo->hWin = 0 then exit sub

    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    if (pCombo->nIdealW <= 0) orelse (pCombo->nIdealH <= 0) then exit sub

    SetWindowPos( pCombo->hWin, 0, 0, 0, pCombo->nIdealW, pCombo->nIdealH, _
                  SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
end sub


' ========================================================================================
' PsComboBox Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is
' handled here and anything unhandled goes to DefWindowProc.
'
' NO MOUSE CAPTURE IS TAKEN, ANYWHERE. The family's test is "take capture only if something
' consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and here it comes out
' negative: the DOWN opens the list, and from that instant the popup owns the interaction.
' There is no press/cancel gesture to protect, so none of the capture price is paid -- no
' WM_CAPTURECHANGED handler, no snapshot-before-release dance, no CancelPress, and no
' "callback result ignored for WM_LBUTTONUP" caution. A note about capture copied in from a
' sibling would be a lie here.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET, which is PsToggle's and PsIconPanel's call rather than
' PsSelectBar's. With it, the second of two rapid clicks arrives as WM_LBUTTONDBLCLK INSTEAD OF
' a second WM_LBUTTONDOWN -- and on this control a rapid second click is a legitimate
' close-then-reopen, which would be silently swallowed.
'
' FOCUS: this is the second focusable control in the family, after PsToggle. See the
' WM_GETDLGCODE handler for the subtlety.
' ========================================================================================
function PsComboBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsComboBox_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pCombo then
            pCombo->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_TIMER
        if (pCombo <> 0) andalso (wParam = IDT_CCOMBOBOX_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot state ourselves.
            dim as RECT rcClient
            GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                PsComboBox_ClearHot( pCombo, hwnd )
            end if
            ' Cheap place to notice a list that was closed behind our back (see
            ' PsComboBox_SyncOpenState), and one that can safely notify.
            PsComboBox_SyncOpenState( pCombo, hwnd, true )
            return 0
        end if

    case WM_MOUSEMOVE
        if pCombo = 0 then exit select
        if pCombo->isEnabled = false then exit select

        PsComboBox_SyncOpenState( pCombo, hwnd, true )

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
            if pCombo->hotTimerOn = false then
                SetTimer( hwnd, IDT_CCOMBOBOX_HOTTRACK, PSCOMBOBOX_HOTTRACK_MS, NULL )
                pCombo->hotTimerOn = true
            end if
        end if

        if bOver <> pCombo->isHot then
            pCombo->isHot = bOver
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pCombo then
            ' Fast path for clearing the hot state (the WM_TIMER poll is the fallback for
            ' when this message never arrives).
            PsComboBox_ClearHot( pCombo, hwnd )
            if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pCombo = 0 then return 0
        ' A disabled window does not receive mouse input at all (PsComboBox_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some
        ' other route.
        if pCombo->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true
        ' even if the host goes on to suppress the click. This runs WM_SETFOCUS synchronously,
        ' so isFocused is already correct by the time we paint below.
        SetFocus( hwnd )

        ' Consume the one-shot reopen guard NOW, whatever else happens below. If the pump
        ' filter armed it, this very click is the one that already dismissed the list, and
        ' letting it also open the list would make the button impossible to close by clicking.
        dim as boolean bSuppress = pCombo->bSuppressNextOpen
        pCombo->bSuppressNextOpen = false

        if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0
        if bSuppress then return 0

        ' Toggle. The second branch only runs for a host that does NOT call
        ' PsComboBox_FilterMessage in its pump -- with the filter in place the list is already
        ' closed by the time we get here and bSuppress caught it. Handling it anyway means a
        ' host missing the pump contract still gets a button that opens and closes; it just
        ' loses keyboard navigation and outside-click dismissal.
        if pCombo->isOpen then
            PsComboBox_CloseUp( hwnd )
        else
            PsComboBox_DropDown( hwnd )
        end if
        return 0

    case WM_LBUTTONUP
        ' Nothing to do: the list opened on the DOWN. Reported for completeness. The result
        ' IS honoured here, unlike in the capture-taking siblings -- see the WndProc header.
        if pCombo then
            if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu).
        if pCombo then
            if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' Focus is a FACT reported by the system, not an action a host can veto, so the state
        ' is updated first and the callback's result is IGNORED. A host that wants to refuse
        ' focus must not make the control a tabstop in the first place.
        if pCombo then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pCombo->isFocused then
                pCombo->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            PsComboBox_SyncOpenState( pCombo, hwnd, true )
            PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the keys we actually consume, and only when we are being asked about a
        ' specific message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the
        ' control would swallow the very navigation it opted into by being a tabstop. Claiming
        ' nothing would let a host's IsDialogMessage pump route Enter to the dialog's default
        ' button before the control ever saw it. Per-message is the only answer that gets both
        ' right. (PsToggle established this; the key list is longer here.)
        if lParam then
            dim as MSG ptr pMsg = cast( MSG ptr, lParam )
            if pMsg->message = WM_KEYDOWN then
                select case pMsg->wParam
                case VK_UP, VK_DOWN, VK_HOME, VK_END, VK_F4, VK_SPACE, VK_RETURN
                    return DLGC_WANTALLKEYS
                end select
            end if
        end if
        return 0

    case WM_KEYDOWN
        if pCombo = 0 then exit select
        if pCombo->isEnabled = false then return 0

        ' While the list is up, the keyboard belongs to PsPopupMenu's pump filter, which
        ' consumes every WM_KEYDOWN. Reaching here with the list open means the host is not
        ' running PsComboBox_FilterMessage; swallow the key rather than moving the selection
        ' behind a list that is still showing the old checkmark.
        PsComboBox_SyncOpenState( pCombo, hwnd, true )
        if pCombo->isOpen then return 0

        if PsComboBox_CallMessage( pCombo, hwnd, uMsg, wParam, lParam ) then return 0

        ' Win32 combobox rules. Up/Down/Home/End move the selection IN PLACE without opening
        ' -- each firing SelChange, because it is user action -- and Alt+Down / F4 / Space /
        ' Enter open the list. Arrow movement CLAMPS at the ends (see NextSelectable): a
        ' wrapping Down at the last item would jump the user back to the first with no list
        ' visible to explain it.
        dim as boolean bAlt = ((GetKeyState( VK_MENU ) and &h8000) <> 0)

        select case wParam
        case VK_DOWN
            if bAlt then
                PsComboBox_DropDown( hwnd )
            else
                dim as long idx = pCombo->NextSelectable( pCombo->nCurSel, 1 )
                if idx <> -1 then PsComboBox_ApplyUserSel( pCombo, hwnd, idx )
            end if
            return 0
        case VK_UP
            if bAlt then
                PsComboBox_DropDown( hwnd )
            else
                ' From "nothing selected", Up lands on the LAST selectable -- the only
                ' sensible answer when there is no current position to step back from.
                dim as long nFrom = pCombo->nCurSel
                if nFrom = -1 then nFrom = pCombo->itemCount
                dim as long idx = pCombo->NextSelectable( nFrom, -1 )
                if idx <> -1 then PsComboBox_ApplyUserSel( pCombo, hwnd, idx )
            end if
            return 0
        case VK_HOME
            dim as long idx = pCombo->NextSelectable( -1, 1 )
            if idx <> -1 then PsComboBox_ApplyUserSel( pCombo, hwnd, idx )
            return 0
        case VK_END
            dim as long idx = pCombo->NextSelectable( pCombo->itemCount, -1 )
            if idx <> -1 then PsComboBox_ApplyUserSel( pCombo, hwnd, idx )
            return 0
        case VK_F4, VK_SPACE, VK_RETURN
            PsComboBox_DropDown( hwnd )
            return 0
        end select

    case CPM_NOTIFY_COMMAND
        ' A row was executed. The chain is ALREADY closed by the time this arrives, so no
        ' CPM_NOTIFY_CLOSED follows and the open state must be cleared here too.
        ' wParam = the row id, which we set to (model index + 1) when the rows were built.
        if pCombo then
            dim as long idx = cast( long, wParam ) - 1
            dim as boolean bWasOpen = pCombo->isOpen
            pCombo->isOpen = false
            InvalidateRect( hwnd, NULL, FALSE )
            if bWasOpen then
                if pCombo->DropDownCallback then pCombo->DropDownCallback( hwnd, false )
            end if
            ' The selection change goes out AFTER the close notification, so a host handling
            ' both sees a consistent "closed, and here is the new value" order.
            PsComboBox_ApplyUserSel( pCombo, hwnd, idx )
            return 0
        end if

    case CPM_NOTIFY_CLOSED
        ' Dismissed by USER interaction without picking anything (Escape, outside click,
        ' foreground loss). Programmatic Hide/CloseChain send nothing, which is why
        ' PsComboBox_CloseUp clears the flag itself.
        if pCombo then
            dim as boolean bWasOpen = pCombo->isOpen
            pCombo->isOpen = false
            InvalidateRect( hwnd, NULL, FALSE )
            if bWasOpen then
                if pCombo->DropDownCallback then pCombo->DropDownCallback( hwnd, false )
            end if
            return 0
        end if

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsComboBox_SetEnabled still gets the greyed
        ' rendering rather than a control that looks live but eats input.
        if pCombo then
            pCombo->isEnabled = (wParam <> 0)
            if pCombo->isEnabled = false then
                pCombo->isHot = false
                PsComboBox_CloseUp( hwnd )
            end if
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_DESTROY
        if pCombo then
            ' Don't let the hot-tracking timer outlive the window...
            if pCombo->hotTimerOn then
                KillTimer( hwnd, IDT_CCOMBOBOX_HOTTRACK )
                pCombo->hotTimerOn = false
            end if
            ' ...nor the dropdown. It is a WS_POPUP owned by the host's top-level window, not
            ' a child of ours, so nothing would destroy it for us.
            if pCombo->hList then
                DestroyWindow( pCombo->hList )
                pCombo->hList = 0
            end if
            pCombo->isOpen = false
        end if

    case WM_NCDESTROY
        ' Delete the stored control data. There is no tooltip window and no font to release:
        ' the font is borrowed and stays the host's.
        if pCombo then delete(pCombo)

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' PsComboBox_Create
' ========================================================================================
function PsComboBox_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus. There is no WS_EX_CONTROLPARENT because there is
    ' no child to see through to -- the tabstop is on this window itself. The dropdown is a
    ' separate TOP-LEVEL popup, not a child, so it is not a tab target either.
    '
    ' THE TRAILING 0 IS THAT SENTENCE'S IMPLEMENTATION, AND IT WAS MISSING UNTIL 2026-07-23.
    ' CWindow.Create defaults its dwExStyle parameter to WS_EX_CONTROLPARENT OR
    ' WS_EX_WINDOWEDGE, so passing only dwStyle -- as this call did -- silently declared the
    ' control a CONTAINER. The dialog manager then descended into it looking for tabstops,
    ' found no children, and SKIPPED it: Tab never landed on a combobox. This repo's demo hid
    ' that, because it also hosts three plain Win32 BUTTONs -- Tab moved between THOSE, so the
    ' interactive pass saw focus moving and concluded navigation worked.
    '
    ' DO NOT COPY THIS INTO A WRAP-A-REAL-CHILD CONTROL. PsListBox, PsTextBox, PsNumericUpDown
    ' and PsScrollPanel all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner
    ' child; passing 0 there would make them unreachable instead. The test is "does this
    ' window have children the dialog manager should find?", not "is this a control?".
    ' Full write-up in C:\dev\Learnings.md.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsComboBox_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header. It would
    ' turn a rapid close-then-reopen into a WM_LBUTTONDBLCLK the control would drop.

    dim as PSCOMBOBOX ptr pCombo = new PSCOMBOBOX
    pCombo->hWin         = hCtrl
    pCombo->idc_ComboBox = CtrlID

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). The three THICKNESS values are not
    ' scaled at all here -- and note nChevronThick is scaled later by PaintLine, which is the
    ' one deliberate exception (a glyph stroke grows, a hairline does not).
    pCombo->nPadLeft       = pWindow->ScaleX( PSCOMBOBOX_DEFAULT_PADLEFT )
    pCombo->nPadRight      = pWindow->ScaleX( PSCOMBOBOX_DEFAULT_PADRIGHT )
    pCombo->nPadTop        = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_PADTOP )
    pCombo->nPadBottom     = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_PADBOTTOM )
    pCombo->nTextGap       = pWindow->ScaleX( PSCOMBOBOX_DEFAULT_TEXTGAP )
    pCombo->nChevronWidth  = pWindow->ScaleX( PSCOMBOBOX_DEFAULT_CHEVRONW )
    pCombo->nChevronHeight = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_CHEVRONH )
    pCombo->nChevronGap    = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_CHEVRONGAP )
    pCombo->nCurvature     = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_CURVATURE )
    pCombo->nFocusGap      = pWindow->ScaleY( PSCOMBOBOX_DEFAULT_FOCUSGAP )

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pCombo)

    function = hCtrl

end function


' ========================================================================================
' Items.  All SILENT -- no callbacks, no notifications.
' ========================================================================================
function PsComboBox_AddItem( byval hCombo as HWND, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return -1
    dim as PSCOMBOBOX_ITEM ptr pItem = pCombo->AddItemSlot()
    if pItem = 0 then return -1
    pItem->wszText  = Text
    pItem->id       = id
    pItem->itemData = itemData
    PsComboBox_ApplyAutoSize( pCombo )
    return pCombo->GetCount() - 1
end function

function PsComboBox_InsertItem( byval hCombo as HWND, byval idx as long, byval Text as DWSTRING, byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return -1
    dim as PSCOMBOBOX_ITEM ptr pItem = pCombo->InsertItemSlot( idx )
    if pItem = 0 then return -1
    pItem->wszText  = Text
    pItem->id       = id
    pItem->itemData = itemData
    PsComboBox_ApplyAutoSize( pCombo )
    return idx
end function

function PsComboBox_DeleteItem( byval hCombo as HWND, byval idx as long ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    dim as boolean bDone = pCombo->RemoveItemAt( idx )
    if bDone then PsComboBox_ApplyAutoSize( pCombo )
    return bDone
end function

sub PsComboBox_Clear( byval hCombo as HWND )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->Clear()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

function PsComboBox_GetCount( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->GetCount()
end function

function PsComboBox_IsValidItem( byval hCombo as HWND, byval idx as long ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    return pCombo->IsValidItem( idx )
end function

function PsComboBox_GetItemText( byval hCombo as HWND, byval idx as long ) as DWSTRING
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return ""
    if pCombo->IsValidItem(idx) = false then return ""
    dim as DWSTRING wszOut = pCombo->items(idx).wszText
    return wszOut
end function

' The caption drives the WIDEST-item measurement, so this re-measures and can change the
' button's ideal width -- and, with auto-size on, the button itself.
function PsComboBox_SetItemText( byval hCombo as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->IsValidItem(idx) = false then return false
    pCombo->items(idx).wszText = Text
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
    return true
end function

function PsComboBox_GetItemID( byval hCombo as HWND, byval idx as long ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    if pCombo->IsValidItem(idx) = false then return 0
    return pCombo->items(idx).id
end function

function PsComboBox_SetItemID( byval hCombo as HWND, byval idx as long, byval id as long ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->IsValidItem(idx) = false then return false
    pCombo->items(idx).id = id
    return true
end function

function PsComboBox_GetItemData( byval hCombo as HWND, byval idx as long ) as integer
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    if pCombo->IsValidItem(idx) = false then return 0
    return pCombo->items(idx).itemData
end function

function PsComboBox_SetItemData( byval hCombo as HWND, byval idx as long, byval itemData as integer ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->IsValidItem(idx) = false then return false
    pCombo->items(idx).itemData = itemData
    return true
end function

function PsComboBox_GetItemEnabled( byval hCombo as HWND, byval idx as long ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->IsValidItem(idx) = false then return false
    return pCombo->items(idx).isEnabled
end function

' Disabling the CURRENT item clears the selection. A disabled item is unreachable by any user
' action, so leaving it selected would leave the button showing a value the user cannot
' restore once they move off it -- and PsComboBox_SetCurSel already refuses to put it back.
function PsComboBox_SetItemEnabled( byval hCombo as HWND, byval idx as long, byval bEnabled as boolean ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->IsValidItem(idx) = false then return false
    dim as boolean bTextBefore = pCombo->IsTextVisible()
    pCombo->items(idx).isEnabled = bEnabled
    if bEnabled = false then
        if pCombo->nCurSel = idx then pCombo->nCurSel = -1
    end if
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
    ' Clearing the selection here can collapse a CBO_TEXT_WHENSELECTED button back to
    ' arrow-only, which is a width change like any other.
    PsComboBox_AfterSelectionChange( pCombo, bTextBefore )
    return true
end function

function PsComboBox_FindItemByID( byval hCombo as HWND, byval id as long ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return -1
    for i as long = 0 to pCombo->itemCount - 1
        if pCombo->items(i).id = id then return i
    next
    return -1
end function

function PsComboBox_FindItemByText( byval hCombo as HWND, byval Text as DWSTRING ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return -1
    dim as wstring * 1024 wszWant
    wszWant = *Text.Wz()
    for i as long = 0 to pCombo->itemCount - 1
        dim as wstring * 1024 wszHave
        wszHave = *pCombo->items(i).wszText.Wz()
        if lcase(wszHave) = lcase(wszWant) then return i
    next
    return -1
end function


' ========================================================================================
' Selection
' ========================================================================================
function PsComboBox_GetCurSel( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return -1
    return pCombo->nCurSel
end function

' ========================================================================================
' SILENT: the change callback fires only for user interaction. See the header.
'
' REFUSES a disabled item: the user could not have reached that state, and a host should not
' be able to fake it silently. -1 always succeeds; an out-of-range index does not.
'
' No layout is marked dirty -- the width comes from the widest item, not the selected one.
' ========================================================================================
function PsComboBox_SetCurSel( byval hCombo as HWND, byval idx as long ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if idx <> -1 then
        if pCombo->IsValidItem(idx) = false then return false
        if pCombo->items(idx).isEnabled = false then return false
    end if
    if pCombo->nCurSel = idx then return true
    dim as boolean bTextBefore = pCombo->IsTextVisible()
    pCombo->nCurSel = idx
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
    PsComboBox_AfterSelectionChange( pCombo, bTextBefore )
    return true
end function

' What the BUTTON is currently showing: the selected caption, the placeholder, or "".
function PsComboBox_GetText( byval hCombo as HWND ) as DWSTRING
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return ""
    dim as boolean bPlaceholder = false
    dim as DWSTRING wszOut = pCombo->DisplayText( bPlaceholder )
    return wszOut
end function


' ========================================================================================
' State
' ========================================================================================
function PsComboBox_GetEnabled( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    return pCombo->isEnabled
end function

' ========================================================================================
' Greys the control AND disables the window. Going through EnableWindow rather than just
' painting differently is what actually stops the input: a disabled window receives no mouse
' messages and the dialog manager's Tab skips it, so there is no way for a click or a
' keystroke to sneak past the cosmetic check. WM_ENABLE does the state clearing (and covers a
' host that calls EnableWindow itself); the work is repeated here because Windows sends no
' WM_ENABLE when the state was already what we asked for.
' ========================================================================================
sub PsComboBox_SetEnabled( byval hCombo as HWND, byval isEnabled as boolean )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->isEnabled = isEnabled
    if isEnabled then
        EnableWindow( hCombo, TRUE )
    else
        EnableWindow( hCombo, FALSE )
        pCombo->isHot = false
        PsComboBox_CloseUp( hCombo )
    end if
    InvalidateRect( hCombo, NULL, FALSE )
end sub

function PsComboBox_GetFocused( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    return pCombo->isFocused
end function

function PsComboBox_IsDroppedDown( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    PsComboBox_SyncOpenState( pCombo, hCombo, true )
    return pCombo->isOpen
end function

' ========================================================================================
' Open the dropdown.
'
' Order matters here and each step is load-bearing:
'   1. the DropDown(TRUE) callback fires FIRST, before a single row is built -- that is the
'      just-in-time hook, so a host can Clear + AddItem and have it picked up below;
'   2. rows are rebuilt from items(), so the list can never drift from the model;
'   3. the font is re-handed over, in case SetFont ran after the list was created;
'   4. the width is synced against the button (see PsComboBox_SyncListWidth);
'   5. the current row is preselected, so keyboard navigation starts where the user is
'      rather than at the top;
'   6. the anchor is the BUTTON CHROME in screen coords -- PsPopupMenu_ShowForRect aligns left
'      edges, sits at bottom-1 so the borders merge, and handles the work-area clamp/flip.
'
' If it bails after step 1 a matching DropDown(FALSE) is fired, so the callback's open/close
' pairing always holds even when the open fails.
' ========================================================================================
function PsComboBox_DropDown( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->isEnabled = false then return false

    PsComboBox_SyncOpenState( pCombo, hCombo, true )
    if pCombo->isOpen then return true

    if pCombo->DropDownCallback then pCombo->DropDownCallback( hCombo, true )

    dim as HWND hList = PsComboBox_EnsureList( hCombo )
    dim as boolean bCanShow = (hList <> 0) andalso (pCombo->itemCount > 0)
    if bCanShow = false then
        if pCombo->DropDownCallback then pCombo->DropDownCallback( hCombo, false )
        return false
    end if

    PsComboBox_RebuildListItems( pCombo )
    PsPopupMenu_SetFonts( hList, pCombo->hTextFont, 0 )

    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    dim as RECT rcAnchor = pCombo->rcButton
    if IsRectEmpty( @rcAnchor ) then GetClientRect( hCombo, @rcAnchor )
    dim as POINT ptTL, ptBR
    ptTL.x = rcAnchor.left  : ptTL.y = rcAnchor.top
    ptBR.x = rcAnchor.right : ptBR.y = rcAnchor.bottom
    ClientToScreen( hCombo, @ptTL )
    ClientToScreen( hCombo, @ptBR )
    SetRect( @rcAnchor, ptTL.x, ptTL.y, ptBR.x, ptBR.y )

    PsComboBox_SyncListWidth( hList, rcAnchor.right - rcAnchor.left )

    if pCombo->IsValidItem( pCombo->nCurSel ) then
        PsPopupMenu_SetCurSel( hList, pCombo->nCurSel )
    else
        PsPopupMenu_SetCurSel( hList, -1 )
    end if

    if PsPopupMenu_ShowForRect( hList, rcAnchor, PM_ALIGN_BELOW ) = false then
        if pCombo->DropDownCallback then pCombo->DropDownCallback( hCombo, false )
        return false
    end if

    pCombo->isOpen = true
    InvalidateRect( hCombo, NULL, FALSE )
    return true
end function

' ========================================================================================
' Close the dropdown. PsPopupMenu_Hide is SILENT (programmatic close sends no CPM_NOTIFY_*),
' so the flag and the callback are our own responsibility here.
' ========================================================================================
sub PsComboBox_CloseUp( byval hCombo as HWND )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if pCombo->hList then PsPopupMenu_Hide( pCombo->hList )
    if pCombo->isOpen = false then exit sub
    pCombo->isOpen = false
    InvalidateRect( hCombo, NULL, FALSE )
    if pCombo->DropDownCallback then pCombo->DropDownCallback( hCombo, false )
end sub

sub PsComboBox_Refresh( byval hCombo as HWND )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->Refresh()
end sub


' ========================================================================================
' Layout.  All setters take RAW PIXELS -- the caller DPI-scales.
' ========================================================================================
function PsComboBox_GetShowText( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    return pCombo->IsTextVisible()
end function

' The two-state convenience over SetTextMode. TRUE = CBO_TEXT_ALWAYS, FALSE = CBO_TEXT_NEVER.
' It cannot reach CBO_TEXT_WHENSELECTED -- that mode has no boolean spelling, which is exactly
' why the enum exists. Note the asymmetry with the getter, which answers "is a caption drawn
' right now" rather than echoing the mode back: for WHENSELECTED those are different questions
' and the state one is what a caller has any use for.
sub PsComboBox_SetShowText( byval hCombo as HWND, byval bShowText as boolean )
    if bShowText then
        PsComboBox_SetTextMode( hCombo, CBO_TEXT_ALWAYS )
    else
        PsComboBox_SetTextMode( hCombo, CBO_TEXT_NEVER )
    end if
end sub

function PsComboBox_GetTextMode( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return CBO_TEXT_ALWAYS
    return pCombo->nTextMode
end function

sub PsComboBox_SetTextMode( byval hCombo as HWND, byval nTextMode as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    select case nTextMode
    case CBO_TEXT_ALWAYS, CBO_TEXT_NEVER, CBO_TEXT_WHENSELECTED
    case else
        exit sub                 ' ignore garbage rather than laying out somewhere strange
    end select
    if pCombo->nTextMode = nTextMode then exit sub
    pCombo->nTextMode = nTextMode
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

function PsComboBox_GetAutoSize( byval hCombo as HWND ) as boolean
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    return pCombo->bAutoSize
end function

' Turning it ON applies immediately, so a host does not have to also call SetWindowPos once
' to get the initial fit.
sub PsComboBox_SetAutoSize( byval hCombo as HWND, byval bAutoSize as boolean )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->bAutoSize = bAutoSize
    PsComboBox_ApplyAutoSize( pCombo )
end sub

sub PsComboBox_GetPadding( byval hCombo as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
    nLeft = 0 : nTop = 0 : nRight = 0 : nBottom = 0
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    nLeft   = pCombo->nPadLeft
    nTop    = pCombo->nPadTop
    nRight  = pCombo->nPadRight
    nBottom = pCombo->nPadBottom
end sub

sub PsComboBox_SetPadding( byval hCombo as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nLeft   < 0 then nLeft   = 0
    if nTop    < 0 then nTop    = 0
    if nRight  < 0 then nRight  = 0
    if nBottom < 0 then nBottom = 0
    pCombo->nPadLeft   = nLeft
    pCombo->nPadTop    = nTop
    pCombo->nPadRight  = nRight
    pCombo->nPadBottom = nBottom
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

function PsComboBox_GetTextGap( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->nTextGap
end function

sub PsComboBox_SetTextGap( byval hCombo as HWND, byval nTextGap as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nTextGap < 0 then nTextGap = 0
    pCombo->nTextGap = nTextGap
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

sub PsComboBox_GetChevronSize( byval hCombo as HWND, byref nWidth as long, byref nHeight as long )
    nWidth = 0 : nHeight = 0
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    nWidth  = pCombo->nChevronWidth
    nHeight = pCombo->nChevronHeight
end sub

' Clamped at 2x2: below that the arms degenerate and PaintLine would take its horizontal or
' vertical branch and draw a stray bar instead of a chevron. The painter guards this too, but
' refusing the value here means GetChevronRect never reports a cell nothing can be drawn in.
sub PsComboBox_SetChevronSize( byval hCombo as HWND, byval nWidth as long, byval nHeight as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nWidth  < 2 then nWidth  = 2
    if nHeight < 2 then nHeight = 2
    pCombo->nChevronWidth  = nWidth
    pCombo->nChevronHeight = nHeight
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

function PsComboBox_GetChevronGap( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->nChevronGap
end function

' The gap lives INSIDE the chevron cell, so it changes the two arms' height and never the
' control's ideal size -- no auto-size pass is needed.
sub PsComboBox_SetChevronGap( byval hCombo as HWND, byval nGap as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nGap < 0 then nGap = 0
    pCombo->nChevronGap = nGap
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
end sub

function PsComboBox_GetChevronThickness( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->nChevronThick
end function

' Stroke weight only, no layout consequence: the arms are drawn inside the cell, so changing
' this repaints but never moves anything. Note PsBufferPaint.PaintLine DPI-scales what it is
' given, unlike the border and ring thicknesses -- see the painter's header.
sub PsComboBox_SetChevronThickness( byval hCombo as HWND, byval nThickness as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nThickness < 1 then nThickness = 1
    pCombo->nChevronThick = nThickness
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
end sub

function PsComboBox_GetCornerCurvature( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->nCurvature
end function

' An ellipse DIAMETER, not a radius -- PsBufferPaint keeps GDI's vocabulary. 0 = square.
sub PsComboBox_SetCornerCurvature( byval hCombo as HWND, byval nCurvature as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pCombo->nCurvature = nCurvature
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
end sub

function PsComboBox_GetBorderThickness( byval hCombo as HWND ) as long
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->nBorderThick
end function

' Thickness only, no layout consequence: the border is drawn INSIDE the chrome. Zero means no
' border at all (and the chrome is then filled with a plain rounded rect).
sub PsComboBox_SetBorderThickness( byval hCombo as HWND, byval nThickness as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pCombo->nBorderThick = nThickness
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
end sub

sub PsComboBox_GetFocusRing( byval hCombo as HWND, byref nGap as long, byref nThickness as long )
    nGap = 0 : nThickness = 0
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    nGap       = pCombo->nFocusGap
    nThickness = pCombo->nFocusThick
end sub

' BOTH are layout inputs, focused or not: the ring's band is reserved unconditionally so the
' button does not jump when focus arrives, which means either value changes the ideal size.
sub PsComboBox_SetFocusRing( byval hCombo as HWND, byval nGap as long, byval nThickness as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if nGap < 0 then nGap = 0
    if nThickness < 0 then nThickness = 0
    pCombo->nFocusGap   = nGap
    pCombo->nFocusThick = nThickness
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

' ========================================================================================
' The measured button size: chrome plus the focus-ring band on every side.
'
' It forces a pending layout, because the width depends on MEASURED text and there is no way
' to compute it from the scalars alone (which is how PsToggle's equivalent dodges the problem).
' What makes it valid before the control has ever been sized is that LayoutCombo runs its
' measuring pass BEFORE bailing on a zero client area -- see the layout header.
' ========================================================================================
sub PsComboBox_GetIdealSize( byval hCombo as HWND, byref nWidth as long, byref nHeight as long )
    nWidth  = 0
    nHeight = 0
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    nWidth  = pCombo->nIdealW
    nHeight = pCombo->nIdealH
end sub

' ========================================================================================
' Rect queries. Each forces a pending layout, so results are always current. FALSE means the
' control has no geometry yet (created but never sized).
'
' GetTextRect returns TRUE with an EMPTY rect in arrow-only mode -- empty is the honest
' answer there, and reporting failure would conflate "no caption" with "not sized yet".
' ========================================================================================
function PsComboBox_GetButtonRect( byval hCombo as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    if pCombo->bLayoutDirty then return false        ' still no client area
    rc = pCombo->rcButton
    return true
end function

function PsComboBox_GetTextRect( byval hCombo as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    if pCombo->bLayoutDirty then return false
    rc = pCombo->rcText
    return true
end function

function PsComboBox_GetChevronRect( byval hCombo as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    if pCombo->bLayoutDirty then return false
    rc = pCombo->rcChevron
    return true
end function

function PsComboBox_GetVisualRect( byval hCombo as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->bLayoutDirty then pCombo->LayoutCombo()
    if pCombo->bLayoutDirty then return false
    rc = pCombo->rcVisual
    return true
end function


' ========================================================================================
' Appearance
' ========================================================================================
sub PsComboBox_GetColors( byval hCombo as HWND, byval pColors as PSCOMBOBOX_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    *pColors = pCombo->colors
end sub

' Also re-derives the DROPDOWN's colors, unless the host has claimed them with
' PsComboBox_SetListColors -- so theming the button themes the list for free.
sub PsComboBox_SetColors( byval hCombo as HWND, byval pColors as PSCOMBOBOX_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->colors = *pColors
    PsComboBox_ApplyDerivedListColors( pCombo )
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, TRUE )
end sub

function PsComboBox_GetFont( byval hCombo as HWND ) as HFONT
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->hTextFont
end function

' BORROWED, never owned: keep it alive and destroy it yourself. It is also the MEASURING font
' and is handed straight to the dropdown, so button and list can never disagree about widths.
sub PsComboBox_SetFont( byval hCombo as HWND, byval hTextFont as HFONT )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->hTextFont = hTextFont
    if pCombo->hList then PsPopupMenu_SetFonts( pCombo->hList, hTextFont, 0 )
    pCombo->Refresh()
    PsComboBox_ApplyAutoSize( pCombo )
end sub

function PsComboBox_GetPlaceholderText( byval hCombo as HWND ) as DWSTRING
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return ""
    dim as DWSTRING wszOut = pCombo->wszPlaceholder
    return wszOut
end function

' The placeholder is NOT measured and never affects the width (see LayoutCombo), so this only
' ever needs a repaint -- and only when nothing is selected.
sub PsComboBox_SetPlaceholderText( byval hCombo as HWND, byval Text as DWSTRING )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->wszPlaceholder = Text
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, FALSE )
end sub

' 0 until the list has been created, which happens on the first DropDown. Call
' PsComboBox_EnsureList first if you want to theme it up front. APPEARANCE ONLY -- see the
' header for why mutating rows through it is not supported.
function PsComboBox_GetListHandle( byval hCombo as HWND ) as HWND
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return 0
    return pCombo->hList
end function

' Claims the dropdown's colors permanently: from here on PsComboBox_SetColors will not
' re-derive them, so an explicit choice is never overwritten by a later theme change.
sub PsComboBox_SetListColors( byval hCombo as HWND, byval pColors as PSPOPUPMENU_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if PsComboBox_EnsureList( hCombo ) = 0 then exit sub
    pCombo->bListColorsCustom = true
    PsPopupMenu_SetColors( pCombo->hList, pColors )
end sub

sub PsComboBox_SetListItemHeight( byval hCombo as HWND, byval nItemHeight as long )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    if PsComboBox_EnsureList( hCombo ) = 0 then exit sub
    PsPopupMenu_SetItemHeight( pCombo->hList, nItemHeight )
end sub


' ========================================================================================
' Callbacks
' ========================================================================================
sub PsComboBox_SetPaintCallback( byval hCombo as HWND, byval usersub as CBO_PaintCallbackSub )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->PaintCallback = usersub
    if pCombo->hWin then InvalidateRect( pCombo->hWin, NULL, TRUE )
end sub

sub PsComboBox_SetMessageCallback( byval hCombo as HWND, byval userfunc as CBO_MessageCallbackFunc )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->MessageCallback = userfunc
end sub

sub PsComboBox_SetSelChangeCallback( byval hCombo as HWND, byval usersub as CBO_SelChangeCallbackSub )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->SelChangeCallback = usersub
end sub

sub PsComboBox_SetDropDownCallback( byval hCombo as HWND, byval usersub as CBO_DropDownCallbackSub )
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then exit sub
    pCombo->DropDownCallback = usersub
end sub


' ========================================================================================
' THE message-pump hook. See the contract in PsComboBox.bi.
'
' Two jobs, in this order:
'
'   1. ARM THE REOPEN GUARD. PsPopupMenu's own filter dismisses the chain on an outside click
'      and then returns FALSE ON PURPOSE, so the click still reaches its target ("clicking a
'      toolbar button while a menu is open both closes the menu and presses the button").
'      For a click on OUR OWN BUTTON that is exactly wrong: the list would close here and the
'      WM_LBUTTONDOWN that follows would immediately reopen it, making the button impossible
'      to close by clicking. So when the click is ours AND the list is open, arm a one-shot
'      flag that the very next WM_LBUTTONDOWN consumes. Deterministic; no timer, no
'      close-time heuristic, and it cannot mis-fire because it is cleared unconditionally by
'      the first down-click that sees it.
'
'   2. DELEGATE to PsPopupMenu's filter, which owns keyboard navigation, Escape, and the
'      dismissal itself.
'
' Returns FALSE immediately when this combobox's list is not open, so a host with several
' comboboxes can call it for each one in its pump at negligible cost.
' ========================================================================================
function PsComboBox_FilterMessage( byval hCombo as HWND, byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    dim as PSCOMBOBOX ptr pCombo = GetComboBoxPointer(hCombo)
    if pCombo = 0 then return false
    if pCombo->hList = 0 then return false
    if pCombo->isOpen = false then return false

    if pMsg->message = WM_LBUTTONDOWN then
        if pMsg->hwnd = hCombo then pCombo->bSuppressNextOpen = true
    end if

    return PsPopupMenu_FilterMessage( pCombo->hList, pMsg )
end function


' ========================================================================================
' Geometry self-test, run with PSCOMBOBOX_SELFTEST=1.
'
' Every number is asserted, never eyeballed -- CLAUDE.md's rule, and Learnings.md records
' several "verifications" that were invalid because they were done by eye. Expectations are
' computed from what the control ACTUALLY reports for its own defaults (which are DPI-scaled
' at Create), so this cannot produce a false failure on a high-DPI display.
'
' WHAT THIS CANNOT COVER, and is therefore left to the author's interactive pass: the pixel
' appearance of the chrome and the chevron, hover, the open/close click cycle (including
' clicking the button while the list is up), Tab navigation, outside-click dismissal, and
' anything about the dropdown's own rendering.
' ========================================================================================
dim shared as long gCBO_TestCount
dim shared as long gCBO_FailCount

private sub CBOTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gCBO_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gCBO_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

' Size the test control and report the client size it actually got, so every expectation is
' computed from reality rather than from what we asked for.
private sub CBOTest_SizeTo( byval h as HWND, byval w as long, byval hgt as long, byref cw as long, byref ch as long )
    SetWindowPos( h, 0, 0, 0, w, hgt, SWP_NOZORDER or SWP_NOMOVE )
    dim as RECT rc
    GetClientRect( h, @rc )
    cw = rc.right - rc.left
    ch = rc.bottom - rc.top
end sub

sub PsComboBox_RunSelfTest( byval hWndParent as HWND )
    if environ("PSCOMBOBOX_SELFTEST") <> "1" then exit sub

    gCBO_TestCount = 0
    gCBO_FailCount = 0
    print "=== PsComboBox geometry self-test ==="

    dim as HWND h = PsComboBox_Create( hWndParent, 9901 )
    if h = 0 then
        print "FAIL  create   [PsComboBox_Create returned 0]"
        exit sub
    end if

    dim as PSCOMBOBOX ptr pC = GetComboBoxPointer(h)
    if pC = 0 then
        print "FAIL  pointer   [GetComboBoxPointer returned 0]"
        DestroyWindow(h)
        exit sub
    end if

    dim as string sD                     ' every trace string is built here first: PRINT on a
                                         ' DWSTRING, or on a string & wstring concat,
                                         ' interleaves nulls (Learnings.md).
    dim as long w, h1, cw, ch
    dim as RECT rcBtn, rcTxt, rcChev, rcVis

    ' ---- 1. ideal size is valid BEFORE the control has ever been sized ------------------
    PsComboBox_AddItem( h, "Add to Existing Window" )
    PsComboBox_AddItem( h, "Open a New Window" )
    PsComboBox_GetIdealSize( h, w, h1 )
    sD = "w=" & str(w) & " h=" & str(h1)
    CBOTest_Check( "ideal size before first size", (w > 0) andalso (h1 > 0), sD )

    ' ---- 2. the ideal width is built from the parts we think it is ----------------------
    dim as long ringPad = pC->RingPad()
    dim as long expectW = (2 * ringPad) + pC->nPadLeft + pC->nTextWidth + pC->nTextGap + _
                          pC->nChevronWidth + pC->nPadRight
    sD = "ideal=" & str(w) & " expect=" & str(expectW) & " textW=" & str(pC->nTextWidth)
    CBOTest_Check( "ideal width = ring+pad+text+gap+chevron+pad", (w = expectW), sD )

    dim as long contentH = pC->nTextHeight
    if pC->nChevronHeight > contentH then contentH = pC->nChevronHeight
    dim as long expectH = (2 * ringPad) + pC->nPadTop + contentH + pC->nPadBottom
    sD = "ideal=" & str(h1) & " expect=" & str(expectH)
    CBOTest_Check( "ideal height = ring+pad+content+pad", (h1 = expectH), sD )

    ' ---- 3. textW is the WIDEST item, not the selected one ------------------------------
    dim as long widestBefore = pC->nTextWidth
    PsComboBox_SetCurSel( h, 1 )                    ' the SHORTER caption
    pC->LayoutCombo()
    sD = "before=" & str(widestBefore) & " after=" & str(pC->nTextWidth)
    CBOTest_Check( "width unchanged by selection", (pC->nTextWidth = widestBefore), sD )

    ' ---- 4. deleting the widest item shrinks the measurement ----------------------------
    PsComboBox_DeleteItem( h, 0 )                   ' remove "Add to Existing Window"
    pC->LayoutCombo()
    sD = "widest=" & str(widestBefore) & " now=" & str(pC->nTextWidth)
    CBOTest_Check( "widest re-measured after delete", (pC->nTextWidth < widestBefore), sD )
    PsComboBox_InsertItem( h, 0, "Add to Existing Window" )
    pC->LayoutCombo()
    sD = "restored=" & str(pC->nTextWidth) & " original=" & str(widestBefore)
    CBOTest_Check( "widest restored after insert", (pC->nTextWidth = widestBefore), sD )

    ' ---- 5. selection fix-up across the three mutation sites ----------------------------
    ' The list is now  0:"Add to Existing Window"  1:"Open a New Window".
    PsComboBox_SetCurSel( h, 1 )
    PsComboBox_InsertItem( h, 0, "Inserted Before" )
    sD = "sel=" & str(PsComboBox_GetCurSel(h)) & " expect=2"
    CBOTest_Check( "insert before selection pushes it up", (PsComboBox_GetCurSel(h) = 2), sD )

    PsComboBox_InsertItem( h, 3, "Inserted After" )
    sD = "sel=" & str(PsComboBox_GetCurSel(h)) & " expect=2"
    CBOTest_Check( "insert after selection leaves it", (PsComboBox_GetCurSel(h) = 2), sD )

    PsComboBox_DeleteItem( h, 0 )
    sD = "sel=" & str(PsComboBox_GetCurSel(h)) & " expect=1"
    CBOTest_Check( "delete before selection pulls it down", (PsComboBox_GetCurSel(h) = 1), sD )

    PsComboBox_DeleteItem( h, 1 )
    sD = "sel=" & str(PsComboBox_GetCurSel(h)) & " expect=-1"
    CBOTest_Check( "delete OF selection clears it", (PsComboBox_GetCurSel(h) = -1), sD )

    PsComboBox_Clear( h )
    sD = "count=" & str(PsComboBox_GetCount(h)) & " sel=" & str(PsComboBox_GetCurSel(h))
    CBOTest_Check( "clear empties and clears selection", _
                   (PsComboBox_GetCount(h) = 0) andalso (PsComboBox_GetCurSel(h) = -1), sD )

    ' ---- 6. disabled items ---------------------------------------------------------------
    PsComboBox_AddItem( h, "Add to Existing Window", 100 )
    PsComboBox_AddItem( h, "Open a New Window", 200 )
    PsComboBox_AddItem( h, "Disabled Choice", 300 )
    PsComboBox_SetItemEnabled( h, 2, false )
    dim as boolean bTookDisabled = PsComboBox_SetCurSel( h, 2 )
    sD = "SetCurSel(2)=" & str(bTookDisabled) & " sel=" & str(PsComboBox_GetCurSel(h))
    CBOTest_Check( "SetCurSel refuses a disabled item", _
                   (bTookDisabled = false) andalso (PsComboBox_GetCurSel(h) <> 2), sD )

    PsComboBox_SetCurSel( h, 1 )
    dim as long nNext = pC->NextSelectable( 1, 1 )
    sD = "NextSelectable(1,+1)=" & str(nNext) & " expect=-1 (2 is disabled, no wrap)"
    CBOTest_Check( "arrow skips disabled and CLAMPS", (nNext = -1), sD )

    dim as long nPrev = pC->NextSelectable( pC->itemCount, -1 )
    sD = "NextSelectable(count,-1)=" & str(nPrev) & " expect=1"
    CBOTest_Check( "End lands on last ENABLED item", (nPrev = 1), sD )

    ' ---- 7. -1 / placeholder --------------------------------------------------------------
    PsComboBox_SetCurSel( h, -1 )
    dim as boolean bPH = false
    dim as DWSTRING wszShown = pC->DisplayText( bPH )
    sD = "len=" & str(PsLen(wszShown)) & " isPlaceholder=" & str(bPH)
    CBOTest_Check( "no selection, no placeholder draws nothing", _
                   (PsLen(wszShown) = 0) andalso (bPH = false), sD )

    PsComboBox_SetPlaceholderText( h, "Choose..." )
    wszShown = pC->DisplayText( bPH )
    sD = "len=" & str(PsLen(wszShown)) & " isPlaceholder=" & str(bPH)
    CBOTest_Check( "no selection with placeholder draws it", _
                   (PsLen(wszShown) > 0) andalso (bPH = true), sD )

    dim as long wWithPH, hWithPH
    PsComboBox_GetIdealSize( h, wWithPH, hWithPH )
    PsComboBox_SetPlaceholderText( h, "A Very Much Longer Placeholder String Indeed" )
    dim as long wLongPH, hLongPH
    PsComboBox_GetIdealSize( h, wLongPH, hLongPH )
    sD = "short=" & str(wWithPH) & " long=" & str(wLongPH)
    CBOTest_Check( "placeholder does NOT affect the width", (wWithPH = wLongPH), sD )
    PsComboBox_SetPlaceholderText( h, "" )

    ' ---- 8. rects partition the button, with the ring band reserved ---------------------
    PsComboBox_GetIdealSize( h, w, h1 )
    CBOTest_SizeTo( h, w, h1, cw, ch )
    PsComboBox_GetButtonRect( h, rcBtn )
    PsComboBox_GetTextRect( h, rcTxt )
    PsComboBox_GetChevronRect( h, rcChev )
    PsComboBox_GetVisualRect( h, rcVis )

    sD = "client=" & str(cw) & "x" & str(ch) & " button=" & str(rcBtn.left) & "," & _
         str(rcBtn.top) & "," & str(rcBtn.right) & "," & str(rcBtn.bottom) & " ring=" & str(ringPad)
    CBOTest_Check( "button rect = client deflated by the ring band", _
                   (rcBtn.left = ringPad) andalso (rcBtn.top = ringPad) andalso _
                   (rcBtn.right = cw - ringPad) andalso (rcBtn.bottom = ch - ringPad), sD )

    sD = "visual=" & str(rcVis.left) & "," & str(rcVis.top) & "," & str(rcVis.right) & "," & str(rcVis.bottom)
    CBOTest_Check( "visual rect = the whole client when sized ideally", _
                   (rcVis.left = 0) andalso (rcVis.top = 0) andalso _
                   (rcVis.right = cw) andalso (rcVis.bottom = ch), sD )

    sD = "chevron.right=" & str(rcChev.right) & " button.right-pad=" & str(rcBtn.right - pC->nPadRight)
    CBOTest_Check( "chevron is right-aligned inside the padding", _
                   (rcChev.right = rcBtn.right - pC->nPadRight) andalso _
                   (rcChev.right - rcChev.left = pC->nChevronWidth), sD )

    sD = "text.right=" & str(rcTxt.right) & " chevron.left-gap=" & str(rcChev.left - pC->nTextGap)
    CBOTest_Check( "text ends exactly one gap before the chevron", _
                   (rcTxt.right = rcChev.left - pC->nTextGap), sD )

    sD = "text.left=" & str(rcTxt.left) & " button.left+pad=" & str(rcBtn.left + pC->nPadLeft)
    CBOTest_Check( "text starts exactly one left pad in", _
                   (rcTxt.left = rcBtn.left + pC->nPadLeft), sD )

    dim as RECT rcHit
    dim as boolean bContained = (IntersectRect( @rcHit, @rcBtn, @rcChev ) <> 0)
    if bContained then bContained = EqualRect( @rcHit, @rcChev ) <> 0
    sD = "intersect(button,chevron) = chevron: " & str(bContained)
    CBOTest_Check( "chevron lies wholly inside the button", bContained, sD )

    ' The ring band is reserved whether or not we are focused, so nothing may move when focus
    ' arrives. Assert it against the flag directly -- the control is never really focused here.
    dim as RECT rcBtnUnfocused = rcBtn
    pC->isFocused = true
    pC->bLayoutDirty = true
    PsComboBox_GetButtonRect( h, rcBtn )
    pC->isFocused = false
    sD = "unfocused.left=" & str(rcBtnUnfocused.left) & " focused.left=" & str(rcBtn.left)
    CBOTest_Check( "button does not move when focus arrives", _
                   EqualRect( @rcBtnUnfocused, @rcBtn ) <> 0, sD )

    ' ---- 9. arrow-only mode ---------------------------------------------------------------
    dim as long wText = w
    PsComboBox_SetShowText( h, false )
    dim as long wArrow, hArrow
    PsComboBox_GetIdealSize( h, wArrow, hArrow )
    dim as long expectArrow = (2 * ringPad) + pC->nPadLeft + pC->nChevronWidth + pC->nPadRight
    sD = "arrow=" & str(wArrow) & " expect=" & str(expectArrow) & " withText=" & str(wText)
    CBOTest_Check( "arrow-only width = pad+chevron+pad (no text, NO GAP)", _
                   (wArrow = expectArrow) andalso (wArrow < wText), sD )

    CBOTest_SizeTo( h, wArrow, hArrow, cw, ch )
    PsComboBox_GetTextRect( h, rcTxt )
    sD = "textRect=" & str(rcTxt.left) & "," & str(rcTxt.top) & "," & str(rcTxt.right) & "," & str(rcTxt.bottom)
    CBOTest_Check( "arrow-only text rect is empty", IsRectEmpty( @rcTxt ) <> 0, sD )
    PsComboBox_SetShowText( h, true )

    ' ---- 9b. CBO_TEXT_WHENSELECTED: collapsed while -1, expanded once selected ----------
    PsComboBox_SetTextMode( h, CBO_TEXT_WHENSELECTED )
    PsComboBox_SetCurSel( h, -1 )
    dim as long wCollapsed, hCollapsed
    PsComboBox_GetIdealSize( h, wCollapsed, hCollapsed )
    sD = "collapsed=" & str(wCollapsed) & " arrowOnly=" & str(wArrow) & " withText=" & str(wText)
    CBOTest_Check( "WHENSELECTED with -1 is exactly the arrow-only width", _
                   (wCollapsed = wArrow), sD )

    PsComboBox_GetTextRect( h, rcTxt )
    sD = "textRect empty=" & str(IsRectEmpty( @rcTxt ) <> 0) & _
         " GetShowText=" & str(PsComboBox_GetShowText(h))
    CBOTest_Check( "WHENSELECTED with -1 draws no caption", _
                   (IsRectEmpty( @rcTxt ) <> 0) andalso (PsComboBox_GetShowText(h) = false), sD )

    ' A placeholder cannot surface in this mode -- the only state that would show one is the
    ' collapsed state, which has no caption at all.
    PsComboBox_SetPlaceholderText( h, "Choose an action..." )
    dim as long wCollapsedPH, hCollapsedPH
    PsComboBox_GetIdealSize( h, wCollapsedPH, hCollapsedPH )
    sD = "withPlaceholder=" & str(wCollapsedPH) & " without=" & str(wCollapsed)
    CBOTest_Check( "WHENSELECTED ignores the placeholder entirely", _
                   (wCollapsedPH = wCollapsed), sD )
    PsComboBox_SetPlaceholderText( h, "" )

    PsComboBox_SetCurSel( h, 0 )
    dim as long wExpanded, hExpanded
    PsComboBox_GetIdealSize( h, wExpanded, hExpanded )
    sD = "expanded=" & str(wExpanded) & " withText=" & str(wText) & " collapsed=" & str(wCollapsed)
    CBOTest_Check( "WHENSELECTED expands to the full captioned width on first pick", _
                   (wExpanded = wText) andalso (wExpanded > wCollapsed), sD )

    ' The IDEAL width moved but the WINDOW did not -- nothing resized it, and auto-size is
    ' off. So the caption has nowhere to go and rcText degenerates against the chevron. That
    ' is this mode's documented cost, asserted here rather than left as a surprise: the
    ' control owns its size only when the host says so.
    PsComboBox_GetTextRect( h, rcTxt )
    sD = "textRect=" & str(rcTxt.left) & "," & str(rcTxt.right) & _
         " window still " & str(wArrow) & " wide, ideal now " & str(wExpanded)
    CBOTest_Check( "WHENSELECTED at the OLD window size has nowhere for the caption", _
                   (rcTxt.right = rcTxt.left) andalso (PsComboBox_GetShowText(h) = true), sD )

    ' Apply the new ideal size, which is exactly what a host must do here.
    CBOTest_SizeTo( h, wExpanded, hExpanded, cw, ch )
    PsComboBox_GetTextRect( h, rcTxt )
    PsComboBox_GetChevronRect( h, rcChev )
    sD = "textRect=" & str(rcTxt.left) & ".." & str(rcTxt.right) & _
         " chevron.left-gap=" & str(rcChev.left - pC->nTextGap)
    CBOTest_Check( "WHENSELECTED draws a real caption once the host resizes", _
                   (rcTxt.right > rcTxt.left) andalso _
                   (rcTxt.right = rcChev.left - pC->nTextGap), sD )

    ' With auto-size ON the control applies that width change ITSELF as the boundary is
    ' crossed, which is what makes the mode usable without a SelChange handler at all.
    PsComboBox_SetCurSel( h, -1 )
    PsComboBox_SetAutoSize( h, true )
    dim as RECT rcWin
    GetWindowRect( h, @rcWin )
    dim as long wWinCollapsed = rcWin.right - rcWin.left
    PsComboBox_SetCurSel( h, 0 )
    GetWindowRect( h, @rcWin )
    dim as long wWinExpanded = rcWin.right - rcWin.left
    sD = "window " & str(wWinCollapsed) & " -> " & str(wWinExpanded) & _
         "  (ideal " & str(wCollapsed) & " -> " & str(wExpanded) & ")"
    CBOTest_Check( "auto-size applies the boundary width change itself", _
                   (wWinCollapsed = wCollapsed) andalso (wWinExpanded = wExpanded), sD )
    PsComboBox_SetAutoSize( h, false )

    ' Selection-to-selection must NOT move the width -- the widest-item rule still holds
    ' inside the expanded state, which is the whole reason the boundary case is acceptable.
    PsComboBox_SetCurSel( h, 1 )
    dim as long wOther, hOther
    PsComboBox_GetIdealSize( h, wOther, hOther )
    sD = "sel0=" & str(wExpanded) & " sel1=" & str(wOther)
    CBOTest_Check( "WHENSELECTED width is stable between selections", (wOther = wExpanded), sD )

    ' And back: clearing the selection collapses it again.
    PsComboBox_SetCurSel( h, -1 )
    dim as long wBack, hBack
    PsComboBox_GetIdealSize( h, wBack, hBack )
    sD = "back=" & str(wBack) & " collapsed=" & str(wCollapsed)
    CBOTest_Check( "WHENSELECTED collapses again when cleared", (wBack = wCollapsed), sD )

    ' Disabling the current item clears it, which must collapse the button too.
    PsComboBox_SetCurSel( h, 0 )
    PsComboBox_SetItemEnabled( h, 0, false )
    dim as long wDis, hDis
    PsComboBox_GetIdealSize( h, wDis, hDis )
    sD = "sel=" & str(PsComboBox_GetCurSel(h)) & " width=" & str(wDis) & " collapsed=" & str(wCollapsed)
    CBOTest_Check( "disabling the current item collapses WHENSELECTED", _
                   (PsComboBox_GetCurSel(h) = -1) andalso (wDis = wCollapsed), sD )
    PsComboBox_SetItemEnabled( h, 0, true )

    PsComboBox_SetTextMode( h, CBO_TEXT_ALWAYS )
    PsComboBox_SetCurSel( h, 1 )

    ' ---- 10. the popup id <-> model index round trip ------------------------------------
    ' The rows carry (index + 1) so that index 0 does not collide with PsPopupMenu's separator
    ' id, and so the host's own item ids never enter the command stream.
    dim as HWND hL = PsComboBox_EnsureList( h )
    if hL = 0 then
        CBOTest_Check( "list created", false, "PsComboBox_EnsureList returned 0" )
    else
        PsComboBox_SetCurSel( h, 1 )
        PsComboBox_RebuildListItems( pC )
        dim as long nRows = PsPopupMenu_GetItemCount( hL )
        sD = "rows=" & str(nRows) & " items=" & str(PsComboBox_GetCount(h))
        CBOTest_Check( "one row per item", (nRows = PsComboBox_GetCount(h)), sD )

        ' Row ids are 1-based, so the CHECKED row for selection 1 is id 2.
        dim as boolean bChk1 = PsPopupMenu_GetItemChecked( hL, 2 )
        dim as boolean bChk0 = PsPopupMenu_GetItemChecked( hL, 1 )
        sD = "checked(id2)=" & str(bChk1) & " checked(id1)=" & str(bChk0) & " sel=1"
        CBOTest_Check( "exactly the selected row is checked (id = idx+1)", _
                       (bChk1 = true) andalso (bChk0 = false), sD )

        dim as boolean bDis = PsPopupMenu_GetItemEnabled( hL, 3 )
        sD = "enabled(id3)=" & str(bDis) & " (model item 2 is disabled)"
        CBOTest_Check( "disabled item is disabled in the list", (bDis = false), sD )

        ' ---- 11. SyncListWidth converges ------------------------------------------------
        ' Case A: the button is WIDER than the natural content, so the floor binds and the
        ' popup should land on the button width exactly.
        PsComboBox_GetIdealSize( h, w, h1 )
        dim as long natW, natH
        PsPopupMenu_SetMinWidth( hL, 0 )
        PsPopupMenu_GetIdealSize( hL, natW, natH )

        dim as long wide = natW + 60
        PsComboBox_SyncListWidth( hL, wide )
        dim as long gotW, gotH
        PsPopupMenu_GetIdealSize( hL, gotW, gotH )
        sD = "natural=" & str(natW) & " target=" & str(wide) & " got=" & str(gotW)
        CBOTest_Check( "SyncListWidth hits the button width when the floor binds", _
                       (gotW = wide), sD )

        ' Case B: the button is NARROWER than the natural content. The floor must not bind
        ' and the popup keeps its natural width -- never squeezed below its content.
        dim as long narrow = natW - 40
        if narrow < 1 then narrow = 1
        PsComboBox_SyncListWidth( hL, narrow )
        PsPopupMenu_GetIdealSize( hL, gotW, gotH )
        sD = "natural=" & str(natW) & " target=" & str(narrow) & " got=" & str(gotW)
        CBOTest_Check( "SyncListWidth leaves a wide list alone", (gotW = natW), sD )

        sD = "got=" & str(gotW) & " >= target=" & str(narrow)
        CBOTest_Check( "list is never narrower than requested", (gotW >= narrow), sD )
    end if

    ' ---- 12. the reopen guard is a strict one-shot --------------------------------------
    ' Armed by the pump filter, consumed by the next WM_LBUTTONDOWN. Assert the flag's own
    ' contract here; whether it actually prevents the reopen is the interactive pass's job.
    pC->bSuppressNextOpen = true
    SendMessage( h, WM_LBUTTONDOWN, 0, 0 )
    sD = "bSuppressNextOpen=" & str(pC->bSuppressNextOpen)
    CBOTest_Check( "reopen guard is cleared by one down-click", _
                   (pC->bSuppressNextOpen = false) andalso (pC->isOpen = false), sD )

    ' ---- 13. the control is a tab ITEM, not a tab CONTAINER -----------------------------
    ' Added 2026-07-23, because this SHIPPED BROKEN. CWindow.Create defaults dwExStyle to
    ' WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE and PsComboBox_Create passed only dwStyle, so the
    ' control declared itself a container; the dialog manager descended into it, found no
    ' children, and skipped it. None of the 43 assertions above could see that, because nothing
    ' here had ever asked what the dialog manager sees -- and the demo masked it by also
    ' hosting plain Win32 BUTTONs for Tab to move between.
    dim as ulong exStyle = cast( ulong, GetWindowLongPtr( h, GWL_EXSTYLE ) )
    sD = "exstyle=&h" & hex(exStyle)
    CBOTest_Check( "NOT a control parent (a leaf control is a tab ITEM)", _
                   (exStyle and WS_EX_CONTROLPARENT) = 0, sD )

    dim as ulong wsStyle = cast( ulong, GetWindowLongPtr( h, GWL_STYLE ) )
    sD = "style=&h" & hex(wsStyle)
    CBOTest_Check( "is a tabstop", (wsStyle and WS_TABSTOP) <> 0, sD )

    ' The end-to-end claim rather than the flag: ask the dialog manager to walk to us. The
    ' WS_VISIBLE bit is set directly rather than with ShowWindow -- the walk tests the style
    ' bit, and this way there is no paint, no flash, and no dependence on whether the host runs
    ' the self-test before or after it shows the form.
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsStyle or WS_VISIBLE ) )
    dim as HWND hFirstTab = GetNextDlgTabItem( hWndParent, 0, FALSE )
    dim as boolean bReachable = false
    dim as HWND hScan = hFirstTab
    for i as long = 0 to 63
        if hScan = 0 then exit for
        if hScan = h then
            bReachable = true
            exit for
        end if
        hScan = GetNextDlgTabItem( hWndParent, hScan, FALSE )
        if hScan = hFirstTab then exit for          ' wrapped all the way round
    next
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsStyle ) )
    sD = "first tab item id=" & str(GetDlgCtrlID(hFirstTab)) & " reached=" & str(bReachable)
    CBOTest_Check( "the dialog manager can reach us by Tab", bReachable, sD )

    DestroyWindow(h)

    dim as string sSummary = "=== PsComboBox: " & str(gCBO_TestCount - gCBO_FailCount) & _
                             "/" & str(gCBO_TestCount) & " assertions passed"
    if gCBO_FailCount > 0 then sSummary &= "  (" & str(gCBO_FailCount) & " FAILED)"
    print sSummary
end sub
