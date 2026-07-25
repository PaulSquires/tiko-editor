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

'  frmOptionsColors.bi  --  the "Themes and Fonts" page.
'
'  Panel-hosted. The themes list and the font-name list are both PsListBoxes (the font names
'  are drawn each in their own font, which a scrollable list can do and a PsComboBox dropdown
'  -- which cannot scroll -- could not). Font size and character set are PsComboBoxes; the
'  extra line spacing is a PsNumericUpDown. Everything reads/writes gOptWork.

#pragma once

#include once "PsScrollPanel.bi"        ' SCP_PAINTINFO, pulled in early

#Define IDC_FRMOPTIONSCOLORS_LSTTHEMES    9400
#Define IDC_FRMOPTIONSCOLORS_LSTFONTS     9401
#Define IDC_FRMOPTIONSCOLORS_COMBOSIZE    9402
#Define IDC_FRMOPTIONSCOLORS_COMBOCHARSET 9403
#Define IDC_FRMOPTIONSCOLORS_NUMSPACING   9404

declare function frmOptionsColors_OwnsPage( byval nPage as long ) as boolean
declare sub      frmOptionsColors_Reset()
declare sub      frmOptionsColors_Build( byval hPage as HWND )
declare function frmOptionsColors_Layout( byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmOptionsColors_Paint( byval p as SCP_PAINTINFO ptr )
declare sub      frmOptionsColors_Show( byval bShow as boolean )
declare sub      frmOptionsColors_ApplyTheme()

' Re-read settings\themes\ and rebuild the list. Called when the colour pages fork a built-in
' theme, which puts a new file in that folder while this page is already built.
declare sub      frmOptionsColors_RefreshThemeList()
