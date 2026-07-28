'    tiko editor - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#include once "frmInputBox.bi"

' The three children. Module-level rather than threaded through, the way every rebuilt dialog
' here holds its controls; cleared in _Show once the loop ends. The dialog is MODAL, so a
' single shared set is safe -- only one is ever open at a time.
dim shared ghIBText   as HWND
dim shared ghIBOK     as HWND
dim shared ghIBCancel as HWND

' The prompt to paint, seeded by _Show. Caption goes straight into the window title, so it
' needs no module copy; the prompt does, because the painter reads it on every WM_PAINT.
dim shared gwszIBPrompt as DWSTRING

' How the dialog closed, and the field's text at the moment it did. gIBResult defaults to
' IDCANCEL so every non-commit exit (Cancel, Escape, the close X, Alt+F4) reports the same
' thing; only the commit path raises it to IDOK. gwszIBResultText is captured in WM_CLOSE,
' which is the single point both the OK and the Cancel paths pass through.
dim shared gIBResult       as LRESULT
dim shared gwszIBResultText as DWSTRING


' ========================================================================================
' THE LAYOUT. Pure -- a function of the client size and the DPI and nothing else, so the
' painter, the placer and the self-test cannot disagree about where anything is.
' ========================================================================================
sub frmInputBox_ComputeLayout( byval cx as long, _
                               byval cy as long, _
                               byref lay as FRMINPUTBOX_LAYOUT )

    dim as long mx = AfxScaleX(FRMINPUTBOX_MARGIN)
    dim as long my = AfxScaleY(FRMINPUTBOX_MARGIN)
    dim as long y  = my

    ' The prompt, full width.
    lay.rcLabel.left   = mx
    lay.rcLabel.right  = cx - mx
    lay.rcLabel.top    = y
    lay.rcLabel.bottom = y + AfxScaleY(FRMINPUTBOX_LABEL_H)

    ' The field, full width, directly beneath it.
    y = lay.rcLabel.bottom + AfxScaleY(FRMINPUTBOX_GAP)
    lay.rcText.left   = mx
    lay.rcText.right  = cx - mx
    lay.rcText.top    = y
    lay.rcText.bottom = y + AfxScaleY(FRMINPUTBOX_TEXT_H)

    ' The footer, pinned to the BOTTOM margin rather than continuing the running y: the two
    ' buttons belong to the window's edge, not to the field above them.
    dim as long bw = AfxScaleX(FRMINPUTBOX_BTN_W)
    dim as long bh = AfxScaleY(FRMINPUTBOX_BTN_H)
    dim as long bg = AfxScaleX(FRMINPUTBOX_BTN_GAP)
    dim as long by = cy - my - bh

    lay.rcCancel.right  = cx - mx
    lay.rcCancel.left   = lay.rcCancel.right - bw
    lay.rcCancel.top    = by
    lay.rcCancel.bottom = by + bh

    lay.rcOK.right  = lay.rcCancel.left - bg
    lay.rcOK.left   = lay.rcOK.right - bw
    lay.rcOK.top    = by
    lay.rcOK.bottom = by + bh

end sub


' ========================================================================================
' THE ONE COMMIT PATH. Reached from the OK button, and from Enter in the field.
'
' It only records the OUTCOME -- the field's text is captured in WM_CLOSE, the single point
' both the commit and the cancel paths share. So there is nothing here but the result flag
' and the request to close.
' ========================================================================================
private sub frmInputBox_Commit()

    if ghIBText = 0 then exit sub

    gIBResult = IDOK

    ' POSTED, NOT SENT, and the Enter path is why. This is reached from inside the field's
    ' RichEdit subclass proc, so a synchronous close would run DestroyWindow -- and with it
    ' PsTextBox's own WM_DESTROY cleanup -- while that proc is still on the stack, tearing
    ' down the control currently handling a message. Posting lets the keystroke finish first.
    ' The button path does not need this, but there is no reason for the two to differ.
    PostMessage( HWND_FRMINPUTBOX, WM_CLOSE, 0, 0 )

end sub


