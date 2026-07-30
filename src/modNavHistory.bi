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

' ========================================================================================
' modNavHistory  --  browser-style navigation history (Go Back / Go Forward)
'
' Replaces the old "Last Position" LIFO (LASTPOSITION_TYPE / gLastPosition), which had no
' Forward step, recorded from only two call sites, and stored a raw clsDocument ptr that
' dangled the moment a file was closed.
'
' MODEL: one list plus a cursor. gNavEntries(gNavCursor) is *where we are now*. A new
' navigation truncates everything after the cursor and appends -- exactly what a browser,
' Visual Studio and VS Code do. gNavCursor = -1 means the list is empty.
'
' IDENTITY: an entry keys on the DiskFilename, never on a clsDocument ptr, so Back can
' reopen a file the user has since closed and no entry can ever dangle.
'
' DURABILITY: the exact caret offset is only meaningful while the document is unchanged, so
' every entry also carries the line and the document length at capture time. On the way back
' NavHistory_UseExactPosition() decides: same length => exact offset, otherwise fall back to
' the line. See "Not covered" in the design notes -- this is not marker-tracked, so a line
' is itself approximate once text has been inserted above it.
'
' THIS HEADER MUST NAME NO Ps* TYPE. It is included early (clsConfig.inc calls
' NavHistory_Clear from its session-load paths), well ahead of every Ps* control.
' ========================================================================================

#include once "clsDocument.bi"

' Deepest history we keep. Past this the OLDEST entry is dropped -- the far end of a long
' history is worth less than a bounded array.
const NAVHISTORY_MAX = 50

type NAVHISTORY_ENTRY
    wszFilename as DWSTRING    ' matches clsDocument.DiskFilename -- the stable identity
    nPosition   as long        ' SCI_GETCURRENTPOS at capture (exact, but perishable)
    nLine       as long        ' SCI_LINEFROMPOSITION( nPosition ) -- the fallback
    nFirstLine  as long        ' SCI_GETFIRSTVISIBLELINE, so the scroll position is restored
    nDocLength  as long        ' SCI_GETLENGTH at capture -- the staleness test
end type

dim shared gNavEntries(any) as NAVHISTORY_ENTRY
dim shared gNavCursor as long = -1

' Re-entrancy counter. NavHistory_Goto drives OpenSelectedDocument, which is itself a
' recording site -- without this, Back records its own destination and the cursor never
' actually moves. Every recording entry point returns early while this is non-zero.
dim shared gNavSuppress as long

' The debounced edit location. A run of typing on one line yields ONE entry, appended when
' the caret next leaves that line (or when a different document is edited).
dim shared gNavPending as NAVHISTORY_ENTRY
dim shared gNavPendingActive as boolean

declare sub NavHistory_Clear()
declare function NavHistory_CanBack() as boolean
declare function NavHistory_CanForward() as boolean
declare function NavHistory_Count() as long
declare function NavHistory_UseExactPosition( byref e as NAVHISTORY_ENTRY, byval nDocLength as long ) as boolean
declare function NavHistory_AppendEntry( byref e as NAVHISTORY_ENTRY ) as boolean
declare function NavHistory_CaptureLive( byref e as NAVHISTORY_ENTRY ) as boolean
declare function NavHistory_IsRecordingAllowed() as boolean
declare sub NavHistory_RecordJump()
declare sub NavHistory_NoteArrival()
declare sub NavHistory_RefreshCurrent()
declare sub NavHistory_Goto( byval idx as long )
declare sub NavHistory_NoteEdit( byval pDoc as clsDocument ptr )
declare sub NavHistory_CheckPendingEdit( byval pDoc as clsDocument ptr )
declare sub NavHistory_FlushPendingEdit()
declare function NavHistory_Back() as long
declare function NavHistory_Forward() as long
declare sub NavHistory_RunSelfTest()
