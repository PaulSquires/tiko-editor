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

' The editor's two horizontal scrollbars (one per split view of the active document) are
' PsHScrollBar instances. The control knows nothing about Scintilla: the message pump
' (handleMouseShowScrollBar) pushes the range in PIXEL units -- total = widest visible
' line in document space, page = visible text width, pos = SCI_GETXOFFSET -- through
' frmEditorHScroll_NeedScrollBar / _UpdateScrollBars, and the control reports user
' scrolling back through a ScrollCallback that drives SCI_SETXOFFSET.

' Control ids are cosmetic (the bars are reached through the HWND_FRMEDITOR_HSCROLLBAR
' globals; nothing does GetDlgItem on HWND_FRMMAIN).
#define IDC_FRMEDITOR_HSCROLLBAR0    6902
#define IDC_FRMEDITOR_HSCROLLBAR1    6903

declare function frmEditorHScroll_UpdateScrollBars( byval pDoc as clsDocument ptr ) as boolean
declare function frmEditorHScroll_DoNeedScrollBar( byval pDoc as clsDocument ptr ) as long
