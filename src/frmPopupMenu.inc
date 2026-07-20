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

declare function frmPopupMenu_Show( _
    byval nPopupType as long, byval nItemID as long, byval hParent as HWND, byval rcParent as RECT) as HWND

dim shared as HWND HWND_MENU(1)
dim shared as HWND HWND_SHADOW(1)

const ID_POPUP = 0
const ID_SUBPOPUP = 1
const IDC_MENU_LISTBOX = 100


' ========================================================================================
' Move highlight to next valid menuitem. This will simulate keyboard control.
' ========================================================================================
function setNextMenuItemTabIndex( byval bReverse as boolean ) as long
    ' Determine the active popup menu that our "cursor' is on
    dim as HWND hActiveWindow = iif( IsWindowVisible( HWND_MENU(1)), HWND_MENU(1), HWND_MENU(0) )
    dim as HWND hListBox = GetDlgItem( hActiveWindow, IDC_MENU_LISTBOX ) 

    ' bypass any disabled menuitems and loop to start if needed
    dim as long nCount = ListBox_GetCount( hListBox )
    dim as long nCurSel = ListBox_GetCurSel( hListBox )
    dim as long nFound = -1

    if bReverse then
        for i as long = nCursel - 1 to 0 step -1
            dim as long nIdx = ListBox_GetItemData( hListBox, i )
            if (gTopMenu(nIdx).isDisabled = false) and (gTopMenu(nIdx).isSeparator = false) then
                nFound = i: exit for
            end if
        next
        if nFound = -1 then nFound = nCount - 1   ' wrap to end of list
    else
        for i as long = nCursel + 1 to nCount - 1
            dim as long nIdx = ListBox_GetItemData( hListBox, i )
            if (gTopMenu(nIdx).isDisabled = false) and (gTopMenu(nIdx).isSeparator = false) then
                nFound = i: exit for
            end if
        next
        if nFound = -1 then nFound = 0   ' wrap to start of list
    end if         

    if hActiveWindow = HWND_MENU(0) then gMenuLastCurSel = nFound
    ListBox_SetCurSel( hListBox, nFound )
    AfxRedrawWindow( hListBox )

    function = 0
end function


' ========================================================================================
' Kill all popup menus and reset variables. Do this when app loses focus or the user
' clicks somewhere away from the menu.
' ========================================================================================
function killPopupMenus() as boolean
    if HWND_MENU(0) then 
        DestroyWindow(HWND_MENU(0))
        HWND_MENU(0) = 0
        return true
    end if
end function

function killPopupSubMenus() as boolean
    if HWND_MENU(1) then 
        DestroyWindow(HWND_MENU(1))
        HWND_MENU(1) = 0
        return true
    end if
end function

function killAllPopupMenus() as boolean
    killPopupSubMenus()
    killPopupMenus()
    gMenuLastCurSel = -1
    ' Every menu-dismissal path funnels through here, so this clear is also the
    ' safety net for a press-and-release on the popup's padding (which sets the
    ' flag in WM_MOUSEACTIVATE but never reaches the listbox's WM_LBUTTONUP).
    gApp.KeepTitleBarActive = false
    AfxRedrawWindow(HWND_FRMMAIN_MENUBAR)
    return true
end function

function isPopupMenuOpen() as boolean
    return iif( IsWindowVisible(HWND_MENU(0)), true, false ) 
end function

function isSubPopupMenuOpen() as boolean
    return iif( IsWindowVisible(HWND_MENU(1)), true, false ) 
end function

function isAnyPopupMenuOpen() as boolean
    return iif( isPopupMenuOpen() orelse isPopupMenuOpen(), true, false )
end function



