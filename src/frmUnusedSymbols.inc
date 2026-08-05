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

' ==========================================================================================
' frmUnusedSymbols implementation.  See frmUnusedSymbols.bi for the shape and the two
' things that are this file's job rather than a control's: sorting and the sort glyph.
' ==========================================================================================

dim shared gUnusedRows(any) as UNUSEDROW     ' the whole report, sorted
dim shared gUnusedRowCount  as long
dim shared gUnusedVisible(any) as long       ' model indices currently in the list
dim shared gUnusedVisCount  as long
dim shared gUnusedToggle(IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST) as HWND
dim shared gUnusedScannedText as DWSTRING
dim shared gUnusedPending     as boolean   ' a scan was requested; rows arrive later
dim shared gUnusedDirtyText   as DWSTRING
' Collapsed ("minimized") state. gUnusedRestoreRect is the window rect to come BACK to, in
' screen coords - captured at the moment of collapse, because the stub's own rect is not it.
dim shared gUnusedCollapsed   as boolean
dim shared gUnusedRestoreRect as RECT
dim shared gUnusedMinOnOpen   as HWND    ' "Minimize when a row is opened" 

declare sub frmUnusedSymbols_PositionWindows()
declare sub frmUnusedSymbols_Collapse()


function frmUnusedSymbols_IsOpen() as boolean
    return (IsWindow(HWND_FRMUNUSED) <> 0)
end function


function frmUnusedSymbols_IsCollapsed() as boolean
    return gUnusedCollapsed
end function


' ------------------------------------------------------------------------------------------
' Does the right-aligned caution fit beside the left-hand count? PURE.
' ------------------------------------------------------------------------------------------
function frmUnusedSymbols_CautionFits( byval nBandWidth as long, _
                                       byval nLeftWidth as long, _
                                       byval nRightWidth as long, _
                                       byval nGap as long ) as boolean
    if nRightWidth <= 0 then return false
    return (nLeftWidth + nGap + nRightWidth) <= nBandWidth
end function


' ------------------------------------------------------------------------------------------
' Where the collapsed stub sits. PURE.
'
' Top-RIGHT of the editing area, inset by the margin. Pure because the failure mode is
' silent: a sign error parks the stub off-screen and the report simply appears to vanish,
' with the window still perfectly alive.
' ------------------------------------------------------------------------------------------
sub frmUnusedSymbols_ComputeStubRect( byref rcAnchor as RECT, _
                                      byval cx as long, byval cy as long, _
                                      byval nMargin as long, byref rcOut as RECT )
    dim as long x = rcAnchor.right - nMargin - cx
    dim as long y = rcAnchor.top + nMargin

    ' An editing area narrower than the stub would push it off the LEFT edge; clamp so it
    ' stays reachable rather than sliding out of the anchor entirely.
    if x < rcAnchor.left then x = rcAnchor.left
    if y < rcAnchor.top then y = rcAnchor.top

    SetRect( @rcOut, x, y, x + cx, y + cy )
end sub


' The editing area, in SCREEN coords: the active document's Scintilla. Falls back to
' frmMain's client area when there is no document, so the stub is always placed against
' something real.
private sub frmUnusedSymbols_AnchorRect( byref rcOut as RECT )
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        if IsWindow( pDoc->hWndActiveScintilla ) then
            GetWindowRect( pDoc->hWndActiveScintilla, @rcOut )
            exit sub
        end if
    end if

    GetClientRect( HWND_FRMMAIN, @rcOut )
    dim as POINT pt = ( rcOut.left, rcOut.top )
    ClientToScreen( HWND_FRMMAIN, @pt )
    SetRect( @rcOut, pt.x, pt.y, pt.x + (rcOut.right - rcOut.left), _
                     pt.y + (rcOut.bottom - rcOut.top) )
end sub


' ------------------------------------------------------------------------------------------
' Layout. PURE, so the band tiling can be asserted with no window.
' ------------------------------------------------------------------------------------------
sub frmUnusedSymbols_ComputeLayout( byval cx as long, byval cy as long, _
                                    byval nHeader as long, byval nToggles as long, _
                                    byval nStatus as long, _
                                    byref lo as FRMUNUSED_LAYOUT )
    dim as long y = 0

    SetRect( @lo.rcHeader, 0, y, cx, y + nHeader ) : y += nHeader
    SetRect( @lo.rcToggles, 0, y, cx, y + nToggles ) : y += nToggles

    dim as long nListBottom = cy - nStatus
    ' A window dragged very short must not give the list a negative height - SetWindowPos
    ' with a negative extent is not an error, it is a silently wrong window.
    if nListBottom < y then nListBottom = y
    SetRect( @lo.rcList, 0, y, cx, nListBottom )
    SetRect( @lo.rcStatus, 0, nListBottom, cx, cy )
end sub


' ------------------------------------------------------------------------------------------
' Rebuild the visible index list from the model, applying the filters.
' ------------------------------------------------------------------------------------------
private sub frmUnusedSymbols_Populate()
    dim as HWND hLV = HWND_FRMUNUSED_LIST
    if IsWindow(hLV) = 0 then exit sub

    ' the mask is read back from the controls, so they are the single source of truth and
    ' gConfig is only their persisted copy
    dim as long nMask = 0
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        if IsWindow(gUnusedToggle(id)) then
            if PsCheckBox_GetChecked( gUnusedToggle(id) ) then
                nMask or= (1 shl (id - IDC_FRMUNUSED_TOGGLEFIRST))
            end if
        end if
    next
    gConfig.UnusedKindMask = nMask

    PsListTree_Clear( hLV )

    if gUnusedRowCount > 0 then redim gUnusedVisible( 0 to gUnusedRowCount - 1 )
    gUnusedVisCount = 0

    for i as long = 0 to gUnusedRowCount - 1
        if UnusedSymbols_Passes( gUnusedRows(i), nMask ) = false then continue for

        dim as DWSTRING wszRow = gUnusedRows(i).wszFile & chr(9) & _
                                 wstr(gUnusedRows(i).nLine) & chr(9) & _
                                 UnusedSymbols_KindText( gUnusedRows(i).nKind ) & chr(9) & _
                                 gUnusedRows(i).wszName & chr(9) & _
                                 UnusedSymbols_StatusText( gUnusedRows(i) )
        ListBox_AddTabbedRow( hLV, wszRow )
        ' the payload is the MODEL index, so it survives every re-sort and re-filter
        PsListTree_SetItemData( hLV, gUnusedVisCount, i )
        gUnusedVisible(gUnusedVisCount) = i
        gUnusedVisCount += 1
    next

    ' A redraw of the CONTAINER never reaches the rows: they are painted onto the control's
    ' own surface, which is a child window.
    PsListTree_Refresh( hLV )

    ' the status line is painted, not a control
    if IsWindow(HWND_FRMUNUSED) then AfxRedrawWindow( HWND_FRMUNUSED )
