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


#define IDC_FRMFUNCTIONS_LISTBOX   1000

type FUNCTION_NODE_TYPE
    wszFunctionName as DWSTRING
    wszPrototype    as DWSTRING      ' the sub/function parameters
    nLineNumber     as long          ' 0-based Scintilla line of the implementation
    nFileIdx        as long          ' index into gFuncPanelFiles()
end type

' The absolute filenames backing the current listbox contents. Header and
' function rows store an index into this array as their ItemData - the panel
' now also lists never-opened include files, which have no clsDocument, so a
' pDoc pointer can no longer identify a row's file.
dim shared gFuncPanelFiles(any) as DWSTRING

enum FunctionsDisplayState
    ViewAsTree = 0
    ViewAsList 
end enum

dim shared gFunctionsDisplay as FunctionsDisplayState = FunctionsDisplayState.ViewAsTree

declare function frmFunctions_Show( byval hWndParent as HWND ) as LRESULT
declare function frmFunctions_ReparseFiles() as long
declare function frmFunctions_SelectItemData( byval pDoc as clsDocument ptr ) as boolean
declare function LoadFunctionsFiles() as long
declare function frmFunctions_ViewAsTree() as long
declare function frmFunctions_ViewAsList() as long
declare function QuickSortFilenames( arrFiles() as DWSTRING, lo as long, hi as long ) as long