' ========================================================================================
' Fill the bitmap with semi transparent values
' ========================================================================================
function bitmapFillAlpha(byval hBmp as HBITMAP, byval clrRGBA as integer ) as Boolean

    dim as Boolean bResult = false

    if (hBmp) then 
        dim as BITMAP bmp
        GetObject(hBmp, sizeof(BITMAP), @bmp)
        dim as DWORD dwCount = bmp.bmWidthBytes * bmp.bmHeight
        if (dwCount >= sizeof(DWORD)) then
            dim as DWORD ptr pcBitsWords = cast(DWORD ptr, bmp.bmBits)
            if (pcBitsWords) then
            dim as DWORD dwIndex = (dwCount / sizeof(DWORD)) - 1
            dim as DWORD dwUp = bmp.bmWidth
            dim as DWORD dwDn = dwIndex -dwUp
            dim as DWORD dwR  = bmp.bmWidth -1
            while dwIndex 
                dim as DWORD dwSides = dwIndex mod bmp.bmWidth
                if (dwIndex < dwUp) or (dwIndex > dwDn) or (dwSides = 0) or(dwSides = dwR) then
                    pcBitsWords[dwIndex] = clrRGBA  'sm_clrPenA;   // 0xFF0080FF
                else 
                    pcBitsWords[dwIndex] = clrRGBA  'sm_clrBrushA; // 0x400020FF
                end if
                dwIndex = dwIndex - 1
            wend
            bResult = true
            end if
        end if
    end if
    return bResult
end function

' ========================================================================================
' Create/Display the alpha blended shadow for the WS_EX_LAYERED window
' ========================================================================================
function paintLayeredWindow( byval hwnd as HWND, byval clrRGBA as integer ) as long
    dim as RECT rcPos = AfxGetWindowRect(hwnd)
    
    dim as HDC hdcScreen = GetDC(0)
    dim as HDC hDC = CreateCompatibleDC(hdcScreen)
    
    SaveDC(hDC)
    
    dim as long iWidth  = rcPos.right - rcPos.left
    dim as long iHeight = rcPos.bottom - rcPos.top

    dim as BITMAPINFO sBI      
    sBI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER)
    sBI.bmiHeader.biWidth       = iWidth
    sBI.bmiHeader.biHeight      = iHeight 
    sBI.bmiHeader.biPlanes      = 1 
    sBI.bmiHeader.biBitCount    = 32 
    sBI.bmiHeader.biCompression = BI_RGB

    dim as HBITMAP hBmp = CreateDIBSection(hDC, @sBI, DIB_RGB_COLORS, null, null, 0)
    dim as HBITMAP hOldBmp = SelectObject(hDC, hBmp)

    dim as boolean bFillAlphaOK = bitmapFillAlpha(hBmp, clrRGBA)
    dim as BLENDfunction blend
    blend.BlendOp             = AC_SRC_OVER
    blend.SourceConstantAlpha = iif(bFillAlphaOK, 160, 64)
    blend.AlphaFormat         = iif(bFillAlphaOK, AC_SRC_ALPHA, 0)
    
    ' Destination position at the screen
    dim as point ptPos  = (rcPos.left, rcPos.top)

    ' Source position in source (memory DC)
    dim as point ptSrc = (0,0) 

    ' Dimensions of the bits transfer
    dim as SIZE sizeWnd  = (iWidth, iHeight)

    UpdateLayeredWindow(hwnd, hdcScreen, @ptPos, @sizeWnd, hDC, @ptSrc, 0, @blend, ULW_ALPHA) 

    SelectObject(hDC, hOldBmp)
    
    RestoreDC(hDC, -1)
    DeleteDC(hDC)
    ReleaseDC(0, hdcScreen)
    if hBmp then DeleteObject(hBmp)

    function = 0
end function
        

' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmPopupMenu
' ========================================================================================
function frmPopupMenu_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as Long
    ' Set the height of the menuitem list box items. 
    dim as long nHeight = iif(gConfig.CompactMenus, MENUITEM_HEIGHT_COMPACT, MENUITEM_HEIGHT)
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    lpmis->itemHeight = pWindow->ScaleY(nHeight)
        
    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmPopupMenu
