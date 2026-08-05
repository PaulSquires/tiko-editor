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

''
''  frmFunctions.inc
''   

#include once "frmFunctions.bi"

' ========================================================================================
' Content just changed programmatically (e.g. an auto-inserted skeleton): hand the
' active document to the background scanner without waiting for the typing debounce.
' The parse-complete handler refreshes the Functions panel from the new results.
' ========================================================================================
function frmFunctions_ReparseFiles() as long
    gScanMgr.RequestBufferScan( gTTabCtl.GetActiveDocumentPtr() )
    function = 0
end function


' ========================================================================================
' Expand/Collapse all Functions Nodes
' ========================================================================================
function frmFunctions_ExpandAll() as long
    PsListTree_ExpandAll( HWND_FRMFUNCTIONS )
    function = 0
end function

function frmFunctions_CollapseAll() as long
    PsListTree_CollapseAll( HWND_FRMFUNCTIONS )
    function = 0
end function


' ========================================================================================
' Select the listbox item that matches the incoming pDoc item. Do not open nodes to
' find a possible hidden document because the user may have purposely closed a node
' and we should not re-open it automatically.
' ========================================================================================
function frmFunctions_SelectItemData( byval pDoc as clsDocument ptr ) as boolean
    ' Select the Function List listbox item whose backing file matches the document.
    if pDoc = 0 then return false
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsLen( wszFile ) = 0 then return false

    ' If the currently selected item already belongs to this file then simply exit
    dim as integer nCurSel = PsListTree_GetCurSel(hCtrl)
    if nCurSel >= 0 then
        dim as integer nFileIdx = PsListTree_GetItemData( hCtrl, nCurSel )
        if (nFileIdx >= 0) andalso (nFileIdx <= ubound(gFuncPanelFiles)) then
            if SymDb_FileNameEq( gFuncPanelFiles(nFileIdx), wszFile ) then return true
        end if
    end if

    dim as long nCount = PsListTree_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        dim as integer nFileIdx = PsListTree_GetItemData( hCtrl, i )
        dim as boolean bMatch = false
        if (nFileIdx >= 0) andalso (nFileIdx <= ubound(gFuncPanelFiles)) then
            bMatch = SymDb_FileNameEq( gFuncPanelFiles(nFileIdx), wszFile )
        end if
        if bMatch then
            ' If the pDoc is currently within the visible range of the Listbox then
            ' simply move to that row, otherwise we set the topindex and position
            ' the selection a few rows from it.
            dim as PSLISTTREE ptr pList = GetListTreePointer(hCtrl)
            if pList = null then return false

            dim as integer nFirstRow = PsListTree_GetTopIndex(hCtrl)
            dim as integer nLastRow = nFirstRow + PsListTree_ItemsPerPage(pList)
            if (i >= nFirstRow) andalso (i <= nLastRow) then
                PsListTree_SetCurSel( hCtrl, i )
            else    
                ' Attempt to set the topindex so that the current selection appears
                ' a little more down the list rather than pinned to the top of the
                ' listbox window.
                PsListTree_SetTopIndex( hCtrl, MAX(0, i - 8) )
                PsListTree_SetCurSel( hCtrl, i )
            end if
            
            return true
        end if
    next
    return false
end function


' ========================================================================================
' frmFunctions_PaintCallback
' ========================================================================================
sub frmFunctions_PaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 

    ' State priority for the row background/foreground: selected > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isSelected then
    ' NOTE: selected deliberately reuses the HOT pair -- there is no dedicated
    ' select colour in the panel namespace. (This was two commented-out lines naming
    ' ghPanel.Back/ForeColorSelect, fields that never existed in that struct.)
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    elseif p->isHot then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        backclr = theme.panel.text.backcolor
        foreclr = theme.panel.text.forecolor
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Paint the entire row background
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc 
    dim as RECT rcIcon = p->rc 

    dim as long nIconWidth = pWindow->ScaleX(20)
    
    dim as DWSTRING wszCaption = p->wszCaption

    ' indent the text based on its type
    if p->isHeader then
        p->b->SetFont( ghFont(SYMBOLFONT_10) )
        rcIcon.right = rcIcon.left + nIconWidth
        dim as DWSTRING wszIcon = iif(p->isCollapsed, wszIconChevronRight, wszIconChevronDown) 
        p->b->PaintText( wszIcon, @rcIcon, DT_CENTER )

        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcIcon.right
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    else
        ' This would be a regular file.
        rcIcon.left = rcText.left + nIconWidth
        rcIcon.right = rcIcon.left + nIconWidth 

        p->b->SetFont( ghFont(GUIFONT_10) )  ' use GUI rather than SYMBOL b/c of dot icon
        p->b->PaintText( wszIconDocument, @rcIcon, DT_CENTER )

        ' The caption contains two parts; the function name and its calltip
        ' separated by an % character.
        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcIcon.right
        wszCaption = PsStrParse(wszCaption, 1, "%")
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    end if

