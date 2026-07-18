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


' ========================================================================================
' Adjust all of the Tab rects left and right dimensions by the incoming amount
' ========================================================================================
function frmTopTabs_AdjustTabRects( byval nAdjAmount as long ) as long
    ' nAdjAmount can be +/-
    dim pDoc as clsDocument ptr
    
    for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs) 
        pDoc = gTTabCtl.tabs(i).pDoc
        if pDoc then
            gTTabCtl.tabs(i).rcTab.left += nAdjAmount
            gTTabCtl.tabs(i).rcTab.right += nAdjAmount
            gTTabCtl.tabs(i).rcIcon.left += nAdjAmount
            gTTabCtl.tabs(i).rcIcon.right += nAdjAmount
            gTTabCtl.tabs(i).rcText.left += nAdjAmount
            gTTabCtl.tabs(i).rcText.right += nAdjAmount
            gTTabCtl.tabs(i).rcClose.left += nAdjAmount
            gTTabCtl.tabs(i).rcClose.right += nAdjAmount
        end if
    next   
        
    function = 0
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmTopTabs_PositionWindows() as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABS)
    if pWindow = 0 then exit function

    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_TOPTABS, @rc )
            
    ' Calculate the RECT positions for all of the top tabs
    dim pDoc as clsDocument ptr 
    dim as long nTextWidth = 0
    dim as long nLeft = rc.left 
    dim as long TabHeight = TOPTABS_HEIGHT
    dim as long rcTextWidth = 70
    dim as long rcCloseWidth = 20
    dim as long rcIconWidth = 20 
    dim as long rightBorderWidth = 2
    dim as long hmargin = 1
    dim as long ActionPanelWidth = 116
    dim as long ActionButtonWidth = 24
    dim as long ActionButtonHeight = 22
    dim as long PrevTabsButtonWidth = 20
    dim as long PrevTabsButtonHeight = 20
    dim as long SplitButtonWidth = 20
    dim as long SplitButtonHeight = 20

    ' calculate the right hand side Action area panel that holds the
    ' more actions "..." button.
    gTTabCtl.rcActionPanel         = rc
    gTTabCtl.rcActionPanel.left    = gTTabCtl.rcActionPanel.right - pWindow->ScaleX( ActionPanelWidth )
    
    ' calculate the Prev and Next Tabs buttons
    dim as long vmargin = (TabHeight - PrevTabsButtonHeight) / 2
    gTTabCtl.rcPrevTabs.left       = gTTabCtl.rcActionPanel.left + pWindow->ScaleX( hmargin * 2)
    gTTabCtl.rcPrevTabs.top        = rc.top + pWindow->ScaleY(vmargin)
    gTTabCtl.rcPrevTabs.bottom     = gTTabCtl.rcPrevTabs.top + pWindow->ScaleY( PrevTabsButtonHeight )
    gTTabCtl.rcPrevTabs.right      = gTTabCtl.rcPrevTabs.left + pWindow->ScaleX( PrevTabsButtonWidth )

    gTTabCtl.rcNextTabs.left       = gTTabCtl.rcPrevTabs.right + pWindow->ScaleX( hmargin )
    gTTabCtl.rcNextTabs.top        = rc.top + pWindow->ScaleY(vmargin)
    gTTabCtl.rcNextTabs.bottom     = gTTabCtl.rcNextTabs.top + pWindow->ScaleY( PrevTabsButtonHeight )
    gTTabCtl.rcNextTabs.right      = gTTabCtl.rcNextTabs.left + pWindow->ScaleX( PrevTabsButtonWidth )

    ' calculate the split editor button
    vmargin = (TabHeight - SplitButtonHeight) / 2
    gTTabCtl.rcSplitEditor.left    = gTTabCtl.rcNextTabs.right + pWindow->ScaleX( hmargin )
    gTTabCtl.rcSplitEditor.top     = rc.top + pWindow->ScaleY(vmargin)
    gTTabCtl.rcSplitEditor.bottom  = gTTabCtl.rcSplitEditor.top + pWindow->ScaleY( SplitButtonHeight )
    gTTabCtl.rcSplitEditor.right   = gTTabCtl.rcSplitEditor.left + pWindow->ScaleY( SplitButtonWidth )

    ' calculate the actual more actions "..." button itself
    vmargin = (TabHeight - ActionButtonHeight) / 2
    gTTabCtl.rcActionButton.left   = gTTabCtl.rcSplitEditor.right + pWindow->ScaleX( hmargin )
    gTTabCtl.rcActionButton.top    = rc.top + pWindow->ScaleY(vmargin)
    gTTabCtl.rcActionButton.bottom = gTTabCtl.rcActionButton.top + pWindow->ScaleY( ActionButtonHeight )
    gTTabCtl.rcActionButton.right  = gTTabCtl.rcActionButton.left + pWindow->ScaleY( ActionButtonWidth )

    for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs) 
        pDoc = gTTabCtl.tabs(i).pDoc
        if pDoc then gTTabCtl.tabs(i).wszText = AfxStrPathname( "NAMEX", pDoc->DiskFilename )
            
        ' Determine the length of the text
        nTextWidth = getTextWidth( HWND_FRMMAIN_TOPTABS, gTTabCtl.tabs(i).wszText, GUIFONT_10 )
        if nTextWidth < 70 then nTextWidth = 70

        vmargin = (TabHeight - rcCloseWidth + 2) / 2
            
        ' calculate the various tab dimensions
        gTTabCtl.tabs(i).rcTab = rc
        gTTabCtl.tabs(i).rcTab.left = nLeft
        gTTabCtl.tabs(i).rcTab.right = _
            nLeft + pWindow->ScaleX(rcIconWidth + hmargin + nTextWidth + rcCloseWidth + hmargin)
        
        gTTabCtl.tabs(i).rcIcon         = gTTabCtl.tabs(i).rcTab
        gTTabCtl.tabs(i).rcIcon.top     = gTTabCtl.tabs(i).rcIcon.top + pWindow->ScaleY(vmargin)
        gTTabCtl.tabs(i).rcIcon.bottom  = gTTabCtl.tabs(i).rcIcon.top + pWindow->ScaleY(rcCloseWidth)
        gTTabCtl.tabs(i).rcIcon.left    = gTTabCtl.tabs(i).rcTab.left 
        gTTabCtl.tabs(i).rcIcon.right   = gTTabCtl.tabs(i).rcIcon.left + pWindow->ScaleX(rcIconWidth)
        
        gTTabCtl.tabs(i).rcText         = gTTabCtl.tabs(i).rcTab
        gTTabCtl.tabs(i).rcText.bottom  = gTTabCtl.tabs(i).rcText.top + pWindow->ScaleY(TOPTABS_HEIGHT)
        gTTabCtl.tabs(i).rcText.left    = gTTabCtl.tabs(i).rcIcon.right + hmargin
        gTTabCtl.tabs(i).rcText.right   = gTTabCtl.tabs(i).rcText.left + pWindow->ScaleX(nTextWidth)
        
        gTTabCtl.tabs(i).rcClose        = gTTabCtl.tabs(i).rcTab
        gTTabCtl.tabs(i).rcClose.top    = gTTabCtl.tabs(i).rcClose.top + pWindow->ScaleY(vmargin)
        gTTabCtl.tabs(i).rcClose.bottom = gTTabCtl.tabs(i).rcClose.top + pWindow->ScaleY(rcCloseWidth)
        gTTabCtl.tabs(i).rcClose.left   = gTTabCtl.tabs(i).rcText.right
        gTTabCtl.tabs(i).rcClose.right  = gTTabCtl.tabs(i).rcClose.left + pWindow->ScaleX(rcCloseWidth)

        gTTabCtl.tabs(i).isHot = false
        nLeft = gTTabCtl.tabs(i).rcTab.right
    next

    ' We now know the exact width and postions of all tabs so now we need to calculate
    ' and adjust the gTTabCtl.FirstDisplayTab based on where the gTTabCtl.CurSel is located.
    if gTTabCtl.GetItemCount > 0 then
        ' Adjust all of the Rects based on the current starting tab position
        dim as long nAdjAmount = gTTabCtl.tabs(gTTabCtl.FirstDisplayTab).rcTab.left
        frmTopTabs_AdjustTabRects( -(nAdjAmount) )
        
        gTTabCtl.ClientRightEdge = gTTabCtl.rcActionPanel.left
        
        if gTTabCtl.CurSel = -1 then
            ' Put if check for CurSel being invalid otherwise other tab calculations
            ' Scenarios will fail via GPF on invalid array access.
            
        ' SCENARIO #1
        ' New gTTabCtl.CurSel is located before gTTabCtl.FirstDisplayTab so it is visually
        ' not on screen. We simply need to move the gTTabCtl.CurSel rect into the
        ' the first position (gTTabCtl.FirstDisplayTab) and adjust all rects accordingly.
        elseif gTTabCtl.CurSel < gTTabCtl.FirstDisplayTab then
            ' The amount of the adjustment is equal to the difference between the
            ' current first tab's left edge and the new cursel's left edge.
            nAdjAmount = gTTabCtl.tabs(gTTabCtl.FirstDisplayTab).rcTab.left - gTTabCtl.tabs(gTTabCtl.CurSel).rcTab.left
            frmTopTabs_AdjustTabRects( nAdjAmount )
            gTTabCtl.FirstDisplayTab = gTTabCtl.CurSel
        
        ' SCENARIO #2
        ' The gTTabCtl.CurSel is already located within view but the right client edge
        ' for the tabcontrol may have grown wider so if the last tab's (ubound(gTTabCtl.tabs)
        ' right edge falls before the right edge then there may be an opportunity to shift 
        ' all of the tabs right to help fill the space.
        elseif gTTabCtl.tabs(gTTabCtl.CurSel).rcTab.right < gTTabCtl.ClientRightEdge then
            if gTTabCtl.tabs(ubound(gTTabCtl.tabs)).rcTab.right < gTTabCtl.ClientRightEdge then
            nAdjAmount = 0
                if gTTabCtl.FirstDisplayTab - 1 >= 0 then
                    dim as long nTabWidth = _
                            gTTabCtl.tabs(gTTabCtl.FirstDisplayTab - 1).rcTab.right - gTTabCtl.tabs(gTTabCtl.FirstDisplayTab - 1).rcTab.left
                    if gTTabCtl.tabs(ubound(gTTabCtl.tabs)).rcTab.right + nTabWidth <= gTTabCtl.ClientRightEdge then
                        frmTopTabs_AdjustTabRects( nTabWidth )
                        gTTabCtl.FirstDisplayTab = gTTabCtl.FirstDisplayTab - 1
                    end if   
                end if
            end if
        
        ' SCENARIO #3
        ' gTTabCtl.CurSel is located past the tab control's right edge. We can begin
        ' "removing" tabs at the start of the view until the gTTabCtl.CurSel tab comes
        ' completely within view.
        elseif gTTabCtl.tabs(gTTabCtl.CurSel).rcTab.right > gTTabCtl.ClientRightEdge then
            ' need to determine how far to move the gTTabCtl.FirstDisplayTab
            nAdjAmount = 0
            for i as long = gTTabCtl.FirstDisplayTab to gTTabCtl.CurSel 
                ' remove the width of each successive tab starting with the
                ' first tab and work our way until we reach the target tab.
                dim as long nTabWidth = gTTabCtl.tabs(i).rcTab.right - gTTabCtl.tabs(i).rcTab.left
                nAdjAmount += nTabWidth
                if gTTabCtl.tabs(gTTabCtl.CurSel).rcTab.right - nAdjAmount <= gTTabCtl.ClientRightEdge then
                    gTTabCtl.FirstDisplayTab = i + 1
                    frmTopTabs_AdjustTabRects( -(nAdjAmount) )
                    exit for
                end if   
            next
        end if

        ' sanity checks
        if gTTabCtl.FirstDisplayTab > ubound(gTTabCtl.tabs) then 
            gTTabCtl.FirstDisplayTab = ubound(gTTabCtl.tabs)
        elseif gTTabCtl.FirstDisplayTab < 0 then 
            gTTabCtl.FirstDisplayTab = 0 
        end if   
    
    end if

    ' Redraw first to ensure that the tab control paints
    AfxRedrawWindow( HWND_FRMMAIN_TOPTABS )

    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmTopTabs
' ========================================================================================
function frmTopTabs_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmTopTabs_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' Do hit test to determine what tab is currently under the mouse cursor
' ========================================================================================
function getHotTabHitTest( byval hWin as HWND ) as long
    dim as point pt: GetCursorPos( @pt )
    MapWindowpoints( HWND_DESKTOP, hWin, cast( point ptr, @pt ), 1 )
    dim as long hotTab = -1
    for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs)
        if PtInRect( @gTTabCtl.tabs(i).rcTab, pt ) then
            hotTab = i
            gTTabCtl.tabs(i).isHot = true
        else   
            gTTabCtl.tabs(i).isHot = false
        end if
    next
    function = hotTab
