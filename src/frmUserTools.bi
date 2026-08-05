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

#include "modCompile.bi"
#include once "app/clsConfig.bi"

' ========================================================================================
' NOTHING IN THIS FILE MAY NAME A Ps* TYPE.
'
' frmUserTools.bi is pulled in very early -- modRoutines.inc includes it at tiko.bas:89,
' which is ahead of every custom-control include (116..142). HWNDs, DWSTRINGs, longs and
' TYPE_TOOLS only; anything that needs PSLISTTREE_PAINTINFO or PSBUTTON_COLORS belongs in
' the .inc, which the unity build reaches at 182.
' ========================================================================================


' Control ids. IDC_FRMUSERTOOLS_CMDBROWSEEXE MUST STAY 1012: AfxIFileOpenDialogW picks its
' file-type filter by button id (modRoutines.inc), so renumbering it silently turns the
' Command browse dialog into an "All files" dialog with the wrong title.
#Define IDC_FRMUSERTOOLS_LIST                       1000
#Define IDC_FRMUSERTOOLS_CMDADD                     1001
#Define IDC_FRMUSERTOOLS_CMDDELETE                  1002
#Define IDC_FRMUSERTOOLS_CMDUP                      1003
#Define IDC_FRMUSERTOOLS_CMDDOWN                    1004
#Define IDC_FRMUSERTOOLS_TXTNAME                    1005
#Define IDC_FRMUSERTOOLS_TXTCOMMAND                 1006
#Define IDC_FRMUSERTOOLS_TXTPARAMETERS              1007
#Define IDC_FRMUSERTOOLS_TXTFOLDER                  1008
#Define IDC_FRMUSERTOOLS_CMDBROWSEEXE               1012
#Define IDC_FRMUSERTOOLS_CMDBROWSEFOLDER            1015
#Define IDC_FRMUSERTOOLS_CMDASSIGN                  1016
#Define IDC_FRMUSERTOOLS_COMBOACTION                1017
#Define IDC_FRMUSERTOOLS_TOGPROMPT                  1018
#Define IDC_FRMUSERTOOLS_TOGMINIMIZED               1019
#Define IDC_FRMUSERTOOLS_TOGWAIT                    1020
#Define IDC_FRMUSERTOOLS_TOGMENU                    1021
#Define IDC_FRMUSERTOOLS_CMDOK                      1029
#Define IDC_FRMUSERTOOLS_CMDCANCEL                  1030

' User Tool actions (selected, pre/post compile, after editor starts up).
' THESE ARE PERSISTED as gConfig.Tools(n).Action, so they cannot be renumbered.
const USERTOOL_ACTION_SELECTED      = 0
const USERTOOL_ACTION_PRECOMPILE    = 1
const USERTOOL_ACTION_POSTCOMPILE   = 2
const USERTOOL_ACTION_EDITORSTARTUP = 3

common shared as HACCEL ghAccelUserTools


' ========================================================================================
' THE STAGING COPY -- what makes Cancel mean anything.
'
' This replaces gConfig.ToolsTemp, which did the same job from inside clsConfig. A dialog's
' undo buffer is not configuration, and the class already carries a comment (modOptionsRows.bi)
' explaining that its variable-length arrays are the reason gOptWork is NOT a copy of
' clsConfig -- an editing copy of one of those arrays living in the same class was the odd one
' out. gConfig.Tools() is not touched until OK commits.
' ========================================================================================
dim shared gToolsWork(any) as TYPE_TOOLS