end sub


' ========================================================================================
' frmFunctions_MessageCallback
' ========================================================================================
function frmFunctions_MessageCallback( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_MOUSEWHEEL
            'print "WM_MOUSEWHEEL"
            return true

        case WM_MOUSEHOVER
            'print "WM_MOUSEHOVER on idx: "; m->idx
            ' Tooltips are now on-demand via the tooltip callback; nothing to push.
            return true
            
        case WM_LBUTTONUP
            ' Attempt to show the bookmark
            dim as HWND hCtl = GetParent( m->hList )

            if PsListTree_IsHeader( hCtl, m->idx ) = false then
                dim as integer nFileIdx = PsListTree_GetItemData( hCtl, m->idx )
                dim as integer nLineNum = PsListTree_GetItemDataExtra( hCtl, m->idx )
                if (nFileIdx >= 0) andalso (nFileIdx <= ubound(gFuncPanelFiles)) then
                    ' if the file is already open in the Top tabs then switch to that
                    ' tab; otherwise open it from disk (never-opened include files
                    ' have no clsDocument yet).
                    dim as clsDocument ptr pDoc = gApp.GetDocumentPtrByFilename( gFuncPanelFiles(nFileIdx) )
                    dim as boolean bSwitched = false
                    if pDoc then
                        if gTTabCtl.SetTabIndexByDocumentPtr( pDoc ) <> -1 then
                            dim as hwnd hEdit = pDoc->hwndActiveScintilla
                            SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, max(nLineNum - 3, 0), 0)
                            SciExec( hEdit, SCI_GOTOLINE, nLineNum, 0)
                            pDoc->CenterCurrentLine()
                            bSwitched = true
                        end if
                    end if
                    if bSwitched = false then
                        OpenSelectedDocument( gFuncPanelFiles(nFileIdx), "", nLineNum )
                    end if
                    frmMain_SetFocusToCurrentCodeWindow()
                end if
            end if

            ' Header collapse/expand is handled inside the control on the mouse-down,
            ' and it preserves the current selection. Return FALSE: the listbox needs
            ' its default WM_LBUTTONUP handling to release the mouse capture it took
            ' on the button-down.
            return false
    
    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function    


' ========================================================================================
' frmFunctions_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function frmFunctions_TooltipCallback( byval hCtl as HWND, byval row as integer, byval col as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as PSLISTTREE ptr pList = GetListTreePointer(hCtl)
    if pList then
        ' The caption contains two parts; the function name and its calltip
        ' separated by an % character.
        dim as DWSTRING wszCaption = PsListTree_GetText(hCtl, row)
        ' NOT "dim as DWSTRING wszTooltip = ..." -- that redeclaration shadowed the
        ' outer variable, so the formatted calltip died with the if-block and the empty
        ' outer one was returned. PsListTree reads an empty string as "no tip for this
        ' row", so the panel showed no tooltips at all.
        wszTooltip = PsStrParse(wszCaption, 2, "%")
        wszTooltip = FormatCodetip( wszTooltip )
        ' The tip wraps because PsTooltip does it natively (armed once, in
        ' Tooltips_ApplyTheme). This used to hand-send TTM_SETMAXTIPWIDTH to the
        ' comctl32 tip from inside the callback -- i.e. on every single tip.
    end if
    return wszTooltip
end function


