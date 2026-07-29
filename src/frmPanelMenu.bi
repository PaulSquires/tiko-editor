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

#define IDC_FRMPANELMENU_ICONBAR              1000
#define IDC_FRMPANELMENU_ICONBAR_RIGHT        1001


declare function frmPanelMenu_FormatTooltip( byval id as integer ) as DWSTRING
declare function frmPanelMenu_Show( byval hWndParent as HWnd ) as LRESULT
declare sub      frmPanelMenu_ApplyTheme()

