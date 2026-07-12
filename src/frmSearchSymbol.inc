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

#include once "frmSearchSymbol.bi"

enum SearchSymbol explicit
    file_symbol = 0
    type_symbol
    function_symbol
end enum

type SEARCHSYMBOL_TYPE
    result as score_t
    id     as SearchSymbol
    pDoc   as clsDocument ptr    ' used to get filename / diskfilename
    pData  as DB2_DATA ptr       ' pointer into database for Types / Functions
end type

#define MAX_SYMBOL_SEARCH  100
dim shared gSymbols( MAX_SYMBOL_SEARCH ) as SEARCHSYMBOL_TYPE
dim shared gSymbolsCount as integer = -1  ' actual # of populated entries (<= MAX_SYMBOL_SEARCH+1)


' ========================================================================================
' Fastest descending sort for this small, string-bearing udt array
' uses swap (bitwise exchange, no string realloc) instead of "="
' adaptive insertion sort: near-sorted input -> near O(n)
' ========================================================================================
sub sortSymbolsDescending( byval nCount as integer )
    dim as integer j
    for i as integer = 1 to nCount - 1
        j = i
        do while (j > 0) andalso (gSymbols(j).result > gSymbols(j - 1).result)
            swap gSymbols(j), gSymbols(j - 1)
            j -= 1
        loop
    next i
end sub

' ========================================================================================
' Process the find text, fuzzy score, and load the results listbox.
' ========================================================================================
function frmSearchSymbol_DoSearch( byval hwndCtl as HWND ) as long
    dim as DWSTRING wszFilename
    dim as DWSTRING wszNeedle = AfxGetWindowText( hwndCtl )

    gSymbolsCount = -1
    
    dim as score_t result
    dim as score_t min_score = 0
    dim as boolean bLoadAll 
    
    ' By default, if empty search string then simply load all filenames
    if wszNeedle = "" then bLoadAll = true

    ' PROCESS ALL FILENAMES
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        wszFilename = AfxStrPathName( "NAMEX", pDoc->DiskFilename )
        result = fuzzy_match( wszNeedle, wszFilename )
        if (result > min_score) orelse (bLoadAll = true) then
            if gSymbolsCount + 1 > MAX_SYMBOL_SEARCH then exit do 
            gSymbolsCount += 1
            gSymbols(gSymbolsCount).result = result
            gSymbols(gSymbolsCount).id     = SearchSymbol.file_symbol
            gSymbols(gSymbolsCount).pDoc   = pDoc 
            gSymbols(gSymbolsCount).pData  = null 
        end if
        pDoc = pDoc->pDocNext
    loop

    
    ' PROCESS ALL TYPES and FUNCTION NAMES
    dim pData as DB2_DATA ptr
    gdb2.dbRewind()
    do 
        pData = gdb2.dbGetnext
        if pData = 0 then exit do
        if pData->deleted then continue do
        
        if pData->nFileType <> DB2_FILETYPE_USERCODE then continue do
        if len(pData->fileName) = 0 then continue do  ' bypass any predefined data
        
        if (pData->id = DB2_TYPE) orelse _
            (pData->id = DB2_FUNCTION) then

            result = fuzzy_match( wszNeedle, pData->ElementName )
                
            if (result > min_score) then
                if gSymbolsCount + 1 > MAX_SYMBOL_SEARCH then exit do 
                gSymbolsCount += 1
                
                gSymbols(gSymbolsCount).result = result
                if pData->id = DB2_FUNCTION then
                    gSymbols(gSymbolsCount).id = SearchSymbol.function_symbol
                elseif pData->id = DB2_TYPE then
                    gSymbols(gSymbolsCount).id = SearchSymbol.type_symbol
                end if    
                gSymbols(gSymbolsCount).pDoc   = null 
                gSymbols(gSymbolsCount).pData  = pData 
            end if
            
        end if
        
    loop


    ' Sort the gSymbols array based on score_t descending
    sortSymbolsDescending( gSymbolsCount )

'cls
'print "gSymbolsCount: "; gSymbolsCount

'for i as long = 0 to gSymbolsCount - 1
'    pDoc = gSymbols(i).pDoc
'    pData = gSymbols(i).pData
    
