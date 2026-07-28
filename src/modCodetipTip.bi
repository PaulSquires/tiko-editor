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
''  modCodetipTip.bi
''
''  The code tip window -- the parameter signature shown when you type "(" or "," inside
''  a call. Replaces Scintilla's own calltip (SCI_CALLTIPSHOW / SCI_CALLTIPCANCEL) with a
''  PsTooltip that tiko owns, so it is drawn in the editor theme and dismissed by rules
''  this app decides rather than inherits.
''
''  DECLARATIONS ONLY, AND DELIBERATELY NAMING NO Ps* TYPE. modCodetips.inc -- which calls
''  Codetip_Show -- is included at tiko.bas:103, well before PsTooltip.inc at :148, so it
''  cannot see a PsTooltip_* declaration. This header goes in early beside
''  frmAutoComplete.bi; the implementation goes in immediately after PsTooltip.inc. Same
''  split, and for the same reason, as frmAutoComplete's.
''
''  THE ONE STRUCTURAL DECISION: a code tip is CARET-driven, not hover-driven, so it must
''  not be subject to PsTooltip's dwell clock -- which would hide it the moment the cursor
''  left the editor, on any mouse button, and after the auto-pop delay. Codetip_Init
''  therefore creates the tip (PsTooltip_Create attaches) and DETACHES it immediately:
''  TIP_TickOne bails at "if pTip->hAttached = 0", but only AFTER advancing the fade, so a
''  detached tip still fades in and out and nothing else in the tick can touch it. Every
''  show and hide below is manual.
''
''  No pump obligation comes from PsTooltip (it has none). Codetip_FilterMessage is tiko's
''  own Escape handling and consumes nothing else.
''

' Create the singleton. Once, from frmMain, after the theme and ghFont() are built.
declare sub      Codetip_Init()
declare sub      Codetip_Destroy()

' Show the tip for a call whose "(" is at nAnchorPos. wszText is already formatted
' (FormatCodetip) and may carry embedded line feeds -- PsTooltip draws the message with
' PaintTextEx, so DrawText honours them.
declare function Codetip_Show( byval pDoc as clsDocument ptr, byval wszText as DWSTRING, _
                               byval nAnchorPos as long ) as boolean
declare sub      Codetip_Hide()
declare function Codetip_IsVisible() as boolean

' Called from SCN_UPDATEUI. Hides the tip when the caret has left the call, and follows
' the anchor across the screen when the view scrolls.
declare sub      Codetip_SyncToCaret( byval pDoc as clsDocument ptr )

' THE DISMISSAL LAW, as a PURE FUNCTION -- which is what makes it assertable with no
' window, no caret and no message pump. Everything Codetip_SyncToCaret decides comes from
' here. nAnchorPos < 0 means "no tip is anchored", which dismisses.
declare function Codetip_ShouldDismiss( byval nCaretPos   as long, byval nCaretLine  as long, _
                                        byval nAnchorPos  as long, byval nAnchorLine as long _
                                        ) as boolean

' Escape only, and only while the tip is up. Sits in frmMain's pump ahead of
' handleEscKeyModeless, or the Escape meant for the tip closes the Find panel instead.
declare function Codetip_FilterMessage( byval pMsg as MSG ptr ) as boolean

' Re-apply the editor theme. Called from the same places frmAutoComplete_SyncTheme is.
declare sub      Codetip_SyncTheme()

declare sub      Codetip_RunSelfTest()
