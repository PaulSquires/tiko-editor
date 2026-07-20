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


' ========================================================================================
' Calculate the RECT that holds the client coordinates of the scrollbar's vertical thumb
' Returns True if RECT is not empty
' ========================================================================================
function frmSearchSymbolVScroll_calcVThumbRect() as boolean
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMSEARCHSYMBOL)
    if pWindow = 0 then exit function

    ' calculate the vertical scrollbar in client coordinates
    SetRectEmpty( @gSearchSymbolVScroll.rc )
    
    dim as long nTopIndex = SendMessage( gSearchSymbolVScroll.hListBox, LB_GETTOPINDEX, 0, 0 ) 

    dim as RECT rc 
    with gSearchSymbolVScroll
        GetClientRect( gSearchSymbolVScroll.hListBox, @rc )
        .listBoxHeight = rc.bottom - rc.top
        .itemHeight = SendMessage( gSearchSymbolVScroll.hListBox, LB_GETITEMHEIGHT, 0, 0 )
        .numItems = ListBox_GetCount( gSearchSymbolVScroll.hListBox ) 
        .itemsPerPage = .listBoxHeight / .itemHeight
        .thumbHeight = (.itemsPerPage / .numItems) * .listBoxHeight
        GetClientRect( HWND_FRMSEARCHSYMBOL_VSCROLLBAR, @rc )
        .rc.Left = rc.Left
        .rc.top = rc.top + ((nTopIndex / .numItems) * .listBoxHeight)
        .rc.Right = rc.right
        .rc.bottom = .rc.top + .thumbHeight
        if .numItems < .itemsPerPage then 
            SetRectEmpty( @gSearchSymbolVScroll.rc )
            return true
        end if
    end with

    function = 0
end function


