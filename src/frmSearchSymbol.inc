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

    ShowWindow( hList, SW_HIDE )
    for i as integer = 0 to gSymbolsCount - 1
        CListBox_AddString( hList, gSymbols(i).wszCaption, i )
    next
    ShowWindow( hList, SW_SHOW )
    
    ' If we have symbols in the list then select the first entry if the text is 
    ' a perfect match.
    if gSymbolsCount > 0 then
        if gSymbols(0).result = SCORE_MAX then
            CListBox_SetCurSel( hList, 0 )
        end if
    end if    

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
' CTextBox callbacks for the find textbox. The control owns the cue banner, colors,
' vertical centering, single-line enforcement and the Cut/Copy/Paste context menu that
' the old RichEdit subclass hand-rolled here.
' ========================================================================================
sub frmSearchSymbol_TextBoxChange( byval hTextBox as HWND )
    ' User edits only; the control never fires this for programmatic changes.
    frmSearchSymbol_DoSearch( hTextBox )
end sub

sub frmSearchSymbol_TextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    ' Populate the results list when the picker opens (was the EN_SETFOCUS path).
    if bGotFocus then frmSearchSymbol_DoSearch( hTextBox )
end sub

function frmSearchSymbol_TextBoxMessage( byval m as CTEXTBOX_MESSAGEINFO ptr ) as boolean
    ' Up/Down keypresses in the textbox move the current selected line in the picker
    ' listbox. Veto (return true) so the RichEdit caret does not also react.
    ' (VK_TAB does the same but is intercepted in the modal message loop below --
    ' IsDialogMessage would consume it before it ever reached this callback.)
    if m->uMsg = WM_KEYDOWN then
        dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )
        select case m->wParam
            case VK_UP
                dim as integer cur = CListBox_GetCurSel(hList)
                if cur > 0 then CListBox_SetCurSel( hList, cur - 1 )
                return true

            case VK_DOWN
                dim as integer cur = CListBox_GetCurSel(hList)
                if (cur < gSymbolsCount - 1) then CListBox_SetCurSel( hList, cur + 1 )
                return true
        end select
    end if
    return false
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

    ' The find textbox notifies through its CTextBox callbacks (change/focus), not
    ' through WM_COMMAND EN_ codes anymore.
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
        HANDLE_MSG (hwnd, WM_COMMAND, frmSearchSymbol_OnCommand)

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
' frmSearchSymbol_PaintCallback
' ========================================================================================
sub frmSearchSymbol_PaintCallback( byval p as CLISTBOX_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 

    ' We don't do Hot tracking in this listbox because it can be visually
    ' confusing for the user. We only highlight the current selection.
    dim as COLORREF backclr = iif( p->IsSelected, ghPanel.BackColorHot,  ghPanel.BackColor)
    dim as COLORREF foreclr = iif( p->IsSelected, ghPanel.ForeColor,  ghPanel.ForeColor)
    p->b->SetColors( foreclr, backclr )

    ' Create our rect that works with the entire line
    dim as RECT rc: SetRect( @rc, 0, 0, p->b->rcClientWidth, p->b->rcClientHeight )
    
    dim as RECT rcIcon = rc 
    rcIcon.right = rcIcon.left + pWindow->ScaleX(24)
    
    dim as RECT rcText = rc 
    rcText.left = rcIcon.right + pWindow->ScaleX(4)
        
    ' Paint the entire background
    p->b->PaintRect( @rc )

    dim as COLORREF icon_foreclr
    dim as COLORREF icon_backclr 

    if p->ItemID > -1 then
        dim as DWSTRING wszIcon
        dim as DWSTRING wszCaption = gSymbols(p->ItemID).wszCaption

        select case gSymbols(p->ItemID).id
            case SearchSymbol.file_symbol
                icon_foreclr = ghPanel.ForeColorFilenameIcon
                icon_backclr = iif( p->IsSelected, ghPanel.BackColorHot, ghPanel.BackColorFilenameIcon)
                wszIcon = wszSearchFile

            case SearchSymbol.function_symbol
                icon_foreclr = ghPanel.ForeColorFunctionIcon
                icon_backclr = iif( p->IsSelected, ghPanel.BackColorHot, ghPanel.BackColorFunctionIcon)
                wszIcon = wszSearchFunction

            case SearchSymbol.type_symbol
                icon_foreclr = ghPanel.ForeColorTypeIcon
                icon_backclr = iif( p->IsSelected, ghPanel.BackColorHot, ghPanel.BackColorTypeIcon)
                wszIcon = iif( gSymbols(p->ItemID).isEnum, wszSearchEnum, wszSearchType )
        end select


        dim as DWSTRING wszChar
        dim as COLORREF clrChar
        dim as long idFont
        dim as RECT rcChar = rcText
        
        dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )        
        dim as HDC hDC = GetDC(hList)

        dim as long nLen = len(wszCaption)
        
        for i as integer = 0 to nLen - 1
            ' Default to normal forecolor and font
            clrChar = ghPanel.ForeColorTextSearch
            idFont  = GUIFONT_10
            
            ' Test mask to see if "1" indicating highlight the letter.
            if gSymbols(p->ItemID).clrMask[i] = 49 then   ' "1"
                clrChar = ghPanel.ForeColorTextSearchHot
                idFont  = GUIFONTBOLD_10
            end if

            p->b->SetFont( ghFont(idFont) )
            wszChar = wchr(wszCaption[i])
            p->b->PaintChar( wszChar, @rcChar, clrChar )

            ' measure this one character to know how far to advance
            dim as HFONT hOldFont = SelectObject( hDC, ghFont(idFont) ) 
            dim as SIZE sz
            GetTextExtentPoint32( hDC, wszChar, 1, @sz )
            rcChar.left += sz.cx
            rcChar.right += sz.cx
            SelectObject(hDC, hOldFont) 
        next

        ReleaseDC( hList, hDC )

        ' Now paint the symbol icon
        p->b->SetForeColor( icon_foreclr )
        p->b->SetBackColor( icon_backclr )
        p->b->SetFont( ghFont(ITALICFONT_10) )
        p->b->PaintRect( @rcIcon )
        p->b->PaintText( wszIcon, @rcIcon, DT_CENTER )
        
    end if

