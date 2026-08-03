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

' What KIND of thing a row is, carried in the row's itemDataExtra. This replaces the old
' EXPLORER_PROMOTE_ROW itemData sentinel (a bare 1), which worked only while there was exactly
' one non-document row: itemData was "a clsDocument ptr, unless it happens to be 1", so a
' reader that cast and then tested "if pDoc then" passed the test and dereferenced address 1.
' That shipped as a GPF on hover. With more non-document row kinds arriving (root groups and
' user folders) the sentinel scheme does not extend at all -- a folder index and a document
' pointer are not distinguishable by value.
'
' So the kind is now declared per row and itemData is read ONLY through the accessor for that
' kind. What itemData means is a function of the kind and nothing else:
'
'   EXPKIND_ROOT     itemData = index into gConfig.Cat()
'   EXPKIND_FOLDER   itemData = index into the folder table
'   EXPKIND_FILE     itemData = clsDocument ptr
'   EXPKIND_PROMOTE  itemData = 0
'
' EXPKIND_NONE IS ZERO DELIBERATELY. itemDataExtra defaults to zero for any row added without
' one, so whatever sits at zero is what an un-tagged row silently claims -- and the safe thing
' to claim is "not a document". Putting EXPKIND_FILE at zero would restore the original bug in
' a new costume: an un-tagged row would present its itemData as a pointer.
enum EXPLORER_ROWKIND
    EXPKIND_NONE    = 0
    EXPKIND_ROOT    = 1
    EXPKIND_FOLDER  = 2
    EXPKIND_FILE    = 3
    EXPKIND_PROMOTE = 4
end enum


#define IDC_FRMEXPLORER_LISTBOX       1000

' The width of one indent column, in UNSCALED units.
'
' Every depth level costs one of these, and so does the chevron/document-glyph column that
' follows it. ONE constant because three places need the same number and they must not drift:
' the painter's own indent arithmetic, the twisty rect the click test reads, and -- less
' obviously -- PsListTree_SetIndentWidth, which is what positions the in-place EDITOR. The
' control computes the editor's x from its own tree indent, so a list that indents its rows
' by hand and never tells the control opens every rename hard against the left margin.
#define EXPLORER_INDENT_UNITS         20

' The kind of an Explorer row. Invalid rows read EXPKIND_NONE.
declare function frmExplorer_KindFromRow( byval hCtl as HWND, byval row as integer ) as EXPLORER_ROWKIND
' The document a row refers to, or NULL for a non-document row. EVERY read of an Explorer
' row's itemData must go through this -- see the note on its definition.
declare function frmExplorer_DocFromRow( byval hCtl as HWND, byval row as integer ) as clsDocument ptr
declare function frmExplorer_IsFileDisplayed( byval wszFilename as DWSTRING ) as boolean
' The gConfig.Cat() index a document is displayed under, or -1 if it is displayed nowhere.
declare function frmExplorer_CatIndexForDoc( byval pDoc as clsDocument ptr ) as long
' Drop any document folder that no longer names a real folder. See its definition.
declare sub frmExplorer_NormalizeDocFolders()
' Recursively emit one folder level (subfolders first, then files) beneath a tree row.
declare sub frmExplorer_AddFolderLevel( byval hCtrl as HWND, byval catIndex as long, _
                                        byval wszParentPath as DWSTRING, byval nParentRow as integer )
declare function frmExplorer_UnSelectListBox() as long
declare function frmExplorer_SelectItemData( byval pDoc as clsDocument ptr ) as boolean
declare function LoadExplorerFiles() as long
declare function frmExplorer_Show( byval hWndParent as HWND ) as LRESULT
declare sub frmExplorer_RunSelfTest()

' Which action icons a row offers, and where they sit. ONE implementation, called by both
' the painter and the hit test -- see its definition.
enum EXPLORER_ICONHIT
    EXPICON_NONE   = 0
    EXPICON_ADD    = 1
    EXPICON_DELETE = 2
    EXPICON_RENAME = 3
end enum

declare sub frmExplorer_IconRects( byval hCtl as HWND, byval row as integer, byref rcRow as RECT, _
                                   byref rcAdd as RECT, byref rcRen as RECT, byref rcDel as RECT )
declare sub frmExplorer_RenameFolder( byval hCtl as HWND, byval row as integer )
declare sub ShowExplorerFolderContextMenu( byval hCtl as HWND, byval row as integer )
declare function frmExplorer_FilterMessage( byval pMsg as MSG ptr ) as boolean
' May a file with this extension live under this root group? Pure, so assertable.

' THIS FILE MAY NAME NO Ps* TYPE. clsConfig.inc includes it, and clsConfig.inc is processed
' before PsListTree.bi has been seen -- so a declaration mentioning PSLISTTREE_DROPINFO here
' fails with "Illegal specification" at a line that looks perfectly correct. That is why
' frmExplorer_CanDropCallback is NOT declared here: it is defined in frmExplorer.inc ahead of
' the only thing that references it, so it needs no prototype at all.
declare function frmExplorer_IconHitTest( byval hCtl as HWND, byval row as integer, byval x as long ) as EXPLORER_ICONHIT
declare function frmExplorer_FolderPathFromRow( byval hCtl as HWND, byval row as integer, byref catIndex as long ) as DWSTRING
declare sub frmExplorer_TwistyRect( byval hCtl as HWND, byval row as integer, byref rcRow as RECT, byref rcOut as RECT )
declare function frmExplorer_BeginLabelEditCallback( byval hCtl as HWND, byval row as integer ) as boolean
declare function frmExplorer_EndLabelEditCallback( byval hCtl as HWND, byval row as integer, byval newText as DWSTRING ) as boolean
declare sub frmExplorer_NewFolder( byval hCtl as HWND, byval row as integer )
declare sub frmExplorer_DeleteFolder( byval hCtl as HWND, byval row as integer )
