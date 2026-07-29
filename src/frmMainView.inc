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

#include once "frmMain.bi"
#include once "clsDocument.bi"


' ========================================================================================
' View fold actions
' ========================================================================================
function OnCommand_ViewFold( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
    
    select case id      
        case IDM_FOLDTOGGLE: pDoc->FoldToggle( pDoc->GetCurrentLineNumber() )
        case IDM_FOLDBELOW:  pDoc->FoldToggleOnwards( pDoc->GetCurrentLineNumber() )
        case IDM_FOLDALL:    pDoc->FoldAll()
        case IDM_UNFOLDALL:  pDoc->UnFoldAll()
    end select
    
    function = 0
end function

' ========================================================================================
' View zoom actions
' ========================================================================================
function OnCommand_ViewZoom( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla
    select case id      
        case IDM_ZOOMIN:    SciExec(hEdit, SCI_ZOOMIN, 0, 0)
        case IDM_ZOOMOUT:   SciExec(hEdit, SCI_ZOOMOUT, 0, 0)
        case IDM_ZOOMRESET: SciExec(hEdit, SCI_SETZOOM, 0, 0)   ' reset to zero zoom
    end select
    
    function = 0
end function

' ========================================================================================
' View toggle split editor
' ========================================================================================
function OnCommand_ViewSplit( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    pDoc->SplitX = 0
    pDoc->SplitY = 0

    select case id
        case IDM_SPLITLEFTRIGHT
            ' If already left/right then toggle it closed
            if pDoc->EditorSplitMode = SplitLeftRight then
                pDoc->EditorSplitMode = SplitNone
                pDoc->hWndActiveScintilla = pDoc->hWindow(0)
            else    
                ' If already split then make sure to cancel the current split and
                ' reposition windows before engaging new split in order to ensure 
                ' that window sizes are calculated correctly.
                pDoc->EditorSplitMode = SplitLeftRight
                frmMain_PositionWindows()
                dim as RECT rc = AfxGetWindowRect(pDoc->hWindow(0))
                MapWindowpoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 )
                pDoc->SplitX = rc.left + (rc.right - rc.left) / 2
                dim as long nTopLine = SciExec( pDoc->hWindow(0), SCI_GETFIRSTVISIBLELINE, 0, 0)  
                SciExec( pDoc->hWindow(1), SCI_SETFIRSTVISIBLELINE, nTopLine, 0) 
                dim as long currPos  = SciExec( pDoc->hWindow(0), SCI_GETCURRENTPOS, 0, 0)
                SciExec( pDoc->hWindow(1), SCI_GOTOPOS, currPos, 0) 
            end if

        case IDM_SPLITTOPBOTTOM
            ' If already top/bottom then toggle it closed
            if pDoc->EditorSplitMode = SplitTopBottom then
                pDoc->EditorSplitMode = SplitNone
                ' Get the number of visible lines in the Top split and then
                ' set the MAIN document's first top line to be the current
                ' MAIN editing line numebr less thhe top split number. This 
                ' makes it less visibly jarring. Switch the active scintilla
                ' to ensure that GetCurrentLineNumber gets the value from the
                ' correct document.
                pDoc->hWndActiveScintilla = pDoc->hWindow(0)
                dim as long nLines =  pDoc->LinesPerPage(1)
                dim as long nCurLine =  pDoc->GetCurrentLineNumber()
                SciExec( pDoc->hWindow(0), SCI_SETFIRSTVISIBLELINE, nCurLine - nLines, 0) 
            else    
                ' If already split then make sure to cancel the current split and
                ' reposition windows before engaging new split in order to ensure 
                ' that window sizes are calculated correctly.
                pDoc->EditorSplitMode = SplitTopBottom 
                frmMain_PositionWindows()
                dim as RECT rc = AfxGetWindowRect(pDoc->hWindow(0))
                MapWindowpoints( HWND_DESKTOP, HWND_FRMMAIN, cast(point ptr, @rc), 2 )
                pDoc->SplitY = (rc.bottom - rc.top) / 2
                dim as long nLines =  pDoc->LinesPerPage(0) / 2 - 3
                dim as long nCurLine =  pDoc->GetCurrentLineNumber()
                SciExec( pDoc->hWindow(1), SCI_SETFIRSTVISIBLELINE, nCurLine - nLines, 0) 
                dim as long currPos  = SciExec( pDoc->hWindow(0), SCI_GETCURRENTPOS, 0, 0)
                SciExec( pDoc->hWindow(1), SCI_GOTOPOS, currPos, 0) 
                SciExec( pDoc->hWindow(0), SCI_SETFIRSTVISIBLELINE, nCurLine, 0) 
            end if
    end select
    
    frmMain_PositionWindows()
    frmMain_SetFocusToCurrentCodeWindow()
            
    function = 0
end function


' ========================================================================================
' Toggle editing focus amongst open windows depending whether spilt in effect
' ========================================================================================
function OnCommand_ViewToggleFocus( byval pDoc as clsDocument ptr ) as LRESULT
    if pDoc = 0 then exit function
    
    dim hCurrent as HWND = pDoc->hWndActiveScintilla
    ' Focus lives on the PsTextBox's RichEdit child; use that handle for the focus
    ' comparisons and SetFocus below.
    dim hFindCtl as HWND = PsTextBox_GetRichEditHandle( GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND ) )
    if IsWindowVisible(HWND_FRMMAIN_TOPTABSINFO) = false then hFindCtl = 0
        
    dim idx as long
    
    dim hWin(2) as HWND
    hWin(0) = pDoc->hWindow(0): if GetFocus() = hWin(0) then idx = 0
    hWin(1) = pDoc->hWindow(1): if GetFocus() = hWin(1) then idx = 1
    hWin(2) = hFindCtl: if GetFocus() = hWin(2) then idx = 2
    
    do
        idx += 1
        if idx > ubound(hWin) then idx = lbound(hWin)
        hCurrent = hWin(idx)
    loop until IsWindowVisible(hCurrent)

    SetFocus(hCurrent)
    frmMain_SetStatusbar
    
    function = 0
end function


' ========================================================================================
' View SidePanelFunctionList, Explorer, Output, Bookmarks windows
' ========================================================================================
function OnCommand_ViewSidePanel() as LRESULT
    dim as long nPanelWidth = AfxGetWindowWidth(HWND_FRMPANEL)
    
    if IsWindowVisible(HWND_FRMPANEL) then
        ShowWindow( HWND_FRMPANEL, SW_HIDE )
        nPanelWidth = -nPanelWidth / 2
    else    
        ShowWindow( HWND_FRMPANEL, SW_SHOW )
        nPanelWidth = nPanelWidth / 2
    end if

    ' If document if left/right split then we adjust the SplitX
    ' position and redraw to update document widths.
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        if pDoc->EditorSplitMode = SplitLeftRight then
            pDoc->SplitX = pDoc->SplitX + nPanelWidth
        end if
    end if
    
    frmPanel_PositionWindows
    frmMain_PositionWindows
    
    function = 0
end function

function OnCommand_ViewExplorer() as LRESULT
    ShowWindow( HWND_FRMPANEL, SW_SHOW )
    gPanel.hActiveChild = HWND_FRMEXPLORER
    frmPanel_PositionWindows
    frmMain_PositionWindows
    function = 0
end function

function OnCommand_ViewFunctionList() as LRESULT
    ' If FunctionList is already active then toggle back to Explorer
    if gPanel.hActiveChild = HWND_FRMFUNCTIONS then
        return OnCommand_ViewExplorer()
    end if
    ShowWindow( HWND_FRMPANEL, SW_SHOW )
    gPanel.hActiveChild = HWND_FRMFUNCTIONS
    ' Rebuild from the symbol database. Scan completions only refresh this list
    ' while it is visible, so switching to the panel can otherwise land on a
    ' list that was built before the first scan finished (empty and unpainted).
    LoadFunctionsFiles()
    frmPanel_PositionWindows
    frmMain_PositionWindows
    function = 0
end function

function OnCommand_ViewBookmarksList() as LRESULT
    ' If BookmarksList is already active then toggle back to Explorer    
    if gPanel.hActiveChild = HWND_FRMBOOKMARKS then
        return OnCommand_ViewExplorer()
    end if
    ShowWindow( HWND_FRMPANEL, SW_SHOW )
    gPanel.hActiveChild = HWND_FRMBOOKMARKS
    frmPanel_PositionWindows
    frmMain_PositionWindows
    function = 0
end function

function OnCommand_ViewOutput() as LRESULT
    dim as long nShowFlag = iif(IsWindowVisible(HWND_FRMOUTPUT), SW_HIDE, SW_SHOW)
    ShowWindow( HWND_FRMOUTPUT, nShowFlag )
    frmMain_PositionWindows
    function = 0
end function

' ========================================================================================
' View Restore Main window size
' ========================================================================================
function OnCommand_ViewRestoreMain() as LRESULT
    dim as long lrmargin = AfxScaleX(10)
    dim as long tbmargin = AfxScaleY(10)
    dim as rect rc
    SystemParametersInfo( SPI_GETWORKAREA, 0, @rc, 0 )
    MoveWindow( HWND_FRMMAIN, _
                lrmargin, tbmargin, _
                rc.right - rc.left - (lrmargin * 2), _
                rc.bottom - rc.top - (tbmargin * 2), _
                true )
    function = 0
end function

' ========================================================================================
' Move Explorer Window Left or Right
' ========================================================================================
function OnCommand_ViewExplorerPosition() as LRESULT
    gConfig.ExplorerPositionRight = not gConfig.ExplorerPositionRight 
    frmMain_PositionWindows()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        AfxRedrawWindow( pDoc->hWindow(0) )
        AfxRedrawWindow( pDoc->hWindow(1) )
    end if   
    function = 0
end function
                
' ========================================================================================
' View Todo, Notes
' ========================================================================================
function OnCommand_ViewToDo() as LRESULT
    ' toggle close the Output window if it is open and already at ToDo 
    if IsWindowVisible(HWND_FRMOUTPUT) then
        if PsSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR ) = OUTPUT_TAB_TODO then
            ShowWindow( HWND_FRMOUTPUT, SW_HIDE )
            frmMain_PositionWindows
            exit function
        end if
    else
        ShowWindow( HWND_FRMOUTPUT, SW_SHOW )
    end if
    frmMain_PositionWindows
    ' Silent: only a user click on the bar fires its change callback, so this cannot
    ' re-enter. frmOutput_PositionWindows ends in ShowHideOutputControls, which is what
    ' actually swaps the visible pane.
    PsSelectBar_SetCurSel( HWND_FRMOUTPUT_SELECTBAR, OUTPUT_TAB_TODO )
    frmOutput_PositionWindows
    AfxRedrawWindow( HWND_FRMOUTPUT_SELECTBAR )
    function = 0
end function

function OnCommand_ViewNotes() as LRESULT
    ' toggle close the Output window if it is open and already at Notes
    if IsWindowVisible(HWND_FRMOUTPUT) then
        if PsSelectBar_GetCurSel( HWND_FRMOUTPUT_SELECTBAR ) = OUTPUT_TAB_NOTES then
            ShowWindow( HWND_FRMOUTPUT, SW_HIDE )
            frmMain_PositionWindows
            exit function
        end if
    else
        ShowWindow( HWND_FRMOUTPUT, SW_SHOW )
    end if
    frmMain_PositionWindows
    ' Silent -- see OnCommand_ViewToDo above.
    PsSelectBar_SetCurSel( HWND_FRMOUTPUT_SELECTBAR, OUTPUT_TAB_NOTES )
    frmOutput_PositionWindows
    AfxRedrawWindow( HWND_FRMOUTPUT_SELECTBAR )
    function = 0
end function
