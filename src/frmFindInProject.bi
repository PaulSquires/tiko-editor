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

' Creates the window on first use, adds (or re-selects) the tab, and focuses the Find bar.
declare function frmFindInProject_Show() as LRESULT
' Builds the window without touching the tab bar. Show calls this; nothing else needs to.
declare function frmFindInProject_CreateWindow() as HWND
' Lays the surface out inside the rect frmMain_PositionWindows hands it, in frmMain client
' coordinates -- the same rect a document's Scintilla windows would have received.
declare sub      frmFindInProject_PositionWindows( byval rcDoc as RECT )
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
