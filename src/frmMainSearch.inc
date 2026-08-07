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
' Position editor to document/function/variable under the current caret position
' ========================================================================================
function OnCommand_SearchGotoDefinition( byval pDoc as clsDocument ptr ) as LRESULT

    If pDoc = 0 Then Exit Function

    dim as DWSTRING wszFilename
    dim as string sSymbolName, sTrigger
    dim as long nLineNum = 0

    dim as hwnd hEdit = pDoc->hWndActiveScintilla

    ' Version 1.8.8 added ability to open an underlying #Include file if such
    ' a file exists on the current editing line.
    if IsCurrentLineIncludeFilename() then
        wszFilename = gApp.IncludeFilename
    else
        dim as SYMBOLREF refSym
        refSym.idx = -1

        ' Determine the word at the current caret position if nothing is selected.
        sSymbolName = trim(pDoc->GetSelText)
        if len(sSymbolName) = 0 then sSymbolName = trim(pDoc->GetWord)

        if len(sSymbolName) then
            dim as DWSTRING wszSymbolName = sSymbolName

            ' Determine if the symbol is part of a class/type. This would be identified
            ' by a preceeding "." or "->" symbol.  eg  pDoc->GetCurrentLineNumber
            dim as long curPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0)
            dim as long wordStartPos = SciExec( hEdit, SCI_WORDSTARTPOSITION, curPos, true)
            sTrigger = right( pDoc->GetWord( wordStartPos - 1 ), 1 )
            if sTrigger = ">" then sTrigger = "->"
            if ( sTrigger = "." ) orelse ( sTrigger = "->" ) then
                ' Resolve the expression before the trigger and look the clicked
                ' word up as a member of its TYPE (EXTENDS chain walked inside).
                dim as SYMBOLREF refExpr = DereferenceLine( pDoc, sTrigger, wordStartPos - 1 )
                dim as SYMBOLREF refType = SymRefTypeOf( refExpr )
                if refType.IsValid then
                    refSym = gSymDb.FindMemberOf( refType, wszSymbolName )
                end if
            end if

            ' Fall back through proc names, scope-aware variables, then TYPE names.
            if refSym.IsValid = false then refSym = gSymDb.FindProc( wszSymbolName )
            if refSym.IsValid = false then
                refSym = gSymDb.FindVariable( wszSymbolName, ScanMgr_GetRootName( pDoc ), _
                                              pDoc->GetCurrentLineNumber + 1 )
            end if
            if refSym.IsValid = false then refSym = gSymDb.FindType( wszSymbolName )
        end if

        dim as wstring ptr pFile = 0
        if refSym.IsValid then pFile = gSymDb.SymFile( refSym )
        if pFile = 0 then
            ' This was the one box built inline rather than through a per-dialog helper, and
            ' the only one that was never themed -- it set fonts but no MBX_COLORS at all, so
            ' it came up in PsMessageBox's own dark defaults whatever the theme said.
            TikoMsgBox( HWND_FRMMAIN, L(184,"Symbol definition not found."), APPNAME, _
                        TMB_ICON_WARNING, TMB_OK )
            exit function
        end if
        wszFilename = *pFile

        ' Jump to the implementation when there is one, else the declaration.
        ' fbcParser lines are 1-based; Scintilla lines are 0-based.
        nLineNum = gSymDb.SymBodyLine( refSym )
        if nLineNum = 0 then nLineNum = gSymDb.SymLine( refSym )
        nLineNum -= 1
        if nLineNum < 0 then nLineNum = 0

    end if
    
    ' The departure point is recorded inside OpenSelectedDocument now -- it is the one choke
    ' point every navigation passes through, so no caller needs its own push.
    OpenSelectedDocument( wszFilename, sSymbolName, nLineNum )

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
    ' FindControls_Show repositions -- no frmMain_PositionWindows call needed here.
    FindControls_Show()

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
    gFind.txtFind = WindowText(hCtl)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    ' NOTHING TO SEARCH FOR MEANS DO NOTHING, and that guard is load-bearing now that an
    ' empty selection leaves the box empty. NextSelection's no-match arm ends in
    ' SCI_SETSEL( startPos, startPos ) with startPos = anchor - 1, so running it on an empty
    ' term would nudge the caret one character left every time Ctrl+F was pressed.
    dim as boolean bHasDoc  = cbool( pDoc <> 0 )
    dim as boolean bHasTerm = cbool( PsLen(gFind.txtFind) > 0 )
    If bHasDoc andalso bHasTerm Then
        FindReplace_HighlightSearches()
        dim as hwnd hEdit = pDoc->hWndActiveScintilla
        dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
        FindReplace_NextSelection(startPos, true, true)
    elseif bHasDoc then
        ' Still drop the old term's highlights -- they belong to a phrase that is no longer
        ' in the box.
        FindReplace_HighlightSearches()
        gFind.wszResults = ""
        gFind.foundCount = 0
    end if
    SetFocus( hCtl )

    function = 0
