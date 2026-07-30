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

' See frmDebug.bi for the layout, the absence of a pump obligation, and why the variables
' tree is built to a bounded depth rather than lazily.

#include once "frmDebug.bi"

dim shared as HWND     gDbgBtn(IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST)
dim shared as DWSTRING gDbgStatusText
dim shared as long     gDbgCurFrame          ' call-stack frame the variables view is scoped to
dim shared as boolean  gDbgSessionEnding
dim shared as boolean  gDbgSplitInit         ' the splitter's default position is chosen once

' ==========================================================================================
' Status line
' ==========================================================================================
private sub frmDebug_SetStatus( byval wszText as DWSTRING )
    gDbgStatusText = wszText
    if IsWindow(HWND_FRMDEBUG) then AfxRedrawWindow( HWND_FRMDEBUG )
end sub

private function frmDebug_ReasonText( byval reason as long ) as DWSTRING
    select case reason
        case DBGP_STOP_ENTRY      : return L(603, "Started")
        case DBGP_STOP_STEP       : return L(604, "Stepped")
        case DBGP_STOP_BREAKPOINT : return L(605, "Breakpoint")
        case DBGP_STOP_TEMPBP     : return L(256, "Run to Cursor")
        case DBGP_STOP_USERBREAK  : return L(601, "Break")
        case DBGP_STOP_EXCEPTION  : return L(606, "Exception")
    end select
    return ""
end function

