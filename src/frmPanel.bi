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


type PANEL_TYPE
    hActiveChild    as HWND
    wszHeader       as DWSTRING
    rcDividerLine   as RECT
    rcHeader        as RECT     ' EXPLORER, FUNCTION LIST, BOOKMARKS
    rcActionMenu    as RECT
end type
dim shared gPanel as PANEL_TYPE

' TODO: Remove when the frmListView scrollbar is replaced with CVScrollBar (the Output
' window, Help Viewer and editor scrollbars have already been converted; frmListView's
' embedded VScrollData is the last consumer of this type).
type PANEL_VSCROLL_TYPE
    hListBox      as HWND
    listBoxHeight as long
    numItems      as long
    itemHeight    as long
    itemsPerPage  as long
    thumbHeight   as long
    rc            as RECT
end type

declare function frmPanel_Show( byval hWndParent as HWnd ) as LRESULT