end function


' ========================================================================================
' Search action Replace
' ========================================================================================
function OnCommand_SearchReplaceDialog() as LRESULT
    ' If Replace is already open then close it (toggle). The test now reads the SAME flag
    ' the layout reads: it used to key off gFind.bExpanded, which Ctrl+F cleared without
    ' closing anything -- so after a Ctrl+F the first Ctrl+H re-opened an already-open bar
    ' and appeared to do nothing.
    if gFind.bShowReplacePanel then
        gFind.bProjectReplaceActive = false
        FindReplace_SetVisible( true, false )
    else
        FindControls_Show()
        ReplaceControls_Show()
    end if

    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    SetFocus( hCtl )
    
    function = 0
end function


' ========================================================================================
' Search action Find in Project
' ========================================================================================
function OnCommand_SearchFindInProject() as LRESULT
    ' SEED THE FIND FIELD FROM THE SELECTION BEFORE THE TAB SWITCH, not after.
    ' FindControls_Show already does this for Ctrl+F, and SetFocusTab calls it on the way
    ' into the Find in Project tab -- but by then PsTabBar_SetCurSel has already moved the
    ' current tab, so GetActiveDocumentPtr() returns null there (the Find tab holds a
    ' sentinel, not a document) and the whole seeding block is skipped. The selection has
    ' to be read here, while the document tab is still the current one.
    dim as boolean bSeeded = false
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        dim as string sFindText = pDoc->GetSelText
        ' A MULTI-LINE SELECTION IS NOT A SEARCH PHRASE. Ctrl+F treats one as "search
        ' within this selection"; a project search has no such mode, so it is simply not a
        ' term and lands in the same empty-box case below. Tested on the text rather than on
        ' CurrentSelection, which is only refreshed on the document paths.
        if instr( sFindText, chr(10) ) then sFindText = ""
        ' AN EMPTY SELECTION EMPTIES THE BOX -- it does not re-offer the last phrase. By
        ' request, and the same rule FindControls_Show applies to Ctrl+F: the field is a
        ' picture of the selection, so no selection means nothing to search for and the tab
        ' opens ready to be typed into.
        gFind.txtFind = sFindText
        dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
        if IsWindow(hCtl) then
            if sFindText <> WindowText(hCtl) then
                AfxSetWindowText( hCtl, sFindText )
            end if
            dim as long nLen = len(sFindText)
            Edit_SetSel( hCtl, nLen, nLen )
        end if
        ' Only a REAL phrase runs a search. An emptied box must not, and that is not merely
        ' "nothing to do": FipBar_RunSearch's empty-phrase arm clears the model, so running
        ' it here would throw away results the user may still be looking at.
        bSeeded = cbool( len(sFindText) > 0 )
    end if

    frmFindInProject_Show()
    frmMain_PositionWindows()

    ' A seeded phrase runs immediately -- opening the tab with the term already in the box
    ' and an empty results list would just be a second keystroke asking for what was already
    ' asked for. ONLY when we seeded: with no selection the box is now EMPTY, and searching
    ' for "" would clear the model and take the results already on screen with it.
    ' AFTER PositionWindows, because RunSearch ends by revealing the current match and the
    ' surface has to have its real size for that to land anywhere.
    if bSeeded then frmFindInProject_RerunSearch()

    function = 0
end function
        