' ========================================================================================
' frmFunctions_Show
' ========================================================================================
function frmFunctions_Show( byval hWndParent as HWND ) as LRESULT

    dim as HWND hCtrl = PsListTree_Create( hWndParent, IDC_FRMFUNCTIONS_LISTBOX )

    PsListTree_SetPaintCallback( hCtrl, @frmFunctions_PaintCallback )
    PsListTree_SetMessageCallback( hCtrl, @frmFunctions_MessageCallback )
    PsListTree_SetTooltipCallback( hCtrl, @frmFunctions_TooltipCallback )
    ' Owner-drawn tips, which is also what makes the calltip wrap without the
    ' per-tip TTM_SETMAXTIPWIDTH the callback used to send.
    PsListTree_SetTooltipMode( hCtrl, PSTIP_MODE_PS )
    ' The WM_LBUTTONUP callback opens the clicked function; a double-click would
    ' deliver a second up and open it twice, so suppress double-click delivery.
    PsListTree_PreventDoubleClick( hCtrl )
    PsListTree_SetBackColor( hCtrl, theme.panel.text.backcolor )
    PsListTree_SetFont( hCtrl, ghFont(GUIFONT_10) )
    'PsListTree_SetExtendedSelect( hCtrl, true )   ' Shift/Ctrl range selection
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (PsListTree_SetScrollBarPaintCallback is available for full custom drawing.)
    PsListTree_SetScrollBarWidth( hCtrl, SCROLLBAR_WIDTH_PANEL )
    PsListTree_SetScrollBarColors( hCtrl, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
'    PsListTree_SetScrollBarColors( hCtrl, theme.editor.scrollbar.backcolor, _
'   (was a commented-out call naming ghEditor.ForeColorScrollBarHot, a field that
'    never existed in that struct.)

    HWND_FRMFUNCTIONS = hCtrl
    
    function = 0
    
end function


' ========================================================================================
' Simple QuickSort of Filename array (sorted by display name, i.e. name + extension)
' ========================================================================================
function QuickSortFilenames( arrFiles() as DWSTRING, lo as long, hi as long ) as long
    dim as long i, j
    dim as DWSTRING pivot

    if lo < hi then
        i = lo : j = hi
        pivot = PsUCase( PsPathName(arrFiles( (lo+hi)/2 ) ) )
        do
            while PsUCase(PsPathName(arrFiles(i))) < pivot and i < hi: i += 1: wend
            while PsUCase(PsPathName(arrFiles(j))) > Pivot and j > lo: j -= 1: wend
            if i <= j then swap arrFiles(i), arrFiles(j): i += 1: j -= 1
        loop while i <= j
        if lo < j then QuickSortFilenames( arrFiles(), lo, j )
        if hi > i then QuickSortFilenames( arrFiles(), i, hi )
    end if

    function = 0
end function

' ========================================================================================
' Simple QuickSort of Function Name array
' ========================================================================================
function QuickSortFuncs( a() as FUNCTION_NODE_TYPE, lo as long, hi as long ) as long
    dim as long i, j
    dim as DWSTRING pivot
    
    if lo < hi then
        i = lo : j = hi
        pivot = PsUCase( a( (lo+hi)/2 ).wszFunctionName )
        do
            while PsUCase(a(i).wszFunctionName) < pivot and i < hi: i += 1: wend
            while PsUCase(a(j).wszFunctionName) > Pivot and j > lo: j -= 1: wend
            if i <= j then swap a(i), a(j): i += 1: j -= 1
        loop while i <= j
        if lo < j then QuickSortFuncs( a(), lo, j )
        if hi > i then QuickSortFuncs( a(), i, hi )
    end if
    
    function = 0
end function


' ========================================================================================
' frmFunctions_BuildFileList
' Fill gFuncPanelFiles() with the files this panel lists, sorted by display name, and
' return the count.
'
' The symbol database knows every user file it has parsed -- including include files that
' have never been opened in the editor -- so it is NOT the panel's file list. The panel
' mirrors the Explorer: a file the user cannot see in the workspace tree should not
' contribute functions here. frmExplorer_IsFileDisplayed is the single statement of that
' rule, so the two panels cannot disagree about which files exist.
'
' Scan filenames are UPPERCASED - normalize to on-disk casing for display and
' click-through before testing, since the Explorer's documents carry real casing.
' ========================================================================================
function frmFunctions_BuildFileList() as long
    erase gFuncPanelFiles
    dim as long nFiles = gSymDb.EnumUserFiles( gFuncPanelFiles() )

    dim as long nKeep = 0
    for i as long = 0 to nFiles - 1
        dim as DWSTRING wszFile = FilenameOriginalCase( gFuncPanelFiles(i) )
        if frmExplorer_IsFileDisplayed( wszFile ) = false then continue for
        gFuncPanelFiles(nKeep) = wszFile
        nKeep += 1
    next

    ' Drop the tail rather than leaving stale entries above the kept run: the array's
    ' ubound is used as a range check when a clicked row is resolved back to a file.
    if nKeep > 0 then
        redim preserve gFuncPanelFiles(nKeep - 1)
        QuickSortFilenames( gFuncPanelFiles(), 0, nKeep - 1 )
    else
        erase gFuncPanelFiles
    end if

    function = nKeep
end function


' ========================================================================================
' ClearFunctions
' ========================================================================================
function ClearFunctions() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS
    PsListTree_Clear( hCtrl )
    function = 0
end function

' ========================================================================================
' LoadFilesAndFunctions
' ========================================================================================
function LoadFilesAndFunctions() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    ' Save the topindex because we will restore it after filling the new contents
    ' (selection is NOT saved by index - it is re-derived from the active
    ' document after the rebuild)
    dim as long nTopIndex = PsListTree_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    PsListTree_Clear( hCtrl )

    PsListTree_BeginUpdate( hCtrl )

    dim as DWSTRING wszText

    ' The files shown in the Explorer, in display order.
    dim as long nFiles = frmFunctions_BuildFileList()

    ' Get the functions (and sort them) for each file
    redim arrFuncs(any) as FUNCTION_NODE_TYPE
    dim rs() as SYMBOLREF

    for i as long = 0 to nFiles - 1
        wszText = PsPathName(gFuncPanelFiles(i) )
        PsListTree_AddHeader( hCtrl, wszText, i, 0 )

        dim as long nProcs = gSymDb.EnumProcsInFile( gFuncPanelFiles(i), rs() )
        for p as long = 0 to nProcs - 1
            ' implementations only - declare-only prototypes never appeared in
            ' the old (parsed-bodies) panel either
            dim as long nBodyLine = gSymDb.SymBodyLine( rs(p) )
            if nBodyLine <= 0 then continue for
            dim as long ub = ubound(arrFuncs) + 1
            redim preserve arrFuncs(ub)
            arrFuncs(ub).wszFunctionName = gSymDb.QualifiedName( rs(p) )
            arrFuncs(ub).wszPrototype = gSymDb.BuildCalltip( rs(p) )
            arrFuncs(ub).nLineNumber = nBodyLine - 1     ' Scintilla lines are 0-based
            arrFuncs(ub).nFileIdx = i
        next

        quickSortFuncs( arrFuncs(), lbound(arrFuncs), ubound(arrFuncs) )

        for ii as long = lbound(arrFuncs) to ubound(arrFuncs)
            wszText = PsLTrim(arrFuncs(ii).wszFunctionName) & _
                      "%" & PsTrim(arrFuncs(ii).wszPrototype)
            PsListTree_AddString( hCtrl, wszText, i, arrFuncs(ii).nLineNumber )
        next
        erase arrFuncs

    next

    PsListTree_EndUpdate( hCtrl )

    ' Restore the scroll position, then re-select by IDENTITY rather than by the
    ' saved index: the list was just rebuilt (background scans add/remove/move
    ' rows), so the old index would land on an arbitrary row. Select the active
    ' document's row; if there is none (or it isn't in the list) leave nothing
    ' selected rather than something random.
    PsListTree_SetTopIndex( hCtrl, nTopIndex )
    if frmFunctions_SelectItemData( gTTabCtl.GetActiveDocumentPtr() ) = false then
        PsListTree_SetCurSel( hCtrl, -1 )
    end if

    function = 0
end function


' ========================================================================================
' LoadFunctionsOnly
' This will clear the current list of files in the listbox and repopulate it.
' ========================================================================================
function LoadFunctionsOnly() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    ' Save the topindex because we will restore it after filling the new contents
    ' (selection is NOT saved by index - it is re-derived from the active
    ' document after the rebuild)
    dim as long nTopIndex = PsListTree_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    PsListTree_Clear( hCtrl )

    PsListTree_BeginUpdate( hCtrl )


    dim as DWSTRING wszText

    ' The files shown in the Explorer, then every implementation in each of them,
    ' flattened into one name-sorted list.
    dim as long nFiles = frmFunctions_BuildFileList()

    redim arrFuncs(any) as FUNCTION_NODE_TYPE
    dim rs() as SYMBOLREF

    for i as long = 0 to nFiles - 1
        dim as long nProcs = gSymDb.EnumProcsInFile( gFuncPanelFiles(i), rs() )
        for p as long = 0 to nProcs - 1
            dim as long nBodyLine = gSymDb.SymBodyLine( rs(p) )
            if nBodyLine <= 0 then continue for
            dim as long ub = ubound(arrFuncs) + 1
            redim preserve arrFuncs(ub)
            arrFuncs(ub).wszFunctionName = gSymDb.QualifiedName( rs(p) )
            arrFuncs(ub).wszPrototype = gSymDb.BuildCalltip( rs(p) )
            arrFuncs(ub).nLineNumber = nBodyLine - 1     ' Scintilla lines are 0-based
            arrFuncs(ub).nFileIdx = i
        next
    next

    quickSortFuncs( arrFuncs(), lbound(arrFuncs), ubound(arrFuncs) )

    for ii as long = lbound(arrFuncs) to ubound(arrFuncs)
        wszText = PsLTrim(arrFuncs(ii).wszFunctionName) & _
                  "%" & PsTrim(arrFuncs(ii).wszPrototype)
        PsListTree_AddString( hCtrl, wszText, arrFuncs(ii).nFileIdx, arrFuncs(ii).nLineNumber )
    next
    erase arrFuncs


    PsListTree_EndUpdate( hCtrl )

    ' Restore the scroll position, then re-select by IDENTITY rather than by the
    ' saved index - same reasoning as LoadFilesAndFunctions above.
    PsListTree_SetTopIndex( hCtrl, nTopIndex )
    if frmFunctions_SelectItemData( gTTabCtl.GetActiveDocumentPtr() ) = false then
        PsListTree_SetCurSel( hCtrl, -1 )
    end if

    function = 0
end function


' ========================================================================================
' LoadFunctionsFiles
' This will clear the current list of files in the listbox and repopulate it.
' ========================================================================================
function LoadFunctionsFiles() as long
    ' Hide the listbox while it is loading so that we don't get flicker from
    ' white background from the empty listbox
    ShowWindow( HWND_FRMFUNCTIONS, SW_HIDE )
    
    ' Call the specific function to load the listbox based on whether we want to
    ' to show Files & Functions, or just Functions only.
    if gFunctionsDisplay = FunctionsDisplayState.ViewAsTree then
        LoadFilesAndFunctions()   
    elseif gFunctionsDisplay = FunctionsDisplayState.ViewAsList then
        LoadFunctionsOnly()   
    end if

    ' Ensure to show/hide this panel if it is active or not
    frmPanel_PositionWindows()

    function = 0
end function


' ========================================================================================
' frmFunctions_ViewAsTree / frmFunctions_ViewAsList
' Set the global view state for the list and then reload the list.
' ========================================================================================
function frmFunctions_ViewAsTree() as long
    if gFunctionsDisplay = FunctionsDisplayState.ViewAsTree then exit function
    gFunctionsDisplay = FunctionsDisplayState.ViewAsTree 
    LoadFunctionsFiles()
    function = 0
end function

function frmFunctions_ViewAsList() as long
    if gFunctionsDisplay = FunctionsDisplayState.ViewAsList then exit function
    gFunctionsDisplay = FunctionsDisplayState.ViewAsList
    LoadFunctionsFiles()
    function = 0
end function