' ========================================================================================
function frmPopupMenu_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function
    
    if lpdis = 0 then exit function
    
    if ( lpdis->itemAction = ODA_DRAWENTIRE ) orelse _
        ( lpdis->itemAction = ODA_SELECT ) orelse _
        ( lpdis->itemAction = ODA_FOCUS ) then

        dim as clsDoubleBuffer b

        b.BeginDoubleBuffer(lpdis->hwndItem, lpdis->hDC, lpdis->rcItem)

        dim as long idx = ListBox_GetItemData( lpdis->hwndItem, lpdis->ItemID )
        dim pMenu as TOPMENU_TYPE ptr = @gTopMenu(idx)

        dim as COLORREF backclr
        dim as COLORREF foreclr
        
        dim as DWSTRING wszCaption = getMenuText(pMenu->nID)

        if (pMenu->isSeparator = true) orelse (pMenu->isDisabled = true) then
            foreclr = ghPopup.ForeColorDisabled
            backclr = ghPopup.BackColorDisabled
        else
            dim as boolean IsHot = false
            if ListBox_GetCurSel(lpdis->hwndItem) = lpdis->itemID then IsHot = true
            backclr = iif( IsHot, ghPopup.BackColorHot, ghPopup.BackColor)
            foreclr = iif( IsHot, ghPopup.ForeColorHot, ghPopup.ForeColor)
        end if

        b.SetForeColor( foreclr )
        b.SetBackColor( backclr )

        ' Create our rect that works with the entire line
        dim as RECT rc
        SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
        dim as RECT rcText = rc 
        dim as RECT rcBitmap = rc 
        
        ' Paint the entire background
        b.PaintRect( @rc )

        if pMenu->isSeparator then 
            rcText.left = rcText.left + pWindow->ScaleX(12) 
            rcText.right = rcText.right - pWindow->ScaleX(12)
            b.SetPenColor(ghPopup.ForeColorDisabled)
            b.SetBackColor( backclr )
            b.PaintLine(1, rcText.left, (rcText.bottom-rcText.top) / 2, _
                            rcText.right, (rcText.bottom-rcText.top) / 2 )
        else
            ' Handle caption, keyboard accelerator or submenu marker
            dim as DWSTRING wszLeft = AfxStrParse(wszCaption, 1, chr(9))
            dim as DWSTRING wszRight = AfxStrParse(wszCaption, 2, chr(9))
            
            ' checkmark
            if pMenu->isChecked then
                b.SetFont( ghFont(SYMBOLFONT_10) )
                rcBitmap.right = rcBitmap.left + pWindow->ScaleX(30) 
                b.PaintText( wszIconCheckmark, @rcBitmap, DT_CENTER )
            end if

            rcText.left = rcText.left + pWindow->ScaleX(30) 
            rcText.right = rcText.right - pWindow->ScaleX(30) 

            ' caption
            b.SetFont( ghFont(GUIFONT_10) )
            b.PaintText( wszLeft, @rcText, DT_LEFT )

            ' keyboard accelerator 
            b.PaintText( wszRight, @rcText, DT_RIGHT )

            ' submenu arrow
            if pMenu->nChildID then
                b.SetFont( ghFont(SYMBOLFONT_10) )
                rcBitmap = rcText
                rcBitmap.Left = rcText.Right
                rcBitmap.Right = rcBitmap.Left + pWindow->ScaleX(20) 
                b.PaintText( wszIconChevronRight, @rcBitmap, DT_CENTER )
            end if   
        end if
            
        b.EndDoubleBuffer()

    end if   

    function = true 
end function


' ========================================================================================
' frmPopupMenuShadow Window procedure
' ========================================================================================
function frmPopupMenuShadow_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_NCDESTROY
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            if hwnd = HWND_SHADOW(0) then HWND_SHADOW(0) = 0
            if hwnd = HWND_SHADOW(1) then HWND_SHADOW(1) = 0

    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmPopupMenu Window procedure