' ==========================================================================================
' Editor markers
' ==========================================================================================
sub frmDebug_ClearExecutionMarkers()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        for i as long = 0 to 1
            if pDoc->hWindow(i) then
                SciExec( pDoc->hWindow(i), SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
            end if
        next
        pDoc = pDoc->pDocNext
    loop
end sub

' Navigate the editor to a location and mark it as the execution point. The marker is ADDED
' explicitly rather than toggled: the old implementation called ToggleMarker here, which
' REMOVES the arrow whenever the debugger stops twice on the same line -- which is exactly
' what a loop body does on every iteration.
private sub frmDebug_ShowExecutionPoint( byval wszFile as DWSTRING, byval nLine1Based as long )
    if len(wszFile) = 0 then exit sub
    if nLine1Based <= 0 then exit sub

    frmDebug_ClearExecutionMarkers()

    dim as long nLine0 = nLine1Based - 1
    dim pDoc as clsDocument ptr = OpenSelectedDocument( wszFile, "", nLine0 )
    if pDoc = 0 then exit sub

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then exit sub
    SciExec( hEdit, SCI_MARKERDELETEALL, MARKER_DEBUGGER_CURLINE, 0 )
    SciExec( hEdit, SCI_MARKERADD, nLine0, MARKER_DEBUGGER_CURLINE )
end sub

' ==========================================================================================
' Call stack
' ==========================================================================================
private sub frmDebug_FillCallStack( byval frameCount as long )
    dim as HWND hLV = HWND_FRMDEBUG_LVSTACK
    if IsWindow(hLV) = 0 then exit sub

    PsListTree_Clear( hLV )
    for i as long = 0 to frameCount - 1
        dim as DBGP_FRAME fr
        if debugparser_get_frame( i, @fr ) <> DBGP_OK then continue for

        dim as DWSTRING wszFile
        if len(fr.wszSrcPath) then wszFile = AfxStrPathname( "NAMEX", fr.wszSrcPath )
        dim as DWSTRING wszLine
        if fr.lineNum > 0 then wszLine = wstr(str(fr.lineNum))

        dim as DWSTRING wszRow = "" & chr(9) & fr.wszProcName & chr(9) & wszLine & chr(9) & wszFile
        dim as long nRow = ListBox_AddTabbedRow( hLV, wszRow )
        ' itemData carries the frame index, so selecting a row can re-scope the variables.
        if nRow >= 0 then PsListTree_SetItemData( hLV, nRow, i )
    next
    if frameCount > 0 then PsListTree_SetCurSel( hLV, 0 )
end sub

' ==========================================================================================
' Variables
'
' One recursive walk bounded by FRMDEBUG_MAXDEPTH / FRMDEBUG_MAXCHILDREN. Everything below
' the top level is added collapsed, so a deep UDT costs its reads once but shows as one row.
' ==========================================================================================
private sub frmDebug_AddChildren( byval hLV as HWND, byval parentRow as long, _
                                  byval pParent as DBGP_NODE ptr, byval depth as long )
    if depth >= FRMDEBUG_MAXDEPTH then exit sub

    dim as long n = debugparser_child_count( pParent )
    if n <= 0 then exit sub

    dim as long shown = n
    if shown > FRMDEBUG_MAXCHILDREN then shown = FRMDEBUG_MAXCHILDREN

    for i as long = 0 to shown - 1
        dim as DBGP_NODE ch
        if debugparser_child_node( pParent, i, @ch ) <> DBGP_OK then continue for
        dim as long r = PsListTree_AddNode( hLV, parentRow, ch.wszName )
        if r < 0 then exit for
        PsListTree_SetCellText( hLV, r, 1, ch.wszValue )
        PsListTree_SetCellText( hLV, r, 2, ch.wszType )
        frmDebug_AddChildren( hLV, r, @ch, depth + 1 )
        PsListTree_SetNodeCollapsed( hLV, r, true )
    next

    ' Say so rather than silently showing a prefix. A truncated list that looks complete is
    ' worse than a short one that admits what it left out.
    if n > shown then
        dim as long r = PsListTree_AddNode( hLV, parentRow, "" )
        if r >= 0 then
            PsListTree_SetCellText( hLV, r, 1, L(607, "more elements not shown") & _
                                    "  (" & str(n - shown) & ")" )
            PsListTree_SetRowSelectable( hLV, r, false )
        end if
    end if
end sub

private sub frmDebug_FillVariables()
    dim as HWND hLV = HWND_FRMDEBUG_LVVARS
    if IsWindow(hLV) = 0 then exit sub

    PsListTree_Clear( hLV )

    dim as long nTab = DEBUG_TAB_LOCALS
    if IsWindow(HWND_FRMDEBUG_SELECTBAR) then
        nTab = PsSelectBar_GetPanelID( HWND_FRMDEBUG_SELECTBAR, _
                                       PsSelectBar_GetCurSel(HWND_FRMDEBUG_SELECTBAR) )
    end if

    dim as long n
    if nTab = DEBUG_TAB_GLOBALS then
        n = debugparser_global_count()
    else
        n = debugparser_local_count( gDbgCurFrame )
    end if

    for i as long = 0 to n - 1
        dim as DBGP_NODE nd
        dim as long rc
        if nTab = DEBUG_TAB_GLOBALS then
            rc = debugparser_global_node( i, @nd )
        else
            rc = debugparser_local_node( gDbgCurFrame, i, @nd )
        end if
        if rc <> DBGP_OK then continue for

        dim as long r = PsListTree_AddNode( hLV, -1, nd.wszName )
        if r < 0 then exit for
        PsListTree_SetCellText( hLV, r, 1, nd.wszValue )
        PsListTree_SetCellText( hLV, r, 2, nd.wszType )
        frmDebug_AddChildren( hLV, r, @nd, 1 )
        PsListTree_SetNodeCollapsed( hLV, r, true )
    next

    PsListTree_Refresh( hLV )
end sub

' ==========================================================================================
' Command enable state. Everything except Stop and Break is meaningless while the debuggee
' is executing, and Break is meaningless while it is stopped.
' ==========================================================================================
private sub frmDebug_SyncButtons()
    dim as boolean bActive  = (debugparser_is_active() <> 0)
    dim as boolean bRunning = (debugparser_is_running() <> 0)
    dim as boolean bStopped = false
    if bActive andalso (bRunning = false) then bStopped = true

    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) = 0 then continue for
        dim as boolean bEnable
        select case id
            case IDC_FRMDEBUG_CMDSTOP
                bEnable = bActive
            case IDC_FRMDEBUG_CMDBREAK
                bEnable = false
                if bActive andalso bRunning then bEnable = true
            case else
                bEnable = bStopped
        end select
        PsButton_SetEnabled( gDbgBtn(id), bEnable )
    next
end sub

' ==========================================================================================
' Stop / exit handling
' ==========================================================================================
private sub frmDebug_OnStopped()
    dim as DBGP_STOPINFO si
    if debugparser_get_stop( @si ) <> DBGP_OK then exit sub

    gDbgCurFrame = 0

    dim as DWSTRING wszWhere
    if len(si.wszSrcPath) then
        wszWhere = AfxStrPathname( "NAMEX", si.wszSrcPath ) & ":" & str(si.lineNum)
        frmDebug_ShowExecutionPoint( si.wszSrcPath, si.lineNum )
    else
        wszWhere = "&h" & hex(si.addr)
    end if

    dim as DWSTRING wszMsg = frmDebug_ReasonText( si.reason ) & "   " & wszWhere
    if len(si.wszProcName) then wszMsg = wszMsg & "   " & L(608, "in") & " " & si.wszProcName
    if si.reason = DBGP_STOP_EXCEPTION then
        wszMsg = wszMsg & "   " & si.wszExcepText
        if si.excepCode = &hC0000005 then
            dim as DWSTRING wszDir = L(610, "reading")
            if si.excepIsWrite <> 0 then wszDir = L(609, "writing")
            wszMsg = wszMsg & " (" & wszDir & " &h" & hex(si.excepTarget) & ")"
        end if
    end if
    frmDebug_SetStatus( wszMsg )

    frmDebug_FillCallStack( si.frameCount )
    frmDebug_FillVariables()
    frmDebug_SyncButtons()
    frmDebug_Trace()
