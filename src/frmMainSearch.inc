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
' Move next/prev amongst the various compiler errors
' ========================================================================================
function OnCommand_SearchGotoCompileError( byval bMoveNext as boolean ) as long
    ' Determine the current selection in the listview and move prev/next as needed
    dim as HWND hLV = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_LVRESULTS )
    Dim As Long nCurSel = ListView_GetSelection(hLV)
    If nCurSel < 0 Then Return 0 

    ' Determine how many lines in the listview so we can "wrap" around
    dim as long nCount = ListView_GetItemCount(hLV)
    
    if bMoveNext then 
        nCurSel = nCurSel + 1
    else   
        nCurSel = nCurSel - 1
    end if
    if nCurSel > nCount - 1 then nCurSel = 0
    if nCurSel < 0 then nCurSel = nCount - 1 

    ListView_SelectItem( hLV, nCurSel )
    SetDocumentErrorPosition( hLV )

    function = 0
end function


' ========================================================================================
' Save the current editor position so we can return to it should the user invoke
' the Last Position (Shift+F6) option. 
' ========================================================================================
function OnCommand_SearchSaveLastPosition( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then exit function
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    gLastPosition.pDoc       = pDoc
    gLastPosition.nFirstLine = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0) 
    gLastPosition.nPosition  = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0) 
    function = 0
end function



'function debug_goto( byval pData as DB2_DATA ptr, byval wszDescription as DWSTRING ) as long
'    print "gdb2 search: "; wszDescription
'    print "pData: "; pData
'    if pData = 0 then return 0
    
'    with *pData
'        print "deleted: "; .deleted
'        print "pDoc: "; .pDoc
'        print "nFiletype: "; .nFiletype
'        print "fileName: "; .filename
'        print "id: "; .id
'        print "nLineStart: "; .nLineStart
'        print "nLineEnd: "; .nLineEnd
'        print "ParentName: "; .ParentName
'        print "ElementName: "; .ElementName   
'        print "ElementData: "; .ElementData
'        print "Variabletype: "; .Variabletype
'        print "TypeExtends: "; .TypeExtends
'        print "VariableScope: "; .VariableScope
'        print "GetSet: "; .GetSet
'        print "isENUM: "; .isENUM
'    end with
    
'    return 0
'end function


' ========================================================================================
' Position editor to document/function/variable under the current caret position
' ========================================================================================
function OnCommand_SearchGotoDefinition( byval pDoc as clsDocument ptr ) as LRESULT

    If pDoc = 0 Then Exit Function
    
    dim pData as DB2_DATA ptr
    dim as DWSTRING wszFilename
    dim as string sTypeName, sSymbolName, sLookFor, sTrigger
    dim as long nLineNum = 0
    
    dim as hwnd hEdit = pDoc->hWndActiveScintilla
    
    ' Version 1.8.8 added ability to open an underlying #Include file if such
    ' a file exists on the current editing line.
    if IsCurrentLineIncludeFilename() then
        wszFilename = gApp.IncludeFilename 
    else

        ' Determine the word at the current caret position if nothing is selected.
        sSymbolName = trim(pDoc->GetSelText)
        if len(sSymbolName) = 0 then sSymbolName = trim(pDoc->GetWord)

        if len(sSymbolName) then
            ' Determine if the function name is part of a class/type. This would be identified
            ' by a preceeding "." or "->" symbol.  eg  pDoc->GetCurrentLineNumber
            dim as long curPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
            dim as long wordStartPos = SciExec( hEdit, SCI_WORDSTARTPOSITION, curPos, true)
            sTrigger = right( pDoc->GetWord( wordStartPos - 1 ), 1 )
            if sTrigger = ">" then sTrigger = "->"
            if ( sTrigger = "." ) orelse ( sTrigger = "->" ) then
                ' In the DereferenceLine function any "->" are converted to "." for simplicity.
                pData = DereferenceLine( pDoc, sTrigger, wordStartPos - 1 )
                if pData then
                    sTypeName = pData->VariableType
                    sLookFor = sTypeName & "." & sSymbolName
                    ' Set pData to null because it currently points at the TYPE definition and
                    ' we use the gdb2 searches below to actually find the symbol definition.
                    pData = 0
                end if
            end if   

