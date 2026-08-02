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
    PsListTree_ExpandAll( HWND_FRMEXPLORER )
    function = 0
end function

function frmExplorer_CollapseAll() as long
    PsListTree_CollapseAll( HWND_FRMEXPLORER )
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
    dim as integer nCurSel = PsListTree_GetCurSel(hCtrl)
    dim as clsDocument ptr pDocSel = frmExplorer_DocFromRow( hCtrl, nCurSel )
    if pDocSel = pDoc then return true


    frmExplorer_UnSelectListBox()


    dim as long nCount = PsListTree_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        if frmExplorer_DocFromRow(hCtrl, i) = pDoc then
            ' Only attempt to reposition if the row is not currently collapsed
            if PsListTree_IsCollapsed(hCtrl, i) then return false

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
' Does this file appear in the Explorer?
'
' The one place the Explorer's display rule is stated, so a panel that wants to mirror it
' (the Functions list does) cannot drift from what LoadExplorerFiles actually renders. Two
' conditions, both of which that loop imposes: the file must be a document in the workspace
' at all, and its ProjectFiletype must name one of the categories the loop walks --
' CATINDEX_FILES is deliberately outside that range, so a document under it is displayed
' nowhere and belongs in no mirror either.
' ========================================================================================
function frmExplorer_IsFileDisplayed( byval wszFilename as DWSTRING ) as boolean
    dim as clsDocument ptr pDoc = gApp.GetDocumentPtrByFilename( wszFilename )
    if pDoc = 0 then return false

    dim as DWSTRING wszFileType = pDoc->ProjectFiletype
    for ii as long = CATINDEX_MAIN to ubound(gConfig.Cat)
        if wszFileType = gConfig.Cat(ii).idFileType then return true
    next
    return false
end function


' ========================================================================================
' Unselect all items in the multiselect listbox and reset it to the currently
' active top tab (if any)
' ========================================================================================
function frmExplorer_UnSelectListBox() as long
    dim as HWND hCtrl = HWND_FRMEXPLORER
    dim as long nCount = PsListTree_GetCount(hCtrl)
    for i as long = 0 to nCount - 1
        PsListTree_SetSel( hCtrl, i, false )
    next

'    ' force a listbox selection for the currently active tab                   
'    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
'    frmExplorer_SelectItemData(pDoc)

    function = 0
end function


' ========================================================================================
' The kind of an Explorer row (see the enum's note in frmExplorer.bi).
'
' Invalid rows read EXPKIND_NONE, which is also what an un-tagged row reads, so a caller
' never has to range-check the row first.
' ========================================================================================
function frmExplorer_KindFromRow( byval hCtl as HWND, byval row as integer ) as EXPLORER_ROWKIND
    if row < 0 then return EXPKIND_NONE
    return cast( EXPLORER_ROWKIND, PsListTree_GetItemDataExtra( hCtl, row ) )
end function


' ========================================================================================
' The document a row refers to, or NULL if it refers to none.
'
' EVERY read of an Explorer row's itemData as a document must go through this. A row's
' itemData means whatever its KIND says it means -- a category index, a folder index, a
' document pointer -- so casting it without consulting the kind is reading one type as
' another. Testing the kind here rather than at each call site is what makes that safe by
' construction: the previous scheme (a bare sentinel value in itemData) relied on every
' reader remembering to exclude it, and shipped a GPF on hover when the tooltip callback
' was the one that did not.
' ========================================================================================
function frmExplorer_DocFromRow( byval hCtl as HWND, byval row as integer ) as clsDocument ptr
    if frmExplorer_KindFromRow( hCtl, row ) <> EXPKIND_FILE then return 0
    return cast( clsDocument ptr, PsListTree_GetItemData( hCtl, row ) )
end function


