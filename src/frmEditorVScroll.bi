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

' The editor's two vertical scrollbars (one per split view of the active document) are
' PsVScrollBar instances. The control knows nothing about Scintilla: the existing
' Scintilla notification handlers (SCN_UPDATEUI via frmMain_SetStatusbar,
' frmMain_PositionWindows, SCN_MODIFIED's auto-show) push the range in LINE units
' through frmEditorVScroll_UpdateScrollBars, and the control reports user scrolling
' back through a ScrollCallback that drives SCI_SETFIRSTVISIBLELINE.

' Control ids are cosmetic (the bars are reached through the HWND_FRMEDITOR_VSCROLLBAR
' globals; nothing does GetDlgItem on HWND_FRMMAIN).
#define IDC_FRMEDITOR_VSCROLLBAR0    6900
#define IDC_FRMEDITOR_VSCROLLBAR1    6901

declare function frmEditorVScroll_UpdateScrollBars( byval pDoc as clsDocument ptr ) as boolean
