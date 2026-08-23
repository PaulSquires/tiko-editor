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

'  frmOptionsKeywords.bi
'
'  The three Keywords pages (FreeBASIC / Windows API / Extra), consolidated into one
'  panel-hosted module -- they only ever differed in which gConfig field they edited. Each
'  page is a single large multiline PsTextBox that fills the pane, with one shared PsVScrollBar
'  driven by whichever page is visible (the same arrangement frmOutput uses for its two
'  textboxes). The three old frmOptionsKeywords / ...WinApi / ...Extra child forms are gone.
'
'  There is no staging copy here: the keyword blobs are large, so instead of mirroring them
'  the module harvests the (built) textboxes straight into gConfig on OK, gated on the
'  gConfig.bKeywordsDirty flag exactly as the old dialog was. An unvisited page has no
'  textbox and is simply not harvested, so its gConfig value is left untouched.

#pragma once

#include once "PsControls\PsScrollPanel.bi"        ' SCP_PAINTINFO, pulled in early

#Define IDC_FRMOPTIONSKEYWORDS_TXTBASE   9500     ' + page index (0..2)
#Define IDC_FRMOPTIONSKEYWORDS_VSCROLL   9510

declare function frmOptionsKeywords_OwnsPage( byval nPage as long ) as boolean
declare sub      frmOptionsKeywords_Reset()
declare sub      frmOptionsKeywords_Build( byval nPage as long, byval hPage as HWND )
declare function frmOptionsKeywords_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long
declare sub      frmOptionsKeywords_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
declare sub      frmOptionsKeywords_Show( byval nPage as long, byval bShow as boolean )
declare sub      frmOptionsKeywords_ApplyTheme()
declare sub      frmOptionsKeywords_Harvest()
