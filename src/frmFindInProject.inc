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

#include once "frmFindInProject.bi"


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmFindInProject
' ========================================================================================
function frmFindInProject_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify As UINT _
            ) as LRESULT

    select case id
        case IDOK  ' Find 
            if codeNotify = BN_CLICKED then
'                EnableWindow(hwndCtl, false)
'                frmFindInFiles_DoFindInFilesEx( FindOnly )
'                EnableWindow(hwndCtl, true)
'                exit function
            end if  

        case IDCANCEL  ' Close
            if codeNotify = BN_CLICKED then
'                SendMessage( HWnd, WM_CLOSE, 0, 0)
'                exit function
            end if

    end select

    function = 0
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmFindInProject
' ========================================================================================
function frmFindInProject_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
  '  DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmFindInProject
' ========================================================================================
function frmFindInProject_OnDestroy( byval hwnd as HWND ) As LRESULT 
    
    HWND_FRMFINDINFILES = 0

 '   PostQuitMessage(0)

    function = 0
end function


' ========================================================================================
' frmFindInProject Window procedure
' ========================================================================================
function frmFindInProject_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CLOSE,    frmFindInProject_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmFindInProject_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmFindInProject_OnCommand)
    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmFindInProject_Show
' ========================================================================================
function frmFindInProject_Show( byval hWndParent as HWND ) as LRESULT
    '  Create the main window and child controls
'    dim pWindow as CWindow ptr = new CWindow

'    HWND_FRMFINDINPROJECT = _
'    pWindow->Create(hwndParent, L(256,"Find In Files"), @frmFindInProject_WndProc, 0, 0, 0, 0, _
'        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
'        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
'    pWindow->SetClientSize(515, 247)
'    pWindow->Center
    
    ' Create a fake pDoc so that it can be added to the top tab control
    dim as clsDocument ptr pDoc = null
    
    ' Add the new document to the top tabcontrol
    dim as long iTab = gTTabCtl.AddTab( pDoc )  

    gTTabCtl.tabs(iTab).docType = DocumentType.ProjectSearch
    gTTabCtl.tabs(iTab).wszText = L(256,"Find in Project")

    gTTabCtl.SetFocusTab(iTab)
    frmMain_PositionWindows()

    function = 0
end function

