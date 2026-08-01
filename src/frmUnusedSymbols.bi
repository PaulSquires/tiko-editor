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

' ==========================================================================================
' frmUnusedSymbols - the Unused Symbols report.
'
' A modeless WS_POPUP owned by frmMain, on frmDebug's shape. MODELESS is the whole point:
' the list is a worklist, so the user clicks a row, edits the source, and comes back to it.
' A modal dialog would have to close on every jump.
'
'   Scanned: <root file>                                          (painted)
'   Unsaved changes (line numbers may be stale): a.bas, b.inc      (painted, hidden if none)
'   [x] Variables [x] Procedures [x] Parameters [x] Types [x] Fields [x] Constants
'   [PsListTree]  File | Line | Class | Name | Status
'   1183 / 1183 shown                                             (painted)
'
' The MODEL is modUnusedSymbols.*; this file owns only the window. That split is what lets
' the comparator and the filter test be asserted with no window open.
'
' ------------------------------------------------------------------------------------------
' NO PUMP OBLIGATION IS ADDED.
'
' The list does not enable label editing, so PsListTree_FilterMessage is not required for it
' -- and frmMain's pump already calls it anyway for the debugger's watch list. Nothing this
' window creates carries a pump obligation; do not add a frmUnusedSymbols_FilterMessage.
'
' ------------------------------------------------------------------------------------------
' SORTING IS THIS FILE'S JOB, NOT THE CONTROL'S.
'
' PsColumnHeader never sorts. It reports a click and the host reorders its own data and draws
' its own glyph -- which is why this file carries a LOCAL header painter instead of using the
' shared ListBox_LVPaintHeader: that one has no sort-indicator support and four other panes
' depend on it unchanged.
'
' The pattern is seeded from PsListTree's own demo (PsListTree/frmMain.inc,
' Header_ClickCallback), including the PsColumnHeader_Refresh at the end - without it the
' arrow stays on the previously sorted column.
' ==========================================================================================

#pragma once

#define IDC_FRMUNUSED_LIST         1001
#define IDC_FRMUNUSED_TOGGLEFIRST  1010
#define IDC_FRMUNUSED_TOGGLELAST   1015

' Segoe Fluent glyphs, defined ONCE and here, in the ESCAPE form.
' Never paste the character itself: these files are BOM-less, so fbc reads a pasted glyph as
' its three UTF-8 bytes in Latin-1 and draws garbage from a perfectly clean build.
#define GLYPH_CHEVRONUP     !"\uE70E"
#define GLYPH_CHEVRONDOWN   !"\uE70D"

' unscaled; SetClientSize applies the DPI factor itself
const as long FRMUNUSED_DEFWIDTH  = 1000
const as long FRMUNUSED_DEFHEIGHT = 640

type UnusedWindowPosition
    bInitialized as boolean = false
    bMaximized   as boolean = false
    nLeft        as long
    nTop         as long
    nRight       as long
    nBottom      as long
end type
dim shared gUnusedPos as UnusedWindowPosition

' THE REPORT GOES STALE AS SOON AS THE USER ACTS ON IT: deleting an unused symbol shifts
' every line number below it in that file. Refresh re-runs the PROJECT SCAN rather than
' re-reading the current one -- the reference counts themselves have to be recomputed, not
' just the rows. That is asynchronous (the scan runs on clsScanMgr's worker), so the rebuild
' happens when MSG_USER_PARSE_COMPLETE lands in frmMain and calls Rebuild().
declare sub      frmUnusedSymbols_Rebuild()
declare function frmUnusedSymbols_Show( byval hWndParent as HWND ) as LRESULT
declare sub      frmUnusedSymbols_ApplyTheme()
declare function frmUnusedSymbols_IsOpen() as boolean
declare sub      frmUnusedSymbols_RunLayoutSelfTest()

' Pure: the band geometry, as a function of the client size and the band heights. Split out
' so it can be asserted without a window.
type FRMUNUSED_LAYOUT
    rcHeader  as RECT       ' "Scanned:" line + the dirty-buffer warning
    rcToggles as RECT       ' the six kind filters
    rcList    as RECT
    rcStatus  as RECT
end type

declare sub frmUnusedSymbols_ComputeLayout( byval cx as long, byval cy as long, _
                                            byval nHeader as long, byval nToggles as long, _
                                            byval nStatus as long, _
                                            byref lo as FRMUNUSED_LAYOUT )