' ========================================================================================
' ENTER in the field. THE ONLY ROUTE -- a single-line PsTextBox swallows VK_RETURN before
' the pump sees it, so IsDialogMessage never synthesises IDOK here. See frmInputBox.bi.
' ========================================================================================
private sub frmInputBox_TextEnterPressed( byval hTextBox as HWND )
    frmInputBox_Commit()
end sub


' ========================================================================================
' The footer buttons. PsButton reports clicks through this rather than WM_COMMAND.
' ========================================================================================
private sub frmInputBox_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMINPUTBOX_CMDOK
            frmInputBox_Commit()
        case IDC_FRMINPUTBOX_CMDCANCEL
            ' gIBResult stays at its IDCANCEL default. The field's text is still captured in
            ' WM_CLOSE, so the caller gets the current text and the IDCANCEL to disregard it.
            SendMessage( HWND_FRMINPUTBOX, WM_CLOSE, 0, 0 )
    end select
end sub


' ========================================================================================
' Paint the dialog's own chrome: the background and the prompt. Everything else on screen
' belongs to a child control.
' ========================================================================================
private function frmInputBox_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as FRMINPUTBOX_LAYOUT lay
    frmInputBox_ComputeLayout( rc.right, rc.bottom, lay )

    ' The same font the field is dressed with (OptionsTheme_ApplyTextBox forces it), so the
    ' prompt and the text under it cannot end up at different sizes. An empty prompt draws
    ' nothing -- the field just sits under a blank band -- which is the caller's call to make.
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( OptionsFont_Base() )
    buffer.PaintText( gwszIBPrompt, @lay.rcLabel, _
                      DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Lay the children out. Fixed-size dialog, so this runs once.
' ========================================================================================
private sub frmInputBox_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as FRMINPUTBOX_LAYOUT lay
    frmInputBox_ComputeLayout( rc.right, rc.bottom, lay )

    #macro IBPLACE( _h_, _rc_ )
        SetWindowPos( _h_, 0, _rc_.left, _rc_.top, _rc_.right - _rc_.left, _
                      _rc_.bottom - _rc_.top, SWP_NOZORDER or SWP_NOACTIVATE )
    #endmacro

    IBPLACE( ghIBText,   lay.rcText )
    IBPLACE( ghIBOK,     lay.rcOK )
    IBPLACE( ghIBCancel, lay.rcCancel )

end sub


' ========================================================================================
' Theme every child that carries its own colours.
'
' Through the Options dialog's shared builders rather than a hand-copied recipe: this file
' is included after modOptionsRows.inc in the unity build, so the palettes cannot drift.
' ========================================================================================
private sub frmInputBox_ApplyTheme( byval hwnd as HWND )

    OptionsTheme_ApplyTextBox( ghIBText )

    ' THE FOCUS BORDER IS A LITERAL BLUE, NOT A THEME KEY, AND THAT IS DELIBERATE.
    '
    ' OptionsTheme_ApplyTextBox leaves it at ghPanel.ForeColorHot, which is panel.texthot --
    ' %LIGHT_GRAY in default_dark and 97,97,97 in default_light. Grey in both, so a focused
    ' field was not highlighted so much as very slightly relit.
    '
    ' There is no theme key to reach for instead. The obvious candidate, ui.accent, falls
    ' back to toptabs.iconselected: 49,73,123 (dark navy) in default_dark but 204,233,252 (a
    ' pale tint) in default_light. As a FILL behind white text that works, which is what the
    ' About box uses it for; as a one-pixel hairline the light value is invisible against a
    ' 243,243,243 page. That is exactly the trap CLAUDE.md records, so this does not take it.
    '
    ' BGR(0,120,215) is the Windows accent and PsTextBox's own documented default for this
    ' field -- it reads against both a near-black and a near-white background, which is the
    ' whole requirement for a focus ring. Overriding the shared builder rather than changing
    ' it: every other PsTextBox in the Options pages keeps the grey it was designed with.
    PsTextBox_SetFocusBorderColor( ghIBText, BGR(0,120,215) )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghIBOK,     @bc ) : PsButton_Refresh( ghIBOK )
    PsButton_SetColors( ghIBCancel, @bc ) : PsButton_Refresh( ghIBCancel )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' DOES IT FIT, AND IS THE FOOTER WHERE IT CLAIMS TO BE?  (TIKO_INPUTBOX_SELFTEST=1)
