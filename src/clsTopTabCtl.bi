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

#include once "PsTabBar.bi"

' document types (not to be confused with FileType)
' Used to distinguish normal code editing documents from Project Search
enum DocumentType
    Normal
    ProjectSearch
end enum


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

