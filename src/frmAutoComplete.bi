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

''
''  frmAutoComplete.bi
''
''  The code-completion popup. Replaces Scintilla's own autocomplete listbox
''  (SCI_AUTOCSHOW and friends) with a tiko-owned window so that the list can carry
''  symbol kinds, be drawn in the editor's theme, and obey a keyboard contract this
''  app decides rather than inherits.
''
''  SHAPE: a persistent WS_POPUP + WS_EX_NOACTIVATE shell (the PsPopupMenu window
''  contract) hosting a PsListTree child. The popup NEVER takes focus -- Scintilla keeps
''  it, keeps its caret, and keeps receiving every typed character. Navigation keys
''  reach us through frmAutoComplete_FilterMessage, called from frmMain's message pump.
''
''  Two rules that are easy to break and hard to notice:
''
''  1. The filter consumes ONLY navigation/commit keys. Its PsMenuBar/PsPopupMenu
''     siblings consume every keydown (menu semantics); doing that here would stop
''     the user from typing.
''
''  2. Items hold no SYMBOLREF. A SYMBOLREF is {pRSet, idx}, and a background parse
''     completing (MSG_USER_PARSE_COMPLETE) installs a new result set and FREES the
''     displaced one -- every ref into it would dangle while the popup was still up.
''     Everything the popup needs is snapshotted into DWSTRINGs at build time.
''

#define IDC_FRMAUTOCOMPLETE_LISTBOX   1000

' Number of rows shown before the list scrolls. Matches the SCI_AUTOCSETMAXHEIGHT 9
' the Scintilla popup was configured with, so the popup keeps its familiar size.
#define AUTOCOMPLETE_VISIBLE_ROWS     9

' Popup geometry, in unscaled units -- AutoC_Layout runs every one of these through
' CWindow's ScaleX/ScaleY. The width clamp keeps a popup readable without letting one
' 200-character calltip detail stretch it across the monitor.
#define AUTOCOMPLETE_GLYPH_WIDTH     22    ' left gutter holding the kind glyph
#define AUTOCOMPLETE_COLUMN_GAP      16    ' minimum gap between the name and the detail
#define AUTOCOMPLETE_PAD_RIGHT        8
#define AUTOCOMPLETE_MIN_WIDTH      220
#define AUTOCOMPLETE_MAX_WIDTH      560

' Row height, in UNSCALED units -- the same contract as PsListTree_SetRowHeight, which
' DPI-scales what it is given. A completion list wants tighter rows than a panel list
' (PsListTree's own default is 22): it is a transient overlay you scan, not a tree you
' browse. Override at runtime with frmAutoComplete_SetRowHeight.
#define AUTOCOMPLETE_DEFAULT_ROW_HEIGHT   20

' Pseudo-kinds for candidates the parser never produced. Negative so they cannot
' collide with the FBCP_KIND_* enum, which starts at 1.
#define AUTOC_KIND_KEYWORD   (-1)
#define AUTOC_KIND_STDTYPE   (-2)

' One candidate. Every field is a SNAPSHOT taken during the build -- see rule 2 above.
type AUTOC_ITEM
    wszName    as DWSTRING     ' the text inserted on commit, in its original casing
    wszSort    as DWSTRING     ' ucase(wszName); the key for filtering, sorting and dedupe
    wszDetail  as DWSTRING     ' dimmed right-aligned hint (type text / return type); may be ""
    nKind      as long         ' FBCP_KIND_* or one of the AUTOC_KIND_* pseudo-kinds
    ' A callable that takes at least one caller-supplied parameter. Decided at BUILD
    ' time (the symbol is in hand there, and a snapshot cannot dangle) and read at
    ' commit time to answer one question: should "(" accept this row? See the WM_CHAR
    ' case in frmAutoComplete_FilterMessage.
    bHasParams as boolean
end type

declare sub      frmAutoComplete_Init()
declare function frmAutoComplete_Show( byval pDoc as clsDocument ptr, items() as AUTOC_ITEM, byval nCount as long ) as boolean
declare sub      frmAutoComplete_Cancel()
declare function frmAutoComplete_IsOpen() as boolean
declare function frmAutoComplete_ReFilter( byval pDoc as clsDocument ptr ) as boolean
declare function frmAutoComplete_Commit( byval pDoc as clsDocument ptr ) as boolean
declare function frmAutoComplete_FilterMessage( byval pMsg as MSG ptr ) as boolean
declare sub      frmAutoComplete_SyncTheme()

' Row height in UNSCALED units (DPI scaling is applied downstream -- do not pre-scale).
' Takes effect on the next popup.
declare sub      frmAutoComplete_SetRowHeight( byval nHeight as long )
declare function frmAutoComplete_GetRowHeight() as long
