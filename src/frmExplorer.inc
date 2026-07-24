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
''  frmExplorer.inc
''   

#include once "frmExplorer.bi"


' ========================================================================================
' Simple QuickSort of a clsDocument ptr array by filename (moved here from
' frmFunctions.inc when the Functions panel switched to symbol-database files;
' the Explorer is now the only consumer)
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
' Expand/Collapse all Explorer Nodes
' ========================================================================================
function frmExplorer_ExpandAll() as long
    PsListBox_ExpandAll( HWND_FRMEXPLORER )
    function = 0
end function

function frmExplorer_CollapseAll() as long
    PsListBox_CollapseAll( HWND_FRMEXPLORER )
    function = 0
end function


' ========================================================================================
' Select the listbox item that matches the incoming pDoc item. Do not open nodes to
' find a possible hidden document because the user may have purposely closed a node
' and we should not re-open it automatically.
' ========================================================================================
function frmExplorer_SelectItemData( byval pDoc as clsDocument ptr ) as boolean
    ' Select the Explorer listbox item where the ItemData holds the pDoc handle.
    ' Reset any other single or multiple selections.  
    if pDoc = 0 then return false

    dim as HWND hCtrl = HWND_FRMEXPLORER

    ' If pDoc is already selected in the listbox then simply exit
    dim as integer nCurSel = PsListBox_GetCurSel(hCtrl)
    dim as integer nItemData = PsListBox_GetItemData( hCtrl, nCurSel )
    dim as clsDocument ptr pDocSel = cast( clsDocument ptr, nItemData )
    if pDocSel = pDoc then return true


    frmExplorer_UnSelectListBox()


    dim as long nCount = PsListBox_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        if PsListBox_GetItemData(hCtrl, i) = cast(integer, pDoc) then 
            ' Only attempt to reposition if the row is not currently collapsed
            if PsListBox_IsCollapsed(hCtrl, i) then return false

            ' If the pDoc is currently within the visible range of the Listbox then
            ' simply move to that row, otherwise we set the topindex and position
            ' the selection a few rows from it.
            dim as PSLISTBOX ptr pList = GetListBoxPointer(hCtrl)
            if pList = null then return false

            dim as integer nFirstRow = PsListBox_GetTopIndex(hCtrl)
            dim as integer nLastRow = nFirstRow + PsListBox_ItemsPerPage(pList)
            if (i >= nFirstRow) andalso (i <= nLastRow) then
                PsListBox_SetCurSel( hCtrl, i )
            else    
                ' Attempt to set the topindex so that the current selection appears
                ' a little more down the list rather than pinned to the top of the
                ' listbox window.
                PsListBox_SetTopIndex( hCtrl, MAX(0, i - 8) )
                PsListBox_SetCurSel( hCtrl, i )
            end if
            return true
        end if
    next

    return false   
end function

            
' ========================================================================================
' Unselect all items in the multiselect listbox and reset it to the currently
' active top tab (if any)
' ========================================================================================
function frmExplorer_UnSelectListBox() as long
    dim as HWND hCtrl = HWND_FRMEXPLORER
    dim as long nCount = PsListBox_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        PsListBox_SetSel( hCtrl, i, false )
    next

'    ' force a listbox selection for the currently active tab                   
'    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
'    frmExplorer_SelectItemData(pDoc)

    function = 0
end function


' ========================================================================================
' frmExplorer_PaintCallback
' ========================================================================================
sub frmExplorer_PaintCallback( byval p as PSLISTBOX_PAINTINFO ptr )
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

        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcIcon.right
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    end if

end sub


