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

#include once "frmTopTabsMenu.bi"


' ========================================================================================
' Format the tooltip based on menu id (itemData)
' ========================================================================================
function frmTopTabsMenu_FormatTooltip( byval id as integer ) as DWSTRING
    dim as DWSTRING wszTooltip = getMenuText(id)
    dim as DWSTRING wszCaption = rtrim(AfxStrParse(wszTooltip, 1, wchr(9)), "...")
    dim as DWSTRING wszShortcut = AfxStrParse(wszTooltip, 2, wchr(9))
    
    if len(wszShortcut) then wszShortcut = " (" & wszShortcut & ")"
    
    return wszCaption & wszShortcut
end function


' ========================================================================================
' frmTopTabsMenu_PaintCallback
' ========================================================================================
sub frmTopTabsMenu_PaintCallback( byval p as PSSTATUSBAR_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 
    if p = null then return

    ' State priority for the row background/foreground: selected > hot > normal
    dim as COLORREF backclr, foreclr
    foreclr = ghTopTabs.ForeColorHot
    backclr = ghTopTabs.BackColor
    if p->isHot then backclr = ghTopTabs.BackColorHot
    p->b->SetColors(foreclr, backclr)

    ' If this is the Spring panel then we don't want any highlighting because it is
    ' just a placeholder that forces the icons right justified.
    if PsStatusBar_GetItemData(p->hStatusBar, p->itemID) = 0 then
        p->b->SetColors(ghPanel.BackColor, ghPanel.BackColor)
    end if

    ' Don't paint the background because we will reduce the size of the drawing
    ' rect and we don't want the outside of that rect to be highlighted.
'    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(SYMBOLFONT_10) )

    dim as integer itemData = PsStatusBar_GetItemData( p->hStatusBar, p->itemID )
    
    ' Draw the icon
    dim as RECT rc = p->rc
    
    ' Shrink the drawing rect top and height for better icon drawing squareness.
    dim as integer py = pWindow->ScaleX(6)
    rc.top += py:  rc.bottom -= py
    p->b->PaintIconButton( p->wszCaption, @rc )

    ' Paint a full line under the control so that continues the line
    ' of the TopTabs control.
    p->b->SetPenColor( ghEditor.Divider )
    p->b->PaintLine( 2, p->b->rcClient.left, p->b->rcClient.bottom, p->b->rcClient.right, p->b->rcClient.bottom )

end sub


' ========================================================================================
' frmTopTabsMenu_MessageCallback
' ========================================================================================
function frmTopTabsMenu_MessageCallback( byval m as PSSTATUSBAR_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_LBUTTONUP
            if m->idx = -1 then return false
        
            dim as integer idMessage = PsStatusBar_GetItemData( m->hStatusBar, m->idx )

            select case idMessage
                 case IDM_GOTONEXTTAB, IDM_GOTOPREVTAB
                    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(idMessage, 0), 0 )
                    
                 case IDM_EDITORSPLIT
                    ' Split editor view
                    ' IDM_SPLITLEFTRIGHT, IDM_SPLITTOPBOTTOM
                    if (wszIconSplitEditor = wszIconSplitTopBottom) then
                        PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_SPLITTOPBOTTOM, 0), 0 )
                    else    
                        PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_SPLITLEFTRIGHT, 0), 0 )
                    end if
                                 
                 case IDM_TABSLIST
                    ' Popup the menu to the bottom of the Action Button (right aligned).
                    ' The tab is switched in the popup's own select callback
                    ' (TopTabsListContextMenu_OnSelect) -- its item ids are tab indexes,
                    ' not commands, so they must never reach frmMain's WM_COMMAND.
                    dim as RECT rc = PsStatusBar_GetItemRect(m->hStatusBar, m->idx)
                    MapWindowpoints( HWND_FRMMAIN_TOPTABSMENU, HWND_DESKTOP, cast(point ptr, @rc), 2 )
                    ShowTopTabsListContextMenu( rc )
                    Return true   ' prevent further processing that leads to WM_CONTEXTMENU

            end select
            return false

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function    


' ========================================================================================
' frmTopTabsMenu_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function frmTopTabsMenu_TooltipCallback( byval hCtl as HWND, byval row as integer ) as DWSTRING
    dim as integer itemData = PsStatusBar_GetItemData( hCtl, row )
    if itemData = 0 then return ""
    return frmTopTabsMenu_FormatTooltip(itemData)
end function


' ========================================================================================
' frmTopTabs_Show
' ========================================================================================
function frmTopTabsMenu_Show( byval hwndParent as HWND ) as LRESULT

    ' Create the statusBar to be used as the icon menu buttons.
    dim as HWND hCtrl = PsStatusBar_Create( hwndParent, IDC_FRMTOPTABSMENU_ICONBAR )
    
    dim as integer nWidth = AfxScaleX( (PANEL_ICON_HEIGHT * 4) + 10 )
    SetWindowPos( hCtrl, 0, 0, 0, nWidth, 0, SWP_NOZORDER )
    
    PsStatusBar_SetPaintCallback( hCtrl, @frmTopTabsMenu_PaintCallback )
    PsStatusBar_SetMessageCallback( hCtrl, @frmTopTabsMenu_MessageCallback )
    PsStatusBar_SetTooltipCallback( hCtrl, @frmTopTabsMenu_TooltipCallback )
    PsStatusBar_SetBackColor( hCtrl, ghTopTabs.BackColor )
    PsStatusBar_SetPadding( hCtrl, 7 )
    PsStatusBar_SetFont( hCtrl, ghFont(SYMBOLFONT_10) )

    PsStatusBar_AddPanel( hCtrl, "", 0 )
    PsStatusBar_AddPanel( hCtrl, wszIconChevronLeft, IDM_GOTOPREVTAB )
    PsStatusBar_AddPanel( hCtrl, wszIconChevronRight, IDM_GOTONEXTTAB )
    PsStatusBar_AddPanel( hCtrl, wszIconSplitEditor, IDM_EDITORSPLIT )
    PsStatusBar_AddPanel( hCtrl, wszIconMoreActions, IDM_TABSLIST )
    
    ' Panel 0 spring ensures that the icon panel becomes flushed right
    PsStatusBar_SetExpandPanel( hCtrl, 0 )

    dim as HWND hTooltip = PsStatusBar_GetTooltipHandle(hCtrl)
    if hTooltip then 
        ' Set the tooltip max width in order to handle the multiline
        ' text needed for the split editor button.
        SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, 300 )
    end if

    HWND_FRMMAIN_TOPTABSMENU = hCtrl
    
    function = 0
    
end function

