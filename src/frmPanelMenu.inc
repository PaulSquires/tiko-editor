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
''  frmPanelMenu.inc
''   

'#include once "frmPanel.bi"
#include once "frmPanelMenu.bi"


' ========================================================================================
' Format the tooltip based on menu id (itemData)
' ========================================================================================
function frmPanelMenu_FormatTooltip( byval id as integer ) as DWSTRING
    dim as DWSTRING wszTooltip = getMenuText(id)
    dim as DWSTRING wszCaption = rtrim(AfxStrParse(wszTooltip, 1, wchr(9)), "...")
    dim as DWSTRING wszShortcut = AfxStrParse(wszTooltip, 2, wchr(9))
    
    if len(wszShortcut) then wszShortcut = " (" & wszShortcut & ")"
    
    return wszCaption & wszShortcut
end function


' ========================================================================================
' frmPanelMenu_PaintCallback
' ========================================================================================
sub frmPanelMenu_PaintCallback( byval p as CSTATUSBAR_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 
    if p = null then return

    ' State priority for the row background/foreground: selected > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
    end if
    p->b->SetColors(foreclr, backclr)

    ' If this is the Spring panel then we don't want any highlighting because it is
    ' just a placeholder that separates the left and right icons.
    if CStatusBar_GetItemData(p->hStatusBar, p->itemID) = 0 then
        p->b->SetColors(ghPanel.BackColor, ghPanel.BackColor)
    end if

    ' Don't paint the background because we will reduce the size of the drawing
    ' rect and we don't want the outside of that rect to be highlighted.
'    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(SYMBOLFONT_10) )

    dim as integer itemData = CStatusBar_GetItemData( p->hStatusBar, p->itemID )
    
    ' Draw the icon
    dim as RECT rc = p->rc
    
    ' Shrink the drawing rect top and height for better icon drawing squareness.
    dim as integer py = pWindow->ScaleX(6)
    rc.top += py:  rc.bottom -= py
    p->b->PaintIconButton( p->wszCaption, @rc )

    ' Should we underline this icon as actively selected    
    dim as boolean bDrawUnderline = false
    if (itemData = IDM_VIEWEXPLORER) andalso _
        (gPanel.hActiveChild = HWND_FRMEXPLORER) then bDrawUnderline = true
    if (itemData = IDM_FUNCTIONLIST) andalso _
        (gPanel.hActiveChild = HWND_FRMFUNCTIONS) then bDrawUnderline = true
    if (itemData = IDM_BOOKMARKSLIST) andalso _
        (gPanel.hActiveChild = HWND_FRMBOOKMARKS) then bDrawUnderline = true
    
    if (bDrawUnderline = true) then
        rc = p->rc
        rc.left += pWindow->ScaleX(2)
        rc.right -= pWindow->ScaleX(2)
        rc.bottom -= pWindow->ScaleX(5)
        p->b->SetPenColor( ghOutput.ForeColor )
        p->b->PaintLine( 1, rc.left, rc.bottom, rc.right, rc.bottom )
    end if
        
end sub


' ========================================================================================
' frmPanelMenu_MessageCallback
' ========================================================================================
function frmPanelMenu_MessageCallback( byval m as CSTATUSBAR_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_MOUSEHOVER
            'print "WM_MOUSEHOVER on idx: "; m->idx
            ' Tooltips are now on-demand via the tooltip callback; nothing to push.
            return true
            
        case WM_LBUTTONUP
            if m->idx <> -1 then
                dim as integer idMessage = CStatusBar_GetItemData( m->hStatusBar, m->idx )
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(idMessage, 0), 0 )
            end if
            return false

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function    


' ========================================================================================
' frmPanelMenu_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function frmPanelMenu_TooltipCallback( byval hCtl as HWND, byval row as integer ) as DWSTRING
    dim as integer itemData = CStatusBar_GetItemData( hCtl, row )
    if itemData = 0 then return ""
    return frmPanelMenu_FormatTooltip(itemData)
end function


' ========================================================================================
' frmPanelMenu_Show
' ========================================================================================
function frmPanelMenu_Show( byval hwndParent as hwnd ) as LRESULT

    ' Create the statusBar to be used as the icon menu buttons.
    dim as HWND hCtrl = CStatusBar_Create( hwndParent, IDC_FRMPANELMENU_ICONBAR )
    
    CStatusBar_SetPaintCallback( hCtrl, @frmPanelMenu_PaintCallback )
    CStatusBar_SetMessageCallback( hCtrl, @frmPanelMenu_MessageCallback )
    CStatusBar_SetTooltipCallback( hCtrl, @frmPanelMenu_TooltipCallback )
    CStatusBar_SetBackColor( hCtrl, ghPanel.BackColor )
    CStatusBar_SetPadding( hCtrl, 7 )
    CStatusBar_SetFont( hCtrl, ghFont(SYMBOLFONT_10) )

    CStatusBar_AddPanel( hCtrl, wszIconExplorer, IDM_VIEWEXPLORER )
    CStatusBar_AddPanel( hCtrl, wszIconFunctions, IDM_FUNCTIONLIST )
    CStatusBar_AddPanel( hCtrl, wszIconBookmarks, IDM_BOOKMARKSLIST )
    CStatusBar_AddPanel( hCtrl, wszIconSettings, IDM_OPTIONSDIALOG )    ' Settings (Environment Options)
    
    '(Expandable spring between Right to left orientation)
    CStatusBar_AddPanel( hCtrl, "", 0 )

    CStatusBar_AddPanel( hCtrl, wszIconFind, IDM_FINDINPROJECT )
    CStatusBar_AddPanel( hCtrl, wszIconSave, IDM_FILESAVEALL )
    CStatusBar_AddPanel( hCtrl, wszIconDebug, IDM_DEBUG_STARTDEBUGGING )
    CStatusBar_AddPanel( hCtrl, wszIconCompile, IDM_COMPILE )
    CStatusBar_AddPanel( hCtrl, wszIconBuildExecute, IDM_BUILDEXECUTE )

    ' Panel 4 absorbs all unused width 
    CStatusBar_SetExpandPanel( hCtrl, 4 )

    HWND_FRMPANEL_MENU = hCtrl

    function = 0
    
end function