end sub


' ========================================================================================
' frmSearchSymbol_MessageCallback
' ========================================================================================
function frmSearchSymbol_MessageCallback( byval m as CLISTBOX_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_LBUTTONUP
            frmSearchSymbol_GotoSymbol( m->idx )
    
    end select

    ' Return false to allow to fall to the default windows handler
    return false
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


    ' The find textbox is the reusable CTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as RECT rc
    dim as HWND hCtlFind = CTextBox_Create( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_TXTFIND )
    CTextBox_SetFont( hCtlFind, ghFont(GUIFONT_10) )
    CTextBox_SetForeColor( hCtlFind, ghPanel.ForeColorHot )
    CTextBox_SetBackColor( hCtlFind, ghPanel.BackColorHot )
    CTextBox_SetBorderWidth( hCtlFind, 0 )                       ' flat, as before
    CTextBox_SetCueBannerText( hCtlFind, L(37,"Search Symbol") & "..." )
    CTextBox_SetCueBannerColor( hCtlFind, ghPanel.ForeColor )
    CTextBox_SetChangeCallback( hCtlFind, @frmSearchSymbol_TextBoxChange )
    CTextBox_SetFocusCallback( hCtlFind, @frmSearchSymbol_TextBoxFocus )
    CTextBox_SetMessageCallback( hCtlFind, @frmSearchSymbol_TextBoxMessage )


    dim as HWND hCtrlList = CListBox_Create( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )
    CListBox_SetPaintCallback( hCtrlList, @frmSearchSymbol_PaintCallback )
    CListBox_SetMessageCallback( hCtrlList, @frmSearchSymbol_MessageCallback )
    CListBox_PreventDoubleClick( hCtrlList )
    CListBox_SetBackColor( hCtrlList, ghPanel.BackColor )
    CListBox_SetRowHeight( hCtrlList, 24 )
    CListBox_SetFont( hCtrlList, ghFont(GUIFONT_10) )
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    CListBox_SetScrollBarWidth( hCtrlList, SCROLLBAR_WIDTH_PANEL )
    CListBox_SetScrollBarColors( hCtrlList, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )


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
    ' Vertical centering is the control's job (it re-derives on every size/font
    ' change); only the margins are ours to specify.
    CTextBox_SetMargins( hCtlFind, nMargin, 0 )

    
    ' Reposition the ownerdrawn results listbox 
    nTop = (nMargin + nHeight + nMargin)
    nHeight = (rc.bottom - nMargin - nTop)
    SetWindowPos( hCtrlList, 0, nMargin, nTop, _
            nWidth - (nMargin * 2), nHeight, SWP_SHOWWINDOW or SWP_NOZORDER )

    
    SetFocus( hCtlFind )    

    ' Message loop (modal)
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMSEARCHSYMBOL, WM_CLOSE, 0, 0 )
        
        elseif (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_RETURN) then
            dim as HWND hFocus   = GetFocus()
            dim as HWND hFind    = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_TXTFIND )
            ' Keyboard focus sits on the CTextBox's RichEdit child, not the control.
            if (hFocus = CTextBox_GetRichEditHandle(hFind)) then
                dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )
                dim as integer cur = CListBox_GetCurSel( hList )
                frmSearchSymbol_GotoSymbol( cur )   ' this will also call WM_CLOSE
            end if

        elseif (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_TAB) then
            ' Tab (Shift+Tab = up) moves the list selection, same as the arrow keys.
            ' Intercepted HERE because IsDialogMessage below would otherwise consume
            ' Tab for focus navigation -- the CTextBox's RichEdit child is a real
            ' WS_TABSTOP, unlike the bare RichEdit this form used to own.
            dim as HWND hList = GetDlgItem( HWND_FRMSEARCHSYMBOL, IDC_FRMSEARCHSYMBOL_LISTBOX )
            dim as integer cur = CListBox_GetCurSel( hList )
            if (GetAsyncKeyState(VK_SHIFT) and &H8000) then
                if cur > 0 then CListBox_SetCurSel( hList, cur - 1 )
            else
                if cur < gSymbolsCount - 1 then CListBox_SetCurSel( hList, cur + 1 )
            end if
            continue while   ' fully handled; don't let IsDialogMessage move focus

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

