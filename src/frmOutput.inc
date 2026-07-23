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
' A double-click on any of the three listview panes jumps the editor to that row's
' file/line. This replaces the MSG_USER_LISTVIEW_LBUTTONDBLCLK round trip the old control
' used to post up to this form.
' ========================================================================================
function frmOutput_LVMessage( byval m as CLISTBOX_MESSAGEINFO ptr ) as boolean
    if m->uMsg <> WM_LBUTTONDBLCLK then return false
    ' m->hList is the inner listbox; the CListBox container is its parent, and that is what
    ' SetDocumentErrorPosition queries.
    SetDocumentErrorPosition( GetParent(m->hList) )
    return false      ' let the listbox keep its own default handling
end function


' ========================================================================================
' Build one of the three Output listview panes. They are identical apart from their
' control id, so this is called once each rather than copied three times.
' ========================================================================================
function frmOutput_CreateListView( byval CtrlID as long ) as HWND
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then return 0

    dim as HWND hCtl = CListBox_Create( HWND_FRMOUTPUT, CtrlID )

    ' Columns, in the order the chr(9)-delimited rows are built: spacer, Line, File,
    ' Description. Description is the fill column.
    CListBox_AddColumn( hCtl, "",                  pWindow->ScaleX(20) )
    CListBox_AddColumn( hCtl, L(253, "Line"),      pWindow->ScaleX(75) )
    CListBox_AddColumn( hCtl, L(254, "File"),      pWindow->ScaleX(250) )
    CListBox_AddColumn( hCtl, L(255, "Description"), pWindow->ScaleX(250) )
    CListBox_SetFillColumn( hCtl, 3 )
    CListBox_ShowHeader( hCtl, true )

    CListBox_SetPaintCallback( hCtl, @ListBox_LVPaintRow )
    CListBox_SetHeaderPaintCallback( hCtl, @ListBox_LVPaintHeader )
    CListBox_SetColumnAutoSizeCallback( hCtl, @ListBox_LVAutoSizeColumn )
    CListBox_SetMessageCallback( hCtl, @frmOutput_LVMessage )
    CListBox_SetFont( hCtl, ghFont(GUIFONT_9) )
    CListBox_SetBackColor( hCtl, ghOutput.BackColor )
    CListBox_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, _
                                 ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )

    return hCtl
end function


' ========================================================================================
' Clear data from all of the controls in the frmOutput windows. This is needed
' when Projects are loaded and closed.
' ========================================================================================
function frmOutput_ResetAllControls() as long
    CListBox_Clear( HWND_FRMOUTPUT_LVRESULTS )
    AfxSetWindowText( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), "" )
    CListBox_Clear( HWND_FRMOUTPUT_LVSEARCH )
    CListBox_Clear( HWND_FRMOUTPUT_LVTODO )
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
    CListBox_Clear( hLV )

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
        ListBox_AddTabbedRow( hLV, wszText )
    next

    frmOutput_PositionWindows

    function = 0
end function

