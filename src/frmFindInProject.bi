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

'#define IDC_FRMFINDINFILES_LBLFINDWHAT              1000
'#define IDC_FRMFINDINFILES_COMBOFIND                1001
'#define IDC_FRMFINDINFILES_COMBOFILES               1002
'#define IDC_FRMFINDINFILES_COMBOFOLDER              1003
'#define IDC_FRMFINDINFILES_CHKWHOLEWORDS            1004
'#define IDC_FRMFINDINFILES_CHKMATCHCASE             1005
'#define IDC_FRMFINDINFILES_FRAMESCOPE               1006
'#define IDC_FRMFINDINFILES_OPTSCOPE1                1007
'#define IDC_FRMFINDINFILES_OPTSCOPE2                1008
'#define IDC_FRMFINDINFILES_OPTSCOPE3                1009
'#define IDC_FRMFINDINFILES_CHKSUBFOLDERS            1010
'#define IDC_FRMFINDINFILES_FRAMEOPTIONS             1011
'#define IDC_FRMFINDINFILES_CMDFILES                 1012
'#define IDC_FRMFINDINFILES_CMDFOLDERS               1013
'#define IDC_FRMFINDINFILES_LABEL1                   1014
'#define IDC_FRMFINDINFILES_LABEL2                   1015
'#define IDC_FRMFINDINFILES_CHKCURRENTDOC            1016
'#define IDC_FRMFINDINFILES_CHKALLOPENDOCS           1017
'#define IDC_FRMFINDINFILES_CHKPROJECT               1018
'#define IDC_FRMFINDINFILES_LBLREPLACEWITH           1019
'#define IDC_FRMFINDINFILES_COMBOREPLACE             1020
'#define IDC_FRMFINDINFILES_CMDREPLACEALL            1021
'#define IDC_FRMFINDINFILES_LBLRESULTS               1022

'enum SEARCH_MODE
'    FindOnly   = 1
'    ReplaceAll = 2
'end enum

'type REPLACE_RESULTS
'    NumReplaced as integer
'    FilesSearched as integer
'    wszResults as DWSTRING
'    wszFileText as DWSTRING
'    pDoc as clsDocument ptr
'    pSci as any ptr
'end type
'dim shared gReplaceResults as REPLACE_RESULTS
    
declare function frmFindInProject_Show( byval hWndParent as HWND ) as LRESULT
