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

#include once "frmOutput.bi"


' ========================================================================================
' Clear data from all of the controls in the frmOutput windows. This is needed
' when Projects are loaded and closed.
' ========================================================================================
function frmOutput_ResetAllControls() as long 
    frmListView_DeleteAllItems( HWND_FRMOUTPUT_LVRESULTS )
    AfxSetWindowText( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), "" )
    frmListView_DeleteAllItems( HWND_FRMOUTPUT_LVSEARCH )
    frmListView_DeleteAllItems( HWND_FRMOUTPUT_LVTODO )
    AfxSetWindowText( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), "" )
    function = 0
end function


' ========================================================================================
' Ensure that the correct notes are shown
' ========================================================================================
function frmOutput_ShowNotes() as long 

    dim hCtl as HWND = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)

    if gApp.IsProjectActive then
        AfxSetWindowText(hCtl, gApp.ProjectNotes)
    else   
        AfxSetWindowText(hCtl, gApp.NonProjectNotes)
    end if

    function = 0
end function


' ========================================================================================
' Update the TODO listview
' ========================================================================================
function frmOutput_UpdateToDoListview() as long
    if gApp.IsProjectLoading then exit function

    dim as hwnd hLV = HWND_FRMOUTPUT_LVTODO
    frmListView_DeleteAllItems( hLV )

    dim as DWSTRING wszText

    ' The store is fed by the scan worker's line scan of each buffer-scanned
    ' document (see clsSymbolDb.bi). Line numbers are 1-based, which is what
    ' the click-through (SetDocumentErrorPosition) expects in this column.
    for i as long = 0 to gTodoCount - 1
        wszText = _
            "" & chr(9) & _
            ltrim(wstr(gTodoItems(i).nLineNum)) & chr(9) & _
            ltrim(gTodoItems(i).wszFile) & chr(9) & _
            ltrim(gTodoItems(i).wszText)
        frmListView_AddString( hLV, wszText )
    next

    frmOutput_PositionWindows

    function = 0
end function

' ========================================================================================
' Update the SEARCH listview
' ========================================================================================
function frmOutput_UpdateSearchListview( byref wszResultFile as wstring ) as long 
    dim hLV as HWND = HWND_FRMOUTPUT_LVSEARCH

    frmListView_DeleteAllItems( hLV )

    if AfxFileExists(wszResultFile) = 0 then exit function
    
    dim as DWSTRING wst
    dim as DWSTRING wszText

    dim pStream as CTextStream
    if pStream.OpenUnicode(wszResultFile) <> S_OK then exit function
    
    do until pStream.EOS
        wst = pStream.ReadLine
        
        wst = trim(wst)
        if len(wst) = 0 then continue do

        dim as DWSTRING wszFilename, wszLineNum, wszDescription
        dim as long f1, f2
        
        ' Search for the 2nd semicolon
        f1 = instr( 3, wst, ":" )
        if f1 then f2 = instr( f1 + 1, wst, ":" )

        if f1 then wszFilename = left(wst, f1 - 1)
        if f2 > f1 then wszLinenum = mid(wst, f1 + 1, f2 - f1 - 1)
        if f2 then wszDescription = rtrim(mid(wst, f2 + 1))

        wszText = "" & chr(9) & wszLineNum & chr(9) & wszFilename & chr(9) & wszDescription
        frmListView_AddString( hLV, wszText )
    loop
    pStream.Close
    
    AfxDeleteFile( wszResultFile )
    
    ' Show the search results
    gOutputTabsCurSel = 2
    frmOutput_RestorePanel() 
    frmOutput_PositionWindows

    function = 0
end function
            

' ========================================================================================
' Show/Hide correct child controls
' ========================================================================================
function frmOutput_ShowHideOutputControls( byval hwnd as HWND ) as LRESULT
    dim as HWND hTextBox
    dim as HWND hLV
    dim as HWND hCtrl
    
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then exit function

    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TABS), SW_SHOW )
    AfxRedrawWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TABS) )
    
    ' By default, hide all controls
    ShowWindow( HWND_FRMOUTPUT_LVRESULTS, SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_LVSEARCH, SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_LVTODO, SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE), SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTNOTES), SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_VSCROLL, SW_HIDE )
    
    select case gOutputTabsCurSel
        case 0    ' compiler results
            hLV = HWND_FRMOUTPUT_LVRESULTS
            hCtrl = hLV

        case 1    ' compiler log file
            hTextBox = GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE)
            hCtrl = hTextBox
            dim as DWSTRING wszText = ltrim(AfxGetWindowText(hTextBox))
            if wszText = "" then
               AfxSetWindowText(hTextBox, "No compiler log file exists.")
            end if   
            dim as RECT rc: GetClientRect( hTextBox, @rc )
            rc.left = rc.left + pWindow->ScaleX(20)
            SendMessage( hTextBox, EM_SETRECT, 0, cast(LPARAM, @rc) )
            
        case 2    ' search results
            hLV = HWND_FRMOUTPUT_LVSEARCH
            hCtrl = hLV

        case 3    ' TODO list
            hLV = HWND_FRMOUTPUT_LVTODO
            hCtrl = hLV

        case 4    ' Notes
            hTextBox = GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTNOTES)
            hCtrl = hTextBox
            dim as RECT rc: GetClientRect( hTextBox, @rc )
            rc.left = rc.left + pWindow->ScaleX(20)
            SendMessage( hTextBox, EM_SETRECT, 0, cast(LPARAM, @rc) )
            SetFocus( hTextBox )
    
    end select
    
    if hLV then 
        ' Only display the owner drawn listview if it has items otherwise the wm_drawitem will
        ' not fire and the whole area will display as white.
        dim as long nShow = iif( frmListView_GetCount( hLV ) = 0, SW_HIDE, SW_SHOW )
        ShowWindow( hLV, nShow )
        frmListView_PositionWindows( hLV )
    end if

    if hTextBox then
        SetWindowPos( hCtrl, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_SHOWWINDOW )
        frmOutputVScroll_calcVThumbRect( hTextBox )    
        ShowWindow( HWND_FRMOUTPUT_VSCROLL, SW_SHOW )
    end if    
            
    function = 0
