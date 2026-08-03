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
' Find in Project -- the results surface behind the special document-less tab.
'
' The tab itself is an ordinary PsTabBar tab carrying TAB_FINDINPROJECT in its itemData
' instead of a clsDocument ptr (see clsTopTabCtl.bi). This window fills the same client
' area a document's editors would, and frmMain_PositionWindows shows and sizes it on the
' pDoc = 0 branch.
'
' It does NOT persist between sessions: clsConfig's session and project writers already
' skip a tab with no document, so the tab is absent from the file for free.
'
' This is currently a plain background -- the virtualized results list, its scrollbar and
' the pooled Scintilla excerpt views come next.
' ========================================================================================

#define IDC_FRMFINDINPROJECT_SURFACE   1000
#define IDC_FRMFINDINPROJECT_VSCROLL   1001

' Control id BASE for the pooled excerpt views: slot n is IDC_SCINTILLA_EXCERPT + n.
' Deliberately far from IDC_SCINTILLA(+1), which clsDocument.IsValidScintillaID matches --
' that is a control-ID test and cannot otherwise tell a pooled view from a document's own.
#define IDC_SCINTILLA_EXCERPT        400
' Upper bound on live excerpt views. The pool only ever needs to cover the viewport plus a
' little, since a view is rebound as it scrolls rather than created.
#define FIP_POOL_MAX                  48

' Unscaled metrics. Everything is put through AfxScaleX/Y at layout time.
#define FIP_HEADER_HEIGHT   24      ' the filename separator bar
#define FIP_BLOCK_PAD        4      ' above and below an excerpt's lines
#define FIP_BLOCK_GAP        8      ' between one excerpt and the next
#define FIP_INDENT          10      ' excerpts inset from the left edge

' ----------------------------------------------------------------------------------------
' THE EXCERPT WINDOW IS A FIXED FIVE LINES TALL, AND THAT IS NOW INDEPENDENT OF ITS BLOCK.
'
' It used to be a PINNED window onto exactly its block's lines: the height was the block's
' line count, the view was re-scrolled to the block's first line after every message, and the
' caret was confined to those lines (Up on the first line and Down on the last were swallowed
' before Scintilla saw them). That made an excerpt un-navigable -- you could only edit the
' handful of lines the search happened to frame.
'
' The views SCROLL FREELY now. Each is a real second view onto the whole document, so the
' arrow keys, PgUp/PgDn and Ctrl+Home/End all work and take the view with them; the block is
' only where the view STARTS. What stays fixed is the height, so the list's geometry is
' still one number per row rather than a per-block measurement, and a file whose matches
' coalesced into a 40-line block does not get a 40-line window in the results list.
' ----------------------------------------------------------------------------------------
#define FIP_VIEW_LINES       5      ' document lines visible in one excerpt window

' ----------------------------------------------------------------------------------------
' THE VIRTUAL CANVAS.
'
' The surface is scrolled by a TOP PIXEL OFFSET into a canvas that is only ever described,
' never built as a window: at ~2000 matches the canvas is around 200,000px tall, and a page
' window that size is not something to hand to the window manager. Children are positioned in
' VIEWPORT coordinates and repositioned as they scroll -- deliberately not PsScrollPanel's
' move-one-giant-page approach, which is why this control is hand-built.
'
' One FIP_ROW per drawn thing, in canvas order, each carrying its own pixel top. That is what
' makes "which rows are visible at this scroll position" a binary search rather than a walk,
' and what makes the whole geometry assertable without a window.
' ----------------------------------------------------------------------------------------
#define FIP_ROW_HEADER  0
#define FIP_ROW_BLOCK   1

type FIP_ROW
    nKind   as long
    nGroup  as long
    nBlock  as long     ' -1 for a header row
    nTop    as long     ' pixel offset into the canvas
    nHeight as long
end type

type FIP_METRICS
    nLineHeight   as long   ' ONE document line, taken from Scintilla itself
    nHeaderHeight as long
    nBlockPad     as long
    nBlockGap     as long
    nIndent       as long
end type