' ========================================================================================
' Update the SEARCH listview
' ========================================================================================
function frmOutput_UpdateSearchListview( byref wszResultFile as wstring ) as long 
    dim hLV as HWND = HWND_FRMOUTPUT_LVSEARCH

    CListBox_Clear( hLV )

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
        ListBox_AddTabbedRow( hLV, wszText )
    loop
    pStream.Close
    
    AfxDeleteFile( wszResultFile )
    
    ' Show the search results. SetCurSel is silent, so PositionWindows below (which ends in
    ' ShowHideOutputControls) is what actually swaps the visible pane.
    CSelectBar_SetCurSel( HWND_FRMOUTPUT_SELECTBAR, OUTPUT_TAB_SEARCH )
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

    ShowWindow( HWND_FRMOUTPUT_SELECTBAR, SW_SHOW )
    ShowWindow( HWND_FRMOUTPUT_CLOSE, SW_SHOW )
    AfxRedrawWindow( HWND_FRMOUTPUT_SELECTBAR )
    
    ' By default, hide all controls
    ShowWindow( HWND_FRMOUTPUT_LVRESULTS, SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_LVSEARCH, SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_LVTODO, SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE), SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTNOTES), SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_VSCROLL, SW_HIDE )
    
    select case CSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR )
        case 0    ' compiler results
            hLV = HWND_FRMOUTPUT_LVRESULTS
            hCtrl = hLV

        case 1    ' compiler log file
            ' (The old EM_SETRECT 20px-indent hack is gone: the CTextBox carries a
            ' permanent left margin instead, set at creation.)
            hTextBox = GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE)
            hCtrl = hTextBox
            dim as DWSTRING wszText = ltrim(AfxGetWindowText(hTextBox))
            if wszText = "" then
               AfxSetWindowText(hTextBox, "No compiler log file exists.")
            end if

        case 2    ' search results
            hLV = HWND_FRMOUTPUT_LVSEARCH
            hCtrl = hLV

        case 3    ' TODO list
            hLV = HWND_FRMOUTPUT_LVTODO
            hCtrl = hLV

        case 4    ' Notes
            ' Do NOT SetFocus the notes textbox here. This routine is pure
            ' show/hide/layout and is also reached from frmOutput_PositionWindows, which
            ' runs on every background-scan refresh -- focusing here yanked the caret off
            ' the editor each time typing paused (the debounce timer fires a scan). The
            ' focus-on-switch now lives in the user-driven frmOutput_SelectBarSelChange.
            hTextBox = GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTNOTES)
            hCtrl = hTextBox

    end select
    
    if hLV then
        ' The old control had to be HIDDEN when empty, because with no items its
        ' WM_DRAWITEM never fired and the area rendered white. CListBox answers
        ' WM_CTLCOLORLISTBOX with its own background brush (see C:\dev\Learnings.md), so an
        ' empty pane simply draws empty and the hack is gone. It also lays itself out, so
        ' there is nothing to position here either.
        ShowWindow( hLV, SW_SHOW )
    end if

    if hTextBox then
        SetWindowPos( hCtrl, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE or SWP_SHOWWINDOW )
        ShowWindow( HWND_FRMOUTPUT_VSCROLL, SW_SHOW )
        ' Push the fresh range now that the textbox is visible (a hidden textbox never
        ' drives the shared scrollbar, so a tab switch must re-push).
        frmOutput_TextBoxScrollChanged( hTextBox )
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
    
    ' The tab strip: CSelectBar on the left, the one-item CIconPanel ("X") hard against the
    ' right. They abut, and each paints its own background, so together they cover the strip
    ' -- there is no container window behind them.
    nTabsHeight = pWindow->ScaleY(OUTPUT_TABS_HEIGHT)
    dim as long nCloseWidth = pWindow->ScaleX(OUTPUT_CLOSE_WIDTH)
    dim as long nStripWidth = rc.right - rc.left

    SetWindowPos( HWND_FRMOUTPUT_SELECTBAR, 0, _
                  nLeft, nTop, nStripWidth - nCloseWidth, nTabsHeight, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_CLOSE, 0, _
                  nLeft + nStripWidth - nCloseWidth, nTop, nCloseWidth, nTabsHeight, SWP_NOZORDER )

    ' Re-push the captions so a language change is picked up, exactly as the old code did by
    ' rebuilding them here every time -- but only when one actually differs. SetText
    ' re-measures and re-lays out the whole bar, so doing it unconditionally on every resize
    ' would be pure churn.
    frmOutput_SetTabCaptions()

    ' Position the child controls. The CVScrollBar is created 0x0 (its width cannot be
    ' read back before the first positioning), so the width is computed, not queried --
    ' same 12px design width the old CPanelWindow scrollbar was created with.
    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR)
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

    ' (The ~60 lines of per-tab rect arithmetic that used to sit here are gone: CSelectBar
    ' measures its own captions and derives every rect. So is the close-button rect -- the
    ' CIconPanel owns that.)

    ' Determine which child controls should be shown or hidden
    frmOutput_ShowHideOutputControls(HWND_FRMOUTPUT)
    
    function = 0
end function


' ========================================================================================
' CTextBox callbacks for the Log/Notes controls.
' ========================================================================================
' Notes changed by USER interaction. Save them to the correct global variable so the
' changes are not lost when documents or projects are switched. (Programmatic SetText
' during those switches is silent, so this can never echo a switch back into the
' wrong project's notes.)
sub frmOutput_NotesChanged( byval hTextBox as HWND )
    if gApp.IsProjectActive then
        gApp.ProjectNotes = CTextBox_GetText( hTextBox )
    else
        gApp.NonProjectNotes = CTextBox_GetText( hTextBox )
    end if