end function


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmOutput_PositionWindows() as LRESULT
    
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then exit function
    
    dim as long nTop  = pWindow->ScaleY(2)
    dim as long nLeft = 0 
    dim as long nTabsHeight
    dim as RECT rc: GetClientRect( HWND_FRMOUTPUT, @rc )
    
    dim as HWND hTabs = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TABS)

    nTabsHeight = AfxGetWindowHeight(hTabs)
    SetWindowPos( hTabs, 0, nLeft, nTop, rc.right - rc.left, nTabsHeight, SWP_NOZORDER )

    ' Position the child controls
    dim as long nVScrollWidth = AfxGetWindowWidth(HWND_FRMOUTPUT_VSCROLL)
    nTop = nTop + nTabsHeight + pWindow->ScaleY(8)
    
    SetWindowPos( HWND_FRMOUTPUT_LVRESULTS, 0, nLeft, nTop, rc.right, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_LVSEARCH, 0, nLeft, nTop, rc.right, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_LVTODO, 0, nLeft, nTop, rc.right, rc.bottom - nTop, SWP_NOZORDER )
    
    SetWindowPos( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), 0, _
        nLeft, nTop, rc.right - nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), 0, _
        nLeft, nTop, rc.right - nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_VSCROLL, 0, _
        rc.right - nVScrollWidth, nTop, nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )

    ' Calculate the tabs rects
    GetClientRect( hTabs, @rc )

    dim wszText as wstring * 100
    dim as long nTextLen 
    dim as long hmargin = pWindow->ScaleX(10) 
    rc.top = rc.top + pWindow->ScaleY(2)
    rc.left = rc.left + pWindow->ScaleX(10)

    wszText = ucase(L(191, "Compiler Results"))
    nTextLen = pWindow->ScaleX(getTextWidth( HWND_FRMOUTPUT, wszText, GUIFONT_9))
    gOutputTabs(0).wszText = wszText
    gOutputTabs(0).rcTab = rc: gOutputTabs(0).rcText = rc
    gOutputTabs(0).rcText.left = gOutputTabs(0).rcTab.left + hmargin 
    gOutputTabs(0).rcText.right = gOutputTabs(0).rcText.left + nTextLen 
    gOutputTabs(0).rcTab.right = gOutputTabs(0).rcText.right + hmargin

    wszText = ucase(L(252, "Compiler Log File"))
    nTextLen = pWindow->ScaleX(getTextWidth( HWND_FRMOUTPUT, wszText, GUIFONT_9))
    gOutputTabs(1).wszText = wszText
    gOutputTabs(1).rcTab = rc: gOutputTabs(1).rcText = rc
    gOutputTabs(1).rcTab.left = gOutputTabs(0).rcTab.right
    gOutputTabs(1).rcText.left = gOutputTabs(1).rcTab.left + hmargin 
    gOutputTabs(1).rcText.right = gOutputTabs(1).rcText.left + nTextLen 
    gOutputTabs(1).rcTab.right = gOutputTabs(1).rcText.right + hmargin

    wszText = ucase(L(262, "Search Results"))
    nTextLen = pWindow->ScaleX(getTextWidth( HWND_FRMOUTPUT, wszText, GUIFONT_9))
    gOutputTabs(2).wszText = wszText
    gOutputTabs(2).rcTab = rc: gOutputTabs(2).rcText = rc
    gOutputTabs(2).rcTab.left = gOutputTabs(1).rcTab.right
    gOutputTabs(2).rcText.left = gOutputTabs(2).rcTab.left + hmargin 
    gOutputTabs(2).rcText.right = gOutputTabs(2).rcText.left + nTextLen 
    gOutputTabs(2).rcTab.right = gOutputTabs(2).rcText.right + hmargin

    wszText = ucase(L(263, "TODO"))
    nTextLen = pWindow->ScaleX(getTextWidth( HWND_FRMOUTPUT, wszText, GUIFONT_9))
    gOutputTabs(3).wszText = wszText
    gOutputTabs(3).rcTab = rc: gOutputTabs(3).rcText = rc
    gOutputTabs(3).rcTab.left = gOutputTabs(2).rcTab.right
    gOutputTabs(3).rcText.left = gOutputTabs(3).rcTab.left + hmargin 
    gOutputTabs(3).rcText.right = gOutputTabs(3).rcText.left + nTextLen 
    gOutputTabs(3).rcTab.right = gOutputTabs(3).rcText.right + hmargin

    wszText = ucase(L(264, "Notes"))
    nTextLen = pWindow->ScaleX(getTextWidth( HWND_FRMOUTPUT, wszText, GUIFONT_9))
    gOutputTabs(4).wszText = wszText
    gOutputTabs(4).rcTab = rc: gOutputTabs(4).rcText = rc
    gOutputTabs(4).rcTab.left = gOutputTabs(3).rcTab.right
    gOutputTabs(4).rcText.left = gOutputTabs(4).rcTab.left + hmargin 
    gOutputTabs(4).rcText.right = gOutputTabs(4).rcText.left + nTextLen 
    gOutputTabs(4).rcTab.right = gOutputTabs(4).rcText.right + hmargin

    dim as long rcCloseWidth = 20
    dim as long rcCloseHeight = 20
    dim as long vmargin = pWindow->ScaleY( (OUTPUT_TABS_HEIGHT - rcCloseHeight) / 2 )
    GetClientRect( hTabs, @rc )
    gOutputCloseRect.top = gOutputTabs(4).rcTab.top + vmargin
    gOutputCloseRect.bottom = gOutputTabs(4).rcTab.bottom - vmargin
    gOutputCloseRect.right = rc.right - hmargin 
    gOutputCloseRect.left = gOutputCloseRect.right - pWindow->ScaleX(rcCloseWidth)

    ' Determine which child controls should be shown or hidden
    frmOutput_ShowHideOutputControls(HWND_FRMOUTPUT)
    
    function = 0
