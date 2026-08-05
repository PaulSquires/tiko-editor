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
dim shared ghTopTabsInfoTip as HWND

' The bar's three icon buttons (Find, Goto Main, Goto Header/Source) are a PsIconPanel rather
' than three hand-painted rects: declared cells, hover and press/cancel, and a matched
' press+release click, none of which this file has to own any more.
'
' The FILE-TYPE PILL IS DELIBERATELY NOT A PsButton. PsButton calls SetFocus(hwnd) in its
' WM_LBUTTONDOWN unconditionally, BEFORE the message callback runs, so a host cannot suppress
' it -- and this bar sits directly over the editor, where a click must never pull focus off
' Scintilla. PsIconPanel is the sibling that takes no focus ("a toolbar must not steal focus
' from the host's editor", its own WndProc says), which is exactly why the icons could move
' and the captioned pill could not. It stays painted by this window's WM_PAINT.
dim shared ghTopTabsInfoIcons as HWND

' These are BOTH the PsTooltip tool index AND the PsIconPanel item index -- the items are
' added in this order so the message callback can hand one straight to the other. Keeping
' them equal is what makes that a pass-through rather than a mapping table.
enum
    TTITIP_NONE = -1
    TTITIP_FIND = 0
    TTITIP_GOTOMAIN
    TTITIP_GOTOHEADER
end enum


' ========================================================================================
' The glyph for the Goto Header/Source button, for the CURRENTLY ACTIVE document.
' ========================================================================================
' Resolved at paint time rather than pushed into the item with PsIconPanel_SetGlyph, because
' the answer changes with the active tab and a tab switch only redraws this bar -- it does
' not call frmTopTabsInfo_PositionWindows. A pushed glyph would go stale on exactly that path.
private function frmTopTabsInfo_GotoGlyph() as DWSTRING
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return ""
    if pDoc->ProjectFileType = FILETYPE_HEADER then return wszIconGotoSource
    return wszIconGotoHeader   ' main, normal, module, resource
end function