end sub

' Vertical scroll state changed (typing, programmatic SetText, wheel) -- push the fresh
' range straight into the CVScrollBar, which re-derives its own thumb. Only the visible
' textbox drives the shared scrollbar (a hidden one is re-pushed on its tab switch).
' During a thumb drag the ScrollCallback -> ScrollToLine round trip echoes the same
' position back through here -- consistent by construction, no oscillation.
sub frmOutput_TextBoxScrollChanged( byval hTextBox as HWND )
    if IsWindowVisible( hTextBox ) = 0 then exit sub
    dim as integer nTotalLines, nLinesPerPage, nFirstVisible
    CTextBox_GetVScrollInfo( hTextBox, nTotalLines, nLinesPerPage, nFirstVisible )
    CVScrollBar_SetRange( HWND_FRMOUTPUT_VSCROLL, nTotalLines, nLinesPerPage, nFirstVisible )
end sub

' The user scrolled the CVScrollBar (thumb drag or track paging): scroll whichever
' textbox is visible to that first line.
sub frmOutput_VScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    dim as HWND hTextBox
    if IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)
    elseif IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)
    end if
    if hTextBox then CTextBox_ScrollToLine( hTextBox, newPos )
end sub

' Custom paint keeping the exact visuals of the old hand-rolled scrollbar (and of the
' listview scrollbars sharing the Output window): flat track, left divider line and a
' flat thumb when there is something to scroll, no hover brightening. Reads the theme
' colors live, so a theme switch only needs a repaint.
sub frmOutput_VScrollPaint( byval p as CVSCROLLBAR_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b
    b->SetBackColor( ghOutput.BackColorScrollBar )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetPenColor( ghOutput.ScrollBarDivider )
        b->PaintLine( 1, p->rcClient.left, p->rcClient.top, p->rcClient.left, p->rcClient.bottom )
        b->SetBackColor( ghOutput.ForeColorScrollBar )
        b->PaintRect( @p->rcThumb )
    end if
end sub


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
' The Output panel's splitter bar. It is a CSplitter owned by frmMain (the bar sits BETWEEN
' the editor area and this panel, and frmMain owns that layout), so what lives here are only
' the three callbacks.
'
' Everything the panel used to carry for it is gone: the WM_MOUSEMOVE hit-test, the
' WM_LBUTTONDOWN/UP drag with its gApp.bDragActive / gApp.hwndPanel globals, and the 100 ms
' timer (gApp.idTimerOutputPanel, doubleClickReceived, frmOutput_TimerProc,
' HandleSingleClick, HandleDoubleClick) that told a single click from a double click. The
' control's class carries CS_DBLCLKS, so that disambiguation is free.
' ========================================================================================

' Paint: the Output panel's background across the whole grab area, with a single 1 pixel
' horizontal divider line centered in it. The line stays 1 physical pixel at every DPI --
' it is a rule, not a band, so it is deliberately not scaled.
sub frmOutput_SplitterPaint( byval p as CSPLITTER_PAINTINFO ptr )
    dim as clsDoubleBuffer ptr b = p->b

    b->SetBackColor( ghOutput.BackColor )
    b->PaintClientRect()

    dim as RECT rc = p->rcClient
    dim as long nMid = rc.top + ((rc.bottom - rc.top) \ 2)
    rc.top = nMid
    rc.bottom = nMid + AfxScaleY(1)

    ' Constant color -- deliberately no isHot/isDragging highlight. The cursor change
    ' from the control is the only hover feedback this bar gets.
    b->SetBackColor( ghOutput.Divider )
    b->PaintRect( @rc )
end sub


' The user dragged the bar. The control has already moved itself; we convert its new top
' edge back into the panel HEIGHT, which is what the rest of tiko (and the persisted
' gConfig.ShowOutputPanelHeight) is expressed in.
sub frmOutput_SplitterMoved( _
            byval hSplitter as HWND, _
            byval newPos    as integer, _
            byval nPhase    as integer _
            )

    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow = 0 then exit sub
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub

    select case nPhase
        case CSPLITTER_PHASE_MOVE
            ' Dragging the bar un-minimizes the panel, exactly as the old drag did.
            gConfig.ShowOutputPanelMinimized = 0

            dim as RECT rc : GetClientRect( HWND_FRMMAIN, @rc )
            dim as long nGrab = pWindow->ScaleY(SPLITTER_GRAB)
            dim as long nHeightStatusBar = 0
            if IsWindowVisible(HWND_FRMMAIN_STATUSBAR) then _
                nHeightStatusBar = AfxGetWindowHeight(HWND_FRMMAIN_STATUSBAR)

            dim as long nHeight = rc.bottom - nHeightStatusBar - newPos - nGrab
            if nHeight < 0 then nHeight = 0
            AfxSetWindowSize( HWND_FRMOUTPUT, AfxGetWindowWidth(HWND_FRMOUTPUT), nHeight )
            frmMain_PositionWindows()

        case CSPLITTER_PHASE_END
            ' Persist the height the user settled on -- what HandleSingleClick did on the
            ' mouse-up.
            gConfig.ShowOutputPanelHeight = AfxGetWindowHeight(HWND_FRMOUTPUT)
    end select
end sub


' Double-click toggles the panel between minimized and its user-set height -- the same
' toggle the Output TABS already perform. (It used to hide the panel outright; the View
' menu item still does that.) Claiming the message means no drag starts.
function frmOutput_SplitterMessage( byval m as CSPLITTER_MESSAGEINFO ptr ) as boolean
    if m->uMsg <> WM_LBUTTONDBLCLK then return false
    if gConfig.ShowOutputPanelMinimized then
        frmOutput_RestorePanel()
    else
        frmOutput_MinimizePanel()
    end if
    return true
end function




' ========================================================================================
' Push the (localized, uppercased) captions into the tab bar, but only where one actually
' differs. The old code rebuilt these on every layout pass, which is how a language change
' got picked up; this preserves that without the churn, because CSelectBar_SetText
' re-measures and re-lays out the whole bar on every call.
' ========================================================================================
sub frmOutput_SetTabCaptions()
    if IsWindow(HWND_FRMOUTPUT_SELECTBAR) = 0 then exit sub

    dim as DWSTRING wszCaption(0 to 4)
    wszCaption(OUTPUT_TAB_RESULTS) = ucase(L(191, "Compiler Results"))
    wszCaption(OUTPUT_TAB_LOGFILE) = ucase(L(252, "Compiler Log File"))
    wszCaption(OUTPUT_TAB_SEARCH)  = ucase(L(262, "Search Results"))
    wszCaption(OUTPUT_TAB_TODO)    = ucase(L(263, "TODO"))
    wszCaption(OUTPUT_TAB_NOTES)   = ucase(L(264, "Notes"))

    for i as long = 0 to 4
        if CSelectBar_GetText( HWND_FRMOUTPUT_SELECTBAR, i ) <> wszCaption(i) then
            CSelectBar_SetText( HWND_FRMOUTPUT_SELECTBAR, i, wszCaption(i) )
        end if
    next
end sub


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
' Tab strip callbacks. The strip is a CSelectBar (five tabs) plus a one-item CIconPanel
' (the "X"), sized side by side by frmOutput_PositionWindows. Between them they replaced
' frmOutputTabs_SubclassProc: ~150 lines of rect arithmetic, hit-testing, hover tracking,
' tooltip plumbing and painting.
' ========================================================================================

' Consumes the WM_LBUTTONUP that PAIRS WITH a double-click. The message sequence for a
' double-click is down, up, dblclk, up -- so without this, a double-click meant to MINIMIZE
' the panel would be undone a moment later by its own trailing up restoring it (any click
' on the strip restores, which is long-standing behaviour worth keeping). One boolean
' consuming one message: deliberately NOT a click-timer dance, since nothing is timed.
dim shared as boolean gOutputSuppressRestore = false


' ----------------------------------------------------------------------------------------
' Paint one tab. Reads the ghOutput theme colors LIVE, so a theme switch needs only a
' repaint and no color-pushing plumbing anywhere -- the same pattern
' frmEditorHScroll_PaintCallback uses. The selected tab draws hot (fore AND back), which is
' what the hand-drawn strip did.
' ----------------------------------------------------------------------------------------
sub frmOutput_SelectBarPaint( byval p as CSELECTBAR_PAINTINFO ptr )
    dim as boolean isHotTab = p->isSelected orelse p->isHot orelse p->isPressed

    p->b->SetForeColor( iif( isHotTab, ghOutput.ForeColorHot, ghOutput.ForeColor ) )
    p->b->SetBackColor( iif( isHotTab, ghOutput.BackColorHot, ghOutput.BackColor ) )
    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_CENTER )

    ' The indicator under the current tab. rcIndicator already spans exactly the caption
    ' text and sits the configured gap below it -- no arithmetic needed here.
    if p->isSelected then
        p->b->SetBackColor( ghOutput.ForeColorHot )
        p->b->PaintRect( @p->rcIndicator )
    end if
end sub


' ----------------------------------------------------------------------------------------
' The USER switched tabs. Programmatic CSelectBar_SetCurSel is silent, so everything that
' switches tabs internally (a build finishing, a search completing, the View menu) reaches
' ShowHideOutputControls by its own path and never through here.
' ----------------------------------------------------------------------------------------
sub frmOutput_SelectBarSelChange( byval hCtl as HWND, byval idxOld as long, byval idxNew as long )
    frmOutput_RestorePanel()
    frmOutput_ShowHideOutputControls( HWND_FRMOUTPUT )
    ' Focus the notes textbox only on a genuine USER switch to the Notes tab. SelChange
    ' fires on user action only (programmatic SetCurSel is silent), so this cannot be
    ' triggered by a scan refresh -- which is exactly why the SetFocus was moved out of
    ' frmOutput_ShowHideOutputControls and to here.
    if idxNew = OUTPUT_TAB_NOTES then
        SetFocus( GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES ) )
    end if
