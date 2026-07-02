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


''  HANDLE RAW INCOMING MESSAGES IN THE MESSAGE PUMP


function handleMouseShowScrollBar( byval uMsg as MSG ) as boolean
    if (gApp.IsProjectLoading) orelse (gApp.IsFileLoading) then return false

    ' Determine if the mouse has moved over (or away from) a window that should invoke
    ' showing or hiding our custom Panel VScroll or Editor HScroll.
    if (uMsg.message = WM_MOUSEMOVE) orelse (uMsg.message = WM_NCMOUSEMOVE) then
        ' Ensure that if moved away from menubar that the selected index is reset.
        if (isMouseOverWindow(HWND_FRMMAIN_MENUBAR) = false) andalso _
            (isPopupMenuOpen() = false) andalso _
            (gActiveMenuBarIndex <> -1) then
            gActiveMenuBarIndex = -1
            AfxRedrawWindow(HWND_FRMMAIN_MENUBAR)
        end if   
    end if

    ' Should we show an Editor HScrollbar
    dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
    if (pDoc <> 0) andalso (pDoc->bSizing = false) then   ' no testing during split panel resizing
        ' If the xOffset has changed then we need to update the horizontal scrollbar
        dim as long GetXOffset = SciExec( pDoc->hWndActiveScintilla, SCI_GETXOFFSET, 0, 0 )
        if pDoc->lastXOffsetPos <> GetXOffset then
            pDoc->lastXOffsetPos = GetXOffset
            AfxRedrawWindow( HWND_FRMEDITOR_HSCROLLBAR(0) )
            AfxRedrawWindow( HWND_FRMEDITOR_HSCROLLBAR(1) )
        end if
        frmEditorHScroll_DoNeedScrollBar( pDoc )
    end if
    
    ' we want our mouse message to continue so return FALSE
    return false
end function


function handleMouseTopMenu( byval uMsg as MSG ) as boolean
    ' Handle any mouse clicks that are outside of a popup menu in order
    ' to close any open top menu popups.
    if uMsg.message = WM_LBUTTONDOWN then
        if (uMsg.HWnd <> HWND_MENU(0)) andalso _
            (GetParent(uMsg.HWnd) <> HWND_MENU(0)) andalso _
            (uMsg.HWnd <> HWND_MENU(1)) andalso _
            (GetParent(uMsg.HWnd) <> HWND_MENU(1)) then
            gActiveMenuBarIndex = -1
            killAllPopupMenus()
        end if
    elseif uMsg.message = WM_NCLBUTTONDOWN then
        gActiveMenuBarIndex = -1
        killAllPopupMenus()
    end if
    ' we want our mouse message to continue so return FALSE
    return false
end function


function handleAltKeyMenuBar( byval uMsg as MSG ) as boolean
    ' 2022-01-08: skip processing the Alt key here because it seems to
    ' interfere with the Alt+Mouse column editing in Scintilla. Need
    ' to investigate a better alternative. The following testing for 
    ' the mouse button does work, but it still results in the menubar
    ' being selected/deselected.

    ' Handle the Split editor window icon in the toptab control
    if IsWindowVisible(HWND_FRMMAIN_TOPTABS) then
        dim as DWSTRING wszNewSplitButton
        ' Alt key is currently down
        if (GetAsyncKeyState(VK_MENU) and &H8000) then
            wszNewSplitButton = wszIconSplitTopBottom
        else    
            wszNewSplitButton = wszIconSplitLeftRight
        end if    
        if wszIconSplitEditor <> wszNewSplitButton then
            wszIconSplitEditor = wszNewSplitButton
            AfxRedrawWindow(HWND_FRMMAIN_TOPTABS)
        end if    
    end if
    
'   ' single ALT key pressed, activate the first top menubar button
'    if (uMsg.message = WM_SYSKEYDOWN) and (uMsg.wParam = VK_MENU) then
'      killPopupSubMenus()
'      killPopupMenus()
'      ' if active menubar exists then simply toggle it off and exit
'      dim as HWND hCtrl 
'      if gActiveMenuBarIndex then
'         hCtrl = gActiveMenuBarIndex
'         gActiveMenuBarIndex = 0
'         AfxRedrawWindow(hCtrl)
'         return true
'      else   
'         hCtrl = GetDlgItem(HWND_FRMMAIN_MENUBAR, IDC_MENUBAR_FILE)
'         gActiveMenuBarIndex = hCtrl
'         AfxRedrawWindow(hCtrl)
'         return true
'      end if   
'   end if
    ' we want our message to continue so return FALSE
    return false
end function


