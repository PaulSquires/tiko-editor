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

#include once "frmCommandLine.bi"

' The three children. Module-level rather than threaded through, the way every rebuilt
' dialog here holds its controls; cleared in _Show once the loop ends.
dim shared ghCLText   as HWND
dim shared ghCLOK     as HWND
dim shared ghCLCancel as HWND


' ========================================================================================
' THE LAYOUT. Pure -- a function of the client size and the DPI and nothing else, so the
' painter, the placer and the self-test cannot disagree about where anything is.
' ========================================================================================
sub frmCommandLine_ComputeLayout( byval cx as long, _
                                  byval cy as long, _
                                  byref lay as FRMCOMMANDLINE_LAYOUT )

    dim as long mx = AfxScaleX(FRMCMDLINE_MARGIN)
    dim as long my = AfxScaleY(FRMCMDLINE_MARGIN)
    dim as long y  = my

    ' The prompt, full width.
    lay.rcLabel.left   = mx
    lay.rcLabel.right  = cx - mx
    lay.rcLabel.top    = y
    lay.rcLabel.bottom = y + AfxScaleY(FRMCMDLINE_LABEL_H)

    ' The field, full width, directly beneath it.
    y = lay.rcLabel.bottom + AfxScaleY(FRMCMDLINE_GAP)
    lay.rcText.left   = mx
    lay.rcText.right  = cx - mx
    lay.rcText.top    = y
    lay.rcText.bottom = y + AfxScaleY(FRMCMDLINE_TEXT_H)

    ' The footer, pinned to the BOTTOM margin rather than continuing the running y: the two
    ' buttons belong to the window's edge, not to the field above them.
    dim as long bw = AfxScaleX(FRMCMDLINE_BTN_W)
    dim as long bh = AfxScaleY(FRMCMDLINE_BTN_H)
    dim as long bg = AfxScaleX(FRMCMDLINE_BTN_GAP)
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
' A commandline is not saved for non-project files -- those are set when the user opens the
' file. For project related files it is saved to the project file. (Unchanged from the
' original dialog; only where the text comes from has moved.)
' ========================================================================================
private sub frmCommandLine_Commit()

    if ghCLText = 0 then exit sub

    dim as DWSTRING wszText = PsTextBox_GetText( ghCLText )

    if gApp.IsProjectActive then
        gApp.ProjectCommandLine = wszText
    else
        gApp.wszCommandLine = wszText
    end if

    ' POSTED, NOT SENT, and the Enter path is why. This is reached from inside the field's
    ' RichEdit subclass proc, so a synchronous close would run DestroyWindow -- and with it
    ' PsTextBox's own WM_DESTROY cleanup -- while that proc is still on the stack, tearing
    ' down the control currently handling a message. Posting lets the keystroke finish first.
    ' The button path does not need this, but there is no reason for the two to differ.
    PostMessage( HWND_FRMCOMMANDLINE, WM_CLOSE, 0, 0 )

end sub


' ========================================================================================
' ENTER in the field. THE ONLY ROUTE -- a single-line PsTextBox swallows VK_RETURN before
' the pump sees it, so IsDialogMessage never synthesises IDOK here. See frmCommandLine.bi.
' ========================================================================================
private sub frmCommandLine_TextEnterPressed( byval hTextBox as HWND )
    frmCommandLine_Commit()
end sub


