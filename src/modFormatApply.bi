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
' CODE FORMATTER -- Scintilla bridge declarations.
'
' Split from modFormatApply.inc for the frmAutoComplete / frmBuildConfig reason: the three
' call sites (frmMainOnNotify's SCN_CHARADDED, frmMainEdit's paste path and
' frmMainOnCommand's dispatch) are all compiled well before the .inc can be, because the
' .inc needs PsMessageBox (for TikoMsgBox) and therefore has to land near the end.
'
' This header names clsDocument ptr but NO Ps* type, so it can sit up with the other early
' declarations.
' ========================================================================================

#pragma once

declare function Format_IsFormattableDocument( byval pDoc as clsDocument ptr ) as long
declare function Format_ApplyToDocument( byval pDoc as clsDocument ptr, _
                                         byval nFirstLine as long, _
                                         byval nLastLine as long ) as long
declare function Format_Document() as long
declare function Format_Selection() as long
declare function Format_AllOpenDocuments( byref nFilesChanged as long ) as long

' The two typing-time triggers. Both return immediately unless their gConfig.Format flag is
' set, so the call sites need no guard of their own.
declare sub Format_OnEnterPressed( byval pDoc as clsDocument ptr )
declare sub Format_OnPaste( byval pDoc as clsDocument ptr, _
                            byval nPosBefore as long, _
                            byval nLenBefore as long )

' Menu command handlers.
declare function OnCommand_FormatDocument() as LRESULT
declare function OnCommand_FormatSelection() as LRESULT
declare function OnCommand_FormatAllOpenDocuments() as LRESULT
declare function OnCommand_FormatProject() as LRESULT

' Env-gated (TIKO_FORMAT_SELFTEST=1). The localized ids only -- see the suite header.
declare sub Format_RunApplySelfTest()