end function


' ========================================================================================
' GetVisibleLineCount number of lines per page in the RichEdit textbox
' ========================================================================================
function GetVisibleLineCount( byval hRichEdit as HWND ) as long
    dim as RECT formatRect
    SendMessage( hRichEdit, EM_GETRECT, 0, cast(LPARAM, @formatRect) )
    
    dim as HDC hdc = GetDC(hRichEdit)

    dim as CHARFORMAT cf
    cf.cbSize = sizeof(CHARFORMAT)
    SendMessage( hRichEdit, EM_GETCHARFORMAT, 0, cast(LPARAM, @cf) )
    dim as integer fontHeightPixels = MulDiv(cf.yHeight, GetDeviceCaps(hdc, LOGPIXELSY), 1440)

    ReleaseDC( hRichEdit, hdc )
    
    dim as long formatHeight = formatRect.bottom - formatRect.top

    return max(0, (formatHeight / fontHeightPixels) -4 )   'TODO: Hack with "4" 
end function


' ========================================================================================
' Calculate the RECT that holds the client coordinates of the scrollbar's vertical thumb
' Returns True if RECT is not empty
' ========================================================================================
function frmOutputVScroll_calcVThumbRect( byval hTextBox as HWND ) as boolean
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 Then exit function

    ' calculate the vertical scrollbar in client coordinates
    dim as RECT rc = AfxGetWindowRect(hTextBox)
    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 ) 
    dim as long clientHeight = rc.bottom - rc.top
    dim as long firstVisibleLine = SendMessage(hTextBox, EM_GETFIRSTVISIBLELINE, 0, 0)
    dim as long iScrollbarWidth = AfxGetWindowWidth(HWND_FRMOUTPUT_VSCROLL) 
    dim as long minThumbHeight = pWindow->ScaleY(SCROLLBAR_MINTHUMBSIZE)
    SetRectEmpty( @gOutputVScroll.rc )

    with gOutputVScroll
        .linesPerPage = GetVisibleLineCount(hTextBox)
        .numLines = SendMessage(hTextBox, EM_GETLINECOUNT, 0, 0) + 1
        dim as double ratio = .linesPerPage / .numLines
        .thumbHeight = int(ratio * clientHeight)
        if .thumbHeight < minThumbHeight then .thumbHeight = minThumbHeight

        .rc.left = 0
        .rc.right = .rc.left + iScrollbarWidth
        .rc.top = ((firstVisibleLine / .numLines) * clientHeight)
        .rc.bottom = .rc.top + .thumbHeight
        if .numLines < .linesPerPage then 
            SetRectEmpty( @gOutputVScroll.rc )
            return true
        end if

    end with

    function = 0
end function