' ========================================================================================
' The footer buttons. PsButton reports clicks through this rather than WM_COMMAND.
' ========================================================================================
private sub frmCommandLine_ButtonClick( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMCOMMANDLINE_CMDOK
            frmCommandLine_Commit()
        case IDC_FRMCOMMANDLINE_CMDCANCEL
            ' NOTHING IS WRITTEN. Leaving by any route other than Commit leaves both gApp
            ' fields exactly as they were found.
            SendMessage( HWND_FRMCOMMANDLINE, WM_CLOSE, 0, 0 )
    end select
end sub


' ========================================================================================
' Paint the dialog's own chrome: the background and the prompt. Everything else on screen
' belongs to a child control.
' ========================================================================================
private function frmCommandLine_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as FRMCOMMANDLINE_LAYOUT lay
    frmCommandLine_ComputeLayout( rc.right, rc.bottom, lay )

    ' The same font the field is dressed with (OptionsTheme_ApplyTextBox forces it), so the
    ' prompt and the text under it cannot end up at different sizes.
    buffer.SetForeColor( ghPanel.ForeColor )
    buffer.SetFont( OptionsFont_Base() )
    buffer.PaintText( L(179,"Enter command line arguments:"), @lay.rcLabel, _
                      DT_LEFT or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS )

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' Lay the children out. Fixed-size dialog, so this runs once.
' ========================================================================================
private sub frmCommandLine_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as FRMCOMMANDLINE_LAYOUT lay
    frmCommandLine_ComputeLayout( rc.right, rc.bottom, lay )

    #macro CLPLACE( _h_, _rc_ )
        SetWindowPos( _h_, 0, _rc_.left, _rc_.top, _rc_.right - _rc_.left, _
                      _rc_.bottom - _rc_.top, SWP_NOZORDER or SWP_NOACTIVATE )
    #endmacro

    CLPLACE( ghCLText,   lay.rcText )
    CLPLACE( ghCLOK,     lay.rcOK )
    CLPLACE( ghCLCancel, lay.rcCancel )

end sub


' ========================================================================================
' Theme every child that carries its own colours.
'
' Through the Options dialog's shared builders rather than a hand-copied recipe: this file
' is included after modOptionsRows.inc in the unity build, so the palettes cannot drift.
' ========================================================================================
private sub frmCommandLine_ApplyTheme( byval hwnd as HWND )

    OptionsTheme_ApplyTextBox( ghCLText )

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
    PsTextBox_SetFocusBorderColor( ghCLText, BGR(0,120,215) )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    PsButton_SetColors( ghCLOK,     @bc ) : PsButton_Refresh( ghCLOK )
    PsButton_SetColors( ghCLCancel, @bc ) : PsButton_Refresh( ghCLCancel )

    Theme_ApplyDarkCaption( hwnd )
    AfxRedrawWindow( hwnd )

end sub


' ========================================================================================
' DOES IT FIT, AND IS THE FOOTER WHERE IT CLAIMS TO BE?  (TIKO_COMMANDLINE_SELFTEST=1)
'
' Runs against the PURE layout function, so it needs no window and the dialog does not have
' to be opened -- unlike frmUserTools's, whose layout half can only run once its modal
' window exists. The dialog is fixed size and nothing here scrolls, so a field or a button
' pushed past an edge is simply clipped, silently, and only at a DPI nobody is testing at.
' ========================================================================================
sub frmCommandLine_RunSelfTest()

    if environ("TIKO_COMMANDLINE_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro CLASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? "==== TIKO_COMMANDLINE_SELFTEST ===="

    dim as long cx = AfxScaleX(FRMCMDLINE_CLIENT_W)
    dim as long cy = AfxScaleY(FRMCMDLINE_CLIENT_H)
    dim as long mx = AfxScaleX(FRMCMDLINE_MARGIN)
    dim as long my = AfxScaleY(FRMCMDLINE_MARGIN)

    dim as FRMCOMMANDLINE_LAYOUT lay
    frmCommandLine_ComputeLayout( cx, cy, lay )

    ' ---- 1. everything is inside the client -------------------------------------------
    #macro CLINSIDE( _rc_, _who_ )
        CLASSERT( _rc_.left >= 0, _who_ & " clears the left edge" )
        CLASSERT( _rc_.top >= 0, _who_ & " clears the top edge" )
        CLASSERT( _rc_.right <= cx, _who_ & " clears the right edge" )
        CLASSERT( _rc_.bottom <= cy, _who_ & " clears the bottom edge" )
    #endmacro

    CLINSIDE( lay.rcLabel,  "the prompt" )
    CLINSIDE( lay.rcText,   "the field" )
    CLINSIDE( lay.rcOK,     "OK" )
    CLINSIDE( lay.rcCancel, "Cancel" )

    ' ---- 2. the vertical stack does not collide ---------------------------------------
    CLASSERT( lay.rcLabel.bottom <= lay.rcText.top, "the prompt sits above the field" )
    ' THE ONE THAT MATTERS. At CLIENT_H = 160 there are 26 px between them; shrink the
    ' window or grow the field and the footer buttons climb into the text.
    CLASSERT( lay.rcText.bottom <= lay.rcOK.top, "the field clears the footer" )

    ' ---- 3. margins -------------------------------------------------------------------
    CLASSERT( lay.rcLabel.left = mx, "the prompt starts at the left margin" )
    CLASSERT( lay.rcText.left = mx, "the field starts at the left margin" )
    CLASSERT( lay.rcText.right = cx - mx, "the field ends at the right margin" )
    CLASSERT( lay.rcLabel.top = my, "the prompt starts at the top margin" )
    CLASSERT( lay.rcCancel.bottom = cy - my, "the footer sits on the bottom margin" )
    CLASSERT( lay.rcOK.bottom = lay.rcCancel.bottom, "both buttons share one baseline" )
    CLASSERT( lay.rcOK.top = lay.rcCancel.top, "both buttons share one top" )

    ' ---- 4. the footer, left to right: OK then Cancel ----------------------------------
    ' Order asserted because it is a convention a rebuild can silently reverse -- and the
    ' screenshot this was built to shows OK inboard of Cancel.
    CLASSERT( lay.rcOK.right < lay.rcCancel.left, "OK sits to the LEFT of Cancel" )
    CLASSERT( lay.rcCancel.left - lay.rcOK.right = AfxScaleX(FRMCMDLINE_BTN_GAP), _
              "the two buttons are one scaled gap apart" )
    CLASSERT( lay.rcCancel.right = cx - mx, "Cancel ends at the right margin" )
    CLASSERT( (lay.rcOK.right - lay.rcOK.left) = (lay.rcCancel.right - lay.rcCancel.left), _
              "the two buttons are the same width" )
    CLASSERT( (lay.rcOK.bottom - lay.rcOK.top) = (lay.rcCancel.bottom - lay.rcCancel.top), _
              "the two buttons are the same height" )

    ' ---- 5. the field is the widest thing on the dialog --------------------------------
    ' Not decoration: a command line is routinely long, and a field narrower than the
    ' footer would mean the layout had stopped being about the field.
    dim as long nFieldW  = lay.rcText.right - lay.rcText.left
    dim as long nFooterW = lay.rcCancel.right - lay.rcOK.left
    CLASSERT( nFieldW > nFooterW, "the field is wider than the two buttons together" )

    ' ---- 6. the function is pure -------------------------------------------------------
    ' Called twice with the same arguments into a DIRTY struct: a layout that reads any of
    ' its own output, or any state outside its parameters, diverges here.
    dim as FRMCOMMANDLINE_LAYOUT lay2
    lay2.rcText.left = 999 : lay2.rcOK.top = 999 : lay2.rcCancel.right = 999
    frmCommandLine_ComputeLayout( cx, cy, lay2 )
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
    CLASSERT( bSame, "the layout is pure -- a second call into a dirty struct agrees" )

    ' ---- 7. it still fits with the footer scaled up ------------------------------------
    ' The layout has to survive a taller button, which is what a larger base font would
    ' bring. Same client, footer height doubled: the field must still clear it.
    dim as FRMCOMMANDLINE_LAYOUT lay3
    frmCommandLine_ComputeLayout( cx, cy + AfxScaleY(FRMCMDLINE_BTN_H), lay3 )
    CLASSERT( lay3.rcText.top = lay.rcText.top, "growing the client does not move the field" )
    CLASSERT( lay3.rcOK.top > lay.rcOK.top, "growing the client pushes the footer down" )

    sLine = "==== TIKO_COMMANDLINE_SELFTEST: " & str(nPass) & " passed, " & str(nFail) & " failed ===="
    ? sLine

end sub


' ========================================================================================
' Window procedure
' ========================================================================================
private function frmCommandLine_WndProc( _
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
            return frmCommandLine_OnPaint( hwnd )

        case WM_SIZE
            frmCommandLine_PositionWindows( hwnd )
            return 0

        case WM_COMMAND
            ' PsButton routes clicks through its own callback, not WM_COMMAND. What still
            ' arrives here is what IsDialogMessage synthesises: IDCANCEL for Escape, and
            ' IDOK for Enter when the focus is somewhere that does not claim it itself.
            select case loword(wParam)
                case IDOK
                    frmCommandLine_Commit()
                    return 0
                case IDCANCEL
                    SendMessage( hwnd, WM_CLOSE, 0, 0 )
                    return 0
            end select

        case WM_CLOSE
            ' Re-enable the parent, keeping its z-order.
            EnableWindow( GetParent(hwnd), true )
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMCOMMANDLINE = 0
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProcW( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmCommandLine_Show
' ========================================================================================
function frmCommandLine_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    HWND_FRMCOMMANDLINE = pWindow->Create( hWndParent, L(178,"Command Line"), _
        @frmCommandLine_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMCMDLINE_CLIENT_W, FRMCMDLINE_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the field --------------------------------------------------------------------
    ' CREATION ORDER IS THE TAB ORDER: these controls live on an ordinary CWindow, not a
    ' dialog template, so GetNextDlgTabItem walks the child Z-order and Z-order is creation
    ' order. field -> OK -> Cancel.
    ghCLText = PsTextBox_Create( HWND_FRMCOMMANDLINE, IDC_FRMCOMMANDLINE_TEXTBOX1 )
    PsTextBox_SetEnterPressedCallback( ghCLText, @frmCommandLine_TextEnterPressed )
    ' Breathing room inside the frame -- the caret and the first character sat hard against
    ' the border. SCALED HERE: PsTextBox_SetMargins hands the value straight to
    ' EM_SETMARGINS and does no scaling of its own, so an unscaled literal would shrink to
    ' nothing at 175%. The right margin is the same value, so a command line long enough to
    ' scroll does not run its tail into the opposite border either.
    PsTextBox_SetMargins( ghCLText, AfxScaleX(FRMCMDLINE_TEXT_PAD), AfxScaleX(FRMCMDLINE_TEXT_PAD) )
    ' The dialog opens on an existing command line far more often than on an empty one, and
    ' the usual next action is to replace it wholesale.
    PsTextBox_SetSelectOnFocus( ghCLText, true )

    ' ---- footer -----------------------------------------------------------------------
    ghCLOK = PsButton_Create( HWND_FRMCOMMANDLINE, IDC_FRMCOMMANDLINE_CMDOK )
    PsButton_SetText( ghCLOK, L(0,"OK") )
    PsButton_SetFont( ghCLOK, OptionsFont_Base() )
    ' APPEARANCE ONLY -- it marks OK, it does not claim Enter. See frmCommandLine.bi.
    PsButton_SetDefault( ghCLOK, true )
    PsButton_SetClickCallback( ghCLOK, @frmCommandLine_ButtonClick )

    ghCLCancel = PsButton_Create( HWND_FRMCOMMANDLINE, IDC_FRMCOMMANDLINE_CMDCANCEL )
    PsButton_SetText( ghCLCancel, L(1,"Cancel") )
    PsButton_SetFont( ghCLCancel, OptionsFont_Base() )
    PsButton_SetClickCallback( ghCLCancel, @frmCommandLine_ButtonClick )

    ShowWindow( ghCLText, SW_SHOW )
    ShowWindow( ghCLOK, SW_SHOW )
    ShowWindow( ghCLCancel, SW_SHOW )

    frmCommandLine_ApplyTheme( HWND_FRMCOMMANDLINE )
    frmCommandLine_PositionWindows( HWND_FRMCOMMANDLINE )

    ' Seed the field. Same split the commit uses, in the same order.
    if gApp.IsProjectActive then
        PsTextBox_SetText( ghCLText, gApp.ProjectCommandLine )
    else
        PsTextBox_SetText( ghCLText, gApp.wszCommandLine )
    end if

    ShowWindow( HWND_FRMCOMMANDLINE, SW_SHOW )
    UpdateWindow( HWND_FRMCOMMANDLINE )

    ' The field is what the dialog is for -- and IsDialogMessage will not Tab anywhere until
    ' something already holds the focus.
    SetFocus( ghCLText )

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
            SendMessage( HWND_FRMCOMMANDLINE, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMCOMMANDLINE, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend

    if pWindow then delete(pWindow)

    ghCLText = 0 : ghCLOK = 0 : ghCLCancel = 0

    function = 0

end function
