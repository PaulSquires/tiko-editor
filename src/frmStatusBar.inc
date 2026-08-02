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
sub frmStatusBar_PaintCallback( byval p as PSSTATUSBAR_PAINTINFO ptr )

    ' Panel state: hot > normal. (No selected state -- a statusbar has no selection.)
    ' The Ln/Col also has no selection
    ' Set default fore and back colors
    dim as COLORREF backclr = theme.statusbar.text.backcolor
    dim as COLORREF foreclr = theme.statusbar.text.forecolor
    
    if (p->itemID <> COMPILE_STATUS_PANEL) andalso _
         (p->itemID <> GOTO_PANEL) then
        if (p->isHot = true) then
            backclr = theme.statusbar.texthot.backcolor
            foreclr = theme.statusbar.texthot.forecolor
        end if    
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
            dim as COLORREF clr = theme.statusbar.text.backcolor
            if gApp.hIconPanel = ghIconGood then clr = theme.compile.iconsuccess
            if gApp.hIconPanel = ghIconBad then clr = theme.compile.iconfail
            p->b->SetForeColor( clr )
            p->b->SetBackColor( theme.statusbar.text.backcolor )

            dim as long txtWidth = getTextWidth( HWND_FRMMAIN_STATUSBAR, p->wszCaption, GUIFONT_9 ) 
            dim as RECT rcIcon = p->rc
            dim as long halfWidth = (rcIcon.right-rcIcon.left) / 2
            rcIcon.right = rcIcon.left + halfWidth - AfxScaleX((txtWidth / 2))
            rcIcon.left = rcIcon.right - AfxScaleX(16) 
            wszText = wszIconCompileResult    ' circle
            
            ' If a Spinner is active then that will take precedence over the good/bad compile icon.
            if (gApp.bShowSpinner = true) then
                p->b->SetForeColor( theme.statusbar.text.forecolor )
                wszText = spinner(gApp.SpinnerCurrentFrame)
                p->b->SetBackColor( theme.statusbar.text.backcolor )
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
function frmStatusBar_MessageCallback( byval m as PSSTATUSBAR_MESSAGEINFO ptr ) as boolean

    select case m->uMsg

        case WM_MOUSEHOVER
            ' Tooltips are on-demand via the tooltip frmStatusBar_TooltipCallback; nothing to
            ' do here for hover.
            return true

        case WM_LBUTTONUP
            ' No capture is taken, so an up only arrives while the cursor is still over the
            ' control: idx = -1 means "released over empty bar area". We simply need to 
            ' retrieve the command message stored in itemData and post it to frmMain.
            ' An itemData of 0 is a panel with no command attached (the Ln/Col readout) --
            ' posting it would send WM_COMMAND id 0 to frmMain for nothing.
            if m->idx <> -1 then
                dim as integer idMessage = PsStatusBar_GetItemData(m->hStatusBar, m->idx)
                if idMessage <> 0 then
                    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(idMessage, 0), 0 )
                end if
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
        case COMPILE_STATUS_PANEL:   wszTooltip = L(201,"View Output Window")
        case THEMES_DIALOG_PANEL:    wszTooltip = L(329,"Themes")
        case BUILD_DIALOG_PANEL:     wszTooltip = L(219,"Build Configurations") & "..."
    end select

    return wszTooltip
end function


' ========================================================================================
' frmStatusBar_Show
' ========================================================================================
function frmStatusBar_Show( byval hwndParent as HWND ) as LRESULT

    ' Create the StatusBar custom control
    dim as HWND hCtrl = PsStatusBar_Create( hwndParent, IDC_FRMMAIN_STATUSBAR )
    SetWindowPos( hCtrl, 0, 0, 0, 0, AfxScaleY(STATUSBAR_HEIGHT), SWP_NOZORDER )
    
    PsStatusBar_SetPaintCallback( hCtrl, @frmStatusBar_PaintCallback )
    PsStatusBar_SetMessageCallback( hCtrl, @frmStatusBar_MessageCallback )
    PsStatusBar_SetTooltipCallback( hCtrl, @frmStatusBar_TooltipCallback )
    PsStatusBar_SetTooltipMode( hCtrl, PSTIP_MODE_PS )
    PsStatusBar_SetBackColor( hCtrl, theme.statusbar.text.backcolor )
    PsStatusBar_SetFont( hCtrl, ghFont(GUIFONT_9) )

    ' The Ln/Col readout is INERT -- itemData 0 means "no command", which the message
    ' callback below skips. It is excluded from the hot state in the painter for the same
    ' reason: there is nothing to click.
    PsStatusBar_AddPanel( hCtrl, "" )
    PsStatusBar_AddPanel( hCtrl, "", IDM_VIEWOUTPUT )            ' the spring
    PsStatusBar_AddPanel( hCtrl, "", IDM_THEMES )
    PsStatusBar_AddPanel( hCtrl, "", IDM_BUILDCONFIG )
    PsStatusBar_AddPanel( hCtrl, "", IDM_BUILDCONFIG_POPUP )
    PsStatusBar_AddPanel( hCtrl, "", IDM_SPACES )
    PsStatusBar_AddPanel( hCtrl, "", IDM_FILEENCODING )
    PsStatusBar_AddPanel( hCtrl, "", IDM_LINEENDINGS )

    ' (panels are zero based)

    ' The Ln/Col readout changes length constantly; floor it so it stops shoving its
    ' neighbours around on every caret move.
    PsStatusBar_SetPanelMinWidth( hCtrl, 0, AfxScaleX(100) )

    ' Panel 1 absorbs all unused width 
    PsStatusBar_SetExpandPanel( hCtrl, 1 )

    HWND_FRMMAIN_STATUSBAR = hCtrl
    
    function = 0
    
end function

