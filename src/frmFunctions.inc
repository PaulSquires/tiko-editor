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
    CListBox_ExpandAll( HWND_FRMFUNCTIONS )
    function = 0
end function

function frmFunctions_CollapseAll() as long
    CListBox_CollapseAll( HWND_FRMFUNCTIONS )
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
    if len( wszFile ) = 0 then return false

    ' If the currently selected item already belongs to this file then simply exit
    dim as integer nCurSel = CListBox_GetCurSel(hCtrl)
    if nCurSel >= 0 then
        dim as integer nFileIdx = CListBox_GetItemData( hCtrl, nCurSel )
        if (nFileIdx >= 0) andalso (nFileIdx <= ubound(gFuncPanelFiles)) then
            if SymDb_FileNameEq( gFuncPanelFiles(nFileIdx), wszFile ) then return true
        end if
    end if

    dim as long nCount = CListBox_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        dim as integer nFileIdx = CListBox_GetItemData( hCtrl, i )
        dim as boolean bMatch = false
        if (nFileIdx >= 0) andalso (nFileIdx <= ubound(gFuncPanelFiles)) then
            bMatch = SymDb_FileNameEq( gFuncPanelFiles(nFileIdx), wszFile )
        end if
        if bMatch then
            ' If the pDoc is currently within the visible range of the Listbox then
            ' simply move to that row, otherwise we set the topindex and position
            ' the selection a few rows from it.
            dim as CLISTBOX ptr pList = GetListBoxPointer(hCtrl)
            if pList = null then return false

            dim as integer nFirstRow = CListBox_GetTopIndex(hCtrl)
            dim as integer nLastRow = nFirstRow + CListBox_ItemsPerPage(pList)
            if (i >= nFirstRow) andalso (i <= nLastRow) then
                CListBox_SetCurSel( hCtrl, i )
            else    
                ' Attempt to set the topindex so that the current selection appears
                ' a little more down the list rather than pinned to the top of the
                ' listbox window.
                CListBox_SetTopIndex( hCtrl, MAX(0, i - 8) )
                CListBox_SetCurSel( hCtrl, i )
            end if
            
            return true
        end if
    next
    return false
end function


