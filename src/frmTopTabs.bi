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


#Define IDC_FRMFIND_TXTFIND                  1000
#Define IDC_FRMREPLACE_TXTREPLACE            1001
#Define IDC_FRMTOPTABSINFO_ICONS             1002
#Define IDC_FRMFIND_ICONSFIELD               1003
#Define IDC_FRMFIND_ICONSNAV                 1004
#Define IDC_FRMFIND_ICONSCLOSE               1005
#Define IDC_FRMREPLACE_ICONSFIELD            1006
#Define IDC_FRMREPLACE_ICONSACTIONS          1007
#Define IDC_FRMFIND_ICONSFOLD                1008


'' frmTopTabsInfo
declare function TopTabsInfoPanel_Hide() as long
declare function modFindReplace_HighlightSearches() as long
declare function frmTopTabsInfo_PositionWindows() as LRESULT
declare function frmTopTabsInfo_Show( byval hwndParent as HWND ) as LRESULT

'' frmFind

'' frmReplace
declare function TopTabsInfoPanel_Show() as long
declare function FindReplace_HighlightSearches() as long

' Both live in modFindReplace.inc, which is included AFTER clsTopTabCtl.inc -- and
' clsTopTabCtl.SetFocusTab forces the Find bar open when the Find in Project tab is
' selected, since the find phrase is that tab's whole subject. Declared here for the same
' reason TopTabsInfoPanel_Show above is.
declare function FindControls_Show() as long
declare function ReplaceControls_Show() as long

' Find in Project's collapse-all state, for the Find bar's fold icon. Declared here rather
' than in frmFindInProject.bi because frmFind.inc is included well before it.
declare function frmFindInProject_AllCollapsed() as boolean
declare sub      frmFindInProject_SetAllCollapsed( byval bCollapsed as boolean )
declare function frmFindInProject_IsActive() as boolean