'    print "["; i; "] "; format( gSymbols(i).result, "0.000" ); " ";
'    select case gSymbols(i).id
'        case SearchSymbol.file_symbol
'            print "FILE: ";
'            if pDoc then print AfxStrPathName( "NAMEX", pDoc->DiskFilename );
'        case SearchSymbol.function_symbol
'            print "FUNCTION: ";
'            if pData then 
'                print pData->ElementName;
'                if len(pData->GetSet) then
'                    print " "; pData->GetSet;
'                end if
'            end if    
'        case SearchSymbol.type_symbol
'            if pData then
'                if pData->isENUM then
'                    print "ENUM: ";
'                else
'                    print "TYPE: ";
'                end if    
'                print pData->ElementName;
'            end if
'    end select
'    print
'next
    
    dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )

    ' Hide the listbox while it is loading so that we don't get the unpainted
    ' white background from the empty listbox
   ' ShowWindow( hList, SW_HIDE )

   ' tell the virtual listbox how many rows exist now
    SendMessage( hList, LB_SETCOUNT, gSymbolsCount, 0 )
    AfxRedrawWindow( hList )
    
   ' if gSymbolsCount > -1 then ShowWindow( hList, SW_SHOW )

    return 0
end function


' ========================================================================================
' frmSearchSymbol_RichEdit_SubclassProc Window procedure
' ========================================================================================
Function frmSearchSymbol_RichEdit_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)

    select case uMsg
                            
        case WM_PRINTCLIENT, WM_PAINT
            dim as long textLength = AfxGetWindowTextLength(hWin)
            if (textLength = 0 ) then 
                ' Get the needed DC with DCX_INTERSECTUPDATE before the EDIT
                ' control's WM_PAINT handler calls BeginPaint/EndPaint, which
                ' validates the update rect and would otherwise lead to drawing
                ' nothing later because the region is empty. Also, grab it from
                ' the cache so we don't mess with the EDIT's DC.
                dim as HDC hdc = _
                    iif(uMsg = WM_PRINTCLIENT, GetDC(hWin), _
                        GetDCEx(hWin, 0, DCX_INTERSECTUPDATE or DCX_CACHE or DCX_CLIPCHILDREN or DCX_CLIPSIBLINGS) )

                ' Call the EDIT control so that the caret is properly handled,
                ' no caret litter left on the screen after tabbing away.
                dim as LRESULT result = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

                SaveDC(hdc)
                
                ' Get the font and margin so the cue banner text has a
                ' consistent appearance and placement with existing text.
                dim as RECT editRect: SendMessage( hWin, EM_GETRECT, 0, cast(LPARAM, @editRect) )

                ' Ideally we would call Edit_GetCueBannerText, but since that message
                ' returns nothing when ES_MULTILINE, use a window property instead.
                dim cueBannerText as wstring * 250
                cueBannerText = L(37,"Search Symbol") & "..."
                SelectObject( hdc, gFind.hCueBannerFont )
                SetTextColor( hdc, ghTopTabs.ForeColor )
                SetBkMode(hdc, TRANSPARENT)
                dim as long wsStyle = DT_TOP or DT_LEFT or DT_NOPREFIX or DT_NOCLIP or DT_SINGLELINE 
                DrawText(hdc, cueBannerText, -1, @editRect, wsStyle)
                RestoreDC( hdc, -1 )
                ReleaseDC( hWin, hdc )

                ' Return the EDIT's result (could probably safely just return zero here,
                ' but seems safer to relay whatever value came from the edit).
                return result
            end if
        
        case WM_CONTEXTMENU 
            ' Create the right click popup menu
            dim as DWSTRING wszText = RichEdit_GetSelText( hWin )
            dim hPopUpMenu As HMENU = CreatePopupMenu()
            if len(wszText) then 
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_CUT, wstr("Cut") )
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_COPY, wstr("Copy") )
            end if
            if RichEdit_CanPaste( hWin, 0 ) then
                if len(wszText) then 
                    AppendMenu( hPopUpMenu, MF_SEPARATOR, 0, "" )
                end if   
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_PASTE, wstr("Paste") )
            end if   

            dim as long nResult
            nResult = TrackPopupMenu( hPopUpMenu, TPM_RETURNCMD or TPM_NONOTIFY, _
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMMAIN_TOPTABSFIND, 0 ) 
            select case nResult
                case IDM_CUT:   SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:  SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE: SendMessage( hWin, WM_PASTE, 0, 0 )
            end select
            DestroyMenu hPopUpMenu

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmSearchSymbol_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function


