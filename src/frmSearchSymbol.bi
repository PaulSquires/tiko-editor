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

type SEARCHSYMBOL_TYPE
    result   as score_t
    id       as SearchSymbol
    pDoc     as clsDocument ptr    ' used to get filename / diskfilename
    pData    as DB2_DATA ptr       ' pointer into database for Types / Functions
    clrMask  as DWSTRING
end type

#define IDC_FRMSEARCHSYMBOL_TXTFIND       1000
#define IDC_FRMSEARCHSYMBOL_LISTBOX       1001

#define MAX_SYMBOL_SEARCH  100
dim shared gSymbols( MAX_SYMBOL_SEARCH ) as SEARCHSYMBOL_TYPE
dim shared gSymbolsCount as integer = 0  ' actual # of populated entries (<= MAX_SYMBOL_SEARCH+1)

declare function frmSearchSymbol_DoSearch( byval hwndCtl as HWND ) as long
declare function frmSearchSymbol_Show( byval hWndParent as HWND ) as LRESULT
