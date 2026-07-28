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

enum
    TTITIP_NONE = -1
    TTITIP_FIND = 0
    TTITIP_GOTOMAIN
    TTITIP_GOTOHEADER
end enum


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
' Which of the bar's rects is the cursor over? TTITIP_NONE for none of them.
' ========================================================================================
private function frmTopTabsInfo_HitTest( byval hwnd as HWND ) as long
    if isMouseOverRECT( hwnd, gTTabCtl.rcFindButton ) then return TTITIP_FIND
    if isMouseOverRECT( hwnd, gTTabCtl.rcGotoMain )   then return TTITIP_GOTOMAIN
    if isMouseOverRECT( hwnd, gTTabCtl.rcGotoHeader ) then return TTITIP_GOTOHEADER
    return TTITIP_NONE
end function


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
    dim as long rcIconWidth = 30 
    dim as long rcIconHeight = 30 
    dim as long vmargin = pWindow->ScaleY(1)
    dim as long nPadding = 10   ' 5 left 5 right
    
    gTTabCtl.wszFileType = L(210,"Normal") 
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then 
        select case pDoc->ProjectFileType
            case FILETYPE_HEADER:    gTTabCtl.wszFileType = L(175,"Header") 
            case FILETYPE_NORMAL:    gTTabCtl.wszFileType = L(210,"Normal") 
            case FILETYPE_MODULE:    gTTabCtl.wszFileType = L(211,"Module")
            case FILETYPE_MAIN:      gTTabCtl.wszFileType = L(212,"Main")
            case FILETYPE_RESOURCE:  gTTabCtl.wszFileType = L(213,"Resource")
        end select 
    end if    

    ' Info area under the row of tabs (right to left)
    gTTabCtl.rcFileTypeButton = rc
    gTTabCtl.rcFileTypeButton.right = rc.right - pWindow->ScaleX(6)
    gTTabCtl.rcFileTypeButton.left = gTTabCtl.rcFileTypeButton.right - _
            pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_TOPTABSINFO, gTTabCtl.wszFileType, GUIFONT_9) + (nPadding * 2))
    gTTabCtl.rcFileTypeButton.left = gTTabCtl.rcFileTypeButton.left
    gTTabCtl.rcFileTypeButton.top += vmargin
    gTTabCtl.rcFileTypeButton.bottom = gTTabCtl.rcFileTypeButton.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
    gTTabCtl.rcGotoHeader = rc
    gTTabCtl.rcGotoHeader.right = gTTabCtl.rcFileTypeButton.left
    gTTabCtl.rcGotoHeader.top += vmargin
    gTTabCtl.rcGotoHeader.left = gTTabCtl.rcGotoHeader.right - pWindow->ScaleX( rcIconWidth )
    gTTabCtl.rcGotoHeader.bottom = gTTabCtl.rcGotoHeader.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin

    gTTabCtl.rcGotoMain = rc
    gTTabCtl.rcGotoMain.right = gTTabCtl.rcGotoHeader.left 
    gTTabCtl.rcGotoMain.top += vmargin
    gTTabCtl.rcGotoMain.left = gTTabCtl.rcGotoMain.right - pWindow->ScaleX( rcIconWidth )
    gTTabCtl.rcGotoMain.bottom = gTTabCtl.rcGotoMain.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin

    gTTabCtl.rcFindButton = rc
    gTTabCtl.rcFindButton.right = gTTabCtl.rcGotoMain.left
    gTTabCtl.rcFindButton.top += vmargin
    gTTabCtl.rcFindButton.left = gTTabCtl.rcFindButton.right - pWindow->ScaleX( rcIconWidth )
    gTTabCtl.rcFindButton.bottom = gTTabCtl.rcFindButton.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
    gTTabCtl.rcDiskFilename = rc
    gTTabCtl.rcDiskFilename.top += vmargin
    gTTabCtl.rcDiskFilename.left += pWindow->ScaleX(15)
    gTTabCtl.rcDiskFilename.right = gTTabCtl.rcFindButton.left
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

        ' Silent when the index has not changed; on a change it hides any visible tip without
        ' a fade and clears a suppressed hover, so the new rect gets a fresh offer.
        PsTooltip_SetToolIndex( ghTopTabsInfoTip, frmTopTabsInfo_HitTest( hwnd ) )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )
        AfxRedrawWindow( hwnd )


    case WM_LBUTTONUP
        if isMouseOverRECT(hwnd, gTTabCtl.rcFindButton) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FIND, 0), 0 )
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcGotoMain) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOMAINFILE, 0), 0 )
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcGotoHeader) then
            dim as DWSTRING wszHeaderOrCode
            dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then 
                if pDoc->ProjectFileType = FILETYPE_HEADER then
                    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOSOURCEFILE, 0), 0 )
                else   ' main, normal, module
                    PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_GOTOHEADERFILE, 0), 0 )
                end if    
            end if
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_PROJECTFILETYPE, 0), 0 )
        end if
        

    case WM_PAINT
        dim as PsBufferPaint b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetColors( ghTopTabs.ForeColorHot, ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Only paint bottom line if Find is not active.
        b.SetPenColor( ghEditor.Divider )
        if gFind.bShowFindPanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if
        
        if gTTabCtl.GetItemCount then
            dim as long i = gTTabCtl.GetCurSel()
            
            dim pDoc as clsDocument ptr 

            ' Determine if it is in a subfolder and if it is, then display the relative file name.
            dim as DWSTRING wszFilename = gTTabCtl.GetTabText(i)
            if gApp.IsProjectActive then
                pDoc = gApp.GetMainDocumentPtr()
                if pDoc then
                    ' GetpDoc returns null when no tab is selected (i = -1)
                    dim as clsDocument ptr pDocTab = gTTabCtl.GetpDoc(i)
                    if pDocTab then
                        dim as DWSTRING wszMainFolder = AfxStrPathname("PATH", pDoc->DiskFilename)
                        wszFilename = AfxStrRemove( pDocTab->DiskFilename, wszMainFolder )
                    end if
                end if
            end if
            ' The strip's field color is BackColorHot, so a hovered button
            ' inverts to the tab area's normal BackColor.
            b.SetColors( ghTopTabs.ForeColorHot, ghTopTabs.BackColorHot )
            b.SetFont( ghFont(INFOFONT_11) )
            b.PaintText( wszFilename, @gTTabCtl.rcDiskFilename, DT_LEFT )

            b.SetFont( ghFont(SYMBOLFONT_10) )
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gTTabCtl.rcFindButton), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
            b.PaintIconButton( wszIconFind, @gTTabCtl.rcFindButton )

            b.SetFont( ghFont(GUIFONT_10) )
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gTTabCtl.rcGotoMain), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
            b.PaintIconButton( wszIconGotoMain, @gTTabCtl.rcGotoMain )

            dim as DWSTRING wszHeaderOrCode
            pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                if pDoc->ProjectFileType = FILETYPE_HEADER then
                    wszHeaderOrCode = wszIconGotoSource
                else   ' main, normal, module, resource
                    wszHeaderOrCode = wszIconGotoHeader
                end if
            end if
            b.SetBackColor( iif( isMouseOverRECT(hwnd, gTTabCtl.rcGotoHeader), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
            b.PaintIconButton( wszHeaderOrCode, @gTTabCtl.rcGotoHeader )

            b.SetBackColor( iif( isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton), _
                                 ghTopTabs.BackColor, ghTopTabs.BackColorHot ) )
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

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so the multi-line
    ' split-editor tip still wraps at the same place; colours and fonts are pushed per hover
    ' from frmTopTabsInfo_TipText, so nothing set here can go stale on a theme change.
    ghTopTabsInfoTip = PsTooltip_Create( HWND_FRMMAIN_TOPTABSINFO )
    PsTooltip_SetToolIndex( ghTopTabsInfoTip, TTITIP_NONE )
    PsTooltip_SetMaxWidth( ghTopTabsInfoTip, 200 )
    PsTooltip_SetTooltipCallback( ghTopTabsInfoTip, @frmTopTabsInfo_TipText )

    function = 0
    
end function

