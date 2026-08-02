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


#include once "frmPanelMenu.bi"


' This bar's tooltip. A PsTooltip rather than the comctl32 tip built by hand here before: it
' is themed, it wraps without the hand-sent TTM_SETMAXTIPWIDTH that used to follow the create,
' and it adds no pump obligation. One tip for the window; the hot rect is carried by the TOOL
' INDEX, set from WM_MOUSEMOVE, so the hit-test runs once on the message rather than a second
' time against a cursor that has since moved.
dim shared ghReplaceTip as HWND

' The bar's three icon buttons are two PsIconPanels, split where the strip is split:
'   FIELD   - Preserve Case, INSIDE the rounded replace frame.
'   ACTIONS - Replace, Replace All, outside it.
' The Find bar next door is the same arrangement with a third panel; this one needs no
' separator and no close button, so two panels cover it. Deliberately NOT sharing
' frmFind's callbacks: the latched button and the ids are this bar's, and reaching into a
' sibling form's private painter to save twenty lines would couple the two files far more
' tightly than they are today.
dim shared ghReplaceIconsField   as HWND
dim shared ghReplaceIconsActions as HWND

' These are BOTH the PsTooltip tool index AND each item's PsIconPanel id, which is what
' lets one click callback and one message callback serve both panels.
enum
    REPLACETIP_NONE = -1
    REPLACETIP_PRESERVE = 0
    REPLACETIP_REPLACE
    REPLACETIP_REPLACEALL
end enum


' ========================================================================================
' Supply the tooltip text for the currently hot rect.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi). Returning "" means no tip for the rest of this hover.
private function frmReplace_TipText( byval hTip as HWND, _
                                            byval hAttached as HWND, _
                                            byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    select case idx
        case REPLACETIP_PRESERVE:    return L(276,"Preserve Case")
        case REPLACETIP_REPLACE:     return L(139,"Replace") & " (Enter)"
        case REPLACETIP_REPLACEALL:  return L(140,"Replace All") & " (Ctrl+Alt+Enter)"
    end select
    return ""
end function


' ========================================================================================
' Draw one icon button, instead of PsIconPanel's built-in painter.
' ========================================================================================
' Shared by both panels. Three things the built-in painter cannot do here:
'   * Preserve Case latches from gFind.nPreserve -- IP_KIND_COMMAND, not IP_KIND_TOGGLE, so
'     the control never keeps a second copy of state FindReplace_DoReplace already reads.
'     Asking the model at paint time cannot go stale.
'   * The latched fill is IconSelectedBackColor, a theme key the control knows nothing of.
'   * The bar's hover INVERTS: the strip's field colour is BackColorHot, so a hovered
'     unselected button goes to the tab area's normal BackColor.
' The glyph font is read back from the panel, which keeps this one callback correct for the
' field panel (GUIFONT_9) and the actions panel (SYMBOLFONT_9) at the same time.
private sub frmReplace_IconPaint( byval p as PSICONPANEL_PAINTINFO ptr )
    ' Fill the whole cell first: p->rc carries the item's padding, and anything left
    ' unpainted shows the panel background through.
    p->b->SetBackColor( theme.toptabs.texthot.backcolor )
    p->b->PaintRect( @p->rc )

    dim as long id = PsIconPanel_GetItemID( p->hIconPanel, p->itemID )
    if (id = REPLACETIP_PRESERVE) andalso (gFind.nPreserve <> 0) then
        p->b->SetBackColor( theme.toptabs.iconselected )
    else
        p->b->SetBackColor( iif( p->isHot orelse p->isPressed, _
                                 theme.toptabs.text.backcolor, theme.toptabs.texthot.backcolor ) )
    end if
    p->b->SetForeColor( theme.toptabs.texthot.forecolor )
    p->b->SetFont( PsIconPanel_GetFont( p->hIconPanel ) )
    p->b->PaintIconButton( p->wszGlyph, @p->rc )
end sub


' ========================================================================================
' Carry the hot item into the bar's PsTooltip.
' ========================================================================================
' One tip serves the whole strip -- it is attached to the PARENT, and PsTooltip's
' cursor-over test accepts a hit on any descendant. But a panel's client area is the
' parent's dead zone, so both panels report from here. Returns FALSE always: this
' observes, it never suppresses.
private function frmReplace_IconMessage( byval m as PSICONPANEL_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
        case WM_MOUSEMOVE
            dim as long idTip = REPLACETIP_NONE
            if m->idx >= 0 then idTip = PsIconPanel_GetItemID( m->hIconPanel, m->idx )
            PsTooltip_SetToolIndex( ghReplaceTip, idTip )
        case WM_MOUSELEAVE
            PsTooltip_SetToolIndex( ghReplaceTip, REPLACETIP_NONE )
    end select
    return false
end function


' ========================================================================================
' A matched press+release on one of the icon buttons.
' ========================================================================================
' Fires only for a completed gesture -- press a button, slide off, release and nothing
' happens. The old WM_LBUTTONUP arm acted on wherever the cursor was at the UP, whatever
' the DOWN had hit. The bodies are the old arms verbatim.
private sub frmReplace_IconClick( byval hIconPanel as HWND, byval idx as long, byval id as long )
    select case id
        case REPLACETIP_PRESERVE
            gFind.nPreserve = not gFind.nPreserve
            AfxRedrawWindow( hIconPanel )

        case REPLACETIP_REPLACE
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACENEXT, 0), 0 )

        case REPLACETIP_REPLACEALL
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEALL, 0), 0 )
    end select