function handleKeysTopMenu( byval uMsg as MSG ) as boolean
    ' If topmenus or menubar are active then process all keyboard input for them rather than 
    ' passing it off to the system to process.
    if (uMsg.message = WM_KEYDOWN) andalso (gActiveMenuBarIndex <> -1) then
        if uMsg.wParam = VK_RETURN then
            ' simulate pressing ENTER on a menuitem by sending WM_LBUTTONUP message
            ' to the active popup menu or menubar button. If this item hasa submenu 
            ' then convert the key from ENTER to RIGHT ARROW.
            dim as HWND hListBox
            if isPopupMenuOpen() then hListBox = GetDlgItem(HWND_MENU(0), IDC_MENU_LISTBOX)
            if isSubPopupMenuOpen() then hListBox = GetDlgItem(HWND_MENU(1), IDC_MENU_LISTBOX)
            ' If we have a valid listbox then process the action on the current listbox selection, otherwise
            ' no popup menu exists so simulate pressing enter on the active menubar button.
            if hListBox then
                ' Determine if the menu entry has a child popup. If yes, then convert the keypress to right
                ' arrow and process it below in the VK_RIGHT handler.
                dim as long nCurSel = ListBox_GetCurSel(hListBox)
                dim as long idx = ListBox_GetItemData(hListBox, nCurSel)
                if gTopMenu(idx).nChildID <> 0 then
                    uMsg.wParam = VK_RIGHT
                else
                    SendMessage( hListBox, WM_LBUTTONUP, 0, 0 )
                    return true
                end if   
            else
                ' Tell the menubar to activate the popupmenu for the currently
                ' selected menubar item. Send wParam = -1 to indicate not do any
                ' hot hittesting in the popupmenu LBUTTONDOWN message handler.
                SendMessage( HWND_FRMMAIN_MENUBAR, WM_LBUTTONDOWN, cast(WPARAM, cint(-1)), 0 )
                return true
            end if
        end if
        
        select case uMsg.wParam
            case VK_ESCAPE
            if isSubPopupMenuOpen() then 
                killPopupSubMenus()
                return true
            elseif isPopupMenuOpen() then
                killPopupMenus()
                return true
            else
                ' No popup menus are open but if a menubar item is hot then
                ' set the selected index.
                gActiveMenuBarIndex = frmMenuBar_GetHotIndex(HWND_FRMMAIN_MENUBAR)
                AfxRedrawWindow(HWND_FRMMAIN_MENUBAR)
                return true
            end if   
    
            case VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN
            if isAnyPopupMenuOpen() then   
                if uMsg.wParam = VK_DOWN then 
                    setNextMenuItemTabIndex(false)
                    return true
                end if
                if uMsg.wParam = VK_UP then 
                    setNextMenuItemTabIndex(true)
                    return true
                end if
                if uMsg.wParam = VK_LEFT then 
                    if isSubPopupMenuOpen() then
                        killPopupSubMenus()
                        return true
                    end if   
                end if
                if uMsg.wParam = VK_RIGHT then
                    if isPopupMenuOpen() then
                        dim as HWND hListBox = GetDlgItem(HWND_MENU(0), IDC_MENU_LISTBOX)
                        dim as RECT rcItem
                        gMenuLastCurSel = ListBox_GetCurSel(hListBox)
                        if gMenuLastCurSel <> -1 then
                            ListBox_GetItemRect(hListBox, gMenuLastCurSel, @rcItem)
                            if frmPopupMenu_Show( ID_SUBPOPUP, gMenuLastCurSel, hListBox, rcItem ) then
                                setNextMenuItemTabIndex(false)
                                return true
                            end if
                        end if       
                    end if
                end if
            end if
        
            if gActiveMenuBarIndex <> -1 then
                if uMsg.wParam = VK_LEFT then 
                    gActiveMenuBarIndex -= 1
                    if gActiveMenuBarIndex < lbound(gMenuBar) then gActiveMenuBarIndex = ubound(gMenuBar)
                    ' if popup menus are already activated then continue to show them as
                    ' we move through the various menubar buttons.
                    if isPopupMenuOpen() then
                        frmPopupMenu_Show(ID_POPUP, 0, HWND_FRMMAIN_MENUBAR, gMenuBar(gActiveMenuBarIndex).rcItem)
                        setNextMenuItemTabIndex(false)
                    end if   
                end if
                if uMsg.wParam = VK_RIGHT then 
                    gActiveMenuBarIndex += 1
                    if gActiveMenuBarIndex > ubound(gMenuBar) then gActiveMenuBarIndex = lbound(gMenuBar)
                    ' if popup menus are already activated then continue to show them as
                    ' we move through the various menubar buttons.
                    if isPopupMenuOpen() then
                        frmPopupMenu_Show(ID_POPUP, 0, HWND_FRMMAIN_MENUBAR, gMenuBar(gActiveMenuBarIndex).rcItem)
                        setNextMenuItemTabIndex(false)
                    end if   
                end if
                if uMsg.wParam = VK_DOWN then 
                    frmPopupMenu_Show(ID_POPUP, 0, HWND_FRMMAIN_MENUBAR, gMenuBar(gActiveMenuBarIndex).rcItem)
                    setNextMenuItemTabIndex(false)
                end if   
                AfxRedrawWindow(HWND_FRMMAIN_MENUBAR)
                return true
            end if
        
            case else
            return true
        end select
    end if

    ' we want our message to continue so return FALSE
    return false
