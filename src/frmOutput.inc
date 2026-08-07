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
' The ONLY two places that convert between a PsSelectBar panel INDEX and an OUTPUT_TAB_* ID.
' They stopped being the same number when the Search Results tab was removed: the ids are
' persisted in settings.ini and therefore cannot be renumbered, so id 2 is now a hole and
' OUTPUT_TAB_TODO (3) / OUTPUT_TAB_NOTES (4) sit on panel indices 2 / 3. See frmOutput.bi.
' ========================================================================================
function frmOutput_GetCurTab() as long
    if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) = 0 then return OUTPUT_TAB_RESULTS
    ' GetPanelID answers 0 (= OUTPUT_TAB_RESULTS) for an invalid index, which is the
    ' fallback we would have written by hand anyway.
    return PsSelectBar_GetPanelID( HWND_FRMOUTPUT_SELECTBAR, _
                                   PsSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR ) )
end function

function frmOutput_SetCurTab( byval nTabID as long ) as boolean
    if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) = 0 then return false
    dim as long idx = PsSelectBar_FindPanelByID( HWND_FRMOUTPUT_SELECTBAR, nTabID )
    ' A retired or garbage id heals to Compiler Results rather than being refused. This is
    ' the path a settings.ini still holding the deleted Search Results tab (id 2) takes.
    if idx = -1 then idx = PsSelectBar_FindPanelByID( HWND_FRMOUTPUT_SELECTBAR, OUTPUT_TAB_RESULTS )
    if idx = -1 then return false
    return PsSelectBar_SetCurSel( HWND_FRMOUTPUT_SELECTBAR, idx )
end function


' ========================================================================================
' A double-click on either listview pane jumps the editor to that row's file/line. This
' replaces the MSG_USER_LISTVIEW_LBUTTONDBLCLK round trip the old control used to post up
' to this form.
' ========================================================================================
function frmOutput_LVMessage( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean
    ' m->hList is the inner surface; the PsListTree container is its parent, and that is what
    ' SetDocumentErrorPosition and the context menu query.
    dim as HWND hList = GetParent(m->hList)

    select case m->uMsg
        case WM_LBUTTONDBLCLK
            SetDocumentErrorPosition( hList )
            return false      ' let the listbox keep its own default handling

        case WM_RBUTTONDOWN
            ' Compiler Results only -- both panes share this callback.
            if hList <> HWND_FRMOUTPUT_LVRESULTS then return false

            ' The control does NOT touch the selection on a right-click (it applies click
            ' selection only for the LEFT button), so the host does the usual thing: a
            ' press on a row already in the selection LEAVES the selection alone -- which
            ' is the whole point when the user is right-clicking a multi-row block to copy
            ' it -- and a press on any other row selects just that one first.
            if (m->idx >= 0) andalso (PsListTree_GetSel( hList, m->idx ) = false) then
                PsListTree_SetCurSel( hList, m->idx )
                PsListTree_Refresh( hList )
            end if

            ShowOutputResultsContextMenu()
            return true       ' claimed
    end select

    return false
end function


' ========================================================================================
' Header painter for the Output panes. A copy of ListBox_LVPaintHeader with the hot
' branch removed: these headers do not hot-track, so neither a column under the cursor nor
' a hovered divider changes the fill. frmDebug's Variables pane still uses the shared
' painter and still hot-tracks -- do not "unify" these two without deciding that as well.
' ========================================================================================
sub frmOutput_LVPaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( theme.output.text.backcolor )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += pWindow->ScaleX(4)
    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' A one-pixel divider on the trailing edge, so columns read as separated.
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.output.scrollbardivider )
    p->b->PaintRect( @rcDiv )
end sub


' ========================================================================================
' No tooltips on the Output headers. Unset, PsColumnHeader falls back to the column's own
' caption, i.e. a tip repeating the text already under the cursor -- returning "" is the
' documented opt-out.
' ========================================================================================
function frmOutput_HeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' Build one of the Output listview panes. They are identical apart from their control id,
' so this is called once each rather than copied per pane.
' ========================================================================================
function frmOutput_CreateListView( byval CtrlID as long ) as HWND
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then return 0

    dim as HWND hCtl = PsListTree_Create( HWND_FRMOUTPUT, CtrlID )

    ' Columns, in the order the chr(9)-delimited rows are built: spacer, Line, File,
    ' Description. Description is the fill column.
    PsListTree_AddColumn( hCtl, "",                  pWindow->ScaleX(20) )
    PsListTree_AddColumn( hCtl, L(200, "Line"),      pWindow->ScaleX(75) )
    PsListTree_AddColumn( hCtl, L(201, "File"),      pWindow->ScaleX(250) )
    PsListTree_AddColumn( hCtl, L(202, "Description"), pWindow->ScaleX(250) )
    PsListTree_SetFillColumn( hCtl, 3 )
    PsListTree_ShowHeader( hCtl, true )

    PsListTree_SetPaintCallback( hCtl, @ListBox_LVPaintRow )
    PsListTree_SetHeaderPaintCallback( hCtl, @frmOutput_LVPaintHeader )
    PsListTree_SetHeaderTooltipCallback( hCtl, @frmOutput_HeaderTooltip )
    ' PsTooltip, matching the rest of the application. WHICH tips appear is unchanged: with
    ' no row callback PsListTree still falls back to the row's own column-0 text, so the
    ' compiler-result and TODO rows keep the tip they have always had -- this changes only
    ' what draws it.
    PsListTree_SetTooltipMode( hCtl, PSTIP_MODE_PS )
    PsListTree_SetColumnAutoSizeCallback( hCtl, @ListBox_LVAutoSizeColumn )
    PsListTree_SetMessageCallback( hCtl, @frmOutput_LVMessage )

    ' Compiler Results only: Shift ranges / Ctrl toggles, so the right-click "Copy" has a
    ' multi-row block to work with. TODO stays single-select -- its rows are a navigation
    ' target, not a body of text, and every click there is a jump.
    if CtrlID = IDC_FRMOUTPUT_LVRESULTS then
        PsListTree_SetExtendedSelect( hCtl, true )
    end if

    PsListTree_SetFont( hCtl, ghFont(GUIFONT_9) )
    PsListTree_SetBackColor( hCtl, theme.output.text.backcolor )
    PsListTree_SetScrollBarColors( hCtl, theme.output.scrollbar.backcolor, _
                                 theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )

    return hCtl
end function


' ========================================================================================
' Clear data from all of the controls in the frmOutput windows. This is needed
' when Projects are loaded and closed.
' ========================================================================================
function frmOutput_ResetAllControls() as long
    PsListTree_Clear( HWND_FRMOUTPUT_LVRESULTS )
    AfxSetWindowText( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), "" )
    PsListTree_Clear( HWND_FRMOUTPUT_LVTODO )
    AfxSetWindowText( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), "" )
    function = 0
end function


' ========================================================================================
' Ensure that the correct notes are shown
' ========================================================================================
function frmOutput_ShowNotes() as long 

    dim hCtl as HWND = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)

    AfxSetWindowText(hCtl, *gApp.ProjectNotes.Wz())

    function = 0
end function


' ========================================================================================
' Update the TODO listview
' ========================================================================================
function frmOutput_UpdateToDoListview() as long
    if gApp.IsProjectLoading then exit function

    dim as hwnd hLV = HWND_FRMOUTPUT_LVTODO
    PsListTree_Clear( hLV )

    dim as DWSTRING wszText

    ' The store is fed by the scan worker's line scan of each buffer-scanned
    ' document (see clsSymbolDb.bi). Line numbers are 1-based, which is what
    ' the click-through (SetDocumentErrorPosition) expects in this column.
    for i as long = 0 to gTodoCount - 1
        wszText = _
            "" & chr(9) & _
            ltrim(wstr(gTodoItems(i).nLineNum)) & chr(9) & _
            PsLTrim(gTodoItems(i).wszFile) & chr(9) & _
            PsLTrim(gTodoItems(i).wszText)
        ListBox_AddTabbedRow( hLV, wszText )
    next

    frmOutput_PositionWindows

    function = 0
