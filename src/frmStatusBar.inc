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

#include once "frmStatusBar.bi"


' ========================================================================================
' frmStatusBar_PaintCallback
' ========================================================================================
sub frmStatusBar_PaintCallback( byval p as CSTATUSBAR_PAINTINFO ptr )

    ' Panel state: hot > normal. (No selected state -- a statusbar has no selection.)
    ' Set default fore and back colors
    dim as COLORREF backclr = ghStatusBar.BackColor
    dim as COLORREF foreclr = ghStatusBar.ForeColor
    
    if (p->isHot = true) andalso (p->itemID <> COMPILE_STATUS_PANEL) then
        backclr = ghStatusBar.BackColorHot
        foreclr = ghStatusBar.ForeColorHot
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Paint the entire panel background
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc

    ' Display the caption text
    ' Default to using the GUI font. We use the Symbol font only for the icons
    p->b->SetFont( ghFont(GUIFONT_9) )
    if (p->itemID = BUILD_DIALOG_PANEL) orelse (p->itemID = THEMES_DIALOG_PANEL) then
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
    end if
    p->b->PaintText( p->wszCaption, @rcText, DT_CENTER )


    ' if this is the compile result panel then display the green/success or red/failure "icon"
    ' or a Spinner if compiling is in progress. 
    if p->itemID = COMPILE_STATUS_PANEL then
        if (gApp.IsProjectLoading = false) andalso (gApp.IsFileLoading = false) then
            dim as DWSTRING wszText 
            
            ' Calculate where the spinner or good/fail compile will display to the 
            ' left of the panel text.
            if p->wszCaption = "" then gApp.hIconPanel = 0
            dim as COLORREF clr = ghStatusBar.BackColor
            if gApp.hIconPanel = ghIconGood then clr = ghGeneral.iconsuccess
            if gApp.hIconPanel = ghIconBad then clr = ghGeneral.iconfail
            p->b->SetForeColor( clr )
            p->b->SetBackColor( ghStatusBar.BackColor )

            dim as long txtWidth = getTextWidth( HWND_FRMMAIN_STATUSBAR, p->wszCaption, GUIFONT_9 ) 
            dim as RECT rcIcon = p->rc
            dim as long halfWidth = (rcIcon.right-rcIcon.left) / 2
            rcIcon.right = rcIcon.left + halfWidth - AfxScaleX((txtWidth / 2))
            rcIcon.left = rcIcon.right - AfxScaleX(16) 
            wszText = wszIconCompileResult    ' circle
            
            ' If a Spinner is active then that will take precedence over the good/bad compile icon.
            if (gApp.bShowSpinner = true) then
                p->b->SetForeColor( ghStatusBar.ForeColor )
                wszText = @spinner(gApp.SpinnerCurrentFrame)
                p->b->SetBackColor( ghStatusBar.BackColor )
                ' Position the spinner a bit better
                rcIcon.top -= AfxScaleY(4)
                rcIcon.left -= AfxScaleY(2)
            end if

            p->b->SetFont( ghFont(SYMBOLFONT_9) )
            p->b->PaintText( wszText, @rcIcon, DT_CENTER )
        end if

    end if   

end sub


' ========================================================================================
' frmStatusBar_MessageCallback
' ========================================================================================
function frmStatusBar_MessageCallback( byval m as CSTATUSBAR_MESSAGEINFO ptr ) as boolean

    select case m->uMsg

        case WM_MOUSEHOVER
            ' Tooltips are on-demand via the tooltip frmStatusBar_TooltipCallback; nothing to
            ' do here for hover.
            return true

        case WM_LBUTTONUP
            ' No capture is taken, so an up only arrives while the cursor is still over the
            ' control: idx = -1 means "released over empty bar area". We simply need to 
            ' retrieve the command message stored in itemData and post it to frmMain.
            if m->idx <> -1 then
                dim as integer idMessage = CStatusBar_GetItemData(m->hStatusBar, m->idx)
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(idMessage, 0), 0 )
            end if
            return false

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function


' ========================================================================================
' frmStatusBar_TooltipCallback  -- per-panel tooltip text, resolved on demand
' ========================================================================================
function frmStatusBar_TooltipCallback( byval hCtl as HWND, byval idx as integer ) as DWSTRING
    dim as DWSTRING wszTooltip = ""
    
    ' Only some of the panels actually have tooltips
    select case idx 
        case COMPILE_STATUS_PANEL:   wszTooltip = L(251,"View Output Window")
        case THEMES_DIALOG_PANEL:    wszTooltip = L(111,"Themes and Fonts")
        case BUILD_DIALOG_PANEL:     wszTooltip = L(277,"Build Configurations") & "..."
    end select

    return wszTooltip
end function


' ========================================================================================
' frmStatusBar_Show
' ========================================================================================
function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

    ' Create the StatusBar custom control
    dim as HWND hCtrl = CStatusBar_Create( hwndParent, IDC_FRMMAIN_STATUSBAR )
    SetWindowPos( hCtrl, 0, 0, 0, 0, AfxScaleY(STATUSBAR_HEIGHT), SWP_NOZORDER )
    
    CStatusBar_SetPaintCallback( hCtrl, @frmStatusBar_PaintCallback )
    CStatusBar_SetMessageCallback( hCtrl, @frmStatusBar_MessageCallback )
    CStatusBar_SetTooltipCallback( hCtrl, @frmStatusBar_TooltipCallback )
    CStatusBar_SetBackColor( hCtrl, ghStatusBar.BackColor )
    CStatusBar_SetFont( hCtrl, ghFont(GUIFONT_9) )

    CStatusBar_AddPanel( hCtrl, "", IDM_GOTOLINE )
    CStatusBar_AddPanel( hCtrl, "", IDM_VIEWOUTPUT )            ' the spring
    CStatusBar_AddPanel( hCtrl, "", IDM_THEMES )
    CStatusBar_AddPanel( hCtrl, "", IDM_BUILDCONFIG )
    CStatusBar_AddPanel( hCtrl, "", IDM_BUILDCONFIG_POPUP )
    CStatusBar_AddPanel( hCtrl, "", IDM_SPACES )
    CStatusBar_AddPanel( hCtrl, "", IDM_FILEENCODING )
    CStatusBar_AddPanel( hCtrl, "", IDM_LINEENDINGS )

    ' (panels are zero based)

    ' The Ln/Col readout changes length constantly; floor it so it stops shoving its
    ' neighbours around on every caret move.
    CStatusBar_SetPanelMinWidth( hCtrl, 0, AfxScaleX(150) )

    ' Panel 1 absorbs all unused width 
    CStatusBar_SetExpandPanel( hCtrl, 1 )

    HWND_FRMMAIN_STATUSBAR = hCtrl
    
    function = 0
    
end function

