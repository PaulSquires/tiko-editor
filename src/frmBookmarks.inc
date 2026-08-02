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
''  frmBookmarks.inc
''

#include once "frmBookmarks.bi"


' ========================================================================================
' Expand/Collapse all Bookmark Nodes
' ========================================================================================
function frmBookmarks_ExpandAll() as long
    PsListTree_ExpandAll( HWND_FRMBOOKMARKS )
    function = 0
end function

function frmBookmarks_CollapseAll() as long
    PsListTree_CollapseAll( HWND_FRMBOOKMARKS )
    function = 0
end function


' ========================================================================================
' frmBookmarks_PaintCallback
' ========================================================================================
sub frmBookmarks_PaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )
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

        p->b->SetFont( ghFont(GUIFONT_10) )
        rcText.left = rcIcon.right
        p->b->PaintText( wszCaption, @rcText, DT_LEFT )
    end if

end sub


' ========================================================================================
' frmBookmarks_MessageCallback
' ========================================================================================
function frmBookmarks_MessageCallback( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

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
            dim as HWND hCtl = GetParent(m->hList)
            if PsListTree_IsHeader( hCtl, m->idx ) = false then
                dim as integer nItemData = PsListTree_GetItemData( hCtl, m->idx )
                dim as integer nLineNum = PsListTree_GetItemDataExtra( hCtl, m->idx )
                dim as clsDocument ptr pDoc = cast(clsDocument ptr, nItemData)
                if pDoc then 
                    OpenSelectedDocument( pDoc->DiskFilename, "", nLineNum )
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
' frmBookmarks_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function frmBookmarks_TooltipCallback( byval hCtl as HWND, byval row as integer, byval col as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as integer nItemData = PsListTree_GetItemData(hCtl, row)
    dim as clsDocument ptr pDoc = cast(clsDocument ptr, nItemData)
    if pDoc then wszTooltip = pDoc->DiskFilename
    return wszTooltip
end function


' ========================================================================================
' frmBookmarks_Show
' ========================================================================================
function frmBookmarks_Show( byval hWndParent as HWND ) as LRESULT

    dim as HWND hCtrl = PsListTree_Create( hWndParent, IDC_FRMBOOKMARKS_LISTBOX )

    PsListTree_SetPaintCallback( hCtrl, @frmBookmarks_PaintCallback )
    PsListTree_SetMessageCallback( hCtrl, @frmBookmarks_MessageCallback )
    PsListTree_SetTooltipCallback( hCtrl, @frmBookmarks_TooltipCallback )
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
'    PsListTree_SetScrollBarColors( hCtrl, ghEditor.BackColorScrollBar, _
'                                 ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBarHot )

    HWND_FRMBOOKMARKS = hCtrl
    
    function = 0

end function


' ========================================================================================
' ClearBookmarks
' ========================================================================================
function ClearBookmarks() as long
    dim as HWND hCtrl = HWND_FRMBOOKMARKS
    PsListTree_Clear( hCtrl )
    function = 0
end function

' ========================================================================================
' LoadBookmarks
' ========================================================================================
function LoadBookmarksFiles() as long
    dim as HWND hCtrl = HWND_FRMBOOKMARKS

    ' Hide the listbox while it is loading so that we don't get flicker from
    ' white background from the empty listbox
    ShowWindow( HWND_FRMBOOKMARKS, SW_HIDE )

    ' Save the topindex because we will restore it after filling the new contents
    dim as long nCurSel = PsListTree_GetCurSel( hCtrl )
    dim as long nTopIndex = PsListTree_GetTopIndex( hCtrl )

    ' Clear all content from the listbox
    PsListTree_Clear( hCtrl )

    PsListTree_BeginUpdate( hCtrl )

    ' Iterate all pDoc in the project/files list
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        ' Test if the pDoc has active bookmarks
        dim as string sBookmarks

        ' Search documents that have been loaded and have valid Scintilla window.
        if pDoc->hWindow(0) <> null then
            sBookmarks = pDoc->GetBookmarks()
        end if

        if len(sBookmarks) then
            dim as DWSTRING wszText = AfxStrPathName( "NAMEX", pDoc->DiskFilename )
            PsListTree_AddHeader( hCtrl, wszText, cast(integer, pDoc), 0 )

            dim as integer nCount = AfxStrParseCount( sBookmarks, "," )
            for i as integer = 1 to nCount
                dim as integer nLineNum = val( AfxStrParse( sBookmarks, i, "," ) )
                dim as string sDescription = ltrim(pDoc->GetLine(nLineNum))
                wszText.utf8 = sDescription  ' convert ut8 to unicode  
                PsListTree_AddString( hCtrl, wszText, cast(integer, pDoc), nLineNum )
            next
        end if

        pDoc = pDoc->pDocNext
    loop

    PsListTree_EndUpdate( hCtrl )

    ' Restore the top index so the list displays like it did before being reset
    PsListTree_SetTopIndex( hCtrl, nTopIndex )
    nCurSel = MIN( nCurSel, PsListTree_GetCount(hCtrl) - 1 )
    PsListTree_SetCurSel( hCtrl, nCurSel )

    ' Ensure to show/hide this panel if it is active or not
    frmPanel_PositionWindows()

    function = 0
end function