end sub


' ------------------------------------------------------------------------------------------
' Which file roots the report's scan?
'
' With a project open that is its main file. WITHOUT one it is the ACTIVE DOCUMENT, which is
' what makes the report usable on loose files at all -- fbcParser follows #includes from
' whatever root it is given, so a lone .bas still yields a real compilation unit. Same
' "project main file, else active document" rule modRoutines.inc:1612 already uses.
'
' Returns "" when there is nothing scannable: no document, or one never saved to disk (the
' project tier reads from disk, so an unsaved buffer has no root).
' ------------------------------------------------------------------------------------------
private function frmUnusedSymbols_RootFile() as DWSTRING
    dim pDoc as clsDocument ptr
    ' Main if there is one, otherwise the active document -- taking only the project arm
    ' would return "" for any workspace with no designated Main.
    pDoc = gApp.GetMainDocumentPtr()
    if pDoc = 0 then pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then return ""

    ' The project tier scans from DISK, so the root has to be a real file. Test for the
    ' file itself rather than for a backslash: a document opened from the command line as
    ' a relative or forward-slashed path is on disk and has no backslash, and a
    ' path-shaped test would reject it.
    dim as DWSTRING wszFile = pDoc->DiskFilename
    if PsLen( wszFile ) = 0 then return ""
    if pDoc->IsNewFlag then return ""             ' never saved
    if PsFileExists( wszFile ) = false then return ""
    return wszFile
end function


' ------------------------------------------------------------------------------------------
' Rebuild the whole report from the current project scan. Called on open, and again from
' frmMain when a scan completes while the window is up - which is how Refresh lands.
' ------------------------------------------------------------------------------------------
sub frmUnusedSymbols_Rebuild()
    if IsWindow(HWND_FRMUNUSED) = 0 then exit sub

    ' reached only from a completed scan, so whatever we were waiting for has arrived
    gUnusedPending = false

    gUnusedRowCount = UnusedSymbols_Build( ScanTierProject, gUnusedRows(), gUnusedDirtyText )

    dim as PARSERESULTSET ptr pRSet = gSymDb.ProjectSet()
    if pRSet then gUnusedScannedText = FilenameOriginalCase( pRSet->wszRootFile )

    UnusedSymbols_Sort( gUnusedRows(), gUnusedRowCount, gConfig.UnusedSortCol, gConfig.UnusedSortDesc )

    ' the warning band appears and disappears with the dirty-file list, so the bands move
    frmUnusedSymbols_PositionWindows()
    frmUnusedSymbols_Populate()

    ' The asynchronous fill is unreachable from the layout suite, which runs at _Show time
    ' while the scan is still pending. Report it so an unattended run can show the rows
    ' actually arrived rather than only that the window opened.
    if environ("TIKO_UNUSED_SELFTEST") = "1" then
        ' BOTH counts: gUnusedRowCount is the model, gUnusedVisCount is what actually
        ' reached the control. They differ by the kind filter, and a visible count of 0
        ' against a non-zero model is exactly the "no results" failure.
        dim as string sR = "  rebuild: " & gUnusedVisCount & " of " & gUnusedRowCount & _
                           " rows visible, from " & gUnusedScannedText
        print sR
    end if
end sub


' ------------------------------------------------------------------------------------------
' Sort. The control never sorts: reorder the model, rebuild, repaint the glyph.
' ------------------------------------------------------------------------------------------
private sub frmUnusedSymbols_HeaderClick( byval hHeader as HWND, byval idx as integer )
    if idx < 0 orelse idx >= UNUSED_COLUMN_COUNT then exit sub

    if idx = gConfig.UnusedSortCol then
        gConfig.UnusedSortDesc = not gConfig.UnusedSortDesc
    else
        gConfig.UnusedSortCol = idx
        gConfig.UnusedSortDesc = false
    end if

    UnusedSymbols_Sort( gUnusedRows(), gUnusedRowCount, gConfig.UnusedSortCol, gConfig.UnusedSortDesc )
    frmUnusedSymbols_Populate()
    ' Without this the arrow stays on the previously sorted column - the header repaints on
    ' its own only for hover and press.
    PsColumnHeader_Refresh( hHeader )
end sub


' Local header painter: the shared ListBox_LVPaintHeader has no sort-indicator support and
' four other panes depend on it unchanged.
private sub frmUnusedSymbols_PaintHeader( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr = theme.output.text.backcolor
    if p->isHot orelse p->isResizeHot then backclr = theme.output.scrollbar.forecolor
    p->b->SetForeColor( theme.output.texthot.forecolor )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(4)

    ' the glyph sits at the right edge of the sorted column
    if p->itemID = gConfig.UnusedSortCol then
        dim as RECT rcGlyph = p->rc
        rcGlyph.left = rcGlyph.right - AfxScaleX(16)
        rcText.right = rcGlyph.left
        ' Segoe Fluent: E70E = chevron up (ascending), E70D = chevron down.
        ' MUST be the escape form, never the character itself - these files are BOM-less
        ' and fbc reads a pasted glyph as its three UTF-8 bytes in Latin-1.
        dim as DWSTRING wszGlyph = iif( gConfig.UnusedSortDesc, GLYPH_CHEVRONDOWN, GLYPH_CHEVRONUP )
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wszGlyph, @rcGlyph, DT_CENTER )
    end if

    p->b->SetFont( ghFont(GUIFONT_9) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' one-pixel trailing divider, matching the shared painter
    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.output.scrollbardivider )
    p->b->PaintRect( @rcDiv )
end sub


