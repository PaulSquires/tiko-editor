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
' THE PAGE ID IS THE SELECT-BAR PANEL INDEX. They must be equal, and the order of these
' defines must match the order of the PsSelectBar_AddPanel calls in frmThemes_Build.
'
' That is not a style preference. frmThemes_BarSelChange receives the panel INDEX from the
' control and passes it straight to frmThemes_SelectPage as a page ID, and
' PsSelectBar_SetCurSel takes an INDEX too -- so the dialog conflates the two throughout and
' only ever worked because they happened to match. Appending THEMEPAGE_PALETTE = 2 while
' showing it FIRST broke that silently: the bar highlighted Interface (index 2) while the
' list filled with roles. The invariant is asserted rather than remembered.
#Define THEMEPAGE_PALETTE     0
#Define THEMEPAGE_SYNTAX      1
#Define THEMEPAGE_INTERFACE   2
#Define THEMEPAGE_COUNT       3

' Shell metrics, UNSCALED -- run through AfxScaleX/AfxScaleY at use. FIXED SIZE, like
' frmOptions: the colour page's three columns have to fit one known width, and a resizable
' dialog would make "does it fit" a question with no stable answer.
#Define FRMTHEMES_CLIENT_W         720
#Define FRMTHEMES_CLIENT_H         432
#Define FRMTHEMES_LIST_W           240
#Define FRMTHEMES_BAR_H             34
#Define FRMTHEMES_FOOTER_H          48
#Define FRMTHEMES_BTN_W             92
#Define FRMTHEMES_BTN_H             32
#Define FRMTHEMES_MARGIN            16

' EDIT-view header band (UNSCALED): a left-aligned stack of the "Edit theme" title, the
' description text + its edit button, and the Back button, above the Syntax/Interface bar.
#Define FRMTHEMES_EDITHDR_H        122

' Selection-view metrics (UNSCALED). The action buttons stack down the right; the list takes
' the remainder. HEADER_H is the drawn band at the top holding the "active theme" heading and
' the one-line instruction, mirroring the localization page's drawn heading.
#Define FRMTHEMES_SELBTN_W         128
#Define FRMTHEMES_SELBTN_H          51
#Define FRMTHEMES_SELBTN_GAP        10
#Define FRMTHEMES_HEADER_H          72

' The two views share one window. THEMEVIEW_SELECT is the theme picker (list + Edit/Clone/
' Delete/Set-active + Close); THEMEVIEW_EDIT is the colour editor (Syntax/Interface bar + page
' + Back). gThemesShowEdit selects between them, exactly the frmOptionsLocal gLocalShowEdit split.
#Define THEMECOL_NAME              0
#Define THEMECOL_DESC              1
#Define THEMECOL_ACTIVE           2

#Define IDC_FRMTHEMES_LSTTHEMES   9600
#Define IDC_FRMTHEMES_SELECTBAR   9601
#Define IDC_FRMTHEMES_PAGE        9602
#Define IDC_FRMTHEMES_CMDEDIT     9603
#Define IDC_FRMTHEMES_CMDCLONE    9604
#Define IDC_FRMTHEMES_CMDDELETE   9605
#Define IDC_FRMTHEMES_CMDSETACTIVE 9606
#Define IDC_FRMTHEMES_CMDCLOSE    9607
#Define IDC_FRMTHEMES_CMDGOBACK   9608
#Define IDC_FRMTHEMES_CMDSAVE     9609
#Define IDC_FRMTHEMES_CMDREVERT   9610
#Define IDC_FRMTHEMES_TXTDESC     9611
#Define IDC_FRMTHEMES_CMDDESCEDIT 9612

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
#Define IDC_FRMTHEMES_PICKER2     9510   ' the selection-only picker inside the colour popup
' Two R/G/B rows, one per CHANNEL. The page used to carry one row that edited whichever
' channel was "active" -- state signalled only by the default border on the Foreground
' button -- so a key could offer two colours to pick and one to type. Two rows delete the
' mode rather than labelling it.
#Define IDC_FRMTHEMES_TXTR        9511
#Define IDC_FRMTHEMES_TXTG        9512
#Define IDC_FRMTHEMES_TXTB        9513
' "Use the palette colour" -- hands a channel back to its role. The other half of override:
' without it every edit detaches a key permanently and the palette decays one key at a time.
#Define IDC_FRMTHEMES_CMDINHF     9517
#Define IDC_FRMTHEMES_CMDINHB     9518
#Define IDC_FRMTHEMES_TXTR2       9514
#Define IDC_FRMTHEMES_TXTG2       9515
#Define IDC_FRMTHEMES_TXTB2       9516

' The THEMECAT_* group-header ids have MOVED to modThemeTypes.bi. They are engine vocabulary
' now: the key table carries each key's category, so the ids have to exist long before this
' dialog is loaded. Nothing else about them changed.

dim shared HWND_FRMTHEMES       as HWND
dim shared HWND_FRMTHEMES_PAGE  as HWND

' The theme this dialog is STAGING. It replaces gOptWork.ThemeShortFilename, which belonged to
' frmOptions' staging session and is not available from a dialog that can be opened without it.
' Committed to gConfig only by OK.
dim shared gThemesWorkFile as DWSTRING

' TRUE once the user has typed in the Description field this editing session. It decides whether
' a fork of a built-in keeps the auto "(custom)" suffix (user did not rename) or the user's own
' text (they did). Set in frmThemes.inc; read in frmThemesPage.inc's SaveThemeFile.
dim shared gThemesDescEdited as boolean

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
' Editor-lifecycle hooks. Defined in frmThemesPage.inc (where gThemeEdited / ghGeneral /
' EnsureForked live); forward-declared here because frmThemes.inc's handlers, compiled first,
' call them.
declare sub      frmThemes_MarkEdited()
declare sub      frmThemes_ClearEdited()
declare function frmThemes_SaveThemeFile() as boolean
declare function frmThemes_PageForKey( byval sKey as string ) as long
declare function frmThemes_CategoryIdForKey( byval sKey as string ) as long
declare function frmThemes_CategoryTitle( byval nCat as long ) as DWSTRING
' The colour popup's host-pump filter (Escape / outside-click dismissal). Defined in
' frmThemesPage.inc; the pump in frmThemes.inc, compiled first, calls it.
declare function frmThemes_ColorPopupFilterMessage( byval pMsg as MSG ptr ) as boolean
declare sub      frmThemes_DestroyColorPopup()
' A click on the page: opens the colour popup if it hit a Fore/Back swatch. Called from the
' page WndProc (frmThemes.inc), defined in frmThemesPage.inc.
declare function frmThemes_SwatchClick( byval x as long, byval y as long ) as boolean