' ========================================================================================
' frmFunctions_PaintCallback
' ========================================================================================
sub frmFunctions_PaintCallback( byval p as CLISTBOX_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 

    ' State priority for the row background/foreground: selected > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isSelected then
'        backclr = ghPanel.BackColorSelect
'        foreclr = ghPanel.ForeColorSelect
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    elseif p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
    end if
    p->b->SetForeColors(foreclr, foreclr)
    p->b->SetBackColors(backclr, backclr)

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
        wszCaption = AfxStrParse(wszCaption, 1, "%")
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    end if

end sub


' ========================================================================================
' frmFunctions_MessageCallback
' ========================================================================================
function frmFunctions_MessageCallback( byval m as CLISTBOX_MESSAGEINFO ptr ) as boolean

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

            if CListBox_IsHeader( hCtl, m->idx ) = false then
                dim as integer nFileIdx = CListBox_GetItemData( hCtl, m->idx )
                dim as integer nLineNum = CListBox_GetItemDataExtra( hCtl, m->idx )
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
function frmFunctions_TooltipCallback( byval hCtl as HWND, byval row as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as CLISTBOX ptr pList = GetListBoxPointer(hCtl)
    if pList then
        ' The caption contains two parts; the function name and its calltip
        ' separated by an % character.
        dim as DWSTRING wszCaption = CListBox_GetText(hCtl, row)
        dim as DWSTRING wszTooltip = AfxStrParse(wszCaption, 2, "%")
        wszTooltip = FormatCodetip( wszTooltip )
        SendMessage( pList->hToolTip, TTM_SETMAXTIPWIDTH, 0, 300 )   ' allows for multiline
    end if    
    return wszTooltip
end function


' ========================================================================================
' frmFunctions_Show
' ========================================================================================
function frmFunctions_Show( byval hWndParent as HWND ) as LRESULT

    dim as HWND hCtrl = CListBox_Create( hWndParent, IDC_FRMFUNCTIONS_LISTBOX )

    CListBox_SetPaintCallback( hCtrl, @frmFunctions_PaintCallback )
    CListBox_SetMessageCallback( hCtrl, @frmFunctions_MessageCallback )
    CListBox_SetTooltipCallback( hCtrl, @frmFunctions_TooltipCallback )
    ' The WM_LBUTTONUP callback opens the clicked function; a double-click would
    ' deliver a second up and open it twice, so suppress double-click delivery.
    CListBox_PreventDoubleClick( hCtrl )
    CListBox_SetBackColor( hCtrl, ghPanel.BackColor )
    CListBox_SetFont( hCtrl, ghFont(GUIFONT_10) )
    'CListBox_SetExtendedSelect( hCtrl, true )   ' Shift/Ctrl range selection
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (CListBox_SetScrollBarPaintCallback is available for full custom drawing.)
    CListBox_SetScrollBarWidth( hCtrl, SCROLLBAR_WIDTH_PANEL )
    CListBox_SetScrollBarColors( hCtrl, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
'    CListBox_SetScrollBarColors( hCtrl, ghEditor.BackColorScrollBar, _
'                                 ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBarHot )

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
        pivot = ucase( AfxStrPathName( "NAMEX", arrFiles( (lo+hi)/2 ) ) )
        do
            while ucase(AfxStrPathName("NAMEX", arrFiles(i))) < pivot and i < hi: i += 1: wend
            while ucase(AfxStrPathName("NAMEX", arrFiles(j))) > Pivot and j > lo: j -= 1: wend
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
        pivot = ucase( a( (lo+hi)/2 ).wszFunctionName )
        do
            while ucase(a(i).wszFunctionName) < pivot and i < hi: i += 1: wend
            while ucase(a(j).wszFunctionName) > Pivot and j > lo: j -= 1: wend
            if i <= j then swap a(i), a(j): i += 1: j -= 1
        loop while i <= j
        if lo < j then QuickSortFuncs( a(), lo, j )
        if hi > i then QuickSortFuncs( a(), i, hi )
    end if
    
    function = 0
end function


' ========================================================================================
' ClearFunctions
' ========================================================================================
function ClearFunctions() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS
    CListBox_Clear( hCtrl )
    function = 0
end function

' ========================================================================================
' LoadFilesAndFunctions
' ========================================================================================
function LoadFilesAndFunctions() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    ' Save the topindex because we will restore it after filling the new contents
    dim as long nCurSel = CListBox_GetCurSel( hCtrl )
    dim as long nTopIndex = CListBox_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    CListBox_Clear( hCtrl )

    CListBox_BeginUpdate( hCtrl )

    dim as DWSTRING wszText

    ' All user files known to the symbol database (project + buffer scans; the
    ' toolchain include folder is excluded). This now includes include files
    ' that have never been opened in the editor. Scan filenames are UPPERCASED -
    ' normalize to on-disk casing for display and click-through.
    erase gFuncPanelFiles
    dim as long nFiles = gSymDb.EnumUserFiles( gFuncPanelFiles() )
    for i as long = 0 to nFiles - 1
        gFuncPanelFiles(i) = FilenameOriginalCase( gFuncPanelFiles(i) )
    next
    if nFiles > 0 then QuickSortFilenames( gFuncPanelFiles(), 0, nFiles - 1 )

    ' Get the functions (and sort them) for each file
    redim arrFuncs(any) as FUNCTION_NODE_TYPE
    dim rs() as SYMBOLREF

    for i as long = 0 to nFiles - 1
        wszText = AfxStrPathName( "NAMEX", gFuncPanelFiles(i) )
        CListBox_AddHeader( hCtrl, wszText, i, 0 )

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
            wszText = ltrim(arrFuncs(ii).wszFunctionName) & _
                      "%" & trim(arrFuncs(ii).wszPrototype)
            CListBox_AddString( hCtrl, wszText, i, arrFuncs(ii).nLineNumber )
        next
        erase arrFuncs

    next

    CListBox_EndUpdate( hCtrl )

    ' Restore the top index so the list displays like it did before being reset
    CListBox_SetTopIndex( hCtrl, nTopIndex )
    nCurSel = MIN( nCurSel, CListBox_GetCount(hCtrl) - 1 )
    CListBox_SetCurSel( hCtrl, nCurSel )

    function = 0
end function


' ========================================================================================
' LoadFunctionsOnly
' This will clear the current list of files in the listbox and repopulate it.
' ========================================================================================
function LoadFunctionsOnly() as long
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    ' Save the topindex because we will restore it after filling the new contents
    dim as long nCurSel = CListBox_GetCurSel( hCtrl )
    dim as long nTopIndex = CListBox_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    CListBox_Clear( hCtrl )

    CListBox_BeginUpdate( hCtrl )


    dim as DWSTRING wszText

    ' All user files known to the symbol database, then every implementation
    ' in each of them, flattened into one name-sorted list. Scan filenames are
    ' UPPERCASED - normalize to on-disk casing for display and click-through.
    erase gFuncPanelFiles
    dim as long nFiles = gSymDb.EnumUserFiles( gFuncPanelFiles() )
    for i as long = 0 to nFiles - 1
        gFuncPanelFiles(i) = FilenameOriginalCase( gFuncPanelFiles(i) )
    next
    if nFiles > 0 then QuickSortFilenames( gFuncPanelFiles(), 0, nFiles - 1 )

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
        wszText = ltrim(arrFuncs(ii).wszFunctionName) & _
                  "%" & trim(arrFuncs(ii).wszPrototype)
        CListBox_AddString( hCtrl, wszText, arrFuncs(ii).nFileIdx, arrFuncs(ii).nLineNumber )
    next
    erase arrFuncs


    CListBox_EndUpdate( hCtrl )

    ' Restore the top index so the list displays like it did before being reset
    CListBox_SetTopIndex( hCtrl, nTopIndex )
    nCurSel = MIN( nCurSel, CListBox_GetCount(hCtrl) - 1 )
    CListBox_SetCurSel( hCtrl, nCurSel )

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