end sub


' ----------------------------------------------------------------------------------------
' Mouse messages on the tab strip.
'   double-click -> toggle the panel between minimized and the user's height
'   plain click  -> restore the panel, even when the tab did not change (clicking the
'                   already-current tab while minimized has always reopened the panel, and
'                   SelChange cannot see that because nothing changed)
' ----------------------------------------------------------------------------------------
function frmOutput_SelectBarMessage( byval m as CSELECTBAR_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
        case WM_LBUTTONDBLCLK
            if m->idx <> -1 then
                if gConfig.ShowOutputPanelMinimized then
                    frmOutput_RestorePanel()
                else
                    frmOutput_MinimizePanel()
                end if
                ' The trailing up is part of this gesture, not a new click.
                gOutputSuppressRestore = true
            end if

        case WM_LBUTTONUP
            if gOutputSuppressRestore then
                gOutputSuppressRestore = false
            elseif m->idx <> -1 then
                frmOutput_RestorePanel()
            end if
    end select
    ' Never suppress: the control needs its own default handling (and the result is ignored
    ' for WM_LBUTTONUP anyway -- it is what releases the mouse capture).
    return false
end function


' ----------------------------------------------------------------------------------------
' The close "X". Painted to match the old hand-drawn button exactly: a rounded rect that
' fills only on hover, then the glyph.
' ----------------------------------------------------------------------------------------
sub frmOutput_ClosePaint( byval p as CICONPANEL_PAINTINFO ptr )
    p->b->SetBackColor( ghOutput.BackColor )
    p->b->PaintRect( @p->rc )

    p->b->SetForeColor( ghOutput.ForeColorHot )
    p->b->SetBackColor( iif( p->isHot orelse p->isPressed, _
                             ghOutput.CloseBackColorHot, ghOutput.BackColor ) )
    p->b->PaintRoundRect( @p->rcIcon )

    p->b->SetFont( ghFont(SYMBOLFONT_10) )
    p->b->PaintText( p->wszGlyph, @p->rcIcon, DT_CENTER )
end sub


sub frmOutput_CloseClick( byval hCtl as HWND, byval idx as long, byval id as long )
    OnCommand_ViewOutput()      ' toggle the Output window off
end sub


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
        HANDLE_MSG (hwnd, WM_SIZE,          frmOutput_OnSize)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmOutput_OnDestroy)

    case WM_NCDESTROY
        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMOUTPUT = 0
        ' The child listviews (and the CVScrollBar) have also been destroyed in their
        ' respective WM_NCDESTROY so reset all of their window handles.
        HWND_FRMOUTPUT_LVRESULTS = 0
        HWND_FRMOUTPUT_LVSEARCH  = 0
        HWND_FRMOUTPUT_LVTODO    = 0
        HWND_FRMOUTPUT_VSCROLL   = 0
        return 0


    ' MSG_USER_LISTVIEW_LBUTTONDBLCLK used to arrive here from the old listview's inner
    ' listbox subclass. The CListBox panes report the double-click through their own
    ' message callback instead -- see frmOutput_LVMessage.

    case WM_ERASEBKGND
        return true

    case WM_PAINT            
        dim as clsDoubleBuffer b
        b.BeginDoubleBuffer(hwnd)
        b.SetBackColor( ghOutput.BackColorHot )
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
    dim as HWND hCtl
    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE )
    CTextBox_SetForeColor( hCtl, ghOutput.ForeColorHot )
    CTextBox_SetBackColor( hCtl, ghOutput.BackColor )
    CTextBox_SetOuterBackColor( hCtl, ghOutput.BackColor )

    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES )
    CTextBox_SetForeColor( hCtl, ghOutput.ForeColorHot )
    CTextBox_SetBackColor( hCtl, ghOutput.BackColor )
    CTextBox_SetOuterBackColor( hCtl, ghOutput.BackColor )

    ' The scrollbar's paint callback reads the ghOutput colors live -- a repaint is all
    ' a theme switch needs.
    InvalidateRect( HWND_FRMOUTPUT_VSCROLL, null, true )

    ' The listview panes' row/header painters read the ghOutput colors live, so a theme
    ' switch only needs the background and scrollbar colors re-pushed plus a repaint.
    for i as long = 0 to 2
        select case i
            case 0 : hCtl = HWND_FRMOUTPUT_LVRESULTS
            case 1 : hCtl = HWND_FRMOUTPUT_LVSEARCH
            case 2 : hCtl = HWND_FRMOUTPUT_LVTODO
        end select
        CListBox_SetBackColor( hCtl, ghOutput.BackColor )
        CListBox_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, _
                                     ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )
        AfxRedrawWindow( hCtl )
    next

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
    
    ' ------------------------------------------------------------------------------------
    ' The tab strip: a CSelectBar for the five tabs, and a one-item CIconPanel for the "X".
    ' Both paint their own backgrounds and abut, so no container window is needed.
    ' ------------------------------------------------------------------------------------
    HWND_FRMOUTPUT_SELECTBAR = CSelectBar_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TABS )
        CSelectBar_SetFont( HWND_FRMOUTPUT_SELECTBAR, ghFont(GUIFONT_9) )
        CSelectBar_SetPaintPanelCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarPaint )
        CSelectBar_SetSelChangeCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarSelChange )
        CSelectBar_SetMessageCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarMessage )
        CSelectBar_SetIndicator( HWND_FRMOUTPUT_SELECTBAR, 1, 12 )
        
        dim as CSELECTBAR_COLORS SelectBarColors 
        CSelectBar_GetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )
        SelectBarColors.BackColor = ghOutput.BackColorHot 
        CSelectBar_SetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )

        ' Panels are added ONCE, in the order the config file's saved index refers to (the
        ' OUTPUT_TAB_* constants). The captions are pushed by frmOutput_SetTabCaptions,
        ' which also re-pushes them after a language change.
        for i as long = 0 to 4
            CSelectBar_AddPanel( HWND_FRMOUTPUT_SELECTBAR, "", i )
        next
        frmOutput_SetTabCaptions()
        ' Config load happens long before this control exists, so the persisted index is
        ' applied HERE. SetCurSel is silent, so this cannot fire the change callback.
        CSelectBar_SetCurSel( HWND_FRMOUTPUT_SELECTBAR, gConfig.ShowOutputPanelIndex )

    HWND_FRMOUTPUT_CLOSE = CIconPanel_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_BTNCLOSE )
        CIconPanel_SetFont( HWND_FRMOUTPUT_CLOSE, ghFont(SYMBOLFONT_10) )
        CIconPanel_SetJustify( HWND_FRMOUTPUT_CLOSE, IP_JUSTIFY_CENTER )
        CIconPanel_SetIconSize( HWND_FRMOUTPUT_CLOSE, pWindow->ScaleX(20), pWindow->ScaleY(20) )
        CIconPanel_SetPaintItemCallback( HWND_FRMOUTPUT_CLOSE, @frmOutput_ClosePaint )
        CIconPanel_SetClickCallback( HWND_FRMOUTPUT_CLOSE, @frmOutput_CloseClick )
        CIconPanel_AddItem( HWND_FRMOUTPUT_CLOSE, IP_KIND_COMMAND, wszIconClose, IDC_FRMOUTPUT_BTNCLOSE )
        CIconPanel_SetTooltipText( HWND_FRMOUTPUT_CLOSE, 0, L(161, "Close") )

        dim as CICONPANEL_COLORS IconPanelColors 
        CIconPanel_GetColors( HWND_FRMOUTPUT_CLOSE, @IconPanelColors )
        IconPanelColors.BackColor = ghOutput.BackColorHot 
        CIconPanel_SetColors( HWND_FRMOUTPUT_CLOSE, @IconPanelColors )

    ' Compiler Log File: multiline read-only CTextBox (borderless; the 20px indent the
    ' old code hacked in with EM_SETRECT is now a plain left margin). Wheel scrolling
    ' and the custom vscroll thumb both ride the control's scroll API.
    hCtl = CTextBox_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE, true )
        CTextBox_SetFont( hCtl, ghFont(GUIFONT_9) )
        CTextBox_SetBorderWidth( hCtl, 0 )
        CTextBox_SetMargins( hCtl, pWindow->ScaleX(20), 0 )
        CTextBox_SetReadOnly( hCtl, true )
        CTextBox_SetScrollChangedCallback( hCtl, @frmOutput_TextBoxScrollChanged )
        CTextBox_SetMenuText( hCtl, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )
        ThemeTextBoxContextMenu( hCtl )

    ' Notes: multiline editable CTextBox. User edits are saved through the
    ' ChangeCallback (programmatic SetText -- project switches -- stays silent).
    hCtl = CTextBox_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES, true )
        CTextBox_SetFont( hCtl, ghFont(GUIFONT_9) )
        CTextBox_SetBorderWidth( hCtl, 0 )
        CTextBox_SetMargins( hCtl, pWindow->ScaleX(20), 0 )
        CTextBox_SetChangeCallback( hCtl, @frmOutput_NotesChanged )
        CTextBox_SetScrollChangedCallback( hCtl, @frmOutput_TextBoxScrollChanged )
        CTextBox_SetMenuText( hCtl, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )
        ThemeTextBoxContextMenu( hCtl )
        frmOutput_ShowNotes()

    ' The three listview panes: CListBox instances with a CColumnHeader band. All three
    ' have identical columns, so frmOutput_CreateListView builds one and is called thrice.
    ' The Description column is the FILL column -- it absorbs whatever width is left, which
    ' is what the old hard-coded ScaleX(1480) was groping at.
    HWND_FRMOUTPUT_LVRESULTS = frmOutput_CreateListView( IDC_FRMOUTPUT_LVRESULTS )
    HWND_FRMOUTPUT_LVSEARCH  = frmOutput_CreateListView( IDC_FRMOUTPUT_LVSEARCH )
    HWND_FRMOUTPUT_LVTODO    = frmOutput_CreateListView( IDC_FRMOUTPUT_LVTODO )

    ' Shared vertical scrollbar for the two textboxes: the reusable CVScrollBar control,
    ' driven in LINE units (SetRange from the ScrollChangedCallback, ScrollCallback back
    ' into ScrollToLine). It owns its drag/paging/auto-repeat interaction itself; the
    ' paint callback keeps the old flat-track visuals.
    HWND_FRMOUTPUT_VSCROLL = CVScrollBar_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_VSCROLL )
    CVScrollBar_SetScrollCallback( HWND_FRMOUTPUT_VSCROLL, @frmOutput_VScrollCallback )
    CVScrollBar_SetPaintCallback( HWND_FRMOUTPUT_VSCROLL, @frmOutput_VScrollPaint )

    frmOutput_ResetAllControls
    frmOutput_SetControlColors
    frmOutput_PositionWindows
    
    function = 0

end function

