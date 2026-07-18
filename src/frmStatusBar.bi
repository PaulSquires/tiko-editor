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


enum STATUSBAR_IDPANEL
    GOTO_PANEL           = 0
    COMPILE_STATUS_PANEL = 1
    THEMES_DIALOG_PANEL  = 2
    BUILD_DIALOG_PANEL   = 3
    BUILD_POPUP_PANEL    = 4
    SPACES_PANEL         = 5
    UTF_PANEL            = 6
    CRLF_PANEL           = 7
end enum

declare function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