'
' Runs against the PURE layout function, so it needs no window and the dialog does not have
' to be opened -- unlike frmUserTools's, whose layout half can only run once its modal
' window exists. The dialog is fixed size and nothing here scrolls, so a field or a button
' pushed past an edge is simply clipped, silently, and only at a DPI nobody is testing at.
' ========================================================================================
sub frmInputBox_RunSelfTest()

    if environ("TIKO_INPUTBOX_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro IBASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? "==== TIKO_INPUTBOX_SELFTEST ===="

    dim as long cx = AfxScaleX(FRMINPUTBOX_CLIENT_W)
    dim as long cy = AfxScaleY(FRMINPUTBOX_CLIENT_H)
    dim as long mx = AfxScaleX(FRMINPUTBOX_MARGIN)
    dim as long my = AfxScaleY(FRMINPUTBOX_MARGIN)

    dim as FRMINPUTBOX_LAYOUT lay
    frmInputBox_ComputeLayout( cx, cy, lay )

    ' ---- 1. everything is inside the client -------------------------------------------
    #macro IBINSIDE( _rc_, _who_ )
        IBASSERT( _rc_.left >= 0, _who_ & " clears the left edge" )
        IBASSERT( _rc_.top >= 0, _who_ & " clears the top edge" )
        IBASSERT( _rc_.right <= cx, _who_ & " clears the right edge" )
        IBASSERT( _rc_.bottom <= cy, _who_ & " clears the bottom edge" )
    #endmacro

    IBINSIDE( lay.rcLabel,  "the prompt" )
    IBINSIDE( lay.rcText,   "the field" )
    IBINSIDE( lay.rcOK,     "OK" )
    IBINSIDE( lay.rcCancel, "Cancel" )

    ' ---- 2. the vertical stack does not collide ---------------------------------------
    IBASSERT( lay.rcLabel.bottom <= lay.rcText.top, "the prompt sits above the field" )
    ' THE ONE THAT MATTERS. At CLIENT_H = 160 there are 26 px between them; shrink the
    ' window or grow the field and the footer buttons climb into the text.
    IBASSERT( lay.rcText.bottom <= lay.rcOK.top, "the field clears the footer" )

    ' ---- 3. margins -------------------------------------------------------------------
    IBASSERT( lay.rcLabel.left = mx, "the prompt starts at the left margin" )
    IBASSERT( lay.rcText.left = mx, "the field starts at the left margin" )
    IBASSERT( lay.rcText.right = cx - mx, "the field ends at the right margin" )
    IBASSERT( lay.rcLabel.top = my, "the prompt starts at the top margin" )
    IBASSERT( lay.rcCancel.bottom = cy - my, "the footer sits on the bottom margin" )
    IBASSERT( lay.rcOK.bottom = lay.rcCancel.bottom, "both buttons share one baseline" )
    IBASSERT( lay.rcOK.top = lay.rcCancel.top, "both buttons share one top" )

    ' ---- 4. the footer, left to right: OK then Cancel ----------------------------------
    ' Order asserted because it is a convention a rebuild can silently reverse -- and the
    ' screenshot this was built to shows OK inboard of Cancel.
    IBASSERT( lay.rcOK.right < lay.rcCancel.left, "OK sits to the LEFT of Cancel" )
    IBASSERT( lay.rcCancel.left - lay.rcOK.right = AfxScaleX(FRMINPUTBOX_BTN_GAP), _
              "the two buttons are one scaled gap apart" )
    IBASSERT( lay.rcCancel.right = cx - mx, "Cancel ends at the right margin" )
    IBASSERT( (lay.rcOK.right - lay.rcOK.left) = (lay.rcCancel.right - lay.rcCancel.left), _
              "the two buttons are the same width" )
    IBASSERT( (lay.rcOK.bottom - lay.rcOK.top) = (lay.rcCancel.bottom - lay.rcCancel.top), _
              "the two buttons are the same height" )

    ' ---- 5. the field is the widest thing on the dialog --------------------------------
    ' Not decoration: a text entry is routinely long, and a field narrower than the footer
    ' would mean the layout had stopped being about the field.
    dim as long nFieldW  = lay.rcText.right - lay.rcText.left
    dim as long nFooterW = lay.rcCancel.right - lay.rcOK.left
    IBASSERT( nFieldW > nFooterW, "the field is wider than the two buttons together" )

    ' ---- 6. the function is pure -------------------------------------------------------
    ' Called twice with the same arguments into a DIRTY struct: a layout that reads any of
    ' its own output, or any state outside its parameters, diverges here.
    dim as FRMINPUTBOX_LAYOUT lay2
    lay2.rcText.left = 999 : lay2.rcOK.top = 999 : lay2.rcCancel.right = 999
    frmInputBox_ComputeLayout( cx, cy, lay2 )
    dim as boolean bSame = true
    if lay2.rcLabel.left <> lay.rcLabel.left then bSame = false
    if lay2.rcLabel.top <> lay.rcLabel.top then bSame = false
    if lay2.rcText.left <> lay.rcText.left then bSame = false
    if lay2.rcText.right <> lay.rcText.right then bSame = false
    if lay2.rcText.top <> lay.rcText.top then bSame = false
    if lay2.rcText.bottom <> lay.rcText.bottom then bSame = false
    if lay2.rcOK.left <> lay.rcOK.left then bSame = false
    if lay2.rcOK.top <> lay.rcOK.top then bSame = false
    if lay2.rcCancel.right <> lay.rcCancel.right then bSame = false
    IBASSERT( bSame, "the layout is pure -- a second call into a dirty struct agrees" )

    ' ---- 7. it still fits with the footer scaled up ------------------------------------
    ' The layout has to survive a taller button, which is what a larger base font would
    ' bring. Same client, footer height doubled: the field must still clear it.
    dim as FRMINPUTBOX_LAYOUT lay3
    frmInputBox_ComputeLayout( cx, cy + AfxScaleY(FRMINPUTBOX_BTN_H), lay3 )
    IBASSERT( lay3.rcText.top = lay.rcText.top, "growing the client does not move the field" )
    IBASSERT( lay3.rcOK.top > lay.rcOK.top, "growing the client pushes the footer down" )

    sLine = "==== TIKO_INPUTBOX_SELFTEST: " & str(nPass) & " passed, " & str(nFail) & " failed ===="
    ? sLine

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmInputBox_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            ' Modal popup: disable the parent for the duration.
            EnableWindow( GetParent(hwnd), false )
            return 0

        case WM_ERASEBKGND
            return 1

        case WM_PAINT
            return frmInputBox_OnPaint( hwnd )

        case WM_SIZE
            frmInputBox_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes clicks through its own callback, not WM_COMMAND. What still
            ' arrives here is what IsDialogMessage synthesises: IDCANCEL for Escape, and
            ' IDOK for Enter when the focus is somewhere that does not claim it itself.
            select case loword(wParam)
                case IDOK
                    frmInputBox_Commit()
                    return 0
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_CLOSE
            ' THE SINGLE CAPTURE POINT. Every exit -- OK, Cancel, Escape, the close X,
            ' Alt+F4 -- passes through here, so reading the field once, here, gives the
            ' caller the current text on all of them. gIBResult already carries IDOK/IDCANCEL.
            if ghIBText then gwszIBResultText = PsTextBox_GetText( ghIBText )
            ' Re-enable the parent, keeping its z-order.
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMINPUTBOX = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProcW( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmInputBox_Show
'
' wszCaption -> the window title. wszPrompt -> the painted prompt above the field ("" for
' none). wszText seeds the field and, on close, receives the field's final text (for both
' OK and Cancel). Returns IDOK (OK / Enter) or IDCANCEL (Cancel / Escape / close X / Alt+F4).
' ========================================================================================
function frmInputBox_Show( byval hWndParent as HWND, _
                           byval wszCaption as DWSTRING, _
                           byval wszPrompt  as DWSTRING, _
                           byref wszText    as DWSTRING ) as LRESULT

    ' Reset the outcome. Default IDCANCEL, so anything but a commit reports a cancel; and a
    ' safe default text in case some future path skips WM_CLOSE.
    gIBResult        = IDCANCEL
    gwszIBResultText = wszText
    gwszIBPrompt     = wszPrompt

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    HWND_FRMINPUTBOX = pWindow->Create( hWndParent, wszCaption, _
        @frmInputBox_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMINPUTBOX_CLIENT_W, FRMINPUTBOX_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the field --------------------------------------------------------------------
    ' CREATION ORDER IS THE TAB ORDER: these controls live on an ordinary CWindow, not a
    ' dialog template, so GetNextDlgTabItem walks the child Z-order and Z-order is creation
    ' order. field -> OK -> Cancel.
    ghIBText = PsTextBox_Create( HWND_FRMINPUTBOX, IDC_FRMINPUTBOX_TEXTBOX1 )
    PsTextBox_SetEnterPressedCallback( ghIBText, @frmInputBox_TextEnterPressed )
    ' Breathing room inside the frame -- the caret and the first character sat hard against
    ' the border. SCALED HERE: PsTextBox_SetMargins hands the value straight to EM_SETMARGINS
    ' and does no scaling of its own, so an unscaled literal would shrink to nothing at 175%.
    ' The right margin is the same value, so a long entry that scrolls does not run its tail
    ' into the opposite border either.
    PsTextBox_SetMargins( ghIBText, AfxScaleX(FRMINPUTBOX_TEXT_PAD), AfxScaleX(FRMINPUTBOX_TEXT_PAD) )
    ' The dialog opens on an existing value far more often than on an empty one, and the usual
    ' next action is to replace it wholesale.
    PsTextBox_SetSelectOnFocus( ghIBText, true )

    ' ---- footer -----------------------------------------------------------------------
    ghIBOK = PsButton_Create( HWND_FRMINPUTBOX, IDC_FRMINPUTBOX_CMDOK )
    PsButton_SetText( ghIBOK, L(0,"OK") )
    PsButton_SetFont( ghIBOK, OptionsFont_Base() )
    ' APPEARANCE ONLY -- it marks OK, it does not claim Enter. See frmInputBox.bi.
    PsButton_SetDefault( ghIBOK, true )
    PsButton_SetClickCallback( ghIBOK, @frmInputBox_ButtonClick )

    ghIBCancel = PsButton_Create( HWND_FRMINPUTBOX, IDC_FRMINPUTBOX_CMDCANCEL )
    PsButton_SetText( ghIBCancel, L(1,"Cancel") )
    PsButton_SetFont( ghIBCancel, OptionsFont_Base() )
    PsButton_SetClickCallback( ghIBCancel, @frmInputBox_ButtonClick )

    ShowWindow( ghIBText, SW_SHOW )
    ShowWindow( ghIBOK, SW_SHOW )
    ShowWindow( ghIBCancel, SW_SHOW )

    frmInputBox_ApplyTheme( HWND_FRMINPUTBOX )
    frmInputBox_PositionWindows( HWND_FRMINPUTBOX )

    ' Seed the field with the caller's incoming text.
    PsTextBox_SetText( ghIBText, wszText )

    ShowWindow( HWND_FRMINPUTBOX, SW_SHOW )
    UpdateWindow( HWND_FRMINPUTBOX )

    ' The field is what the dialog is for -- and IsDialogMessage will not Tab anywhere until
    ' something already holds the focus.
    SetFocus( ghIBText )

    ' ---- message loop -----------------------------------------------------------------
    ' Its own loop, not CWindow.DoEvents: that has no *_FilterMessage hook, and the field
    ' needs one.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' MANDATORY. The field's right-click Cut/Copy/Paste menu is a PsPopupMenu; without
        ' this it has no keyboard navigation and never closes on an outside click.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' ESCAPE cancels -- handled explicitly, and AFTER the popup filter above so an open
        ' context menu closes on Escape first rather than the whole dialog going with it.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMINPUTBOX, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMINPUTBOX, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    ghIBText = 0 : ghIBOK = 0 : ghIBCancel = 0

    ' Hand the captured text and the outcome back to the caller.
    wszText  = gwszIBResultText
    function = gIBResult

end function