end sub


' ========================================================================================
' Push the cell metrics into one panel. Both values are ALREADY DPI-SCALED.
' ========================================================================================
' Cells are square and as tall as the replace field, and both of those depend on the bar's
' current size -- so this runs from PositionWindows on every resize, not once at create.
' Padding is zeroed: the icons butt up against each other exactly as the old rects did.
private sub frmReplace_SetIconMetrics( byval hPanel as HWND, byval nIconW as long, byval nIconH as long )
    if IsWindow(hPanel) = 0 then exit sub
    PsIconPanel_SetIconSize( hPanel, nIconW, nIconH )
    PsIconPanel_SetPadding( hPanel, 0, 0 )
end sub


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmReplace_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_REPLACE)
    if pWindow = 0 then exit function

    dim as HWND hCtlReplace = GetDlgItem(HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE)
    
    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_REPLACE, @rc )
            
    ' Calculate the RECT positions for all of the top tabs
    dim as long iconWidth
    dim as long iconHeight 
    dim as long nPadding = 10   ' 5 left 5 right
    dim as long vmargin = pWindow->ScaleY(4)
    dim as long nLeft
    dim as long nTop
    dim as long nWidth 
    dim as long nHeight
    
    gTTabCtl.rcReplaceTextRect = rc
    gTTabCtl.rcReplaceTextRect.left   = gTTabCtl.rcFindTextRect.left
    gTTabCtl.rcReplaceTextRect.top    = 0
    gTTabCtl.rcReplaceTextRect.right  = gTTabCtl.rcFindTextRect.right
    gTTabCtl.rcReplaceTextRect.bottom = pWindow->ScaleY(TOPTABS_REPLACE_HEIGHT) - pWindow->ScaleY(8)

    rc      = gTTabCtl.rcReplaceTextRect
    nLeft   = rc.left + vmargin + pWindow->ScaleX(10)
    nTop    = rc.top + vmargin
    nWidth  = (rc.right - rc.left - vmargin*2 - pWindow->ScaleX(10))
    nHeight = (rc.bottom - rc.top - vmargin*2)

    ' Make the width of the textbox smaller because we want to visually fit the
    ' Preserve Case icon into the Replace text rect. nHeight will be our icon 
    ' height and width as well.
    iconWidth = nHeight
    iconHeight = nHeight
    nWidth = nWidth - iconWidth - pWindow->ScaleX(4)

    ' Vertical centering and colors are the PsTextBox control's job.
    SetWindowPos( hCtlReplace, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

    ' The icon cells are square and as tall as the field, and the panels declare them --
    ' which is why the widths below are asked of the control rather than multiplied out
    ' here. Every setter takes RAW PIXELS (the family rule: only the Create-time defaults
    ' are scaled for us), so iconWidth/nHeight go in already scaled.
    frmReplace_SetIconMetrics( ghReplaceIconsField,   iconWidth, nHeight )
    frmReplace_SetIconMetrics( ghReplaceIconsActions, iconWidth, nHeight )

    ' FIELD panel: Preserve Case, inside the rounded replace frame, hard against the
    ' textbox's right edge.
    SetWindowPos( ghReplaceIconsField, 0, _
                  nLeft + nWidth, nTop, PsIconPanel_GetIdealWidth( ghReplaceIconsField ), nHeight, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' ACTIONS panel: Replace + Replace All, outside the frame.
    SetWindowPos( ghReplaceIconsActions, 0, _
                  rc.right + pWindow->ScaleX(8), nTop, _
                  PsIconPanel_GetIdealWidth( ghReplaceIconsActions ), nHeight, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    AfxRedrawWindow( HWND_FRMMAIN_REPLACE )

    function = 0
end function


' ========================================================================================
' PsTextBox callback for the Replace textbox. The control owns the cue banner, colors,
' vertical centering, single-line enforcement and the Cut/Copy/Paste context menu that
' the old RichEdit subclass hand-rolled here. ENTER/TAB handling stays in
' handleKeysFindReplace (modMsgPump). No change callback: the replace text is read on
' demand by FindReplace_DoReplace.
' ========================================================================================
sub frmReplace_TextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    ' Was the EN_SETFOCUS path: repaint the panel chrome around the textbox.
    if bGotFocus then AfxRedrawWindow( HWND_FRMMAIN_REPLACE )
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmReplace
' ========================================================================================
function frmReplace_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmReplace_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmReplace
' ========================================================================================
function frmReplace_OnCommand( _
            byval hwnd as HWND, _
            byval id as Long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    ' The Replace textbox notifies through its PsTextBox focus callback, not through
    ' WM_COMMAND EN_ codes anymore.
    function = 0
end function


' ========================================================================================
' frmReplace Window procedure
' ========================================================================================
function frmReplace_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmReplace_OnSize)
        HANDLE_MSG (hwnd, WM_COMMAND, frmReplace_OnCommand)

    case WM_DESTROY
        ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with the
        ' parent -- destroying it here is also what removes it from the shared clock's registry.
        if IsWindow(ghReplaceTip) then DestroyWindow( ghReplaceTip )
        ghReplaceTip = 0

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_REPLACE = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_REPLACE)
        if pWindow = 0 then exit function

        ' TME_LEAVE only. TME_HOVER existed solely to drive the old WM_MOUSEHOVER tooltip arm;
        ' PsTooltip does its own dwell timing off the shared clock.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)

        ' Nothing this window still paints itself has a tip -- the field frame never had one
        ' -- so being over the PARENT means no tip at all. The three that do have tips are
        ' the panels', reported from frmReplace_IconMessage.
        PsTooltip_SetToolIndex( ghReplaceTip, REPLACETIP_NONE )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghReplaceTip, REPLACETIP_NONE )
        AfxRedrawWindow( hwnd )


    ' No WM_LBUTTONUP arm: every button on this bar belongs to a PsIconPanel now and
    ' reports through frmReplace_IconClick, on a matched press+release.

    case WM_PAINT
        dim as PsBufferPaint b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetForeColor( theme.toptabs.texthot.forecolor )
        b.SetBackColor( theme.toptabs.texthot.backcolor )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Paint the bottom line that separates from the code editing window.
        b.SetPenColor( theme.editor.divider )
        b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )

        ' Draw the Replace controls. The strip's field color is BackColorHot, so
        ' a hovered (unselected) button inverts to the tab area's normal BackColor.
        b.SetColors( theme.toptabs.texthot.forecolor, theme.toptabs.texthot.backcolor )
        b.SetPenColor( theme.editor.divider )

        b.PaintRoundBorderRect( @gTTabCtl.rcReplaceTextRect )

        ' The three icon buttons are the two PsIconPanels' own client areas
        ' (frmReplace_IconPaint). All that is left to this window is the frame around the
        ' replace field and the divider line under the bar.

        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmReplace_Show
