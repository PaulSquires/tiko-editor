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
#include once "clsDB2.bi"

' ========================================================================================
' Reparse any dirty files
' ========================================================================================
function frmFunctions_ReparseFiles() as long
    '  Need to re-parse any dirty files. This is especially important for QuickRun scenarios
    '  where the file is never physically saved.
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        If cbool(SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 )) orelse pDoc->UserModified then
            pDoc->bNeedsParsing = true
            pDoc->ParseDocument()
        end if
        pDoc = pDoc->pDocNext
    loop   
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
    ' Select the Function List listbox item where the ItemData holds the pDoc handle.
    if pDoc = 0 then return false
    dim as HWND hCtrl = HWND_FRMFUNCTIONS

    ' If pDoc is already selected in the listbox then simply exit
    dim as integer nCurSel = CListBox_GetCurSel(hCtrl)
    dim as integer nItemData = CListBox_GetItemData( hCtrl, nCurSel )
    dim as clsDocument ptr pDocSel = cast( clsDocument ptr, nItemData )
    if pDocSel = pDoc then return true

    dim as long nCount = CListBox_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        if CListBox_GetItemData(hCtrl, i) = cast(integer, pDoc) then 
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
                dim as integer nItemData = CListBox_GetItemData( hCtl, m->idx )
                dim as integer nLineNum = CListBox_GetItemDataExtra( hCtl, m->idx )
                dim as clsDocument ptr pDoc = cast(clsDocument ptr, nItemData)
                if pDoc then 
                    ' if the file is already showing in the Top tabs then switch to that tab.
                    if gTTabCtl.SetTabIndexByDocumentPtr( pDoc ) then
                        dim as hwnd hEdit = pDoc->hwndActiveScintilla
                        SciExec( hEdit, SCI_SETFIRSTVISIBLELINE, max(nLineNum - 3, 0), 0) 
                        SciExec( hEdit, SCI_GOTOLINE, nLineNum, 0) 
                        pDoc->CenterCurrentLine
                    else
                        ' We don't need to parse the caption for the function name because
                        ' we already have the pDoc and line number stored.
                        OpenSelectedDocument( pDoc->DiskFilename, "", nLineNum )
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
' Simple QuickSort of Filename array
' ========================================================================================
function QuickSortpDocs( pDocs() as clsDocument ptr, lo as long, hi as long ) as long
    dim as long i, j
    dim as DWSTRING pivot
    
    if lo < hi then
        i = lo : j = hi
        pivot = ucase( pDocs( (lo+hi)/2 )->DiskFilename )
        do
            while ucase(pDocs(i)->DiskFilename) < pivot and i < hi: i += 1: wend
            while ucase(pDocs(j)->DiskFilename) > Pivot and j > lo: j -= 1: wend
            if i <= j then swap pDocs(i), pDocs(j): i += 1: j -= 1
        loop while i <= j
        if lo < j then QuickSortpDocs( pDocs(), lo, j )
        if hi > i then QuickSortpDocs( pDocs(), i, hi )
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
    dim as DWSTRING wszFilename 
    
    ' Iterate all pDoc in the project/files list. Create an array and then
    ' sort it alphabetically.
    dim as long nCount = gApp.GetDocumentCount() - 1
    dim pDocs(nCount) as clsDocument ptr
    
    dim as long idx = 0
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        pDocs(idx) = pDoc
        idx += 1
        pDoc = pDoc->pDocNext
    loop

    QuickSortpDocs( pDocs(), lbound(pDocs), ubound(pDocs) )
    
    ' Get the functions (and sort them) for each file
    redim arrFuncs(any) as FUNCTION_NODE_TYPE
    
    for i as long = lbound(pDocs) to ubound(pDocs)
        wszText = AfxStrPathName( "NAMEX", pDocs(i)->DiskFilename )
        CListBox_AddHeader( hCtrl, wszText, cast(integer, pDocs(i)), 0 )

        dim pData as DB2_DATA ptr
        gdb2.dbRewind()
        do 
            pData = gdb2.dbGetNext
            if pData = 0 then exit do
            if pData->deleted then continue do
            if pData->nFileType <> DB2_FILETYPE_USERCODE then continue do
            select case pData->id 
                case DB2_FUNCTION
                    if ucase(pDocs(i)->DiskFilename) = ucase(pData->fileName) then
                        dim as DWSTRING wszNodeName = pData->ElementName
                        if len(pData->GetSet) then 
                            wszNodeName = wszNodeName & " " & pData->GetSet
                        end if
                        dim as long ub = ubound(arrFuncs) + 1
                        redim preserve arrFuncs(ub)
                        arrFuncs(ub).wszFunctionName = wszNodeName
                        arrFuncs(ub).wszPrototype = pData->CallTip
                        arrFuncs(ub).nLineNumber = pData->nLineStart
                    end if
            end select
        loop      
            
        quickSortFuncs( arrFuncs(), lbound(arrFuncs), ubound(arrFuncs) )

        for ii as long = lbound(arrFuncs) to ubound(arrFuncs)
            wszText = ltrim(arrFuncs(ii).wszFunctionName) & _
                      "%" & trim(arrFuncs(ii).wszPrototype)
            CListBox_AddString( hCtrl, wszText, cast(integer, pDocs(i)), arrFuncs(ii).nLineNumber )
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
    dim as DWSTRING wszNodeName
    
    ' Get the functions (and sort them) 
    redim arrFuncs(any) as FUNCTION_NODE_TYPE
    
    dim pData as DB2_DATA ptr
    gdb2.dbRewind()
    do 
        pData = gdb2.dbGetNext
        if pData = 0 then exit do
        if pData->deleted then continue do
        if pData->nFileType <> DB2_FILETYPE_USERCODE then continue do
        select case pData->id 
            case DB2_FUNCTION
                dim as DWSTRING wszNodeName = pData->ElementName
                if len(pData->GetSet) then
                    wszNodeName = wszNodeName & " " & pData->GetSet
                end if
                dim as long ub = ubound(arrFuncs) + 1
                redim preserve arrFuncs(ub)
                arrFuncs(ub).wszFunctionName = wszNodeName
                arrFuncs(ub).wszPrototype = pData->CallTip
                arrFuncs(ub).nLineNumber = pData->nLineStart
                arrFuncs(ub).pDoc = pData->pDoc
        end select
    loop      
            
    quickSortFuncs( arrFuncs(), lbound(arrFuncs), ubound(arrFuncs) )

    for ii as long = lbound(arrFuncs) to ubound(arrFuncs)
        wszText = ltrim(arrFuncs(ii).wszFunctionName) & _
                  "%" & trim(arrFuncs(ii).wszPrototype)
        CListBox_AddString( hCtrl, wszText, cast(integer, arrFuncs(ii).pDoc), arrFuncs(ii).nLineNumber )
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