' ------------------------------------------------------------------------------------------
' Row activation: double-click OR Enter. 1-BASED to 0-BASED is mandatory here - the engine
' reports 1-based lines and OpenSelectedDocument takes Scintilla's 0-based.
' ------------------------------------------------------------------------------------------
private sub frmUnusedSymbols_Activate( byval hCtl as HWND, byval nRow as integer )
    if nRow < 0 orelse nRow >= gUnusedVisCount then exit sub
    dim as long nModel = gUnusedVisible(nRow)
    if nModel < 0 orelse nModel >= gUnusedRowCount then exit sub

    dim as DWSTRING wszFile = gUnusedRows(nModel).wszFile
    dim as long nLine = gUnusedRows(nModel).nLine - 1
    if nLine < 0 then nLine = 0
    OpenSelectedDocument( wszFile, "", nLine )

    ' Opt-in: collapse to the stub so the report is out of the way of the code just jumped
    ' to. Read from the CONTROL, not gConfig - the control is the live state and gConfig is
    ' only its persisted copy, the same rule the kind mask follows.
    ' AFTER the jump, never before: collapsing first would move the window out from under
    ' the click that is still being processed.
    if IsWindow(gUnusedMinOnOpen) then
        if PsCheckBox_GetChecked( gUnusedMinOnOpen ) then frmUnusedSymbols_Collapse()
    end if
end sub


private sub frmUnusedSymbols_MinOnOpenChanged( byval hCheckBox as HWND, byval isChecked as boolean )
    ' Persisted here rather than in Populate: it filters nothing, so it must not drag a
    ' full list rebuild along with it.
    gConfig.UnusedMinimizeOnOpen = isChecked
end sub


private sub frmUnusedSymbols_ToggleChanged( byval hCheckBox as HWND, byval isChecked as boolean )
    frmUnusedSymbols_Populate()
end sub


' ------------------------------------------------------------------------------------------
sub frmUnusedSymbols_ApplyTheme()
    if IsWindow(HWND_FRMUNUSED) = 0 then exit sub

    ' The caption is drawn by Windows, not by us, so it follows the theme only if we ask.
    ' Every other top-level window here does this from its own ApplyTheme (frmDebug.inc:612,
    ' frmAbout.inc:607, ...); this one did not, so its title bar stayed light under a dark
    ' theme.
    Theme_ApplyDarkCaption( HWND_FRMUNUSED )

    dim as HWND hLV = HWND_FRMUNUSED_LIST
    if IsWindow(hLV) then
        PsListTree_SetBackColor( hLV, theme.output.text.backcolor )
        PsListTree_SetScrollBarColors( hLV, theme.output.scrollbar.backcolor, _
                                       theme.output.scrollbar.forecolor, theme.output.scrollbar.forecolor )
    end if

    AfxRedrawWindow( HWND_FRMUNUSED )
end sub


sub frmUnusedSymbols_PositionWindows()
    if IsWindow(HWND_FRMUNUSED) = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMUNUSED)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMUNUSED, @rc )

    dim as long nHeader  = pWindow->ScaleY( iif( len(gUnusedDirtyText) > 0, 46, 26 ) )
    dim as long nToggles = pWindow->ScaleY(38)
    dim as long nStatus  = pWindow->ScaleY(24)

    dim as FRMUNUSED_LAYOUT lo
    frmUnusedSymbols_ComputeLayout( rc.right, rc.bottom, nHeader, nToggles, nStatus, lo )

    dim as long nPad = pWindow->ScaleX(8)

    ' Refresh first, then the kind filters. Every control measures itself, so nothing here
    ' has to know how wide a caption renders.
    dim as long x = nPad
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        if IsWindow(gUnusedToggle(id)) = 0 then continue for
        dim as long tw, th
        PsCheckBox_GetIdealSize( gUnusedToggle(id), tw, th )
        dim as long ty = lo.rcToggles.top + (lo.rcToggles.bottom - lo.rcToggles.top - th) \ 2
        SetWindowPos( gUnusedToggle(id), 0, x, ty, tw, th, SWP_NOZORDER )
        x += tw + pWindow->ScaleX(12)
    next

    ' The behaviour switch is RIGHT-aligned, so the gap between it and the filters is
    ' whatever is left over - that separation is what says it is not a seventh filter.
    if IsWindow(gUnusedMinOnOpen) then
        dim as long mw, mh
        PsCheckBox_GetIdealSize( gUnusedMinOnOpen, mw, mh )
        dim as long mx = rc.right - nPad - mw
        ' A narrowed window must not slide it left THROUGH the filters: it stops where they
        ' end. Overlapping checkboxes both paint their own background, so the collision is
        ' not a cosmetic overlap, it is one control erased by the other.
        if mx < x then mx = x
        dim as long my = lo.rcToggles.top + (lo.rcToggles.bottom - lo.rcToggles.top - mh) \ 2
        SetWindowPos( gUnusedMinOnOpen, 0, mx, my, mw, mh, SWP_NOZORDER )
    end if

    if IsWindow(HWND_FRMUNUSED_LIST) then
        SetWindowPos( HWND_FRMUNUSED_LIST, 0, nPad, lo.rcList.top, _
                      rc.right - nPad * 2, lo.rcList.bottom - lo.rcList.top, SWP_NOZORDER )
    end if
end sub