' ========================================================================================
function frmReplace_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_REPLACE = pWindow->Create( hwndParent, @frmReplace_WndProc, _
        0, 0, 0, TOPTABS_REPLACE_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    ' The Replace textbox is the reusable PsTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as HWND hCtlReplace = PsTextBox_Create( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_TXTREPLACE )
    PsTextBox_SetFont( hCtlReplace, ghFont(INFOFONT_11) )
    PsTextBox_SetForeColor( hCtlReplace, theme.toptabs.texthot.forecolor )
    PsTextBox_SetBackColor( hCtlReplace, theme.toptabs.texthot.backcolor )
    PsTextBox_SetBorderWidth( hCtlReplace, 0 )                    ' flat, as before
    PsTextBox_SetCueBannerText( hCtlReplace, L(139,"Replace") & "..." )
    PsTextBox_SetCueBannerColor( hCtlReplace, theme.toptabs.text.forecolor )
    PsTextBox_SetFocusCallback( hCtlReplace, @frmReplace_TextBoxFocus )
    PsTextBox_SetText( hCtlReplace, gFind.txtReplace )            ' carried-over replace term
    ThemeTextBoxContextMenu( hCtlReplace )

    ' The two icon strips. IP_KIND_COMMAND throughout -- see frmReplace_IconPaint for why
    ' Preserve Case is not IP_KIND_TOGGLE. Item ids are the REPLACETIP_* values. Cell
    ' metrics are pushed per resize by frmReplace_SetIconMetrics, not here.
    '
    ' No tooltip TEXT and no tooltip CALLBACK on either: the tips are the bar's PsTooltip,
    ' so each control's own comctl32 tip is left with nothing to say.
    ghReplaceIconsField = PsIconPanel_Create( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_ICONSFIELD )
        PsIconPanel_SetFont( ghReplaceIconsField, ghFont(GUIFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghReplaceIconsField, @frmReplace_IconPaint )
        PsIconPanel_SetMessageCallback( ghReplaceIconsField, @frmReplace_IconMessage )
        PsIconPanel_SetClickCallback( ghReplaceIconsField, @frmReplace_IconClick )
        PsIconPanel_AddItem( ghReplaceIconsField, IP_KIND_COMMAND, wszIconPreserveCase, REPLACETIP_PRESERVE )

    ghReplaceIconsActions = PsIconPanel_Create( HWND_FRMMAIN_REPLACE, IDC_FRMREPLACE_ICONSACTIONS )
        PsIconPanel_SetFont( ghReplaceIconsActions, ghFont(SYMBOLFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghReplaceIconsActions, @frmReplace_IconPaint )
        PsIconPanel_SetMessageCallback( ghReplaceIconsActions, @frmReplace_IconMessage )
        PsIconPanel_SetClickCallback( ghReplaceIconsActions, @frmReplace_IconClick )
        PsIconPanel_AddItem( ghReplaceIconsActions, IP_KIND_COMMAND, wszIconReplace, REPLACETIP_REPLACE )
        PsIconPanel_AddItem( ghReplaceIconsActions, IP_KIND_COMMAND, wszIconReplaceAll, REPLACETIP_REPLACEALL )

    Theme_ApplyReplaceIcons()

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so a long tip still
    ' wraps at the same place; colours and fonts are pushed per hover from
    ' frmReplace_TipText, so nothing set here can go stale on a theme change.
    ghReplaceTip = PsTooltip_Create( HWND_FRMMAIN_REPLACE )
    PsTooltip_SetToolIndex( ghReplaceTip, REPLACETIP_NONE )
    PsTooltip_SetMaxWidth( ghReplaceTip, 200 )
    PsTooltip_SetTooltipCallback( ghReplaceTip, @frmReplace_TipText )

    function = 0
    
end function