' ========================================================================================
' frmExplorer_PaintCallback
' ========================================================================================
sub frmExplorer_PaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )
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

    ' PSLISTTREE_PAINTINFO carries the model row but no itemData and no control handle, so
    ' the row descriptor is fetched by index. Safe here only because this painter serves
    ' exactly one control; a shared painter would need a per-tree wrapper naming its HWND.
    dim as EXPLORER_ROWKIND kind = frmExplorer_KindFromRow( HWND_FRMEXPLORER, p->itemID )

    ' indent the text based on its type
    if kind = EXPKIND_ROOT then
        ' Keyed on the row KIND, not on p->isHeader -- a root group is a plain tree parent
        ' now, so isHeader is never true here (see LoadExplorerFiles).
        rcIcon.right = rcIcon.left + nIconWidth
        ' The chevron is painted only when there is actually a subtree to fold. An empty
        ' group offering a control that does nothing is the wart the header row had; it is
        ' also what PsListTree_CanCollapse already believes -- it refuses a childless node,
        ' so under ExpandAll/CollapseAll an empty group's chevron would sit at odds with
        ' every other group's. The caption's x is unchanged either way, so the captions of
        ' populated and empty groups still line up.
        if p->hasChildren then
            p->b->SetFont( ghFont(SYMBOLFONT_10) )
            dim as DWSTRING wszIcon = iif(p->isCollapsed, wszIconChevronRight, wszIconChevronDown)
            p->b->PaintText( wszIcon, @rcIcon, DT_CENTER )
        end if

        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcIcon.right
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    elseif kind = EXPKIND_PROMOTE then
        ' The promotion row reads as an action, not a file: accent-coloured, no document
        ' glyph, and indented to the header's text column rather than the file column.
        p->b->SetForeColor( ghEditor.ForeColorKeyword )
        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcText.left + nIconWidth
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
function frmExplorer_MessageCallback( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_MOUSEWHEEL
            'print "WM_MOUSEWHEEL"
            return true

        case WM_MOUSEHOVER
            'print "WM_MOUSEHOVER on idx: "; m->idx
            ' Tooltips are now on-demand via the tooltip callback; nothing to push.
            return true
            
        case WM_LBUTTONDOWN
            ' Whole-row collapse/expand for a group, implemented HERE because the control no
            ' longer does it. A root group used to be an AddHeader row, which the control
            ' toggles on a click anywhere in it -- but it does that before the callback runs
            ' and so cannot be vetoed, which is unusable once the row carries an icon. As a
            ' plain parent it toggles only via a twisty (not shown here), so the gesture is
            ' reinstated by hand and the icon zone can be carved out of it later.
            '
            ' Guarded on hasChildren to agree with the painter: no chevron is drawn on an
            ' empty group, so clicking one must not silently flip a state nothing displays.
            dim as HWND hCtlDn = GetParent(m->hList)
            if frmExplorer_KindFromRow( hCtlDn, m->idx ) = EXPKIND_ROOT then
                if PsListTree_HasChildren( hCtlDn, m->idx ) then
                    PsListTree_ToggleRow( hCtlDn, m->idx )
                end if
                return true
            end if
            return false

        case WM_LBUTTONUP
            ' Attempt to show the file
            dim as HWND hCtl = GetParent(m->hList)
            if frmExplorer_KindFromRow( hCtl, m->idx ) <> EXPKIND_ROOT then
                ' The one reader that cares WHICH non-document row it is on. It asks for the
                ' kind and never touches itemData, so there is nothing here to mis-cast.
                if frmExplorer_KindFromRow( hCtl, m->idx ) = EXPKIND_PROMOTE then
                    OnCommand_ProjectSave( GetParent(hCtl), true )   ' Save As
                    return false
                end if

                dim as clsDocument ptr pDoc = frmExplorer_DocFromRow( hCtl, m->idx )
                if pDoc then
                    ' if the file is already showing in the Top tabs then switch to that tab.
                    if gTTabCtl.SetTabIndexByDocumentPtr( pDoc ) = -1 then
                        OpenSelectedDocument( pDoc->DiskFilename )
                    end if
                end if
                frmMain_SetFocusToCurrentCodeWindow()
            end if

            ' Group collapse/expand happened on the mouse-down above. Return FALSE: the
            ' listbox needs its default WM_LBUTTONUP handling to release the mouse capture
            ' it took on the button-down.
            return false
    
        case WM_RBUTTONDOWN
            ' Create the popup menu
            dim as HWND hCtl = GetParent(m->hList)
            if frmExplorer_KindFromRow( hCtl, m->idx ) = EXPKIND_ROOT then return false

            ' No context menu on the promotion row -- every item on it is a file action.
            ' DocFromRow returns NULL for it, so the existing null test covers that too.
            dim as clsDocument ptr pDoc = frmExplorer_DocFromRow( hCtl, m->idx )
            if pDoc = null then return false
            
            ' If only 1 line has been previously selected then unselect that line
            ' and select this current line, otherwise, the menu actions on all selected lines.
            dim as long nSelCount = PsListTree_GetSelCount(hCtl)
            if nSelCount = 1 then PsListTree_SelectAll(hCtl, false)
            PsListTree_SetSel(hCtl, m->idx, true)

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
function frmExplorer_TooltipCallback( byval hCtl as HWND, byval row as integer, byval col as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as clsDocument ptr pDoc = frmExplorer_DocFromRow( hCtl, row )
    if pDoc then wszTooltip = pDoc->DiskFilename
    return wszTooltip
end function


' ========================================================================================
' frmExplorer_Show
' ========================================================================================
function frmExplorer_Show( byval hWndParent as HWND ) as LRESULT

    dim as HWND hCtrl = PsListTree_Create( hWndParent, IDC_FRMEXPLORER_LISTBOX )

    PsListTree_SetPaintCallback( hCtrl, @frmExplorer_PaintCallback )
    PsListTree_SetMessageCallback( hCtrl, @frmExplorer_MessageCallback )
    PsListTree_SetTooltipCallback( hCtrl, @frmExplorer_TooltipCallback )
    ' Owner-drawn tips. The text policy above is untouched -- PsTipHost switches how a
    ' tip is drawn and driven, never what it says.
    PsListTree_SetTooltipMode( hCtrl, PSTIP_MODE_PS )
    ' The WM_LBUTTONUP callback opens the clicked function; a double-click would
    ' deliver a second up and open it twice, so suppress double-click delivery.
    PsListTree_PreventDoubleClick( hCtrl )
    PsListTree_SetBackColor( hCtrl, ghPanel.BackColor )
    PsListTree_SetFont( hCtrl, ghFont(GUIFONT_10) )
    PsListTree_SetExtendedSelect( hCtrl, true )   ' Shift/Ctrl range selection
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (PsListTree_SetScrollBarPaintCallback is available for full custom drawing.)
    PsListTree_SetScrollBarWidth( hCtrl, SCROLLBAR_WIDTH_PANEL )
    PsListTree_SetScrollBarColors( hCtrl, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
'    PsListTree_SetScrollBarColors( hCtrl, ghEditor.BackColorScrollBar, _
'                                 ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBarHot )

    HWND_FRMEXPLORER = hCtrl

    function = 0
    
end function


' ========================================================================================
' The gConfig.Cat() index a document is displayed under, or -1 for none.
'
' The same lookup LoadExplorerFiles performs by walking the categories, extracted so that
' the folder code can answer "which group is this file in" without a loop of its own --
' folder identity is (category, path), so nothing about a file's folder means anything until
' its category is known.
' ========================================================================================
function frmExplorer_CatIndexForDoc( byval pDoc as clsDocument ptr ) as long
    if pDoc = 0 then return -1
    dim as DWSTRING wszFileType = pDoc->ProjectFiletype
    for ii as long = CATINDEX_MAIN to ubound(gConfig.Cat)
        if wszFileType = gConfig.Cat(ii).idFileType then return ii
    next
    return -1
end function


' ========================================================================================
' Forget any document folder that does not name a folder that actually exists.
'
' A FILE WHOSE FOLDER CANNOT BE RESOLVED WOULD OTHERWISE RENDER NOWHERE. The tree is built
' by walking folders and asking which files sit in each, so a file naming a folder that is
' not in the table is never asked for by anybody -- it disappears from the Explorer while
' remaining in the workspace, which is the same failure mode the FileType=0 repair in
' clsConfig exists to prevent.
'
' Three ways to arrive here, none of them exotic: a hand-edited or older .tiko, a folder the
' user deleted while a file still pointed at it, and -- the routine one -- a file whose
' category changed, since a folder under Module is not a folder under Normal. Clearing on
' the last of those is the wanted behaviour rather than a repair: the file lands at its new
' group's top level.
'
' Run from LoadExplorerFiles rather than from the project loader, so that every path which
' can invalidate a folder is covered without each having to remember to call it.
' ========================================================================================
sub frmExplorer_NormalizeDocFolders()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if len(pDoc->docData.wszFolder) then
            dim as long catIndex = frmExplorer_CatIndexForDoc( pDoc )
            if ProjectFolders_Exists( catIndex, pDoc->docData.wszFolder ) = false then
                pDoc->docData.wszFolder = ""
            end if
        end if
        pDoc = pDoc->pDocNext
    loop
end sub


' ========================================================================================
' Emit one level of a group's tree beneath nParentRow: the folders directly inside
' wszParentPath, each recursed into, and then the files that sit at this level.
'
' FOLDERS BEFORE FILES, each half alphabetical. Ordering is derived on every load rather
' than persisted -- drag/drop chooses which folder a file lands in, never where it sits
' among its siblings, so there is no manual order to remember.
'
' Recursion depth is the folder depth the user created, which is unbounded by design.
' AddNode appends as the parent's LAST child, so recursing into a folder before adding the
' next sibling still lays the siblings out in order.
' ========================================================================================
sub frmExplorer_AddFolderLevel( byval hCtrl as HWND, byval catIndex as long, _
                                byval wszParentPath as DWSTRING, byval nParentRow as integer )
    ' --- the folders directly inside this level -------------------------------------------
    ' Collected as table indices, then insertion-sorted by leaf name. A folder tree is small
    ' enough that the sort's cost is irrelevant and its simplicity is not.
    redim nFolders(any) as long
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex <> catIndex then continue for
        if ucase( ProjectFolders_ParentPath( gProjectFolders(i).wszPath ) ) <> ucase( wszParentPath ) then continue for
        dim as long ub = ubound(nFolders) + 1
        redim preserve nFolders(ub)
        nFolders(ub) = i
    next

    for i as long = lbound(nFolders) + 1 to ubound(nFolders)
        dim as long nHold = nFolders(i)
        dim as DWSTRING wszHold = ucase( ProjectFolders_LeafName( gProjectFolders(nHold).wszPath ) )
        dim as long j = i - 1
        do while j >= lbound(nFolders)
            if ucase( ProjectFolders_LeafName( gProjectFolders(nFolders(j)).wszPath ) ) <= wszHold then exit do
            nFolders(j + 1) = nFolders(j)
            j -= 1
        loop
        nFolders(j + 1) = nHold
    next

    for i as long = lbound(nFolders) to ubound(nFolders)
        dim as long nIdx = nFolders(i)
        ' The row shows the LEAF name; the full path is recovered from the table via the
        ' itemData index, so nothing has to parse the caption back into a path.
        dim as integer nRow = PsListTree_AddNode( hCtrl, nParentRow, _
                                  ProjectFolders_LeafName( gProjectFolders(nIdx).wszPath ), _
                                  nIdx, EXPKIND_FOLDER )
        frmExplorer_AddFolderLevel( hCtrl, catIndex, gProjectFolders(nIdx).wszPath, nRow )
    next

    ' --- the files at this level ----------------------------------------------------------
    redim pDocs(any) as clsDocument ptr
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->ProjectFiletype = gConfig.Cat(catIndex).idFileType then
            if ucase( pDoc->docData.wszFolder ) = ucase( wszParentPath ) then
                dim as long ub = ubound(pDocs) + 1
                redim preserve pDocs(ub)
                pDocs(ub) = pDoc
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    QuickSortpDocs( pDocs(), lbound(pDocs), ubound(pDocs) )

    for i as long = lbound(pDocs) to ubound(pDocs)
        ' AddNode, not AddString: AddString derives its level from the PRECEDING row and
        ' only ever produces level 1 when that row is a header or is itself level 1. With
        ' the group now a plain parent, AddString would put every file at level 0 -- a flat
        ' list with no subtree to collapse -- and could not express a nested folder at all.
        PsListTree_AddNode( hCtrl, nParentRow, _
                            AfxStrPathName( "NAMEX", pDocs(i)->DiskFilename ), _
                            cast(integer, pDocs(i)), EXPKIND_FILE )
    next
end sub


' ========================================================================================
' LoadExplorerFiles
' Clears the Explorer and repopulates it from gApp.pDocList, grouped under one header per
' gConfig.Cat() entry from CATINDEX_MAIN up. NOTHING IS EXCLUDED -- every document in the
' workspace appears exactly once, under the category its ProjectFileType names. (This used
' to claim files were filtered by a Cat().bShow flag; that flag was written in two places,
' read in none, and has been deleted.)
' ========================================================================================
function LoadExplorerFiles() as long
    dim as HWND hCtrl = HWND_FRMEXPLORER

    ' Hide the listbox while it is loading so that we don't get the unpainted
    ' white background from the empty listbox
    ShowWindow( hCtrl, SW_HIDE )

    ' Save the topindex because we will restore it after filling the new contents
    dim as long nCurSel = PsListTree_GetCurSel( hCtrl )
    dim as long nTopIndex = PsListTree_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    PsListTree_Clear( hCtrl )

    PsListTree_BeginUpdate( hCtrl )


    ' Add the list of filenames 
    dim as long nStart, nEnd
    dim wszText as wstring * MAX_PATH

    ' Every workspace groups by category now, so the range is fixed. CATINDEX_FILES (the
    ' old single "File" header) is skipped: it stays in the Cat() array -- two menu paths
    ' index that array by an offset from a command id, and the table is persisted in
    ' settings.ini besides -- but nothing is ever displayed under it.
    nStart = CATINDEX_MAIN
    nEnd = ubound(gConfig.Cat)


    ' While the workspace is untitled, offer to make it a real project. Pinned at index 0,
    ' above every header, and present only until the user names it -- IsProjectNamed is the
    ' whole condition, so it disappears the moment Save As succeeds.
    if gApp.IsProjectNamed() = false then
        PsListTree_AddString( hCtrl, L(550,"Save as Project..."), 0, EXPKIND_PROMOTE )
    end if

    ' Before any row is emitted: a file whose folder cannot be resolved must be pulled back
    ' to its group's top level, or the walk below will simply never ask for it and it will
    ' vanish from the list while staying in the workspace.
    frmExplorer_NormalizeDocFolders()

    for ii as long = nStart to nEnd
        ' A root group is an AddNode PARENT, not an AddHeader row. The control toggles a
        ' header on a click ANYWHERE in it, and does so BEFORE offering the message callback
        ' (PsListTree.inc, WM_LBUTTONDOWN) -- so the callback cannot veto it, and the
        ' add-folder icon this row is about to carry would collapse the group on every
        ' click. As a plain parent nothing toggles by itself and the host owns the gesture.
        '
        ' Non-selectable for the second half of that: WM_LBUTTONDOWN computes
        ' canDrag = bDragReorder andalso bSelectable, and a root group must never be
        ' draggable. Headers got that exclusion for free; plain parents do not. Nothing
        ' depended on selecting a group row -- the context menu and the up-handler both bail
        ' on them -- and a non-selectable row still reports its index to the message
        ' callback, so the host toggle is unaffected.
        wszText = gConfig.Cat(ii).wszDescription
        dim as integer nRootRow = PsListTree_AddNode( hCtrl, -1, wszText, ii, EXPKIND_ROOT )
        PsListTree_SetRowSelectable( hCtrl, nRootRow, false )

        ' The group's whole subtree, starting at its top level (""). A group that allows no
        ' folders simply has none in the table, so this needs no special case for Main and
        ' Resource -- it emits their files and recurses into nothing.
        frmExplorer_AddFolderLevel( hCtrl, ii, "", nRootRow )
    next

    PsListTree_EndUpdate( hCtrl )

    ' Restore the top index so the list displays like it did before being reset
    PsListTree_SetTopIndex( hCtrl, nTopIndex )
    nCurSel = MIN( nCurSel, PsListTree_GetCount(hCtrl) - 1 )
    PsListTree_SetCurSel( hCtrl, nCurSel )

    function = 0
end function

    