'cls
'print "sSymbolName: "; sSymbolName, "sTypeName: "; sTypeName, "sLookFor: "; sLookFor
            ' Search the list of TYPE Function names
            if pData = 0 then 
                pData = gdb2.dbFindFunctionTYPE( sTypeName, sLookFor )
'debug_goto( pData, "dbFindFunctionTYPE" )
            end if
            ' Search the list of Function names
            if pData = 0 then 
                pData = gdb2.dbFindFunction( sSymbolName )
'debug_goto( pData, "dbFindFunction" )
            end if
            ' Search for variable definitions
            if pData = 0 then 
                pData = gdb2.dbFindVariable( "", sSymbolName )
'debug_goto( pData, "dbFindVariable" )
            end if
            ' Search for the TYPE definition
            if pData = 0 then 
                pData = gdb2.dbFindType( sTypeName )
'debug_goto( pData, "dbFindType #1" )
            end if
            ' Maybe just the name itself was clicked. eg. KEYBINDINGS_TYPE 
            if pData = 0 then 
                pData = gdb2.dbFindType( sSymbolName )
'debug_goto( pData, "dbFindType #2" )
            end if
        
        end if
        
        if pData = 0 then
            MessageBox( HWND_FRMMAIN, L(224,"Symbol definition not found."), @wstr(APPNAME), MB_ICONWARNING)
            exit function
        end if
        wszFilename = pData->filename
        nLineNum = pData->nLineStart

    end if
    
    ' Save our current position in case user invokes "Last Position" option,   
    OnCommand_SearchSaveLastPosition( pDoc )
    OpenSelectedDocument( wszFilename, sSymbolName, nLineNum )

    function = 0
end function

' ========================================================================================
' Return to most previous position (invoked when "Last Position" is selected)
' ========================================================================================
function OnCommand_SearchGotoLastPosition() as LRESULT

    ' Is the pDoc pointer still valid
    if gLastPosition.pDoc = 0 then exit function

    ' Position ourselves to the correct previous position.
    OpenSelectedDocument( gLastPosition.pDoc->DiskFilename )
    SciExec( GetFocus, SCI_SETFIRSTVISIBLELINE, gLastPosition.nFirstLine, 0) 
    SciExec( GetFocus, SCI_GOTOPOS, gLastPosition.nPosition, 0) 
    
    function = 0
end function

' ========================================================================================
' Search Symbol popup dialog
' ========================================================================================
function OnCommand_SearchSymbol() as LRESULT
    frmSearchSymbol_Show( HWND_FRMMAIN )
    function = 0
end function

' ========================================================================================
' Search action Find
' ========================================================================================
function OnCommand_SearchFindDialog() as LRESULT
    FindControls_Show()
    frmMain_PositionWindows()

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND)
    gFind.txtFind = AfxGetWindowText(hCtl)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    If pDoc Then 
        FindReplace_HighlightSearches()
        dim as hwnd hEdit = pDoc->hWndActiveScintilla
        dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
        FindReplace_NextSelection(startPos, true, true)
    end if    
    SetFocus( hCtl )

    function = 0
end function


' ========================================================================================
' Search action Replace
' ========================================================================================
function OnCommand_SearchReplaceDialog() as LRESULT
    ' If Replace is already open then close it (toggle).
    gFind.bExpanded = not gFind.bExpanded
    if gFind.bExpanded = false then
        gFind.bShowReplacePanel = false
        gFind.bProjectReplaceActive = false
    else
        FindControls_Show()
        ReplaceControls_Show()
    end if    
    frmMain_PositionWindows()

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND )
    SetFocus( hCtl )
    
    function = 0
end function

' ========================================================================================
' Search action Find in Project
' ========================================================================================
function OnCommand_SearchFindInProject() as LRESULT
    frmFindInProject_Show()
    frmMain_PositionWindows()
    function = 0
end function
        
' ========================================================================================
' Search action Find actions
' ========================================================================================
function OnCommand_SearchFindActions( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as HWND hWndFind = GetDlgItem(HWND_FRMMAIN_TOPTABSFIND, IDC_FRMTOPTABSFIND_TXTFIND)
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
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) then AfxRedrawWindow( HWND_FRMMAIN_TOPTABSFIND )

        case IDM_FINDNEXT
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) then AfxRedrawWindow( HWND_FRMMAIN_TOPTABSFIND )

        case IDM_FINDPREVACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, false, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()

        case IDM_FINDNEXTACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_TOPTABSFIND) = 0 then pDoc->RemoveSelectionAttributes()

    end select
    
    SetFocus( hFocus )
    
    function = 0