' ========================================================================================
' frmShowSymbolListBox_SubclassProc
' ========================================================================================
function frmSearchSymbolListBox_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // Pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMSEARCHSYMBOL)
    static as long accumDelta
    
    ' keep track of last index we were over so that we only issue a 
    ' repaint if the cursor has moved off of the line
    static as long nLastIdx = -1
        
    select case uMsg

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
'                frmPanel_PositionWindows
            elseif accumDelta <= -120 then  ' scroll down 3 lines
                nTopIndex = nTopIndex + 3
                SendMessage( hWin, LB_SETTOPINDEX, nTopIndex, 0 ) 
                accumDelta = 0
'                frmPanel_PositionWindows
            end if

        Case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' WM_MOUSELEAVE events
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_LEAVE
            tme.hwndTrack = hWin
            TrackMouseEvent(@tme) 
            
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
                        
            
        case WM_MOUSELEAVE
            nLastIdx = -1
            AfxRedrawWindow(hWin)
            

        case WM_LBUTTONUP
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
'            if hiword(idx) <> 1 then
'                dim as DWSTRING wszCaption = AfxGetListBoxText( hWin, idx )
'                dim as clsDocument ptr pDoc = cast(clsDocument ptr, ListBox_GetItemData( hWin, idx ))
'            end if

                
        case WM_ERASEBKGND
            ' if the number of lines in the listbox maybe less than the number per page then 
            ' calculate from last item to bottom of listbox, otherwise calculate based on
            ' the mod of the lineheight to listbox height so we can color the partial line
            ' that won't be displayed at the bottom of the list.
            dim as RECT rc = GetListBoxEmptyClientArea( hWin )
            if rc.top < rc.bottom then
                dim as HDC _hDC = cast(HDC, _wParam)
                PaintRect( _hDC, @rc, ghTopTabs.BackColor )
            end if
            return true
            
            
        Case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmSearchSymbolListBox_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' Process WM_MEASUREITEM message for window/dialog: frmSearchSymbol
' ========================================================================================
function frmSearchSymbol_OnMeasureItem( _
            byval hwnd as HWND, _
            byval lpmis as MEASUREITEMSTRUCT ptr _
            ) as long
    ' Set the height of the list box items. 
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    lpmis->itemHeight = pWindow->ScaleY(24)
        
    function = 0
end function