end function

' (frmOutput_UpdateSearchListview and the Search Results pane it filled are gone. Project
'  search results are the Find in Project tab's now -- an editable Scintilla excerpt per
'  hit, not a row of text -- so there is no second place for them to live. It had already
'  been dead code for some time: its only caller went with the old frmFindInFiles engine.)


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
    ShowWindow( HWND_FRMOUTPUT_UNDOCK, SW_SHOW )
    ' The "X" is the one strip control that is NOT unconditionally shown: it hides while the
    ' panel is floating, where "close" has no meaning in a window that already carries its
    ' own caption X. This runs at the end of every layout pass, so showing it here
    ' unconditionally would undo frmOutput_UpdateDockIcon a few lines after it ran.
    ShowWindow( HWND_FRMOUTPUT_CLOSE, iif( frmOutputFloat_IsFloating(), SW_HIDE, SW_SHOW ) )
    AfxRedrawWindow( HWND_FRMOUTPUT_SELECTBAR )
    
    ' By default, hide all controls
    ShowWindow( HWND_FRMOUTPUT_LVRESULTS, SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_LVTODO, SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE), SW_HIDE )
    ShowWindow( GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTNOTES), SW_HIDE )
    ShowWindow( HWND_FRMOUTPUT_VSCROLL, SW_HIDE )
    
    ' Switch on the panel ID, not the index. This used to read the raw GetCurSel and compare
    ' it against bare literals 0..4 -- which meant it was invisible to a grep for
    ' OUTPUT_TAB_ and would have silently mapped the wrong pane once the Search Results tab
    ' was removed and the indices shifted under it.
    select case frmOutput_GetCurTab()
        case OUTPUT_TAB_RESULTS
            hLV = HWND_FRMOUTPUT_LVRESULTS
            hCtrl = hLV

        case OUTPUT_TAB_LOGFILE
            ' (The old EM_SETRECT 20px-indent hack is gone: the PsTextBox carries a
            ' permanent left margin instead, set at creation.)
            hTextBox = GetDlgItem(hwnd, IDC_FRMOUTPUT_TXTLOGFILE)
            hCtrl = hTextBox
            dim as DWSTRING wszText = PsLTrim(WindowText(hTextBox))
            if wszText = "" then
                AfxSetWindowText(hTextBox, *L(92, "No compiler log file exists").Wz() )
            end if

        case OUTPUT_TAB_TODO
            hLV = HWND_FRMOUTPUT_LVTODO
            hCtrl = hLV

        case OUTPUT_TAB_NOTES
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
        ' WM_DRAWITEM never fired and the area rendered white. PsListTree answers
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
    
    ' The tab strip: PsSelectBar on the left, then the one-item PsIconPanels hard against the
    ' right -- the undock icon and, when docked, the "X" outside it. They abut, and each
    ' paints its own background, so together they cover the strip: there is no container
    ' window behind them.
    '
    ' The icon cells are reserved per VISIBLE control, so hiding the "X" while floating hands
    ' its width back to the tab bar rather than leaving a gap.
    nTabsHeight = pWindow->ScaleY(OUTPUT_TABS_HEIGHT)
    dim as long nCellWidth  = pWindow->ScaleX(OUTPUT_CLOSE_WIDTH)
    dim as long nStripWidth = rc.right - rc.left
    ' The "X" hides while floating -- a window that carries its own caption X has no use for
    ' a second one. That is asked of the DOCK STATE directly rather than read back out of the
    ' button's WS_VISIBLE bit, which is what it used to do: the bit is set by
    ' frmOutput_UpdateDockIcon, and a layout pass can run between the dock state flipping and
    ' that sub catching up -- measured, it does -- so the strip was laid out for a button
    ' whose visibility was one pass stale.
    '
    ' (The old comment about reading GWL_STYLE rather than IsWindowVisible was right about
    ' IsWindowVisible: it walks the ancestor chain, so with the panel simply closed every
    ' child reads as invisible. Asking the dock state sidesteps both hazards.)
    dim as boolean bCloseShown = (frmOutputFloat_IsFloating() = false)
    dim as long nIconsWidth = nCellWidth + iif( bCloseShown, nCellWidth, 0 )

    ' Laid out RIGHT TO LEFT. The "X" owns the rightmost cell whether or not it is currently
    ' shown, and the undock icon takes the cell to its left, or that same cell when there is
    ' no "X" beside it. Built the other way round -- an origin derived from how many icons
    ' are visible, then filled left to right -- the hidden "X" landed at exactly the strip's
    ' right edge and hung off the corner of the window, which is visible for as long as any
    ' pass shows it before the hide catches up.
    dim as long nCloseLeft  = nLeft + nStripWidth - nCellWidth
    dim as long nUndockLeft = nCloseLeft - iif( bCloseShown, nCellWidth, 0 )

    SetWindowPos( HWND_FRMOUTPUT_SELECTBAR, 0, _
                  nLeft, nTop, nStripWidth - nIconsWidth, nTabsHeight, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_UNDOCK, 0, _
                  nUndockLeft, nTop, nCellWidth, nTabsHeight, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_CLOSE, 0, _
                  nCloseLeft, nTop, nCellWidth, nTabsHeight, SWP_NOZORDER )

    if environ("TIKO_OUTPUTICONS_TRACE") = "1" then
        dim as string sT = "  ICONS floating=" & str(frmOutputFloat_IsFloating()) & _
            "  strip=" & str(nLeft) & ".." & str(nLeft + nStripWidth) & _
            "  cell=" & str(nCellWidth) & "  closeShown=" & str(bCloseShown) & _
            "  undockX=" & str(nUndockLeft) & "  closeX=" & str(nCloseLeft)
        print sT
        ' Read BACK from the windows, and in the FRAME's coordinates -- the numbers above are
        ' what was asked for, which is not evidence about where anything landed.
        dim as HWND hFrame = GetParent( HWND_FRMOUTPUT )
        if IsWindow(hFrame) then
            dim as RECT rcF, rcP, rcU, rcC
            GetClientRect( hFrame, @rcF )
            GetWindowRect( HWND_FRMOUTPUT, @rcP )
            GetWindowRect( HWND_FRMOUTPUT_UNDOCK, @rcU )
            GetWindowRect( HWND_FRMOUTPUT_CLOSE, @rcC )
            MapWindowPoints( HWND_DESKTOP, hFrame, cast(LPPOINT, @rcP), 2 )
            MapWindowPoints( HWND_DESKTOP, hFrame, cast(LPPOINT, @rcU), 2 )
            MapWindowPoints( HWND_DESKTOP, hFrame, cast(LPPOINT, @rcC), 2 )
            sT = "    parent client 0.." & str(rcF.right) & _
                 "   panel=" & str(rcP.left) & ".." & str(rcP.right) & _
                 "   undock=" & str(rcU.left) & ".." & str(rcU.right) & _
                 "   close=" & str(rcC.left) & ".." & str(rcC.right) & _
                 iif( (GetWindowLongPtr(HWND_FRMOUTPUT_CLOSE, GWL_STYLE) and WS_VISIBLE) <> 0, _
                      "  CLOSE-VISIBLE", "  close-hidden" )
            print sT
        end if
    end if

    ' Re-push the captions so a language change is picked up, exactly as the old code did by
    ' rebuilding them here every time -- but only when one actually differs. SetText
    ' re-measures and re-lays out the whole bar, so doing it unconditionally on every resize
    ' would be pure churn.
    frmOutput_SetTabCaptions()

    ' Position the child controls. The PsVScrollBar is created 0x0 (its width cannot be
    ' read back before the first positioning), so the width is computed, not queried --
    ' same 12px design width the old CPanelWindow scrollbar was created with.
    dim as long nVScrollWidth = pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR)
    nTop = nTop + nTabsHeight + pWindow->ScaleY(8)
    
    SetWindowPos( HWND_FRMOUTPUT_LVRESULTS, 0, nLeft, nTop, rc.right, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_LVTODO, 0, nLeft, nTop, rc.right, rc.bottom - nTop, SWP_NOZORDER )
    
    SetWindowPos( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE), 0, _
        nLeft, nTop, rc.right - nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), 0, _
        nLeft, nTop, rc.right - nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )
    SetWindowPos( HWND_FRMOUTPUT_VSCROLL, 0, _
        rc.right - nVScrollWidth, nTop, nVScrollWidth, rc.bottom - nTop, SWP_NOZORDER )

    ' (The ~60 lines of per-tab rect arithmetic that used to sit here are gone: PsSelectBar
    ' measures its own captions and derives every rect. So is the close-button rect -- the
    ' PsIconPanel owns that.)

    ' Determine which child controls should be shown or hidden
    frmOutput_ShowHideOutputControls(HWND_FRMOUTPUT)
    
    function = 0