end function


' ========================================================================================
' Do hit test to determine if "X" close button on tab was clicked
' ========================================================================================
function isTabCloseHitTest( byval hWin as HWND, byval idx as long ) as long
    if gTTabCtl.IsSafeIndex(idx) = false then exit function
    dim as point pt: GetCursorPos( @pt )
    MapWindowpoints( HWND_DESKTOP, hWin, cast( point ptr, @pt ), 1 )
    if PtInRect( @gTTabCtl.tabs(idx).rcClose, pt ) then return idx
    function = -1
end function


' ========================================================================================
' frmTopTabs Window procedure
' ========================================================================================
function frmTopTabs_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as long curr_idxHot = -1
    
    static as DWSTRING wszPrevTooltip 
    static as HWND hTooltip 
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabs_OnSize)
    
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMMAIN_TOPTABS = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABS)
        if pWindow = 0 then exit function

        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
        TrackMouseEvent(@tme) 

        if IsWindow(hTooltip) = 0 then 
            hTooltip = AfxAddTooltip( hwnd, "", false, false )
            ' Set the tooltip max width in order to handle the multiline
            ' text needed for the split editor button.
            SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, 300 )
        end if
    
        curr_idxHot = getHotTabHitTest( hwnd )
        AfxRedrawWindow( hwnd )
        
        if gApp.bDragTabActive then
            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
            ' Only activate tab drag if we have moved delta 5px xpos from original position
            dim as point pt = (GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))
            if abs(pt.x - gApp.ptDragTabPrev.x) >= 5 then
                SetCursor( LoadCursor(0, MAKEINTRESOURCE(OCR_SIZEALL)) )
                if gTTabCtl.CurSel <> curr_idxHot then
                    ' Swap the two elements in the array
                    swap gTTabCtl.tabs(gTTabCtl.CurSel), gTTabCtl.tabs(curr_idxHot)
                    gTTabCtl.SetFocusTab( curr_idxHot )
                    frmTopTabs_PositionWindows()
                end if
            end if    
        end if
        
           
    case WM_MOUSELEAVE
        wszPrevTooltip = ""
        ' this will reset all tabs isHot to false and curr_idxHot to -1
        for i as long = lbound(gTTabCtl.tabs) to ubound(gTTabCtl.tabs)
            gTTabCtl.tabs(i).isHot = false
        next
        curr_idxHot = -1
        AfxRedrawWindow( hwnd )


    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip 
        ' test for Action Button, Filename caption or Close caption
        curr_idxHot = getHotTabHitTest( hwnd )
        if isMouseOverRECT(hwnd, gTTabCtl.rcPrevTabs) then
            wszTooltip  = L(431, "Scroll Tabs Left")
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcNextTabs) then
            wszTooltip  = L(432, "Scroll Tabs Right")
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionButton) then
            wszTooltip  = L(430, "Tab List")

        elseif isMouseOverRECT(hwnd, gTTabCtl.rcSplitEditor) then
            ' If CTRL is pressed then use the top/bottom split, otherwise
            ' use the left/right split icon.
            if (wszIconSplitEditor = wszIconSplitTopBottom) then
                wszTooltip  = L(070,"Toggle Split Editor Top/Bottom")
            else    
                wszTooltip  = L(069,"Toggle Split Editor Left/Right") & _
                                    chr(13,10) & "[Alt]: " & L(071,"Split Top/Bottom")
            end if

        elseif isTabCloseHitTest( hwnd, curr_idxHot ) = curr_idxHot then
            wszTooltip  = L(86, "Close Tab")
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionPanel) then
            ' do nothing. no tooltip.
        else
            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
            wszTooltip = ""
            if gTTabCtl.tabs(curr_idxHot).pDoc <> 0 then
                wszTooltip = gTTabCtl.tabs(curr_idxHot).pDoc->DiskFilename   
            end if    
        end if
        
        ' Display the tooltip
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip 
        end if    

    case WM_RBUTTONDOWN
        if isMouseOverRECT(hwnd, gTTabCtl.rcPrevTabs) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcNextTabs) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionButton) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcSplitEditor) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionPanel) then
        else
            ' Create the popup menu
            curr_idxHot = getHotTabHitTest( hwnd )
            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
            gTTabCtl.SetFocusTab(curr_idxHot)
            dim as point pt
            dim as HMENU hPopupMenu = CreateTopTabCtlContextMenu(curr_idxHot)
            GetCursorPos @pt
            TrackPopupMenu(hPopUpMenu, 0, pt.x, pt.y, 0, HWND_FRMMAIN, byval Null)
            DestroyMenu hPopUpMenu
            Return True   ' prevent further processing that leads to WM_CONTEXTMENU
        end if

    case WM_LBUTTONDOWN
        if isMouseOverRECT(hwnd, gTTabCtl.rcPrevTabs) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcNextTabs) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionButton) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcSplitEditor) then
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionPanel) then
        else ' regular tab
            curr_idxHot = getHotTabHitTest( hwnd )
            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
            if (isTabCloseHitTest( hwnd, curr_idxHot ) = curr_idxHot) andalso (curr_idxHot <> gTTabCtl.CurSel) then
                ' we are mouse down on an "X" close on a tab that is not currently the active tab. We
                ' will simply close that tab rather than bring it into focus and then delete.
                ' allow the mouse message to eventually bubble up WM_LBUTTONUP where we do the close.
            else
                ' we have clicked on tab so bring it into focus
                gTTabCtl.SetFocusTab( curr_idxHot )
                dim as point pt = (GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))
                gApp.bDragTabActive = true
                gApp.ptDragTabPrev = pt
                AfxRedrawWindow( hwnd )
            end if
            SetCapture(hwnd)
        end if

    case WM_LBUTTONUP
        if isMouseOverRECT(hwnd, gTTabCtl.rcPrevTabs) then
            ' if the first displayed tab is not index 0 then there must be tabs before it
            ' that can be displayed. if no previous tabs then set focus to the first tab.
            if gTTabCtl.FirstDisplayTab > 0 then
                gTTabCtl.SetFocusTab( gTTabCtl.FirstDisplayTab - 1 )
            else   
                gTTabCtl.SetFocusTab(0)
            end if
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcNextTabs) then
            ' determine if there are any tabs to the right of the current display view. Bring
            ' the first one found (if any) into view. Keep track of the found tab because if
            ' none found then simply set the CurSel to the last tab.
            dim as long nFound = -1
            for i as long = gTTabCtl.FirstDisplayTab to gTTabCtl.GetItemCount - 1
                if gTTabCtl.tabs(i).rcTab.left > gTTabCtl.ClientRightEdge then
                    nFound = i: exit for
                    exit for
                end if
            next
            if nFound = -1 then nFound = gTTabCtl.GetItemCount - 1
            gTTabCtl.SetFocusTab(nFound)

        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionButton) then
            dim as HMENU hPopupMenu = CreateTopTabsActionButtonContextMenu()
            ' Popup the menu to the bottom of the Action Button (right aligned)
            dim as RECT rc = gTTabCtl.rcActionButton   ' work with a copy
            MapWindowpoints( HWND_FRMMAIN_TOPTABS, HWND_DESKTOP, cast(point ptr, @rc), 2 )
            dim as long id = TrackPopupMenu(hPopUpMenu, _
                                TPM_RIGHTALIGN or TPM_RETURNCMD, _
                                rc.right, rc.bottom, 0, HWND_FRMMAIN, byval null)
            ' Return value is 1 based because 0 indicates that menu was cancelled
            if (id > 0) andalso (id <= gTTabCtl.GetItemCount) then
                gTTabCtl.SetFocusTab(id-1)
            end if
            DestroyMenu( hPopUpMenu )
            Return true   ' prevent further processing that leads to WM_CONTEXTMENU

        elseif isMouseOverRECT(hwnd, gTTabCtl.rcSplitEditor) then
            ' Split editor view
            ' IDM_SPLITLEFTRIGHT, IDM_SPLITTOPBOTTOM
            if (wszIconSplitEditor = wszIconSplitTopBottom) then
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_SPLITTOPBOTTOM, 0), 0 )
            else    
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_SPLITLEFTRIGHT, 0), 0 )
            end if

        elseif isMouseOverRECT(hwnd, gTTabCtl.rcActionPanel) then
                
        else
            ' Reset the mouse pointer
            SetCursor( LoadCursor( null, IDC_ARROW ))
            ReleaseCapture
            gApp.bDragTabActive = false
            ' if we are still over an "X" close during buttonup then close the tab
            curr_idxHot = getHotTabHitTest( hwnd )
            if gTTabCtl.IsSafeIndex(curr_idxHot) = false then exit function
            if isTabCloseHitTest( hwnd, curr_idxHot ) = curr_idxHot then
                gTTabCtl.CloseTab( curr_idxHot )
                frmTopTabs_PositionWindows()
            end if
        end if


    case WM_LBUTTONDBLCLK
        ' If doubleclicking in the unused client area of the tab control then 
        ' add a new file. We do not track the actual unused area, so we need to
        ' test whether the double click occured over a Tab or over the Action Panel. 
        if ( getHotTabHitTest(hwnd) = -1 ) andalso _
           ( isMouseOverRect(hwnd, gTTabCtl.rcActionPanel) = false ) then
           PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FILENEW, 0), 0 )
        end if
        
        
    case WM_PAINT
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Paint the tab info area 
        b.SetForeColors( ghTopTabs.ForeColor, ghTopTabs.ForeColor )
        b.SetBackColors( ghTopTabs.BackColor, ghTopTabs.BackColor )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' All of the rc calculations have already been done in frmTopTabs_PostionWindows
        if gTTabCtl.FirstDisplayTab > ubound(gTTabCtl.tabs) then gTTabCtl.FirstDisplayTab = lbound(gTTabCtl.tabs)
        for i as long = gTTabCtl.FirstDisplayTab to ubound(gTTabCtl.tabs)
            ' paint this tab based on active/inactive status
            if i = gTTabCtl.CurSel then
                b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
                b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )
                b.SetPenColor( ghEditor.Divider )
                b.PaintBorderRect( @gTTabCtl.tabs(i).rcTab )
            else
                b.SetForeColors( ghTopTabs.ForeColor, ghTopTabs.ForeColor )
                b.SetBackColors( ghTopTabs.BackColor, ghTopTabs.BackColor )
                b.PaintRect( @gTTabCtl.tabs(i).rcTab )
            end if
            
            ' display the "Find" icon if this in a special "Find in Project" tab.
            if gTTabCtl.tabs(i).docType = DocumentType.ProjectSearch then
                b.SetFont( ghFont(SYMBOLFONT_9) )
                b.PaintText( wszIconFind, @gTTabCtl.tabs(i).rcIcon, DT_CENTER )
            end if
            
            ' display the tab text
            b.SetFont( ghFont(GUIFONT_10) )
            b.PaintText( gTTabCtl.tabs(i).wszText, @gTTabCtl.tabs(i).rcText, DT_CENTER )

            ' if this document is dirty then the dirty circle icon overrides the display of the close icon
            ' unless the tab isHot. The dirty icon displays always for active and inactive tabs.
            dim as boolean isDirty 
            if gTTabCtl.tabs(i).pDoc then
                if cbool(SciExec( gTTabCtl.tabs(i).pDoc->hWindow(0), SCI_GETMODIFY, 0, 0 )) then isDirty = true
                if gTTabCtl.tabs(i).pDoc->UserModified then isDirty = true
            end if    
            
            if isDirty then
                ' if mouse is over the rcClose then draw the close icon, otherwise draw the dirty circle
                b.SetFont( ghFont(SYMBOLFONT_9) )
                b.PaintText( wszIconCompileResult, @gTTabCtl.tabs(i).rcClose, DT_CENTER )
            end if
            
            if (gTTabCtl.tabs(i).isHot = true) then
                ' if this is mouse hot then display the close icon only 
                ' if the dirty icon is not already being displayed
                b.SetFont( ghFont(SYMBOLFONT_9) )
                if isDirty = false then
                    b.PaintText( wszIconClose, @gTTabCtl.tabs(i).rcClose, DT_CENTER )
                end if
                if (isTabCloseHitTest( hwnd, i ) = i) andalso (gApp.bDragTabActive = false) then
                    ' if we are hovered over the "X" close icon rect then highlight it
                    b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
                    b.SetBackColors( ghTopTabs.CloseBackColorHot, ghTopTabs.CloseBackColorHot )
                    b.PaintIconButton( wszIconClose, @gTTabCtl.tabs(i).rcClose )
                end if   
            end if
            
            ' Do the calculation of the LastDisplayTab here because we need to
            ' do testing of when the tab rect goes beyond the visual area.
            if gTTabCtl.tabs(i).rcTab.right < gTTabCtl.rcActionPanel.left then
                gTTabCtl.LastDisplayTab = i
            end if

        next

        ' Draw the TopTabs Action Area (basically, the little panel to the
        ' right of all of the top tabs that has the "..." icon.
        if gTTabCtl.GetItemCount then
            b.SetFont( ghFont(SYMBOLFONT_10) )
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColor, ghTopTabs.CloseBackColorHot )

            ' paint the full Action Panel
            b.PaintRect( @gTTabCtl.rcActionPanel )
            
            ' Draw the PrevTabs button
            b.PaintIconButton( wszIconChevronLeft, @gTTabCtl.rcPrevTabs )

            ' Draw the NextTabs button
            b.PaintIconButton( wszIconChevronRight, @gTTabCtl.rcNextTabs )

            ' Draw the SplitEditor button
            b.PaintIconButton( wszIconSplitEditor, @gTTabCtl.rcSplitEditor )
            
            ' Draw the Action button "..."
            b.PaintIconButton( wszIconMoreActions, @gTTabCtl.rcActionButton )
        end if
    
        ' Lastly, underline the entire tabcontrol.
        b.SetPenColor( ghEditor.Divider )
        b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        
        ' Clear the bottom border of the active tab
        dim as long idx = gTTabCtl.CurSel
        if gTTabCtl.IsSafeIndex(idx) then
            b.SetPenColor( ghTopTabs.BackColorHot )
            b.PaintLine( 1, gTTabCtl.tabs(idx).rcTab.left, b.rcClient.bottom, _
                                gTTabCtl.tabs(idx).rcTab.right, b.rcClient.bottom )
        end if
        
        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function



' ========================================================================================
' frmTopTabs_Show
' ========================================================================================
function frmTopTabs_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_TOPTABS = pWindow->Create( hwndParent, @frmTopTabs_WndProc, _
        0, 0, 0, TOPTABS_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    frmTopTabs_PositionWindows()
    function = 0
    
end function