end sub

private sub frmDebug_OnExited( byval exitCode as long )
    gDbgSessionEnding = true
    gApp.IsDebuggerActive = false
    frmDebug_ClearExecutionMarkers()

    if exitCode = 0 then
        SetCompileStatusBarMessage( L(259, "Debug Target Exited Normally"), ghIconGood )
    else
        SetCompileStatusBarMessage( L(260, "Debug Target Terminated Unexpectantly"), ghIconBad )
    end if

    frmMain_DebugTopMenuStates()
    if IsWindow(HWND_FRMDEBUG) then DestroyWindow( HWND_FRMDEBUG )
end sub

' ==========================================================================================
' Commands. Reached from the toolbar, the Debug menu and the accelerators alike.
' ==========================================================================================
sub frmDebug_Command( byval id as long )
    if debugparser_is_active() = 0 then exit sub

    select case id
        case IDC_FRMDEBUG_CMDCONTINUE, IDM_DEBUG_STARTDEBUGGING
            debugparser_resume( DBGP_RUN_CONTINUE )
        case IDC_FRMDEBUG_CMDSTEPINTO, IDM_DEBUG_STEPINTO
            debugparser_resume( DBGP_RUN_STEPINTO )
        case IDC_FRMDEBUG_CMDSTEPOVER, IDM_DEBUG_STEPOVER
            debugparser_resume( DBGP_RUN_STEPOVER )
        case IDC_FRMDEBUG_CMDSTEPOUT, IDM_DEBUG_STEPOUT
            debugparser_resume( DBGP_RUN_STEPOUT )
        case IDC_FRMDEBUG_CMDBREAK, IDM_DEBUG_PAUSE
            debugparser_pause()
            frmDebug_SetStatus( L(602, "Pausing at the next source line...") )
            exit sub
        case IDC_FRMDEBUG_CMDSTOP, IDM_DEBUG_STOPDEBUGGING
            debugparser_stop()
            exit sub
        case IDC_FRMDEBUG_CMDRUNCURSOR, IDM_DEBUG_RUNTOCURSOR
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then exit sub
            dim as wstring * 520 wszFile = pDoc->DiskFilename
            dim as long si = debugparser_source_index( @wszFile )
            if si < 0 then exit sub
            dim as long ln = debugparser_next_executable_line( si, pDoc->GetCurrentLineNumber + 1 )
            if ln < 0 then exit sub
            debugparser_set_run_to_cursor( si, ln )
            debugparser_resume( DBGP_RUN_TOCURSOR )
    end select

    frmDebug_SetStatus( L(600, "Running...") )
    frmDebug_SyncButtons()
end sub

private sub frmDebug_ButtonClick( byval hButton as HWND, byval id as long )
    frmDebug_Command( id )
end sub

' Selecting a call-stack frame re-scopes the variables view to it and navigates the editor.
private sub frmDebug_StackSelChange( byval hListControl as HWND, byval row as integer )
    if row < 0 then exit sub
    dim as HWND hList = GetParent( hListControl )
    if IsWindow(hList) = 0 then hList = hListControl
    dim as long frameIndex = cast(long, PsListTree_GetItemData( hList, row ))
    if frameIndex < 0 then exit sub

    gDbgCurFrame = frameIndex
    dim as DBGP_FRAME fr
    if debugparser_get_frame( frameIndex, @fr ) = DBGP_OK then
        if (len(fr.wszSrcPath) > 0) andalso (fr.lineNum > 0) then
            frmDebug_ShowExecutionPoint( fr.wszSrcPath, fr.lineNum )
        end if
    end if
    frmDebug_FillVariables()
end sub

private sub frmDebug_TabSelChange( byval hSelectBar as HWND, byval idxOld as long, byval idxNew as long )
    frmDebug_FillVariables()
end sub

