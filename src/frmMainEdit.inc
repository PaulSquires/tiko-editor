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

#include once "frmMain.bi"
#include once "frmTopTabs.bi"
#include once "clsDocument.bi"


' ========================================================================================
' Edit action Cut
' ========================================================================================
function OnCommand_EditCut( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT
    
    ' Check if the Compiler log file or Notes windows or Snippets have
    ' input focus. If they do then select that text rather than Scintilla.
    select case GetFocus()
        case PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE))
            SendMessage( GetFocus, WM_CUT, 0, 0 )
        case else     
            SciExec( GetFocus(), iif(GetFocus()=hEdit, SCI_CUT, WM_CUT), 0, 0)
            frmMain_PositionWindows()    ' ensure custom scrollbars display
    end select

    function = 0
end function
        

' ========================================================================================
' Edit action Copy
' ========================================================================================
function OnCommand_EditCopy( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT
    
    ' Check if the Compiler log file or Notes windows or Snippets have
    ' input focus. If they do then select that text rather than Scintilla.
    select case GetFocus()
        case PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE))
            SendMessage( GetFocus, WM_COPY, 0, 0 )
        case else     
            SciExec( GetFocus(), Iif(GetFocus()=hEdit, SCI_COPY, WM_COPY), 0, 0)
    end select

    function = 0
end function


' ========================================================================================
' Edit action Paste
' ========================================================================================
function OnCommand_EditPaste( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT

    ' Check if the Compiler log file or Notes windows or Snippets have
    ' input focus. If they do then select that text rather than Scintilla.
    select case GetFocus()
        case PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)), _
             PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)), _
             PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)), _
             PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE))
            SendMessage( GetFocus, WM_PASTE, 0, 0 )
        case else     
            SciExec( GetFocus(), Iif(GetFocus()=hEdit, SCI_PASTE, WM_PASTE), 0, 0)
            ' Codetip may be active when pasting after a ".' or ">" so test to update code complete list.
            if gConfig.CodeTips then ShowAutocompleteList()
            frmMain_PositionWindows()    ' ensure custom scrollbars display
            SetFocus( hEdit )    ' ensure that the original edit window has not lost focus
    end select
    
    function = 0
end function


' ========================================================================================
' Edit action Indent Block
' ========================================================================================
function OnCommand_EditIndentBlock( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT

    ' (The "Tab commits the open autocomplete" case used to be handled here, because a
    '  Tab reached this command through the accelerator table before anything else saw
    '  it. frmAutoComplete_FilterMessage now consumes Tab ahead of TranslateAccelerator,
    '  so IDM_INDENTBLOCK cannot fire while the popup is up and there is nothing to
    '  special-case.)
    if PsTextBox_HasFocus( GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND) ) then
        if gFind.bShowReplacePanel then SetFocus GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE)
    elseif PsTextBox_HasFocus( GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE) ) then
        if gFind.bShowReplacePanel then SetFocus GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
    else
        gApp.SuppressNotify = true
        SetWindowRedraw( hEdit, false )
        Function = SciExec(hEdit, SCI_TAB, 0, 0)
        SetWindowRedraw( hEdit, true )
        gApp.SuppressNotify = false
        AfxRedrawWindow( hEdit )
        frmMain_SetStatusbar
    end if

    function = 0
end function


' ========================================================================================
' Edit action UnIndent Block
' ========================================================================================
function OnCommand_EditUnIndentBlock( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT
    
    if PsTextBox_HasFocus( GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND) ) then
        if gFind.bShowReplacePanel then SetFocus GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE)
    elseif PsTextBox_HasFocus( GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE) ) then
        if gFind.bShowReplacePanel then SetFocus GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
    else
        gApp.SuppressNotify = true
        SetWindowRedraw( hEdit, false )
        Function = SciExec(hEdit, SCI_BACKTAB, 0, 0)
        SetWindowRedraw( hEdit, true )
        gApp.SuppressNotify = false
        AfxRedrawWindow( hEdit )
        frmMain_SetStatusbar
    end if

    function = 0
end function


' ========================================================================================
' Edit action Select All
' ========================================================================================
function OnCommand_EditSelectAll( _
            byval pDoc as clsDocument ptr, _
            byval hEdit as HWND _
            ) as LRESULT

    ' Check if the Compiler log file or Notes windows or Snippets have
    ' input focus. If they do then select that text rather than Scintilla.
    select case GetFocus()
        case PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)), _
            PsTextBox_GetRichEditHandle(GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE))
            Edit_SetSel( GetFocus(), 0, -1 )
        case else     
            SciExec(hEdit, SCI_SELECTALL, 0, 0)
    end select
                                
    function = 0
end function


' ========================================================================================
' Edit action File Encoding
' ========================================================================================
function OnCommand_EditEncoding( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    ' Reachable with no active document -- an accelerator bypasses menu enablement.
    if pDoc = 0 then exit function

    ' Map the menu command to a target encoding. ConvertTextBuffer owns BOTH the byte
    ' conversion and the FileEncoding relabel (INVARIANT E1); the caller must not assign
    ' FileEncoding itself, nor override the function's decision -- a lossy conversion the
    ' user cancels must leave the document untouched.
    dim as long nEnc = -1
    select case id
        case IDM_ANSI     : nEnc = FILE_ENCODING_ANSI
        case IDM_UTF8     : nEnc = FILE_ENCODING_UTF8
        case IDM_UTF8BOM  : nEnc = FILE_ENCODING_UTF8_BOM
        case IDM_UTF16BOM : nEnc = FILE_ENCODING_UTF16_BOM
    end select

    if nEnc <> -1 andalso nEnc <> pDoc->FileEncoding then
        if ConvertTextBuffer( pDoc, nEnc ) then pDoc->UserModified = true
    end if

    frmMain_SetStatusbar

    function = 0
end function


' ========================================================================================
' Edit action Common actions
' ========================================================================================
function OnCommand_EditCommon( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    ' Reachable with no active document -- an accelerator bypasses menu enablement.
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        case IDM_REDO:                SciExec( hEdit, SCI_REDO, 0, 0)
        case IDM_UNDO, WM_UNDO:       SciExec( hEdit, SCI_UNDO, 0, 0)
        case WM_UNDO:                 SciExec( GetFocus(), WM_UNDO, 0, 0)
        case IDM_DELETELINE:          SciExec(hEdit, SCI_LINECUT, 0, 0)
        case IDM_INSERTFILE:          pDoc->InsertFile()
        case IDM_COMMENTBLOCK:        pDoc->BlockComment(true) 
        case IDM_UNCOMMENTBLOCK:      pDoc->BlockComment(false) 
        case IDM_DUPLICATELINE:       pDoc->LineDuplicate()
        case IDM_MOVELINEUP:          pDoc->MoveCurrentLines(false) 
        case IDM_MOVELINEDOWN:        pDoc->MoveCurrentLines(true)  
        case IDM_NEWLINEBELOWCURRENT: pDoc->NewLineBelowCurrent()  
        case IDM_TOUPPERCASE:         pDoc->ChangeSelectionCase(1)
        case IDM_TOLOWERCASE:         pDoc->ChangeSelectionCase(2)
        case IDM_TOMIXEDCASE:         pDoc->ChangeSelectionCase(3)
        case IDM_EOLTOCRLF:           pDoc->ConvertEOL(SC_EOL_CRLF)
        case IDM_EOLTOLF:             pDoc->ConvertEOL(SC_EOL_LF)
        case IDM_TABSTOSPACES:        pDoc->TabsToSpaces()
        case IDM_SELECTLINE:          pDoc->SelectLine(-1)
    end select
    
    function = 0
end function



