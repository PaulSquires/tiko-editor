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
' Height of an excerpt showing nLineCount document lines.
declare function FipRows_BlockHeight( byval nLineCount as long, byref m as FIP_METRICS ) as long

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
' There is deliberately NO frmFindInProject_Close. Closing this tab goes through the ordinary
' OnCommand_FileClose path like every other tab -- the tab's X routes there via
' gTTabCtl.CloseTab, and shutdown via EFC_CLOSEALL -- and that loop owns destroying this
' window. A second door would have to duplicate the loop's own tab bookkeeping.
'
' True when the Find in Project tab is the ACTIVE tab. The Find bar and the message pump
' both branch on this, so it is worth having in one place rather than repeating the
' IsFindTab( GetCurSel ) pair.
declare function frmFindInProject_IsActive() as boolean
' Env-gated: TIKO_FINDPROJ_SELFTEST=1. Opens the tab, asserts, closes it again. Must run
' after the session has loaded, or the assertions about documents being hidden are vacuous.
declare sub      frmFindInProject_RunSelfTest()
