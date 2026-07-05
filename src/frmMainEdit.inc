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
        case GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), _
            GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), _ 
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND), _
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
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
        case GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), _
            GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), _ 
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND), _
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
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
        case GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), _
            GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), _ 
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND), _
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
            SendMessage( GetFocus, WM_PASTE, 0, 0 )
        case else     
            SciExec( GetFocus(), Iif(GetFocus()=hEdit, SCI_PASTE, WM_PASTE), 0, 0)
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

    ' If a TAB was pressed then manually terminate any active autocomplete
    ' and eat the TAB character.
    if SciExec(hEdit, SCI_AUTOCACTIVE, 0, 0) then
        SciExec(hEdit, SCI_AUTOCCOMPLETE, 0, 0)
        return 0 
    end if
    if GetFocus = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND) then
        if gFind.bExpanded then SetFocus GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
    elseif GetFocus = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE) then
        if gFind.bExpanded then SetFocus GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND)
    elseif AfxGetFormHandle(GetFocus) = HWND_FRMFINDINFILES then
        Dim As HWnd hCtrl = GetNextDlgTabItem( HWND_FRMFINDINFILES, GetFocus, false )
        SetFocus(hCtrl)
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
    
    if GetFocus = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND) then
        if gFind.bExpanded then SetFocus GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
    elseif GetFocus = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE) then
        if gFind.bExpanded then SetFocus GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND)
    elseif AfxGetFormHandle(GetFocus) = HWND_FRMFINDINFILES then
        Dim As HWND hCtrl = GetNextDlgTabItem( HWND_FRMFINDINFILES, GetFocus, true )
        SetFocus(hCtrl)
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
        case GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), _
            GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), _ 
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND), _
            GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE)
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


    select case id      
        case IDM_ANSI
            if pDoc->FileEncoding <> FILE_ENCODING_ANSI THEN 
                ' Convert buffer to ANSI and redisplay the text in the editor
                ConvertTextBuffer(pDoc, FILE_ENCODING_ANSI)
                pDoc->UserModified = true
            end if   
            pDoc->FileEncoding = FILE_ENCODING_ANSI 
      
        case IDM_UTF8
            if pDoc->FileEncoding <> FILE_ENCODING_UTF8 THEN 
                ConvertTextBuffer(pDoc, FILE_ENCODING_UTF8)
                pDoc->UserModified = true
            end if   
            pDoc->FileEncoding = FILE_ENCODING_UTF8
 
        case IDM_UTF8BOM
            if pDoc->FileEncoding <> FILE_ENCODING_UTF8_BOM THEN 
                ConvertTextBuffer(pDoc, FILE_ENCODING_UTF8_BOM)
                pDoc->UserModified = true
            end if   
            pDoc->FileEncoding = FILE_ENCODING_UTF8_BOM 
      
        case IDM_UTF16BOM
            if pDoc->FileEncoding <> FILE_ENCODING_UTF16_BOM THEN 
                ConvertTextBuffer(pDoc, FILE_ENCODING_UTF16_BOM)
                pDoc->UserModified = true
            end if   
            pDoc->FileEncoding = FILE_ENCODING_UTF16_BOM 
    end select
   
    frmMain_SetStatusbar

    function = 0
end function


' ========================================================================================
' Edit action Find
' ========================================================================================
function OnCommand_EditFindDialog() as LRESULT
    FindControls_Show()
    frmMain_PositionWindows()
    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND )
    SetFocus( hCtl )
    function = 0
end function

' ========================================================================================
' Edit action Replace
' ========================================================================================
function OnCommand_EditReplaceDialog() as LRESULT
    ' If Replace is already open then close it (toggle).
    gFind.bExpanded = not gFind.bExpanded
    if gFind.bExpanded = false then
        gTTabCtl.InfoPanelHeight = TOPTABS_INFO_HEIGHT_FIND
    else
        FindControls_Show()
        ReplaceControls_Show()
    end if    
    frmMain_PositionWindows()
    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND )
    SetFocus( hCtl )
    function = 0
end function

' ========================================================================================
' Edit action Find in Project
' ========================================================================================
function OnCommand_EditFindInProject( byval hEdit as HWND ) as LRESULT
    if hEdit = 0 then hEdit = HWND_FRMMAIN   ' possible no files are open
    frmFindInProject_Show( hEdit )
    'frmFindInFiles_Show( hEdit )   ' todo: Remove this frmFindInFiles_Show call when Project Search completed
    function = 0
end function
        
' ========================================================================================
' Edit action Find actions
' ========================================================================================
function OnCommand_EditFindActions( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as HWND hWndFind = GetDlgItem(HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND)
    dim as HWND hFocus = GetFocus()

    if hFocus = hWndFind then
        gFind.txtFind = AfxGetWindowText(hWndFind)
    elseif hFocus = hEdit then    
        dim as string sFindText = pDoc->GetSelText
        if len(sFindText) then 
            gFind.foundCount = 0
            gFind.txtFind = sFindText
        end if
    end if

    dim as long startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0) 

    select case id
        case IDM_FINDPREV
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, false, true) then pDoc->CenterCurrentLine
            AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )

        case IDM_FINDNEXT
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine
            AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )

        case IDM_FINDPREVACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, false, true) then pDoc->CenterCurrentLine

        case IDM_FINDNEXTACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine

    end select
    
    SetFocus( hFocus )
    
    function = 0
end function


' ========================================================================================
' Edit action Replace actions
' ========================================================================================
function OnCommand_EditReplaceActions( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
            
    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        case IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
'            if (GetForeGroundWindow() = HWND_FRMMAIN_TOPTABSINFO) then
                if gFind.foundCount = 0 then exit function
                if id = IDM_REPLACEPREV then 
                    FindReplace_DoReplace( false, false )
                elseif id = IDM_REPLACENEXT then 
                    FindReplace_DoReplace( false, true )
                elseif id = IDM_REPLACEALL then 
                    FindReplace_DoReplace( true, true )
                end if   
                FindReplace_HighlightSearches()
                pDoc->CenterCurrentLine
                AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )
'            end if
    end select
    
    function = 0
end function


' ========================================================================================
' Edit action Common actions
' ========================================================================================
function OnCommand_EditCommon( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
        
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



