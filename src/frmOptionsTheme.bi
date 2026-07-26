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

'  frmOptionsTheme.bi  --  the "Syntax Colors" and "Interface Colors" pages.
'
'  ONE MODULE, TWO PAGES. They differ only in which keys they list -- editor.* on the syntax
'  page, everything else on the interface page -- so they share one set of controls and one
'  layout, following frmOptionsKeywords' precedent of taking nPage in every entry point.
'
'  THE KEY LIST IS DERIVED FROM THE BIND TABLE, NOT HAND-WRITTEN.
'    gThemeBind already knows every key, which channels it has, whether it carries
'    bold/italic/underline, and whether alpha can reach a pixel for it. Deriving the pages
'    from it means a key added to the engine shows up in the UI automatically, and there is
'    no second list that can silently fall out of step with the first.
'
'  EDITING IS LIVE. A colour change goes Theme_SetColor -> Theme_RequestApply(mask), so the
'  editor behind the dialog recolours on the next coalescing tick. Cancel restores the
'  snapshot frmOptions_Show took. Nothing is written to disk until OK.

#pragma once

#include once "PsScrollPanel.bi"        ' SCP_PAINTINFO, pulled in early

#Define IDC_FRMOPTIONSTHEME_LSTKEYS     9500
#Define IDC_FRMOPTIONSTHEME_CMDFORE     9501
#Define IDC_FRMOPTIONSTHEME_CMDBACK     9502
#Define IDC_FRMOPTIONSTHEME_TOGBOLD     9503
#Define IDC_FRMOPTIONSTHEME_TOGITALIC   9504
#Define IDC_FRMOPTIONSTHEME_TOGUNDER    9505
#Define IDC_FRMOPTIONSTHEME_NUMALPHA    9506
#Define IDC_FRMOPTIONSTHEME_CMDRESETF   9507
#Define IDC_FRMOPTIONSTHEME_CMDRESETB   9508
#Define IDC_FRMOPTIONSTHEME_PICKER      9509

declare function frmOptionsTheme_OwnsPage( byval nPage as long ) as boolean
declare sub      frmOptionsTheme_Reset()
declare sub      frmOptionsTheme_Build( byval nPage as long, byval hPage as HWND )
declare function frmOptionsTheme_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmOptionsTheme_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
declare sub      frmOptionsTheme_Show( byval nPage as long, byval bShow as boolean )
declare sub      frmOptionsTheme_ApplyTheme()

' Used by the Themes and Fonts page so its list can refresh after a fork creates a new file.
declare function frmOptionsTheme_IsProtectedTheme( byval wszShortFilename as DWSTRING ) as boolean
declare function frmOptionsTheme_MakeForkName( byval wszShortFilename as DWSTRING ) as DWSTRING
declare function frmOptionsTheme_WasEdited() as boolean
declare function frmOptionsTheme_PageForKey( byval sKey as string ) as long

' Group-header categories. Stable ids, deliberately NOT localized strings -- grouping is
' structure, and must not change meaning with the UI language. THEMECAT_OTHER is the
' unclassified bucket the self-test asserts stays empty.
enum
    THEMECAT_OTHER = 0
    THEMECAT_MAIN
    THEMECAT_PANEL
    THEMECAT_TOPTABS
    THEMECAT_MENUBAR
    THEMECAT_POPUP
    THEMECAT_STATUSBAR
    THEMECAT_OUTPUT
    THEMECAT_COMPILE
    THEMECAT_SHARED
    THEMECAT_SYNTAX
    THEMECAT_CURSOR
    THEMECAT_MARGINS
    THEMECAT_MARKERS
    THEMECAT_EDCHROME
end enum

declare function frmOptionsTheme_CategoryIdForKey( byval sKey as string ) as long
declare function frmOptionsTheme_CategoryTitle( byval nCat as long ) as DWSTRING
