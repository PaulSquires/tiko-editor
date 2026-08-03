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


' These are POSITIONS in the bar, so they renumber whenever a panel is added or removed --
' unlike the IDM_* commands the panels carry, which are stable ids. Always add or remove a
' panel here and in frmStatusBar_Show together.
'
' BUILD_DIALOG_PANEL is gone: a "+" icon that opened the Build Configurations dialog, sat
' immediately left of the build-name panel whose popup menu offers "Build Configurations..."
' as its first item. Two clicks apart from each other, same destination. The command it
' posted (IDM_BUILDCONFIG) is untouched -- it is still on the Settings menu, on F7, and at
' the head of that popup.
enum STATUSBAR_IDPANEL
    GOTO_PANEL           = 0
    COMPILE_STATUS_PANEL = 1
    THEMES_DIALOG_PANEL  = 2
    BUILD_POPUP_PANEL    = 3
    SPACES_PANEL         = 4
    UTF_PANEL            = 5
    CRLF_PANEL           = 6
end enum

declare function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