end function


' ========================================================================================
' PsTextBox callbacks for the Log/Notes controls.
' ========================================================================================
' Notes changed by USER interaction. Save them to the correct global variable so the
' changes are not lost when documents or projects are switched. (Programmatic SetText
' during those switches is silent, so this can never echo a switch back into the
' wrong project's notes.)
sub frmOutput_NotesChanged( byval hTextBox as HWND )
    gApp.ProjectNotes = PsTextBox_GetText( hTextBox )
end sub

' Vertical scroll state changed (typing, programmatic SetText, wheel) -- push the fresh
' range straight into the PsVScrollBar, which re-derives its own thumb. Only the visible
' textbox drives the shared scrollbar (a hidden one is re-pushed on its tab switch).
' During a thumb drag the ScrollCallback -> ScrollToLine round trip echoes the same
' position back through here -- consistent by construction, no oscillation.
sub frmOutput_TextBoxScrollChanged( byval hTextBox as HWND )
    if IsWindowVisible( hTextBox ) = 0 then exit sub
    dim as integer nTotalLines, nLinesPerPage, nFirstVisible
    PsTextBox_GetVScrollInfo( hTextBox, nTotalLines, nLinesPerPage, nFirstVisible )
    PsVScrollBar_SetRange( HWND_FRMOUTPUT_VSCROLL, nTotalLines, nLinesPerPage, nFirstVisible )
end sub

' The user scrolled the PsVScrollBar (thumb drag or track paging): scroll whichever
' textbox is visible to that first line.
sub frmOutput_VScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    dim as HWND hTextBox
    if IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE)
    elseif IsWindowVisible( GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES) ) then
        hTextBox = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)
    end if
    if hTextBox then PsTextBox_ScrollToLine( hTextBox, newPos )
end sub

' Custom paint keeping the exact visuals of the old hand-rolled scrollbar (and of the
' listview scrollbars sharing the Output window): flat track, left divider line and a
' flat thumb when there is something to scroll, no hover brightening. Reads the theme
' colors live, so a theme switch only needs a repaint.
sub frmOutput_VScrollPaint( byval p as PSVSCROLLBAR_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b
    b->SetBackColor( theme.output.scrollbar.backcolor )
    b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        b->SetPenColor( theme.output.scrollbardivider )
        b->PaintLine( 1, p->rcClient.left, p->rcClient.top, p->rcClient.left, p->rcClient.bottom )
        b->SetBackColor( theme.output.scrollbar.forecolor )
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
' The Output panel's splitter bar. It is a PsSplitter owned by frmMain (the bar sits BETWEEN
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
sub frmOutput_SplitterPaint( byval p as PSSPLITTER_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b

    b->SetBackColor( theme.output.text.backcolor )
    b->PaintClientRect()

    dim as RECT rc = p->rcClient
    dim as long nMid = rc.top + ((rc.bottom - rc.top) \ 2)
    rc.top = nMid
    rc.bottom = nMid + AfxScaleY(1)

    ' Constant color -- deliberately no isHot/isDragging highlight. The cursor change
    ' from the control is the only hover feedback this bar gets.
    b->SetBackColor( theme.output.divider )
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
    ' Defensive: frmMain_PositionWindows hides the bar while the panel is floating, so this
    ' should be unreachable. It writes gConfig.ShowOutputPanelHeight from a window whose
    ' height is the float frame's client area, which would silently overwrite the banked
    ' docked height -- so the guard is cheap insurance against a stray in-flight drag.
    if frmOutputFloat_IsFloating() then exit sub

    select case nPhase
        case PSSPLITTER_PHASE_MOVE
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

        case PSSPLITTER_PHASE_END
            ' Persist the height the user settled on -- what HandleSingleClick did on the
            ' mouse-up. UnScaleY because gConfig holds this field unscaled at all times;
            ' AfxGetWindowHeight reports device pixels.
            gConfig.ShowOutputPanelHeight = _
                frmOutput_ClampHeight( pWindow->UnScaleY( AfxGetWindowHeight(HWND_FRMOUTPUT) ) )
    end select
end sub


' ========================================================================================
' Clamp a stored (unscaled) panel height into the legal range. See OUTPUT_PANEL_MIN_HEIGHT
' for why the floor sits above the tabs row rather than on it.
' ========================================================================================
function frmOutput_ClampHeight( byval nHeight as long ) as long
    if nHeight < OUTPUT_PANEL_MIN_HEIGHT then return OUTPUT_TABS_HEIGHT * 5
    function = nHeight
end function


' ========================================================================================
' Refresh every persisted Output-panel field from the live windows.
'
' This exists because those four fields used to be refreshed at three DIFFERENT times --
' visibility and tab index only in frmMain_OnClose, minimized live, height live-but-scaled.
' Any of the ten SaveConfigFile call sites that was not the final one therefore wrote a
' blend of two sessions, which is what made the panel jump after an abnormal exit. One
' function, called from SaveConfigFile, is what makes every save path coherent.
' ========================================================================================
sub frmOutput_CaptureState()
    ' SaveConfigFile can run before the panel exists (and during shutdown, after it has
    ' gone). Leave the loaded values untouched in that case rather than writing zeros.
    if IsWindow( HWND_FRMOUTPUT ) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMOUTPUT )
    if pWindow = 0 then exit sub

    ' FLOATING: the panel is a child of the float frame, so BOTH captures below would be
    ' wrong -- its height is the frame's client height, not the user's docked height, and its
    ' IsWindowVisible is always TRUE because the FRAME carries visibility while floating. So
    ' the frame answers the visibility question, its own geometry fields carry its size, and
    ' the docked height/minimized pair is left exactly as it was banked at undock time.
    if frmOutputFloat_IsFloating() then
        gConfig.OutputFloating  = 1
        gConfig.ShowOutputPanel = IsWindowVisible( HWND_FRMOUTPUTFLOAT )
        frmOutputFloat_CaptureState()
        if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) then
            gConfig.ShowOutputPanelIndex = frmOutput_GetCurTab()
        end if
        exit sub
    end if
    gConfig.OutputFloating = 0

    gConfig.ShowOutputPanel = IsWindowVisible( HWND_FRMOUTPUT )

    ' Only a panel that is BOTH visible and not minimized is showing the user's chosen
    ' height; in any other state the live window height is the tabs row (or stale), so the
    ' stored value is the thing to keep.
    if gConfig.ShowOutputPanel andalso (gConfig.ShowOutputPanelMinimized = 0) then
        gConfig.ShowOutputPanelHeight = _
            pWindow->UnScaleY( AfxGetWindowHeight( HWND_FRMOUTPUT ) )
    end if
    gConfig.ShowOutputPanelHeight = frmOutput_ClampHeight( gConfig.ShowOutputPanelHeight )

    ' The tab bar owns the current tab; the config field is only its persisted copy. What is
    ' persisted is the tab's ID, never its panel index -- the two stopped agreeing when the
    ' Search Results tab was removed, and it is the id that an existing settings.ini holds.
    if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) then
        gConfig.ShowOutputPanelIndex = frmOutput_GetCurTab()
    end if
