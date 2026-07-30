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

#include once "frmTopTabs.bi"
#include once "PsTabBar.bi"


' ========================================================================================
' frmTopTabs_PaintCallback
'
' The four rects arrive precomputed (p->rc deflated, p->rcText / rcIcon / rcClose) --
' no geometry is derived here, which is the whole point of the PAINTINFO contract.
' State priority: selected > hot > normal. The control draws the active tab's border
' and the bottom line itself, AFTER this callback; our fill of p->rc (which runs to the
' client bottom on the active tab) is what shows through the line's break.
' ========================================================================================
sub frmTopTabs_PaintCallback( byval p as PSTABBAR_PAINTINFO ptr )

    ' State priority: dragging > selected > hot > normal. The dragged tab gets its own
    ' fill so the eye can follow it while its neighbours change places around it.
    dim as COLORREF backclr, foreclr
    if p->isDragging then
        backclr = ghTopTabs.BackColorHot
        foreclr = ghTopTabs.ForeColorHot
    elseif p->isSelected then
        backclr = ghTopTabs.BackColorHot
        foreclr = ghTopTabs.ForeColorHot
    elseif p->isHot then
        backclr = ghTopTabs.BackColor
        foreclr = ghTopTabs.ForeColor
    else
        backclr = ghTopTabs.BackColor
        foreclr = ghTopTabs.ForeColor
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Fill the (already deflated) paint area
    p->b->PaintRect( @p->rc )

    ' Caption in its precomputed rect, with the font we handed to SetFont (the widths
    ' were measured with it). PaintText applies DT_VCENTER|DT_SINGLELINE itself, and
    ' rcText's symmetric extents keep the text still when selection changes.
    ' The Find in Project tab carries a magnifier to the LEFT of the search term, so it reads
    ' as a search rather than as a file whose name happens to be the phrase. The caption rect
    ' gives up that much room on its left; everything else about the tab is unchanged.
    dim as RECT rcCaption = p->rcText
    if gTTabCtl.IsFindTab( p->itemID ) then
        dim as RECT rcIcon = p->rcText
        rcIcon.right = rcIcon.left + AfxScaleX(16)
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wszIconFind, @rcIcon, DT_CENTER )
        rcCaption.left = rcIcon.right
    end if

    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @rcCaption, DT_CENTER )



    ' if this document is dirty then the dirty circle icon overrides the display of the close icon
    ' unless the tab isHot. The dirty icon displays always for active and inactive tabs.
    dim as boolean isDirty
    if gTTabCtl.IsValidTab( p->itemID ) then
        dim as clsDocument ptr pDoc = gTTabCtl.GetpDoc(p->itemID)
        if pDoc then
            if cbool(SciExec( pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 )) then isDirty = true
            if pDoc->UserModified then isDirty = true
        end if
        ' The Find in Project tab has no document of its own, but its results ARE editable --
        ' so it is dirty when any file in them is. It then takes the SAME treatment every
        ' other tab gets below: the circle replaces the close X unless the tab is hot.
        if gTTabCtl.IsFindTab( p->itemID ) then
            if FindProject_AnyDirty() then isDirty = true
        end if
    end if

    if (isDirty = true) andalso (p->isHot = false) andalso (p->isHotClose = false) then
        ' if mouse is over the rcClose then draw the close icon, otherwise draw the dirty circle
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wszIconCompileResult, @p->rcClose, DT_CENTER )
    end if
            
    ' The close "X" (Segoe Fluent Icons), only while the tab is hot;
    ' brightened while the cursor is over the X itself. This painting takes precedence
    ' over the file dirty circle, that is why we paint it last here.
    if p->isHotClose then 
        p->b->SetForeColor( ghTopTabs.ForeColorHot )
        p->b->SetBackColor( ghTopTabs.CloseBackColorHot )
    end if    
    if p->isHot orelse p->isHotClose then
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintRoundRect( @p->rcClose )
        p->b->PaintText( wszIconClose, @p->rcClose, DT_CENTER )
    end if

end sub


' ========================================================================================
' frmTopTabs_SelChangeCallback  -- the USER changed the active tab (clicks only; the
' SetCurSel calls at startup are programmatic and stay silent).
' ========================================================================================
sub frmTopTabs_SelChangeCallback( byval hTabBar as HWND, byval idxOld as integer, byval idxNew as integer )
    ' A tab click is a navigation, and this is the ONLY place that sees it as one: the
    ' control fires this for user clicks only, so the programmatic SetFocusTab calls
    ' elsewhere (including the one NavHistory_Goto drives) never reach here.
    NavHistory_RecordJump()

    gTTabCtl.TabFocusLost( idxOld )
    gTTabCtl.SetFocusTab( idxNew )

    NavHistory_NoteArrival()
