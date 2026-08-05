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


' Which action icon the cursor is currently over, and on which row.
'
' The tooltip callback is handed (row, col) and no x, so it cannot tell the two icons of one
' row apart by itself -- and col is no help either, since this list defines no columns and
' PsListTree_ColFromX therefore answers 0 for the whole width. The zone is tracked here on
' the mouse move instead and consulted when the tip is resolved. The row is stored alongside
' it so a stale zone can never be applied to a different row.
dim shared as EXPLORER_ICONHIT gExpTipIcon = EXPICON_NONE
dim shared as long gExpTipRow = -1

' The row the folder context menu was opened on. PsPopupMenu is not modal, so its select
' callback runs after the menu closes -- and every command on that menu rebuilds the tree,
' so the row has to be captured when the menu is raised rather than looked up afterwards.
dim shared as long gExpMenuRow = -1

' TIKO_EXPLORER_TIPTRACE=1 -- every icon-zone transition the mouse move detects. The tip's
' own behaviour is driven by a 100 ms clock in another module, so "the tip did not change"
' has two completely different causes -- the zone was never detected, or it was detected and
' the re-resolve did not take -- and only a trace at this seam tells them apart.
dim shared as boolean gExpTipTrace = false




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
' Where a row's action icons sit, and therefore which ones it has.
'
' THE ONE IMPLEMENTATION. The painter draws from these rects and the hit test reads them; a
' second copy of the arithmetic in either place is a guarantee that a click will eventually
' land somewhere other than where the icon was drawn. An empty rect means "this row has no
' such icon", so which icons a row offers is expressed by the same call that places them and
' the two cannot disagree either.
'
' Rules: a root group offers Add only if its category takes folders at all (Main and
' Resource hold one file each, so a folder under them could contain nothing useful). A user
' folder offers both. Everything else -- files, the promotion row -- offers neither.
'
' Icons are right-aligned, Delete outermost. The rects span the row's full height, so the
' hit test only ever has to compare x.
' ========================================================================================
' ========================================================================================
' The chevron column for a row: one icon width, offset by the row's depth.
'
' Shared by the painter and the hit test for the same reason the action icons' rects are --
' the chevron is drawn here and clicked here, and two copies of "level * width" would
' eventually disagree. The control's own twisty is deliberately not used
' (PsListTree_ShowTwisty stays off): it would draw a second glyph beside this one and own a
' hit rect the host could not see, so the two toggles would fight.
' ========================================================================================
sub frmExplorer_TwistyRect( byval hCtl as HWND, byval row as integer, byref rcRow as RECT, byref rcOut as RECT )
    SetRectEmpty( @rcOut )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub
    dim as long nIconWidth = pWindow->ScaleX(EXPLORER_INDENT_UNITS)
    rcOut.left   = rcRow.left + ( PsListTree_GetLevel( hCtl, row ) * nIconWidth )
    rcOut.right  = rcOut.left + nIconWidth
    rcOut.top    = rcRow.top
    rcOut.bottom = rcRow.bottom
end sub


sub frmExplorer_IconRects( byval hCtl as HWND, byval row as integer, byref rcRow as RECT, _
                           byref rcAdd as RECT, byref rcRen as RECT, byref rcDel as RECT )
    SetRectEmpty( @rcAdd )
    SetRectEmpty( @rcRen )
    SetRectEmpty( @rcDel )

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    dim as boolean bWantAdd = false, bWantRen = false, bWantDel = false
    select case frmExplorer_KindFromRow( hCtl, row )
        case EXPKIND_ROOT
            ' A group can be added to but never renamed or deleted: its caption comes from
            ' gConfig.Cat() and its existence is what every file's ProjectFileType names.
            bWantAdd = ProjectFolders_CatAllowsFolders( PsListTree_GetItemData( hCtl, row ) )
        case EXPKIND_FOLDER
            bWantAdd = true
            bWantRen = true
            bWantDel = true
    end select
    if (bWantAdd = false) andalso (bWantRen = false) andalso (bWantDel = false) then exit sub

    dim as long nIcon = pWindow->ScaleX(22)
    dim as long nPad  = pWindow->ScaleX(2)
    dim as long xRight = rcRow.right - nPad

    ' Laid out from the RIGHT inwards, so the reading order left-to-right ends up
    ' Add, Rename, Delete. Each row right-aligns its OWN set, so a group row -- which has
    ' only Add -- puts it at the margin rather than three columns in where a folder's Add
    ' sits. That is deliberate: a ragged left edge across rows with different icon counts
    ' would read worse than a straight right one.
    if bWantDel then
        SetRect( @rcDel, xRight - nIcon, rcRow.top, xRight, rcRow.bottom )
        xRight = rcDel.left
    end if
    if bWantRen then
        SetRect( @rcRen, xRight - nIcon, rcRow.top, xRight, rcRow.bottom )
        xRight = rcRen.left
    end if
    if bWantAdd then
        SetRect( @rcAdd, xRight - nIcon, rcRow.top, xRight, rcRow.bottom )
    end if
end sub


