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

'  modMenuDefinitions.bi  --  declarations for the menu text builders.
'
'  createToolsMenuShortcut moved here out of frmUserTools.bi. It composes a shortcut label
'  from gConfig.Tools and the key vocabulary and knows nothing about a window; its two real
'  callers are modMenuDefinitions.inc and frmMenuBar.inc, both of which are building menus.
'  It sat in the user-tools DIALOG's header only because that is where it was written.

#pragma once

declare function createToolsMenuShortcut( byval nCtrlID as long ) as DWSTRING
