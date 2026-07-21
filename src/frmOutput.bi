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

#define IDC_FRMOUTPUT_TABS                          1000
#define IDC_FRMOUTPUT_LVRESULTS                     1001
#define IDC_FRMOUTPUT_TXTLOGFILE                    1002
#define IDC_FRMOUTPUT_LVSEARCH                      1003
#define IDC_FRMOUTPUT_LVTODO                        1004
#define IDC_FRMOUTPUT_TXTNOTES                      1005
#define IDC_FRMOUTPUT_BTNCLOSE                      1006
#define IDC_FRMOUTPUT_VSCROLL                       1007

' The tab strip is a CSelectBar plus a one-item CIconPanel for the "X", sized side by side
' to cover the strip. There is no container behind them: each paints its own background, so
' the old subclassed LABEL (and its ~150 lines of rect arithmetic, hit-testing, hover
' tracking and painting) is gone rather than merely bypassed.
'
' The CURRENT TAB lives in the CSelectBar -- there is deliberately no gOutputTabsCurSel
' global any more. Read it with CSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR ) and set it
' with CSelectBar_SetCurSel (silent: only user clicks fire the change callback, so calling
' it from a handler cannot recurse). gConfig.ShowOutputPanelIndex is the persisted copy,
' applied to the control once it exists.
'
' Panel indices, in the order they are added -- these ARE the values persisted in the
' config file, so their numbering must not change.
#define OUTPUT_TAB_RESULTS    0
#define OUTPUT_TAB_LOGFILE    1
#define OUTPUT_TAB_SEARCH     2
#define OUTPUT_TAB_TODO       3
#define OUTPUT_TAB_NOTES      4

declare sub      frmOutput_TextBoxScrollChanged( byval hTextBox as HWND )
declare sub      frmOutput_SetTabCaptions()
declare function frmOutput_ShowNotes() as long
declare function frmOutput_UpdateToDoListview() as long 
declare function frmOutput_UpdateSearchListview( byref wszResultFile as wstring ) as long 
declare function frmOutput_ShowHideOutputControls( byval hwnd as HWND ) As LRESULT
declare function frmOutput_PositionWindows() as LRESULT
declare function frmOutput_Show( byval hWndParent as HWND ) as LRESULT
declare function frmOutput_ResetAllControls() as long 
declare function frmOutput_RestorePanel() as long
' Forward declared: the splitter's message callback is defined above them.
declare function frmOutput_MinimizePanel() as long