' ========================================================================================
' Which icon, if any, sits under x on this row.
'
' The row rect is rebuilt from the SURFACE's client rect, which is the same window and the
' same GetClientRect the control's own paint loop and hit test both read (PsListTree.inc) --
' so the x span here is the x span the icons were drawn into, by construction rather than by
' the two happening to agree. Only x is compared: the icons span the row's full height, so
' being on the row at all is already established by the caller having a row index.
' ========================================================================================
function frmExplorer_IconHitTest( byval hCtl as HWND, byval row as integer, byval x as long ) as EXPLORER_ICONHIT
    dim as HWND hSurf = GetDlgItem( hCtl, IDC_FRMEXPLORER_LISTBOX )
    if hSurf = 0 then return EXPICON_NONE

    dim as RECT rcRow
    GetClientRect( hSurf, @rcRow )
    rcRow.top = 0
    rcRow.bottom = PsListTree_GetRowHeight( hCtl )

    dim as RECT rcAdd, rcRen, rcDel
    frmExplorer_IconRects( hCtl, row, rcRow, rcAdd, rcRen, rcDel )

    if IsRectEmpty( @rcDel ) = 0 then
        if (x >= rcDel.left) andalso (x < rcDel.right) then return EXPICON_DELETE
    end if
    if IsRectEmpty( @rcRen ) = 0 then
        if (x >= rcRen.left) andalso (x < rcRen.right) then return EXPICON_RENAME
    end if
    if IsRectEmpty( @rcAdd ) = 0 then
        if (x >= rcAdd.left) andalso (x < rcAdd.right) then return EXPICON_ADD
    end if
    return EXPICON_NONE
end function


