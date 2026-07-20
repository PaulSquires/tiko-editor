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

enum SearchSymbol explicit
    file_symbol = 0
    type_symbol
    function_symbol
end enum

' Plain data copies, never SYMBOLREFs: the picker runs a modal message loop, and a
' background scan completing while it is open swaps/frees the result set a stored
' ref would point into.
type SEARCHSYMBOL_TYPE
    result      as score_t
    id          as SearchSymbol
    wszCaption  as DWSTRING        ' display name (file name / qualified symbol name)
    wszFilename as DWSTRING        ' absolute path to open on selection
    nLineNumber as long            ' 0-based Scintilla line; -1 = open without repositioning
    isEnum      as boolean         ' type_symbol only: draw the enum icon
    clrMask     as DWSTRING
end type

#define IDC_FRMSEARCHSYMBOL_TXTFIND       1000
#define IDC_FRMSEARCHSYMBOL_LISTBOX       1001

#define MAX_SYMBOL_SEARCH  100
dim shared gSymbols( MAX_SYMBOL_SEARCH ) as SEARCHSYMBOL_TYPE
dim shared gSymbolsCount as integer = 0  ' actual # of populated entries (<= MAX_SYMBOL_SEARCH+1)

declare function frmSearchSymbol_DoSearch( byval hwndCtl as HWND ) as long
declare function frmSearchSymbol_Show( byval hWndParent as HWND ) as LRESULT