' ========================================================================================
function frmPopupMenu_WndProc( _
                byval hwnd   as HWND, _
                byval uMsg   as UINT, _
                byval wParam as WPARAM, _
                byval lParam as LPARAM _
                ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_MEASUREITEM, frmPopupMenu_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    frmPopupMenu_OnDrawItem)

        case WM_NCDESTROY
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if hwnd = HWND_MENU(0) then 
                DestroyWindow(HWND_SHADOW(0))
                HWND_MENU(0) = 0
            end if
            if hwnd = HWND_MENU(1) then 
                DestroyWindow(HWND_SHADOW(1))
                HWND_MENU(1) = 0
            end if
            if pWindow then delete(pWindow)
            
        ' prevent this popup menu from stealing focus from main app
        ' This message is received before WM_NCACTIVATE for the main form is processed
        ' This popup is not activated but the mouse click is not thrown away thereby
        ' allowing us to reset the flag in WM_LBUTTUPUP
        case WM_MOUSEACTIVATE
            ' test that the form itself is being clicked on rather than a label
            gApp.KeepTitleBarActive = true
            return MA_NOACTIVATE

        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim as clsDoubleBuffer b

            b.BeginDoubleBuffer(hwnd)
            b.SetBackColor( ghPopup.PanelColor )
            b.PaintClientRect()
            b.EndDoubleBuffer()
            
            return 0
            
    end Select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmPopupMenu_SubclassProc 
' ========================================================================================
function frmPopupMenu_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _              ' // First message parameter
            byval _lParam as LPARAM, _              ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)
    
    ' save the most recent selected line for the listbox on the main popup
    ' menu. We need this to ensure that the listbox line is highlighted
    ' when we mouseover the listbox in the child submenu popup.
    ' gMenuLastCurSel  declared as a global because frmMain needs to access
    ' it for keyboard access.
    
    select case uMsg

        case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' eventual WM_MOUSELEAVE event
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 1    ' system default is 400ms
            TrackMouseEvent(@tme) 

            ' get the item rect that the mouse is over and only invalidate
            ' that instead of the entire listbox
            dim as RECT rc
            dim as long nCurSel = ListBox_GetCurSel(hWin)
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            if idx <> nCurSel then
                ListBox_SetCurSel(hWin, idx)
                ListBox_GetItemRect( hWin, idx, @rc )
                InvalidateRect( hWin, @rc, true )
                ListBox_GetItemRect( hWin, nCurSel, @rc )
                InvalidateRect( hWin, @rc, true )
                ' if we are mouse over on the submenu popup then ensure that the 
                ' line in the main menu pope that called this submenu is also highlighted.
                if GetParent(hWin) = HWND_MENU(1) then
                    dim as HWND hListBox = GetDlgItem(HWND_MENU(0), IDC_MENU_LISTBOX)
                    ListBox_SetCurSel( hListBox, gMenuLastCurSel)
                    ListBox_GetItemRect( hListBox, idx, @rc )
                    InvalidateRect( hListBox, @rc, true )
                end if
            end if
                
        case WM_MOUSELEAVE
            ListBox_SetCurSel(hWin, -1)
            AfxRedrawWindow(hWin)   
            
        case WM_LBUTTONDOWN, WM_MOUSEHOVER
            ' test if this menu item has a submenu then show it now if not already shown
            dim as HWND hListBox = GetDlgItem( HWND_MENU(0), IDC_MENU_LISTBOX )
            if hWin = hListBox then 
                ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
                ' if the specified point is in the client area of the list box, or one if it is outside the 
                ' client area.
                dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
                if hiword(idx) <> 1 then
                    dim as RECT rc
                    ListBox_GetItemRect( hWin, idx, @rc )
                    gMenuLastCurSel = idx
                    frmPopupMenu_Show( ID_SUBPOPUP, gMenuLastCurSel, hWin, rc )
                end if
            end if
            
        case WM_ERASEBKGND
            ' we paint the listbox via WM_DRAWITEM so no need to erase and paint
            ' the background here causing unnecessary flicker.
            return true

        case WM_LBUTTONUP
            gApp.KeepTitleBarActive = false
            ' process a menu item that was clicked on
            dim as long nCurSel = ListBox_GetCurSel(hWin)
            dim as long idx = ListBox_GetItemData(hWin, nCurSel)
            ' bypass if the menu entry is a separator or disabled or has a popup child menu.
            if (gTopMenu(idx).isDisabled = true) orelse (gTopMenu(idx).isSeparator = true) then
            elseif gTopMenu(idx).nChildID <> 0 then
            else
                killAllPopupMenus()
                PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(gTopMenu(idx).nID, 0), 0 )
            end if
            
        Case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmPopupMenu_SubclassProc, uIdSubclass )
    end Select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function

        