' Build the row list for the current model. Returns the canvas height in pixels.
' (An array parameter takes no BYREF in FreeBASIC -- arrays are always passed by reference.)
declare function FipRows_Build( rows() as FIP_ROW, byref nRowCount as long, _
                                byref m as FIP_METRICS ) as long
' Index of the first row whose bottom edge is below nTop, or -1 when there is none. Pure, and
' takes the array so a synthetic one can be handed to it.
declare function FipRows_FirstVisible( rows() as FIP_ROW, byval nRowCount as long, _
                                       byval nTop as long ) as long
' Height of an excerpt window. EVERY block gets the same one -- see FIP_VIEW_LINES.
declare function FipRows_BlockHeight( byref m as FIP_METRICS ) as long

' Creates the window on first use, adds (or re-selects) the tab, and focuses the Find bar.
declare function frmFindInProject_Show() as LRESULT
' Builds the window without touching the tab bar. Show calls this; nothing else needs to.
declare function frmFindInProject_CreateWindow() as HWND
' Lays the surface out inside the rect frmMain_PositionWindows hands it, in frmMain client
' coordinates -- the same rect a document's Scintilla windows would have received.
declare sub      frmFindInProject_PositionWindows( byval rcDoc as RECT )
' Adopt the current gFip model: re-measure, rebuild the row list, reset the scroll and push
' the scrollbar range. Call after every search and after a collapse/expand.
declare sub      frmFindInProject_Refresh( byval bResetScroll as boolean = false )
' Re-style the excerpt views after a theme change. They are Scintilla views but are not in
' gApp.pDocList, so modThemeApply's document walk cannot reach them.
declare sub      frmFindInProject_ApplyTheme()
' The Find / Replace bar acted while this tab is current. Returns TRUE when it handled the
' command, so the caller's single-document path is skipped. This is the ONE interception
' point: Enter in the find box already funnels to IDM_FINDNEXT.
declare function frmFindInProject_BarAction( byval id as long ) as boolean
' The document Ctrl+S saves while this tab is up: the focused excerpt's, or the one that
' last held the caret. See the function header -- it is NOT the same test as ActiveEdit.
declare function frmFindInProject_SaveTarget( byref pDocOut as clsDocument ptr ) as boolean
' The topmost excerpt view on screen, in ROW order. This tab has no document Scintilla, so
' this is what the message pump tabs to past the Find/Replace bars. 0 when the tab is not
' active or the results list is empty.
declare function frmFindInProject_FirstExcerptWindow() as HWND
' (frmFindInProject_RestoreFocus is declared in frmTopTabs.bi -- clsTopTabCtl.inc calls it
'  and is compiled long before this file.)
' There is deliberately NO frmFindInProject_Close. Closing this tab goes through the ordinary
' OnCommand_FileClose path like every other tab -- the tab's X routes there via
' gTTabCtl.CloseTab, and shutdown via EFC_CLOSEALL -- and that loop owns destroying this
' window. A second door would have to duplicate the loop's own tab bookkeeping.
'
' True when the Find in Project tab is the ACTIVE tab. The Find bar and the message pump both
' branch on this, so it is worth having in one place rather than repeating the
' IsFindTab( GetCurSel ) pair. DECLARED IN frmTopTabs.bi, not here: frmFind.inc needs it and
' is included well before this file.
' Env-gated: TIKO_FINDPROJ_SELFTEST=1. Opens the tab, asserts, closes it again. Must run
' after the session has loaded, or the assertions about documents being hidden are vacuous.
declare sub      frmFindInProject_RunSelfTest()
' Env-gated: TIKO_FINDPROJ_REPRO=1. Drives the reported no-hits-then-corrected-phrase crash.
declare sub      frmFindInProject_RunRepro()

' Env-gated harness: TIKO_FINDPROJ_REPLACETEST=1. Drives Replace / Replace All, the Match
' Case and Whole Word toggles, and Ctrl+S against a TEMPORARY FILE OF ITS OWN, so the
' expected counts are known constants rather than whatever happens to be open.
declare sub      frmFindInProject_RunReplaceTest()

' Env-gated repro: TIKO_FINDPROJ_REOPEN=1. Search, close the tab, search again -- the second
' open used to GPF. Unreachable from the self-test, which never reopens the tab.
declare sub      frmFindInProject_RunReopen()