' ========================================================================================
' Supply the tooltip text for the currently hot rect.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi). Returning "" means no tip for the rest of this hover.
'
' The file-type button has no entry on purpose -- it never had a tip, and with no caption
' fallback in PsTooltip that is now expressed by simply not naming it rather than by a branch
' assigning "".
private function frmTopTabsInfo_TipText( byval hTip as HWND, _
                                         byval hAttached as HWND, _
                                         byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    select case idx
        case TTITIP_FIND
            return frmPanelMenu_FormatTooltip( IDM_FIND )
        case TTITIP_GOTOMAIN
            return frmPanelMenu_FormatTooltip( IDM_GOTOMAINFILE )
        case TTITIP_GOTOHEADER
            ' Resolved at show time, not at hover time: the label depends on which document is
            ' active, and that can change under a resting cursor.
            dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then return ""
            if pDoc->ProjectFileType = FILETYPE_HEADER then
                return frmPanelMenu_FormatTooltip( IDM_GOTOSOURCEFILE )
            else   ' main, normal, module
                return frmPanelMenu_FormatTooltip( IDM_GOTOHEADERFILE )
            end if
    end select
    return ""
end function


' ========================================================================================
' Draw one icon button, instead of PsIconPanel's built-in painter.
' ========================================================================================
' Two reasons this callback exists rather than the built-in rendering:
'   * PER-ITEM FONT. Find is a Segoe Fluent codepoint; the two Goto buttons are the plain
'     letters "M"/"H"/"C", which the symbol font cannot draw. PsIconPanel has ONE font for
'     the whole control (the font is a paint-time input there), so the choice belongs here.
'   * The bar's inversion. The strip's field colour is BackColorHot, so a HOVERED button
'     inverts to the tab area's normal BackColor -- the opposite direction to every other
'     control in the family, and to PsIconPanel's own defaults.
' Colours are read from ghTopTabs live, so a theme change needs only a repaint.
private sub frmTopTabsInfo_IconPaint( byval p as PSICONPANEL_PAINTINFO ptr )
    ' Fill the whole cell first: p->rc carries the item's padding and the control has
    ' background-filled it, so anything left unpainted shows the panel through.
    p->b->SetBackColor( theme.toptabs.texthot.backcolor )
    p->b->PaintRect( @p->rc )

    ' With no tabs open the bar draws nothing but its field -- the same guard the rest of
    ' this window's painting is under.
    if gTTabCtl.GetItemCount = 0 then exit sub

    dim as DWSTRING wszGlyph = p->wszGlyph
    if p->itemID = TTITIP_GOTOHEADER then wszGlyph = frmTopTabsInfo_GotoGlyph()
    if PsLen(wszGlyph) = 0 then exit sub

    p->b->SetFont( ghFont( iif( p->itemID = TTITIP_FIND, SYMBOLFONT_10, GUIFONT_10 ) ) )
    p->b->SetForeColor( theme.toptabs.texthot.forecolor )
    p->b->SetBackColor( iif( p->isHot orelse p->isPressed, _
                             theme.toptabs.text.backcolor, theme.toptabs.texthot.backcolor ) )
    p->b->PaintIconButton( wszGlyph, @p->rc )
end sub


' ========================================================================================
' Carry the hot item into the bar's PsTooltip.
' ========================================================================================
' The tip is attached to the PARENT (this bar), not to the panel: PsTooltip's cursor-over
' test accepts a hit on any descendant of the attached window, so one tip still serves the
' whole strip. But the panel's client area is the parent's dead zone -- the parent sees no
' WM_MOUSEMOVE while the cursor is over a button -- so the index has to come from here.
'
' Item index and tool index are the same number by construction (see the enum above).
' Returns FALSE always: this observes, it never suppresses.
private function frmTopTabsInfo_IconMessage( byval m as PSICONPANEL_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
        case WM_MOUSEMOVE
            PsTooltip_SetToolIndex( ghTopTabsInfoTip, m->idx )
        case WM_MOUSELEAVE
            PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )
    end select
    return false
end function


' ========================================================================================
' A matched press+release on one of the icon buttons.
' ========================================================================================
' Fires only for a completed gesture -- press a button, slide off, release and nothing
' happens. The old WM_LBUTTONUP arm acted on wherever the cursor was at the UP, whatever
' the DOWN had hit.
private sub frmTopTabsInfo_IconClick( byval hIconPanel as HWND, byval idx as long, byval id as long )
    select case idx
        case TTITIP_FIND
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FIND, 0), 0 )
        case TTITIP_GOTOMAIN
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOMAINFILE, 0), 0 )
        case TTITIP_GOTOHEADER
            dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then exit sub
            if pDoc->ProjectFileType = FILETYPE_HEADER then
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOSOURCEFILE, 0), 0 )
            else   ' main, normal, module
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOHEADERFILE, 0), 0 )
            end if
    end select