' ========================================================================================
' frmPopupMenu_GetMaxMenuWidth
' ========================================================================================
function frmPopupMenu_GetMaxMenuWidth( byval nCtrlID as long ) as long
    ' Calculate the width of the menu based on the caption text, keyboard shortcut,
    ' and any padding. Need to do this calculation because menu widths will vary
    ' based on the language used.
    ' Returns an unscaled value.
    dim as DWSTRING wszText
    dim as long max_left_width
    dim as long max_right_width
    dim as long left_width
    dim as long right_width
    dim as long interior_width = 0
    dim as DWSTRING wszLeft
    dim as DWSTRING wszRight
    
    for i as long = lbound(gTopMenu) to ubound(gTopMenu)
        if gTopMenu(i).nParentID = nCtrlID then
            wszText = getMenuText(gTopMenu(i).nID)
            ' if an embedded tab character then this menu item has a caption and a keyboard
            ' accelerator so add extra internal space to expand the tab.
            if instr(wszText, wchr(9)) then 
                interior_width = getTextWidth( HWND_FRMMAIN_MENUBAR, wchr(9), GUIFONT_10 )

                wszLeft = AfxStrParse(wszText, 1, wchr(9))
                left_width = getTextWidth( HWND_FRMMAIN_MENUBAR, wszLeft, GUIFONT_10 )
                if left_width > max_left_width then max_left_width = left_width
                
                wszRight = AfxStrParse(wszText, 2, wchr(9))
                right_width = getTextWidth( HWND_FRMMAIN_MENUBAR, wszRight, GUIFONT_10 )
                if right_width > max_right_width then max_right_width = right_width
            else   
                left_width = getTextWidth( HWND_FRMMAIN_MENUBAR, wszText, GUIFONT_10 )
                if left_width > max_left_width then max_left_width = left_width
            end if
        end if   
    next
    
    function = max_left_width + interior_width + max_right_width  ' unscaled value
end function


