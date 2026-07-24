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

#include once "AfxNova/AfxRichEdit.inc"

#define IDC_FRMHELPVIEWER_LEFTPANEL        1000
#define IDC_FRMHELPVIEWER_RIGHTPANEL       1001
' 1002 was the topics-list scrollbar; the PsListBox owns that bar internally now.
#define IDC_FRMHELPVIEWER_VSCROLLBAR2      1003
#define IDC_FRMHELPVIEWER_RTFCONVERT       1004
#define IDC_FRMHELPVIEWER_SPLITTER         1005

' Width of the PsSplitter control between the topics list and the help text. The control's
' whole client rect is its grab area, so this is the GRAB width; the paint callback draws
' the narrower SPLITSIZE band centered inside it, keeping the old visual weight.
#define HELPVIEWER_SPLITGRAB               6

' Smallest either panel may be dragged to. Replaces the old clamp, which let one side
' shrink to SPLITSIZE (4 px) and the other to nothing.
#define HELPVIEWER_MINPANE                 120

type HELPVIEWER_TYPE
    ' Topic captions live on the PsListBox rows now; only the parallel filename lookup
    ' (row index -> help file) is still needed here.
    as DWSTRING Filenames(any)
end type

dim shared as HELPVIEWER_TYPE gHelpViewer

' Forward declared: the control callbacks are defined above the functions they call.
declare function frmHelpViewer_PositionWindows() as LRESULT
declare function frmHelpViewer_LoadHelpFile( byval nIndex as long ) as long
declare function frmHelpViewer_Show( byval hWndParent as HWND ) as LRESULT