end sub


' Double-click toggles the panel between minimized and its user-set height -- the same
' toggle the Output TABS already perform. (It used to hide the panel outright; the View
' menu item still does that.) Claiming the message means no drag starts.
function frmOutput_SplitterMessage( byval m as PSSPLITTER_MESSAGEINFO ptr ) as boolean
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
' got picked up; this preserves that without the churn, because PsSelectBar_SetText
' re-measures and re-lays out the whole bar on every call.
' ========================================================================================
sub frmOutput_SetTabCaptions()
    if IsWindow(HWND_FRMOUTPUT_SELECTBAR) = 0 then exit sub

    ' Subscripted by TAB ID, so the retired id 2 is simply an empty slot that no panel ever
    ' asks for. The loop walks panel INDICES and looks each one's id up -- it must not run to
    ' a hardcoded 4: this sub is called from frmOutput_PositionWindows on every layout pass,
    ' so a loop overrunning the panel count would call SetText (which re-measures and
    ' re-lays out the whole bar) forever, silently, on the hot path.
    dim as DWSTRING wszCaption(OUTPUT_TAB_RESULTS to OUTPUT_TAB_NOTES)
    wszCaption(OUTPUT_TAB_RESULTS) = PsUCase(L(153, "Compiler Results"))
    wszCaption(OUTPUT_TAB_LOGFILE) = PsUCase(L(199, "Compiler Log File"))
    wszCaption(OUTPUT_TAB_TODO)    = PsUCase(L(204, "TODO"))
    wszCaption(OUTPUT_TAB_NOTES)   = PsUCase(L(205, "Notes"))

    for i as long = 0 to PsSelectBar_GetCount( HWND_FRMOUTPUT_SELECTBAR ) - 1
        dim as long nTabID = PsSelectBar_GetPanelID( HWND_FRMOUTPUT_SELECTBAR, i )
        if (nTabID < lbound(wszCaption)) orelse (nTabID > ubound(wszCaption)) then continue for
        if PsSelectBar_GetText( HWND_FRMOUTPUT_SELECTBAR, i ) <> wszCaption(nTabID) then
            PsSelectBar_SetText( HWND_FRMOUTPUT_SELECTBAR, i, wszCaption(nTabID) )
        end if
    next
end sub


' ========================================================================================
' Minimize the Output window
' Result of double clicking an Output panel tab or clicking the "X' close icon.
' ========================================================================================
function frmOutput_MinimizePanel() as long
    ' Meaningless while floating: the panel fills its own frame, so shrinking it to the tabs
    ' row would leave a mostly empty window rather than a collapsed strip. The frame's own
    ' minimize box is what minimizes a floating panel.
    if frmOutputFloat_IsFloating() then exit function
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
    ' FLOATING: "make sure the panel is showing" means show and raise the FRAME. This is the
    ' path a failed build takes (modCompileErrors), so without it a compile error would
    ' surface nothing at all whenever the panel happened to be undocked and behind the
    ' editor -- the silent failure this whole branch exists to prevent.
    if frmOutputFloat_IsFloating() then
        frmOutputFloat_SetVisible( true )
        exit function
    end if

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMOUTPUT)
    if pWindow = 0 then exit function

    gConfig.ShowOutputPanelMinimized = 0

    dim as long nWidth = AfxGetWindowWidth(HWND_FRMOUTPUT)

    ' Repair a degenerate stored height, then restore to THAT -- the old code repaired the
    ' config and then sized the window to the minimized height anyway, so restoring from a
    ' degenerate value left the panel looking minimized while the flag said restored.
    gConfig.ShowOutputPanelHeight = frmOutput_ClampHeight( gConfig.ShowOutputPanelHeight )
    dim as long nHeight = pWindow->ScaleY( gConfig.ShowOutputPanelHeight )

    ' Ensure that window is visible. Compile or Find results need a visible
    ' window in order to display results.
    AfxSetWindowSize(HWND_FRMOUTPUT, nWidth, nHeight)
    ShowWindow( HWND_FRMOUTPUT, SW_SHOW)
    frmMain_PositionWindows()
    
    function = 0
end function


' ========================================================================================
' Tab strip callbacks. The strip is a PsSelectBar (four tabs) plus a one-item PsIconPanel
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

' The panel's class background brush -- see where it is created in frmOutput_Show.
dim shared as HBRUSH ghOutputPanelBrush = 0


' ----------------------------------------------------------------------------------------
' Paint one tab. Reads the ghOutput theme colors LIVE, so a theme switch needs only a
' repaint and no color-pushing plumbing anywhere -- the same pattern
' frmEditorHScroll_PaintCallback uses. The selected tab draws hot (fore AND back), which is
' what the hand-drawn strip did.
' ----------------------------------------------------------------------------------------
sub frmOutput_SelectBarPaint( byval p as PSSELECTBAR_PAINTINFO ptr )
    dim as boolean isHotTab = p->isSelected orelse p->isHot orelse p->isPressed

    p->b->SetForeColor( iif( isHotTab, theme.output.texthot.forecolor, theme.output.text.forecolor ) )
    p->b->SetBackColor( iif( isHotTab, theme.output.texthot.backcolor, theme.output.text.backcolor ) )
    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_CENTER )

    ' The indicator under the current tab. rcIndicator already spans exactly the caption
    ' text and sits the configured gap below it -- no arithmetic needed here.
    if p->isSelected then
        p->b->SetBackColor( theme.output.texthot.forecolor )
        p->b->PaintRect( @p->rcIndicator )
    end if
end sub


' ----------------------------------------------------------------------------------------
' The USER switched tabs. Programmatic PsSelectBar_SetCurSel is silent, so everything that
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
    ' idxNew is a panel INDEX; the comparison is against a tab ID.
    if PsSelectBar_GetPanelID( hCtl, idxNew ) = OUTPUT_TAB_NOTES then
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
function frmOutput_SelectBarMessage( byval m as PSSELECTBAR_MESSAGEINFO ptr ) as boolean
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

        case WM_RBUTTONDOWN
            ' The dock/undock menu, offered from the whole tab strip (and from both icon
            ' bands, through frmOutput_IconPanelMessage). Not from the four content panes:
            ' the results list and both textboxes already have menus of their own.
            ' PsPopupMenu is non-modal, so this returns immediately and the toggle runs from
            ' the select callback -- never synchronously from inside this button-down.
            ShowOutputDockContextMenu()
    end select
    ' Never suppress: the control needs its own default handling (and the result is ignored
    ' for WM_LBUTTONUP anyway -- it is what releases the mouse capture).
    return false
end function


' ----------------------------------------------------------------------------------------
' The close "X". Painted to match the old hand-drawn button exactly: a rounded rect that
' fills only on hover, then the glyph.
' ----------------------------------------------------------------------------------------
sub frmOutput_ClosePaint( byval p as PSICONPANEL_PAINTINFO ptr )
    p->b->SetBackColor( theme.output.text.backcolor )
    p->b->PaintRect( @p->rc )

    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( iif( p->isHot orelse p->isPressed, _
                             theme.output.closehot, theme.output.text.backcolor ) )
    p->b->PaintRoundRect( @p->rcIcon )

    p->b->SetFont( ghFont(SYMBOLFONT_10) )
    p->b->PaintText( p->wszGlyph, @p->rcIcon, DT_CENTER )
end sub


