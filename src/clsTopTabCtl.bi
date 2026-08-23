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

#include once "PsControls\PsTabBar.bi"

' A tab's per-tab payload is the single itemData integer PsTabBar carries, and it normally
' holds a clsDocument ptr. TAB_FINDINPROJECT is a SENTINEL stored in that same slot to mark
' the one tab that has no document: the Find in Project results tab. 1 is never a valid
' pointer.
'
' THE SENTINEL IS FILTERED INSIDE GetpDoc, and that is not optional. Roughly seventy call
' sites across the app are written as "if pDoc then <dereference>", so a sentinel that
' reached them would pass every one of those truth tests and dereference address 0x1 --
' crashing in the tab paint callback, in the message pump on every mouse message, and in
' SetTabText at tab creation. GetpDoc answers NULL for this tab; ask IsFindTab to tell a
' document-less tab apart from an invalid index.
'
' (This replaces an orphaned DocumentType enum -- Normal / ProjectSearch -- left behind when
' the per-tab TOPTABS_TYPE array that used to hold a docType field was deleted. The enum had
' no storage behind it and every one of its eleven use sites was commented out.)
const TAB_FINDINPROJECT = 1


' Forward reference
'type clsDocument_ as clsDocument

type clsTopTabCtl
    private:
        
    public:
        hWindow           as HWND
        rcDiskFilename    as RECT
        ' The Find / Goto Main / Goto Header buttons that used to live here are a PsIconPanel
        ' now (frmTopTabsInfo.inc) -- the control owns their cells, so there is nothing left
        ' for the host to store.
        rcFileTypeButton  as RECT        ' Main, Module, Normal, etc
        rcFindTextRect    as RECT
        rcReplaceTextRect as RECT
        wszFileType       as DWSTRING

        declare function GetItemCount() as long
        declare function GetCurSel() as integer
        declare function SetCurSel( byval idx as integer ) as boolean
        declare function IsValidTab( byval idx as integer ) as boolean
        declare function GetpDoc( byval idx as integer ) as clsDocument ptr
        ' Is this tab the document-less Find in Project tab? Reads the RAW itemData, so it is
        ' the only way to distinguish it from an invalid index (GetpDoc answers null for both).
        declare function IsFindTab( byval idx as integer ) as boolean
        ' Index of the Find in Project tab, or -1. There is at most one.
        declare function FindTabIndex() as long
        ' How many tabs actually hold a document. Menu enablers want THIS, not GetItemCount:
        ' the Find tab makes the count non-zero while GetActiveDocumentPtr stays null.
        declare function GetDocumentTabCount() as long
        ' The value the session/project writers should persist as "ActiveTab=". NOT the raw
        ' GetCurSel index -- see the implementation for why those differ.
        declare function SaveActiveTabIndex() as long
        declare function RemoveElement( byval idx as long ) as long
        declare function AddTab( byval pDoc as clsDocument Ptr ) as long
        declare function InsertTab( byval pDoc as clsDocument ptr, byval insertIdx as long ) as long
        declare function GetTabIndexFromFilename( byval wszName as DWSTRING ) as long
        declare function GetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
        declare function SetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
        declare function SetFocusTab( byval idx as long ) as long
        declare function GetActiveDocumentPtr() as clsDocument ptr
        declare function GetDocumentPtr( byval idx as long ) as clsDocument ptr
        declare function DisplayScintilla( byval idx as long, byval bShow as boolean ) as long
        declare function GetTabText( byval idx as long ) as DWSTRING
        declare function SetTabText( byval idx as long ) as long
        declare function TabFocusLost( byval nTabIdx as long = -1) as long
        declare function NextTab() as long
        declare function PrevTab() as long
        declare function CloseTab( byval idx as long = -1 ) as long
        
End Type

