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
''  frmListView.inc
''   

#include once "frmListView.bi"
#include once "modCompileErrors.bi"


' ========================================================================================
' Calculate the RECT that holds the client coordinates of the scrollbar's vertical thumb
' Returns True if RECT is not empty
' ========================================================================================
function frmListViewVScroll_calcVThumbRect( byval hLV as HWND ) as boolean
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then exit function

    dim as integer minThumb = AfxScaleY(SCROLLBAR_MINTHUMBSIZE)

    ' calculate the vertical scrollbar in client coordinates
    SetRectEmpty( @pData->VScrollData.rc )
    
    dim as long nTopIndex = SendMessage( pData->hListBox, LB_GETTOPINDEX, 0, 0 ) 

    dim as RECT rc 
    with pData->VScrollData
        GetClientRect( pData->hListBox, @rc )
        .listBoxHeight = rc.bottom - rc.top
        .itemHeight = SendMessage( pData->hListBox, LB_GETITEMHEIGHT, 0, 0 )
        .numItems = ListBox_GetCount( pData->hListBox ) 
        .itemsPerPage = .listBoxHeight / .itemHeight
        
        dim as double ratio = .itemsPerPage / .numItems
        .thumbHeight = int(ratio * .listBoxHeight)
        if .thumbHeight < minThumb then .thumbHeight = minThumb
        
        GetClientRect( pData->hVScroll, @rc )
        .rc.Left = rc.Left
        .rc.top = rc.top + ((nTopIndex / .numItems) * .listBoxHeight)
        .rc.Right = rc.right
        .rc.bottom = .rc.top + .thumbHeight
        if .numItems < .itemsPerPage then 
            SetRectEmpty( @pData->VScrollData.rc )
            return true
        end if
    end with

    function = 0
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmListView_PositionWindows( byval hLV as HWND ) as LRESULT
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then exit function

    ' Position the Header and the ListBox
    dim as RECT rc = AfxGetWindowClientRect( hLV )
    dim as long nClientWidth = rc.right - rc.left
    dim as long nClientHeight = rc.bottom - rc.top
    dim as long nHeaderHeight = AfxGetWindowHeight(pData->hHeader)
    dim as long nVScrollWidth = AfxGetWindowWidth(pData->hVScroll)
    
    SetWindowPos( pData->hHeader, 0, _
        0, 0, nClientWidth - nVScrollWidth, nHeaderHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )
    
    SetWindowPos( pData->hVScroll, 0, _
        nClientWidth - nVScrollWidth, nHeaderHeight, _
        nVScrollWidth, nClientHeight - nHeaderHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    SetWindowPos( pData->hListBox, 0, _
        0, nHeaderHeight, _
        nClientWidth - nVScrollWidth, nClientHeight - nHeaderHeight, _
        SWP_NOZORDER or SWP_SHOWWINDOW )
    
    AfxRedrawWindow(pData->hHeader)
    AfxRedrawWindow(pData->hListBox)
    
    frmListViewVScroll_calcVThumbRect( hLV )
    AfxRedrawWindow(pData->hVScroll)

    function = 0
end function


' ========================================================================================
' Refresh the position and ownerdraw of header, listbox and vertical scrollbar.
' ========================================================================================
function frmListView_Refresh( byval hLV as HWND ) as LRESULT
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then exit function
    frmListView_PositionWindows( hLV )
    function = 0
end function


' ========================================================================================
' frmListViewVScroll Window procedure
' ========================================================================================
function frmListViewVScroll_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as point prev_pt   ' screen pt.y cursor position

    dim as HWND hLV = GetParent(hwnd)
    dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hLV)
    if pData = 0 then exit function

    select case uMsg
        case WM_LBUTTONDOWN
            dim as point pt: GetCursorPos( @pt )
            frmListViewVScroll_calcVThumbRect(hLV)   ' in client coordinates
            dim as RECT rc = pData->VScrollData.rc       ' covert copy to screen coordinates
            MapWindowpoints( pData->hVScroll, HWND_DESKTOP, cast(point ptr, @rc), 2)
            if PtInRect( @rc, pt ) then
                prev_pt = pt
                gApp.bDragActive = true
                SetCapture( hwnd )
            else
                ' we have clicked on a PageUp or PageDn
                dim as long nTopIndex = SendMessage( pData->hListBox, LB_GETTOPINDEX, 0, 0 ) 
                if pt.y < rc.top then
                    nTopIndex = max( nTopIndex - pData->VScrollData.itemsPerPage, 0 )
                    SendMessage( pData->hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                elseif pt.y > rc.bottom then
                    dim as long nMaxTopIndex = pData->VScrollData.numItems - pData->VScrollData.itemsPerPage
                    nTopIndex = min( nTopIndex + pData->VScrollData.itemsPerPage, nMaxTopIndex )
                    SendMessage( pData->hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                end if
                frmListViewVScroll_calcVThumbRect(hLV)
                AfxRedrawWindow( pData->hVScroll )
                AfxRedrawWindow( pData->hListBox )
            end if
            
        case WM_MOUSEMOVE
            if gApp.bDragActive then
                dim as point pt: GetCursorPos( @pt )
                if pt.y <> prev_pt.y then 
                    dim as long delta = pt.y - prev_pt.y 
                    dim as RECT rc: GetClientRect( pData->hVScroll, @rc )

                    pData->VScrollData.rc.top = max(0, pData->VScrollData.rc.top + delta)
                    pData->VScrollData.rc.top = min(pData->VScrollData.rc.top, rc.bottom-pData->VScrollData.thumbHeight)
                    pData->VScrollData.rc.bottom = pData->VScrollData.rc.top + pData->VScrollData.thumbHeight

                    prev_pt = pt
                    
                    dim as long nPrevTopIndex = SendMessage( pData->hListBox, LB_GETTOPINDEX, 0, 0 ) 
                    dim as long nLastIndex = (pData->VScrollData.rc.bottom / rc.bottom) * pData->VScrollData.numItems
                    dim as long nTopIndex = nLastIndex - pData->VScrollData.itemsPerPage
                    if nTopIndex <> nPrevTopIndex then
                        SendMessage( pData->hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                        AfxRedrawWindow( pData->hListBox )
                    end if   
                    AfxRedrawWindow( pData->hVScroll )
                end if
            end if

        case WM_LBUTTONUP   
            gApp.bDragActive = false
            prev_pt.x = 0
            prev_pt.y = 0
            ReleaseCapture
        
        case WM_ERASEBKGND
            return true
            
        case WM_PAINT
            dim as clsDoubleBuffer b

            b.BeginDoubleBuffer(hwnd)
            b.SetBackColors( pData->BackColorScrollBar, pData->BackColorScrollBar )
            b.PaintClientRect()
            if IsRectEmpty( @pData->VScrollData.rc ) = 0 then
                b.SetPenColor( pData->DividerColorScrollBar )
                b.PaintLine( 1, b.rcClient.left, b.rcClient.top, b.rcClient.left, b.rcClient.bottom )
            end if    
            b.SetBackColors( pData->ForeColorScrollBar, pData->ForeColorScrollBar )
            b.PaintRect( @pData->VScrollData.rc )

            b.EndDoubleBuffer()

            return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( HWnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmListViewHeader_SubclassProc 
' ========================================================================================
function frmListViewHeader_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    select case uMsg

        case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' eventual WM_MOUSEHOVER and WM_MOUSELEAVE events
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 1    ' HOVER_DEFAULT is 500
            TrackMouseEvent(@tme) 
            
        case WM_MOUSEHOVER
            
        case WM_MOUSELEAVE
            
        case WM_RBUTTONDOWN
            
        case WM_LBUTTONUP
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
            end if
                    
        case WM_ERASEBKGND
            return true
        
        case WM_PAINT
            dim pWindow as CWindow ptr = AfxCWindowPtr(GetParent(hWin))

            dim as clsDoubleBuffer b
            
            b.BeginDoubleBuffer(hWin)

            dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer( GetParent(hWin) )
            if pData then 
                ' Set default fore and back colors
                b.SetForeColors( pData->ForeColorHot, pData->ForeColorHot )
                b.SetBackColors( pData->BackColor, pData->BackColorHot )

                ' Clear the entire back client area
                b.PaintClientRect()
              
                b.SetFont( ghFont(GUIFONT_9) )

                dim as RECT rc = b.rcClient
                for i as long = lbound(pData->ColData) to ubound(pData->ColData)
                    rc.right = rc.left + pData->ColData(i).nWidth
                    b.PaintRect( @rc )
                    b.PaintText( pData->ColData(i).wszText, @rc, DT_LEFT )
                    rc.left = rc.right
                next
            end if
            
            b.EndDoubleBuffer()

            return 0

        case WM_NCDESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmListViewHeader_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' frmListViewListBox_SubclassProc 
' ========================================================================================
function frmListViewListBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(GetParent(hWin))
    static as long accumDelta

    ' Convert our ENTER key presses into LBUTTONDBLCLK to process them similarly
    dim as boolean bEnterPressed = false
    if (uMsg = WM_KEYUP) andalso (loword(_wParam) = VK_RETURN) then 
        uMsg = WM_LBUTTONDBLCLK
        bEnterPressed = true
    end if
    
    dim as HWND hLV = GetParent(hWin)    
    
    select case uMsg
        
        case WM_MOUSEWHEEL
            ' accumulate delta until scroll one line (up +120, down -120). 
            ' 120 is the Microsoft default delta
            dim as long zDelta = GET_WHEEL_DELTA_WPARAM( _wParam )
            dim as long nTopIndex = SendMessage( hWin, LB_GETTOPINDEX, 0, 0 ) 
            accumDelta = accumDelta + zDelta
            if accumDelta >= 120 then       ' scroll up 3 lines
                nTopIndex = nTopIndex - 3
                nTopIndex = max( 0, nTopIndex )
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
                frmListView_Refresh(hLV)
            elseif accumDelta <= -120 then  ' scroll down 3 lines
                nTopIndex = nTopIndex + 3
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
                frmListView_Refresh(hLV)
            end if

        case WM_LBUTTONUP
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                ListBox_SetCurSel( hWin, idx )
            end if
                    
        case WM_LBUTTONDBLCLK 
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            if (hiword(idx) <> 1) orelse (bEnterPressed = true) then
                SendMessage( GetParent(GetParent(hWin)), MSG_USER_LISTVIEW_LBUTTONDBLCLK, cast(WPARAM, GetParent(hWin)), 0 )
            end if
            exit function
        
        case WM_KEYUP
            select case loword(_wParam)
                case VK_RETURN  ' already processed in WM_LBUTTONDBLCLK 
            end select
            exit function

        case WM_CHAR   ' prevent the annoying beep!
            if _wParam = VK_RETURN then return 0
            if _wParam = VK_ESCAPE then return 0
                            
        case WM_ERASEBKGND
            ' if the number of lines in the listbox maybe less than the number per page then 
            ' calculate from last item to bottom of listbox, otherwise calculate based on
            ' the mod of the lineheight to listbox height so we can color the partial line
            ' that won't be displayed at the bottom of the list.
            dim as RECT rc = GetListBoxEmptyClientArea( hWin )
            if rc.top < rc.bottom then
                dim as HDC _hDC = cast(HDC, _wParam)
                PaintRect( _hDC, @rc, ghOutput.BackColor )
            end if
            return true

        case WM_NCDESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmListViewListBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long

    ' Set the height of the list box items. 
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    lpmis->itemHeight = pWindow->ScaleY(EXPLORERITEM_HEIGHT_COMPACT)

    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit function
    if lpdis = 0 then exit function
    
    if ( lpdis->itemAction = ODA_DRAWENTIRE ) orelse _
        ( lpdis->itemAction = ODA_SELECT ) orelse _
        ( lpdis->itemAction = ODA_FOCUS ) then

        dim as clsDoubleBuffer b

        b.BeginDoubleBuffer(lpdis->hwndItem, lpdis->hDC, lpdis->rcItem)

        dim as boolean IsHot = false
        if ListBox_GetCurSel(lpdis->hwndItem) = lpdis->itemID then IsHot = true

        dim as LISTVIEW_DATA_TYPE ptr pData = frmListView_GetPointer(hwnd)

        if pData then 
            dim as COLORREF backclr = iif( IsHot, pData->BackColorHot, pData->BackColor)
            dim as COLORREF foreclr = iif( IsHot, pData->ForeColorHot, pData->ForeColorHot)
            b.SetForeColors(foreclr, foreclr)
            b.SetBackColors(backclr, backclr)

            ' Create our rect that works with the entire line
            dim as RECT rc
            SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
            dim as RECT rcText = rc 
            
            ' Paint the entire background
            b.PaintRect( @rc )

            b.SetFont( ghFont(GUIFONT_9) )

            dim as DWSTRING wszCaption = pData->rowData(lpdis->ItemID)

            dim as long nCount = AfxStrParseCount(wszCaption, chr(9))
            for i as long = 0 to nCount - 1
                rcText.right = rcText.left + pData->ColData(i).nWidth
                ' offset the left margin ever so slightly
                rcText.left += pWindow->ScaleX(4)
                dim as DWSTRING wszText = AfxStrParse(wszCaption, i + 1, chr(9))
                dim as long flags = DT_LEFT
                if i = 2 then flags = flags or DT_PATH_ELLIPSIS  ' for long filenames
                b.PaintText( wszText, @rcText, flags )
                rcText.left = rcText.right
            next

        end if
        
        b.EndDoubleBuffer()

    end if   

    function = true
    
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case codeNotify
        case LBN_SELCHANGE
            ' update the highlighting of the current line
            AfxRedrawWindow(hwndCtl)
            ' update the scrollbar position if necessary
            frmListView_PositionWindows(hwnd)
    end select      

    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnDestroy( byval hwnd as HWND ) as LRESULT
    function = 0
end Function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmListView_PositionWindows( hwnd )
    end if
    function = 0
end function
        
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: frmListView
' ========================================================================================
function frmListView_OnPaint( byval hwnd as HWND ) as LRESULT
            
    dim as clsDoubleBuffer b
    
    b.BeginDoubleBuffer(hwnd)

    ' Set default fore and back colors
    b.SetForeColors( ghOutput.ForeColor, ghOutput.ForeColor )
    b.SetBackColors( ghOutput.BackColor, ghOutput.BackColor )

    ' Clear the entire back client area
    b.PaintClientRect()
    
    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' frmListView Window procedure
' ========================================================================================
function frmListView_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,        frmListView_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       frmListView_OnPaint)
        HANDLE_MSG (hwnd, WM_DESTROY,     frmListView_OnDestroy)
        HANDLE_MSG (hwnd, WM_COMMAND,     frmListView_OnCommand)
        HANDLE_MSG (hwnd, WM_MEASUREITEM, frmListView_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    frmListView_OnDrawItem)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow = 0 then exit function
    
        ' Destroy the manually allocated LISTVIEW_DATA_TYPE pointer
        if pWindow->UserData(0) > 0 then 
            dim as LISTVIEW_DATA_TYPE ptr pData = cast(LISTVIEW_DATA_TYPE ptr, pWindow->UserData(0))
            if pData then delete(pData)
        end if
        if pWindow then delete(pWindow)

        return 0

    case WM_ERASEBKGND
        return true

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmListView_Show
' ========================================================================================
function frmListView_Show( byval hwndParent as hwnd ) as HWND

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    dim as HWND hLV = pWindow->Create( hwndParent, @frmListView_WndProc, 0, 0, 0, 0 )

    dim as LISTVIEW_DATA_TYPE ptr pData = new LISTVIEW_DATA_TYPE
    pWindow->UserData(0) = cast(LONG_PTR, pData)
    
    ' Must use GetDlgItem to retrieve the child window handles because there
    ' could be multiple instances of this ListView control.

    pData->hHeader = _
        pWindow->AddControl( "LABEL", , IDC_FRMLISTVIEW_HEADER, "", 0, 0, 0, EXPLORERITEM_HEIGHT_COMPACT, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_NOTIFY or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)
        pWindow->SetControlSubclass(pData->hHeader, @frmListViewHeader_SubclassProc, IDC_FRMLISTVIEW_HEADER, @pWindow)
    pData->hListBox = _
        pWindow->AddControl( "LISTBOX", , IDC_FRMLISTVIEW_LISTBOX, "", 0, 0, 0, 0, _
        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or WS_TABSTOP or _
        LBS_NOINTEGRALHEIGHT or LBS_OWNERDRAWFIXED or LBS_NOTIFY or LBS_NODATA, _
        WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(pData->hListBox, @frmListViewListBox_SubclassProc, IDC_FRMLISTVIEW_LISTBOX, @pWindow)

    dim pWindowPanel as CPanelWindow ptr = New CPanelWindow
    pData->hVScroll = _
        pWindowPanel->Create( hLV, @frmListViewVScroll_WndProc, 0, 0, SCROLLBAR_WIDTH_EDITOR, 0 )
    pWindow->ClassStyle = CS_DBLCLKS

    frmListView_PositionWindows(hLV)
    
    function = hLV
    
end function