' Both icon panels share this. The click callback fires AFTER PsIconPanel has released its
' capture, so undocking from here -- which reparents the control's own grandparent -- is safe.
sub frmOutput_CloseClick( byval hCtl as HWND, byval idx as long, byval id as long )
    select case id
        case IDC_FRMOUTPUT_BTNUNDOCK
            frmOutputFloat_Toggle()
        case IDC_FRMOUTPUT_BTNCLOSE
            OnCommand_ViewOutput()      ' toggle the Output window off
    end select
end sub


' ----------------------------------------------------------------------------------------
' Right-click on either icon band -> the dock/undock context menu. Shared with the tab
' strip, so the whole strip offers it and the four content panes keep the menus they have.
' Never suppress: the control needs its own default handling, and the result is ignored for
' WM_LBUTTONUP anyway (that is what releases the mouse capture).
' ----------------------------------------------------------------------------------------
function frmOutput_IconPanelMessage( byval m as PSICONPANEL_MESSAGEINFO ptr ) as boolean
    if m->uMsg = WM_RBUTTONDOWN then ShowOutputDockContextMenu()
    return false
end function


' ========================================================================================
' Point the undock icon at whichever direction is currently available, and show or hide the
' "X" beside it.
'
' While floating the "X" is hidden: "close" has no meaning in a window that already carries
' its own caption X, and the caption X is what re-docks. Hiding it hands its cell width back
' to the tab bar, which is why the re-layout at the end is not optional.
' ========================================================================================
sub frmOutput_UpdateDockIcon()
    if IsWindow( HWND_FRMOUTPUT_UNDOCK ) = 0 then exit sub
    dim as boolean bFloating = frmOutputFloat_IsFloating()

    ' Spelled out rather than folded into iif(): both arms are strings, and the family keeps
    ' iif to numerics.
    if bFloating then
        PsIconPanel_SetGlyph( HWND_FRMOUTPUT_UNDOCK, 0, wszIconDock )
        PsIconPanel_SetTooltipText( HWND_FRMOUTPUT_UNDOCK, 0, L(394, "Dock Output Window") )
    else
        PsIconPanel_SetGlyph( HWND_FRMOUTPUT_UNDOCK, 0, wszIconUndock )
        PsIconPanel_SetTooltipText( HWND_FRMOUTPUT_UNDOCK, 0, L(393, "Undock Output Window") )
    end if

    if IsWindow( HWND_FRMOUTPUT_CLOSE ) then
        ShowWindow( HWND_FRMOUTPUT_CLOSE, iif( bFloating, SW_HIDE, SW_SHOW ) )
    end if
    frmOutput_PositionWindows()
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
        ' The child listviews (and the PsVScrollBar) have also been destroyed in their
        ' respective WM_NCDESTROY so reset all of their window handles.
        HWND_FRMOUTPUT_LVRESULTS = 0
        HWND_FRMOUTPUT_LVTODO    = 0
        HWND_FRMOUTPUT_VSCROLL   = 0
        ' Ours to free: the system does NOT delete a class background brush at unregister --
        ' measured on the floating frame, which creates and destroys one per undock and
        ' leaked exactly one object per cycle without this. The panel is created once per
        ' session, so this is tidiness rather than a live leak, but the rule is the same.
        if ghOutputPanelBrush then
            DeleteObject( ghOutputPanelBrush )
            ghOutputPanelBrush = 0
        end if
        return 0


    ' MSG_USER_LISTVIEW_LBUTTONDBLCLK used to arrive here from the old listview's inner
    ' listbox subclass. The PsListTree panes report the double-click through their own
    ' message callback instead -- see frmOutput_LVMessage.

    case WM_ERASEBKGND
        return true

    case WM_PAINT            
        dim as PsBufferPaint b
        b.BeginDoubleBuffer(hwnd)
        b.SetBackColor( theme.output.texthot.backcolor )
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
    ' The class background brush is the one colour here that is NOT read live at paint time,
    ' so it is the one a theme change has to be pushed into. Left stale it would flash the
    ' OLD theme's colour on the next system erase -- a reparent, or a show after the panel
    ' had been closed. Rebuilt rather than tinted: a brush carries a fixed colour.
    dim pPanel as CWindow ptr = AfxCWindowPtr( HWND_FRMOUTPUT )
    if pPanel then
        dim as HBRUSH hOld = ghOutputPanelBrush
        ghOutputPanelBrush = CreateSolidBrush( theme.output.texthot.backcolor )
        pPanel->Brush = ghOutputPanelBrush
        ' Freed only AFTER the class has stopped pointing at it.
        if hOld then DeleteObject( hOld )
    end if
    if IsWindow( HWND_FRMOUTPUTFLOAT ) then frmOutputFloat_SyncBrush()

    dim as HWND hCtl
    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE )
    PsTextBox_SetForeColor( hCtl, theme.output.texthot.forecolor )
    PsTextBox_SetBackColor( hCtl, theme.output.text.backcolor )
    PsTextBox_SetOuterBackColor( hCtl, theme.output.text.backcolor )

    hCtl = GetDlgItem( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES )
    PsTextBox_SetForeColor( hCtl, theme.output.texthot.forecolor )
    PsTextBox_SetBackColor( hCtl, theme.output.text.backcolor )
    PsTextBox_SetOuterBackColor( hCtl, theme.output.text.backcolor )

    ' The scrollbar's paint callback reads the ghOutput colors live -- a repaint is all
    ' a theme switch needs.
    InvalidateRect( HWND_FRMOUTPUT_VSCROLL, null, true )

    ' The listview panes' row/header painters read the ghOutput colors live, so a theme
    ' switch only needs the background and scrollbar colors re-pushed plus a repaint.
    for i as long = 0 to 1
        select case i
            case 0 : hCtl = HWND_FRMOUTPUT_LVRESULTS
            case 1 : hCtl = HWND_FRMOUTPUT_LVTODO
        end select
        PsListTree_SetBackColor( hCtl, theme.output.text.backcolor )
        PsListTree_SetScrollBarColors( hCtl, theme.output.scrollbar.backcolor, _
                                     theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )
        AfxRedrawWindow( hCtl )
    next

    ' The tab strip. These three DO store their colour rather than reading it live, so a
    ' theme switch has to be pushed into them -- and until this ran they kept whichever
    ' theme was current when frmOutput_Show built them.
    '
    ' What that looked like: PsSelectBar_OnPaint fills the WHOLE bar with colors.BackColor,
    ' "which also covers the area to the right of the last panel". The tabs themselves are
    ' drawn by frmOutput_SelectBarPaint, which reads the theme live and so was always
    ' correct -- so switching dark -> light left the tabs light and the strip beyond the
    ' last tab still dark. The two icon panels hid the same staleness better, because
    ' frmOutput_ClosePaint paints each ITEM's background live; only the strip around the
    ' items showed it.
    if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) then
        dim as PSSELECTBAR_COLORS SelectBarColors
        PsSelectBar_GetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )
        SelectBarColors.BackColor = theme.output.texthot.backcolor
        PsSelectBar_SetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )
        AfxRedrawWindow( HWND_FRMOUTPUT_SELECTBAR )
    end if

    for i as long = 0 to 1
        select case i
            case 0 : hCtl = HWND_FRMOUTPUT_UNDOCK
            case 1 : hCtl = HWND_FRMOUTPUT_CLOSE
        end select
        if IsWindow( hCtl ) then
            dim as PSICONPANEL_COLORS IconPanelColors
            PsIconPanel_GetColors( hCtl, @IconPanelColors )
            IconPanelColors.BackColor = theme.output.texthot.backcolor
            PsIconPanel_SetColors( hCtl, @IconPanelColors )
            AfxRedrawWindow( hCtl )
        end if
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
    
    ' Set the height of the window to the last used visible height. The clamp is what heals
    ' a settings.ini written by an older build, whose height could be ratcheted up by the
    ' DPI factor on every launch or divided down by a cancelled exit.
    gConfig.ShowOutputPanelHeight = frmOutput_ClampHeight( gConfig.ShowOutputPanelHeight )
    nHeight = gConfig.ShowOutputPanelHeight
    ' Witness for the post-Show half of the self-test. Creating the panel must leave the
    ' stored height ALONE -- mutating it here is the exact defect that made the panel grow
    ' by the DPI factor on every launch, and no pure function can see that happen.
    gOutPanelHeightAtShow = gConfig.ShowOutputPanelHeight

    ' If the Output window was in minimized state when use the default tabs height
    ' for showing the initial height of the window.
    if gConfig.ShowOutputPanelMinimized <> 0 then nHeight = OUTPUT_TABS_HEIGHT

    ' Only make the Output panel initially visible if it was already visible
    ' when the most previous instance of the program closed. 
    HWND_FRMOUTPUT = pWindow->Create( hwndParent, "", @frmOutput_WndProc, 0, 0, 0, nHeight, _
        WS_CHILD or iif(gConfig.ShowOutputPanel, WS_VISIBLE, 0) or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)
    pWindow->ClassStyle = CS_DBLCLKS

    ' The panel's class background brush, matched to what its own WM_PAINT draws.
    '
    ' CWindow registers with hbrBackground = COLOR_3DFACE+1, so every erase the SYSTEM
    ' performs -- at ShowWindow time, and again when the panel is reparented into the
    ' floating frame -- painted the client in the system 3D face colour first. WM_ERASEBKGND
    ' returning TRUE does not help there: those erases happen before the window procedure is
    ' reached. Undocking flashed white because of this and the frame's own brush together;
    ' the panel is the one covering the whole client, so it is the one that showed.
    ' The class is per-instance, so this affects nothing else. Freed in WM_NCDESTROY.
    ghOutputPanelBrush = CreateSolidBrush( theme.output.texthot.backcolor )
    pWindow->Brush = ghOutputPanelBrush

    ' NOTE: gConfig.ShowOutputPanelHeight is deliberately NOT scaled here. It stays in
    ' unscaled units for the whole life of the process and is scaled only where it is used
    ' (frmOutput_RestorePanel). The window is created at the unscaled height and gets its
    ' real size a moment later from frmMain_PositionWindows, which drives the layout off the
    ' live window rect rather than off this field.

    dim as HWND hCtl, hLV, hLB
    
    ' ------------------------------------------------------------------------------------
    ' The tab strip: a PsSelectBar for the four tabs, and a one-item PsIconPanel for the "X".
    ' Both paint their own backgrounds and abut, so no container window is needed.
    ' ------------------------------------------------------------------------------------
    HWND_FRMOUTPUT_SELECTBAR = PsSelectBar_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TABS )
        PsSelectBar_SetFont( HWND_FRMOUTPUT_SELECTBAR, ghFont(GUIFONT_9) )
        PsSelectBar_SetPaintPanelCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarPaint )
        PsSelectBar_SetSelChangeCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarSelChange )
        PsSelectBar_SetMessageCallback( HWND_FRMOUTPUT_SELECTBAR, @frmOutput_SelectBarMessage )
        PsSelectBar_SetIndicator( HWND_FRMOUTPUT_SELECTBAR, 1, 12 )
        
        dim as PSSELECTBAR_COLORS SelectBarColors 
        PsSelectBar_GetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )
        SelectBarColors.BackColor = theme.output.texthot.backcolor 
        PsSelectBar_SetColors( HWND_FRMOUTPUT_SELECTBAR, @SelectBarColors )

        ' Panels are added ONCE. The third argument is the panel's host ID -- an
        ' OUTPUT_TAB_* constant, and the value persisted in settings.ini. These are named
        ' explicitly rather than being a loop counter because the ids are no longer
        ' contiguous: id 2 (Search Results) is retired, so the ids run 0, 1, 3, 4 across
        ' panel indices 0, 1, 2, 3.
        PsSelectBar_AddPanel( HWND_FRMOUTPUT_SELECTBAR, "", OUTPUT_TAB_RESULTS )
        PsSelectBar_AddPanel( HWND_FRMOUTPUT_SELECTBAR, "", OUTPUT_TAB_LOGFILE )
        PsSelectBar_AddPanel( HWND_FRMOUTPUT_SELECTBAR, "", OUTPUT_TAB_TODO )
        PsSelectBar_AddPanel( HWND_FRMOUTPUT_SELECTBAR, "", OUTPUT_TAB_NOTES )
        frmOutput_SetTabCaptions()
        ' Config load happens long before this control exists, so the persisted tab is
        ' applied HERE. SetCurTab is silent, so this cannot fire the change callback, and it
        ' heals a stored id that no longer exists -- which is the path a settings.ini written
        ' before the Search Results tab was removed takes.
        frmOutput_SetCurTab( gConfig.ShowOutputPanelIndex )

    ' The undock icon, in its OWN PsIconPanel rather than as a second item in the one below.
    ' PsIconPanel is static by contract -- no hide/delete API, and that omission is the point
    ' of the control -- so a separate window is what lets the "X" be hidden with a plain
    ' ShowWindow while the panel is floating. Both paint the same background and abut, so the
    ' seam is invisible; frmPanelMenu's left/right split established the pattern.
    HWND_FRMOUTPUT_UNDOCK = PsIconPanel_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_BTNUNDOCK )
    ' PsTooltip rather than the system tip, so this panel matches the seven forms migrated on
    ' 2026-07-29 and picks up ghPopup's colours and the system tooltip FACE. Armed before any
    ' SetTooltipText: PsTipHost builds the tool on first use and a tip inherits the defaults
    ' current at that moment.
    PsIconPanel_SetTooltipMode( HWND_FRMOUTPUT_UNDOCK, PSTIP_MODE_PS )
        PsIconPanel_SetFont( HWND_FRMOUTPUT_UNDOCK, ghFont(SYMBOLFONT_10) )
        PsIconPanel_SetJustify( HWND_FRMOUTPUT_UNDOCK, IP_JUSTIFY_CENTER )
        PsIconPanel_SetIconSize( HWND_FRMOUTPUT_UNDOCK, pWindow->ScaleX(20), pWindow->ScaleY(20) )
        PsIconPanel_SetPaintItemCallback( HWND_FRMOUTPUT_UNDOCK, @frmOutput_ClosePaint )
        PsIconPanel_SetClickCallback( HWND_FRMOUTPUT_UNDOCK, @frmOutput_CloseClick )
        PsIconPanel_SetMessageCallback( HWND_FRMOUTPUT_UNDOCK, @frmOutput_IconPanelMessage )
        PsIconPanel_AddItem( HWND_FRMOUTPUT_UNDOCK, IP_KIND_COMMAND, wszIconUndock, IDC_FRMOUTPUT_BTNUNDOCK )

        dim as PSICONPANEL_COLORS UndockColors
        PsIconPanel_GetColors( HWND_FRMOUTPUT_UNDOCK, @UndockColors )
        UndockColors.BackColor = theme.output.texthot.backcolor
        PsIconPanel_SetColors( HWND_FRMOUTPUT_UNDOCK, @UndockColors )

    HWND_FRMOUTPUT_CLOSE = PsIconPanel_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_BTNCLOSE )
    PsIconPanel_SetTooltipMode( HWND_FRMOUTPUT_CLOSE, PSTIP_MODE_PS )
        PsIconPanel_SetFont( HWND_FRMOUTPUT_CLOSE, ghFont(SYMBOLFONT_10) )
        PsIconPanel_SetJustify( HWND_FRMOUTPUT_CLOSE, IP_JUSTIFY_CENTER )
        PsIconPanel_SetIconSize( HWND_FRMOUTPUT_CLOSE, pWindow->ScaleX(20), pWindow->ScaleY(20) )
        PsIconPanel_SetPaintItemCallback( HWND_FRMOUTPUT_CLOSE, @frmOutput_ClosePaint )
        PsIconPanel_SetClickCallback( HWND_FRMOUTPUT_CLOSE, @frmOutput_CloseClick )
        PsIconPanel_SetMessageCallback( HWND_FRMOUTPUT_CLOSE, @frmOutput_IconPanelMessage )
        PsIconPanel_AddItem( HWND_FRMOUTPUT_CLOSE, IP_KIND_COMMAND, wszIconClose, IDC_FRMOUTPUT_BTNCLOSE )
        PsIconPanel_SetTooltipText( HWND_FRMOUTPUT_CLOSE, 0, L(135, "Close") )

        dim as PSICONPANEL_COLORS IconPanelColors 
        PsIconPanel_GetColors( HWND_FRMOUTPUT_CLOSE, @IconPanelColors )
        IconPanelColors.BackColor = theme.output.texthot.backcolor 
        PsIconPanel_SetColors( HWND_FRMOUTPUT_CLOSE, @IconPanelColors )

    ' Compiler Log File: multiline read-only PsTextBox (borderless; the 20px indent the
    ' old code hacked in with EM_SETRECT is now a plain left margin). Wheel scrolling
    ' and the custom vscroll thumb both ride the control's scroll API.
    hCtl = PsTextBox_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTLOGFILE, true )
        PsTextBox_SetFont( hCtl, ghFont(GUIFONT_9) )
        PsTextBox_SetBorderWidth( hCtl, 0 )
        PsTextBox_SetMargins( hCtl, pWindow->ScaleX(20), 0 )
        PsTextBox_SetReadOnly( hCtl, true )
        PsTextBox_SetScrollChangedCallback( hCtl, @frmOutput_TextBoxScrollChanged )
        PsTextBox_SetMenuText( hCtl, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )
        ThemeTextBoxContextMenu( hCtl )

    ' Notes: multiline editable PsTextBox. User edits are saved through the
    ' ChangeCallback (programmatic SetText -- project switches -- stays silent).
    hCtl = PsTextBox_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES, true )
        PsTextBox_SetFont( hCtl, ghFont(GUIFONT_9) )
        PsTextBox_SetBorderWidth( hCtl, 0 )
        PsTextBox_SetMargins( hCtl, pWindow->ScaleX(20), 0 )
        PsTextBox_SetChangeCallback( hCtl, @frmOutput_NotesChanged )
        PsTextBox_SetScrollChangedCallback( hCtl, @frmOutput_TextBoxScrollChanged )
        PsTextBox_SetMenuText( hCtl, L(17,"Cut"), L(18,"Copy"), L(19,"Paste"), L(40,"Select All") )
        ThemeTextBoxContextMenu( hCtl )
        frmOutput_ShowNotes()

    ' The listview panes: PsListTree instances with a PsColumnHeader band. Both have
    ' identical columns, so frmOutput_CreateListView builds one and is called twice.
    ' The Description column is the FILL column -- it absorbs whatever width is left, which
    ' is what the old hard-coded ScaleX(1480) was groping at.
    HWND_FRMOUTPUT_LVRESULTS = frmOutput_CreateListView( IDC_FRMOUTPUT_LVRESULTS )
    HWND_FRMOUTPUT_LVTODO    = frmOutput_CreateListView( IDC_FRMOUTPUT_LVTODO )

    ' Shared vertical scrollbar for the two textboxes: the reusable PsVScrollBar control,
    ' driven in LINE units (SetRange from the ScrollChangedCallback, ScrollCallback back
    ' into ScrollToLine). It owns its drag/paging/auto-repeat interaction itself; the
    ' paint callback keeps the old flat-track visuals.
    HWND_FRMOUTPUT_VSCROLL = PsVScrollBar_Create( HWND_FRMOUTPUT, IDC_FRMOUTPUT_VSCROLL )
    PsVScrollBar_SetScrollCallback( HWND_FRMOUTPUT_VSCROLL, @frmOutput_VScrollCallback )
    PsVScrollBar_SetPaintCallback( HWND_FRMOUTPUT_VSCROLL, @frmOutput_VScrollPaint )

    frmOutput_ResetAllControls
    frmOutput_SetControlColors
    ' Point the undock icon the right way before the first layout pass -- it decides whether
    ' the "X" is shown, which decides how much width the tab bar gets.
    frmOutput_UpdateDockIcon
    frmOutput_PositionWindows
    
    function = 0

