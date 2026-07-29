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
'
' Replaces PsIconPanel's built-in painter for every item. The built-in fills the whole
' cell; this one draws a rounded pill inside a vertically shrunk cell instead, which is
' the look the statusbar version had. The control has already filled the client with
' colors.BackColor, so the area outside the pill needs no painting here.
' ========================================================================================
sub frmPanelMenu_PaintCallback( byval p as PSICONPANEL_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return
    if p = null then return

    ' State priority for the icon background/foreground: hot > normal
    dim as COLORREF backclr, foreclr
    if p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
    end if
    p->b->SetColors(foreclr, backclr)

    p->b->SetFont( ghFont(SYMBOLFONT_10) )

    dim as long id = PsIconPanel_GetItemID( p->hIconPanel, p->itemID )

    ' Draw the icon
    dim as RECT rc = p->rc

    ' Shrink the drawing rect top and height for better icon drawing squareness.
    dim as integer py = pWindow->ScaleX(6)
    rc.top += py:  rc.bottom -= py
    p->b->PaintIconButton( p->wszGlyph, @rc )

    ' Should we underline this icon as actively selected
    dim as boolean bDrawUnderline = false
    if (id = IDM_VIEWEXPLORER) andalso _
        (gPanel.hActiveChild = HWND_FRMEXPLORER) then bDrawUnderline = true
    if (id = IDM_FUNCTIONLIST) andalso _
        (gPanel.hActiveChild = HWND_FRMFUNCTIONS) then bDrawUnderline = true
    if (id = IDM_BOOKMARKSLIST) andalso _
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
' frmPanelMenu_ClickCallback  -- a COMMAND item was clicked (matched press + release)
'
' The press/cancel gesture belongs to the control now: pressing an icon, sliding off and
' releasing fires nothing, where the statusbar version acted on any WM_LBUTTONUP.
' ========================================================================================
sub frmPanelMenu_ClickCallback( byval hCtl as HWND, byval idx as long, byval id as long )
    if id = 0 then return
    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(id, 0), 0 )
end sub


' ========================================================================================
' frmPanelMenu_TooltipCallback  -- per-item tooltip text, resolved on demand
' ========================================================================================
function frmPanelMenu_TooltipCallback( byval hCtl as HWND, byval idx as long ) as DWSTRING
    dim as long id = PsIconPanel_GetItemID( hCtl, idx )
    if id = 0 then return ""
    return frmPanelMenu_FormatTooltip(id)
end function


' ========================================================================================
' frmPanelMenu_CreateBar  -- one of the two icon groups
' ========================================================================================
private function frmPanelMenu_CreateBar( _
            byval hwndParent as HWND, _
            byval CtrlID as long, _
            byval nJustify as long _
            ) as HWND

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)

    dim as HWND hCtrl = PsIconPanel_Create( hwndParent, CtrlID )

    PsIconPanel_SetPaintItemCallback( hCtrl, @frmPanelMenu_PaintCallback )
    PsIconPanel_SetClickCallback( hCtrl, @frmPanelMenu_ClickCallback )
    PsIconPanel_SetTooltipCallback( hCtrl, @frmPanelMenu_TooltipCallback )
    ' Owner-drawn tips. The text policy above is untouched -- PsTipHost switches how a tip
    ' is drawn and driven, never what it says.
    PsIconPanel_SetTooltipMode( hCtrl, PSTIP_MODE_PS )
    PsIconPanel_SetFont( hCtrl, ghFont(SYMBOLFONT_10) )
    PsIconPanel_SetJustify( hCtrl, nJustify )

    ' These setters take RAW pixels -- the caller DPI-scales (the family rule; only the
    ' Create-time defaults are scaled for you).
    if pWindow then
        PsIconPanel_SetPadding( hCtrl, pWindow->ScaleX(2), pWindow->ScaleX(2) )
        PsIconPanel_SetIconSize( hCtrl, pWindow->ScaleX(20), pWindow->ScaleY(20) )
    end if

    return hCtrl
end function


' ========================================================================================
' frmPanelMenu_ApplyTheme  -- push the current panel colours into both bars
' ========================================================================================
sub frmPanelMenu_ApplyTheme()
    dim as PSICONPANEL_COLORS colors
    colors.BackColor         = ghPanel.BackColor
    colors.ForeColor         = ghPanel.ForeColor
    colors.BackColorHot      = ghPanel.BackColorHot
    colors.ForeColorHot      = ghPanel.ForeColorHot
    colors.BackColorSelect   = ghPanel.BackColorHot
    colors.ForeColorSelect   = ghPanel.ForeColorHot
    colors.ForeColorDisabled = ghPanel.ForeColor
    colors.SeparatorColor    = ghPanel.ForeColor

    if IsWindow(HWND_FRMPANEL_MENU) then _
        PsIconPanel_SetColors( HWND_FRMPANEL_MENU, @colors )
    if IsWindow(HWND_FRMPANEL_MENU_RIGHT) then _
        PsIconPanel_SetColors( HWND_FRMPANEL_MENU_RIGHT, @colors )
end sub


' ========================================================================================
' frmPanelMenu_Show
'
' TWO controls, not one. PsIconPanel has no spring panel: it declares every cell and moves
' the whole run as a block, so the left-hand and right-hand icon groups are separate
' panels sharing the strip (frmPanel_PositionWindows splits the width between them).
' ========================================================================================
function frmPanelMenu_Show( byval hwndParent as hwnd ) as LRESULT

    dim as HWND hLeft  = frmPanelMenu_CreateBar( hwndParent, IDC_FRMPANELMENU_ICONBAR, IP_JUSTIFY_LEFT )
    dim as HWND hRight = frmPanelMenu_CreateBar( hwndParent, IDC_FRMPANELMENU_ICONBAR_RIGHT, IP_JUSTIFY_RIGHT )

    PsIconPanel_AddItem( hLeft, IP_KIND_COMMAND, wszIconExplorer, IDM_VIEWEXPLORER )
    PsIconPanel_AddItem( hLeft, IP_KIND_COMMAND, wszIconFunctions, IDM_FUNCTIONLIST )
    PsIconPanel_AddItem( hLeft, IP_KIND_COMMAND, wszIconBookmarks, IDM_BOOKMARKSLIST )
    PsIconPanel_AddItem( hLeft, IP_KIND_COMMAND, wszIconSettings, IDM_OPTIONSDIALOG )   ' Environment Options

    PsIconPanel_AddItem( hRight, IP_KIND_COMMAND, wszIconFind, IDM_FINDINPROJECT )
    PsIconPanel_AddItem( hRight, IP_KIND_COMMAND, wszIconSave, IDM_FILESAVEALL )
    PsIconPanel_AddItem( hRight, IP_KIND_COMMAND, wszIconDebug, IDM_DEBUG_STARTDEBUGGING )
    PsIconPanel_AddItem( hRight, IP_KIND_COMMAND, wszIconCompile, IDM_COMPILE )
    PsIconPanel_AddItem( hRight, IP_KIND_COMMAND, wszIconBuildExecute, IDM_BUILDEXECUTE )

    HWND_FRMPANEL_MENU       = hLeft
    HWND_FRMPANEL_MENU_RIGHT = hRight

    frmPanelMenu_ApplyTheme()

    function = 0

end function