end function


function handleKeysExplorerListBox( byval uMsg as MSG ) as boolean
    ' Was ENTER key pressed while scrolling the Explorer listbox via keyboard
    dim as HWND hList = GetDlgItem(HWND_FRMEXPLORER, IDC_FRMEXPLORER_LISTBOX)
    if (uMsg.message = WM_KEYDOWN) andalso (uMsg.HWnd = hList) then
        dim as long nSel = ListBox_GetCaretIndex( hList )
        dim as DWSTRING wszCaption = AfxGetListBoxText( hList, nSel )
        if uMsg.wParam = VK_RETURN then
            if left(wszCaption, 1) = "%" then
                ' expand/contract the header node
                dim as long idxArray = getExplorerNodeHeaderIndex( wszCaption )
                if idxArray <> -1 then
                    gConfig.Cat(idxArray).bShow = not gConfig.Cat(idxArray).bShow
                end if
                LoadExplorerFiles()
                SetFocus( hList )
                return true
            else   
                ' open the selected document
                dim as DWSTRING wszFilename
                dim as long p = ListBox_GetItemData( hList, nSel )
                dim pDoc as clsDocument ptr = cast( clsDocument ptr, cast(integer, p) )
                if pDoc then
                    wszFilename = pDoc->DiskFilename
                    OpenSelectedDocument( wszFilename, "" )
                end if
                SetFocus( hList )
                return true
            end if
        end if
    end if

    ' we want our message to continue so return FALSE
    return false
end function


function handleEscKeyModeless( byval uMsg as MSG ) as boolean
    ' Close any open Find/Replace window with ESC key
    ' Close any open FindInFiles window with ESC key
    if (uMsg.message = WM_KEYDOWN) andAlso (uMsg.wParam = VK_ESCAPE) then
        if IsWindowVisible( HWND_FRMMAIN_TOPTABSINFO ) THEN 
            FindReplaceControls_Hide()
            return true
        end if
        if IsWindowVisible( HWND_FRMFINDINFILES ) then 
            SendMessage( HWND_FRMFINDINFILES, WM_CLOSE, 0, 0 )
            return true
        end if
    end if
    ' we want our message to continue so return FALSE
    return false
end function


function handleKeysFindReplace( byval uMsg as MSG ) as boolean
    ' For Find textbox, an ENTER will move to the next selection. A SHIFT+ENTER
    ' will move to the previous. For Replace textbox, use the same logic but 
    ' also perform the replace action.
    dim as HWND hFind = GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTFIND )
    dim as HWND hReplace = GetDlgItem( HWND_FRMMAIN_TOPTABSINFO, IDC_FRMTOPTABSINFO_TXTREPLACE )
    if (GetFocus = hFind) orelse (GetFocus = hReplace) then
        if (uMsg.message = WM_KEYDOWN) and (uMsg.wParam = VK_RETURN) then
            if (GetAsyncKeyState(VK_CONTROL) and &H8000) andalso _
            (GetAsyncKeyState(VK_MENU) and &H8000) THEN
                ' Ctrl+Alt+Enter  Replace All
                SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEALL, 0), 0 )
                return true
            end if
            if (GetAsyncKeyState(VK_SHIFT) and &H8000) then
                if GetFocus = hFind then
                    SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDPREV, 0), 0 )
                elseif GetFocus = hReplace then
                    SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACEPREV, 0), 0 )
                end if   
                return true
            else   
                if GetFocus = hFind then
                    SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDNEXT, 0), 0 )
                elseif GetFocus = hReplace then
                    SendMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACENEXT, 0), 0 )
                end if
                return true
            end if   
        end if
    end if
    ' we want our message to continue so return FALSE
    return false
end function



        
