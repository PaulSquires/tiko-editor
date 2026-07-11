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

#include once "frmGotoAnything.bi"


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmGotoAnything
' ========================================================================================
function frmGotoAnything_OnCommand( _
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
' Process WM_CLOSE message for window/dialog: frmGotoAnything
' ========================================================================================
function frmGotoAnything_OnClose( byval hwnd as HWND ) as LRESULT
    ' Enables parent window keeping parent's zorder
    'DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmGotoAnything
' ========================================================================================
function frmGotoAnything_OnDestroy( byval hwnd as HWND ) As LRESULT 
    
'   PostQuitMessage(0)

    function = 0
end function


' ========================================================================================
' frmGotoAnything Window procedure
' ========================================================================================
function frmGotoAnything_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CLOSE,    frmGotoAnything_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,  frmGotoAnything_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,  frmGotoAnything_OnCommand)

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMGOTOANYTHING = 0

    case WM_LBUTTONDOWN, _
         WM_RBUTTONDOWN
        dim as POINT pt = ( GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) )
        dim as RECT rc: GetClientRect(hwnd, @rc)

        if ( not PtInRect(@rc, pt) ) then
            ' Click was outside the child's own bounds
            ReleaseCapture()
            ShowWindow(hwnd, SW_HIDE)
            return 0
        end if
        ' else: click was inside — handle normally, capture stays active

    case WM_CAPTURECHANGED
        ' Capture was taken away by something else (e.g. another window called SetCapture,
        ' or the user alt-tabbed) — treat as "dismiss" too
        ShowWindow( hwnd, SW_HIDE )
    
    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmGotoAnything_Show
' ========================================================================================
function frmGotoAnything_Show( byval hwndParent as HWND ) as LRESULT

    dim pWindow as CPanelWindow ptr = new CPanelWindow
    
    HWND_FRMGOTOANYTHING = _
    pWindow->Create( hwndParent, @frmGotoAnything_WndProc, 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, 0 )

    function = 0
end function


' ========================================================================================
' Display the actual fuzzy finder form.
' ========================================================================================
function ShowGotoAnything( byval idGotoMode as long ) as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMGOTOANYTHING)
    if pWindow = 0 then delete(pWindow)

    ' If there is no active underlying document then exit out.
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla
   
    ' Get the client area of the main window and subtract width of any visible side panel. We
    ' use this width in order to center the popup fuzzy finder.
    dim as RECT rc: GetClientRect( hEdit, @rc )
    
    ' Calculate the position for this popup fuzzy finder
    dim as long nWidth   = pWindow->ScaleX( 400 )
    dim as long nHeight  = pWindow->ScaleY( 600 )
    dim as long nTop     = rc.top
    dim as long nLeft    = rc.left + ( (rc.right - nWidth) / 2 )
    
    ' Show the child window and bring it to the front of the zorder
    SetWindowPos( HWND_FRMGOTOANYTHING, HWND_TOP, nLeft, nTop, nWidth, nHeight, SWP_SHOWWINDOW or SWP_NOACTIVATE ) 
    SetCapture( HWND_FRMGOTOANYTHING )

    return 0
end function
