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
#Define IDC_FRMOPTIONS_SEARCH                       1011
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
'                   =  (720 - 196)       x  (434 - 44 - 48)
'                   =   524              x   342          , unscaled
'
' THE HEIGHT WAS CUT 30% (620 -> 434). That did not come out of the page metrics alone: the
' tallest page, Code Editor, is twelve rows, and a row cannot be shorter than the taller of its
' control and its title. So the toggle itself was shrunk for this dialog -- see
' OPTROW_TOGGLE_TRACKH in modOptionsRows.bi -- alongside the row height, the row padding and the
' two bands. Twelve rows now need 8 + 12*26 + 8 = 328 of the 342 available.
'
' THE WIDTH WAS THEN CUT 20% (900 -> 720). The nav column did NOT take a proportional share:
' at 176 the longest nav title ("Windows API Keywords") cleared its row by 4px, which is not a
' margin. 196 leaves it 22 unscaled px of slack. The page keeps 524, and the widest row title
' ("Margin click toggles Breakpoint rather than Bookmark") needs 617 of the 744px it is given
' at 175% DPI -- measured, because the labels ellipsize SILENTLY and nothing asserts it.
'
' A page that needs more than that no longer scrolls -- only OPTPAGE_EDITOR does -- so it would
' CLIP. frmOptions_SelfTest asserts the fit for every page rather than leaving it to be noticed.
#Define FRMOPTIONS_CLIENT_W                          720
#Define FRMOPTIONS_CLIENT_H                          432
#Define FRMOPTIONS_NAV_W                             196
#Define FRMOPTIONS_SEARCH_H                          34    ' the search box in the nav column
#Define FRMOPTIONS_SEARCH_GLYPHW                     34    ' the magnifying-glass gutter left of it
#Define FRMOPTIONS_TITLE_H                            44
#Define FRMOPTIONS_FOOTER_H                           48
#Define FRMOPTIONS_BTN_W                              92
#Define FRMOPTIONS_BTN_H                              32
#Define FRMOPTIONS_MARGIN                             16

dim shared OptionsDialogLastOpened as long

dim shared HWND_FRMOPTIONS_NAV    as HWND
dim shared HWND_FRMOPTIONS_SEARCH as HWND    ' PsTextBox -- filters the nav list by page title
dim shared HWND_FRMOPTIONS_PAGE   as HWND    ' the single, non-scrolling page container

declare function frmOptions_Show( byval hWndParent as HWND ) as LRESULT
declare sub      frmOptions_ShowPage( byval nPage as long )
' Re-flow the current page after its own content height changed (e.g. a panel page
' showing/hiding part of itself). Was PsScrollPanel_Recalc; there is no scroll panel now.
declare sub      frmOptions_RelayoutCurrentPage()

' A theme-matched PsMessageBox owned by the dialog -- a thin forwarder onto TikoMsgBox that
' supplies the owner and the Options font. nIcon is TMB_ICON_*, nButtons is TMB_OK or
' TMB_OKCANCEL; returns the dismissing id (IDOK / IDCANCEL). Runs its own nested modal loop,
' so it is safe to call from inside the dialog's message loop.
declare function frmOptions_ThemedMsgBox( byval wszText as DWSTRING, _
                                          byval wszCaption as DWSTRING, _
                                          byval nIcon as long, _
                                          byval nButtons as long ) as long