end function



' ========================================================================================
' Env-gated self-test: TIKO_OUTPUTPANEL_SELFTEST=1
'
' HEADLESS, and honest about its reach: it covers the CLAMP and the SCALED/UNSCALED round
' trip -- the two things that made the panel jump -- and nothing else. It cannot see
' frmOutput_CaptureState, which needs live windows, and it proves nothing about the panel
' rendering at the height it computed. The interactive pass is the evidence for those.
'
' The round-trip assertions are the point. They encode the invariant the old code broke:
' a height that goes to disk and comes back must be the SAME NUMBER, whatever the DPI.
' ========================================================================================
dim shared gOutPanelTestPass as long
dim shared gOutPanelTestFail as long

private sub frmOutput_Check( byval sName as string, byval bCond as boolean, byval sGot as string = "" )
    if bCond then
        gOutPanelTestPass += 1
    else
        gOutPanelTestFail += 1
        dim as string sLine = "  FAIL: " & sName
        if len(sGot) then sLine = sLine & "   got: " & sGot
        print sLine
    end if
end sub

sub frmOutput_RunSelfTest()
    if environ("TIKO_OUTPUTPANEL_SELFTEST") <> "1" then exit sub

    gOutPanelTestPass = 0
    gOutPanelTestFail = 0
    print "--- TIKO_OUTPUTPANEL_SELFTEST ---"

    ' --- the clamp ---------------------------------------------------------------------
    frmOutput_Check( "clamp: a normal height passes through untouched", _
        frmOutput_ClampHeight( 200 ) = 200, str(frmOutput_ClampHeight(200)) )
    frmOutput_Check( "clamp: the floor itself is legal", _
        frmOutput_ClampHeight( OUTPUT_PANEL_MIN_HEIGHT ) = OUTPUT_PANEL_MIN_HEIGHT )
    ' The whole reason the floor sits ABOVE the tabs row: a height equal to it is what
    ' "restore" used to land on, leaving the panel looking minimized.
    frmOutput_Check( "clamp: the tabs height is REFUSED, not accepted", _
        frmOutput_ClampHeight( OUTPUT_TABS_HEIGHT ) > OUTPUT_TABS_HEIGHT, _
        str(frmOutput_ClampHeight(OUTPUT_TABS_HEIGHT)) )
    frmOutput_Check( "clamp: a divided-down height is repaired", _
        frmOutput_ClampHeight( 11 ) = OUTPUT_TABS_HEIGHT * 5 )
    frmOutput_Check( "clamp: zero is repaired", _
        frmOutput_ClampHeight( 0 ) = OUTPUT_TABS_HEIGHT * 5 )
    frmOutput_Check( "clamp: a negative height is repaired", _
        frmOutput_ClampHeight( -50 ) = OUTPUT_TABS_HEIGHT * 5 )
    ' Not clamped from above: a legitimately tall panel on a tall monitor is the user's
    ' choice, and frmMain_PositionWindows already bounds it against the live client rect.
    frmOutput_Check( "clamp: a very tall height is NOT capped", _
        frmOutput_ClampHeight( 4000 ) = 4000 )
    frmOutput_Check( "clamp: is idempotent", _
        frmOutput_ClampHeight( frmOutput_ClampHeight( 5 ) ) = frmOutput_ClampHeight( 5 ) )

    ' --- the scaled/unscaled round trip --------------------------------------------------
    ' The defect this replaces: the live field was SCALED, the file documented UNSCALED, and
    ' only the final save unscaled it -- so any other save wrote a number that came back
    ' multiplied by the DPI factor, every launch. These run against the real frmMain CWindow
    ' so the DPI is whatever the machine is actually at, not an assumed 100%.
    dim pWindow as CWindow ptr = AfxCWindowPtr( HWND_FRMMAIN )
    if pWindow then
        dim as long nStored = 200
        dim as long nRoundTrip = pWindow->UnScaleY( pWindow->ScaleY( nStored ) )
        frmOutput_Check( "roundtrip: unscale(scale(h)) returns h", _
            nRoundTrip = nStored, str(nRoundTrip) )

        ' The ratchet, stated directly: scaling a value that is ALREADY scaled must not be
        ' what reaches the file. Above 100% these two differ, and that difference IS the bug.
        ' At exactly 100% they coincide and this assertion is vacuous -- said out loud
        ' rather than left as a flattering green.
        dim as long nOnce = pWindow->ScaleY( nStored )
        dim as long nTwice = pWindow->ScaleY( nOnce )
        if nOnce = nStored then
            print "  NOTE: running at 100% DPI -- the double-scale assertion is VACUOUS here."
        end if
        frmOutput_Check( "roundtrip: double scaling is detectable (>100% DPI only)", _
            iif( nOnce = nStored, true, nTwice > nOnce ), str(nTwice) & " vs " & str(nOnce) )

        ' What frmOutput_Show now does: clamp the stored value, and DO NOT mutate it.
        dim as long nCfg = frmOutput_ClampHeight( nStored )
        frmOutput_Check( "roundtrip: Show leaves the stored height unscaled", _
            nCfg = nStored, str(nCfg) )
    end if

    print "--- TIKO_OUTPUTPANEL_SELFTEST: " & str(gOutPanelTestPass) & " passed, " & _
          str(gOutPanelTestFail) & " failed ---"