' ==========================================================================================
' Layout
' ==========================================================================================
private sub frmDebug_PositionWindows()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long nMargin  = pWindow->ScaleX(12)
    dim as long nToolH   = pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT)
    dim as long nStatusH = pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT)
    dim as long nTabH    = pWindow->ScaleY(FRMDEBUG_TABHEIGHT)
    dim as long nSplitW  = pWindow->ScaleX(FRMDEBUG_SPLITWIDTH)
    dim as long nMinPane = pWindow->ScaleX(FRMDEBUG_MINPANE)

    ' --- toolbar: buttons left to right at their ideal widths ---
    dim as long x = nMargin
    dim as long y = pWindow->ScaleY(6)
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) = 0 then continue for
        dim as long bw, bh
        PsButton_GetIdealSize( gDbgBtn(id), bw, bh )
        SetWindowPos( gDbgBtn(id), 0, x, y, bw, bh, SWP_NOZORDER )
        x += bw + pWindow->ScaleX(6)
        ' A wider gap after Stop and after Step Out groups the seven into three sets.
        if (id = IDC_FRMDEBUG_CMDSTOP) orelse (id = IDC_FRMDEBUG_CMDSTEPOUT) then
            x += pWindow->ScaleX(14)
        end if
    next

    dim as long nTop = nToolH + nStatusH

    ' --- splitter. Its range is recomputed here rather than derived by the control, which
    ' cannot know what else lives in this client area -- PsSplitter.bi states that.
    dim as long nMinBar = nMinPane
    dim as long nMaxBar = rc.right - nMinPane - nSplitW
    if nMaxBar < nMinBar then nMaxBar = nMinBar

    ' The default position has to be chosen BEFORE SetRange, not after: SetRange clamps the
    ' current position into the new range silently, so by the time it returns the bar is
    ' never out of range and an "is it out of range" test can never fire.
    if gDbgSplitInit = false then
        gDbgSplitInit = true
        PsSplitter_SetPos( HWND_FRMDEBUG_SPLITTER, rc.right * 3 \ 5 )
    end if
    PsSplitter_SetRange( HWND_FRMDEBUG_SPLITTER, nMinBar, nMaxBar )
    dim as long nBarPos = PsSplitter_GetPos( HWND_FRMDEBUG_SPLITTER )

    SetWindowPos( HWND_FRMDEBUG_SPLITTER, 0, nBarPos, nTop, nSplitW, rc.bottom - nTop, SWP_NOZORDER )

    ' --- left pane: tab strip over the variables tree ---
    SetWindowPos( HWND_FRMDEBUG_SELECTBAR, 0, 0, nTop, nBarPos, nTabH, SWP_NOZORDER )
    SetWindowPos( HWND_FRMDEBUG_LVVARS, 0, 0, nTop + nTabH, nBarPos, rc.bottom - nTop - nTabH, SWP_NOZORDER )

    ' --- right pane: the call stack; its own header band supplies the caption ---
    dim as long xRight = nBarPos + nSplitW
    SetWindowPos( HWND_FRMDEBUG_LVSTACK, 0, xRight, nTop, rc.right - xRight, rc.bottom - nTop, SWP_NOZORDER )
end sub

' Env-gated geometry dump (TIKO_DEBUGWIN_TRACE=1). Every rect this window computes, read
' back from the CONTROLS rather than from the variables that placed them -- a layout bug
' that leaves a control at 0x0 is invisible to a trace of the intended numbers.
sub frmDebug_Trace()
    if environ("TIKO_DEBUGWIN_TRACE") <> "1" then exit sub
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then exit sub

    dim as string sLine
    dim as RECT rc
    GetClientRect( HWND_FRMDEBUG, @rc )
    sLine = "--- TIKO_DEBUGWIN_TRACE ---"
    print sLine
    sLine = "  DPI rx=" & str(pWindow->rxRatio) & " ry=" & str(pWindow->ryRatio) & _
            "   client=" & str(rc.right) & "x" & str(rc.bottom)
    print sLine

    #macro TRACEWND( nm, h )
        if IsWindow(h) then
            dim as RECT r2
            GetWindowRect( h, @r2 )
            MapWindowPoints( HWND_DESKTOP, HWND_FRMDEBUG, cast(LPPOINT, @r2), 2 )
            sLine = "  " & nm & " = (" & str(r2.left) & "," & str(r2.top) & ")-(" & _
                    str(r2.right) & "," & str(r2.bottom) & ")  " & _
                    str(r2.right - r2.left) & "x" & str(r2.bottom - r2.top) & _
                    iif( IsWindowVisible(h), "  visible", "  HIDDEN" )
            print sLine
        else
            sLine = "  " & nm & " = NOT A WINDOW"
            print sLine
        end if
    #endmacro

    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        TRACEWND( "button" & str(id), gDbgBtn(id) )
    next
    TRACEWND( "selectbar", HWND_FRMDEBUG_SELECTBAR )
    TRACEWND( "splitter ", HWND_FRMDEBUG_SPLITTER )
    TRACEWND( "vars     ", HWND_FRMDEBUG_LVVARS )
    TRACEWND( "stack    ", HWND_FRMDEBUG_LVSTACK )

    sLine = "  splitter pos=" & str(PsSplitter_GetPos(HWND_FRMDEBUG_SPLITTER))
    print sLine
    sLine = "  rows: vars=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVVARS)) & _
            "  stack=" & str(PsListTree_GetCount(HWND_FRMDEBUG_LVSTACK))
    print sLine
    print "--- end ---"
