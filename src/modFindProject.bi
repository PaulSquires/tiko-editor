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
    nGroupCount  as long
    nMatchCount  as long            ' entries in matches(), dead ones included
    nBlockCount  as long
    nLiveMatches as long            ' matches still alive; this is what the n/m count reports
    wszPhrase    as DWSTRING
    bMatchCase   as boolean
    bWholeWord   as boolean
    bValid       as boolean         ' a search has been run
end type
dim shared gFip as FINDPROJ_MODEL

' Set while a replace loop is rewriting the model itself, so the SCN_MODIFIED hook does not
' also try to remap the same edit. (Used from Phase 6's project-wide replace.)
dim shared gFipSuppressRemap as boolean


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

declare sub      FindProject_RunSelfTest()