end sub


' ========================================================================================
' Post-Show half of the suite. Runs after frmOutput_Show has created the panel.
'
' THIS IS THE HALF THAT GUARDS THE ACTUAL BUG. The pure half above stayed 11/11 green with
' the scale-up put straight back into frmOutput_Show -- measured, not assumed -- because it
' only ever reaches frmOutput_ClampHeight. An invariant about a field can only be asserted
' by a test that watches the field.
' ========================================================================================
sub frmOutput_RunPostShowSelfTest()
    if environ("TIKO_OUTPUTPANEL_SELFTEST") <> "1" then exit sub

    print "--- TIKO_OUTPUTPANEL_SELFTEST (post-Show) ---"

    frmOutput_Check( "Show: leaves the stored height UNSCALED (the ratchet)", _
        gConfig.ShowOutputPanelHeight = gOutPanelHeightAtShow, _
        str(gConfig.ShowOutputPanelHeight) & " vs " & str(gOutPanelHeightAtShow) )

    ' The stored height must stay a legal value across creation, whatever the panel's
    ' visibility or minimized state -- a minimized panel still remembers a real height.
    frmOutput_Check( "Show: the stored height is still legal", _
        gConfig.ShowOutputPanelHeight >= OUTPUT_PANEL_MIN_HEIGHT, _
        str(gConfig.ShowOutputPanelHeight) )

    ' CaptureState must be a no-op for the height while the panel is minimized or hidden:
    ' in those states the live window height is the tabs row, and capturing it would
    ' destroy the height the user chose. This is the invariant that lets CaptureState run
    ' on EVERY save rather than only on the way out.
    if IsWindow( HWND_FRMOUTPUT ) then
        dim as long nBefore = gConfig.ShowOutputPanelHeight
        dim as long nSavedMin = gConfig.ShowOutputPanelMinimized
        gConfig.ShowOutputPanelMinimized = 1
        frmOutput_CaptureState()
        frmOutput_Check( "CaptureState: a minimized panel does not overwrite the height", _
            gConfig.ShowOutputPanelHeight = nBefore, _
            str(gConfig.ShowOutputPanelHeight) & " vs " & str(nBefore) )
        gConfig.ShowOutputPanelMinimized = nSavedMin

        ' ...and it always reports visibility, which is the field that used to be refreshed
        ' only in frmMain_OnClose and so went stale on an abnormal exit.
        frmOutput_CaptureState()
        frmOutput_Check( "CaptureState: visibility tracks the live window", _
            (gConfig.ShowOutputPanel <> 0) = (IsWindowVisible( HWND_FRMOUTPUT ) <> 0) )
    end if

    ' --- tab ID vs panel INDEX ------------------------------------------------------------
    ' Removing the Search Results tab left a hole at id 2, so a tab's persisted ID and its
    ' panel index no longer agree. These assertions exist because getting that wrong is
    ' SILENT: a stored 3 would quietly select Notes instead of TODO, with no crash, no
    ' warning and nothing in the build to say so. They run against the real bar, and the
    ' current tab is saved and restored so the user's state is untouched.
    if IsWindow( HWND_FRMOUTPUT_SELECTBAR ) then
        dim as long nTabSaved = frmOutput_GetCurTab()

        frmOutput_Check( "tabs: four panels on the bar", _
            PsSelectBar_GetCount( HWND_FRMOUTPUT_SELECTBAR ) = 4, _
            str(PsSelectBar_GetCount( HWND_FRMOUTPUT_SELECTBAR )) )

        ' THE PREMISE OF THE WHOLE SCHEME. If these two are ever equal again the id
        ' indirection has become vacuous and every assertion below would pass for the wrong
        ' reason -- so assert the DISAGREEMENT directly rather than relying on it.
        dim as long nTodoIdx = PsSelectBar_FindPanelByID( HWND_FRMOUTPUT_SELECTBAR, OUTPUT_TAB_TODO )
        frmOutput_Check( "tabs: TODO's id is NOT its panel index (id 3, index 2)", _
            nTodoIdx <> OUTPUT_TAB_TODO, "index " & str(nTodoIdx) & " id " & str(OUTPUT_TAB_TODO) )

        ' The retired Search Results id. No panel carries it, which is what makes the heal
        ' in frmOutput_SetCurTab reachable rather than theoretical.
        frmOutput_Check( "tabs: the retired Search Results id (2) matches no panel", _
            PsSelectBar_FindPanelByID( HWND_FRMOUTPUT_SELECTBAR, 2 ) = -1 )

        ' Every surviving tab round-trips: set by id, read back the SAME id.
        dim as long nIDs(0 to 3) = { OUTPUT_TAB_RESULTS, OUTPUT_TAB_LOGFILE, _
                                     OUTPUT_TAB_TODO, OUTPUT_TAB_NOTES }
        for i as long = 0 to 3
            frmOutput_SetCurTab( nIDs(i) )
            frmOutput_Check( "tabs: id " & str(nIDs(i)) & " round-trips through SetCurTab", _
                frmOutput_GetCurTab() = nIDs(i), str(frmOutput_GetCurTab()) )
        next

        ' A settings.ini written before the Search Results tab was removed. It must land on
        ' Compiler Results, NOT on whatever panel happens to sit at index 2 (TODO).
        frmOutput_SetCurTab( 2 )
        frmOutput_Check( "tabs: a stored id of 2 heals to Compiler Results", _
            frmOutput_GetCurTab() = OUTPUT_TAB_RESULTS, str(frmOutput_GetCurTab()) )

        ' ...and a stored id of 3 still means TODO. This is the assertion that a renumber
        ' would have broken, and the reason the ids were left with a hole in them.
        frmOutput_SetCurTab( 3 )
        frmOutput_Check( "tabs: a stored id of 3 still selects TODO, not Notes", _
            frmOutput_GetCurTab() = OUTPUT_TAB_TODO, str(frmOutput_GetCurTab()) )

        frmOutput_SetCurTab( nTabSaved )
    end if

    print "--- TIKO_OUTPUTPANEL_SELFTEST: " & str(gOutPanelTestPass) & " passed, " & _
          str(gOutPanelTestFail) & " failed (both halves) ---"
end sub