' ========================================================================================
' frmExplorer_MessageCallback
' ========================================================================================
function frmExplorer_MessageCallback( byval m as PSLISTBOX_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_MOUSEWHEEL
            'print "WM_MOUSEWHEEL"
            return true

        case WM_MOUSEHOVER
            'print "WM_MOUSEHOVER on idx: "; m->idx
            ' Tooltips are now on-demand via the tooltip callback; nothing to push.
            return true
            
        case WM_LBUTTONUP
            ' Attempt to show the file
            dim as HWND hCtl = GetParent(m->hList)
            if PsListBox_IsHeader( hCtl, m->idx ) = false then
                dim as integer nItemData = PsListBox_GetItemData( hCtl, m->idx )
                dim as clsDocument ptr pDoc = cast(clsDocument ptr, nItemData)
                if pDoc then 
                    ' if the file is already showing in the Top tabs then switch to that tab.
                    if gTTabCtl.SetTabIndexByDocumentPtr( pDoc ) = -1 then
                        OpenSelectedDocument( pDoc->DiskFilename )
                    end if
                end if
                frmMain_SetFocusToCurrentCodeWindow()
            end if

            ' Header collapse/expand is handled inside the control on the mouse-down,
            ' and it preserves the current selection. Return FALSE: the listbox needs
            ' its default WM_LBUTTONUP handling to release the mouse capture it took
            ' on the button-down.
            return false
    
        case WM_RBUTTONDOWN
            ' Create the popup menu
            dim as HWND hCtl = GetParent(m->hList)
            if PsListBox_IsHeader( hCtl, m->idx ) then return false

            dim as clsDocument ptr pDoc = cast(clsDocument ptr, PsListBox_GetItemData(hCtl, m->idx))
            if pDoc = null then return false
            
            ' If only 1 line has been previously selected then unselect that line
            ' and select this current line, otherwise, the menu actions on all selected lines.
            dim as long nSelCount = PsListBox_GetSelCount(hCtl)
            if nSelCount = 1 then PsListBox_SelectAll(hCtl, false)
            PsListBox_SetSel(hCtl, m->idx, true)

            ' Show the menu and return. It is a PsPopupMenu, so this does NOT block the
            ' way TrackPopupMenu did: the whole "process the selected rows" tail that
            ' used to follow this call is ExplorerContextMenu_OnSelect in
            ' modContextMenus.inc, which the control fires once the menu has closed.
            ' That preserves the ordering the old code depended on -- the selection is
            ' read AFTER dismissal, because some of the actions reload this listbox and
            ' invalidate its indexes.
            ShowExplorerContextMenu( pDoc )
            return true   ' prevent further processing that leads to WM_CONTEXTMENU

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function    


' ========================================================================================
' frmExplorer_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function frmExplorer_TooltipCallback( byval hCtl as HWND, byval row as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as integer nItemData = PsListBox_GetItemData(hCtl, row)
    dim as clsDocument ptr pDoc = cast(clsDocument ptr, nItemData)
    if pDoc then wszTooltip = pDoc->DiskFilename
    return wszTooltip
end function


' ========================================================================================
' frmExplorer_Show
' ========================================================================================
function frmExplorer_Show( byval hWndParent as HWND ) as LRESULT

    dim as HWND hCtrl = PsListBox_Create( hWndParent, IDC_FRMEXPLORER_LISTBOX )

    PsListBox_SetPaintCallback( hCtrl, @frmExplorer_PaintCallback )
    PsListBox_SetMessageCallback( hCtrl, @frmExplorer_MessageCallback )
    PsListBox_SetTooltipCallback( hCtrl, @frmExplorer_TooltipCallback )
    ' The WM_LBUTTONUP callback opens the clicked function; a double-click would
    ' deliver a second up and open it twice, so suppress double-click delivery.
    PsListBox_PreventDoubleClick( hCtrl )
    PsListBox_SetBackColor( hCtrl, ghPanel.BackColor )
    PsListBox_SetFont( hCtrl, ghFont(GUIFONT_10) )
    PsListBox_SetExtendedSelect( hCtrl, true )   ' Shift/Ctrl range selection
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (PsListBox_SetScrollBarPaintCallback is available for full custom drawing.)
    PsListBox_SetScrollBarWidth( hCtrl, SCROLLBAR_WIDTH_PANEL )
    PsListBox_SetScrollBarColors( hCtrl, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
'    PsListBox_SetScrollBarColors( hCtrl, ghEditor.BackColorScrollBar, _
'                                 ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBarHot )

    HWND_FRMEXPLORER = hCtrl

    function = 0
    
end function


' ========================================================================================
' LoadExplorerFiles
' This will clear the current list of files in the Explorer and repopulate it
' with the latest list of filenames that are stored in the hidden FunctionList treeview.
' Files are excluded from being added based on the state of the global array gConfig.Cat()
' ========================================================================================
function LoadExplorerFiles() as long
    dim as HWND hCtrl = HWND_FRMEXPLORER

    ' Hide the listbox while it is loading so that we don't get the unpainted
    ' white background from the empty listbox
    ShowWindow( hCtrl, SW_HIDE )

    ' Save the topindex because we will restore it after filling the new contents
    dim as long nCurSel = PsListBox_GetCurSel( hCtrl )
    dim as long nTopIndex = PsListBox_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    PsListBox_Clear( hCtrl )

    PsListBox_BeginUpdate( hCtrl )


    ' Add the list of filenames 
    dim as long nStart, nEnd
    dim wszText as wstring * MAX_PATH

    if gApp.IsProjectActive then
        nStart = CATINDEX_MAIN
        nEnd = ubound(gConfig.Cat)
    else   
        ' no project active so just "files"
        nStart = CATINDEX_FILES
        nEnd = CATINDEX_FILES
    end if


    redim pDocs(any) as clsDocument ptr

    for ii as long = nStart to nEnd
        wszText = gConfig.Cat(ii).wszDescription
        PsListBox_AddHeader( hCtrl, wszText )

        ' Iterate all pDoc in the project/files list. Create an array and then
        ' sort it alphabetically.
        erase pDocs
        
        dim pDoc as clsDocument ptr = gApp.pDocList
        do until pDoc = 0
            dim as DWSTRING wszFileType = pDoc->ProjectFiletype
            ' If project not active then ensure file displays under the "File" header.
            if gApp.IsProjectActive = false then wszFileType = CATINDEX_FILES
            if wszFileType = gConfig.Cat(ii).idFileType then
                dim as long ub = ubound(pDocs) + 1
                redim preserve pDocs(ub)
                pDocs(ub) = pDoc
            end if
            pDoc = pDoc->pDocNext
        loop

        QuickSortpDocs( pDocs(), lbound(pDocs), ubound(pDocs) )
    
        for i as long = lbound(pDocs) to ubound(pDocs)
            dim as DWSTRING wszText = AfxStrPathName( "NAMEX", pDocs(i)->DiskFilename )
            PsListBox_AddString( hCtrl, wszText, cast(integer, pDocs(i)), 0 )
        next   
        
    next

    PsListBox_EndUpdate( hCtrl )

    ' Restore the top index so the list displays like it did before being reset
    PsListBox_SetTopIndex( hCtrl, nTopIndex )
    nCurSel = MIN( nCurSel, PsListBox_GetCount(hCtrl) - 1 )
    PsListBox_SetCurSel( hCtrl, nCurSel )

    function = 0
end function

    