end sub


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmTopTabsInfo_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSINFO)
    if pWindow = 0 then exit function

    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_TOPTABSINFO, @rc )
            
    ' Calculate the RECT positions for all of the top tabs
    dim as long TabHeight = TOPTABS_INFO_HEIGHT
    dim as long vmargin = pWindow->ScaleY(1)
    dim as long nPadding = 10   ' 5 left 5 right
    
    gTTabCtl.wszFileType = L(171,"Normal") 
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        select case pDoc->ProjectFileType
            case FILETYPE_HEADER:    gTTabCtl.wszFileType = L(140,"Header") 
            case FILETYPE_NORMAL:    gTTabCtl.wszFileType = L(171,"Normal") 
            case FILETYPE_MODULE:    gTTabCtl.wszFileType = L(172,"Module")
            case FILETYPE_MAIN:      gTTabCtl.wszFileType = L(173,"Main")
            case FILETYPE_RESOURCE:  gTTabCtl.wszFileType = L(174,"Resource")
        end select 
    end if    

    ' Info area under the row of tabs (right to left)
    gTTabCtl.rcFileTypeButton = rc
    gTTabCtl.rcFileTypeButton.right = rc.right - pWindow->ScaleX(6)
    gTTabCtl.rcFileTypeButton.left = gTTabCtl.rcFileTypeButton.right - _
            pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_TOPTABSINFO, gTTabCtl.wszFileType, GUIFONT_11) + (nPadding * 2))
    gTTabCtl.rcFileTypeButton.left = gTTabCtl.rcFileTypeButton.left
    gTTabCtl.rcFileTypeButton.top += vmargin
    gTTabCtl.rcFileTypeButton.bottom = gTTabCtl.rcFileTypeButton.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
    ' The three icon buttons, as one PsIconPanel window occupying what used to be three
    ' separate rects. The width is the control's own -- the cells are DECLARED (padding 0,
    ' icon rcIconWidth), so it is 3 * rcIconWidth and cannot drift from what gets painted.
    dim as long yTop      = rc.top + vmargin
    dim as long nBarH     = pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    dim as long nIconsW   = PsIconPanel_GetIdealWidth( ghTopTabsInfoIcons )
    dim as long xIconsLeft = gTTabCtl.rcFileTypeButton.left - nIconsW

    SetWindowPos( ghTopTabsInfoIcons, 0, _
                  xIconsLeft, yTop, nIconsW, nBarH, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    gTTabCtl.rcDiskFilename = rc
    gTTabCtl.rcDiskFilename.top += vmargin
    gTTabCtl.rcDiskFilename.left += pWindow->ScaleX(15)
    gTTabCtl.rcDiskFilename.right = xIconsLeft
    gTTabCtl.rcDiskFilename.bottom = gTTabCtl.rcDiskFilename.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin

    AfxRedrawWindow( HWND_FRMMAIN_TOPTABSINFO )

    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmTopTabsInfo
' ========================================================================================
function frmTopTabsInfo_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmTopTabsInfo_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' frmTopTabsInfo Window procedure
' ========================================================================================
function frmTopTabsInfo_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsInfo_OnSize)

    case WM_DESTROY
        ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with the
        ' parent -- destroying it here is also what removes it from the shared clock's registry.
        if IsWindow(ghTopTabsInfoTip) then DestroyWindow( ghTopTabsInfoTip )
        ghTopTabsInfoTip = 0

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_TOPTABSINFO = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSINFO)
        if pWindow = 0 then exit function

        ' TME_LEAVE only. TME_HOVER existed solely to drive the old WM_MOUSEHOVER tooltip arm;
        ' PsTooltip does its own dwell timing off the shared clock.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)

        ' Nothing this window still paints itself has a tip -- the filename never had one and
        ' the file-type pill never had one -- so being over the PARENT means no tip at all.
        ' The three that do have tips are the PsIconPanel's, and they report from its own
        ' message callback. Silent when the index has not changed; on a change it hides any
        ' visible tip without a fade and clears a suppressed hover.
        PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )
        AfxRedrawWindow( hwnd )


    case WM_LBUTTONUP
        ' Only the file-type pill is still this window's own. The three icon buttons report
        ' through frmTopTabsInfo_IconClick, which fires on a matched press+release.
        if isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_PROJECTFILETYPE, 0), 0 )
        end if


    case WM_PAINT
        dim as PsBufferPaint b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetColors( theme.toptabs.texthot.forecolor, theme.toptabs.texthot.backcolor )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Only paint bottom line if Find is not active.
        b.SetPenColor( theme.editor.divider )
        if gFind.bShowFindPanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if
        
        if gTTabCtl.GetItemCount then
            dim as long i = gTTabCtl.GetCurSel()
            
            dim pDoc as clsDocument ptr 

            ' Determine if it is in a subfolder and if it is, then display the relative file name.
            dim as DWSTRING wszFilename = gTTabCtl.GetTabText(i)
            ' Only a designated Main gives a folder worth making paths relative to. With
            ' none, show the full name rather than relativizing against the active tab,
            ' which would be relative to itself.
            pDoc = gApp.GetMainDocumentPtr()
            if pDoc then
                ' GetpDoc returns null when no tab is selected (i = -1)
                dim as clsDocument ptr pDocTab = gTTabCtl.GetpDoc(i)
                if pDocTab then
                    dim as DWSTRING wszMainFolder = AfxStrPathname("PATH", pDoc->DiskFilename)
                    wszFilename = PsStrRemove( pDocTab->DiskFilename, wszMainFolder )
                end if
            end if
            ' The strip's field color is BackColorHot, so a hovered button
            ' inverts to the tab area's normal BackColor.
            b.SetColors( theme.toptabs.texthot.forecolor, theme.toptabs.texthot.backcolor )
            b.SetFont( ghFont(INFOFONT_11) )
            b.PaintText( wszFilename, @gTTabCtl.rcDiskFilename, DT_LEFT )

            ' The three icon buttons are the PsIconPanel's own client area and are painted
            ' by frmTopTabsInfo_IconPaint. Nothing here reaches them.
            '
            ' THE FONT MUST BE SET AGAIN HERE. Without it the pill inherited INFOFONT_11
            ' (Consolas) from the filename above, while the icon buttons beside it draw in
            ' GUIFONT_10 -- and DT_VCENTER centres each font's OWN line box, so two captions
            ' in equal rects sit on different baselines. Measured (baseline = top +
            ' (H - tmHeight)\2 + tmAscent): Consolas 11 and Segoe UI 10 agree at 96 dpi and
            ' are 1px apart at 168, so the error only appears above 100% DPI -- which is why
            ' nothing here can be checked by looking at it at 100%.
            ' Keep this in step with the font PositionWindows measures the pill's width in.
            b.SetFont( ghFont(GUIFONT_11) )
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton), _
                                 theme.toptabs.text.backcolor, theme.toptabs.texthot.backcolor ) )
            b.PaintRoundRect( @gTTabCtl.rcFileTypeButton )
            b.PaintText( gTTabCtl.wszFileType, @gTTabCtl.rcFileTypeButton, DT_CENTER )
        end if
        
        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmTopTabsInfo_Show