' ========================================================================================
' frmOutputVScroll Window procedure
' ========================================================================================
function frmOutputVScroll_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    ' screen pt.y cursor position
    static as point prev_pt   

    dim as HWND hTextBox
    if IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)
    elseif IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)
    end if
    
    select case uMsg
        case WM_NCDESTROY
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMOUTPUT_VSCROLL   = 0
            return 0

        case WM_LBUTTONDOWN
            dim as point pt: GetCursorPos( @pt )
            frmOutputVScroll_calcVThumbRect(hTextBox)   ' in client coordinates
            dim as RECT rc = gOutputVScroll.rc       ' covert copy to screen coordinates
            MapWindowpoints( hwnd, HWND_DESKTOP, cast(point ptr, @rc), 2)
            if PtInRect( @rc, pt ) then
                prev_pt = pt
                gApp.bDragActive = true
                SetCapture( hwnd )
            else
                ' we have clicked on a PageUp or PageDn
                dim as long nDiff
                if pt.y < rc.top then
                    nDiff = -gOutputVScroll.linesPerPage
                elseif pt.y > rc.bottom then
                    nDiff = gOutputVScroll.linesPerPage
                end if
                SendMessage( hTextBox, EM_LINESCROLL, 0, cast(LPARAM, nDiff) )
                frmOutputVScroll_calcVThumbRect(hTextBox)
                AfxRedrawWindow( hwnd )
            end if
            
        case WM_MOUSEMOVE
            if gApp.bDragActive then
                dim as point pt: GetCursorPos( @pt )
                if pt.y <> prev_pt.y then 
                    dim as long delta = pt.y - prev_pt.y 

                    ' convert to client coordinates for ease of use
                    dim as RECT rc: GetClientRect( hwnd, @rc )
                    gOutputVScroll.rc.top = max(0, gOutputVScroll.rc.top + delta)
                    gOutputVScroll.rc.top = min(gOutputVScroll.rc.top, rc.bottom - gOutputVScroll.thumbHeight)
                    gOutputVScroll.rc.bottom = gOutputVScroll.rc.top + gOutputVScroll.thumbHeight

                    prev_pt = pt
                    
                    dim as long nPrevTopLine = SendMessage( hTextBox, EM_GETFIRSTVISIBLELINE, 0, 0 ) 
                    rc = AfxGetWindowRect(hTextBox)
                    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 ) 
                    dim as long clientHeight = rc.bottom - rc.top
                    if clientHeight = 0 then exit function
                    dim as double scrollRatio = gOutputVScroll.rc.top / (clientHeight - gOutputVScroll.thumbHeight)
                    dim as long nTopLine = int(scrollRatio * (gOutputVScroll.numLines - gOutputVScroll.linesPerPage))
                    if nTopLine <> nPrevTopLine then
                        dim as long nDiff = nTopLine - nPrevTopLine
                        SendMessage( hTextBox, EM_LINESCROLL, 0, cast(LPARAM, nDiff) )
                    end if   
                    AfxRedrawWindow( hwnd )

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
            b.SetBackColors( ghOutput.BackColorScrollBar, ghOutput.BackColorScrollBar )
            b.PaintClientRect()
            if IsRectEmpty( @gOutputVScroll.rc ) = 0 then
                b.SetPenColor( ghOutput.ScrollBarDivider )
                b.PaintLine( 1, b.rcClient.left, b.rcClient.top, b.rcClient.left, b.rcClient.bottom )
            end if    
            b.SetBackColors( ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )
            b.PaintRect( @gOutputVScroll.rc )

            b.EndDoubleBuffer()

            return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( HWnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        ' Position all of the child windows
        frmOutput_PositionWindows()
    end if

    function = 0
end function
    

' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnDestroy( byval hwnd as HWND ) as LRESULT
    function = 0
end Function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case codeNotify
     
        case EN_UPDATE
            if gApp.bDragActive = false then
                frmOutputVScroll_calcVThumbRect(hwndCtl)
                AfxRedrawWindow( HWND_FRMOUTPUT_VSCROLL )
            end if
            
        case EN_CHANGE
            ' Notes have been modified. Save them to the correct global variable
            ' to ensure that the changes are not lost when documents or projects
            ' are switched.
            if id = IDC_FRMOUTPUT_TXTNOTES then
                if gApp.IsProjectActive then
                    gApp.ProjectNotes = AfxGetWindowText(hwndCtl)
                else
                    gApp.NonProjectNotes = AfxGetWindowText(hwndCtl)
                end if   
                exit function
            end if
            
    end select
                        
    function = 0
end function
        
   
' ========================================================================================
' Process WM_MOUSEMOVE message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnMouseMove( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function

    ' HITTEST (PANELS SPLITTER)
    dim as point pt 
    dim as RECT rc
    GetWindowRect( HWND_FRMOUTPUT, @rc )
    rc.Bottom = rc.Top + pWindow->ScaleY(SPLITSIZE)
    GetCursorPos(@pt)
    if PtInRect( @rc, pt ) then
        if WindowFromPoint(pt) = HWND_FRMOUTPUT then
            SetCursor( ghCursorSizeNS )
        end if
    end if
    
    if gApp.bDragActive then
        if gApp.hwndPanel = HWND_FRMOUTPUT then
            gConfig.ShowOutputPanelMinimized = 0
            GetCursorPos(@pt)
            GetWindowRect( HWND_FRMOUTPUT, @rc )
            dim as long nHeight 
            dim as long nDiff = pt.y - rc.top
            ' Adjust the height. The positioning will be taken care of in PositionMainWindows().
            rc.top = rc.top + nDiff
    
            ' Don't move the Output pane if the top is less than the bottom of the TopTabs
            dim as RECT rc2
            dim as long nTopLimit 
            GetWindowRect( HWND_FRMMAIN_MENUBAR, @rc2 )
            nTopLimit = rc2.bottom
            if gTTabCtl.GetItemCount then 
                GetWindowRect( HWND_FRMMAIN_TOPTABS, @rc2 )
                nTopLimit = rc2.bottom
            end if
            rc.top = max(rc.top, nTopLimit)
            nHeight = (rc.bottom-rc.top) - pWindow->ScaleY(SPLITSIZE) ' allow room to grab the top
            
            ' The minimum height of the Output window when visible is the height of the tabs
            nHeight = max( nHeight, pWindow->ScaleY(OUTPUT_TABS_HEIGHT) )

            SetWindowPos( HWND_FRMOUTPUT, 0, 0, 0, rc.Right - rc.Left, nHeight, SWP_NOMOVE or SWP_NOZORDER )
            frmMain_PositionWindows
            exit function
        end if
    end if

    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONDOWN message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnLButtonDown( _
            byval hwnd as HWND, _
            byval fDoubleClick as Boolean, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit function

    ' HITTEST (PANELS TOP/BOTTOM SPLITTER)
    dim as Rect rc
    dim as point pt 

    gApp.bDragActive = False 
    
    GetWindowRect HWND_FRMOUTPUT, @rc
    rc.Bottom = rc.Top + pWindow->ScaleY(SPLITSIZE)
    GetCursorPos(@pt)
    if PtInRect( @rc, pt ) then
        if WindowFromPoint(pt) = HWND_FRMOUTPUT then
            SetCursor( ghCursorSizeNS )
            gApp.bDragActive = true 
            gApp.hwndPanel   = HWND_FRMOUTPUT
            SetCapture( HWND_FRMOUTPUT )
        end if
        exit function
    end if

    function = 0
end function


sub frmOutput_HandleSingleClick(hwnd as HWND)
    if gApp.bDragActive then
        gConfig.ShowOutputPanelHeight = AfxGetWindowHeight(hwnd)
        gApp.bDragActive = False 
        gApp.hwndPanel = 0
    end if    
    ReleaseCapture()
end sub


sub frmOutput_HandleDoubleClick(hwnd as HWND)
    OnCommand_ViewOutput()   ' toggle the Output window off
    ReleaseCapture()
end sub


' ========================================================================================
' Timer callback function for differentiating between single/double click
' ========================================================================================
sub frmOutput_TimerProc(hwnd as HWND, uMsg as UINT, idEvent as UINT_PTR, dwTime as DWORD)
    KillTimer(hwnd, idEvent)
    if gApp.doubleClickReceived = false then
        frmOutput_HandleSingleClick(hwnd)
    end if
end sub


' ========================================================================================
' Process WM_LBUTTONUP message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnLButtonUp( _
            byval hwnd as HWND, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    ' Start a timer to delay single-click action and distinguish between
    ' the single click and doubleclick
    gApp.doubleClickReceived = false
    SetTimer(hwnd, gApp.idTimerOutputPanel, 100, @frmOutput_TimerProc)
    ReleaseCapture
    SetCursor( LoadCursor( null, IDC_ARROW ))

    function = 0
end function


' ========================================================================================
' Process WM_LBUTTONDBLCLK message for window/dialog: frmOutput
' ========================================================================================
function frmOutput_OnLButtonDblClk( _
            byval hwnd as HWND, _
            byval fDoubleClick as boolean, _
            byval x as long, _
            byval y as long, _
            byval keyflags as UINT _
            ) as long

    if gApp.bDragActive then gApp.bDragActive = false
    KillTimer(hwnd, gApp.idTimerOutputPanel)
    gApp.doubleClickReceived = true
    frmOutput_HandleDoubleClick(hwnd)

    function = 0
end function


' ========================================================================================
' Do hit test to determine what tab is currently under the mouse cursor
' ========================================================================================
function frmOutputTabs_getHotTabHitTest( byval hWin as HWND ) as long
    dim as point pt: GetCursorPos( @pt )
    MapWindowpoints( HWND_DESKTOP, hWin, cast( point ptr, @pt ), 1 )
    dim as long hotTab = -1
    for i as long = lbound(gOutputTabs) to ubound(gOutputTabs)
        if PtInRect( @gOutputTabs(i).rcTab, pt ) then
            hotTab = i
            gOutputTabs(i).isHot = true
        else   
            gOutputTabs(i).isHot = false
        end if
    next
    function = hotTab
end function


' ========================================================================================
' Minimize the Output window
' Result of double clicking an Output panel tab or clicking the "X' close icon.
' ========================================================================================
function frmOutput_MinimizePanel() as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then exit function
    gConfig.ShowOutputPanelMinimized = 1
    dim as long nWidth = AfxGetWindowWidth(HWND_FRMOUTPUT)
    dim as long nHeight = pWindow->ScaleY(OUTPUT_TABS_HEIGHT)
    AfxSetWindowSize(HWND_FRMOUTPUT, nWidth, nHeight)
    frmMain_PositionWindows()
    function = 0
end function


' ========================================================================================
' Restore the Output window to user controlled height
' Result of double clicking an Output panel tab.
' ========================================================================================
function frmOutput_RestorePanel() as long
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then exit function
    
    gConfig.ShowOutputPanelMinimized = 0
    
    dim as long nDefaultHeight = pWindow->ScaleY(OUTPUT_TABS_HEIGHT)
    dim as long nWidth = AfxGetWindowWidth(HWND_FRMOUTPUT)
    dim as long nHeight = gConfig.ShowOutputPanelHeight
    
    if nHeight <= nDefaultHeight then
        gConfig.ShowOutputPanelHeight = pWindow->ScaleY(OUTPUT_TABS_HEIGHT * 5)
        nHeight = nDefaultHeight
    end if
   
    ' Ensure that window is visible. Compile or Find results need a visible
    ' window in order to display results.
    AfxSetWindowSize(HWND_FRMOUTPUT, nWidth, nHeight)
    ShowWindow( HWND_FRMOUTPUT, SW_SHOW)
    frmMain_PositionWindows()
    
    function = 0
end function


' ========================================================================================
' frmOutputTabs_SubclassProc 
' ========================================================================================
function frmOutputTabs_SubclassProc ( _
            byval hWin   as HWND, _                 ' // Control window handle
            byval uMsg   as UINT, _                 ' // Type of message
            byval _wParam as WPARAM, _               ' // First message parameter
            byval _lParam as LPARAM, _               ' // Second message parameter
            byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
            byval dwRefData as DWORD_PTR _          ' // pointer to reference data
            ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    static as long accumDelta
    
    ' keep track of last index we were over so that we only issue a 
    ' repaint if the cursor has moved off of the tab
    static as long nLastIdx = -1
    static as boolean isLastClose = false

    static hTooltip as HWND
    if (uMsg = WM_DESTROY) andalso (IsWindow(hToolTip)) then DestroyWindow(hToolTip)
        
    select case uMsg
            
        case WM_MOUSEMOVE
            ' Track that we are over the control in order to catch the 
            ' eventual WM_MOUSELEAVE event
            dim tme as TrackMouseEvent
            tme.cbSize = sizeof(TrackMouseEvent)
            tme.dwFlags = TME_HOVER or TME_LEAVE
            tme.hwndTrack = hWin
            tme.dwHoverTime = 20    ' HOVER_DEFAULT is 500
            TrackMouseEvent(@tme) 

            if IsWindow(hTooltip) = 0 then hTooltip = AfxAddTooltip( hWin, "", false, false )

            dim as long idx = frmOutputTabs_getHotTabHitTest( hWin )
            if idx <> nLastIdx then
                nLastIdx = idx
                AfxRedrawWindow(hWin)   
            end if
            
            dim as boolean isClose = isMouseOverRECT( hWin, gOutputCloseRect )
            if isClose <> isLastClose then
                isLastClose = isClose
                AfxRedrawWindow(hWin)   
            end if
                        
        case WM_MOUSEHOVER
            dim as DWSTRING wszTooltip
            if isMouseOverRECT( hWin, gOutputCloseRect ) = true then
                ' Display the tooltip
                wszTooltip  = L(161, "Close")
            end if
            AfxSetTooltipText( hTooltip, hWin, wszTooltip )

        case WM_MOUSELEAVE
            ' reset the hot tab index
            frmOutputTabs_getHotTabHitTest( hWin )
            nLastIdx = -1
            AfxRedrawWindow(hWin)   
       
        case WM_LBUTTONUP
            if isMouseOverRECT( hWin, gOutputCloseRect ) = true then
                OnCommand_ViewOutput()   ' toggle the Output window off
            end if
            
        case WM_LBUTTONDOWN
            dim as long idx = frmOutputTabs_getHotTabHitTest( hWin )
            if idx = -1 then exit function
            frmOutput_RestorePanel()
            gOutputTabsCurSel = idx
            AfxRedrawWindow(hWin)   
            frmOutput_ShowHideOutputControls( HWND_FRMOUTPUT )

        case WM_LBUTTONDBLCLK
            ' One of the Output window tabs has been doubleclicked.
            ' Toggle the Output window between "minimized" state and the height
            ' that the user has manually resized the window to.
            if isMouseOverRECT( hWin, gOutputCloseRect ) = false then
                dim as long idx = frmOutputTabs_getHotTabHitTest( hWin )
                if idx = -1 then exit function
                if gConfig.ShowOutputPanelMinimized then
                    frmOutput_RestorePanel()
                else
                    frmOutput_MinimizePanel()
                end if
                gOutputTabsCurSel = idx
                AfxRedrawWindow(hWin)   
                frmOutput_ShowHideOutputControls( HWND_FRMOUTPUT )
            end if
            
        case WM_ERASEBKGND
            return true
        
        case WM_PAINT
            dim as clsDoubleBuffer b
            
            b.BeginDoubleBuffer(hWin)

            ' Set default fore and back colors
            b.SetForeColors( ghOutput.ForeColor, ghOutput.ForeColorHot )
            b.SetBackColors( ghOutput.BackColor, ghOutput.BackColorHot )

            ' Clear the entire back client area
            b.PaintClientRect()
          
            for i as long = lbound(gOutputTabs) to ubound(gOutputTabs)
                dim as RECT rc = gOutputTabs(i).rcTab
                if (i = gOutputTabsCurSel) or (gOutputTabs(i).isHot = true) then
                    b.SetForeColors( ghOutput.ForeColorHot, ghOutput.ForeColorHot )
                    b.SetBackColors( ghOutput.BackColorHot, ghOutput.BackColorHot )
                else
                    b.SetForeColors( ghOutput.ForeColor, ghOutput.ForeColorHot )
                    b.SetBackColors( ghOutput.BackColor, ghOutput.BackColorHot )
                end if
                b.SetFont( ghFont(GUIFONT_9) )
                b.PaintRect(@rc, true)
                b.PaintText( gOutputTabs(i).wszText, @rc, DT_CENTER )
            next

            ' Draw the "X" close item
            b.SetFont( ghFont(SYMBOLFONT_10) )
            b.SetForeColors( ghOutput.ForeColorHot, ghOutput.ForeColorHot )
            b.SetBackColors( ghOutput.BackColor, ghOutput.CloseBackColorHot )
            b.PaintRoundRect( @gOutputCloseRect, true )
            b.PaintText( wszIconClose, @gOutputCloseRect, DT_CENTER, true )
            
            ' Paint a simple line under the currently active tab
            if gOutputTabsCurSel <> -1 then
                dim as RECT rc = gOutputTabs(gOutputTabsCurSel).rcText
                b.SetPenColor( ghOutput.ForeColorHot )
                b.PaintLine( 2, _
                        rc.left, rc.bottom - pWindow->ScaleY(4), rc.right, rc.bottom - pWindow->ScaleY(4) )
            end if
            
            ' Paint a simple line at the top of the window that will act as a 
            ' visual separator between the Output window and the Scintilla window.
            dim as RECT rc = b.rcClient
            b.SetPenColor( ghOutput.Divider )
            b.PaintLine( 1, rc.left, rc.top, rc.right, rc.top )
                                    
            b.EndDoubleBuffer()

            return 0
    
        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmOutputTabs_SubclassProc, uIdSubclass )
            
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc( hWin, uMsg, _wParam, _lParam )

end function


' ========================================================================================
' frmOutput_RichEdit_SubclassProc Window procedure
' ========================================================================================
function frmOutput_RichEdit_SubclassProc ( _
        byval hWin   as HWND, _                 ' // Control window handle
        byval uMsg   as UINT, _                 ' // Type of message
        byval _wParam as WPARAM, _               ' // First message parameter
        byval _lParam as LPARAM, _               ' // Second message parameter
        byval uIdSubclass as UINT_PTR, _        ' // The subclass ID
        byval dwRefData as DWORD_PTR _          ' // pointer to reference data
        ) as LRESULT

    dim pWindow as CWindow ptr = AfxCWindowPtr(hWin)

    select case uMsg
        
        case WM_CONTEXTMENU 
            ' Create the right click popup menu
            dim as DWSTRING wszText = RichEdit_GetSelText( hWin )
            dim hPopUpMenu as HMENU = CreatePopupMenu()
            if len(wszText) then 
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_CUT, L(17,"Cut") )
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_COPY, L(18,"Copy") )
            end if
            if RichEdit_CanPaste( hWin, 0 ) then
                AppendMenu( hPopUpMenu, MF_ENABLED, IDM_PASTE, L(19,"Paste") )
            end if   
            AppendMenu( hPopUpMenu, MF_ENABLED, IDM_SELECTALL, L(40,"Select All") )

            dim as long nResult
            nResult = TrackPopupMenu( hPopUpMenu, TPM_RETURNCMD or TPM_NONOTIFY, _
                            loword(_lParam), hiword(_lParam), 0, HWND_FRMOUTPUT, 0 ) 
            SetFocus( hWin )
            select case nResult
                case IDM_CUT:       SendMessage( hWin, WM_CUT, 0, 0 )
                case IDM_COPY:      SendMessage( hWin, WM_COPY, 0, 0 )
                case IDM_PASTE:     SendMessage( hWin, WM_PASTE, 0, 0 )
                case IDM_SELECTALL
                    PostMessage( HWND_FRMOUTPUT, MSG_USER_RICHEDIT_SELECTALL, cast(WPARAM, hWin), 0 )
            end select
            DestroyMenu hPopUpMenu
            

        case WM_DESTROY
            ' REQUIRED: Remove control subclassing
            RemoveWindowSubclass( hWin, @frmOutput_RichEdit_SubclassProc, uIdSubclass )
    end select
    
    ' For messages that we don't deal with
    function = DefSubclassProc(hWin, uMsg, _wParam, _lParam)