' ========================================================================================
' Process WM_DRAWITEM message for window/dialog: frmSearchSymbol
' ========================================================================================
function frmSearchSymbol_OnDrawItem( _
            byval hwnd as HWND, _
            byval lpdis as const DRAWITEMSTRUCT ptr _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMSEARCHSYMBOL)
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
 '       if isMouseOverWindow( HWND_FRMSEARCHSYMBOL_VSCROLLBAR ) then IsHot = false
        if ListBox_GetSel(lpdis->hwndItem, lpdis->itemID) then IsHot = true

        dim as COLORREF backclr = iif( IsHot, ghPanel.BackColorHot,  ghPanel.BackColor)
        dim as COLORREF foreclr = iif( IsHot, ghPanel.ForeColorHot,  ghPanel.ForeColor)
        b.SetForeColors(foreclr, foreclr)
        b.SetBackColors(backclr, backclr)


        ' Create our rect that works with the entire line
        dim as RECT rc: SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
        
        dim as RECT rcIcon = rc 
        rcIcon.right = rcIcon.left + pWindow->ScaleX(24)
        
        dim as RECT rcText = rc 
        rcText.left = rcIcon.right + pWindow->ScaleX(4)
        
        ' Paint the entire background
        b.PaintRect( @rc )

        if lpdis->ItemID > -1 then
            dim as DWSTRING wszIcon
            dim as DWSTRING wszCaption

            dim as clsDocument ptr pDoc = gSymbols(lpdis->ItemID).pDoc
            dim as DB2_DATA ptr pData = gSymbols(lpdis->ItemID).pData

            select case gSymbols(lpdis->ItemID).id
                case SearchSymbol.file_symbol
                    wszIcon = wszSearchFile
                    if pDoc then wszCaption = AfxStrPathName( "NAMEX", pDoc->DiskFilename )
                case SearchSymbol.function_symbol
                    wszIcon = wszSearchFunction
                    if pData then 
                        wszCaption = pData->ElementName
                        if len(pData->GetSet) then
                            wszCaption += " " & pData->GetSet
                        end if
                    end if    
                case SearchSymbol.type_symbol
                    if pData then
                        wszIcon = iif( pData->isENUM, wszSearchEnum, wszSearchType )
                        wszCaption = pData->ElementName
                    end if
            end select

            b.SetFont( ITALICFONT_10 )
            b.PaintText( wszIcon, @rcIcon, DT_CENTER )
            
            b.SetFont( GUIFONT_10 )
            b.PaintText( wszCaption, @rcText, DT_LEFT )
        end if
            
        b.EndDoubleBuffer()

    end if   

    function = true
    
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmSearchSymbol
' ========================================================================================
function frmSearchSymbol_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify As UINT _
            ) as LRESULT

    select case id

        case IDC_FRMSEARCHSYMBOL_TXTFIND
            if (codeNotify = EN_CHANGE) then
                frmSearchSymbol_DoSearch( hwndCtl )

            elseif (codeNotify = EN_SETFOCUS) then
                frmSearchSymbol_DoSearch( hwndCtl )
            end if

    end select

    function = 0
end function