' ========================================================================================
' Search action Find actions
' ========================================================================================
function OnCommand_SearchFindActions( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    ' On the Find in Project tab these actions mean the PROJECT search, not this document's.
    ' Enter in the find box already funnels here through IDM_FINDNEXT, so this one test
    ' retargets typing, F3, and the bar's own prev/next icons together.
    if frmFindInProject_BarAction( id ) then return 0

    ' Reachable with no active document -- an accelerator bypasses menu enablement.
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    dim as HWND hWndFind = GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
    dim as HWND hFocus = GetFocus()

    if PsTextBox_HasFocus( hWndFind ) then
        gFind.txtFind = WindowText(hWndFind)
    elseif hFocus = hEdit then    
        dim as string sFindText = pDoc->GetSelText
        if len(sFindText) then 
            ' Update the Find search term if the frmFind pane is open.
            '' HOISTED TO A BOOLEAN. IsWindowVisible returns a BOOL, and mixing that with a
            '' boolean in `andalso` is fbc warning 38 -- see C:\dev\Learnings.md.
            dim as boolean bFindVisible = (IsWindowVisible(HWND_FRMMAIN_FIND) <> 0)
            if (sFindText <> gFind.txtFind) andalso bFindVisible then
                dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)
                AfxSetWindowText( hCtl, sFindText )
            end if
            gFind.foundCount = 0
            gFind.txtFind = sFindText
        end if
    end if

    dim as long startPos = SendMessage( hEdit, SCI_GETCURRENTPOS, 0, 0)

    ' A Find is a navigation within the document. Recorded here because it never reaches
    ' OpenSelectedDocument. A search that finds nothing leaves the caret where it was, and
    ' the same-file/same-line rule in NavHistory_AppendEntry collapses that to one entry.
    NavHistory_RecordJump()

    select case id
        case IDM_FINDPREV
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, false, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_FIND) then AfxRedrawWindow( HWND_FRMMAIN_FIND )

        case IDM_FINDNEXT
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_FIND) then AfxRedrawWindow( HWND_FRMMAIN_FIND )

        case IDM_FINDPREVACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, false, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_FIND) = 0 then pDoc->RemoveSelectionAttributes()

        case IDM_FINDNEXTACCEL
            FindReplace_HighlightSearches()
            if FindReplace_NextSelection(startPos, true, true) then pDoc->CenterCurrentLine
            if IsWindowVisible(HWND_FRMMAIN_FIND) = 0 then pDoc->RemoveSelectionAttributes()

    end select

    NavHistory_NoteArrival()

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

    ' Project-wide replace on the Find in Project tab.
    if frmFindInProject_BarAction( id ) then return 0

    ' Reachable with no active document -- see OnCommand_SearchFindActions above.
    if pDoc = 0 then exit function

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
        ' These two move the caret WITHIN the document, so they never reach
        ' OpenSelectedDocument and need to record for themselves.
        case IDM_GOTONEXTFUNCTION
            NavHistory_RecordJump()
            pDoc->GotoNextFunction
            NavHistory_NoteArrival()
            exit function

        case IDM_GOTOPREVFUNCTION
            NavHistory_RecordJump()
            pDoc->GotoPrevFunction
            NavHistory_NoteArrival()
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

    ' Reachable with no active document -- an accelerator bypasses menu enablement.
    if pDoc = 0 then exit function

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    select case id
        ' In-document jumps: record here, since OpenSelectedDocument is not involved.
        case IDM_BOOKMARKNEXT
            NavHistory_RecordJump()
            pDoc->NextBookmark
            NavHistory_NoteArrival()

        case IDM_BOOKMARKPREV
            NavHistory_RecordJump()
            pDoc->PrevBookmark
            NavHistory_NoteArrival()

        case IDM_BOOKMARKCLEARALL
            SciExec( hEdit, SCI_MARKERDELETEALL, -1, 0 )
            ClearBookmarks()

        case IDM_BOOKMARKTOGGLE
            pDoc->ToggleBookmark( pDoc->GetCurrentLineNumber )
            LoadBookmarksFiles()

        case IDM_BOOKMARKCLEARALLDOCS
            if TikoMsgBox( HWND_FRMMAIN, L(248,"Do you want to delete all bookmarks?"), L(214,"Confirm"), _
                        TMB_ICON_QUESTION, TMB_YESNOCANCEL ) <> IDYES then
                return true
            end if            
            dim pDoc as clsDocument ptr = gApp.pDocList
            do until pDoc = 0
                hEdit = DocView(pDoc, 0)
                SciExec( hEdit, SCI_MARKERDELETEALL, MARKER_BOOKMARK, 0 )
                pDoc = pDoc->pDocNext
            loop
            ClearBookmarks()
    end select
    
    function = 0
end function