' ========================================================================================
' The folder path a row denotes, and the category it belongs to.
'
' A ROOT row denotes its group's top level -- an empty path, which is not a folder but IS a
' legal container to create one in. A FOLDER row denotes its own path. Anything else answers
' with catIndex -1, which every caller treats as "not a place a folder can go".
'
' Resolving through the table by index rather than by parsing the caption is what lets a
' folder's row show only its leaf name while still knowing its full path.
' ========================================================================================
function frmExplorer_FolderPathFromRow( byval hCtl as HWND, byval row as integer, byref catIndex as long ) as DWSTRING
    catIndex = -1
    select case frmExplorer_KindFromRow( hCtl, row )
        case EXPKIND_ROOT
            catIndex = PsListTree_GetItemData( hCtl, row )
            return ""
        case EXPKIND_FOLDER
            dim as long idx = PsListTree_GetItemData( hCtl, row )
            if idx < lbound(gProjectFolders) then return ""
            if idx > ubound(gProjectFolders) then return ""
            catIndex = gProjectFolders(idx).catIndex
            return gProjectFolders(idx).wszPath
    end select
    return ""
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

    dim as long nIconWidth = pWindow->ScaleX(EXPLORER_INDENT_UNITS)
    
    dim as DWSTRING wszCaption = p->wszCaption

    ' PSLISTTREE_PAINTINFO carries the model row but no itemData and no control handle, so
    ' the row descriptor is fetched by index. Safe here only because this painter serves
    ' exactly one control; a shared painter would need a per-tree wrapper naming its HWND.
    dim as EXPLORER_ROWKIND kind = frmExplorer_KindFromRow( HWND_FRMEXPLORER, p->itemID )

    ' Action icons, on the hot row only -- a column of them down every group would compete
    ' with the filenames for attention and read as chrome rather than as a control. Placed
    ' BEFORE the caption is drawn so the caption can be clipped short of them: a long folder
    ' name would otherwise run underneath, and DrawText ellipsizes without reporting it.
    dim as RECT rcAdd, rcRen, rcDel
    if p->isHot then
        frmExplorer_IconRects( HWND_FRMEXPLORER, p->itemID, p->rc, rcAdd, rcRen, rcDel )
    end if
    dim as long xIconsLeft = p->rc.right
    if IsRectEmpty( @rcAdd ) = 0 then xIconsLeft = MIN( xIconsLeft, rcAdd.left )
    if IsRectEmpty( @rcRen ) = 0 then xIconsLeft = MIN( xIconsLeft, rcRen.left )
    if IsRectEmpty( @rcDel ) = 0 then xIconsLeft = MIN( xIconsLeft, rcDel.left )
    rcText.right = xIconsLeft

    ' Depth indent, derived from the row's level rather than from the control (SetTreeIndent
    ' is deliberately left off). One icon column per level reproduces the pre-folders layout
    ' exactly at the only two depths that existed then -- a group at level 0 starts at the
    ' left edge, a file at level 1 starts one column in, which is where its dot already was --
    ' and generalizes it to any depth for free.
    dim as long nIndent = p->level * nIconWidth
    rcIcon.left  = p->rc.left + nIndent
    rcIcon.right = rcIcon.left + nIconWidth
    rcText.left  = rcIcon.right

    if (kind = EXPKIND_ROOT) orelse (kind = EXPKIND_FOLDER) then
        ' Keyed on the row KIND, not on p->isHeader -- a root group is a plain tree parent
        ' now, so isHeader is never true here (see LoadExplorerFiles). Groups and folders
        ' paint identically apart from their indent; a folder IS a group one level in.
        '
        ' The chevron is painted only when there is actually a subtree to fold. A container
        ' offering a control that does nothing is the wart the header row had, and it is
        ' also what PsListTree_CanCollapse already believes -- it refuses a childless node,
        ' so under ExpandAll/CollapseAll an empty group's chevron would sit at odds with
        ' every other one's. The caption's x is unchanged either way, so populated and empty
        ' containers still line their captions up.
        if p->hasChildren then
            p->b->SetFont( ghFont(SYMBOLFONT_10) )
            dim as DWSTRING wszIcon = iif(p->isCollapsed, wszIconChevronRight, wszIconChevronDown)
            p->b->PaintText( wszIcon, @rcIcon, DT_CENTER )
        end if
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    elseif kind = EXPKIND_PROMOTE then
        ' The promotion row reads as an action, not a file: accent-coloured, no document
        ' glyph, and indented to the group's text column rather than the file column.
        p->b->SetForeColor( theme.editor.keyword1.forecolor )
        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = p->rc.left + nIconWidth
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    else
        ' This would be a regular file.
        p->b->SetFont( ghFont(GUIFONT_10) )  ' use GUI rather than SYMBOL b/c of dot icon
        p->b->PaintText( wszIconDocument, @rcIcon, DT_CENTER )
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    end if

    ' The action icons last, so nothing the branches above drew can overlap them. Painted in
    ' the row's own foreground colour, which is already the hot colour -- they only appear on
    ' the hot row, so there is no idle appearance for them to have.
    if IsRectEmpty( @rcAdd ) = 0 then
        p->b->SetFont( ghFont(SYMBOLFONT_10) )
        p->b->SetForeColor( foreclr )
        p->b->PaintText( wszIconNewFolder, @rcAdd, DT_CENTER )
    end if
    if IsRectEmpty( @rcRen ) = 0 then
        p->b->SetFont( ghFont(SYMBOLFONT_10) )
        p->b->SetForeColor( foreclr )
        p->b->PaintText( wszIconRename, @rcRen, DT_CENTER )
    end if
    if IsRectEmpty( @rcDel ) = 0 then
        p->b->SetFont( ghFont(SYMBOLFONT_10) )
        p->b->SetForeColor( foreclr )
        p->b->PaintText( wszIconTrash, @rcDel, DT_CENTER )
    end if

end sub


' ========================================================================================
' Create a folder inside the container this row denotes, and open its editor.
'
' The name is provisional: the folder is created with a default caption and the in-place
' editor is opened on it immediately, so typing over it is the naming gesture rather than a
' separate dialog. A user who presses Escape keeps the default name -- the folder is already
' real by then, which is deliberate: the alternative is a folder that appears and then
' silently vanishes, and Delete is one click away on the same row.
'
' The default name is made UNIQUE among its siblings first, because ProjectFolders_Add is
' idempotent: a second "New Folder" in the same place would otherwise return the existing
' one's index and appear to do nothing at all.
' ========================================================================================
sub frmExplorer_NewFolder( byval hCtl as HWND, byval row as integer )
    dim as long catIndex
    dim as DWSTRING wszParent = frmExplorer_FolderPathFromRow( hCtl, row, catIndex )
    if catIndex < 0 then exit sub
    if ProjectFolders_CatAllowsFolders( catIndex ) = false then exit sub

    dim as DWSTRING wszBase = L(462, "New Folder")
    dim as DWSTRING wszName = wszBase
    dim as long nTry = 2
    do while ProjectFolders_Exists( catIndex, ProjectFolders_Combine( wszParent, wszName ) )
        wszName = wszBase & " (" & str(nTry) & ")"
        nTry += 1
    loop

    dim as DWSTRING wszNew = ProjectFolders_Combine( wszParent, wszName )
    if ProjectFolders_Add( catIndex, wszNew ) < 0 then exit sub

    ' The container has to be open or the new row is created inside a collapsed subtree and
    ' the editor opens on something the user cannot see.
    if PsListTree_IsCollapsed( hCtl, row ) then PsListTree_ExpandRow( hCtl, row )

    LoadExplorerFiles()

    ' Rebuilding invalidated every row index, so the new row is found by what it IS rather
    ' than by remembering where it was going to be.
    dim as long nIdx = ProjectFolders_Find( catIndex, wszNew )
    for i as long = 0 to PsListTree_GetCount( hCtl ) - 1
        if frmExplorer_KindFromRow( hCtl, i ) <> EXPKIND_FOLDER then continue for
        if PsListTree_GetItemData( hCtl, i ) = nIdx then
            PsListTree_SetCurSel( hCtl, i )
            PsListTree_BeginEdit( hCtl, i, 0 )
            exit for
        end if
    next
end sub


' ========================================================================================
' Delete the folder this row denotes. Its contents move up one level.
'
' Nothing is destroyed: child files AND child folders re-attach to the deleted folder's
' parent, which is a dissolve rather than a delete and is why no confirmation is asked for.
' Root groups are unreachable from here -- they carry no delete icon -- which is what keeps
' the five categories permanent.
'
' The path is resolved BEFORE the table is touched. ProjectFolders_RemoveAt compacts, so a
' row's stored folder index is stale the instant anything is removed.
' ========================================================================================
sub frmExplorer_DeleteFolder( byval hCtl as HWND, byval row as integer )
    if frmExplorer_KindFromRow( hCtl, row ) <> EXPKIND_FOLDER then exit sub

    dim as long catIndex
    dim as DWSTRING wszPath = frmExplorer_FolderPathFromRow( hCtl, row, catIndex )
    if catIndex < 0 then exit sub
    if PsLen(wszPath) = 0 then exit sub

    ' Dissolving IS rebasing onto the parent: descendants lose one level, documents follow,
    ' and the folder's own record maps onto its parent and is dropped as a duplicate. One
    ' implementation shared with rename rather than a second path walk that could disagree.
    ProjectFolders_Rebase( catIndex, wszPath, ProjectFolders_ParentPath( wszPath ) )

    LoadExplorerFiles()
end sub


' ========================================================================================
' Open the in-place editor on a folder row.
'
' Its own entry point rather than a bare BeginEdit at each call site, because there are
' three of them now -- the rename icon, the context menu and F2 -- and each has to select
' the row first: BeginEdit is happy to open an editor on a row that is not the current one,
' which leaves the selection highlight somewhere else entirely while the user types.
' ========================================================================================
sub frmExplorer_RenameFolder( byval hCtl as HWND, byval row as integer )
    if frmExplorer_KindFromRow( hCtl, row ) <> EXPKIND_FOLDER then exit sub
    PsListTree_SetCurSel( hCtl, row )
    PsListTree_BeginEdit( hCtl, row, 0 )
end sub


' ========================================================================================
' F2, on its way to the Explorer.
'
' F2 IS A BOUND ACCELERATOR IN TIKO -- IDM_BOOKMARKNEXT, "Goto the next bookmark" -- so
' TranslateAccelerator consumes it before the control's own VK_F2 handler can ever run, and
' rename by keyboard simply did nothing. This claims it back, but ONLY while the focus is
' inside the Explorer and only on a folder row, so the bookmark binding keeps working
' everywhere else. Called from frmMain's pump AHEAD of the accelerator table, which is the
' only place the interception can happen.
' ========================================================================================
function frmExplorer_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if pMsg->message <> WM_KEYDOWN then return false
    if pMsg->wParam <> VK_F2 then return false
    if HWND_FRMEXPLORER = 0 then return false

    ' Focus sits on the control's SURFACE, a child of the container, so the test is
    ' "inside the Explorer" rather than "is the Explorer".
    dim as HWND hFocus = GetFocus()
    if hFocus = 0 then return false
    if (hFocus <> HWND_FRMEXPLORER) andalso (IsChild( HWND_FRMEXPLORER, hFocus ) = 0) then return false

    dim as long row = PsListTree_GetCurSel( HWND_FRMEXPLORER )
    if frmExplorer_KindFromRow( HWND_FRMEXPLORER, row ) <> EXPKIND_FOLDER then return false

    frmExplorer_RenameFolder( HWND_FRMEXPLORER, row )
    return true
end function


' ========================================================================================
' THERE IS NO EXTENSION RULE. Any file may be dragged into any group.
'
' frmExplorer_ExtAllowedInCat used to say which extensions were LEGAL under each root group
' (.bas under Main, .rc under Resource, .bi/.h under Header, .bas/.inc under Module, anything
' under Normal) and the drop validator refused everything else. Removed by request: the
' groups are how the AUTHOR chooses to organize a project, and a picker that silently refuses
' the drop cannot explain itself -- the cursor just says no.
'
' What survives is what is not about the extension: Main and Resource still hold exactly ONE
' file, and folders still cannot enter a group that holds no folders. Workspace_DeriveFileType
' is untouched -- it says what a NEWLY OPENED file defaults to, which was always a separate
' question from what a drag may do.
' ========================================================================================


' ========================================================================================
' frmExplorer_CanDropCallback -- validate a drag, and perform it.
'
' IT ALWAYS RETURNS FALSE, which tells the control to leave its own model alone. That is
' deliberate, not a refusal: PsListTree.MoveRows is a pure permutation and DOES NOT REWRITE
' .level (PsListTree.bi), so the control cannot express a reparent at all -- a file dropped
' into a folder would land next to it at the wrong depth. The host model is authoritative
' and the tree is rebuilt from it, so letting the control permute first would only produce a
' shape that is immediately discarded.
'
' The rebuild is POSTED rather than done here. We are inside the control's WM_LBUTTONUP,
' between CommitDrop and CancelDrag; mutating the model is only data, but clearing and
' refilling the rows underneath the code that is still tearing down the drag is not worth
' the risk when a posted message costs nothing.
'
' The DESTINATION is resolved from targetRow alone, which is more forgiving than requiring a
' precise hit on a group row: dropping onto a FILE means "into whatever folder that file is
' in", which is what a user aiming at a list of files expects.
' ========================================================================================
function frmExplorer_CanDropCallback( byval p as PSLISTTREE_DROPINFO ptr ) as boolean
    if p = 0 then return false
    dim as HWND hCtl = HWND_FRMEXPLORER

    ' --- where is this going? --------------------------------------------------------------
    dim as long catDest = -1
    dim as DWSTRING wszDest
    select case frmExplorer_KindFromRow( hCtl, p->targetRow )
        case EXPKIND_ROOT, EXPKIND_FOLDER
            wszDest = frmExplorer_FolderPathFromRow( hCtl, p->targetRow, catDest )
        case EXPKIND_FILE
            ' Into the dropped-on file's own container.
            dim as clsDocument ptr pT = frmExplorer_DocFromRow( hCtl, p->targetRow )
            if pT = 0 then return false
            catDest = frmExplorer_CatIndexForDoc( pT )
            wszDest = pT->docData.wszFolder
        case else
            return false        ' the promotion row, or past the last row
    end select
    if catDest < 0 then return false

    ' --- snapshot the sources BEFORE anything moves -----------------------------------------
    ' Row indices stop meaning anything the moment the model changes, so every decision below
    ' is taken against pointers and paths captured here.
    redim srcDocs(any) as clsDocument ptr
    redim srcFolderPaths(any) as DWSTRING
    redim srcFolderCats(any) as long
    for i as long = 0 to p->srcCount - 1
        dim as long row = p->srcRows[i]
        select case frmExplorer_KindFromRow( hCtl, row )
            case EXPKIND_FILE
                dim as clsDocument ptr pD = frmExplorer_DocFromRow( hCtl, row )
                if pD then
                    dim as long ub = ubound(srcDocs) + 1
                    redim preserve srcDocs(ub)
                    srcDocs(ub) = pD
                end if
            case EXPKIND_FOLDER
                dim as long catSrc
                dim as DWSTRING wszSrc = frmExplorer_FolderPathFromRow( hCtl, row, catSrc )
                if PsLen(wszSrc) then
                    dim as long ub = ubound(srcFolderPaths) + 1
                    redim preserve srcFolderPaths(ub)
                    redim preserve srcFolderCats(ub)
                    srcFolderPaths(ub) = wszSrc
                    srcFolderCats(ub)  = catSrc
                end if
        end select
    next
    dim as long nDocs = ubound(srcDocs) + 1
    dim as long nFolders = ubound(srcFolderPaths) + 1
    if (nDocs = 0) andalso (nFolders = 0) then return false

    ' --- validate EVERYTHING before moving anything -----------------------------------------
    ' A refused drop must change nothing at all, so nothing is applied until the whole drag
    ' is known to be legal. A partial move is the failure mode worth ruling out by
    ' construction: it is silent, and it splits a selection the user thought was one thing.
    if nFolders > 0 then
        ' A folder cannot enter a group that holds no folders. The subtree walk that used to
        ' sit here -- every file inside the folder checked against an extension rule -- is
        ' gone with that rule: nothing about a descendant can refuse the move now.
        if ProjectFolders_CatAllowsFolders( catDest ) = false then return false
        for i as long = 0 to nFolders - 1
            if srcFolderCats(i) = catDest then
                if ucase(srcFolderPaths(i)) = PsUCase(wszDest) then return false
                if ProjectFolders_IsDescendantOf( wszDest, srcFolderPaths(i) ) then return false
            end if
        next
    end if

    ' Main and Resource hold exactly ONE file. Dropping a second is refused rather than
    ' silently demoting a bystander: the incumbent IS demoted, but only when the drop names
    ' a single replacement, which is the gesture the user can actually see the result of.
    if (catDest = CATINDEX_MAIN) orelse (catDest = CATINDEX_RESOURCE) then
        if nFolders > 0 then return false
        if nDocs <> 1 then return false
    end if

    ' --- apply ------------------------------------------------------------------------------
    ' Folders first. Their documents' TYPES are collected before the move, because afterwards
    ' their folders name the destination and nothing identifies them as having come from the
    ' source subtree.
    for i as long = 0 to nFolders - 1
        redim moved(any) as clsDocument ptr
        if srcFolderCats(i) <> catDest then
            dim pScan as clsDocument ptr = gApp.pDocList
            do until pScan = 0
                if frmExplorer_CatIndexForDoc( pScan ) = srcFolderCats(i) then
                    dim as DWSTRING wszF = pScan->docData.wszFolder
                    dim as boolean bInside = false
                    if PsUCase(wszF) = ucase(srcFolderPaths(i)) then bInside = true
                    if ProjectFolders_IsDescendantOf( wszF, srcFolderPaths(i) ) then bInside = true
                    if bInside then
                        dim as long ub = ubound(moved) + 1
                        redim preserve moved(ub)
                        moved(ub) = pScan
                    end if
                end if
                pScan = pScan->pDocNext
            loop
        end if

        if ProjectFolders_MoveFolder( srcFolderCats(i), srcFolderPaths(i), catDest, wszDest ) then
            for j as long = lbound(moved) to ubound(moved)
                if moved(j) then gApp.ProjectSetFileType( moved(j), gConfig.Cat(catDest).idFileType )
            next
        end if
    next

    ' Then the loose files. The TYPE change goes through gApp.ProjectSetFileType rather than
    ' assigning the field, because that is what carries the Main/Resource demotion -- so a
    ' drag inherits the context menu's rule instead of restating it.
    for i as long = 0 to nDocs - 1
        if frmExplorer_CatIndexForDoc( srcDocs(i) ) <> catDest then
            gApp.ProjectSetFileType( srcDocs(i), gConfig.Cat(catDest).idFileType )
        end if
        srcDocs(i)->docData.wszFolder = wszDest
    next

    PostMessage( HWND_FRMMAIN, MSG_USER_LOAD_EXPLORERFILES, 0, 0 )
    return false
end function


' ========================================================================================
' frmExplorer_BeginLabelEditCallback -- may this row's caption be edited?
'
' Folders only. THE ROOT GROUPS MUST REFUSE: their captions come from gConfig.Cat(), are
' localized, and are what every file's ProjectFileType is matched against -- renaming one in
' place would either be silently discarded on the next load or, worse, appear to work.
' ========================================================================================
function frmExplorer_BeginLabelEditCallback( byval hCtl as HWND, byval row as integer ) as boolean
    return ( frmExplorer_KindFromRow( hCtl, row ) = EXPKIND_FOLDER )
end function


' ========================================================================================
' frmExplorer_EndLabelEditCallback -- accept or refuse a renamed folder.
'
' Refuses silently rather than complaining: an empty name, a name carrying a path separator,
' and a name already taken by a sibling. Returning FALSE leaves the caption as it was, which
' for a folder just created means it keeps its default name -- a legal outcome, not a
' failure.
' ========================================================================================
function frmExplorer_EndLabelEditCallback( byval hCtl as HWND, byval row as integer, byval newText as DWSTRING ) as boolean
    dim as long catIndex
    dim as DWSTRING wszOld = frmExplorer_FolderPathFromRow( hCtl, row, catIndex )
    if catIndex < 0 then return false
    if PsLen(wszOld) = 0 then return false

    dim as DWSTRING wszName = PsTrim(newText)
    if ProjectFolders_IsValidName( wszName ) = false then return false

    ' Unchanged apart from case is not a rename, and must not be refused as a self-collision.
    if PsUCase(wszName) = ucase( ProjectFolders_LeafName( wszOld ) ) then return true

    dim as DWSTRING wszNew = ProjectFolders_Combine( ProjectFolders_ParentPath( wszOld ), wszName )
    if ProjectFolders_Exists( catIndex, wszNew ) then return false

    ProjectFolders_Rebase( catIndex, wszOld, wszNew )

    ' The control would write the new caption into the row it has, but every row index is
    ' about to be rebuilt from the table anyway -- and the table is now the truth.
    LoadExplorerFiles()
    return true
end function


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

        case WM_MOUSEMOVE
            ' Track which action icon the cursor is over, so the tooltip callback can name it.
            '
            ' AND FORCE A RE-RESOLVE WHEN THE ZONE CHANGES. The control only re-queries a tip
            ' when the hot ROW changes (PsListTree.inc updates its tool index inside the
            ' "idx <> nLastHotIdx" guard), so sliding from Add to Delete within one row would
            ' otherwise keep showing whichever tip resolved first.
            '
            ' SetToolIndex, NOT Hide -- and that distinction is the whole reason no icon tip
            ' ever appeared. Hovering a folder row's caption resolves to no text, which is a
            ' REFUSED show, and PsTooltip suppresses further attempts until the cursor leaves
            ' the tool entirely (PsTooltip.inc, bSuppressed). Hide does not clear that flag,
            ' so sliding from the caption onto an icon stayed silent forever. SetToolIndex is
            ' the call that means "a new offer": it clears the suppression, invalidates the
            ' cached text and restarts the dwell, and it hides a visible tip without a fade
            ' on the way. The index handed back is the SAME row the control set, because
            ' PsListTree resolves its tip text by casting that index straight to a row.
            dim as HWND hCtlMv = GetParent(m->hList)
            dim as EXPLORER_ICONHIT nIcon = _
                frmExplorer_IconHitTest( hCtlMv, m->idx, GET_X_LPARAM( m->lParam ) )
            if (nIcon <> gExpTipIcon) orelse (m->idx <> gExpTipRow) then
                dim as HWND hTip = PsListTree_GetPsTooltipHandle( hCtlMv )
                if gExpTipTrace then
                    dim as string sT = "TIPTRACE zone " & str(gExpTipIcon) & "->" & str(nIcon) & _
                                       "  row " & str(gExpTipRow) & "->" & str(m->idx) & _
                                       "  x=" & str(GET_X_LPARAM( m->lParam )) & _
                                       "  hTip=" & str(cast(integer, hTip))
                    print sT
                end if
                gExpTipIcon = nIcon
                gExpTipRow  = m->idx
                ' THE INDEX IS TOGGLED THROUGH -1, and that is not a flourish.
                ' PsTooltip_SetToolIndex opens with "if nToolIndex = idx then exit sub", so
                ' handing it the row it already holds does NOTHING -- and crossing from one
                ' icon to the next never changes the row. That is precisely why the tip
                ' refused to change within a row while leaving the row and coming back worked:
                ' the latter is PsListTree calling this with a genuinely different index.
                ' Setting -1 first makes both calls real, and the pair leaves the index where
                ' it started, which matters because PsListTree resolves its tip text by
                ' casting that index straight to a row.
                ' Nothing observes the intermediate value: the tip's clock runs on this same
                ' thread, so no tick can land between the two calls.
                if hTip then
                    PsTooltip_SetToolIndex( hTip, -1 )
                    PsTooltip_SetToolIndex( hTip, m->idx )
                end if
            end if
            ' FALSE: the control's own hot tracking and repaint still have to happen. This
            ' arm only observes.
            return false
            
        case WM_LBUTTONDOWN
            dim as HWND hCtlDn = GetParent(m->hList)
            dim as EXPLORER_ROWKIND kindDn = frmExplorer_KindFromRow( hCtlDn, m->idx )
            dim as long xDn = GET_X_LPARAM( m->lParam )

            ' The action icons come FIRST, before any gesture that would otherwise claim the
            ' click. Consuming the down is not enough on its own -- see the WM_LBUTTONUP arm,
            ' which has to be kept off these rows too, or the click that created a folder
            ' would also be delivered as a click on the row it landed on.
            select case frmExplorer_IconHitTest( hCtlDn, m->idx, xDn )
                case EXPICON_ADD:    frmExplorer_NewFolder( hCtlDn, m->idx ):    return true
                case EXPICON_RENAME: frmExplorer_RenameFolder( hCtlDn, m->idx ): return true
                case EXPICON_DELETE: frmExplorer_DeleteFolder( hCtlDn, m->idx ): return true
            end select

            ' Collapse/expand, implemented HERE because the control no longer does it. A root
            ' group used to be an AddHeader row, which the control toggles on a click anywhere
            ' in it -- but it does that before the callback runs and so cannot be vetoed,
            ' which is unusable once the row carries icons.
            '
            ' Guarded on hasChildren to agree with the painter: no chevron is drawn on an
            ' empty container, so clicking one must not silently flip a state nothing shows.
            dim as boolean bContainer = false
            if (kindDn = EXPKIND_ROOT) orelse (kindDn = EXPKIND_FOLDER) then bContainer = true

            if bContainer andalso PsListTree_HasChildren( hCtlDn, m->idx ) then
                dim as RECT rcRowDn, rcTwisty
                GetClientRect( m->hList, @rcRowDn )
                rcRowDn.top = 0
                rcRowDn.bottom = PsListTree_GetRowHeight( hCtlDn )
                frmExplorer_TwistyRect( hCtlDn, m->idx, rcRowDn, rcTwisty )
                if (xDn >= rcTwisty.left) andalso (xDn < rcTwisty.right) then
                    PsListTree_ToggleRow( hCtlDn, m->idx )
                    return true
                end if
            end if

            ' A ROOT row has no other use for a click -- it cannot be selected, renamed or
            ' dragged -- so it keeps the old whole-row fold gesture. A FOLDER row does: it
            ' selects, renames in place and (next) drags, so it folds from its chevron only,
            ' which is what every other tree on this platform does.
            if kindDn = EXPKIND_ROOT then
                if PsListTree_HasChildren( hCtlDn, m->idx ) then PsListTree_ToggleRow( hCtlDn, m->idx )
                return true
            end if
            return false

        case WM_LBUTTONUP
            ' Attempt to show the file
            dim as HWND hCtl = GetParent(m->hList)
            dim as EXPLORER_ROWKIND kindUp = frmExplorer_KindFromRow( hCtl, m->idx )
            ' Containers take no action on the up. Letting a FOLDER row through would fall
            ' past the null document test and still call SetFocusToCurrentCodeWindow, so
            ' clicking a folder -- or its Add icon -- would throw focus into the editor.
            if (kindUp <> EXPKIND_ROOT) andalso (kindUp <> EXPKIND_FOLDER) then
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
            ' Groups and folders get the folder menu instead of the file menu -- and a group
            ' gets one at all now, where a right-click on a header used to do nothing.
            select case frmExplorer_KindFromRow( hCtl, m->idx )
                case EXPKIND_ROOT, EXPKIND_FOLDER
                    ShowExplorerFolderContextMenu( hCtl, m->idx )
                    return true   ' prevent further processing that leads to WM_CONTEXTMENU
            end select

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
    ' An action icon names itself, and takes precedence over whatever the row would say.
    ' In practice there is nothing to take precedence over -- icons only appear on group and
    ' folder rows, and those resolve to no document and so to no tip -- but the guard keeps
    ' that from being load-bearing if a file row ever grows an icon.
    if (row = gExpTipRow) andalso (gExpTipIcon <> EXPICON_NONE) then
        select case gExpTipIcon
            case EXPICON_ADD:    return L(462, "New Folder")
            case EXPICON_RENAME: return L(464, "Rename Folder")
            case else:           return L(463, "Delete Folder")
        end select
    end if

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
    PsListTree_SetBackColor( hCtrl, theme.panel.text.backcolor )
    PsListTree_SetFont( hCtrl, ghFont(GUIFONT_10) )
    PsListTree_SetExtendedSelect( hCtrl, true )   ' Shift/Ctrl range selection

    ' In-place folder rename. F2 opens it; the Begin callback refuses every row that is not a
    ' folder, so the root groups cannot be renamed. Click-to-edit is deliberately NOT enabled:
    ' a single click on the current row would open an editor on a folder the user is about to
    ' drag, and dragging is the more common gesture by far.
    '
    ' The editor is themed explicitly -- the control creates it lazily and owns it, so without
    ' this it opens black-on-white inside a dark panel.
    ' The control's tree indent is enabled ONLY so the in-place editor lands in the right
    ' place. The painter still computes its own indent and ignores PAINTINFO.indent, and the
    ' twisty stays off, so nothing about the rows' appearance changes -- with no columns
    ' defined the control applies its indent to nothing else. What it does drive is
    ' PsListTree_RowLevelIndentPx, which returns 0 outright while bTreeIndent is false and is
    ' what positions the editor: without this a rename opens hard against the left margin
    ' whatever depth the folder sits at.
    PsListTree_SetTreeIndent( hCtrl, true )
    PsListTree_SetIndentWidth( hCtrl, EXPLORER_INDENT_UNITS )
    ' That places the editor at the row's INDENT; the caption starts one further column in,
    ' past the chevron. SetEditTextInset is in already-scaled pixels and is a constant, which
    ' is exactly right here -- the per-level part comes from the indent above, and only the
    ' fixed glyph column is left to add.
    dim pWin as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWin then PsListTree_SetEditTextInset( hCtrl, pWin->ScaleX(EXPLORER_INDENT_UNITS) )

    ' Drag and drop. The control does the gesture, the indicator and the multi-row source
    ' set; the HOST does the move -- see frmExplorer_CanDropCallback for why it always
    ' answers FALSE. No DragDropCallback is set: it only fires when the control moved rows
    ' itself, which here it never does.
    PsListTree_SetDragReorder( hCtrl, true )
    PsListTree_SetCanDropCallback( hCtrl, @frmExplorer_CanDropCallback )

    PsListTree_EnableLabelEdit( hCtrl, true )
    PsListTree_SetBeginLabelEditCallback( hCtrl, @frmExplorer_BeginLabelEditCallback )
    PsListTree_SetEndLabelEditCallback( hCtrl, @frmExplorer_EndLabelEditCallback )
    PsListTree_SetEditColors( hCtrl, theme.panel.text.backcolor, theme.panel.text.forecolor, _
                                     theme.panel.text.forecolor, theme.panel.text.forecolor )
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (PsListTree_SetScrollBarPaintCallback is available for full custom drawing.)
    PsListTree_SetScrollBarWidth( hCtrl, SCROLLBAR_WIDTH_PANEL )
    PsListTree_SetScrollBarColors( hCtrl, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
'    PsListTree_SetScrollBarColors( hCtrl, theme.editor.scrollbar.backcolor, _
'   (was a commented-out call naming ghEditor.ForeColorScrollBarHot, a field that
'    never existed in that struct.)

    HWND_FRMEXPLORER = hCtrl
    if environ("TIKO_EXPLORER_TIPTRACE") = "1" then gExpTipTrace = true

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
        if ucase( ProjectFolders_ParentPath( gProjectFolders(i).wszPath ) ) <> PsUCase( wszParentPath ) then continue for
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
            if ucase( pDoc->docData.wszFolder ) = PsUCase( wszParentPath ) then
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
                            PsPathName(pDocs(i)->DiskFilename ), _
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
    ' white background from the empty listbox.
    '
    ' THE VISIBILITY IS RESTORED AT THE BOTTOM, and it was not always. This function used to
    ' hide the control and simply leave it hidden, which worked only because every caller was
    ' a project/workspace load with frmPanel_ShowPanel behind it to show the Explorer again.
    ' It is called from user gestures now -- creating, renaming and deleting a folder all
    ' rebuild the tree -- and on those paths nothing follows to undo the hide, so the whole
    ' panel went blank on the first click of the add-folder icon.
    '
    ' The saved state is the control's OWN style bit, never IsWindowVisible: that walks the
    ' ancestor chain, so during startup -- when frmMain is not shown yet -- it reports false
    ' for a control that is perfectly visible in its own right, and restoring from it would
    ' leave the Explorer hidden for the rest of the session. Same trap PsScrollPanel hit.
    dim as boolean bWasVisible = ( (GetWindowLongPtrW( hCtrl, GWL_STYLE ) and WS_VISIBLE) <> 0 )
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
        PsListTree_AddString( hCtrl, L(398,"Save as Project..."), 0, EXPKIND_PROMOTE )
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

    ' Put the control back exactly as it was found -- see the note on the hide above. A
    ' plain SW_SHOW would be wrong: the Explorer is legitimately hidden whenever the panel
    ' is showing Functions or Bookmarks instead, and reloading it must not yank it forward.
    if bWasVisible then ShowWindow( hCtrl, SW_SHOW )

    function = 0
end function

    