end function


' ========================================================================================
' Search action Replace actions
' ========================================================================================
function OnCommand_SearchReplaceActions( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
            
    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        case IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
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
    end select
    
    function = 0
end function


' ========================================================================================
' Position editor to Header/Source/Main/Resource document
' ========================================================================================
function OnCommand_SearchGotoFile( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    if pDoc = 0 then exit function
    
    dim pDocFile as clsDocument ptr
    dim as DWSTRING wszFilename

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    
    select case id
        case IDM_GOTOLINE
            frmGoto_Show( hEdit )
            exit function
        
        case IDM_GOTONEXTFUNCTION
            pDoc->GotoNextFunction
            exit function
        
        case IDM_GOTOPREVFUNCTION
            pDoc->GotoPrevFunction
            exit function
            
        case IDM_GOTONEXTTAB
            gTTabCtl.NextTab
            exit function
        
        case IDM_GOTOPREVTAB
            gTTabCtl.PrevTab
            exit function
        
        case IDM_CLOSETAB
            gTTabCtl.CloseTab
            exit function

        case IDM_GOTOMAINFILE
            pDocFile = gApp.GetMainDocumentPtr
            if pDocFile then wszFilename = pDocFile->DiskFilename
            
        case IDM_GOTORESOURCEFILE
            pDocFile = gApp.GetResourceDocumentPtr
            if pDocFile then wszFilename = pDocFile->DiskFilename

        case IDM_GOTOHEADERFILE
            pDocFile = gApp.GetHeaderDocumentPtr( pDoc )
            if pDocFile then wszFilename = pDocFile->DiskFilename

        case IDM_GOTOSOURCEFILE
            pDocFile = gApp.GetSourceDocumentPtr( pDoc )
            if pDocFile then wszFilename = pDocFile->DiskFilename
    end select

    ' If the document when going to is the same as the one that we are 
    ' already in then no need to execute the movement.
    if pDocFile = pDoc then exit function
    
    ' Save our current position in case user invokes "Last Position" option,   
    gLastPosition.pDoc       = pDoc
    gLastPosition.nFirstLine = SciExec( hEdit, SCI_GETFIRSTVISIBLELINE, 0, 0) 
    gLastPosition.nPosition  = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0) 

    OpenSelectedDocument( wszFilename )
    
    function = 0
end function


' ========================================================================================
' Search action Common actions
' ========================================================================================
function OnCommand_SearchBookmarks( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
        
    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        case IDM_BOOKMARKNEXT:     pDoc->NextBookmark
        case IDM_BOOKMARKPREV:     pDoc->PrevBookmark
        
        case IDM_BOOKMARKCLEARALL
            SciExec( hEdit, SCI_MARKERDELETEALL, -1, 0 )
            LoadBookmarksFiles
            AfxRedrawWindow( HWND_FRMBOOKMARKS )

        case IDM_BOOKMARKTOGGLE
            pDoc->ToggleBookmark( pDoc->GetCurrentLineNumber )
            LoadBookmarksFiles
            AfxRedrawWindow( HWND_FRMBOOKMARKS )

        case IDM_BOOKMARKCLEARALLDOCS
            if MessageBox( HWND_FRMMAIN, L(316,"Do you want to delete all bookmarks?"), L(276,"Confirm"), _
                        MB_YESNOCANCEL or MB_ICONQUESTION or MB_DEFBUTTON1 ) <> IDYES then
                return true
            end if            
            dim pDoc as clsDocument ptr = gApp.pDocList
            do until pDoc = 0
                hEdit = pDoc->hWindow(0)
                SciExec( hEdit, SCI_MARKERDELETEALL, MARKER_BOOKMARK, 0 )
                pDoc = pDoc->pDocNext
            loop
            LoadBookmarksFiles
            AfxRedrawWindow( HWND_FRMBOOKMARKS )
    end select
    
    function = 0
end function
