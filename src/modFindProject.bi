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

' ========================================================================================
' Find in Project -- the search engine and the result model.
'
' No windows, no Ps* controls: this half is only the data and the search. The surface that
' draws it is frmFindInProject.
'
' SCOPE is every document in gApp.pDocList -- exactly what the Explorer pane lists. Any
' extension, untitled buffers included, and project files that were loaded but never given a
' tab. Nothing is read from disk: an unrealized document's bytes are in pDoc->TextBuffer, and
' a realized one's live in its Scintilla document, which is where unsaved edits are.
'
' THE SEARCH IS SCINTILLA'S OWN, through SCI_SEARCHINTARGET with the same SCFIND_* flags the
' in-document Find bar uses, so the two cannot disagree about what matches. It runs entirely
' on a hidden scratch view, never on the user's editor windows -- search flags and the target
' range are per-view state, and clobbering them under the Find bar would be a bug at a
' distance. For a document that already has Scintilla windows the scratch is pointed at its
' document with SCI_SETDOCPOINTER (no copy); for one that does not, its TextBuffer is loaded
' into the scratch's own buffer. Either way the SEARCH LOOP is one function.
'
' Positions are BYTE OFFSETS into the document, which is what Scintilla deals in. They are
' remapped as the user edits -- see FindProject_RemapMatch.
' ========================================================================================

' Lines of context either side of a matched line, so a lone match shows 5 lines.
#define FINDPROJ_CONTEXT_LINES  2

' ----------------------------------------------------------------------------------------
' TWO CAPS, both found by measurement rather than caution.
'
' Searching a project for a single common character -- "e" -- left the editor unresponsive
' for over THREE MINUTES with no result, which from the outside is indistinguishable from a
' crash. Two things go quadratic-ish at once, and each needs its own bound:
'
'   * The sheer number of matches. Every one is recorded, indicator-filled on its document,
'     and walked by navigation.
'   * The COALESCING. When nearly every line matches, their context windows all touch, so a
'     whole file collapses into ONE block spanning thousands of lines -- and each block
'     snapshots every one of its lines for the dirty-stripe baseline. An excerpt that is the
'     entire file is not an excerpt anyway.
'
' A capped search is reported as capped (see FindProject_WasCapped), never silently truncated.
' ----------------------------------------------------------------------------------------
#define FINDPROJ_MAX_MATCHES     5000
#define FINDPROJ_MAX_BLOCK_LINES   40

' Control id for the hidden scratch view. Deliberately NOT IDC_SCINTILLA(+1), which
' clsDocument.IsValidScintillaID matches and frmMain_OnNotify uses to decide whether a
' notification belongs to a document.
#define IDC_FINDPROJ_SCRATCH  399

type FINDPROJ_MATCH
    nStart as long          ' byte offset of the match within its document
    nLen   as long          ' byte length
    nLine  as long          ' 0-based document line, cached for excerpt building
    ' Set when an edit destroyed the match (typed over, or deleted). The entry is KEPT rather
    ' than removed so that every stored index stays valid -- blocks and groups address matches
    ' by range, and compacting the array would invalidate all of them at once.
    bDead  as boolean
end type

' A coalesced run of lines shown as one excerpt. Matches whose context windows overlap or
' touch share a block, so no line is ever displayed twice.
type FINDPROJ_BLOCK
    nGroup      as long
    nFirstLine  as long
    nLineCount  as long
    nMatchFirst as long     ' index into the model's flat matches array
    nMatchCount as long
    ' The block's lines AS THEY WERE when the search ran, in the model's flat origLines
    ' array. This is the baseline the dirty stripe compares against, so a line that is edited
    ' and then undone stops being dirty -- which a "was it touched" flag cannot express.
    nOrigFirst  as long
    nOrigCount  as long
end type

' One file's worth of results. Matches and blocks live in the model's FLAT arrays and are
' addressed here by range -- which keeps the model free of nested variable-length arrays and
' makes the whole thing trivially walkable in display order.
type FINDPROJ_GROUP
    pDoc        as clsDocument ptr
    pSciDoc     as any ptr          ' its Scintilla document (the excerpt views bind to this)
    wszFile     as DWSTRING         ' full path, or the Untitled name
    bCollapsed  as boolean
    nMatchFirst as long
    nMatchCount as long
    nBlockFirst as long
    nBlockCount as long
end type

type FINDPROJ_MODEL
    groups(any)  as FINDPROJ_GROUP
    matches(any) as FINDPROJ_MATCH
    blocks(any)  as FINDPROJ_BLOCK
    origLines(any) as DWSTRING     ' every block's lines as found, addressed by range
    nOrigCount   as long
    nGroupCount  as long
    nMatchCount  as long            ' entries in matches(), dead ones included
    nBlockCount  as long
    nLiveMatches as long            ' matches still alive; this is what the n/m count reports
    wszPhrase    as DWSTRING
    bMatchCase   as boolean
    bWholeWord   as boolean
    bValid       as boolean         ' a search has been run
    bCapped      as boolean         ' the match cap was hit; results are incomplete
end type
dim shared gFip as FINDPROJ_MODEL

' Set while a replace loop is rewriting the model itself, so the SCN_MODIFIED hook does not
' also try to remap the same edit. (Used from Phase 6's project-wide replace.)
dim shared gFipSuppressRemap as boolean

' ----------------------------------------------------------------------------------------
' The model tells the surface it changed through this, rather than calling into it directly:
' modFindProject is included well before frmFindInProject and must not name it. The surface
' installs it when it is created.
' ----------------------------------------------------------------------------------------
type FIP_ModelChangedSub as sub()
dim shared gFipModelChanged as FIP_ModelChangedSub
' Repaint only -- no rebuild, no re-binding, and above all no hiding of windows. Used on the
' per-keystroke path, where a rebuild would take the focus away from the excerpt being typed
' into.
dim shared gFipRepaint      as FIP_ModelChangedSub

