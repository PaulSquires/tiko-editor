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

#include once "frmKeyboard.bi"


#Define IDC_FRMKEYBOARDEDIT_CAPTURE          1200
#Define IDC_FRMKEYBOARDEDIT_TOGCTRL          1201
#Define IDC_FRMKEYBOARDEDIT_TOGALT           1202
#Define IDC_FRMKEYBOARDEDIT_TOGSHIFT         1203
#Define IDC_FRMKEYBOARDEDIT_TOGDISABLE       1204
#Define IDC_FRMKEYBOARDEDIT_KEYLIST          1205
#Define IDC_FRMKEYBOARDEDIT_CMDOK            1206
#Define IDC_FRMKEYBOARDEDIT_CMDCANCEL        1207
#Define IDC_FRMKEYBOARDEDIT_CMDRESET         1208

' Shell metrics, UNSCALED.
'
' THE KEY PICKER IS A SCROLLABLE LIST, NOT A DROPDOWN, and that is not a style preference.
' It was a PsComboBox first. The vocabulary is 89 names plus "None", and PsComboBox.bi says
' in its own header: "THERE IS NO SCROLLING ... a list taller than the screen is CLIPPED and
' its tail is unreachable. This control is built for a dozen items, not fifty."
'
' Measured rather than assumed: with the list open, its popup window came back 3812 px tall
' on an 1800 px screen -- clamped to the top and running 2012 px below the bottom edge. More
' than half the vocabulary could not be reached at all, including every function key and all
' the punctuation. Since the picker is the ONLY route to a bare letter or digit and to keys
' the capture field reserves, a clipped picker is a broken feature, not a cosmetic flaw.
'
' frmOptions hit exactly this and solved it the same way: its font name picker is "a
' scrollable PsListTree ... deliberately, because PsPopupMenu cannot scroll and the font list
' is hundreds long".
'
' A happy side effect: PsListTree adds NO pump obligation when in-place editing is off, so
' this dialog's message loop lost its PsComboBox_FilterMessage call rather than gaining one.
' THE HEIGHT HAS TO CLEAR THE MESSAGE LINE. Walking frmKeyboardEdit_OnPaint's running y:
'   20 margin + 30 desc + 30 category + 10 + 30 default-row + 14
'     + 46 capture + 18 + 30 toggles + 6 + 150 picker + 12       = message line top 396
'   message line is one 30px row                                 = ends at        426
' and the footer sits at CLIENT_H - MARGIN - BTN_H. At 470 that put the buttons at 418,
' eight pixels INSIDE the message -- the refusal text ran straight through the Reset
' button. Found by looking at a refused keystroke, not by any assertion. 500 leaves 22px.
#Define FRMKBEDIT_CLIENT_W                   480
#Define FRMKBEDIT_CLIENT_H                   500
#Define FRMKBEDIT_MARGIN                      20
#Define FRMKBEDIT_BTN_W                       92
#Define FRMKBEDIT_BTN_H                       32
#Define FRMKBEDIT_ROW_H                       30
#Define FRMKBEDIT_CAPTURE_H                   46
#Define FRMKBEDIT_KEYLIST_H                  150
#Define FRMKBEDIT_KEYROW_H                    24

dim shared HWND_FRMKEYBOARDEDIT_CAPTURE as HWND

declare function frmKeyboardEdit_Show( byval hWndParent as HWND ) as LRESULT

' ========================================================================================
' MAY THIS KEYSTROKE BECOME A SHORTCUT?   -- pure, and that is the point.
'
' Kept as a free function of its inputs so the rules can be asserted directly, with no
' window and no message pump. Everything a self-test could otherwise reach is upstream of
' the pump, and the pump is exactly what a self-test does not have.
'
' Returns FALSE and fills wszWhy when the combination must be refused.
' ========================================================================================
declare function frmKeyboardEdit_AcceptCapture( byval vk as long, _
                                                byval bCtrl as boolean, _
                                                byval bAlt as boolean, _
                                                byval bShift as boolean, _
                                                byref wszWhy as DWSTRING ) as boolean

' Compose the canonical "Ctrl+Shift+D" form. Returns "" if vk has no name.
declare function frmKeyboardEdit_BuildKeyString( byval vk as long, _
                                                 byval bCtrl as boolean, _
                                                 byval bAlt as boolean, _
                                                 byval bShift as boolean ) as DWSTRING

' TIKO_KEYBOARD_SELFTEST=1. The capture rules, driven as pure functions.
declare sub      frmKeyboardEdit_RunSelfTest()
