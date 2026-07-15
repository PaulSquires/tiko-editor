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
''  frmExplorer.inc
''   

#include once "frmExplorer.bi"
#include once "frmFunctions.bi"
#include once "clsDB2.bi"

#define NODE_INDEX_SIZE    5     ' size of field holding array index into gConfig.Cat()
#define NODE_STATE_SIZE    1     ' size of field holding 0 collapsed, 1 expanded

function getExplorerNodeHeaderIndex( byval wszCaption as DWSTRING ) as long
    ' wszCaption will be in this formt:
    '  %NODE_INDEX_SIZE%NODE_STATE_SIZE%Description
        '       %    0%1%Files
        '       %    1%1%Main
        '       %    2%1%Resource
        '       %    3%0%Header
        '       %    4%0%Module
        '       %    5%1%Normal
    dim as long idx = val(mid(wszCaption, 2, NODE_INDEX_SIZE))
    ' Make sure array index is valid
    if (idx < lbound(gConfig.Cat)) orelse (idx > ubound(gConfig.Cat)) then
        return -1
    else
        return idx   
    end if
end function

function getExplorerNodeHeaderState( byval wszCaption as DWSTRING ) as long
    ' wszCaption will be in this formt:
    '  %NODE_INDEX_SIZE%NODE_STATE_SIZE%Description
        '       %    0%1%Files
        '       %    1%1%Main
        '       %    2%1%Resource
        '       %    3%0%Header
        '       %    4%0%Module
        '       %    5%1%Normal
    dim as long nState = val(mid(wszCaption, 2 + NODE_INDEX_SIZE + 1, NODE_STATE_SIZE))
    ' nState will be 0 collapsed, or 1 expanded
    return nState
end function

function getExplorerNodeHeaderDescription( byval wszCaption as DWSTRING ) as DWSTRING
    ' wszCaption will be in this formt:
    '  %NODE_INDEX_SIZE%NODE_STATE_SIZE%Description
        '       %    0%1%Files
        '       %    1%1%Main
        '       %    2%1%Resource
        '       %    3%0%Header
        '       %    4%0%Module
        '       %    5%1%Normal
    return mid(wszCaption, 4 + NODE_INDEX_SIZE + NODE_STATE_SIZE)
end function

' ========================================================================================
' Expand/Collapse all Explorer Nodes
' ========================================================================================
function frmExplorer_ExpandAll() as long
    for i as long = lbound(gConfig.Cat) to ubound(gConfig.Cat)
        gConfig.Cat(i).bShow = true
    next
    LoadExplorerFiles()
    function = 0
end function

function frmExplorer_CollapseAll() as long
    for i as long = lbound(gConfig.Cat) to ubound(gConfig.Cat)
        gConfig.Cat(i).bShow = false
    next
    LoadExplorerFiles()
    function = 0
end function   


' ========================================================================================
' Unselect all items in the multiselect listbox and reset it to the currently
' active top tab (if any)
' ========================================================================================
function frmExplorer_UnSelectListBox() as long
    for i as long = 0 to ListBox_GetCount( HWND_FRMEXPLORER_LISTBOX ) - 1 
        ListBox_SetSel( HWND_FRMEXPLORER_LISTBOX, false, i )
    next 
    ' force a listbox selection for the currently active tab                   
    dim as long idx = gTTabCtl.CurSel: gTTabCtl.CurSel = -1
    gTTabCtl.SetFocusTab(idx)
    function = 0
end function