' ========================================================================================
' frmPopupMenu_Show
' ========================================================================================
function frmPopupMenu_Show( _
                byval nPopupType as long, _   ' popup or subpopup
                byval nItemID as long, _      ' parent listbox row 
                byval hParent as HWND, _      ' listbox or menubar hwnd
                byval rcParent as RECT _      ' listbox or menubar RECT
                ) as HWND

    dim pWindow as CPanelWindow ptr 
    dim as long nMenuLeft, nMenuTop, nMenuWidth, nMenuHeight
    dim as long nCtrlID
    dim as RECT rc
        
    frmMain_ChangeTopMenuStates()

    if nPopupType = ID_SUBPOPUP then
        ' Determine if incoming hParent menuitem has a child menu
        dim as long nIdx = ListBox_GetItemData( hParent, nItemID )
        if gTopMenu(nIdx).nChildID = 0 then 
            if IsWindowVisible(HWND_MENU(1)) then killPopupSubMenus()
            exit function
        end if
        nCtrlID = gTopMenu(nIdx).nID
        ListBox_GetItemRect( hParent, nItemID, @rc )
        MapWindowPoints( hParent, HWND_DESKTOP, cast(point ptr, @rc), 2 )
        nMenuLeft = rc.right
        nMenuTop = rc.top

        ' if the popup already exists and is being shown for this nCtrlID
        ' then simply exit, otherwise the menu will just be destroyed and
        ' re-displayed causing unnecessary flicker. The nCtrlID is stored
        ' in the UserData section of the CWindow form.
        if IsWindowVisible(HWND_MENU(1)) then
            pWindow = cast(CPanelWindow ptr, AfxCWindowPtr(HWND_MENU(1)))
            if pWindow->UserData(0) = nCtrlID then exit function
        end if
        
    elseif nPopupType = ID_POPUP then
        nCtrlID = gMenuBar(gActiveMenuBarIndex).id
        MapWindowpoints( HWND_FRMMAIN_MENUBAR, HWND_DESKTOP, cast( point ptr, @rcParent), 2 )
        rc = rcParent 
        nMenuLeft = rc.left
        nMenuTop = rc.bottom - 1  ' b/c rect doesn't include bottom pixel

        ' if the popup already exists and is being shown for this nCtrlID
        ' then simply exit, otherwise the menu will just be destroyed and
        ' re-displayed causing unnecessary flicker. The nCtrlID is stored
        ' in the UserData section of the CWindow form.
        if IsWindowVisible(HWND_MENU(0)) then
            pWindow = cast(CPanelWindow ptr, AfxCWindowPtr(HWND_MENU(0)))
            if pWindow->UserData(0) = nCtrlID then exit function
        end if
    end if
    
    ' Keep the main form's titlebar painting as active while the popup windows are
    ' created (their creation would otherwise deactivate it and flicker the caption).
    gApp.KeepTitleBarActive = true
    
    '  Popup menus are non-modal (modeless) windows
    pWindow = New CPanelWindow
    nMenuLeft = pWindow->UnScaleX(nMenuLeft)
    nMenuTop = pWindow->UnScaleY(nMenuTop)

    ' if popup menu was already active prior to this menubar button gaining
    ' focus then automatically show the new popup menu rather than forcing
    ' the user to have to click on the button.
    dim as HWND prevPopup
    if nPopupType = ID_POPUP and IsWindow(HWND_MENU(0)) then prevPopup = HWND_MENU(0)
    if nPopupType = ID_SUBPOPUP and IsWindow(HWND_MENU(1)) then prevPopup = HWND_MENU(1)

    dim as HWND hPopupMenu = pWindow->Create( HWND_FRMMAIN, @frmPopupMenu_WndProc, _
        nMenuLeft, nMenuTop, 0, 0, WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->UserData(0) = nCtrlID

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS
    
    dim as HWND hPopupListBox = _ 
        pWindow->AddControl("LISTBOX", , IDC_MENU_LISTBOX, "", 0, 0, 0, 0, _
        WS_CHILD or WS_TABSTOP or LBS_NOINTEGRALHEIGHT or _
        LBS_OWNERDRAWFIXED or LBS_HASSTRINGS or LBS_NOTIFY, WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(hPopupListBox, @frmPopupMenu_SubclassProc, IDC_MENU_LISTBOX, @pWindow)

    dim as long margin_width = 40
    ' get unscaled text width and add left/right margin areas
    nMenuWidth = frmPopupMenu_GetMaxMenuWidth(nCtrlID) + (margin_width * 2)  
    select case nCtrlID
        case IDM_MRU
            ' update the list of filenames with the most current data
            ' the width of the MRU menu depends on the width of the actual filenames
            nMenuWidth = max( 262, updateMRUFilesItems() )         
        case IDM_MRUSESSION
            nMenuWidth = max( 262, updateMRUSessionFilesItems() )
        case IDM_MRUPROJECT
            nMenuWidth = max( 262, updateMRUProjectFilesItems() )
    end select
    
    ' add the menuitems to the popup menu ownerdraw listbox. we do this after the
    ' above menu width calculations because if this is an MRU menu then the menuitems
    ' text would have possibly changed.
    for i as long = lbound(gTopMenu) to ubound(gTopMenu)
        if gTopMenu(i).nParentID = nCtrlID then
            dim as long idx = Listbox_AddString( hPopupListBox, @"" )  ' text is retrieved from array when drawn
            ListBox_SetItemData( hPopupListBox, idx, i )    ' store index to gTopMenu array
        end if   
    next
    
    ' calculate final size of the popup menu based on margins/padding and the embedded listbox
    dim as long nLeft, nTop, nWidth, nHeight
    
    nHeight = iif(gConfig.CompactMenus, MENUITEM_HEIGHT_COMPACT, MENUITEM_HEIGHT)
    nLeft = 2
    nTop = 10
    nWidth = nMenuWidth - (nLeft * 2)
    nHeight = (ListBox_GetCount(hPopupListBox) * nHeight)

    SetWindowPos( hPopupListBox, 0, _
                    pWindow->ScaleX(nLeft), pWindow->ScaleY(nTop), _
                    pWindow->ScaleX(nWidth), pWindow->ScaleY(nHeight), _
                    SWP_NOZORDER or SWP_SHOWWINDOW)

    nMenuHeight = nTop + nHeight + nTop
    
    if nPopupType = ID_SUBPOPUP then
        ' Attempt to adjust the positioning of the popup should it extend past the bottom 
        ' of the visible display dimensions of the desktop.
        dim as long desktop_height = AfxGetWorkAreaHeight()
        dim as rect rcPopupMenu = AfxGetWindowRect(hPopupMenu)
        dim as long popupmenu_height = pWindow->ScaleY(nMenuHeight)
        if rcPopupMenu.top + popupmenu_height > desktop_height then
            rcPopupMenu.top = desktop_height - popupmenu_height
            AfxSetWindowLocation(hPopupMenu, rcPopupMenu.left, rcPopupMenu.top)
            nMenuTop = pWindow->UnScaleY(rcPopupMenu.top)
        end if
    end if      
    
    ' Create semi-transparent window slightly offset under our popup menu in order to simulate a shadow.
    pWindow = new CPanelWindow
    dim as HWND hPopupShadow = pWindow->Create( HWND_FRMMAIN, _
        @frmPopupMenuShadow_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, WS_EX_LAYERED or WS_EX_NOACTIVATE )
    pWindow->Brush = GetSysColorBrush(COLOR_WINDOWTEXT)   ' black background
    SetLayeredWindowAttributes( hPopupShadow, 0, 20, LWA_ALPHA ) 

    if nPopupType = ID_POPUP and IsWindowVisible(HWND_MENU(1)) then killPopupSubMenus()
    if IsWindow(prevPopup) then DestroyWindow(prevPopup)

    SetWindowPos( hPopupShadow, HWND_TOP, _
                    pWindow->ScaleX(nMenuLeft + 4), pWindow->ScaleY(nMenuTop + 4), _
                    pWindow->ScaleX(nMenuWidth), pWindow->ScaleY(nMenuHeight), _
                    SWP_NOACTIVATE or SWP_SHOWWINDOW)

    SetWindowPos( hPopupMenu, HWND_TOP, 0, 0, _
                    pWindow->ScaleX(nMenuWidth), pWindow->ScaleY(nMenuHeight), _
                    SWP_NOACTIVATE or SWP_NOMOVE or SWP_SHOWWINDOW)
    gApp.KeepTitleBarActive = false
    
    HWND_MENU(nPopupType) = hPopupMenu
    HWND_SHADOW(nPopupType) = hPopupShadow
            
    function = hPopupMenu
    
end function

