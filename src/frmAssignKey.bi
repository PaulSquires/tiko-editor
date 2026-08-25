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
' Assign one shortcut. SHARED by the User Tools dialog and the Build Configurations dialog.
'
' It began as frmUserToolKey and was generalized in place. Only ONE thing about it was ever
' User-Tools-specific -- which records the clash test compares against -- so the whole
' generalization is a domain selector (gAssignKeyDomain) plus two arguments on ClashText.
' The caption at the top was already a plain string the caller fills.
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
' -- and a build configuration, which stores the identical five fields -- keeps
' IsCtrl/IsAlt/IsShift as separate longs plus a BARE key name resolved through
' KeyBindings_PickListKeyToValue. This dialog therefore hands back the parts, and never
' composes a combined string that something downstream would have to take apart again.
'
' NO PUMP OBLIGATION: the picker is a PsListTree with in-place editing off, and there is no
' popup and no wrapped child. There is no frmAssignKey_FilterMessage and there must not be.
' ========================================================================================

' ----------------------------------------------------------------------------------------
' WHICH RECORDS THE CLASH TEST COMPARES AGAINST. This is the whole of what used to be
' hardcoded. The caller names its own domain so the test can skip the record being edited
' (gAssignKeyIndex) without a tool reporting that it clashes with itself.
' ----------------------------------------------------------------------------------------
#Define ASSIGNKEY_DOMAIN_NONE              (-1)   ' check everything, skip nothing
#Define ASSIGNKEY_DOMAIN_TOOLS               0
#Define ASSIGNKEY_DOMAIN_BUILDS              1

#Define IDC_FRMASSIGNKEY_TOGCTRL           1301
#Define IDC_FRMASSIGNKEY_TOGALT            1302
#Define IDC_FRMASSIGNKEY_TOGSHIFT          1303
#Define IDC_FRMASSIGNKEY_KEYLIST           1304
#Define IDC_FRMASSIGNKEY_CMDOK             1305
#Define IDC_FRMASSIGNKEY_CMDCANCEL         1306
#Define IDC_FRMASSIGNKEY_CMDCLEAR          1307

' Shell metrics, UNSCALED. Walking the running y that frmAssignKey_OnPaint and
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
' THE VERTICAL SUM, unscaled -- both walks (PositionWindows and OnPaint) follow it, so it is
' written down here rather than rediscovered twice:
'
'     20 margin + 30 name + 46 capture + 18 + 30 toggles + 6 + 132 list + 12 + 30 message
'         = 324, then 16 gap + 32 buttons + 20 margin = 392
'
' TOP AND BOTTOM MARGIN ARE BOTH FRMASSIGNKEY_MARGIN, by construction.
'
' KEYLIST_H WAS 210 AND CLIENT_H 470. At 328 the list ran to 360 -- 32px past the client
' bottom and straight through the bottom-anchored buttons, because the height came down and
' the list did not. The list is the only elastic element here (everything else is one row of
' something), so it is what absorbs a height change. 132 is five and a half rows at KEYROW_H.
#Define FRMASSIGNKEY_CLIENT_W                   440
#Define FRMASSIGNKEY_CLIENT_H                   392
#Define FRMASSIGNKEY_MARGIN                      20
#Define FRMASSIGNKEY_BTN_W                       92
#Define FRMASSIGNKEY_BTN_H                       32
#Define FRMASSIGNKEY_ROW_H                       30
#Define FRMASSIGNKEY_CAPTURE_H                   46
#Define FRMASSIGNKEY_KEYLIST_H                  132
#Define FRMASSIGNKEY_KEYROW_H                    24
#Define FRMASSIGNKEY_TOGW                        46     ' the toggle's own cell
#Define FRMASSIGNKEY_CAPW                        42     ' its drawn caption

dim shared HWND_FRMASSIGNKEY         as HWND
dim shared HWND_FRMASSIGNKEY_CAPTURE as HWND

' ----------------------------------------------------------------------------------------
' THE HAND-OFF. The caller fills these in, calls Show, and reads them back. On Cancel they
' are left exactly as they were found -- the dialog edits private copies and writes here only
' on OK, which is what makes Cancel mean anything at this level too.
' ----------------------------------------------------------------------------------------
dim shared gAssignKeyCaption as DWSTRING    ' the record's name, shown at the top. Display only.
dim shared gAssignKeyName    as DWSTRING    ' bare key name from gKeyNames, "" = no shortcut
dim shared gAssignKeyCtrl    as boolean
dim shared gAssignKeyAlt     as boolean
dim shared gAssignKeyShift   as boolean
' Which domain the caller is editing, and which row of it. Together they let the clash test
' skip the record's OWN current binding -- without that, opening the dialog on a tool that
' already has Ctrl+1 would immediately report that it clashes with itself.
' ASSIGNKEY_DOMAIN_NONE / -1 skips nothing.
dim shared gAssignKeyDomain  as long = ASSIGNKEY_DOMAIN_NONE
dim shared gAssignKeyIndex   as long = -1

declare function frmAssignKey_Show( byval hWndParent as HWND ) as LRESULT

' ========================================================================================
' DOES THIS SHORTCUT ALREADY BELONG TO SOMETHING?
'
' "" when it is free; otherwise a sentence naming the owner. Three sources: the live editor
' bindings (gKeys, through KeyBindings_FindCommandForKeys), the user tools, and the build
' configurations. nSkipIndex is skipped WITHIN nDomain only, so a record never reports
' clashing with itself. Pass ASSIGNKEY_DOMAIN_NONE / -1 to check against everything.
'
' Public because both dialogs' lists need it too: a settings.ini written before this check
' existed can hold a clash the dialog would now refuse to create, and a shortcut that silently
' loses to an editor command is exactly the failure this is here to make visible.
' ========================================================================================
declare function frmAssignKey_ClashText( byval wszKey as DWSTRING, _
                                           byval bCtrl as boolean, _
                                           byval bAlt as boolean, _
                                           byval bShift as boolean, _
                                           byval nDomain as long, _
                                           byval nSkipIndex as long ) as DWSTRING