end function

' ========================================================================================
' frmOutput Window procedure
' ========================================================================================
function frmOutput_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_COMMAND,       frmOutput_OnCommand)
        HANDLE_MSG (hwnd, WM_SIZE,          frmOutput_OnSize)
        HANDLE_MSG (hwnd, WM_LBUTTONUP,     frmOutput_OnLButtonUp)
        HANDLE_MSG (hwnd, WM_LBUTTONDOWN,   frmOutput_OnLButtonDown)
        HANDLE_MSG (hwnd, WM_MOUSEMOVE,     frmOutput_OnMouseMove)
        HANDLE_MSG (hwnd, WM_LBUTTONDBLCLK, frmOutput_OnLButtonDblClk)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmOutput_OnDestroy)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMOUTPUT = 0
        ' The child listviews have also been destroyed in their respective WM_NCDESTROY
        ' so reset all of the listview window handles.
        HWND_FRMOUTPUT_LVRESULTS = 0
        HWND_FRMOUTPUT_LVSEARCH  = 0
        HWND_FRMOUTPUT_LVTODO    = 0
        return 0


    case MSG_USER_LISTVIEW_LBUTTONDBLCLK
        dim as HWND hLV = cast(HWND, wParam)
        select case hLV
            case HWND_FRMOUTPUT_LVRESULTS, _
                 HWND_FRMOUTPUT_LVSEARCH, _
                 HWND_FRMOUTPUT_LVTODO
                SetDocumentErrorPosition( hLV )    
        end select
        
    case MSG_USER_RICHEDIT_SELECTALL
        dim as HWND hCtrl = cast(HWND, wParam) 
        SetFocus( hCtrl )
        Edit_SetSel( hCtrl, 0, -1 )

    case WM_ERASEBKGND
        return true

    case WM_PAINT            
        dim as clsDoubleBuffer b
        b.BeginDoubleBuffer(hwnd)
        b.SetBackColors( ghOutput.BackColor, ghOutput.BackColor )
        b.PaintClientRect()
        b.EndDoubleBuffer()
        return 0
    
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' Set the colors for the frmOutput controls. This is also called when the 
' user changes the theme (dark/light)
' ========================================================================================
function frmOutput_SetControlColors() as long
    dim cf as CHARFORMATW 
    cf.cbSize = sizeof(cf)
    cf.dwMask = CFM_COLOR
    cf.crTextColor = ghOutput.forecolorhot

    dim as HWND hCtl
    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE )
    SendMessage( hCtl, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtl, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghOutput.backcolor) )
    
    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES )
    SendMessage( hCtl, EM_SETCHARFORMAT, SCF_ALL, cast(LPARAM, @cf) ) 
    SendMessage( hCtl, EM_SETBKGNDCOLOR , 0, cast(LPARAM, ghOutput.backcolor) )

    hCtl = HWND_FRMOUTPUT_LVRESULTS 
    frmListView_SetForeColors( hCtl, ghOutput.ForeColor, ghOutput.ForeColorHot )
    frmListView_SetBackColors( hCtl, ghOutput.BackColor, ghOutput.ForeColorScrollBar )
    frmListView_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, ghOutput.ForeColorScrollBar, ghOutput.ScrollBarDivider )

    hCtl = HWND_FRMOUTPUT_LVSEARCH
    frmListView_SetForeColors( hCtl, ghOutput.ForeColor, ghOutput.ForeColorHot )
    frmListView_SetBackColors( hCtl, ghOutput.BackColor, ghOutput.ForeColorScrollBar )
    frmListView_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, ghOutput.ForeColorScrollBar, ghOutput.ScrollBarDivider )

    hCtl = HWND_FRMOUTPUT_LVTODO 
    frmListView_SetForeColors( hCtl, ghOutput.ForeColor, ghOutput.ForeColorHot )
    frmListView_SetBackColors( hCtl, ghOutput.BackColor, ghOutput.ForeColorScrollBar )
    frmListView_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, ghOutput.ForeColorScrollBar, ghOutput.ScrollBarDivider )

    function = 0
