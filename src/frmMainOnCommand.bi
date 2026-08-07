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

enum eFileClose
    EFC_CLOSECURRENT
    EFC_CLOSEALL
    EFC_CLOSEALLFORWARD
    EFC_CLOSEALLOTHERS
    EFC_CLOSEALLBACKWARD 
end enum

declare function OnCommand_FileNew( byval hwnd as HWND ) as clsDocument ptr
declare function OnCommand_FileOpen( byval hwnd as HWND, byval bShowInTab as boolean = true, byval bIsTemplate as boolean = false ) as LRESULT
declare function OnCommand_FileSave( byval hwnd as HWND, byval pDoc as clsDocument ptr, _
        byval bSaveAs as boolean = false, byval bSaveAll as boolean = false ) as boolean
declare function OnCommand_FileRename( byval hwnd as HWND, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_FileDuplicate( byval hwnd as HWND, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_FileSaveDeclares( byval hwnd as HWND ) as LRESULT
declare function OnCommand_FileSaveAll( byval hwnd as HWND ) as boolean
declare function OnCommand_FileClose( byval hwnd as HWND, byval veFileClose as eFileClose, byval nTabNum as long = -1 ) as LRESULT
declare function OnCommand_FileExplorerMessage( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT

declare function OnCommand_EditRedo( byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditUndo( byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditCut( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditCopy( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditPaste( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditIndentBlock( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditUnIndentBlock( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditSelectAll( byval pDoc as clsDocument ptr, byval hEdit as HWND ) as LRESULT
declare function OnCommand_EditEncoding( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_EditCommon( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT

declare function OnCommand_SearchSymbol() as LRESULT
declare function OnCommand_SearchFindDialog() as LRESULT
declare function OnCommand_SearchReplaceDialog() as LRESULT
declare function OnCommand_SearchFindInProject() as LRESULT
declare function OnCommand_SearchFindActions( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_SearchReplaceActions( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_SearchGotoDefinition( byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_SearchGotoCompileError( byval bMoveNext as boolean ) as long
declare function OnCommand_SearchGotoFile( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_SearchBookmarks( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT

declare function OnCommand_ViewSidePanel() as LRESULT
declare function OnCommand_ViewFunctionList() as LRESULT
declare function OnCommand_ViewBookmarksList() as LRESULT
declare function OnCommand_ViewExplorer() as LRESULT
declare function OnCommand_ViewOutput() as LRESULT
declare function OnCommand_ViewFold( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_ViewZoom( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_ViewSplit( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_ViewToggleFocus( byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_ViewToDo() as LRESULT
declare function OnCommand_ViewNotes() as LRESULT
declare function OnCommand_ViewRestoreMain() as LRESULT
declare function OnCommand_ViewExplorerPosition() as LRESULT

declare function OnCommand_ProjectSave( byval HWnd as HWnd, byval bSaveAs as BOOLEAN = False ) as LRESULT
declare function Workspace_DeriveFileType( byval wszExt as DWSTRING, byval bHasMain as boolean ) as DWSTRING
declare function Workspace_EstablishUntitled( byval bRestoreFromFile as boolean = false ) as boolean
declare sub Workspace_RunSelfTest()
declare function Workspace_SaveAndCloseAll( byval hwnd as HWND ) as boolean
declare function OnCommand_ProjectClose( byval hwnd as HWND ) as LRESULT
declare function OnCommand_ProjectNew( byval hwnd as HWND ) as LRESULT
declare function OnCommand_ProjectOpen( byval hwnd as HWND ) as LRESULT
declare function OnCommand_ProjectFilesAdd( byval hwnd as HWND ) as LRESULT
declare function OnCommand_ProjectSetFileType( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT
declare function OnCommand_ProjectRemove( byval id as long, byval pDoc as clsDocument ptr ) as LRESULT

declare function OnCommand_CompileCommon( byval id as long ) as LRESULT
declare function OnCommand_DebugCommon( byval id as long ) as LRESULT

declare function frmMain_OnCommand( byval hwnd as HWND, _
                                    byval id as Long, _
                                    byval hwndCtl as HWND, _
                                    byval codeNotify as UINT _
                                    ) as LRESULT