' ========================================================================================
' Select the listbox item that matches the incoming pDoc item. Do not open nodes to
' find a possible hidden document because the user may have purposely closed a node
' and we should not re-open it automatically.
' ========================================================================================
function frmExplorer_SelectItemData( byval pDoc as clsDocument ptr ) as boolean
    ' Select the Explorer listbox item where the ItemData holds the pDoc handle.
    ' Reset any other single or multiple selections. Process entire listbox. 
    if pDoc = 0 then return false
    dim as long idx = -1
    for i as long = 0 to ListBox_GetCount( HWND_FRMEXPLORER_LISTBOX ) - 1 
        ListBox_SetSel( HWND_FRMEXPLORER_LISTBOX, false, i )
        if ListBox_GetItemData(HWND_FRMEXPLORER_LISTBOX, i) = pDoc then
            idx = i
        end if   
    next 

    if idx <> -1 then
        ListBox_SetSel( HWND_FRMEXPLORER_LISTBOX, true, idx )
        AfxRedrawWindow( HWND_FRMEXPLORER_LISTBOX )
        frmPanelVScroll_calcVThumbRect()
        AfxRedrawWindow( HWND_FRMPANEL_VSCROLLBAR )
    end if
    
    if idx >= 0 then function = true   
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmExplorer_PositionWindows() as LRESULT
    ' Get the entire client area
    dim as Rect rc
    GetClientRect( HWND_FRMEXPLORER, @rc )

    SetWindowPos( HWND_FRMEXPLORER_LISTBOX, 0, _
                    rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, _
                    SWP_NOZORDER )

    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnDestroy( byval hwnd as HWND ) as LRESULT 
    function = 0
end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmExplorer_PositionWindows
    end if
    function = 0