' ========================================================================================
' frmSearchSymbol Window procedure
' ========================================================================================
function frmSearchSymbol_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_COMMAND,     frmSearchSymbol_OnCommand)
        HANDLE_MSG (hwnd, WM_MEASUREITEM, frmSearchSymbol_OnMeasureItem)
        HANDLE_MSG (hwnd, WM_DRAWITEM,    frmSearchSymbol_OnDrawItem)

    case WM_CLOSE
        DestroyWindow hwnd
        return 0
        
    case WM_DESTROY
        PostQuitMessage(0)
        return 0
        
    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMSEARCHSYMBOL = 0
        return 0

    case WM_ACTIVATE:
        if (loword(wParam) = WA_INACTIVE) then
            dim as HWND hwndNewActive = cast( HWND, lParam)
            ' If activation went to anything other than the owner itself,
            ' the user clicked away — dismiss.
            if (hwndNewActive <> hwnd) then
                SendMessage( hwnd, WM_CLOSE, 0, 0 )
            end if    
        end if
        
    case WM_ERASEBKGND
        return true

    case WM_PAINT
        dim as clsDoubleBuffer b
        
        b.BeginDoubleBuffer(hwnd)

        ' Paint the tab info area 
        b.SetForeColors( ghTopTabs.ForeColor, ghTopTabs.ForeColor )
        b.SetBackColors( ghTopTabs.BackColor, ghTopTabs.BackColor )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Lastly, border the entire form.
        dim as RECT rc = b.rcClient
        b.SetPenColor( ghEditor.Divider )
        b.PaintBorderRect( @rc )
        
        b.EndDoubleBuffer()
        
        return 0

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmSearchSymbol_Show
' ========================================================================================
function frmSearchSymbol_Show( byval hwndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow
    
    ' Prevent the main form's NC_ACTIVATE from painting it's caption bar as inactive.
    gPrevent_WM_NCACTIVATE = true

    HWND_FRMSEARCHSYMBOL = _
    pWindow->Create( hwndParent, "", @frmSearchSymbol_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, WS_EX_TOOLWINDOW or WS_EX_TOPMOST )

    ' we use RichEdit controls because regular Edit controls can not easily have
    ' their text vertically centered.
    gFind.hCueBannerFont = ghFont(GUIFONT_10)

    dim as RECT rc
    dim cf as CHARFORMATW 
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_FACE or CFM_SIZE or CFM_CHARSET or CFM_COLOR
    cf.crTextColor = ghTopTabs.ForeColorHot
    cf.yHeight = 10 * 20      ' in twips (1/20 pt)
    cf.bCharSet = DEFAULT_CHARSET
    cf.bPitchAndFamily = DEFAULT_PITCH or FF_DONTCARE
    cf.szFaceName = GUIFONT
    
    
    dim as HWND hCtlFind
    hCtlFind = _
        pWindow->AddControl("RICHEDIT", , IDC_FRMSEARCHSYMBOL_TXTFIND, "", _
        0, 0, 0, 0, _
        WS_CHILD or WS_TABSTOP or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtlFind, @frmSearchSymbol_RichEdit_SubclassProc, IDC_FRMSEARCHSYMBOL_TXTFIND, NULL)
    SendMessage( hCtlFind, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtlFind, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghTopTabs.BackColorHot) )
    SendMessage( hCtlFind, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )

    dim as HWND hCtlList
    hCtlList = _ 
        pWindow->AddControl("LISTBOX", , IDC_FRMSEARCHSYMBOL_LISTBOX, "", 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or WS_TABSTOP or _
        LBS_NOINTEGRALHEIGHT or LBS_OWNERDRAWFIXED or LBS_NOTIFY or LBS_NODATA, _
        WS_EX_LEFT or WS_EX_RIGHTSCROLLBAR)
        pWindow->SetControlSubclass(hCtlList, @frmSearchSymbolListBox_SubclassProc, IDC_FRMSEARCHSYMBOL_LISTBOX, @pWindow)


    ' Get the client area of the main window and subtract width of any visible side panel. We
    ' use this width in order to center the popup fuzzy finder.
    GetWindowRect( HWND_FRMMAIN, @rc )
    if IsWindowVisible(HWND_FRMPANEL) then
        dim as long nWidthPanel = AfxGetWindowWidth(HWND_FRMPANEL)
        if gConfig.ExplorerPositionRight then 
            rc.right -= nWidthPanel
        else
            rc.left += nWidthPanel    
        end if
    end if  
    
    ' If the top tab control is active (meaning that tabcount exists) then reposition the
    ' fuzzy picker to just under the tabs info panel.
    if IsWindowVisible( HWND_FRMMAIN_TOPTABSINFO ) then
        dim as RECT rcInfo: GetWindowRect(HWND_FRMMAIN_TOPTABSINFO, @rcInfo)
        rc.top = rcInfo.bottom
    end if
    
    ' Calculate the position for this popup fuzzy finder
    dim as long nMargin
    dim as long nWidth   = pWindow->ScaleX( 550 )
    dim as long nHeight  = pWindow->ScaleY( 400 )
    dim as long nTop     = rc.top
    dim as long nLeft    = rc.left + ( (rc.right - rc.left - nWidth) / 2 )
    

    ' Position the Fuzzy popup form into place
    SetWindowPos( HWND_FRMSEARCHSYMBOL, HWND_TOPMOST, nLeft, nTop, nWidth, nHeight, SWP_SHOWWINDOW ) 

    ' Reposition the find textbox
    nMargin = pWindow->ScaleY(6)
    nHeight = pWindow->ScaleY( 30 )
    SetWindowPos( hCtlFind, 0, nMargin, nMargin, _
            nWidth - (nMargin * 2), nHeight, SWP_SHOWWINDOW or SWP_NOZORDER )
    RichEditCenterSingleLineText( hCtlFind )
    SendMessage( hCtlFind, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(nMargin, 0))
    
    ' Reposition the ownerdrawn results listbox 
    nTop = (nMargin + nHeight)
    nHeight = (rc.bottom - nMargin - nTop)
    SetWindowPos( hCtlList, 0, nMargin, nTop, _
            nWidth - (nMargin * 2), nHeight, SWP_SHOWWINDOW or SWP_NOZORDER )
    
    
    SetFocus( hCtlFind )    

    ' Message loop (modal)
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMSEARCHSYMBOL, WM_CLOSE, 0, 0 )
        end if
        if IsDialogMessage( HWND_FRMSEARCHSYMBOL, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend
    
    gPrevent_WM_NCACTIVATE = false
    
    function = uMsg.wParam
end function