private sub frmUnusedSymbols_OnPaint( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )
    b.SetBackColor( theme.panel.text.backcolor )
    b.PaintRect( @rc )
    b.SetFont( ghFont(GUIFONT_9) )

    ' COLLAPSED: the client is one centred invitation and nothing else. Returning here is
    ' what stops the bands being drawn into a client far too small to hold them.
    if gUnusedCollapsed then
        b.SetForeColor( theme.panel.text.forecolor )
        ' PaintText already forces DT_VCENTER or DT_SINGLELINE, so DT_CENTER completes the
        ' horizontal-and-vertical centring
        b.PaintText( L(429, "Click here to restore window"), @rc, DT_CENTER )
        b.EndDoubleBuffer()
        exit sub
    end if

    dim as long nHeader  = pWindow->ScaleY( iif( len(gUnusedDirtyText) > 0, 46, 26 ) )
    dim as long nToggles = pWindow->ScaleY(38)
    dim as long nStatus  = pWindow->ScaleY(24)
    dim as FRMUNUSED_LAYOUT lo
    frmUnusedSymbols_ComputeLayout( rc.right, rc.bottom, nHeader, nToggles, nStatus, lo )

    dim as long nPad = pWindow->ScaleX(8)

    ' --- header: what was scanned, and the stale-line-number warning ---
    dim as RECT rcT = lo.rcHeader
    rcT.left += nPad
    rcT.right -= nPad
    rcT.bottom = rcT.top + pWindow->ScaleY(22)
    b.SetForeColor( theme.panel.text.forecolor )
    b.PaintText( L(415, "Scanned:") & " " & gUnusedScannedText, @rcT, DT_LEFT )

    if len(gUnusedDirtyText) > 0 then
        dim as RECT rcW = lo.rcHeader
        rcW.left += nPad
        rcW.right -= nPad
        rcW.top = rcT.bottom
        ' the one line here that must not be missed
        b.SetForeColor( theme.compile.iconfail )
        b.PaintText( L(416, "Unsaved changes (line numbers may be stale):") & " " & gUnusedDirtyText, _
                     @rcW, DT_LEFT )
        b.SetForeColor( theme.panel.text.forecolor )
    end if

    ' (the kind filters are PsCheckBoxes and paint their own captions)

    ' --- status line ---
    dim as RECT rcS = lo.rcStatus
    rcS.left += nPad
    rcS.right -= nPad
    dim as DWSTRING wszStatus
    if gUnusedPending then
        ' an empty list while a scan is running must NOT read as "nothing found"
        wszStatus = L(422, "Scanning...")
    elseif gUnusedRowCount = 0 then
        wszStatus = L(417, "No unused symbols found")
    else
        wszStatus = wstr(gUnusedVisCount) & " / " & wstr(gUnusedRowCount) & " " & L(419, "shown")
    end if
    b.PaintText( wszStatus, @rcS, DT_LEFT )

    ' --- the caution, right-aligned in the same band ---
    ' This report is EVIDENCE, not a verdict: counts are per scan, so a proc called only
    ' from a file this scan never reached reads as unused, and anything resolved by name at
    ' runtime is invisible to a parser. Deleting straight off the list loses code.
    '
    ' Painted in the ordinary foreground rather than the warning colour the unsaved-buffer
    ' banner uses: this line never changes, and a permanently red strip along the bottom
    ' stops being read within a day.
    dim as DWSTRING wszCaution = L(431, "Review before deleting - not every reference is visible to a scan.")
    dim as long nLeftW  = getTextWidth( hwnd, wszStatus, GUIFONT_9 )
    dim as long nRightW = getTextWidth( hwnd, wszCaution, GUIFONT_9 )

    ' It gives way rather than colliding: the count is data, this is advice.
    if frmUnusedSymbols_CautionFits( rcS.right - rcS.left, nLeftW, nRightW, _
                                     pWindow->ScaleX(24) ) then
        b.PaintText( wszCaution, @rcS, DT_RIGHT )
    end if

    b.EndDoubleBuffer()
end sub


' ------------------------------------------------------------------------------------------
' Collapse to the stub, and come back. The window is never made ICONIC - see the .bi.
' ------------------------------------------------------------------------------------------
private sub frmUnusedSymbols_SetChildrenVisible( byval bShow as boolean )
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    if IsWindow(HWND_FRMUNUSED_LIST) then ShowWindow( HWND_FRMUNUSED_LIST, nCmd )
    if IsWindow(gUnusedMinOnOpen) then ShowWindow( gUnusedMinOnOpen, nCmd )
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        if IsWindow(gUnusedToggle(id)) then ShowWindow( gUnusedToggle(id), nCmd )
    next
end sub


sub frmUnusedSymbols_Collapse()
    if IsWindow(HWND_FRMUNUSED) = 0 then exit sub
    if gUnusedCollapsed then exit sub

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMUNUSED)
    if pWindow = 0 then exit sub

    ' remember where to come back to BEFORE moving anything
    GetWindowRect( HWND_FRMUNUSED, @gUnusedRestoreRect )
    gUnusedCollapsed = true

    ' The children are hidden rather than left behind the stub's paint: the list would
    ' otherwise still be there, still hit-testable, and would swallow the restore click.
    frmUnusedSymbols_SetChildrenVisible( false )

    dim as RECT rcAnchor, rcStub
    frmUnusedSymbols_AnchorRect( rcAnchor )
    frmUnusedSymbols_ComputeStubRect( rcAnchor, _
        pWindow->ScaleX(FRMUNUSED_STUBWIDTH), pWindow->ScaleY(FRMUNUSED_STUBHEIGHT), _
        pWindow->ScaleX(FRMUNUSED_STUBMARGIN), rcStub )

    SetWindowPos( HWND_FRMUNUSED, 0, rcStub.left, rcStub.top, _
                  rcStub.right - rcStub.left, rcStub.bottom - rcStub.top, SWP_NOZORDER )
    AfxRedrawWindow( HWND_FRMUNUSED )
end sub


private sub frmUnusedSymbols_RestoreFromStub()
    if IsWindow(HWND_FRMUNUSED) = 0 then exit sub
    if gUnusedCollapsed = false then exit sub

    gUnusedCollapsed = false
    SetWindowPos( HWND_FRMUNUSED, 0, gUnusedRestoreRect.left, gUnusedRestoreRect.top, _
                  gUnusedRestoreRect.right - gUnusedRestoreRect.left, _
                  gUnusedRestoreRect.bottom - gUnusedRestoreRect.top, SWP_NOZORDER )

    frmUnusedSymbols_SetChildrenVisible( true )
    ' the bands are sized from the client, which has just changed
    frmUnusedSymbols_PositionWindows()
    AfxRedrawWindow( HWND_FRMUNUSED )
end sub