end function


' ========================================================================================
' frmOutput_Show
' ========================================================================================
function frmOutput_Show( byval hwndParent as HWND ) as LRESULT
    '  Create the main window and child controls
    '  This pointer destroyed at end of frmMain ends before application ends.
    dim pWindow as CWindow ptr = new CWindow
    dim as long nHeight
    
    ' Set the height of the window to the last used visible height or to the
    ' default height if gConfig.ShowOutputPanelHeight is less than it.
    gConfig.ShowOutputPanelHeight = max(gConfig.ShowOutputPanelHeight, OUTPUT_TABS_HEIGHT)
    nHeight = gConfig.ShowOutputPanelHeight
    
    ' If the Output window was in minimized state when use the default tabs height
    ' for showing the initial height of the window.
    if gConfig.ShowOutputPanelMinimized <> 0 then nHeight = OUTPUT_TABS_HEIGHT

    ' Only make the Output panel initially visible if it was already visible
    ' when the most previous instance of the program closed. 
    HWND_FRMOUTPUT = pWindow->Create( hwndParent, "", @frmOutput_WndProc, 0, 0, 0, nHeight, _
        WS_CHILD or iif(gConfig.ShowOutputPanel, WS_VISIBLE, 0) or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->ClassStyle = CS_DBLCLKS

    ' Now that the Output window has been created using the unscaled height from the
    ' config file, we need to scale up the user defined panel height because future psoitioning
    ' of the window depends on scaled values. However, it is unscaled prior to saving the
    ' value back to the config file on program exit.
    gConfig.ShowOutputPanelHeight = pWindow->ScaleY(gConfig.ShowOutputPanelHeight)
    
    dim as HWND hCtl, hLV, hLB
    
    ' custom tab control (we paint our Tabs on this control)
    hCtl = _
    pWindow->AddControl("LABEL", , IDC_FRMOUTPUT_TABS, , 0, 0, 0, OUTPUT_TABS_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN or SS_NOTIFY or SS_LEFT, _
        WS_EX_LEFT or WS_EX_LTRREADING)
    pWindow->SetControlSubclass(hCtl, @frmOutputTabs_SubclassProc, IDC_FRMOUTPUT_TABS, @pWindow)

    hCtl = pWindow->AddControl("RICHEDIT", , IDC_FRMOUTPUT_TXTLOGFILE, "", _
                    0, 0, 0, 0, _
                    WS_CHILD or WS_TABSTOP or WS_VSCROLL or _
                    ES_READONLY or ES_MULTILINE or ES_LEFT or ES_AUTOVSCROLL, _
                    WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtl, @frmOutput_RichEdit_SubclassProc, IDC_FRMOUTPUT_TXTLOGFILE, NULL)
        SendMessage( hCtl, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE or ENM_UPDATE) )
        SendMessage( hCtl, EM_SHOWSCROLLBAR, SB_VERT, FALSE)
        SendMessage( hCtl, EM_SHOWSCROLLBAR, SB_HORZ, FALSE)

    hCtl = pWindow->AddControl("RICHEDIT", , IDC_FRMOUTPUT_TXTNOTES, "", _
                    0, 0, 0, 0, _
                    WS_CHILD or WS_TABSTOP or WS_VSCROLL or _
                    ES_MULTILINE or ES_LEFT or ES_AUTOVSCROLL or ES_WANTRETURN, _
                    WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->SetControlSubclass(hCtl, @frmOutput_RichEdit_SubclassProc, IDC_FRMOUTPUT_TXTNOTES, NULL)
        
        SendMessage( hCtl, EM_SETEVENTMASK, 0, cast(LPARAM, ENM_SELCHANGE or ENM_CHANGE or ENM_UPDATE) )
        SendMessage( hCtl, EM_SHOWSCROLLBAR, SB_VERT, FALSE)
        SendMessage( hCtl, EM_SHOWSCROLLBAR, SB_HORZ, FALSE)
        frmOutput_ShowNotes()   

    ' Compiler Results Listview
    hLV = frmListView_Show( HWND_FRMOUTPUT )
        frmListView_AddColumn( hLV, 0, "", pWindow->ScaleX(20) )
        frmListView_AddColumn( hLV, 1, L(253, "Line"), pWindow->ScaleX(75) )
        frmListView_AddColumn( hLV, 2, L(254, "File"), pWindow->ScaleX(250) )
        frmListView_AddColumn( hLV, 3, L(255, "Description"), pWindow->ScaleX(1480) )
        HWND_FRMOUTPUT_LVRESULTS = hLV

    ' Search Listview
    hLV = frmListView_Show( HWND_FRMOUTPUT )
        frmListView_AddColumn( hLV, 0, "", pWindow->ScaleX(20) )
        frmListView_AddColumn( hLV, 1, L(253, "Line"), pWindow->ScaleX(75) )
        frmListView_AddColumn( hLV, 2, L(254, "File"), pWindow->ScaleX(250) )
        frmListView_AddColumn( hLV, 3, L(255, "Description"), pWindow->ScaleX(1480) )
        HWND_FRMOUTPUT_LVSEARCH = hLV

    ' TODO listview
    hLV = frmListView_Show( HWND_FRMOUTPUT )
        frmListView_AddColumn( hLV, 0, "", pWindow->ScaleX(20) )
        frmListView_AddColumn( hLV, 1, L(253, "Line"), pWindow->ScaleX(75) )
        frmListView_AddColumn( hLV, 2, L(254, "File"), pWindow->ScaleX(250) )
        frmListView_AddColumn( hLV, 3, L(255, "Description"), pWindow->ScaleX(1480) )
        HWND_FRMOUTPUT_LVTODO = hLV

    dim pWindowPanel as CPanelWindow ptr = New CPanelWindow
    HWND_FRMOUTPUT_VSCROLL = _
        pWindowPanel->Create( HWND_FRMOUTPUT, @frmOutputVScroll_WndProc, 0, 0, SCROLLBAR_WIDTH_EDITOR, 0 )
    pWindowPanel->ClassStyle = CS_DBLCLKS

    frmOutput_ResetAllControls
    frmOutput_SetControlColors
    frmOutput_PositionWindows
    
    function = 0

end function

