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
    CRLF_PANEL           = 7
    UTF_PANEL            = 6
    SPACES_PANEL         = 5
    BUILD_POPUP_PANEL    = 4
    BUILD_DIALOG_PANEL   = 3
    THEMES_DIALOG_PANEL  = 2
    COMPILE_STATUS_PANEL = 1
end enum

' type and array to hold values related to the statusbar panels
type STATUSBAR_PANEL_TYPE
    wszText as DWSTRING
    rc      as RECT            ' client coordinates 
    nID     as long            ' id to invoke if clicked on
    isHot   as boolean
end type
dim shared gSBPanels(7) as STATUSBAR_PANEL_TYPE 
dim shared grcGripper as RECT  

declare function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