' ------------------------------------------------------------------------------------------
private function frmUnusedSymbols_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                           byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case WM_SYSCOMMAND
            ' The minimize button never reaches Windows: an owned window has no taskbar
            ' button, so a real minimize would put the report somewhere unreachable.
            ' Collapse to the stub instead. Covers the caption button, Alt+Space > Minimize
            ' and anything else that raises SC_MINIMIZE. The low four bits carry internal
            ' state and must be masked off before comparing.
            if (wParam and &hFFF0) = SC_MINIMIZE then
                frmUnusedSymbols_Collapse()
                return 0
            end if

        case WM_LBUTTONDOWN
            ' Collapsed, the whole client IS the restore button - the children are hidden,
            ' so nothing else can claim the click.
            if gUnusedCollapsed then
                frmUnusedSymbols_RestoreFromStub()
                return 0
            end if

        case WM_SETCURSOR
            ' say it is clickable, since the stub looks like a label
            if gUnusedCollapsed andalso (loword(lParam) = HTCLIENT) then
                SetCursor( LoadCursor( 0, IDC_HAND ) )
                return true
            end if

        case WM_SIZE
            ' A minimize sends WM_SIZE with a 0x0 client. Re-laying the bands out against
            ' it is pointless work on a window nobody can see, and it is how a restored
            ' window comes back with everything collapsed.
            if wParam = SIZE_MINIMIZED then return 0
            ' collapsed: the children are hidden and the client belongs to the stub message
            if gUnusedCollapsed then
                AfxRedrawWindow( hwnd )
                return 0
            end if
            frmUnusedSymbols_PositionWindows()
            AfxRedrawWindow( hwnd )
            return 0

        case WM_ERASEBKGND
            return true

        case WM_PAINT
            frmUnusedSymbols_OnPaint( hwnd )
            return 0

        case WM_CLOSE
            ' CLOSED WHILE COLLAPSED: persist nothing. The live geometry is the stub's -
            ' a corner of the editing area, 280x110 - and saving that would reopen the
            ' report as a stub-sized window in the corner next time.
            if gUnusedCollapsed then
                DestroyWindow( hwnd )
                return 0
            end if

            ' remember where the window was, the way frmDebug does
            dim as WINDOWPLACEMENT wp
            wp.length = sizeof(WINDOWPLACEMENT)
            if GetWindowPlacement( hwnd, @wp ) then
                gUnusedPos.bInitialized = true
                ' Closed while MINIMIZED: rcNormalPosition still holds the restored
                ' geometry, and showCmd is SW_SHOWMINIMIZED - which this deliberately does
                ' not persist. Reopening a window already minimized would look like the
                ' menu command doing nothing.
                gUnusedPos.bMaximized   = (wp.showCmd = SW_SHOWMAXIMIZED)
                gUnusedPos.nLeft   = wp.rcNormalPosition.left
                gUnusedPos.nTop    = wp.rcNormalPosition.top
                gUnusedPos.nRight  = wp.rcNormalPosition.right
                gUnusedPos.nBottom = wp.rcNormalPosition.bottom
            end if
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            HWND_FRMUNUSED = 0
            HWND_FRMUNUSED_LIST = 0
            gUnusedCollapsed = false
            gUnusedMinOnOpen = 0
            for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
                gUnusedToggle(id) = 0
            next
            erase gUnusedRows
            gUnusedRowCount = 0
            gUnusedVisCount = 0
            return 0

        case WM_NCDESTROY
            dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
            if pWindow then delete pWindow
            return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function
' ------------------------------------------------------------------------------------------
' One place for the checkbox palette, so the six kind filters and the behaviour switch
' beside them cannot drift apart.
' ------------------------------------------------------------------------------------------
        ' THESE WERE NEVER THEMED. Every other setter above was called and this one was
        ' not, so the five filters rendered in PsCheckBox's library defaults -- a fixed
        ' dark palette matching nothing in this window. tiko has no other PsCheckBox host,
        ' which is why nothing else had to notice.
        '
        ' THE PANEL PALETTE, because that is what frmUnusedSymbols_OnPaint fills its own
        ' client rect with. Getting this wrong is VISIBLE rather than subtle: a checkbox
        ' whose BackColor differs from the surface behind it paints its whole control rect
        ' in that colour, so the five filters read as five raised tiles on the pane instead
        ' of as text on it. The library default and the Output palette are both wrong here
        ' for the same reason -- neither is what this window paints.
private sub frmUnusedSymbols_ApplyCheckBoxColors( byval h as HWND )
    if IsWindow(h) = 0 then exit sub
    dim as PSCHECKBOX_COLORS cc
            cc.BackColor               = theme.panel.text.backcolor
            cc.BackColorHot            = theme.panel.text.backcolor
            cc.BackColorPressed        = theme.panel.text.backcolor
            cc.BackColorDisabled       = theme.panel.text.backcolor
            cc.ForeColor               = theme.panel.text.forecolor
            cc.ForeColorHot            = theme.panel.texthot.forecolor
            cc.ForeColorPressed        = theme.panel.texthot.forecolor
            cc.ForeColorDisabled       = theme.role.clr(THROLE_FOREGROUNDDIM)
            ' The box is ONE COMPOSITE GLYPH -- outline and tick are a single piece of ink
            ' in a single colour -- so unchecked wants a muted outline and checked wants
            ' the accent. There is no fill/tick split to reach for.
            cc.BoxColor                = theme.role.clr(THROLE_FOREGROUNDDIM)
            cc.BoxColorHot             = theme.panel.texthot.forecolor
            cc.BoxColorPressed         = theme.panel.texthot.forecolor
            cc.BoxColorDisabled        = theme.role.clr(THROLE_FOREGROUNDDIM)
            ' ACCENTFORE, not ui.accent. The box glyph is INK, and ui.accent resolves to
            ' toptabs.iconselected -- a colour picked to sit behind an icon, which reads
            ' too dark as a stroke on this background. That is the rule CLAUDE.md states
            ' (the accent is a fill, never a foreground) and this is what breaking it
            ' looks like.
            cc.BoxColorChecked         = theme.role.clr(THROLE_ACCENTFORE)
            cc.BoxColorCheckedHot      = theme.role.clr(THROLE_ACCENTFORE)
            cc.BoxColorCheckedPressed  = theme.role.clr(THROLE_ACCENTFORE)
            cc.BoxColorCheckedDisabled = theme.role.clr(THROLE_FOREGROUNDDIM)
            cc.FocusRingColor          = theme.role.clr(THROLE_ACCENTFORE)
            PsCheckBox_SetColors( h, @cc )
end sub