' ========================================================================================
' Fastest descending sort for this small, string-bearing udt array
' uses swap (bitwise exchange, no string realloc) instead of "="
' adaptive insertion sort: near-sorted input -> near O(n)
' ========================================================================================
sub frmSearchSymbol_sortSymbolsDescending( byval nCount as integer )
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

    gSymbolsCount = 0
    dim as long nextEl = 0
        
    dim as score_t result
    dim as score_t min_score = 0
    dim as boolean bLoadAll 
    
    ' By default, if empty search string then simply load all filenames
    if wszNeedle = "" then bLoadAll = true

    ' PROCESS ALL FILENAMES (every user file the symbol database knows,
    ' including include files never opened in the editor). Scan filenames are
    ' UPPERCASED - normalize to on-disk casing for display and open-on-select.
    dim arrFiles(any) as DWSTRING
    dim as long nFiles = gSymDb.EnumUserFiles( arrFiles() )
    for i as long = 0 to nFiles - 1
        arrFiles(i) = FilenameOriginalCase( arrFiles(i) )
    next
    if nFiles > 0 then QuickSortFilenames( arrFiles(), 0, nFiles - 1 )
    for f as long = 0 to nFiles - 1
        dim positions(any) as uinteger
        wszFilename = AfxStrPathName( "NAMEX", arrFiles(f) )
        result = fuzzy_match_positions( wszNeedle, wszFilename, positions() )
        if (result > min_score) orelse (bLoadAll = true) then
            if gSymbolsCount + 1 > MAX_SYMBOL_SEARCH then exit for
            gSymbolsCount += 1
            gSymbols(nextEl).result      = result
            gSymbols(nextEl).id          = SearchSymbol.file_symbol
            gSymbols(nextEl).wszCaption  = wszFilename
            gSymbols(nextEl).wszFilename = arrFiles(f)
            gSymbols(nextEl).nLineNumber = -1
            gSymbols(nextEl).isEnum      = false
            gSymbols(nextEl).clrMask     = WSTRING( len(wszFilename), 48 )    ' "0"
            for i as long = lbound(positions) to ubound(positions)
                gSymbols(nextEl).clrMask[ positions(i) ] = 49   '  "1"
            next

            nextEl += 1
        end if
    next


    ' PROCESS ALL TYPES and FUNCTION NAMES
    if gSymbolsCount < MAX_SYMBOL_SEARCH then
        dim rs() as SYMBOLREF
        dim as long nSyms = gSymDb.EnumAllProcsTypes( rs(), true )
        for s as long = 0 to nSyms - 1
            dim as wstring ptr pFile = gSymDb.SymFile( rs(s) )
            if pFile = 0 then continue for

            dim positions(any) as uinteger
            dim as DWSTRING wszName = gSymDb.QualifiedName( rs(s) )
            result = fuzzy_match_positions( wszNeedle, wszName, positions() )

            if (result > min_score) then
                if gSymbolsCount + 1 > MAX_SYMBOL_SEARCH then exit for
                gSymbolsCount += 1

                dim as long nKind = gSymDb.SymKind( rs(s) )
                gSymbols(nextEl).result = result
                if (nKind = FBCP_KIND_SUB) orelse (nKind = FBCP_KIND_FUNCTION) then
                    gSymbols(nextEl).id = SearchSymbol.function_symbol
                else
                    gSymbols(nextEl).id = SearchSymbol.type_symbol
                end if
                gSymbols(nextEl).wszCaption  = wszName
                gSymbols(nextEl).wszFilename = FilenameOriginalCase( *pFile )
                dim as long nLineNum = gSymDb.SymBodyLine( rs(s) )
                if nLineNum = 0 then nLineNum = gSymDb.SymLine( rs(s) )
                gSymbols(nextEl).nLineNumber = iif( nLineNum > 0, nLineNum - 1, 0 )
                gSymbols(nextEl).isEnum      = ( nKind = FBCP_KIND_ENUM )
                gSymbols(nextEl).clrMask     = WSTRING( len(wszName), 48 )  ' "0"
                for i as long = lbound(positions) to ubound(positions)
                    gSymbols(nextEl).clrMask[ positions(i) ] = 49  ' "1"
                next

                nextEl += 1
            end if
        next
    end if


    ' Sort the gSymbols array based on score_t descending
    frmSearchSymbol_sortSymbolsDescending( gSymbolsCount )

    
    dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )

   ' tell the virtual listbox how many rows exist now
    SendMessage( hList, LB_SETCOUNT, gSymbolsCount, 0 )
    
    ShowWindow( hList, iif(gSymbolsCount = 0, SW_HIDE, SW_SHOW) )
    AfxRedrawWindow( HWND_FRMSEARCHSYMBOL )
    
    ' If we symbols in the list then select the first entry if the text is 
    ' a perfect match.
    if gSymbolsCount > 0 then
        if gSymbols(0).result = SCORE_MAX then
            ListBox_SetCurSel( hList, 0 )
        end if
        ' Always redraw regardless of whether an exact match
        AfxRedrawWindow( hList )
    end if    
    frmSearchSymbolVScroll_calcVThumbRect()
    AfxRedrawWindow( HWND_FRMSEARCHSYMBOL_VSCROLLBAR )

    return 0
end function


' ========================================================================================
' Go to the Symbol as selected from the listbox and then close the picker
' ========================================================================================
function frmSearchSymbol_GotoSymbol( byval idx as long ) as long
    
    if (idx < 0) orelse (idx > gSymbolsCount) then return 0

    if len( gSymbols(idx).wszFilename ) then
        if gSymbols(idx).nLineNumber >= 0 then
            OpenSelectedDocument( gSymbols(idx).wszFilename, "", gSymbols(idx).nLineNumber )
        else
            OpenSelectedDocument( gSymbols(idx).wszFilename )
        end if
    end if
    
    SendMessage( HWND_FRMSEARCHSYMBOL, WM_CLOSE, 0, 0 )
    return 0
end function


