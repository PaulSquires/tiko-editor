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

'  frmThemes.bi  --  the Themes dialog: everything about COLOUR, in one place.
'
'  SHAPE
'      +-------------------+------------------------------------------+
'      |  themes list      |  PsSelectBar: Syntax | Interface         |
'      |  (PsListTree)     +------------------------------------------+
'      |                   |  the selected colour page                |
'      |                   |    key list | picker | controls          |
'      +-------------------+------------------------------------------+
'      |                              [ PsButton OK ] [ PsButton .. ] |
'      +----------------------------------------------------------------+
'
'  WHY IT IS ITS OWN DIALOG
'      All of this used to live in frmOptions: the themes list on the "Themes and Fonts"
'      page, and the two colour pages as two more entries in the nav strip. Three problems
'      with that, and the third is what forced the move. The colour pages are the widest
'      thing in the application and were being squeezed into a page viewport sized for rows
'      of checkboxes; "Themes and Fonts" was two unrelated jobs sharing a page; and the theme
'      SAVE/CANCEL lifecycle was tangled into frmOptions' OK/Cancel, so the engine could not
'      be reasoned about without reading the options dialog.
'
'  IT OWNS THE THEME LIFECYCLE COMPLETELY
'      Theme_Snapshot on entry, live preview while open, and on OK: write the .theme file,
'      set gConfig.ThemeShortFilename, LoadThemeFile, Theme_ApplyAll. Cancel is
'      Theme_Restore + Theme_ApplyAll. frmOptions no longer knows the theme engine exists.
'
'      THAT MEANS ITS UNDO SCOPE IS ITS OWN. OK here commits the theme even if the user later
'      cancels some other dialog. That is the deliberate consequence of making it reachable
'      from the main menu rather than from inside Options -- there is no outer staging
'      session to defer to, and inventing one would put the tangle back.
'
'  PUMP
'      It runs its own nested GetMessage loop, and the loop is nearly empty: one
'      PsTextBox_FilterMessage (for the alpha spinner's PsNumericUpDown, which wraps one),
'      Escape, and IsDialogMessage. PsColorPicker contributes NOTHING -- no FilterMessage
'      exists for it, which is the property that whole control was designed around.

#pragma once


' The two colour pages. These were OPTPAGE_SYNTAX / OPTPAGE_INTERFACE while they were entries
' in the Options nav strip; they are tabs on a PsSelectBar now, so they are numbered from zero
' and are local to this dialog. Nothing outside this file should name them.
#Define THEMEPAGE_NONE       -1
#Define THEMEPAGE_SYNTAX      0
#Define THEMEPAGE_INTERFACE   1
#Define THEMEPAGE_COUNT       2

' Shell metrics, UNSCALED -- run through AfxScaleX/AfxScaleY at use. FIXED SIZE, like
' frmOptions: the colour page's three columns have to fit one known width, and a resizable
' dialog would make "does it fit" a question with no stable answer.
#Define FRMTHEMES_CLIENT_W        1000
#Define FRMTHEMES_CLIENT_H         640
#Define FRMTHEMES_LIST_W           240
#Define FRMTHEMES_BAR_H             34
#Define FRMTHEMES_FOOTER_H          58
#Define FRMTHEMES_BTN_W             92
#Define FRMTHEMES_BTN_H             32
#Define FRMTHEMES_MARGIN            16

#Define IDC_FRMTHEMES_LSTTHEMES   9600
#Define IDC_FRMTHEMES_SELECTBAR   9601
#Define IDC_FRMTHEMES_PAGE        9602
#Define IDC_FRMTHEMES_CMDOK       9603
#Define IDC_FRMTHEMES_CMDCANCEL   9604

' Control ids for the colour page itself. Unchanged from when it lived in frmOptions -- the
' page module moved wholesale and its ids came with it.
#Define IDC_FRMTHEMES_LSTKEYS     9500
#Define IDC_FRMTHEMES_CMDFORE     9501
#Define IDC_FRMTHEMES_CMDBACK     9502
#Define IDC_FRMTHEMES_TOGBOLD     9503
#Define IDC_FRMTHEMES_TOGITALIC   9504
#Define IDC_FRMTHEMES_TOGUNDER    9505
#Define IDC_FRMTHEMES_NUMALPHA    9506
#Define IDC_FRMTHEMES_CMDRESETF   9507
#Define IDC_FRMTHEMES_CMDRESETB   9508
#Define IDC_FRMTHEMES_PICKER      9509

' Group-header categories for the key list. Stable ids, deliberately NOT localized strings --
' grouping is structure, and must not change meaning with the UI language. THEMECAT_OTHER is
' the unclassified bucket the self-test asserts stays empty.
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

dim shared HWND_FRMTHEMES       as HWND
dim shared HWND_FRMTHEMES_PAGE  as HWND

' The theme this dialog is STAGING. It replaces gOptWork.ThemeShortFilename, which belonged to
' frmOptions' staging session and is not available from a dialog that can be opened without it.
' Committed to gConfig only by OK.
dim shared gThemesWorkFile as DWSTRING

declare function frmThemes_Show( byval hWndParent as HWND ) as LRESULT
declare sub      frmThemes_RefreshThemeList()
declare function frmThemes_ThemedMsgBox( byval wszText as DWSTRING, _
                                         byval wszCaption as DWSTRING, _
                                         byval nIcon as long, _
                                         byval nButtons as long ) as long

' ---- the colour page (moved from frmOptionsTheme, renamed) -----------------------------
declare sub      frmThemes_Reset()
declare sub      frmThemes_Build( byval nPage as long, byval hPage as HWND )
declare function frmThemes_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmThemes_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
declare sub      frmThemes_ShowPage( byval nPage as long, byval bShow as boolean )
declare sub      frmThemes_ApplyTheme()
declare function frmThemes_IsProtectedTheme( byval wszShortFilename as DWSTRING ) as boolean
declare function frmThemes_MakeForkName( byval wszShortFilename as DWSTRING ) as DWSTRING
declare function frmThemes_WasEdited() as boolean
declare function frmThemes_PageForKey( byval sKey as string ) as long
declare function frmThemes_CategoryIdForKey( byval sKey as string ) as long
declare function frmThemes_CategoryTitle( byval nCat as long ) as DWSTRING