' ------------------------------------------------------------------------------------------
function frmUnusedSymbols_Show( byval hWndParent as HWND ) as LRESULT
    if IsWindow(HWND_FRMUNUSED) then
        ' Re-invoking the menu command while the report is parked should bring it back,
        ' not just raise a stub the user then has to click as well.
        if gUnusedCollapsed then frmUnusedSymbols_RestoreFromStub()
        SetForegroundWindow( HWND_FRMUNUSED )
        return 0
    end if

    ' What should this report cover? With no project open that is the active document, so
    ' the window works on loose files instead of refusing.
    dim as DWSTRING wszRoot = frmUnusedSymbols_RootFile()
    if PsLen( wszRoot ) = 0 then
        ' nothing scannable at all: no document open, or the only one has never been saved
        TikoMsgBox( hWndParent, L(420, "Open or save a source file first"), L(214, "Confirm"), _
                    TMB_ICON_INFO, TMB_OK )
        return 0
    end if

    ' Is the scan we already hold the right one? A scan rooted at some other file answers a
    ' different question, so compare roots rather than merely testing for presence.
    gUnusedPending = true
    dim as PARSERESULTSET ptr pExisting = gSymDb.ProjectSet()
    if pExisting then
        if lstrcmpiW( @pExisting->wszRootFile, wszRoot ) = 0 then gUnusedPending = false
    end if

    if gUnusedPending then
        ' Request it and open ANYWAY, showing "Scanning...". The rebuild lands in frmMain's
        ' MSG_USER_PARSE_COMPLETE handler. Refusing here and making the user find the menu
        ' again is the behaviour this replaces.
        ' The override is passed only when there is no project: with one open it would
        ' replace the project's own scan with a single file's.
        ' Root the scan at the workspace's Main when there is one. With no Main there is
        ' nothing to walk includes from, so scan just this file -- which also keeps an
        ' untitled workspace holding a pile of unrelated loose files from triggering a
        ' whole-workspace scan the user did not ask for.
        if gApp.GetMainDocumentPtr() then
            gScanMgr.RequestProjectScan()
        else
            gScanMgr.RequestProjectScan( wszRoot )
        end if
    end if

    dim pWindow as CWindow ptr = new CWindow
    dim pParent as CWindow ptr = AfxCWindowPtr(hWndParent)
    if pParent then pWindow->DPI = pParent->DPI

    ' WS_MINIMIZEBOX gives the caption its button, but SC_MINIMIZE is intercepted and the
    ' window collapses to a stub instead - see the .bi. No WS_EX_APPWINDOW: that was only
    ' ever needed to give a REAL minimize somewhere to come back from, and this window
    ' never becomes iconic, so it does not need a taskbar button of its own.
    HWND_FRMUNUSED = pWindow->Create( hWndParent, L(409, "Unused Symbols"), @frmUnusedSymbols_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_THICKFRAME or WS_CAPTION or WS_SYSMENU or WS_MINIMIZEBOX or _
        WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT )
    ' UNSCALED: SetClientSize applies the DPI factor itself, so a pre-scaled value squares it
    pWindow->SetClientSize( FRMUNUSED_DEFWIDTH, FRMUNUSED_DEFHEIGHT )
    pWindow->Center( HWND_FRMUNUSED, hWndParent )

    if gUnusedPos.bInitialized then
        dim as WINDOWPLACEMENT wp
        wp.length = sizeof(WINDOWPLACEMENT)
        wp.rcNormalPosition.left   = gUnusedPos.nLeft
        wp.rcNormalPosition.top    = gUnusedPos.nTop
        wp.rcNormalPosition.right  = gUnusedPos.nRight
        wp.rcNormalPosition.bottom = gUnusedPos.nBottom
        wp.showCmd = iif( gUnusedPos.bMaximized, SW_SHOWMAXIMIZED, SW_SHOWNORMAL )
        SetWindowPlacement( HWND_FRMUNUSED, @wp )
    end if

    pWindow->BigIcon   = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 32, 32, LR_SHARED)
    pWindow->SmallIcon = LoadImage( pWindow->InstanceHandle, "IMAGE_AAA_MAINICON", IMAGE_ICON, 16, 16, LR_SHARED)

    ' --- kind filters ---
    ' PsCheckBox, not PsToggle: these are filters rather than settings, and a checkbox
    ' carries its own caption. With toggles this window had to paint six captions itself
    ' and keep their measured widths in step with the layout by hand, in two places.
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        dim as HWND h = PsCheckBox_Create( HWND_FRMUNUSED, id )
        gUnusedToggle(id) = h
        PsCheckBox_SetText( h, UnusedSymbols_KindText( id - IDC_FRMUNUSED_TOGGLEFIRST ) )
        PsCheckBox_SetFont( h, ghFont(GUIFONT_9) )
        PsCheckBox_SetGlyphFont( h, ghFont(SYMBOLFONT_10) )
        PsCheckBox_SetChecked( h, (gConfig.UnusedKindMask and (1 shl (id - IDC_FRMUNUSED_TOGGLEFIRST))) <> 0 )
        PsCheckBox_SetCheckChangedCallback( h, @frmUnusedSymbols_ToggleChanged )
        frmUnusedSymbols_ApplyCheckBoxColors( h )
    next

    ' --- "minimize when a row is opened", right-aligned and apart from the filters ---
    ' Deliberately NOT part of the TOGGLEFIRST..TOGGLELAST block: that block IS the filter
    ' set and is walked to build the kind mask. This is a behaviour switch, and both its id
    ' and its placement say so.
    gUnusedMinOnOpen = PsCheckBox_Create( HWND_FRMUNUSED, IDC_FRMUNUSED_MINONOPEN )
    PsCheckBox_SetText( gUnusedMinOnOpen, L(399, "Minimize when a row is opened") )
    PsCheckBox_SetFont( gUnusedMinOnOpen, ghFont(GUIFONT_9) )
    PsCheckBox_SetGlyphFont( gUnusedMinOnOpen, ghFont(SYMBOLFONT_10) )
    PsCheckBox_SetChecked( gUnusedMinOnOpen, gConfig.UnusedMinimizeOnOpen )
    PsCheckBox_SetCheckChangedCallback( gUnusedMinOnOpen, @frmUnusedSymbols_MinOnOpenChanged )
    frmUnusedSymbols_ApplyCheckBoxColors( gUnusedMinOnOpen )

    ' --- list ---
    dim as HWND hLV = PsListTree_Create( HWND_FRMUNUSED, IDC_FRMUNUSED_LIST )
    HWND_FRMUNUSED_LIST = hLV
    PsListTree_AddColumn( hLV, L(410, "File"),   pWindow->ScaleX(300) )
    PsListTree_AddColumn( hLV, L(411, "Line"),   pWindow->ScaleX(60) )
    PsListTree_AddColumn( hLV, L(412, "Class"),  pWindow->ScaleX(90) )
    PsListTree_AddColumn( hLV, L(413, "Name"),   pWindow->ScaleX(260) )
    PsListTree_AddColumn( hLV, L(414, "Status"), pWindow->ScaleX(140) )
    PsListTree_SetFillColumn( hLV, UnusedColName )
    PsListTree_ShowHeader( hLV, true )
    PsListTree_SetPaintCallback( hLV, @ListBox_LVPaintRow )
    ' LOCAL header painter: the shared one draws no sort glyph (see the .bi header)
    PsListTree_SetHeaderPaintCallback( hLV, @frmUnusedSymbols_PaintHeader )
    PsListTree_SetColumnClickCallback( hLV, @frmUnusedSymbols_HeaderClick )
    PsListTree_SetColumnAutoSizeCallback( hLV, @ListBox_LVAutoSizeColumn )
    PsListTree_SetActivateCallback( hLV, @frmUnusedSymbols_Activate )
    PsListTree_SetFont( hLV, ghFont(GUIFONT_9) )

    frmUnusedSymbols_ApplyTheme()

    ' Every Ps* control is created WS_CHILD but NOT WS_VISIBLE - the family leaves showing
    ' them to the host, which has to position them first.
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        if IsWindow(gUnusedToggle(id)) then ShowWindow( gUnusedToggle(id), SW_SHOW )
    next
    if IsWindow(gUnusedMinOnOpen) then ShowWindow( gUnusedMinOnOpen, SW_SHOW )
    ShowWindow( HWND_FRMUNUSED_LIST, SW_SHOW )

    ' Builds the model, positions the bands and fills the list. Skipped while a scan is
    ' still pending - Rebuild() clears the pending flag, so calling it now would report
    ' "No unused symbols found" over a scan that has not run.
    if gUnusedPending then
        gUnusedScannedText = FilenameOriginalCase( wszRoot )
        gUnusedRowCount = 0
        gUnusedVisCount = 0
        frmUnusedSymbols_PositionWindows()
    else
        frmUnusedSymbols_Rebuild()
    end if

    ShowWindow( HWND_FRMUNUSED, SW_SHOW )
    UpdateWindow( HWND_FRMUNUSED )

    frmUnusedSymbols_RunLayoutSelfTest()
    return 0
