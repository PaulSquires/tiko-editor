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
' Assign one User Tool shortcut.
'
' WHY THIS EXISTS RATHER THAN THREE CHECKBOXES AND A DROPDOWN ON THE PAGE.
' The key vocabulary is 90 names. PsComboBox cannot scroll -- frmKeyboardEdit.bi records the
' measurement: with the list open its popup came back 3812 px tall on an 1800 px screen, and
' more than half the vocabulary was unreachable. The replacement is a scrolling PsListTree
' 150 px tall, and the User Tools detail column has 56 px of slack. So the picker cannot live
' on the page, and a shortcut editor that cannot reach half its keys is a broken feature
' rather than a cramped one.
'
' WHY NOT REUSE frmKeyboardEdit ITSELF. It edits a KEYBINDINGS_TYPE and shows a "Default
' Keys" line, a Reset button and a "Disable default" toggle. A user tool has no shipped
' default, so all three are meaningless here and suppressing them means a mode flag threaded
' through a dialog that has other callers. The capture RULES are shared instead --
' frmKeyboardEdit_AcceptCapture is called directly, so the two dialogs cannot come to
' disagree about which keystrokes are legal.
'
' THE MODEL IS NOT A STRING. frmKeyboardEdit produces a canonical "Ctrl+Shift+D"; a user tool
' stores IsCtrl/IsAlt/IsShift as separate longs plus a BARE key name resolved through
' KeyBindings_PickListKeyToValue. This dialog therefore hands back the parts, and never
' composes a combined string that something downstream would have to take apart again.
'
' NO PUMP OBLIGATION: the picker is a PsListTree with in-place editing off, and there is no
' popup and no wrapped child. There is no frmUserToolKey_FilterMessage and there must not be.
' ========================================================================================

#Define IDC_FRMUSERTOOLKEY_TOGCTRL           1301
#Define IDC_FRMUSERTOOLKEY_TOGALT            1302
#Define IDC_FRMUSERTOOLKEY_TOGSHIFT          1303
#Define IDC_FRMUSERTOOLKEY_KEYLIST           1304
#Define IDC_FRMUSERTOOLKEY_CMDOK             1305
#Define IDC_FRMUSERTOOLKEY_CMDCANCEL         1306
#Define IDC_FRMUSERTOOLKEY_CMDCLEAR          1307

' Shell metrics, UNSCALED. Walking the running y that frmUserToolKey_OnPaint and
' _PositionWindows share:
'   20 margin + 30 tool name + 46 capture + 18 + 30 toggles + 6 + 210 picker + 12
'     = message line top 372, and the message line is one 30px row = ends at 402
' the footer sits at CLIENT_H - MARGIN - BTN_H = 418. 16 px clear. frmKeyboardEdit shipped
' with that gap NEGATIVE once -- the refusal text ran through the Reset button -- and it was
' found by looking, not by an assertion, so the sum is written down here too.
'
' THE PICKER IS 210, NOT frmKeyboardEdit'S 150, AND THE CLIENT IS 470, NOT 440. At 440/160 the
' render carried an ~80px empty band between the picker and the footer: the message line is
' blank except after a refused keystroke, so that space was doing nothing at all. It went to
' the picker, which is a 90-item list and the one thing here that gains from height. Found by
' looking; no assertion had anything to say about it, because everything already fit.
#Define FRMTOOLKEY_CLIENT_W                   480
#Define FRMTOOLKEY_CLIENT_H                   470
#Define FRMTOOLKEY_MARGIN                      20
#Define FRMTOOLKEY_BTN_W                       92
#Define FRMTOOLKEY_BTN_H                       32
#Define FRMTOOLKEY_ROW_H                       30
#Define FRMTOOLKEY_CAPTURE_H                   46
#Define FRMTOOLKEY_KEYLIST_H                  210
#Define FRMTOOLKEY_KEYROW_H                    24
#Define FRMTOOLKEY_TOGW                        46     ' the toggle's own cell
#Define FRMTOOLKEY_CAPW                        42     ' its drawn caption

dim shared HWND_FRMUSERTOOLKEY         as HWND
dim shared HWND_FRMUSERTOOLKEY_CAPTURE as HWND

' ----------------------------------------------------------------------------------------
' THE HAND-OFF. The caller fills these in, calls Show, and reads them back. On Cancel they
' are left exactly as they were found -- the dialog edits private copies and writes here only
' on OK, which is what makes Cancel mean anything at this level too.
' ----------------------------------------------------------------------------------------
dim shared gToolKeyCaption as DWSTRING    ' the tool's name, shown at the top. Display only.
dim shared gToolKeyName    as DWSTRING    ' bare key name from gKeyNames, "" = no shortcut
dim shared gToolKeyCtrl    as boolean
dim shared gToolKeyAlt     as boolean
dim shared gToolKeyShift   as boolean
' Which gToolsWork() row is being edited. Needed so the clash test can skip the tool's OWN
' current binding -- without it, opening the dialog on a tool that already has Ctrl+1 would
' immediately report that it clashes with itself. -1 disables the tool-vs-tool half.
dim shared gToolKeyIndex   as long = -1

declare function frmUserToolKey_Show( byval hWndParent as HWND ) as LRESULT

' ========================================================================================
' DOES THIS SHORTCUT ALREADY BELONG TO SOMETHING?
'
' "" when it is free; otherwise a sentence naming the owner. Checks the live editor bindings
' (gKeys, through KeyBindings_FindCommandForKeys) AND the other user tools in gToolsWork(),
' skipping nSkipToolIndex so a tool never reports clashing with itself. Pass -1 to check
' against every tool.
'
' Public because the User Tools list needs it too: a settings.ini written before this check
' existed can hold a clash the dialog would now refuse to create, and a shortcut that silently
' loses to an editor command is exactly the failure this is here to make visible.
' ========================================================================================
declare function frmUserToolKey_ClashText( byval wszKey as DWSTRING, _
                                           byval bCtrl as boolean, _
                                           byval bAlt as boolean, _
                                           byval bShift as boolean, _
                                           byval nSkipToolIndex as long ) as DWSTRING