end sub

' ========================================================================================
' frmTopTabs_CloseCallback  -- the user clicked a tab's "X" (or middle-clicked the tab).
' NOTIFY-ONLY: the control deleted nothing. The host decides -- tiko would prompt
' "save changes?" here and veto by simply not deleting. 
' ========================================================================================
sub frmTopTabs_CloseCallback( byval hTabBar as HWND, byval idx as integer )
    gTTabCtl.CloseTab(idx)
end sub


' ========================================================================================
' frmTopTabs_ReorderCallback  -- a drag moved a tab (one call per live move; the control's
' state is already updated). tiko would mirror this move into its document list.
' ========================================================================================
sub frmTopTabs_ReorderCallback( byval hTabBar as HWND, byval idxFrom as integer, byval idxTo as integer )
'    print " reorder " & str(idxFrom) & " -> " & str(idxTo)
end sub


' ========================================================================================
' frmTopTabs_MessageCallback
' ========================================================================================
function frmTopTabs_MessageCallback( byval m as PsTabBar_MESSAGEINFO ptr ) as boolean

    select case m->uMsg

        case WM_MOUSEHOVER
            ' Tooltips are on-demand via the tooltip callback; nothing to push.
            return true

        case WM_LBUTTONUP
            ' The control holds capture across the press, so this up arrives even when
            ' the cursor was dragged outside: idx = -1 then. (Our return value is
            ' IGNORED for this message -- it is what releases the capture.)
            if m->idx <> -1 then
                ' We handle the actual changing of displayed scintilla windows in the
                ' frmTopTabs_SelChangeCallback() rather than handling button up.
            end if
            return false

        case WM_LBUTTONDBLCLK
            ' If doubleclicking in the unused client area of the tab control then 
            ' add a new file. We do not track the actual unused area, so we need to
            ' test whether the double click occured over a Tab or over the Action Panel. 
'            if getHotTabHitTest(hwnd) = -1 then
            if m->idx = -1 then
               PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FILENEW, 0), 0 )
            end if
        
        case WM_RBUTTONDOWN
            if m->idx = -1 then return false
            ' Create the popup menu
            gTTabCtl.SetFocusTab(m->idx)
            if gTTabCtl.IsValidTab(m->idx) = false then return false
            ShowTopTabCtlContextMenu( m->idx )
            return true   ' prevent further processing that leads to WM_CONTEXTMENU
        
    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function


' ========================================================================================
' frmTopTabs_TooltipCallback  -- per-tab tooltip text, resolved on demand
' ========================================================================================
function frmTopTabs_TooltipCallback( byval hCtl as HWND, byval idx as integer ) as DWSTRING
    dim as DWSTRING wszTooltip
    dim as clsDocument ptr pDoc = gTTabCtl.GetpDoc(idx)
    if pDoc then wszTooltip = pDoc->DiskFilename   
    return wszTooltip    
end function


' ========================================================================================
' frmTopTabs_Show
' ========================================================================================
function frmTopTabs_Show( byval hwndParent as HWND ) as LRESULT

    dim as HWND hCtrl = PsTabBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TABBAR )
    SetWindowPos( hCtrl, 0, 0, 0, 0, AfxScaleY(TOPTABS_HEIGHT), SWP_NOZORDER )

    PsTabBar_SetPaintCallback( hCtrl, @frmTopTabs_PaintCallback )
    PsTabBar_SetMessageCallback( hCtrl, @frmTopTabs_MessageCallback )
    PsTabBar_SetTooltipCallback( hCtrl, @frmTopTabs_TooltipCallback )
    PsTabBar_SetTooltipMode( hCtrl, PSTIP_MODE_PS )
    PsTabBar_SetSelChangeCallback( hCtrl, @frmTopTabs_SelChangeCallback )
    PsTabBar_SetCloseCallback( hCtrl, @frmTopTabs_CloseCallback )
    PsTabBar_SetReorderCallback( hCtrl, @frmTopTabs_ReorderCallback )
    PsTabBar_SetPadding( hCtrl, 0 )
    PsTabBar_SetBackColor( hCtrl, ghTopTabs.BackColor )
    PsTabBar_SetFont( hCtrl, ghFont(GUIFONT_10) )

    ' The (bottom line + active tab border) is the CONTROL's one color.
    PsTabBar_SetBorderColor( hCtrl, ghEditor.Divider )

    HWND_FRMMAIN_TOPTABS = hCtrl
    gTTabCtl.hWindow = hCtrl
    
    function = 0
    
end function

