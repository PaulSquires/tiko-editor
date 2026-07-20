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

#include once "frmOptionsKeywords.bi"
#include once "frmOptionsKeywordsWinApi.bi"


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmOptionsKeywords
' ========================================================================================
function frmOptionsKeywords_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        case IDC_FRMOPTIONSKEYWORDS_TXTKEYWORDS
            if codeNotify = EN_CHANGE then 
                ' The EN_CHANGE notification code is not sent when the ES_MULTILINE style 
                ' is used and the text is sent through WM_SETTEXT. 
                ' Set the flag in gConfig to indicate that the keywords need to be saved 
                ' to disk should the user press OK to close the Environment Options dialog.
                gConfig.bKeywordsDirty = true
            end if
    end select

    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmOptionsKeywords
' ========================================================================================
function frmOptionsKeywords_OnDestroy( byval hwnd as HWND ) as LRESULT
    Dim as HFONT hFont = AfxGetWindowFont(GetDlgItem(hwnd, IDC_FRMOPTIONSKEYWORDS_TXTKEYWORDS))
    DeleteFont(hFont)
    function = 0
end function


' ========================================================================================
' Subclass the multiline textbox to catch the ESCAPE key that is normally eaten by
' the textbox control when in multiline mode.
' ========================================================================================
function frmOptionsKeywordsTextBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMEXPLORER)
        
    select case uMsg
        case WM_KEYDOWN
            if _wParam = VK_ESCAPE then
                ' Send the close message to the dialog window
                PostMessage( HWND_FRMOPTIONS, WM_COMMAND, MAKELONG(IDCANCEL, 0), 0 )
            end if
        
        Case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmOptionsKeywordsTextBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' frmOptionsKeywords Window procedure
' ========================================================================================
function frmOptionsKeywords_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_COMMAND, frmOptionsKeywords_OnCommand)
        HANDLE_MSG (hwnd, WM_DESTROY, frmOptionsKeywords_OnDestroy)
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmOptionsKeywords_Show
' ========================================================================================
function frmOptionsKeywords_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMOPTIONSKEYWORDS = pWindow->Create( hwndParent, "", @frmOptionsKeywords_WndProc, 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING )
    ' Height and width of this child form is set in frmOptions_OnNotify when the treeview option is selected.
    
    dim as HWND hTextBox = _
        pWindow->AddControl("TEXTBOX", , IDC_FRMOPTIONSKEYWORDS_TXTKEYWORDS, "", 0, 0, 386, 345, _
        WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_TABSTOP or ES_LEFT or _
        ES_MULTILINE or ES_AUTOVSCROLL or ES_WANTRETURN, _
        WS_EX_CLIENTEDGE or WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->SetControlSubclass(hTextBox, @frmOptionsKeywordsTextBox_SubclassProc, IDC_FRMOPTIONSKEYWORDS_TXTKEYWORDS, @pWindow)        
    
    SendMessage( hTextBox, EM_SETLIMITTEXT, (1024 * 100), 0 )    ' max 100K text   
    
    dim as HFONT hFont = pWindow->CreateFont("Courier New", 9)
    AfxSetWindowFont( hTextBox, hFont, true )

    gConfig.bKeywordsDirty = false
    AfxSetWindowText( hTextBox, wstr(gConfig.FBKeywords) )
    
    function = 0
    
end function

