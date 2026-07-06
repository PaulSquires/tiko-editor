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

    ' Info area under the row of tabs 
    gTTabCtl.rcFindButton = rc
    gTTabCtl.rcFindButton.left = gTTabCtl.rcPrevTabs.left 
    gTTabCtl.rcFindButton.top += vmargin
    gTTabCtl.rcFindButton.right = gTTabCtl.rcFindButton.left + pWindow->ScaleX( rcIconWidth )
    gTTabCtl.rcFindButton.bottom = gTTabCtl.rcFindButton.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
    gTTabCtl.rcDiskFilename = rc
    gTTabCtl.rcDiskFilename.top += vmargin
    gTTabCtl.rcDiskFilename.left += pWindow->ScaleX(15)
    gTTabCtl.rcDiskFilename.right = gTTabCtl.rcFindButton.left
    gTTabCtl.rcDiskFilename.bottom = gTTabCtl.rcDiskFilename.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
    gTTabCtl.rcFileTypeButton = rc
    gTTabCtl.rcFileTypeButton.left = gTTabCtl.rcFindButton.right + pWindow->ScaleX(6)
    gTTabCtl.rcFileTypeButton.top += vmargin
    gTTabCtl.rcFileTypeButton.right = gTTabCtl.rcFileTypeButton.left + _
            pWindow->ScaleX( getTextWidth(HWND_FRMMAIN_TOPTABSINFO, gTTabCtl.wszFileType, GUIFONT_9) + (nPadding * 2))
    gTTabCtl.rcFileTypeButton.bottom = gTTabCtl.rcFileTypeButton.top + pWindow->ScaleY(TOPTABS_INFO_HEIGHT-1) - vmargin
    
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

    static as DWSTRING wszPrevTooltip 
    static as HWND hTooltip 
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmTopTabsInfo_OnSize)
            
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow = 0 then delete(pWindow)
        HWND_FRMMAIN_TOPTABSINFO = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_TOPTABSINFO)
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
            SendMessage( hTooltip, TTM_SETMAXTIPWIDTH, 0, 200 )
        end if
    
        AfxRedrawWindow( hwnd )
        
           
    case WM_MOUSELEAVE
        wszPrevTooltip = ""
        AfxRedrawWindow( hwnd )


    case WM_MOUSEHOVER
        dim as DWSTRING wszTooltip 
        if isMouseOverRECT(hwnd, gTTabCtl.rcFindButton) then
            wszTooltip  = frmPanelMenu_FormatTooltip( IDM_FIND )
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton) then
            wszTooltip = ""
        end if
        
        ' Display the tooltip
        if wszTooltip <> wszPrevTooltip then
            AfxSetTooltipText( hTooltip, hwnd, wszTooltip )
            wszPrevTooltip = wszTooltip 
        end if    


    case WM_LBUTTONUP
        if isMouseOverRECT(hwnd, gTTabCtl.rcFindButton) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FIND, 0), 0 )
        elseif isMouseOverRECT(hwnd, gTTabCtl.rcFileTypeButton) then
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_PROJECTFILETYPE, 0), 0 )
        end if
        

    case WM_PAINT
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors
        b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
        b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColorHot )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Only paint bottom line if Find is not active.
        b.SetPenColor( ghEditor.Divider )
        if gFind.bShowFindPanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if
        
        if gTTabCtl.GetItemCount then
            dim as long i = gTTabCtl.CurSel
            
            ' Clear the bottom border of the active tab
            b.SetPenColor( ghTopTabs.BackColorHot )
            b.PaintLine( 1, gTTabCtl.tabs(i).rcTab.left, b.rcClient.top, _
                                gTTabCtl.tabs(i).rcTab.right, b.rcClient.top )

            ' Determine if it is in a subfolder and if it is, then display the relative file name.
            dim as DWSTRING wszFilename = gTTabCtl.tabs(i).wszText
            if gApp.IsProjectActive then
                dim pDoc as clsDocument ptr 
                pDoc = gApp.GetMainDocumentPtr()
                if pDoc then
                    dim as DWSTRING wszMainFolder = AfxStrPathname("PATH", pDoc->DiskFilename)
                    wszFilename = AfxStrRemove( gTTabCtl.tabs(i).pDoc->DiskFilename, wszMainFolder )
                end if
            end if
            b.SetForeColors( ghTopTabs.ForeColorHot, ghTopTabs.ForeColorHot )
            b.SetBackColors( ghTopTabs.BackColorHot, ghTopTabs.BackColor )
            b.SetFont( INFOFONT_11 )
            b.PaintText( wszFilename, @gTTabCtl.rcDiskFilename, DT_LEFT )

            b.SetFont( SYMBOLFONT_10 )
            b.PaintIconButton( wszIconFind, @gTTabCtl.rcFindButton )

            b.SetFont( GUIFONT_10 )
            b.PaintRoundRect( @gTTabCtl.rcFileTypeButton, true )
            b.PaintText( gTTabCtl.wszFileType, @gTTabCtl.rcFileTypeButton, DT_CENTER, true )
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

    function = 0
    
end function

