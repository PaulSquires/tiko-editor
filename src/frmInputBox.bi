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
' frmInputBox -- a reusable single-field text input dialog.
'
' Generalised from frmCommandLine (2026-07-28), which was the last raw-Win32 dialog of its
' shape and had just been rebuilt on the Ps* family. Everything command-line-specific -- the
' fixed prompt, and the gApp read/write of the entered text -- has moved OUT to the caller;
' what is left is a caption, a painted prompt, one full-width field and OK / Cancel, with the
' text passed in and out by reference:
'
'   dim as DWSTRING wszText = <initial value>
'   if frmInputBox_Show( hParent, "Caption", "Prompt:", wszText ) = IDOK then
'       ' wszText now holds what the user typed
'   end if
'
' wszText SEEDS the field on entry and, on close, is set to whatever the field currently
' holds -- for both OK and Cancel, so the caller decides what to keep off the return value.
' The return is IDOK (OK button, or Enter) or IDCANCEL (Cancel, Escape, the close X, Alt+F4).
'
' It is reusable WITHIN tiko: it still draws through the shared Options theme builders
' (OptionsTheme_ApplyTextBox / OptionsTheme_FillButton / OptionsFont_Base) and ghPanel, so it
' follows the active theme with no per-caller wiring. It is NOT cross-project standalone --
' those dependencies are tiko's -- and decoupling them was not the ask.
'
' ----------------------------------------------------------------------------------------
' ENTER IS THE WHOLE INTERACTION, AND IT DOES NOT ARRIVE THE WAY A STOCK DIALOG'S DOES.
'
' The old frmCommandLine OK button carried BS_DEFPUSHBUTTON, so Enter anywhere fired it. Two
' separate things about the Ps* replacement mean that route no longer exists:
'
'   1. A single-line PsTextBox SWALLOWS VK_RETURN in its own WM_KEYDOWN and returns 0
'      (PsTextBox.inc, the `bSingleLine` arm). It never reaches the pump, so IsDialogMessage
'      never synthesises the WM_COMMAND / IDOK that every other dialog here relies on. The
'      control offers TXT_EnterPressedCallbackSub instead, and that is the ONLY route.
'   2. PsButton_SetDefault is APPEARANCE ONLY -- one accent border, no key claim. It marks
'      OK as the default; it does not make Enter fire it.
'
' So the Enter path is wired explicitly (SetEnterPressedCallback -> frmInputBox_Commit) and
' IDOK is ALSO handled in WM_COMMAND, for the case where focus has left the field: a focused
' PsButton claims Enter itself, and IsDialogMessage covers anything else. Deleting either
' half leaves a dialog whose one field cannot be submitted from the keyboard, and nothing in
' a build or an assertion sweep would say so.
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
' NO SELF-TEST FOR THE COMMIT RULE, deliberately: it is a few lines and the dialog is modal,
' so an assertion would have to open it. The geometry IS asserted -- see below.
' ========================================================================================

' The prompt is painted, so it has no control id. 1000 is left unused rather than recycled.
#Define IDC_FRMINPUTBOX_TEXTBOX1                    1001
#Define IDC_FRMINPUTBOX_CMDOK                       1002
#Define IDC_FRMINPUTBOX_CMDCANCEL                   1003

' ----------------------------------------------------------------------------------------
' Shell metrics, UNSCALED -- frmInputBox_ComputeLayout scales them.
'
' Walking the sum: 20 margin + 24 prompt + 6 + 32 field = 82, and the footer sits at
' CLIENT_H - MARGIN - BTN_H = 108. 26 px clear. The field is the whole width of the dialog,
' because a text entry is the one thing here that is routinely long.
' ----------------------------------------------------------------------------------------
#Define FRMINPUTBOX_CLIENT_W                         520
#Define FRMINPUTBOX_CLIENT_H                         160
#Define FRMINPUTBOX_MARGIN                            20
#Define FRMINPUTBOX_LABEL_H                           24
#Define FRMINPUTBOX_GAP                                6
#Define FRMINPUTBOX_TEXT_H                            32
#Define FRMINPUTBOX_TEXT_PAD                           8     ' inside the field, per side
#Define FRMINPUTBOX_BTN_W                             92
#Define FRMINPUTBOX_BTN_H                             32
#Define FRMINPUTBOX_BTN_GAP                            8

dim shared HWND_FRMINPUTBOX as HWND

' ----------------------------------------------------------------------------------------
' ONE LAYOUT, THREE READERS. The sum is computed once, by a PURE function of the client
' size, and the painter, the placer and the self-test all read the same struct. Nothing to
' keep in step by hand, and the geometry is assertable without opening a modal window.
' ----------------------------------------------------------------------------------------
type FRMINPUTBOX_LAYOUT
    rcLabel  as RECT
    rcText   as RECT
    rcOK     as RECT
    rcCancel as RECT
end type

declare sub      frmInputBox_ComputeLayout( byval cx as long, _
                                            byval cy as long, _
                                            byref lay as FRMINPUTBOX_LAYOUT )
declare function frmInputBox_Show( byval hWndParent as HWND, _
                                   byval wszCaption as DWSTRING, _
                                   byval wszPrompt  as DWSTRING, _
                                   byref wszText    as DWSTRING ) as LRESULT

' Env-gated (TIKO_INPUTBOX_SELFTEST=1). Runs from frmMain's startup block rather than from
' _Show: the layout is pure, so the suite needs no window and the dialog -- which is MODAL --
' never has to be opened to run it. That is the payoff for computing the geometry in one pure
' function instead of walking a running y in the painter and the placer.
declare sub      frmInputBox_RunSelfTest()