' ========================================================================================
function frmTopTabsInfo_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_TOPTABSINFO = pWindow->Create( hwndParent, @frmTopTabsInfo_WndProc, _
        0, 0, 0, TOPTABS_INFO_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    ' The three icon buttons. COMMAND items, so nothing latches. Cells are DECLARED here --
    ' padding 0 either side and a 30px icon -- which is what makes the strip exactly 3 * 30
    ' wide and lets frmTopTabsInfo_PositionWindows ask the control for that number instead of
    ' recomputing it. Items are added in TTITIP_* order; see the enum.
    ghTopTabsInfoIcons = PsIconPanel_Create( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_ICONS )
        PsIconPanel_SetFont( ghTopTabsInfoIcons, ghFont(SYMBOLFONT_10) )
        PsIconPanel_SetIconSize( ghTopTabsInfoIcons, pWindow->ScaleX(30), pWindow->ScaleY(30) )
        PsIconPanel_SetPadding( ghTopTabsInfoIcons, 0, 0 )
        PsIconPanel_SetPaintItemCallback( ghTopTabsInfoIcons, @frmTopTabsInfo_IconPaint )
        PsIconPanel_SetMessageCallback( ghTopTabsInfoIcons, @frmTopTabsInfo_IconMessage )
        PsIconPanel_SetClickCallback( ghTopTabsInfoIcons, @frmTopTabsInfo_IconClick )
        PsIconPanel_AddItem( ghTopTabsInfoIcons, IP_KIND_COMMAND, wszIconFind, IDM_FIND )
        PsIconPanel_AddItem( ghTopTabsInfoIcons, IP_KIND_COMMAND, wszIconGotoMain, IDM_GOTOMAINFILE )
        PsIconPanel_AddItem( ghTopTabsInfoIcons, IP_KIND_COMMAND, wszIconGotoHeader, IDM_GOTOHEADERFILE )
        ' No tooltip TEXT and no tooltip CALLBACK: the tips are the bar's PsTooltip, so the
        ' control's own comctl32 tip is left with nothing to say and never shows.
        Theme_ApplyTopTabsIcons()

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so the multi-line
    ' split-editor tip still wraps at the same place; colours and fonts are pushed per hover
    ' from frmTopTabsInfo_TipText, so nothing set here can go stale on a theme change.
    ghTopTabsInfoTip = PsTooltip_Create( HWND_FRMMAIN_TOPTABSINFO )
    PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )
    PsTooltip_SetMaxWidth( ghTopTabsInfoTip, 200 )
    PsTooltip_SetTooltipCallback( ghTopTabsInfoTip, @frmTopTabsInfo_TipText )

    function = 0
    
end function