end sub

private sub frmDebug_SplitterMoved( byval hSplitter as HWND, byval newPos as integer, byval nPhase as integer )
    frmDebug_PositionWindows()
end sub

private sub frmDebug_SplitterPaint( byval p as PSSPLITTER_PAINTINFO ptr )
    if p = 0 then exit sub
    ' NOT the window background. Painting the bar in BackColorHot -- which is what this
    ' window's own WM_PAINT fills with -- makes a correctly placed splitter invisible.
    dim as COLORREF clr = ghOutput.Divider
    if p->isHot orelse p->isDragging then clr = ghOutput.ForeColorScrollBar
    p->b->SetBackColor( clr )
    p->b->PaintRect( @p->rcClient )
end sub

' ==========================================================================================
' Theming
'
' The list colours are pushed from here rather than through modThemeApply's shared helper:
' that file is included AFTER this one (it dispatches to every frmXxx_ApplyTheme, so it has
' to come last), and its helpers are therefore not in scope here.
' ==========================================================================================
private sub frmDebug_PushListColors( byval hCtl as HWND )
    if IsWindow(hCtl) = 0 then exit sub
    PsListTree_SetBackColor( hCtl, ghOutput.BackColor )
    PsListTree_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, _
                                   ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )
    AfxRedrawWindow( hCtl )
end sub

sub frmDebug_ApplyTheme()
    if IsWindow(HWND_FRMDEBUG) = 0 then exit sub

    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) = 0 then continue for
        dim as PSBUTTON_COLORS bc
        PsButton_GetColors( gDbgBtn(id), @bc )
        bc.BackColor        = ghOutput.BackColorHot
        bc.BackColorHot     = ghOutput.ForeColorScrollBar
        bc.BackColorPressed = ghOutput.ForeColorScrollBar
        bc.ForeColor        = ghOutput.ForeColorHot
        bc.ForeColorHot     = ghOutput.ForeColorHot
        bc.BorderColor      = ghOutput.Divider
        bc.BorderColorHot   = ghOutput.Divider
        PsButton_SetColors( gDbgBtn(id), @bc )
    next

    if IsWindow(HWND_FRMDEBUG_SELECTBAR) then
        dim as PSSELECTBAR_COLORS sc
        PsSelectBar_GetColors( HWND_FRMDEBUG_SELECTBAR, @sc )
        sc.BackColor       = ghOutput.BackColorHot
        sc.ForeColor       = ghOutput.ForeColor
        sc.ForeColorSelect = ghOutput.ForeColorHot
        PsSelectBar_SetColors( HWND_FRMDEBUG_SELECTBAR, @sc )
    end if

    frmDebug_PushListColors( HWND_FRMDEBUG_LVVARS )
    frmDebug_PushListColors( HWND_FRMDEBUG_LVSTACK )
    if IsWindow(HWND_FRMDEBUG_SPLITTER) then
        PsSplitter_SetColors( HWND_FRMDEBUG_SPLITTER, ghOutput.BackColorHot, ghOutput.Divider )
    end if
    Theme_ApplyDarkCaption( HWND_FRMDEBUG )
    AfxRedrawWindow( HWND_FRMDEBUG )
end sub

function frmDebug_IsOpen() as boolean
    return (IsWindow(HWND_FRMDEBUG) <> 0)
end function

