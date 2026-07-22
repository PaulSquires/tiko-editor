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

declare function frmMain_BuildAcceleratorTable( byval pWindow as CWindow ptr ) as long
declare function frmMain_ChangeTopMenuStates( byval nBarID as long ) as long

' The Create*ContextMenu builders that used to be declared here are gone: the standalone
' popup menus run on CPopupMenu now and are declared in modContextMenus.bi.