end function


' ==========================================================================================
' TIKO_UNUSED_SELFTEST=1, layout half. Runs from _Show, so it measures REAL windows at the
' real DPI.
'
' What it cannot reach: the sort GLYPH rendering in the right column, a row click landing on
' the right line, whether Refresh actually re-scans, and whether any of this is readable in
' either theme. Those are the interactive pass.
' ==========================================================================================
sub frmUnusedSymbols_RunLayoutSelfTest()
    if environ("TIKO_UNUSED_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    print "--- TIKO_UNUSED_SELFTEST (layout, live window) ---"

    ' ---- band tiling, pure ----------------------------------------------------------
    dim as FRMUNUSED_LAYOUT lo
    dim as long cx = 1000, cy = 640
    frmUnusedSymbols_ComputeLayout( cx, cy, 26, 38, 24, lo )

    if lo.rcToggles.top <> lo.rcHeader.bottom then
        nFail += 1 : print "  FAIL the filter row does not start where the header ends"
    else
        nPass += 1
    end if
    if lo.rcList.top <> lo.rcToggles.bottom then
        nFail += 1 : print "  FAIL the list does not start where the filter row ends"
    else
        nPass += 1
    end if
    if lo.rcStatus.top <> lo.rcList.bottom then
        nFail += 1 : print "  FAIL the status line does not start where the list ends"
    else
        nPass += 1
    end if
    if lo.rcStatus.bottom <> cy then
        nFail += 1 : print "  FAIL the bands do not reach the bottom of the client"
    else
        nPass += 1
    end if

    ' The list must occupy real space. This is the assertion that would have caught a
    ' band-arithmetic slip leaving it zero-height, which looks exactly like "no results".
    if (lo.rcList.bottom - lo.rcList.top) > 100 then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the list band has no usable height: " & _
                           str(lo.rcList.bottom - lo.rcList.top)
    end if

    ' a window dragged very short must not give the list a negative height
    frmUnusedSymbols_ComputeLayout( cx, 40, 26, 38, 24, lo )
    if lo.rcList.bottom < lo.rcList.top then
        nFail += 1 : print "  FAIL a too-short window gives the list a negative height"
    else
        nPass += 1
    end if

    ' ---- the live window ------------------------------------------------------------
    if IsWindow(HWND_FRMUNUSED_LIST) then
        if PsListTree_GetColumnCount( HWND_FRMUNUSED_LIST ) = UNUSED_COLUMN_COUNT then
            nPass += 1
        else
            nFail += 1 : print "  FAIL the list does not have " & str(UNUSED_COLUMN_COUNT) & " columns"
        end if

        dim as RECT rcCtl
        GetWindowRect( HWND_FRMUNUSED_LIST, @rcCtl )
        if (rcCtl.right > rcCtl.left) andalso (rcCtl.bottom > rcCtl.top) then
            nPass += 1
        else
            nFail += 1 : print "  FAIL the list has no area"
        end if

        if IsWindowVisible( HWND_FRMUNUSED_LIST ) then
            nPass += 1
        else
            ' the Ps* family creates children hidden; forgetting the ShowWindow gives a
            ' window that paints its background and nothing else
            nFail += 1 : print "  FAIL the list is not visible"
        end if
    end if

    ' every kind filter must exist, be visible, and have a non-zero size. A PsCheckBox sizes
    ' itself from its CAPTION, so a zero width here is also how a missing .lang id shows up.
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST
        dim as boolean bOK = false
        if IsWindow(gUnusedToggle(id)) then
            dim as RECT rcT
            GetWindowRect( gUnusedToggle(id), @rcT )
            bOK = IsWindowVisible( gUnusedToggle(id) ) andalso (rcT.right > rcT.left)
        end if
        if bOK then
            nPass += 1
        else
            nFail += 1 : print "  FAIL kind filter " & str(id) & " is missing, hidden or zero-sized"
        end if
    next

    ' The filters are laid out left to right from their own measured widths, so an overlap
    ' is what a wrong ideal-size read looks like on screen.
    dim as boolean bNoOverlap = true
    for id as long = IDC_FRMUNUSED_TOGGLEFIRST to IDC_FRMUNUSED_TOGGLELAST - 1
        if IsWindow(gUnusedToggle(id)) = 0 orelse IsWindow(gUnusedToggle(id+1)) = 0 then continue for
        dim as RECT rcA, rcB2
        GetWindowRect( gUnusedToggle(id), @rcA )
        GetWindowRect( gUnusedToggle(id+1), @rcB2 )
        if rcB2.left < rcA.right then bNoOverlap = false
    next
    if bNoOverlap then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the kind filters overlap each other"
    end if

    ' the caption needs the button the collapse hangs off
    if IsWindow(HWND_FRMUNUSED) then
        if (GetWindowLongPtr( HWND_FRMUNUSED, GWL_STYLE ) and WS_MINIMIZEBOX) <> 0 then
            nPass += 1
        else
            nFail += 1 : print "  FAIL the window has no minimize box"
        end if
    end if

    ' ---- the collapsed stub's placement, pure ----------------------------------------
    ' A sign error here parks the stub off-screen, where the only symptom is a report that
    ' seems to have vanished - so it is asserted rather than eyeballed.
    dim as RECT rcAnchor, rcStub
    SetRect( @rcAnchor, 100, 200, 900, 800 )        ' editing area, screen coords
    frmUnusedSymbols_ComputeStubRect( rcAnchor, 280, 110, 12, rcStub )

    if (rcStub.right = rcAnchor.right - 12) andalso (rcStub.top = rcAnchor.top + 12) then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the stub is not inset from the anchor TOP-RIGHT: " & _
                           str(rcStub.left) & "," & str(rcStub.top)
    end if
    if ((rcStub.right - rcStub.left) = 280) andalso ((rcStub.bottom - rcStub.top) = 110) then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the stub is not the requested size"
    end if
    if (rcStub.left >= rcAnchor.left) andalso (rcStub.right <= rcAnchor.right) then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the stub is not inside the editing area"
    end if

    ' ---- the behaviour switch is present, right-aligned, and clear of the filters ----
    ' Two PsCheckBoxes that overlap do not merely look untidy: each paints its own
    ' background over its whole control rect, so one erases the other.
    if IsWindow(gUnusedMinOnOpen) then
        dim as RECT rcMin, rcLast
        GetWindowRect( gUnusedMinOnOpen, @rcMin )
        if IsWindowVisible( gUnusedMinOnOpen ) andalso (rcMin.right > rcMin.left) then
            nPass += 1
        else
            nFail += 1 : print "  FAIL the minimize-on-open checkbox is hidden or zero-sized"
        end if

        if IsWindow(gUnusedToggle(IDC_FRMUNUSED_TOGGLELAST)) then
            GetWindowRect( gUnusedToggle(IDC_FRMUNUSED_TOGGLELAST), @rcLast )
            if rcMin.left >= rcLast.right then
                nPass += 1
            else
                nFail += 1 : print "  FAIL it overlaps the last kind filter"
            end if
        end if

        ' RIGHT-ALIGNED means its right edge sits at the client's right edge, less the
        ' padding - NOT merely "somewhere in the right half". The weaker test passed with
        ' the control packed hard against the filters, because six filters already reach
        ' past the midpoint; it was measuring their width, not its alignment.
        dim as RECT rcClient
        GetClientRect( HWND_FRMUNUSED, @rcClient )
        dim as POINT ptTR = ( rcClient.right, rcClient.top )
        ClientToScreen( HWND_FRMUNUSED, @ptTR )
        dim as long nSlack = AfxScaleX(14)          ' the 8px pad, plus rounding
        if (ptTR.x - rcMin.right) <= nSlack then
            nPass += 1
        else
            nFail += 1 : print "  FAIL it is not right-aligned: " & _
                               str(ptTR.x - rcMin.right) & "px from the right edge"
        end if
    end if

    ' its persisted setting must round-trip through the control
    if IsWindow(gUnusedMinOnOpen) then
        if PsCheckBox_GetChecked( gUnusedMinOnOpen ) = gConfig.UnusedMinimizeOnOpen then
            nPass += 1
        else
            nFail += 1 : print "  FAIL the checkbox does not reflect gConfig.UnusedMinimizeOnOpen"
        end if
    end if

    ' ---- the status band's caution gives way rather than colliding ------------------
    ' Its whole point is that it disappears instead of smearing over the count, and the
    ' width that triggers that depends on the TRANSLATION, so it cannot be eyeballed once
    ' in English and called done.
    if frmUnusedSymbols_CautionFits( 1000, 120, 400, 24 ) then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the caution is dropped even though it fits"
    end if
    if frmUnusedSymbols_CautionFits( 300, 120, 400, 24 ) = false then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the caution would be painted over the count"
    end if
    ' exactly touching is a fit; one pixel more is not
    if frmUnusedSymbols_CautionFits( 544, 120, 400, 24 ) andalso _
       (frmUnusedSymbols_CautionFits( 543, 120, 400, 24 ) = false) then
        nPass += 1
    else
        nFail += 1 : print "  FAIL the fit boundary is off by one"
    end if
    ' a caution that measured as nothing must not be painted
    if frmUnusedSymbols_CautionFits( 1000, 120, 0, 24 ) = false then
        nPass += 1
    else
        nFail += 1 : print "  FAIL an empty caution is still painted"
    end if

    ' an editing area NARROWER than the stub must not push it off the left edge
    SetRect( @rcAnchor, 100, 200, 300, 800 )
    frmUnusedSymbols_ComputeStubRect( rcAnchor, 280, 110, 12, rcStub )
    if rcStub.left >= rcAnchor.left then
        nPass += 1
    else
        nFail += 1 : print "  FAIL a narrow editing area pushes the stub off its left edge"
    end if

    ' The engine uppercases filenames; the report shows them in their original casing.
    if gUnusedRowCount > 0 then
        if gUnusedRows(0).wszFile <> ucase( gUnusedRows(0).wszFile ) then
            nPass += 1
        else
            nFail += 1 : print "  FAIL filenames are still uppercased (FilenameOriginalCase not applied)"
        end if
    end if

    dim as string sOut = "--- TIKO_UNUSED_SELFTEST (layout): " & nPass & " passed, " & nFail & " failed ---"
    print sOut
end sub