' ==========================================================================================
' Control construction
' ==========================================================================================
private function frmDebug_CreateListView( byval CtrlID as long ) as HWND
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMDEBUG)
    if pWindow = 0 then return 0

    dim as HWND hCtl = PsListTree_Create( HWND_FRMDEBUG, CtrlID )

    if CtrlID = IDC_FRMDEBUG_LVVARS then
        PsListTree_AddColumn( hCtl, L(611, "Name"),  pWindow->ScaleX(200) )
        PsListTree_AddColumn( hCtl, L(286, "Value"), pWindow->ScaleX(220) )
        PsListTree_AddColumn( hCtl, L(612, "Type"),  pWindow->ScaleX(140) )
        PsListTree_SetFillColumn( hCtl, 1 )
        ' A real tree: UDT fields and array elements are children, not flattened rows.
        PsListTree_ShowTwisty( hCtl, true )
        PsListTree_SetIndentWidth( hCtl, 14 )
        PsListTree_SetTwistyFont( hCtl, ghFont(SYMBOLFONT_9) )
    else
        PsListTree_AddColumn( hCtl, "",                 pWindow->ScaleX(20) )
        PsListTree_AddColumn( hCtl, L(613, "Function"), pWindow->ScaleX(200) )
        PsListTree_AddColumn( hCtl, L(203, "Line"),     pWindow->ScaleX(60) )
        PsListTree_AddColumn( hCtl, L(204, "File"),     pWindow->ScaleX(160) )
        PsListTree_SetFillColumn( hCtl, 3 )
        PsListTree_SetSelChangeCallback( hCtl, @frmDebug_StackSelChange )
    end if

    PsListTree_ShowHeader( hCtl, true )
    PsListTree_SetPaintCallback( hCtl, @ListBox_LVPaintRow )
    PsListTree_SetHeaderPaintCallback( hCtl, @ListBox_LVPaintHeader )
    PsListTree_SetColumnAutoSizeCallback( hCtl, @ListBox_LVAutoSizeColumn )
    PsListTree_SetFont( hCtl, ghFont(GUIFONT_9) )
    PsListTree_SetBackColor( hCtl, ghOutput.BackColor )
    PsListTree_SetScrollBarColors( hCtl, ghOutput.BackColorScrollBar, _
                                   ghOutput.ForeColorScrollBar, ghOutput.ForeColorScrollBar )
    return hCtl
end function

private sub frmDebug_CreateButton( byval id as long, byval wszText as DWSTRING, byval wszGlyph as DWSTRING )
    dim as HWND h = PsButton_Create( HWND_FRMDEBUG, id )
    PsButton_SetText( h, wszText )
    if len(wszGlyph) then PsButton_SetGlyphLeft( h, wszGlyph )
    PsButton_SetFont( h, ghFont(GUIFONT_9) )
    PsButton_SetGlyphFont( h, ghFont(SYMBOLFONT_9) )
    PsButton_SetClickCallback( h, @frmDebug_ButtonClick )
    gDbgBtn(id) = h
end sub

' ==========================================================================================
' Window procedure
' ==========================================================================================
private function frmDebug_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                   byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case MSG_DBGP_STOPPED
            frmDebug_OnStopped()
            return 0

        case MSG_DBGP_EXITED
            frmDebug_OnExited( cast(long, wParam) )
            return 0

        case MSG_DBGP_FAILED
            dim as wstring * 512 werr
            debugparser_get_error( @werr, 512 )
            gApp.IsDebuggerActive = false
            gDbgSessionEnding = true
            TikoMsgBox( HWND_FRMMAIN, L(614, "The debugger could not start the program.") & _
                        chr(13) & chr(13) & werr, L(218, "Confirm"), TMB_ICON_ERROR, TMB_OK )
            DestroyWindow( hwnd )
            return 0

        case WM_SIZE
            frmDebug_PositionWindows()
            return 0

        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            dim as PsBufferPaint b
            b.BeginDoubleBuffer(hwnd)
            b.SetBackColor( ghOutput.BackColorHot )
            b.PaintClientRect()

            if pWindow then
                dim as RECT rc
                GetClientRect( hwnd, @rc )
                dim as RECT rcStatus
                rcStatus.left   = pWindow->ScaleX(12)
                rcStatus.top    = pWindow->ScaleY(FRMDEBUG_TOOLHEIGHT)
                rcStatus.right  = rc.right - pWindow->ScaleX(12)
                rcStatus.bottom = rcStatus.top + pWindow->ScaleY(FRMDEBUG_STATUSHEIGHT)
                b.SetFont( ghFont(GUIFONT_9) )
                b.SetForeColor( ghOutput.ForeColorHot )
                b.SetBackColor( ghOutput.BackColorHot )
                b.PaintText( gDbgStatusText, @rcStatus, DT_LEFT or DT_END_ELLIPSIS )

                ' A hairline under the status line, separating chrome from content.
                dim as RECT rcLine
                rcLine.left   = 0
                rcLine.top    = rcStatus.bottom - 1
                rcLine.right  = rc.right
                rcLine.bottom = rcStatus.bottom
                b.SetBackColor( ghOutput.Divider )
                b.PaintRect( @rcLine )
            end if

            b.EndDoubleBuffer()
            return 0

        case WM_CLOSE
            ' Closing the window ends the session. The engine's teardown then posts
            ' MSG_DBGP_EXITED, and THAT is what destroys this window -- so the debuggee is
            ' never left running with no way to reach it.
            if (debugparser_is_active() <> 0) andalso (gDbgSessionEnding = false) then
                debugparser_stop()
                return 0
            end if
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            dim as WINDOWPLACEMENT wp
            wp.length = sizeof(WINDOWPLACEMENT)
            if GetWindowPlacement( hwnd, @wp ) then
                gDebugPos.bInitialized = true
                gDebugPos.bMaximized   = (wp.showCmd = SW_SHOWMAXIMIZED)
                gDebugPos.nLeft   = wp.rcNormalPosition.left
                gDebugPos.nTop    = wp.rcNormalPosition.top
                gDebugPos.nRight  = wp.rcNormalPosition.right
                gDebugPos.nBottom = wp.rcNormalPosition.bottom
            end if
            return 0

        case WM_NCDESTROY
            frmDebug_ClearExecutionMarkers()
            gApp.IsDebuggerActive = false
            debugparser_unload()
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete pWindow
            HWND_FRMDEBUG           = 0
            HWND_FRMDEBUG_LVVARS    = 0
            HWND_FRMDEBUG_LVSTACK   = 0
            HWND_FRMDEBUG_SELECTBAR = 0
            HWND_FRMDEBUG_SPLITTER  = 0
            for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
                gDbgBtn(id) = 0
            next
            frmMain_DebugTopMenuStates()
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function

