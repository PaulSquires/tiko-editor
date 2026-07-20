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


type OUTPUT_TABS
    wszText as DWSTRING
    rcTab   as RECT
    rcText  as RECT     ' diff rect b/c line drawn under Text for CurSel
    isHot   as boolean
end type

dim shared gOutputTabs(4) as OUTPUT_TABS
dim shared gOutputTabsCurSel as long = 0  ' default to first tab
dim shared gOutputCloseRect as RECT

declare sub      frmOutput_TextBoxScrollChanged( byval hTextBox as HWND )
declare function frmOutput_ShowNotes() as long
declare function frmOutput_UpdateToDoListview() as long 
declare function frmOutput_UpdateSearchListview( byref wszResultFile as wstring ) as long 
declare function frmOutput_ShowHideOutputControls( byval hwnd as HWND ) As LRESULT
declare function frmOutput_PositionWindows() as LRESULT
declare function frmOutput_Show( byval hWndParent as HWND ) as LRESULT
declare function frmOutput_ResetAllControls() as long 
declare function frmOutput_RestorePanel() as long