end function
 
        
' ========================================================================================
' Process WM_PAINT message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as clsDoubleBuffer b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColors( ghPanel.BackColor, ghPanel.BackColor )
    b.PaintClientRect()
    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long
    ' Set the height of the list box items. 
    dim as long nHeight = iif(gConfig.CompactMenus, EXPLORERITEM_HEIGHT_COMPACT, EXPLORERITEM_HEIGHT)
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    lpmis->itemHeight = pWindow->ScaleY(nHeight)
        
    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnDrawItem( _
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

        dim as boolean IsHot = false
        dim as boolean isNodeHeader = false
        dim as boolean isIconDown = false
        
        if isMouseOverRECT(lpdis->hwndItem, b.rcClient) then IsHot = true   
        if isMouseOverWindow( HWND_FRMPANEL_VSCROLLBAR ) then IsHot = false
        if ListBox_GetSel(lpdis->hwndItem, lpdis->itemID) then IsHot = true

        dim as COLORREF backclr = iif( IsHot, ghPanel.BackColorHot,  ghPanel.BackColor)
        dim as COLORREF foreclr = iif( IsHot, ghPanel.ForeColorHot,  ghPanel.ForeColor)
        b.SetForeColors(foreclr, foreclr)
        b.SetBackColors(backclr, backclr)

        ' if this is a "node" header then use that icon
        dim as DWSTRING wszCaption = AfxGetListBoxText(lpdis->hwndItem, lpdis->ItemID)
        if left(wszCaption, 1) = "%" then isNodeHeader = true
        if getExplorerNodeHeaderState(wszCaption) = 1 then isIconDown = true
        
        ' Create our rect that works with the entire line
        dim as RECT rc
        SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
        dim as RECT rcText = rc 
        dim as RECT rcBitmap = rc 
        
        ' Paint the entire background
        b.PaintRect( @rc )

        ' indent the text based on its type
        if isNodeHeader then
            b.SetFont( SYMBOLFONT_10 )
            rcBitmap.right = rcBitmap.left + pWindow->ScaleX(20)
            dim as DWSTRING wszBitmap = iif(isIconDown, wszIconChevronDown, wszIconChevronRight) 
            b.PaintText( wszBitmap, @rcBitmap, DT_CENTER )

            b.SetFont( GUIFONT_10 )
            wszCaption = getExplorerNodeHeaderDescription(wszCaption)
            rcText.left = rcBitmap.right
            b.PaintText( wszCaption, @rcText, DT_LEFT )
        else
            ' This would be a regular file.
            rcBitmap.left = rcText.left + pWindow->ScaleX(20)
            rcBitmap.right = rcBitmap.left + pWindow->ScaleX(20) 

            b.SetFont( GUIFONT_10 )  ' use GUI rather than SYMBOL b/c of dot icon
            b.PaintText( wszIconDocument, @rcBitmap, DT_CENTER )

            b.SetFont( GUIFONT_10 )
            rcText.left = rcBitmap.right
            b.PaintText( wszCaption, @rcText, DT_LEFT )
        end if
            
        b.EndDoubleBuffer()

    end if   

    function = true
    
end function

' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmExplorer
' ========================================================================================
function frmExplorer_OnCommand( _
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
            frmExplorer_PositionWindows()
    end select      

    function = 0
end function


' ========================================================================================
' frmExplorer Window procedure
' ========================================================================================
function frmExplorer_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_DESTROY,     frmExplorer_OnDestroy)
        HANDLE_MSG (hwnd, WM_SIZE,        frmExplorer_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,       frmExplorer_OnPaint)
        HANDLE_MSG (hwnd, WM_COMMAND,     frmExplorer_OnCommand)
        HANDLE_MSG (hwnd, WM_MEASUREITEM, frmExplorer_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    frmExplorer_OnDrawItem)
    
    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMEXPLORER = 0
        return 0

    case WM_ERASEBKGND
        return true

    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function

' ========================================================================================
' frmExplorerListBox_SubclassProc 
' ========================================================================================
function frmExplorerListBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMEXPLORER)
    static as long accumDelta
    
    static as DWSTRING wszPrevTooltip 
    static as HWND hToolTip
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)
        
    ' keep track of last index we were over so that we only issue a 
    ' repaint if the cursor has moved off of the line
    static as long nLastIdx = -1
        
    select case uMsg
        case MSG_USER_LOAD_EXPLORERFILES
            LoadExplorerFiles()

        Case WM_MOUSEWHEEL
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
                frmPanel_PositionWindows
            elseif accumDelta <= -120 then  ' scroll down 3 lines
                nTopIndex = nTopIndex + 3
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
                frmPanel_PositionWindows
            end if

        Case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' eventual WM_MOUSEHOVER and WM_MOUSELEAVE events
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 1    ' HOVER_DEFAULT is 500
            TrackMouseEvent(@tme) 
            
            if IsWindow(hTooltip) = 0 then hTooltip = AfxAddTooltip( hWin, "", false, false )

            ' get the item rect that the mouse is over and only invalidate
            ' that instead of the entire listbox
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                if idx <> nLastIdx then
                    ListBox_GetItemRect( hWin, idx, @rc )
                    InvalidateRect( hWin, @rc, true )
                    ListBox_GetItemRect( hWin, nLastIdx, @rc )
                    InvalidateRect( hWin, @rc, true )
                    nLastIdx = idx
                end if
            end if
                        
        case WM_MOUSEHOVER
            dim as DWSTRING wszTooltip 
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                dim as clsDocument ptr pDoc = cast(clsDocument ptr, ListBox_GetItemData( hWin, idx ))
                if pDoc then wszTooltip = pDoc->DiskFilename
                ' Display the tooltip
                if wszTooltip <> wszPrevTooltip then
                    AfxSetTooltipText( hTooltip, hWin, wszTooltip )
                    wszPrevTooltip = wszTooltip 
                end if    
            end if
            
        case WM_MOUSELEAVE
            wszPrevTooltip = ""
            nLastIdx = -1
            AfxRedrawWindow(hWin)
            
        case WM_RBUTTONDOWN
            ' Create the popup menu
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                dim as clsDocument ptr pDoc = cast(clsDocument ptr, ListBox_GetItemData( hWin, idx ))
                if pDoc then
                    ' If only 1 line has been previously selected then unselect that line
                    ' and select this current line, otherwise, the menu actions on all selected lines.
                    dim as long nSelCount = SendMessage( hWin, LB_GETSELCOUNT, 0, 0 )
                    if nSelCount = 1 then
                        for i as long = 0 to ListBox_GetCount( hWin ) - 1 
                            ListBox_SetSel( hWin, false, i )
                        next 
                    end if
                    
                    ListBox_SetSel( hWin, true, idx )
                    dim as HMENU hPopupMenu = CreateExplorerContextMenu(pDoc)
                    dim as POINT pt: GetCursorPos( @pt )
                    dim as long id = TrackPopupMenu(hPopUpMenu, TPM_RETURNCMD, pt.x, pt.y, 0, HWND_FRMMAIN, byval null)
                    ' process the selected rows. Need to save the current state of selected
                    ' rows prior to processing because as the items are processed some of the
                    ' actions may reset the state of listbox. We save the pDoc's in an array
                    ' rather than index values because the listbox may get reloaded during
                    ' some actions invalidating some indexes. 
                    nSelCount = SendMessage( hWin, LB_GETSELCOUNT, 0, 0 )
                    dim pDocArray(nSelCount-1) as clsDocument ptr 
                    dim as long nextIdx = 0
                    for i as long = 0 to ListBox_GetCount( hWin ) - 1
                        if ListBox_GetSel( hWin, i ) then
                            dim as DWSTRING wszCaption = AfxGetListBoxText( hWin, i )
                            if left(wszCaption, 1) = "%" then 
                                pDocArray(nextIdx) = 0
                            else   
                                pDocArray(nextIdx) = cast(clsDocument ptr, ListBox_GetItemData( hWin, i ))
                            end if
                            nextIdx = nextIdx + 1
                        end if
                    next         
                    ' Process our array of selected pDoc's.
                    for i as long = lbound(pDocArray) to ubound(pDocArray)
                        if pDocArray(i) = 0 then continue for
                        select case id
                            case IDM_FILEOPEN_EXPLORERLISTBOX, IDM_FILECLOSE_EXPLORERLISTBOX, _
                                IDM_FILESAVE_EXPLORERLISTBOX, IDM_FILESAVEAS_EXPLORERLISTBOX, _
                                IDM_FILERENAME_EXPLORERLISTBOX, IDM_FILEDUPLICATE_EXPLORERLISTBOX
                                OnCommand_FileExplorerMessage( id, pDocArray(i) )
                            case IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX
                                OnCommand_ProjectRemove( id, pDocArray(i) )
                            case IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, _
                                IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEMODULE_EXPLORERTREEVIEW, _
                                IDM_SETFILENORMAL_EXPLORERTREEVIEW 
                                OnCommand_ProjectSetFileType( id, pDocArray(i) )
                            case is > IDM_SETCATEGORY     
                                OnCommand_ProjectSetFileType( id, pDocArray(i) )
                        end select
                    next
                    LoadExplorerFiles()
                    frmExplorer_UnSelectListBox() 
                    DestroyMenu( hPopUpMenu )
                    return true   ' prevent further processing that leads to WM_CONTEXTMENU
                end if
            end if
            
        case WM_LBUTTONUP
            ' Prevent this programmatic selection if Ctrl or Shift is active
            ' because we want the listbox to select the listbox item rather for
            ' us to mess with that selection via SetTabIndexByDocumentPtr().
            dim as boolean isCtrl = (GetAsyncKeyState(VK_CONTROL) and &H8000)
            dim as boolean isShift = (GetAsyncKeyState(VK_SHIFT) and &H8000) 
            if (isCtrl = false) andalso (isShift = false) then
                ' determine if we clicked on a regular file or a node header
                dim as RECT rc
                dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
                ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
                ' if the specified point is in the client area of the list box, or one if it is outside the 
                ' client area.
                if hiword(idx) <> 1 then
                    dim as DWSTRING wszCaption = AfxGetListBoxText( hWin, idx )
                    if left(wszCaption, 1) = "%" then
                        ' Toggle the show/hide of files under this node
                        dim as long idxArray = getExplorerNodeHeaderIndex(wszCaption)
                        if idxArray <> -1 then
                            gConfig.Cat(idxArray).bShow = not gConfig.Cat(idxArray).bShow
                        end if   
                        ' allow listbox click event to fully process before loading new explorer files
                        ' so that we can correctly select the current item.
                        PostMessage( hWin, MSG_USER_LOAD_EXPLORERFILES, 0, 0 )
                    else
                        ' if the file is already showing in the Top tabs then switch to that tab.
                        ' We do not open a new tab on single click. Only double click or ENTER will
                        ' open a new tab. 
                        dim as clsDocument ptr pDoc = cast(clsDocument ptr, ListBox_GetItemData( hWin, idx ))
                        gTTabCtl.SetTabIndexByDocumentPtr( pDoc )
                    end if
                end if
            end if
                    
        case WM_LBUTTONDBLCLK 
            ' determine if we clicked on a regular file or a node header. If it is a regular
            ' file then load it.
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                dim as DWSTRING wszCaption = AfxGetListBoxText( hWin, idx )
                if left(wszCaption, 1) <> "%" then
                    dim as DWSTRING wszFilename
                    dim as clsDocument ptr pDoc = cast(clsDocument ptr, ListBox_GetItemData( hWin, idx ))
                    if pDoc then
                        ' If the pDoc was already in an open tab then we would have already switched
                        ' to it during the WM_LBUTTONUP message handler. Check here to see if we are 
                        ' indeed on that that, otherwise we will open it now.
                        if pDoc <> gTTabCtl.GetActiveDocumentPtr() then
                            wszFilename = pDoc->DiskFilename
                            OpenSelectedDocument( wszFilename )
                        end if
                        frmMain_SetFocusToCurrentCodeWindow()
                    end if   
                end if
            end if
                
        case WM_ERASEBKGND
            ' if the number of lines in the listbox maybe less than the number per page then 
            ' calculate from last item to bottom of listbox, otherwise calculate based on
            ' the mod of the lineheight to listbox height so we can color the partial line
            ' that won't be displayed at the bottom of the list.
            dim as RECT rc = GetListBoxEmptyClientArea( hWin )
            if rc.top < rc.bottom then
                dim as HDC _hDC = cast(HDC, _wParam)
                PaintRect( _hDC, @rc, ghPanel.BackColor )
            end if
            return true
            
        Case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmExplorerListBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' frmExplorer_Show
