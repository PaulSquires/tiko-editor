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

#pragma once

' ========================================================================================
' Enter the command line arguments passed to the built executable when it is run.
'
' Rebuilt on the Ps* family (2026-07-28). It was the last raw-Win32 dialog of its shape: a
' system LABEL, an EDIT with WS_EX_CLIENTEDGE and two stock BUTTONs on a system-grey
' CWindow, none of which follow tiko's theme. The layout is unchanged -- prompt, one
' full-width field, OK / Cancel bottom right -- because the layout was never the problem.
'
' ----------------------------------------------------------------------------------------
' ENTER IS THE WHOLE INTERACTION, AND IT DOES NOT ARRIVE THE WAY IT USED TO.
'
' The old OK button carried BS_DEFPUSHBUTTON, so Enter anywhere in the dialog fired it. Two
' separate things about the replacement mean that route no longer exists:
'
'   1. A single-line PsTextBox SWALLOWS VK_RETURN in its own WM_KEYDOWN and returns 0
'      (PsTextBox.inc, the `bSingleLine` arm). It never reaches the pump, so IsDialogMessage
'      never synthesises the WM_COMMAND / IDOK that every other dialog here relies on. The
'      control offers TXT_EnterPressedCallbackSub instead, and that is the ONLY route.
'   2. PsButton_SetDefault is APPEARANCE ONLY -- one accent border, no key claim. It marks
'      OK as the default; it does not make Enter fire it.
'
' So the Enter path is wired explicitly (SetEnterPressedCallback -> frmCommandLine_Commit)
' and IDOK is ALSO handled in WM_COMMAND, for the case where focus has left the field: a
' focused PsButton claims Enter itself, and IsDialogMessage covers anything else. Deleting
' either half leaves a dialog whose one field cannot be submitted from the keyboard, and
' nothing in a build or an assertion sweep would say so.
'
' ----------------------------------------------------------------------------------------
' PUMP OBLIGATION, MANDATORY: PsTextBox_FilterMessage. The field's right-click Cut/Copy/
' Paste menu is a PsPopupMenu; without the filter it has no keyboard navigation and never
' closes on an outside click. That is why this dialog runs its own GetMessage loop instead
' of CWindow.DoEvents, which has no filter hook. Escape is handled AFTER the filter, so an
' open context menu closes on Escape before the dialog does.
'
' ----------------------------------------------------------------------------------------
' THE PROMPT IS PAINTED, NOT A CONTROL. One less child window, and it recolours with the
' theme for free. frmUserToolKey does the same with its three toggle captions.
'
' NO SELF-TEST FOR THE COMMIT RULE, deliberately: it is four lines and the dialog is modal,
' so an assertion would have to open it. The geometry IS asserted -- see below.
' ========================================================================================

' The label is painted, so it has no control id. 1000 is left unused rather than recycled.
#Define IDC_FRMCOMMANDLINE_TEXTBOX1                 1001
#Define IDC_FRMCOMMANDLINE_CMDOK                    1002
#Define IDC_FRMCOMMANDLINE_CMDCANCEL                1003

' ----------------------------------------------------------------------------------------
' Shell metrics, UNSCALED -- frmCommandLine_ComputeLayout scales them.
'
' Walking the sum: 20 margin + 24 prompt + 6 + 32 field = 82, and the footer sits at
' CLIENT_H - MARGIN - BTN_H = 108. 26 px clear. Wider than the old 420 because a command
' line is the one thing here that is routinely long, and the field is the whole dialog.
' ----------------------------------------------------------------------------------------
#Define FRMCMDLINE_CLIENT_W                          520
#Define FRMCMDLINE_CLIENT_H                          160
#Define FRMCMDLINE_MARGIN                             20
#Define FRMCMDLINE_LABEL_H                            24
#Define FRMCMDLINE_GAP                                 6
#Define FRMCMDLINE_TEXT_H                             32
#Define FRMCMDLINE_TEXT_PAD                            8     ' inside the field, per side
#Define FRMCMDLINE_BTN_W                              92
#Define FRMCMDLINE_BTN_H                              32
#Define FRMCMDLINE_BTN_GAP                             8

dim shared HWND_FRMCOMMANDLINE as HWND

' ----------------------------------------------------------------------------------------
' ONE LAYOUT, THREE READERS. frmUserToolKey's .bi has to warn that its _OnPaint and
' _PositionWindows "walk the same sum" and must be kept in step by hand; that is a standing
' invitation to drift. Here the sum is computed once, by a PURE function of the client size,
' and the painter, the placer and the self-test all read the same struct. Nothing to keep in
' step, and the geometry is assertable without opening a modal window.
' ----------------------------------------------------------------------------------------
type FRMCOMMANDLINE_LAYOUT
    rcLabel  as RECT
    rcText   as RECT
    rcOK     as RECT
    rcCancel as RECT
end type

declare sub      frmCommandLine_ComputeLayout( byval cx as long, _
                                               byval cy as long, _
                                               byref lay as FRMCOMMANDLINE_LAYOUT )
declare function frmCommandLine_Show( byval hWndParent as HWND ) as LRESULT

' Env-gated (TIKO_COMMANDLINE_SELFTEST=1). Runs from frmMain's startup block rather than
' from _Show: the layout is pure, so the suite needs no window and the dialog -- which is
' MODAL -- never has to be opened to run it. That is the payoff for computing the geometry
' in one pure function instead of walking a running y in the painter and the placer.
declare sub      frmCommandLine_RunSelfTest()
