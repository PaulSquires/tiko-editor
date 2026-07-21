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

#define IDC_FRMMAIN_TOPTABCONTROL                   1000
#define IDC_FRMMAIN_COMPILETIMER                    1001
#define IDC_FRMMAIN_STATUSBAR                       1002
#define IDC_FRMMAIN_TABBAR                          1003
#define IDC_FRMMAIN_SPLITV                          1004
#define IDC_FRMMAIN_SPLITH                          1005

' Width (or height) of the editor split CSplitter. The control's whole client rect is its
' grab area, so this is the GRAB size; the paint callback draws the narrower SPLITSIZE band
' centered inside it. Same treatment as the help viewer's bar.
#define EDITORSPLIT_GRAB                            6

declare function frmMain_OpenProjectSafely( byval HWnd as HWnd, byref wszProjectFileName as const WString ) as Boolean
declare function frmMain_GotoFile( byval pDoc as clsDocument ptr, byval nMenuId as long ) as long
declare function frmMain_GotoLastPosition() as long
declare function frmMain_GotoDefinition( byval pDoc as clsDocument Ptr ) as long
declare function frmMain_SetStatusbar() as long
declare function frmMain_SetFocusToCurrentCodeWindow() as long
declare function frmMain_PositionWindows() as LRESULT
' Forward declared: the split bar's position callback is defined above them.
declare function frmMain_HideHorizScrollBars() as LRESULT
declare function frmMain_HighlightWord( byval pDoc as clsDocument ptr, byref text as string ) as long
declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
