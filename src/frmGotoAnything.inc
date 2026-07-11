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
    DestroyWindow hwnd
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmGotoAnything
' ========================================================================================
function frmGotoAnything_OnDestroy( byval hwnd as HWND ) As LRESULT 
    PostQuitMessage(0)
    function = 0
end function


' ========================================================================================
' frmGotoAnything_RichEdit_SubclassProc Window procedure
' ========================================================================================
Function frmGotoAnything_RichEdit_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)

    select case uMsg
                            
        case WM_PRINTCLIENT, WM_PAINT
            dim as long textLength = AfxGetWindowTextLength(hWin)
            if (textLength = 0 ) then 
                ' Get the needed DC with DCX_INTERSECTUPDATE before the EDIT
                ' control's WM_PAINT handler calls BeginPaint/EndPaint, which
                ' validates the update rect and would otherwise lead to drawing
                ' nothing later because the region is empty. Also, grab it from
                ' the cache so we don't mess with the EDIT's DC.
                dim as HDC hdc = _
                    iif(uMsg = WM_PRINTCLIENT, GetDC(hWin), _
                        GetDCEx(hWin, 0, DCX_INTERSECTUPDATE or DCX_CACHE or DCX_CLIPCHILDREN or DCX_CLIPSIBLINGS) )

                ' Call the EDIT control so that the caret is properly handled,
                ' no caret litter left on the screen after tabbing away.
                dim as LRESULT result = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

                SaveDC(hdc)
                
                ' Get the font and margin so the cue banner text has a
                ' consistent appearance and placement with existing text.
                dim as RECT editRect: SendMessage( hWin, EM_GETRECT, 0, cast(LPARAM, @editRect) )

                ' Ideally we would call Edit_GetCueBannerText, but since that message
                ' returns nothing when ES_MULTILINE, use a window property instead.
                dim cueBannerText as wstring * 250
                cueBannerText = L(158,"Find") & "..."
                SelectObject( hdc, gFind.hCueBannerFont )
                SetTextColor( hdc, ghTopTabs.ForeColor )
                SetBkMode(hdc, TRANSPARENT)
                dim as long wsStyle = DT_TOP or DT_LEFT or DT_NOPREFIX or DT_NOCLIP or DT_SINGLELINE 
                DrawText(hdc, cueBannerText, -1, @editRect, wsStyle)
                RestoreDC( hdc, -1 )
                ReleaseDC( hWin, hdc )

                ' Return the EDIT's result (could probably safely just return zero here,
                ' but seems safer to relay whatever value came from the edit).
                return result
            end if
        
        case WM_CONTEXTMENU 
            ' Create the right click popup menu
            dim as DWSTRING wszText = RichEdit_GetSelText( hWin )
            dim hPopUpMenu As HMENU = CreatePopupMenu()
            if len(wszText) then 
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_CUT, wstr("Cut") )
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_COPY, wstr("Copy") )
            end if
            if RichEdit_CanPaste( hWin, 0 ) then
                if len(wszText) then 
                    AppendMenu( hPopUpMenu, MF_SEPARATOR, 0, "" )
                end if   
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_PASTE, wstr("Paste") )
            end if   

            dim as long nResult
            nResult = TrackPopupMenu( hPopUpMenu, TPM_RETURNCMD or TPM_NONOTIFY, _
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMMAIN_TOPTABSFIND, 0 ) 
            select case nResult
                case IDM_CUT:   SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:  SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE: SendMessage( hWin, WM_PASTE, 0, 0 )
            end select
            DestroyMenu hPopUpMenu

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmGotoAnything_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

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
        if pWindow then delete(pWindow)
        HWND_FRMGOTOANYTHING = 0
        return 0

    case WM_LBUTTONDOWN, _
         WM_RBUTTONDOWN
        dim as RECT rc: GetClientRect(hwnd, @rc)
        if isMouseOverRECT( hwnd, rc ) = false then
            ' Click was outside the child's own bounds
            ReleaseCapture()
            SendMessage( hwnd, WM_CLOSE, 0, 0 )
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
function frmGotoAnything_Show( byval hwndParent as HWND, byval idGotoMode as long ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow
    
    HWND_FRMGOTOANYTHING = _
    pWindow->Create( hwndParent, "", @frmGotoAnything_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, WS_EX_NOACTIVATE or WS_EX_TOOLWINDOW )

    ' we use RichEdit controls because regular Edit controls can not easily have
    ' their text vertically centered.
    if gFind.hCueBannerFont = 0 then 
        gFind.hCueBannerFont = ghFont(INFOFONT_11)
    end if

    dim as RECT rc
    dim cf as CHARFORMATW 
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_FACE or CFM_SIZE or CFM_CHARSET or CFM_COLOR
    cf.crTextColor = ghTopTabs.ForeColorHot
    cf.yHeight = 11 * 20      ' in twips (1/20 pt)
    cf.bCharSet = DEFAULT_CHARSET
    cf.bPitchAndFamily = DEFAULT_PITCH or FF_DONTCARE
    cf.szFaceName = GUIFIXEDFONT
    
    dim as HWND hCtlFind
    
    hCtlFind = pWindow->AddControl("RICHEDIT", , IDC_FRMGOTOANYTHING_TXTFIND, "", _
                0, 0, 0, 0, _
                WS_CHILD or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
                WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtlFind, @frmGotoAnything_RichEdit_SubclassProc, IDC_FRMTOPTABSFIND_TXTFIND, NULL)
    SendMessage( hCtlFind, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtlFind, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghTopTabs.BackColorHot) )


    ' If there is no active underlying document then exit out.
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return 0
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla
   
    ' Get the client area of the main window and subtract width of any visible side panel. We
    ' use this width in order to center the popup fuzzy finder.
    GetWindowRect( hEdit, @rc )
    MapWindowpoints( HWND_DESKTOP, HWND_FRMMAIN, cast( point ptr, @rc), 2 )
    
    ' Calculate the position for this popup fuzzy finder
    dim as long nWidth   = pWindow->ScaleX( 400 )
    dim as long nHeight  = pWindow->ScaleY( 600 )
    dim as long nTop     = rc.top
    dim as long nLeft    = rc.left + ( (rc.right - rc.left - nWidth) / 2 )
    
    
'    ' Reposition the find textbox
'    dim as HWND hCtl = GetDlgItem( HWND_FRMGOTOANYTHING, IDC_FRMGOTOANYTHING_TXTFIND )
'    nHeight = pWindow->ScaleY( 16 )


    ShowWindow( HWND_FRMGOTOANYTHING, SW_SHOWNOACTIVATE )
    SetWindowPos( HWND_FRMGOTOANYTHING, HWND_TOPMOST, nLeft, nTop, nWidth, nHeight, SWP_SHOWWINDOW or SWP_NOACTIVATE ) 
    SetCapture( HWND_FRMGOTOANYTHING )

    ' Message loop (modal)
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMGOTOANYTHING, WM_CLOSE, 0, 0 )
        end if
        if IsDialogMessage( HWND_FRMGOTOANYTHING, @uMsg ) = 0 THEN
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    WEND
    
    ReleaseCapture()
    
    function = uMsg.wParam
end function