' ========================================================================================
function frmExplorer_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = New CPanelWindow

    HWND_FRMEXPLORER = pWindow->Create( hWndParent, @frmExplorer_WndProc )
    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    HWND_FRMEXPLORER_LISTBOX = _ 
        pWindow->AddControl("LISTBOX", , IDC_FRMEXPLORER_LISTBOX, "", 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or WS_TABSTOP or _
        LBS_NOINTEGRALHEIGHT or LBS_EXTENDEDSEL or LBS_MULTIPLESEL or _
        LBS_OWNERDRAWFIXED or LBS_HASSTRINGS or LBS_NOTIFY, _
        WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(HWND_FRMEXPLORER_LISTBOX, @frmExplorerListBox_SubclassProc, IDC_FRMEXPLORER_LISTBOX, @pWindow)

    function = 0
    
end function


' ========================================================================================
' LoadExplorerFiles
' This will clear the current list of files in the Explorer and repopulate it
' with the latest list of filenames that are stored in the hidden FunctionList treeview.
' Files are excluded from being added based on the state of the global array gConfig.Cat()
' ========================================================================================
function LoadExplorerFiles() as long
    dim as HWND hList = HWND_FRMEXPLORER_LISTBOX 
    redim pDocs(any) as clsDocument ptr
    
    ' Hide the listbox while it is loading so that we don't get the unpainted
    ' white background from the empty listbox
    ShowWindow( hList, SW_HIDE )
    
    ' Save the topindex because we will restore it after filling the new contents 
    dim as long nTopIndex = SendMessage( hList, LB_GETTOPINDEX, 0, 0 ) 

    dim pDocActive as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    
    dim as long idxRestore = -1
    dim as long idxCaret = ListBox_GetCaretIndex( hList )

    ' Clear all content from the listbox
    ListBox_ResetContent(hList)
    
    ' Add the list of filenames (allow the treeview to sort the data for us)
    dim as long idx, nStart, nEnd
    dim wszText as wstring * MAX_PATH

    if gApp.IsProjectActive then
        nStart = CATINDEX_MAIN
        nEnd = ubound(gConfig.Cat)
    else   
        ' no project active so just "files"
        nStart = CATINDEX_FILES
        nEnd = CATINDEX_FILES
    end if

    for ii as long = nStart to nEnd
        ' The node header description starts with a % in order for the
        ' drawing routine to know that it is a node header and not a
        ' regular file.
        ' e.g.
        '       %    0%1%Files
        '       %    1%1%Main
        '       %    2%1%Resource
        '       %    3%0%Header
        '       %    4%0%Module
        '       %    5%1%Normal
        ' The value after the % is the array index into gConfig.Cat(i).bShow
        ' that allows the mouse routines to toggle on/off displaying of
        ' the files. The value after the second % indicates to the drawing 
        ' routine that files exist ("1") under the node, or ("0") that 
        ' no files exist. This will then dictate what icon to show next
        ' to the line.
        wszText = "%" & AfxStrRSet(str(ii), NODE_INDEX_SIZE) & "%"
        if gConfig.Cat(ii).bShow = false then 
            wszText = wszText & AfxStrRSet("0", NODE_STATE_SIZE) & "%" & gConfig.Cat(ii).wszDescription
        else   
            wszText = wszText & AfxStrRSet("1", NODE_STATE_SIZE) & "%" & gConfig.Cat(ii).wszDescription
        end if

        ' Add the header description to the listbox
        idx = Listbox_AddString( hList, @wszText )

        ' Check to see if these types of files should be loaded into the listbox.
        if gConfig.Cat(ii).bShow = false then continue for
        
        ' Iterate all pDoc in the project/files list. Create an array and then
        ' sort it alphabetically.
        dim as long ub
        dim as DWSTRING wszFileType
        
        erase pDocs
        
        dim pDoc as clsDocument ptr = gApp.pDocList
        do until pDoc = 0
            wszFileType = pDoc->ProjectFiletype
            ' If project not active then ensure file displays under the "File" header.
            if gApp.IsProjectActive = false then wszFileType = CATINDEX_FILES
            if wszFileType = gConfig.Cat(ii).idFileType then
                ub = ubound(pDocs) + 1
                redim preserve pDocs(ub)
                pDocs(ub) = pDoc
            end if
            pDoc = pDoc->pDocNext
        loop

        QuickSortpDocs( pDocs(), lbound(pDocs), ubound(pDocs) )
    
        for i as long = lbound(pDocs) to ubound(pDocs)
            wszText = AfxStrPathName( "NAMEX", pDocs(i)->DiskFilename )
            idx = Listbox_AddString( hList, @wszText )
            ' pDoc item is stored in ListBox ItemData
            ListBox_SetItemData( hList, idx, pDocs(i) )    
            if pDocActive then 
                if pDocs(i) = pDocActive then idxRestore = idx
            end if
        next   
        
    next

    ' Restore the top index so the list displays like it did before being reset
    SendMessage( hList, LB_SETTOPINDEX, nTopIndex, 0 ) 

    ' Ensure that Listbox is now properly sized and then show  the listbox now that 
    ' it is fully populated (only display the listbox if it contains items because 
    ' zero items can produce white background).
    if ListBox_GetCount( hList ) then ShowWindow( hList, SW_SHOW )
    frmExplorer_PositionWindows()

    ' Set current selection to the item that was selected  prior
    ' to us reloading the listbox
    if idxRestore <> -1 then
        ListBox_SetSel( hList, true, idxRestore )
    else
        ' Most likely a header node was collapsed/expanded so attempt to
        ' reposition to saved caret index
        if idxCaret <> -1 then
            ListBox_SetCaretIndex( hList, idxCaret )
            ListBox_SetAnchorIndex( hList, idxCaret )
            ListBox_SetSel( hList, true, idxCaret )
        end if   
    end if
    AfxRedrawWindow(hList)

    ' Determine if the VScroll bar has changed size or is now hidden/shown
    frmPanel_PositionWindows()

    function = 0
end function

    