' ----------------------------------------------------------------------------------------
' Somewhere to send SCI_RELEASEDOCUMENT. It is a MESSAGE, so releasing a document needs a
' live Scintilla window -- destroy every window first and the references can never be given
' back. The scratch view is the designated messenger and is destroyed LAST.
' ----------------------------------------------------------------------------------------
declare function FindProject_Messenger() as HWND
' Unbind any excerpt view pointed at this document. Installed by the surface for the same
' include-order reason as gFipModelChanged; null until then.
type FIP_UnbindDocSub as sub( byval pSciDoc as any ptr )
dim shared gFipUnbindDoc as FIP_UnbindDocSub

' A document is about to lose its Scintilla windows -- closed, or reloaded from disk. Its
' results cannot outlive it: the excerpt views have nothing to bind to, and every byte offset
' refers to a buffer that is going away. Called from clsDocument.DestroyScintillaWindows,
' which is the ONLY choke point that covers all three routes (RemoveDocument,
' RemoveAllDocuments -- which does not call the first -- and ReloadDocument, which the file
' watcher drives with no user action at all).
declare sub      FindProject_OnDocumentClosing( byval pDocGone as clsDocument ptr )

' A result file has just been written to disk: re-take its line snapshots, so the dirty
' stripe means "changed since the last save" rather than "changed since the search ran".
' Called from clsDocument.SaveFile, the one place every save route converges on.
declare sub      FindProject_OnDocumentSaved( byval pDocSaved as clsDocument ptr )


' Run a search over every document. Returns the number of matches found.
declare function FindProject_Search( byval wszPhrase as DWSTRING, _
                                     byval bMatchCase as boolean, _
                                     byval bWholeWord as boolean ) as long
' Drop every result. Safe to call repeatedly.
declare sub      FindProject_Clear()
' Destroy the scratch view. Call once on shutdown.
declare sub      FindProject_Shutdown()

' ----------------------------------------------------------------------------------------
' PURE helpers. No window, no document, no message pump -- which is what lets the self-test
' cover them headlessly, and is the reason the remapping is done explicitly here rather than
' being left to a Scintilla indicator's own tracking.
' ----------------------------------------------------------------------------------------
' The context window for a matched line, clamped to the document. nLast is INCLUSIVE.
declare sub      FindProject_LineWindow( byval nLine as long, byval nContext as long, _
                                         byval nLineCount as long, _
                                         byref nFirst as long, byref nLast as long )
' Do two windows belong in one excerpt? True when they overlap, abut, or are separated by a
' single line -- one hidden line is cheaper to show than to draw a separator over. A two-line
' gap keeps them apart.
declare function FindProject_ShouldMerge( byval nPrevLast as long, byval nNextFirst as long ) as boolean
' Move one match across one document edit. Returns FALSE when the edit destroyed it.
declare function FindProject_RemapMatch( byval nStart as long, byval nLen as long, _
                                         byval nModType as long, _
                                         byval nModPos as long, byval nModLen as long, _
                                         byref nNewStart as long, byref nNewLen as long ) as boolean

' Apply one SCN_MODIFIED to every match in the edited document. Called from the TOP of
' frmMain_OnNotify -- above its active-document gate, or it would never run while the Find in
' Project tab is the active tab, which is the situation it exists for.
declare sub      FindProject_OnDocumentModified( byval pDocMod as clsDocument ptr, _
                                                 byval nModType as long, _
                                                 byval nModPos as long, byval nModLen as long, _
                                                 byval nLinesAdded as long )

' Indicator used to mark project-search hits inside the excerpt views. 0-7 belong to the
' lexer, 8 is the Find bar's highlight, 9 brace matching, 10 occurrence highlight.
#define FINDPROJ_INDICATOR  11

' Is any file in the results modified? Drives the circle on the tab.
declare function FindProject_AnyDirty() as boolean
' Save them all. FALSE when the user cancelled one, which aborts the close that asked.
declare function FindProject_SaveAllDirty() as boolean
' Has this line of this block been changed since the search ran? Compared against the text
' captured at search time.
'
' A SCINTILLA MARKER WAS TRIED FIRST AND IS WRONG FOR THIS. A marker tracks its line as text
' moves, which is genuinely useful -- but it is set on an edit and nothing takes it off again,
' so undoing a change left the stripe behind. Comparing the text answers the question that is
' actually being asked ("is this line different from what was found?") rather than "was this
' line ever touched", and it answers it correctly after an undo.
declare function FindProject_IsLineDirty( byval nBlock as long, byval nLineInBlock as long ) as boolean

' Paint (or clear) the match indicator over a group's document. Indicator RANGES live on the
' document, so this runs once per file -- but the indicator's COLOUR is per-view, which is why
' FipPool_Bind styles each excerpt view separately.
declare sub      FindProject_ApplyIndicators( byval nGroup as long )
declare sub      FindProject_ClearIndicators( byval nGroup as long )

' Replace every live match in the project. Returns how many were replaced. Files are left
' DIRTY -- nothing is written to disk.
declare function FindProject_ReplaceAll( byval wszReplace as DWSTRING ) as long
' Replace one match by its flat index. Returns TRUE if it was replaced.
declare function FindProject_ReplaceOne( byval nMatch as long, byval wszReplace as DWSTRING ) as boolean
' Next/previous LIVE match after (before) nFrom, wrapping. -1 when there are none.
declare function FindProject_NextLive( byval nFrom as long, byval bForward as boolean ) as long
' Which block holds this match? -1 if none.
declare function FindProject_BlockOfMatch( byval nMatch as long ) as long

declare sub      FindProject_RunSelfTest()
