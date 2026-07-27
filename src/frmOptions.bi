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

#Define IDC_FRMOPTIONS_NAVLIST                      1000
#Define IDC_FRMOPTIONS_SCROLLPANEL                  1010
#Define IDC_FRMOPTIONS_CMDOK                        1003
#Define IDC_FRMOPTIONS_CMDCANCEL                    1001

' Control ids for the table-driven rows. Each row takes ONE id, its own table index added
' to this base. Kept well clear of the ids above; PsScrollPanel additionally consumes
' IDC_FRMOPTIONS_SCROLLPANEL + 1 (its page) and + 2 (its scrollbar).
#Define IDC_FRMOPTIONS_FIRSTROW                     2000

' Shell metrics, UNSCALED -- run through AfxScaleX/AfxScaleY at use.
' THE CLIENT SIZE IS FIXED -- the dialog carries no WS_THICKFRAME. There is deliberately no
' minimum size any more: there is only one size.
'
' Everything a page is allowed to occupy falls out of these four numbers, and it is worth writing
' the sum down because every page layout is measured against it:
'
'     page viewport = (CLIENT_W - NAV_W) x (CLIENT_H - TITLE_H - FOOTER_H)
'                   =  (900 - 220)       x  (620 - 52 - 58)
'                   =   680              x   510          , unscaled
'
' A page that needs more than that no longer scrolls -- only OPTPAGE_EDITOR does -- so it would
' CLIP. frmOptions_SelfTest asserts the fit for every page rather than leaving it to be noticed.
#Define FRMOPTIONS_CLIENT_W                          900
#Define FRMOPTIONS_CLIENT_H                          620
#Define FRMOPTIONS_NAV_W                             220
#Define FRMOPTIONS_TITLE_H                            52
#Define FRMOPTIONS_FOOTER_H                           58
#Define FRMOPTIONS_BTN_W                              92
#Define FRMOPTIONS_BTN_H                              32
#Define FRMOPTIONS_MARGIN                             16

dim shared OptionsDialogLastOpened as long

dim shared HWND_FRMOPTIONS_NAV    as HWND
dim shared HWND_FRMOPTIONS_PAGE   as HWND    ' the single, non-scrolling page container

declare function frmOptions_Show( byval hWndParent as HWND ) as LRESULT
declare sub      frmOptions_ShowPage( byval nPage as long )
' Re-flow the current page after its own content height changed (e.g. a panel page
' showing/hiding part of itself). Was PsScrollPanel_Recalc; there is no scroll panel now.
declare sub      frmOptions_RelayoutCurrentPage()

' A theme-matched PsMessageBox owned by the dialog. nPreset is MBX_BTN_OK or
' MBX_BTN_OKCANCEL; returns the dismissing id (IDOK / IDCANCEL). Runs its own nested modal
' loop, so it is safe to call from inside the dialog's message loop.
declare function frmOptions_ThemedMsgBox( byval wszText as DWSTRING, _
                                          byval wszCaption as DWSTRING, _
                                          byval nIconKind as long, _
                                          byval nPreset as long ) as long
