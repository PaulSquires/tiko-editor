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

'' ========================================================================================
'' THE FIND/REPLACE ENGINE'S STATE AND ITS SEARCH FUNCTIONS -- 7c step 26.
''
'' Moved down out of src/modDeclares.bi and src/modFindReplace.inc. What stayed behind is
'' the WINDOW MANAGEMENT -- FindReplace_SetVisible, FindControls_Show and their siblings --
'' because that is what the shell replaces rather than ports.
''
'' ---- THE TYPE LOST EXACTLY TWO MEMBERS, AND BOTH WERE PRESENTATION --------------------
''
''   hCueBannerFont as HFONT   DELETED. Nothing read it. It has been dead since the cue
''                             banner became the text control's own business, and moving a
''                             dead Win32 handle into the portable layer to keep a diff
''                             small would have been the worse of the two choices.
''
''   rcResults as RECT         STAYED IN THE SHELL, as gFindResultsRect. It is read at five
''                             sites in frmFind.inc, all of them layout, and a rectangle
''                             measured by a Win32 window belongs with the window. Same
''                             split clsConfig took in 7c step 3.
''
'' Everything else in the type is DWSTRING, long and boolean.
''
'' ---- WHY THE ENGINE COULD COME DOWN AT ALL --------------------------------------------
''
'' Every search function was `SciExec(hEdit, ...)` on an HWND. SciExec IS SendMessage, and
'' app/modScintilla.bi has declared the same call in portable types since 7c step 3 for
'' exactly this reason:
''
''     (any ptr, ulong, uinteger, integer) as integer
''
'' So the port is a rename at 60-odd call sites and an `HWND` that becomes an `any ptr`.
'' clsDocument's own view members made the same move and its header says the same thing.
'' ========================================================================================

#pragma once

#include once "core/DWString.bi"
#include once "app/clsDocument.bi"


type FINDREPLACE_TYPE
    bFirstTimeInvoked   as boolean = true
    foundCount          as long
    txtFind             as DWSTRING
    txtReplace          as DWSTRING
    txtFindCombo(10)    as DWSTRING
    txtReplaceCombo(10) as DWSTRING
    txtFilesCombo(10)   as DWSTRING
    txtFolderCombo(10)  as DWSTRING
    txtLastFind         as DWSTRING
    txtFiles            as DWSTRING         '' *.*, *.bas, etc (FindInFolder)
    txtFolder           as DWSTRING         '' start search from this folder (FindInFolder)
    nSearchSubFolders   as long             '' search sub folders as well (FindInFolder)
    nWholeWord          as long             '' find/replace whole word search
    nMatchCase          as long             '' match case when searching
    nSelection          as long             '' search only selected text
    nPreserve           as long             '' preserve case when replacing
    nSearchCurrentDoc   as long
    nSearchAllOpenDocs  as long
    nSearchProject      as long
    wszResults          as DWSTRING = "0/0"
    bShowInfoPanel      as boolean = true
    bShowFindPanel      as boolean = false
    bShowReplacePanel   as boolean = false
    bProjectReplaceActive as boolean = false
end type

dim shared gFind as FINDREPLACE_TYPE
dim shared gFindInFiles as FINDREPLACE_TYPE

'' The start position of every highlighted match in the active document, in document order,
'' 1-based. Filled by FindReplace_CollectMatchPositions and valid only until the next call.
'' Module-level rather than a byref array parameter because it is redim'd as it grows.
dim shared gFindMatchPos(any) as long


'' Is the whole string upper- / lowercase? Used by the Preserve Case replace.
declare function isUpperCaseString( byval sText as string ) as boolean
declare function isLowerCaseString( byval sText as string ) as boolean

'' Move to the next or previous match, optionally repositioning the caret. TRUE when one
'' was found. Updates gFind.foundCount and gFind.wszResults.
declare function FindReplace_NextSelection( _
            byval startPos as long, _
            byval bGetNext as boolean, _
            byval bRepositionCaret as boolean _
            ) as boolean

'' Refresh the "current / total" count from where the caret actually is. Moves nothing.
declare function FindReplace_UpdateResultsFromCaret() as long

'' Re-highlight every match in the active document.
''
'' THE OCCURRENCE COLOUR IS A PARAMETER, and it is the one signature change this move made.
'' It used to read `theme.editor.occurrence` directly, and `theme` lives in the shell's
'' modThemeTypes.bi -- so the alternative was moving the whole theme tree down to satisfy
'' one indicator colour. A caller that has a theme passes it; a caller that does not passes
'' whatever it wants highlights to look like.
declare function FindReplace_HighlightSearches( byval clrOccurrence as ulong ) as long

'' Replace the current match, or every match. Reads gFind.txtFind / gFind.txtReplace.
'' ---- clrOccurrence IS LAST, AND IT HAS NO DEFAULT ------------------------------------
'' It went FIRST when this moved down, and boolean converts to ulong without a murmur -- so
'' every existing call site kept compiling and shifted one place. `DoReplace(false, true)`,
'' which meant "replace this one, then move on", became clrOccurrence = 0 and
'' fReplaceAll = TRUE. Replace did Replace All, silently, and no gate could see it.
''
'' LAST AND MANDATORY means a call site that was not updated is a COMPILE ERROR rather than
'' a different meaning. That is the only property of a signature change that matters here.
declare function FindReplace_DoReplace( _
            byval fReplaceAll as boolean, _
            byval fMovenext as boolean, _
            byval clrOccurrence as ulong _
            ) as long
