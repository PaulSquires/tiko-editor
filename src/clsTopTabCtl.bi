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


' Forward reference
type clsDocument_ as clsDocument

type TOPTABS_TYPE
    pDoc    as clsDocument_ ptr
    wszText as DWSTRING
    rcTab   as RECT          ' client coordinates 
    rcIcon  as RECT          ' client coordinates 
    rcText  as RECT          ' client coordinates 
    rcClose as RECT          ' client coordinates 
    isHot   as boolean
end type


type clsTopTabCtl
    private:
        
    public:
        hWindow           as HWND
        ClientRightEdge   as long        ' the right edge (client right - action Panel)
        CurSel            as long = -1
        FirstDisplayTab   as long = 0   
        LastDisplayTab    as long = 0    ' for positionign any "New File" from clicking + button
        rcAddFileButton   as RECT
        rcActionPanel     as RECT
        rcActionButton    as RECT
        rcPrevTabs        as RECT
        rcNextTabs        as RECT
        rcSplitEditor     as RECT
        rcDiskFilename    as RECT
        rcFindButton      as RECT
        rcFileTypeButton  as RECT        ' Main, Modulesm Normal, etc
        rcFindTextRect    as RECT
        rcReplaceTextRect as RECT
        wszFileType       as DWSTRING
        InfoPanelHeight   as long = TOPTABS_INFO_HEIGHT
            ' TOPTABS_INFO_HEIGHT, TOPTABS_INFO_HEIGHT_FIND, TOPTABS_INFO_HEIGHT_REPLACE

        tabs(any)         as TOPTABS_TYPE
        
        declare function IsSafeIndex( byval idx as long ) as boolean
        declare function GetItemCount() as long
        declare function RemoveElement( byval idx as long ) as long
        declare function AddTab( byval pDoc as clsDocument Ptr ) as long
        declare function InsertTab( byval pDoc as clsDocument ptr, byval insertIdx as long ) as long
        declare function GetTabIndexFromFilename( byref wszName as wstring ) as long
        declare function GetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
        declare function SetTabIndexByDocumentPtr( byval pDocIn as clsDocument ptr ) as long
        declare function SetFocusTab( byval idx as long ) as long
        declare function GetActiveDocumentPtr() as clsDocument ptr
        declare function GetDocumentPtr( byval idx as long ) as clsDocument ptr
        declare function DisplayScintilla( byval idx as long, byval bShow as boolean ) as long
        declare function SetTabText( byval idx as long ) as long
        declare function NextTab() as long
        declare function PrevTab() as long
        declare function CloseTab( byval idx as long = -1 ) as long
        
End Type