' ==========================================================================================
' frmDebug_Show - builds the window, loads the debug information, seeds the breakpoints the
' user set in the editor, and starts the session.
' ==========================================================================================
function frmDebug_Show( byval hWndParent as HWND, byval executable as DWSTRING, byval cmdparams as DWSTRING ) as LRESULT
    if IsWindow(HWND_FRMDEBUG) then
        SetForegroundWindow( HWND_FRMDEBUG )
        return 0
    end if

    ' Read the debug information BEFORE creating any window. A release build carries none,
    ' and failing here means no debugger window ever appears rather than an empty one that
    ' then has to explain itself.
    dim as wstring * 1024 wszExe = executable
    dim as long rc = debugparser_load( @wszExe, 0 )
    if rc <> DBGP_OK then
        dim as DWSTRING wszMsg
        if rc = DBGP_ERR_NODEBUGINFO then
            wszMsg = L(615, "This program carries no debug information.") & chr(13) & chr(13) & _
                     L(616, "Build it with -g and the gas64 backend, or gas for 32-bit. The gcc backend emits a format that cannot be read.")
        else
            dim as wstring * 512 werr
            debugparser_get_error( @werr, 512 )
            wszMsg = L(617, "The debug information could not be read.") & chr(13) & chr(13) & werr
        end if
        TikoMsgBox( hWndParent, wszMsg, L(218, "Confirm"), TMB_ICON_ERROR, TMB_OK )
        return 0
    end if

    gDbgSessionEnding = false
    gDbgSplitInit     = false
    gDbgCurFrame      = 0
    gDbgStatusText    = L(600, "Running...")

    dim pWindow as CWindow ptr = new CWindow
    dim pParent as CWindow ptr = AfxCWindowPtr(hWndParent)
    if pParent then pWindow->DPI = pParent->DPI

    HWND_FRMDEBUG = pWindow->Create( hWndParent, L(618, "Debugger"), @frmDebug_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_THICKFRAME or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT )
    ' UNSCALED. SetClientSize applies the DPI factor itself; passing a pre-scaled value
    ' squares it, which at 175% turned a 900px window into a 2756px one.
    pWindow->SetClientSize( FRMDEBUG_DEFWIDTH, FRMDEBUG_DEFHEIGHT )
    pWindow->Center( HWND_FRMDEBUG, hWndParent )

    if gDebugPos.bInitialized then
        dim as WINDOWPLACEMENT wp
        wp.length = sizeof(WINDOWPLACEMENT)
        wp.rcNormalPosition.left   = gDebugPos.nLeft
        wp.rcNormalPosition.top    = gDebugPos.nTop
        wp.rcNormalPosition.right  = gDebugPos.nRight
        wp.rcNormalPosition.bottom = gDebugPos.nBottom
        wp.showCmd = iif( gDebugPos.bMaximized, SW_SHOWMAXIMIZED, SW_SHOWNORMAL )
        SetWindowPlacement( HWND_FRMDEBUG, @wp )
    end if

    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' --- toolbar ---
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDCONTINUE,  L(253, "Continue Debugging"), wszIconContinue )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDBREAK,     L(601, "Break"),              wszIconPause )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTOP,      L(254, "Stop Debugging"),     wszIconStop )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPINTO,  L(247, "Step Into"),          wszIconStepNext )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPOVER,  L(248, "Step Over"),          wszIconStepOver )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDSTEPOUT,   L(255, "Step Out"),           wszIconStepOut )
    frmDebug_CreateButton( IDC_FRMDEBUG_CMDRUNCURSOR, L(256, "Run to Cursor"),      wszIconRunToCursor )

    ' --- tab strip over the variables tree ---
    HWND_FRMDEBUG_SELECTBAR = PsSelectBar_Create( HWND_FRMDEBUG, IDC_FRMDEBUG_SELECTBAR )
    PsSelectBar_SetFont( HWND_FRMDEBUG_SELECTBAR, ghFont(GUIFONT_9) )
    PsSelectBar_SetIndicator( HWND_FRMDEBUG_SELECTBAR, 1, 12 )
    PsSelectBar_SetSelChangeCallback( HWND_FRMDEBUG_SELECTBAR, @frmDebug_TabSelChange )
    PsSelectBar_AddPanel( HWND_FRMDEBUG_SELECTBAR, L(619, "Locals"),  DEBUG_TAB_LOCALS )
    PsSelectBar_AddPanel( HWND_FRMDEBUG_SELECTBAR, L(620, "Globals"), DEBUG_TAB_GLOBALS )

    HWND_FRMDEBUG_LVVARS  = frmDebug_CreateListView( IDC_FRMDEBUG_LVVARS )
    HWND_FRMDEBUG_LVSTACK = frmDebug_CreateListView( IDC_FRMDEBUG_LVSTACK )

    HWND_FRMDEBUG_SPLITTER = PsSplitter_Create( HWND_FRMDEBUG, IDC_FRMDEBUG_SPLITTER, PSSPLITTER_VERTICAL )
    PsSplitter_SetPaintCallback( HWND_FRMDEBUG_SPLITTER, @frmDebug_SplitterPaint )
    PsSplitter_SetPosChangedCallback( HWND_FRMDEBUG_SPLITTER, @frmDebug_SplitterMoved )

    frmDebug_ApplyTheme()
    frmDebug_PositionWindows()

    ' Every Ps* control is created WS_CHILD but NOT WS_VISIBLE: the family leaves showing
    ' them to the host, which has to position them first. Miss this and the window paints
    ' its background and nothing else.
    for id as long = IDC_FRMDEBUG_CMDFIRST to IDC_FRMDEBUG_CMDLAST
        if IsWindow(gDbgBtn(id)) then ShowWindow( gDbgBtn(id), SW_SHOW )
    next
    ShowWindow( HWND_FRMDEBUG_SELECTBAR, SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_LVVARS,    SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_LVSTACK,   SW_SHOW )
    ShowWindow( HWND_FRMDEBUG_SPLITTER,  SW_SHOW )

    ' --- seed the breakpoints the user set in the editor ---
    debugparser_clear_breakpoints()
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        if pDoc->hWindow(0) then
            dim as wstring * 520 wszFile = pDoc->DiskFilename
            dim as long si = debugparser_source_index( @wszFile )
            if si >= 0 then
                dim as DWSTRING wszBps = pDoc->GetBreakPoints()
                dim as long n = AfxStrParseCount( wszBps, "," )
                for i as long = 1 to n
                    dim as DWSTRING wszOne = AfxStrParse( wszBps, i, "," )
                    if len(trim(wszOne)) = 0 then continue for
                    ' Editor lines are 0-based; the engine's are 1-based. That boundary is
                    ' stated once, here, because the old gdb path converted it in four
                    ' places and got one of them wrong.
                    dim as long ln = valint(wszOne) + 1
                    ' The line table says which lines actually generated code, so a
                    ' breakpoint on a comment or a declaration moves forward to the next
                    ' real one instead of silently never firing.
                    dim as long lnExec = debugparser_next_executable_line( si, ln )
                    if lnExec > 0 then debugparser_add_breakpoint( si, lnExec )
                next
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    ShowWindow( HWND_FRMDEBUG, SW_SHOW )
    UpdateWindow( HWND_FRMDEBUG )

    gApp.IsDebuggerActive = true
    frmMain_DebugTopMenuStates()

    dim as wstring * 1024 wszParams = cmdparams
    if debugparser_start( @wszExe, @wszParams, HWND_FRMDEBUG ) <> DBGP_OK then
        dim as wstring * 512 werr
        debugparser_get_error( @werr, 512 )
        gApp.IsDebuggerActive = false
        gDbgSessionEnding = true
        TikoMsgBox( hWndParent, L(614, "The debugger could not start the program.") & _
                    chr(13) & chr(13) & werr, L(218, "Confirm"), TMB_ICON_ERROR, TMB_OK )
        DestroyWindow( HWND_FRMDEBUG )
        return 0
    end if

    frmDebug_SyncButtons()
    return 0
end function
