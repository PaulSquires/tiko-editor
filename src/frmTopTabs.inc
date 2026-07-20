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
#include once "CTabBar.bi"


'        if IsWindow(hTooltip) = 0 then 
'            hTooltip = AfxAddTooltip( hwnd, "", false, false )
'            ' Set the tooltip max width in order to handle the multiline
'            ' text needed for the split editor button.
'            SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, 300 )
'        end if
    
'    case WM_MOUSEHOVER
'        dim as DWSTRING wszTooltip 
'        ' test for Action Button, Filename caption or Close caption
'        curr_idxHot = getHotTabHitTest( hwnd )
'        if isMouseOverRECT(hwnd, gTTabCtl.rcPrevTabs) then
'            wszTooltip  = L(431, "Scroll Tabs Left")
'        elseif isMouseOverRECT(hwnd, gTTabCtl.rcNextTabs) then
'            wszTooltip  = L(432, "Scroll Tabs Right")
'        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionButton) then
'            wszTooltip  = L(430, "Tab List")

'        elseif isMouseOverRECT(hwnd, gTTabCtl.rcSplitEditor) then
'            ' If CTRL is pressed then use the top/bottom split, otherwise
'            ' use the left/right split icon.
'            if (wszIconSplitEditor = wszIconSplitTopBottom) then
'                wszTooltip  = L(070,"Toggle Split Editor Top/Bottom")
'            else    
'                wszTooltip  = L(069,"Toggle Split Editor Left/Right") & _
'                                    chr(13,10) & "[Alt]: " & L(071,"Split Top/Bottom")
'            end if

'        elseif isTabCloseHitTest( hwnd, curr_idxHot ) = curr_idxHot then
'            wszTooltip  = L(86, "Close Tab")
'        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionPanel) then
'            ' do nothing. no tooltip.
'        else
'            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
'            wszTooltip = ""
'            if gTTabCtl.tabs(curr_idxHot).pDoc <> 0 then
'                wszTooltip = gTTabCtl.tabs(curr_idxHot).pDoc->DiskFilename   
'            end if    
'        end if
        



' ========================================================================================
' frmTopTabs_PaintCallback
'
' The four rects arrive precomputed (p->rc deflated, p->rcText / rcIcon / rcClose) --
' no geometry is derived here, which is the whole point of the PAINTINFO contract.
' State priority: selected > hot > normal. The control draws the active tab's border
' and the bottom line itself, AFTER this callback; our fill of p->rc (which runs to the
' client bottom on the active tab) is what shows through the line's break.
' ========================================================================================
sub frmTopTabs_PaintCallback( byval p as CTabBar_PAINTINFO ptr )

    ' State priority: dragging > selected > hot > normal. The dragged tab gets its own
    ' fill so the eye can follow it while its neighbours change places around it.
    dim as COLORREF backclr, foreclr
    if p->isDragging then
        backclr = ghTopTabs.BackColor
        foreclr = ghTopTabs.ForeColor
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
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_CENTER )

    ' The close "X" (Segoe Fluent Icons ChromeClose), only while the tab is hot;
    ' brightened while the cursor is over the X itself.
    if p->isHot orelse p->isHotClose then
        if p->isHotClose then p->b->SetBackColor( ghTopTabs.CloseBackColorHot )
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wszIconClose, @p->rcClose, DT_CENTER )
    end if

'    ' display the "Find" icon if this in a special "Find in Project" tab.
'    if gTTabCtl.tabs(i).docType = DocumentType.ProjectSearch then
'        b.SetFont( ghFont(SYMBOLFONT_9) )
'        b.PaintText( wszIconFind, @gTTabCtl.tabs(i).rcIcon, DT_CENTER )
'    end if
            
'            ' if this document is dirty then the dirty circle icon overrides the display of the close icon
'            ' unless the tab isHot. The dirty icon displays always for active and inactive tabs.
'            dim as boolean isDirty 
'            if gTTabCtl.tabs(i).pDoc then
'                if cbool(SciExec( gTTabCtl.tabs(i).pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 )) then isDirty = true
'                if gTTabCtl.tabs(i).pDoc->UserModified then isDirty = true
'            end if    
            
'            if isDirty then
'                ' if mouse is over the rcClose then draw the close icon, otherwise draw the dirty circle
'                b.SetFont( ghFont(SYMBOLFONT_9) )
'                b.PaintText( wszIconCompileResult, @gTTabCtl.tabs(i).rcClose, DT_CENTER )
'            end if
            

end sub


' ========================================================================================
' frmTopTabs_SelChangeCallback  -- the USER changed the active tab (clicks only; the
' SetCurSel calls at startup are programmatic and stay silent).
' ========================================================================================
sub frmTopTabs_SelChangeCallback( byval hTabBar as HWND, byval idxOld as integer, byval idxNew as integer )
    gTTabCtl.TabFocusLost( idxOld )
    gTTabCtl.SetFocusTab( idxNew )
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
function frmTopTabs_MessageCallback( byval m as CTabBar_MESSAGEINFO ptr ) as boolean

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

'    case WM_LBUTTONDBLCLK
'        ' If doubleclicking in the unused client area of the tab control then 
'        ' add a new file. We do not track the actual unused area, so we need to
'        ' test whether the double click occured over a Tab or over the Action Panel. 
'        if ( getHotTabHitTest(hwnd) = -1 ) andalso _
'           ( isMouseOverRect(hwnd, gTTabCtl.rcActionPanel) = false ) then
'           PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FILENEW, 0), 0 )
'        end if
        
'    case WM_RBUTTONDOWN
'            ' Create the popup menu
'            curr_idxHot = getHotTabHitTest( hwnd )
'            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
'            gTTabCtl.SetFocusTab(curr_idxHot)
'            dim as point pt
'            dim as HMENU hPopupMenu = CreateTopTabCtlContextMenu(curr_idxHot)
'            GetCursorPos @pt
'            TrackPopupMenu(hPopUpMenu, 0, pt.x, pt.y, 0, HWND_FRMMAIN, byval Null)
'            DestroyMenu hPopUpMenu
'            Return True   ' prevent further processing that leads to WM_CONTEXTMENU
'        end if

        
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

    dim as HWND hCtrl = CTabBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TABBAR )
    SetWindowPos( hCtrl, 0, 0, 0, 0, AfxScaleY(TOPTABS_HEIGHT), SWP_NOZORDER )

    CTabBar_SetPaintCallback( hCtrl, @frmTopTabs_PaintCallback )
    CTabBar_SetMessageCallback( hCtrl, @frmTopTabs_MessageCallback )
    CTabBar_SetTooltipCallback( hCtrl, @frmTopTabs_TooltipCallback )
    CTabBar_SetSelChangeCallback( hCtrl, @frmTopTabs_SelChangeCallback )
    CTabBar_SetCloseCallback( hCtrl, @frmTopTabs_CloseCallback )
    CTabBar_SetReorderCallback( hCtrl, @frmTopTabs_ReorderCallback )
    CTabBar_SetPadding( hCtrl, 0 )
    CTabBar_SetBackColor( hCtrl, ghTopTabs.BackColor )
    CTabBar_SetFont( hCtrl, ghFont(GUIFONT_10) )

    ' The (bottom line + active tab border) is the CONTROL's one color.
    CTabBar_SetBorderColor( hCtrl, ghEditor.Divider )

    HWND_FRMMAIN_TOPTABS = hCtrl
    gTTabCtl.hWindow = hCtrl
    
    function = 0
    
end function