' Shell metrics, UNSCALED -- run through AfxScaleX/AfxScaleY at use, exactly as FRMOPTIONS_*
' and FRMKEYBOARD_*. THE CLIENT SIZE IS FIXED (no WS_THICKFRAME), which is what makes both
' viewports one number rather than "whatever the user dragged the window to":
'
'     list      = LIST_W x (CLIENT_H - TITLE_H - FOOTER_H) = 340 x 510
'     detail    = (CLIENT_W - MARGIN - LIST_X2) x same     = 508 x 510
'
' The detail column is a stack of fixed rows and it must FIT: nothing here scrolls, so a row
' pushed past 510 is silently clipped, and only above 100% DPI where nobody is looking.
' frmUserTools_RunSelfTest asserts the fit rather than leaving it to be noticed.
#Define FRMUSERTOOLS_CLIENT_W                        900
#Define FRMUSERTOOLS_CLIENT_H                        620
#Define FRMUSERTOOLS_TITLE_H                          52
#Define FRMUSERTOOLS_FOOTER_H                         58
#Define FRMUSERTOOLS_MARGIN                           16
#Define FRMUSERTOOLS_BTN_W                            92
#Define FRMUSERTOOLS_BTN_H                            32
#Define FRMUSERTOOLS_GAP                               8

' The list column and its command strip.
#Define FRMUSERTOOLS_LIST_W                          340
#Define FRMUSERTOOLS_ROW_H                            24
' Add / Delete. 88, not 72: at 175% "Delete" rendered as "Dele..." -- PsButton ellipsizes
' without complaining, and no assertion can see it. Found by looking at the render.
#Define FRMUSERTOOLS_CMD_W                            88
#Define FRMUSERTOOLS_GLYPH_W                          32     ' the two chevron buttons
' TWO COLUMNS, Tool and Shortcut. There is no "When" column: the Action is a full sentence
' ("Invoke only when selected by user") and in a column this narrow all four of them rendered
' as "Invoke only ..." -- four rows saying the same thing. Short forms were tried and dropped;
' the Action belongs in the detail pane, where the row IS the explanation.
#Define FRMUSERTOOLS_COL_SHORTCUT                     110

' The detail column. LABEL_W is the label gutter; a field runs from LABEL_W to the right
' margin, less BROWSE_W (+ GAP) on the two rows that carry a browse button.
#Define FRMUSERTOOLS_DETAIL_GAP                        20     ' between the list and the labels
#Define FRMUSERTOOLS_LABEL_W                          130
#Define FRMUSERTOOLS_ROWH                              40     ' one detail row
#Define FRMUSERTOOLS_FIELD_H                           32
#Define FRMUSERTOOLS_HINT_H                            24
#Define FRMUSERTOOLS_BROWSE_W                          40
' The Assign button. 150, not 110, for the same reason and found the same way: the caption
' is "Assign Shortcut" and it came out "Assign Sh...".
#Define FRMUSERTOOLS_ASSIGN_W                         150


dim shared HWND_FRMUSERTOOLS_LIST as HWND     ' PsListTree, 3 columns


declare function frmUserTools_ExecuteUserTool( byval pCompile as COMPILE_TYPE ptr, byval nToolNum as long ) as long
declare function frmUserTools_CreateAcceleratorTable() as long
declare function frmUserTools_Show( byval hWndParent As HWND ) as LRESULT
declare function createToolsMenuShortcut( byval nCtrlID as long ) as DWSTRING

' The shortcut a tool row DISPLAYS: "Ctrl+Alt+R", or "" when the tool has no key. Composed
' from the modifier booleans and the bare key name, never stored -- so the list, the detail
' pane and the Tools menu cannot disagree about it. Same modifier order as
' createToolsMenuShortcut and as frmKeyboardEdit_BuildKeyString (Ctrl, Shift, Alt is what
' createToolsMenuShortcut has always emitted; this matches it rather than "correcting" it).
declare function frmUserTools_ShortcutText( byval idx as long ) as DWSTRING

' TRUE when this tool carries a key that CANNOT FIRE -- a name in settings.ini that
' KeyBindings_PickListKeyToValue does not resolve. Such a shortcut saves, reloads and shows
' in the Tools menu perfectly and never once works; the list paints it in theme.compile.iconfail
' so it is visible rather than merely broken.
declare function frmUserTools_ShortcutIsDead( byval idx as long ) as boolean

' TIKO_USERTOOLS_SELFTEST=1. Layout fit, staging semantics and the shortcut round trip.
' Runs from frmUserTools_Show once the controls exist, so it measures real windows at the
' real DPI rather than the unscaled constants above.
declare sub      frmUserTools_RunSelfTest()