' ========================================================================================
' frmSearchSymbolVScroll Window procedure
' ========================================================================================
function frmSearchSymbolVScroll_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    static as point prev_pt   ' screen pt.y cursor position

    select case uMsg
        case WM_NCDESTROY
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMSEARCHSYMBOL_VSCROLLBAR = 0
            return 0

        case WM_LBUTTONDOWN
            dim as point pt: GetCursorPos( @pt )
            frmSearchSymbolVScroll_calcVThumbRect()   ' in client coordinates
            dim as RECT rc = gSearchSymbolVScroll.rc ' covert copy to screen coordinates
            MapWindowpoints( HWND_FRMSEARCHSYMBOL_VSCROLLBAR, HWND_DESKTOP, cast(point ptr, @rc), 2)
            if PtInRect( @rc, pt ) then
                prev_pt = pt
                gApp.bDragActive = true
                SetCapture( hwnd )
            else
                ' we have clicked on a PageUp or PageDn
                dim as long nTopIndex = SendMessage( gSearchSymbolVScroll.hListBox, LB_GETTOPINDEX, 0, 0 ) 
                if pt.y < rc.top then
                    nTopIndex = max( nTopIndex - gSearchSymbolVScroll.itemsPerPage, 0 )
                    SendMessage( gSearchSymbolVScroll.hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                    frmSearchSymbolVScroll_calcVThumbRect()   ' in client coordinates
                    AfxRedrawWindow( HWND_FRMSEARCHSYMBOL_VSCROLLBAR )
                    AfxRedrawWindow( gSearchSymbolVScroll.hListBox )
                elseif pt.y > rc.bottom then
                    dim as long nMaxTopIndex = gSearchSymbolVScroll.numItems - gSearchSymbolVScroll.itemsPerPage
                    nTopIndex = min( nTopIndex + gSearchSymbolVScroll.itemsPerPage, nMaxTopIndex )
                    SendMessage( gSearchSymbolVScroll.hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                    frmSearchSymbolVScroll_calcVThumbRect()   ' in client coordinates
                    AfxRedrawWindow( HWND_FRMSEARCHSYMBOL_VSCROLLBAR )
                    AfxRedrawWindow( gSearchSymbolVScroll.hListBox )
                end if
            end if
            
        case WM_MOUSEMOVE
            if gApp.bDragActive then
                dim as point pt: GetCursorPos( @pt )
                if pt.y <> prev_pt.y then 
                    dim as long delta = pt.y - prev_pt.y 
                    dim as RECT rc: GetClientRect( HWND_FRMSEARCHSYMBOL_VSCROLLBAR, @rc )

                    gSearchSymbolVScroll.rc.top = max(0, gSearchSymbolVScroll.rc.top + delta)
                    gSearchSymbolVScroll.rc.top = min(gSearchSymbolVScroll.rc.top, rc.bottom-gSearchSymbolVScroll.thumbHeight)
                    gSearchSymbolVScroll.rc.bottom = gSearchSymbolVScroll.rc.top + gSearchSymbolVScroll.thumbHeight

                    prev_pt = pt
                    
                    dim as long nPrevTopIndex = SendMessage( gSearchSymbolVScroll.hListBox, LB_GETTOPINDEX, 0, 0 ) 
                    dim as long nLastIndex = (gSearchSymbolVScroll.rc.bottom / rc.bottom) * gSearchSymbolVScroll.numItems
                    dim as long nTopIndex = nLastIndex - gSearchSymbolVScroll.itemsPerPage
                    if nTopIndex <> nPrevTopIndex then
                        SendMessage( gSearchSymbolVScroll.hListBox, LB_SETTOPINDEX, nTopIndex, 0 ) 
                        AfxRedrawWindow( gSearchSymbolVScroll.hListBox )
                    end if   
                    AfxRedrawWindow( HWND_FRMSEARCHSYMBOL_VSCROLLBAR )
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
            b.SetBackColor( ghPanel.BackColorScrollBar )
            b.PaintClientRect()
            b.SetBackColor( ghPanel.ForeColorScrollBar )
            b.PaintRect( @gSearchSymbolVScroll.rc )

            b.EndDoubleBuffer()

            return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( HWnd, uMsg, wParam, lParam )

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
                SetTextColor( hdc, ghPanel.ForeColor )
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


        case WM_CHAR
            ' extra safety net, prevents newline insertion
            if _wParam = 13 then return 0
    
        ' Up/Down keypresses in the textbox will move the current selected line
        ' in the picker listbox.
        case WM_KEYDOWN
            if _wParam = VK_TAB then
                ' Simulate Up/Down arrow keys
                if (GetAsyncKeyState(VK_SHIFT) and &H8000) then 
                    _wParam = VK_UP
                else    
                    _wParam = VK_DOWN
                end if
            end if
                    
            dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )        
            select case _wParam
                case VK_UP
                    dim as long cur = SendMessage(hList, LB_GETCURSEL, 0, 0)
                    if cur > 0 then SendMessage(hList, LB_SETCURSEL, cur - 1, 0)
                    AfxRedrawWindow(hList)
                    return 0

                case VK_DOWN
                    dim as long cur = SendMessage(hList, LB_GETCURSEL, 0, 0)
                    if (cur < gSymbolsCount - 1) then SendMessage(hList, LB_SETCURSEL, cur + 1, 0)
                    AfxRedrawWindow(hList)
                    return 0
            end select
            
            
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
                AfxRedrawWindow(hWin)
            end if
                        
            
        case WM_MOUSELEAVE
            AfxRedrawWindow(hWin)
            

        case WM_LBUTTONUP
            dim as RECT rc
            dim as long idx = Listbox_ItemFromPoint( hWin, GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam)) 
            ' The return value contains the index of the nearest item in the LOWORD. The HIWORD is zero 
            ' if the specified point is in the client area of the list box, or one if it is outside the 
            ' client area.
            if hiword(idx) <> 1 then
                frmSearchSymbol_GotoSymbol( idx )
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

        ' We don't do Hot tracking in this listbox because it can be visually
        ' confusing for the user. We only highlight the current selection.
        dim as boolean IsHot = false
        if ListBox_GetSel(lpdis->hwndItem, lpdis->itemID) then IsHot = true

        dim as COLORREF backclr = iif( IsHot, ghPanel.BackColorHot,  ghPanel.BackColor)
        dim as COLORREF foreclr = iif( IsHot, ghPanel.ForeColor,  ghPanel.ForeColor)
        b.SetForeColor( foreclr )
        b.SetBackColor( backclr )

        ' Create our rect that works with the entire line
        dim as RECT rc: SetRect( @rc, 0, 0, b.rcClientWidth, b.rcClientHeight )
        
        dim as RECT rcIcon = rc 
        rcIcon.right = rcIcon.left + pWindow->ScaleX(24)
        
        dim as RECT rcText = rc 
        rcText.left = rcIcon.right + pWindow->ScaleX(4)
        
        ' Paint the entire background
        b.PaintRect( @rc )

        dim as COLORREF icon_foreclr
        dim as COLORREF icon_backclr 

        if lpdis->ItemID > -1 then
            dim as DWSTRING wszIcon
            dim as DWSTRING wszCaption = gSymbols(lpdis->ItemID).wszCaption

            select case gSymbols(lpdis->ItemID).id
                case SearchSymbol.file_symbol
                    icon_foreclr = ghPanel.ForeColorFilenameIcon
                    icon_backclr = iif( IsHot, ghPanel.BackColorHot, ghPanel.BackColorFilenameIcon)
                    wszIcon = wszSearchFile

                case SearchSymbol.function_symbol
                    icon_foreclr = ghPanel.ForeColorFunctionIcon
                    icon_backclr = iif( IsHot, ghPanel.BackColorHot, ghPanel.BackColorFunctionIcon)
                    wszIcon = wszSearchFunction

                case SearchSymbol.type_symbol
                    icon_foreclr = ghPanel.ForeColorTypeIcon
                    icon_backclr = iif( IsHot, ghPanel.BackColorHot, ghPanel.BackColorTypeIcon)
                    wszIcon = iif( gSymbols(lpdis->ItemID).isEnum, wszSearchEnum, wszSearchType )
            end select


            dim as DWSTRING wszChar
            dim as COLORREF clrChar
            dim as long idFont
            dim as RECT rcChar = rcText
            
            dim as long nLen = len(wszCaption)
            
            for i as long = 0 to nLen - 1
                ' Default to normal forecolor and font
                clrChar = ghPanel.ForeColorTextSearch
                idFont  = GUIFONT_10
                
                ' Test mask to see if "1" indicating highlight the letter.
                if gSymbols(lpdis->ItemID).clrMask[i] = 49 then   ' "1"
                    clrChar = ghPanel.ForeColorTextSearchHot
                    idFont  = GUIFONTBOLD_10
                end if

                b.SetFont( ghFont(idFont) )
                wszChar = wchr(wszCaption[i])
                b.PaintChar( wszChar, @rcChar, clrChar )

                ' measure this one character to know how far to advance
                dim as HFONT hOldFont = SelectObject( lpdis->hDC, ghFont(idFont) ) 
                dim as SIZE sz
                GetTextExtentPoint32( lpdis->hDC, wszChar, 1, @sz )
                rcChar.left += sz.cx
                rcChar.right += sz.cx
                SelectObject(lpdis->hDC, hOldFont) 
            next

            ' Now paint the symbol icon
            b.SetForeColor( icon_foreclr )
            b.SetBackColor( icon_backclr )
            b.SetFont( ghFont(ITALICFONT_10) )
            b.PaintRect( @rcIcon )
            b.PaintText( wszIcon, @rcIcon, DT_CENTER )
            
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
        b.SetForeColor( ghPanel.ForeColor )
        b.SetBackColor( ghPanel.BackColor )

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

    dim pWindowPanel as CPanelWindow ptr = New CPanelWindow
        HWND_FRMSEARCHSYMBOL_VSCROLLBAR = _
            pWindowPanel->Create( HWND_FRMSEARCHSYMBOL, @frmSearchSymbolVScroll_WndProc, 0, 0, SCROLLBAR_WIDTH_PANEL, 0 )
        pWindow->ClassStyle = CS_DBLCLKS

    ' we use RichEdit controls because regular Edit controls can not easily have
    ' their text vertically centered.
    gFind.hCueBannerFont = ghFont(GUIFONT_10)

    dim as RECT rc
    dim cf as CHARFORMATW 
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_FACE or CFM_SIZE or CFM_CHARSET or CFM_COLOR
    cf.crTextColor = ghPanel.ForeColorHot
    cf.yHeight = 10 * 20      ' in twips (1/20 pt)
    cf.bCharSet = DEFAULT_CHARSET
    cf.bPitchAndFamily = DEFAULT_PITCH or FF_DONTCARE
    cf.szFaceName = GUIFONT
    
    
    dim as HWND hCtlFind
    hCtlFind = _
        pWindow->AddControl("RICHEDIT", , IDC_FRMSEARCHSYMBOL_TXTFIND, "", _
        0, 0, 0, 0, _
        WS_CHILD or ES_LEFT or ES_AUTOHSCROLL, _
        WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtlFind, @frmSearchSymbol_RichEdit_SubclassProc, IDC_FRMSEARCHSYMBOL_TXTFIND, NULL)
    SendMessage( hCtlFind, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtlFind, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghPanel.BackColorHot) )
    SendMessage( hCtlFind, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE) )

    dim as HWND hCtlList
    hCtlList = _ 
        pWindow->AddControl("LISTBOX", , IDC_FRMSEARCHSYMBOL_LISTBOX, "", 0, 0, 0, 0, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or _
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

    GetClientRect( HWND_FRMSEARCHSYMBOL, @rc )
    
    ' Reposition the find textbox
    nMargin = pWindow->ScaleY(6)
    nHeight = pWindow->ScaleY( 30 )
    nWidth  = (rc.right - rc.left)
    SetWindowPos( hCtlFind, 0, nMargin, nMargin, _
            nWidth - (nMargin * 2), nHeight, SWP_SHOWWINDOW or SWP_NOZORDER )
    RichEditCenterSingleLineText( hCtlFind )
    SendMessage( hCtlFind, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(nMargin, 0))
    
    ' Reposition the ownerdrawn results listbox 
    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_PANEL)
    nTop = (nMargin + nHeight + nMargin)
    nHeight = (rc.bottom - nMargin - nMargin - nTop)
    SetWindowPos( hCtlList, 0, nMargin, nTop, _
            nWidth - nVScrollWidth - (nMargin * 2), nHeight, SWP_SHOWWINDOW or SWP_NOZORDER )
    ' Reposition the vertical scrollbar
    gSearchSymbolVScroll.hListBox = hCtlList
    frmSearchSymbolVScroll_calcVThumbRect()
    SetWindowPos( HWND_FRMSEARCHSYMBOL_VSCROLLBAR, 0, _
        nMargin + AfxGetWindowWidth(gSearchSymbolVScroll.hListBox), nTop, _
        nVScrollWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )
    
    
    SetFocus( hCtlFind )    

    ' Message loop (modal)
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMSEARCHSYMBOL, WM_CLOSE, 0, 0 )
        
        elseif (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_RETURN) then
            dim as HWND hFocus   = GetFocus()
            dim as HWND hFind    = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_TXTFIND )
            if (hFocus = hFind) then 
                dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )        
                dim as long cur = SendMessage( hList, LB_GETCURSEL, 0, 0 )
                frmSearchSymbol_GotoSymbol( cur )   ' this will also call WM_CLOSE
            end if

        end if

        if IsDialogMessage( HWND_FRMSEARCHSYMBOL, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    wend
    
    gPrevent_WM_NCACTIVATE = false

    frmMain_SetFocusToCurrentCodeWindow()

    function = uMsg.wParam
end function

