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
''  frmAutoComplete.inc
''
''  See frmAutoComplete.bi for the design and the two rules that are easy to break.
''

#include once "frmAutoComplete.bi"


' The full candidate set as handed to frmAutoComplete_Show, and the subset currently
' passing the typed prefix. gAutoCVisible(i) is the row i of the listbox -- the painter
' indexes straight through it, so no per-row itemData is needed.
redim shared gAutoCItems(any) as AUTOC_ITEM
redim shared gAutoCVisible(any) as long
dim shared gAutoCCount    as long          ' logical length of gAutoCItems (array may be larger)
dim shared gAutoCVisCount as long          ' logical length of gAutoCVisible
dim shared gAutoCOpen     as boolean
' Row height in UNSCALED units. PsListTree_SetRowHeight DPI-scales what it is handed
' (and WM_MEASUREITEM scales it again), so a pre-scaled value here would be scaled
' TWICE -- rows too tall, and a popup window shorter than its own contents because the
' window height was computed from the unscaled figure. Keep this unscaled and scale
' only where the window size is derived.
dim shared gAutoCRowHeight as long = AUTOCOMPLETE_DEFAULT_ROW_HEIGHT
dim shared gAutoCDetailWidth as long       ' measured width of the detail column, set by AutoC_Layout
dim shared ghAutoCList    as HWND          ' the PsListTree child
dim shared ghAutoCEdit    as HWND          ' the Scintilla window the open popup belongs to

' Per-popup geometry trace, on with TIKO_AUTOC_TRACE=1. Read once at Init: this prints
' on every keystroke that re-filters, so it is not something to leave on.
dim shared gAutoCTrace    as boolean


' ========================================================================================
' Glyph and colour for a candidate's kind. Colours come from the EDITOR theme, not the
' panel theme, so the popup reads as part of the code window it overlaps.
' ========================================================================================
private function AutoC_KindGlyph( byval nKind as long ) as DWSTRING
    select case nKind
        case FBCP_KIND_SUB, FBCP_KIND_FUNCTION
            return wszAutoCGlyphProc
        case FBCP_KIND_TYPE, FBCP_KIND_UNION, FBCP_KIND_TYPEDEF
            return wszAutoCGlyphType
        case FBCP_KIND_ENUM, FBCP_KIND_ENUMVAL
            return wszAutoCGlyphEnum
        case AUTOC_KIND_KEYWORD, AUTOC_KIND_STDTYPE, FBCP_KIND_DEFINE
            return wszAutoCGlyphKeyword
        case else
            ' PARAM, FIELD, VAR, CONST, NAMESPACE: everything that names a value.
            return wszAutoCGlyphVar
    end select
end function

private function AutoC_KindColor( byval nKind as long ) as COLORREF
    select case nKind
        case FBCP_KIND_SUB, FBCP_KIND_FUNCTION
            return ghEditor.ForeColorKeyword2
        case FBCP_KIND_TYPE, FBCP_KIND_UNION, FBCP_KIND_TYPEDEF
            return ghEditor.ForeColorKeyword3
        case FBCP_KIND_ENUM, FBCP_KIND_ENUMVAL
            return ghEditor.ForeColorNumbers
        case AUTOC_KIND_KEYWORD, AUTOC_KIND_STDTYPE, FBCP_KIND_DEFINE
            return ghEditor.ForeColorKeyword
        case else
            return ghEditor.ForeColorText
    end select
end function


' ========================================================================================
' frmAutoComplete_PaintCallback
'
' Three columns: a kind glyph in a fixed left gutter, the name, and a dimmed
' right-aligned detail. The columns are laid out here rather than through PsListTree's
' PsColumnHeader support because there is no header band and the widths are decided once
' per popup by AutoC_Layout.
' ========================================================================================
private sub frmAutoComplete_PaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMAUTOCOMPLETE)
    if pWindow = 0 then exit sub

    ' The paint callback gets a model row, and rows map 1:1 onto the filtered set.
    if (p->itemID < 0) orelse (p->itemID >= gAutoCVisCount) then exit sub
    dim as long nItem = gAutoCVisible(p->itemID)
    if (nItem < 0) orelse (nItem >= gAutoCCount) then exit sub

    ' State priority for the row: selected > hot > normal.
    dim as COLORREF backclr, foreclr, detailclr, glyphclr
    if p->isSelected then
        backclr   = ghEditor.BackColorSelection
        foreclr   = ghEditor.ForeColorSelection
        detailclr = ghEditor.ForeColorSelection
        glyphclr  = ghEditor.ForeColorSelection
    elseif p->isHot then
        backclr   = ghEditor.BackColorCurrentLine
        foreclr   = ghEditor.ForeColorText
        detailclr = ghEditor.ForeColorComments
        glyphclr  = AutoC_KindColor( gAutoCItems(nItem).nKind )
    else
        backclr   = ghEditor.BackColorText
        foreclr   = ghEditor.ForeColorText
        detailclr = ghEditor.ForeColorComments
        glyphclr  = AutoC_KindColor( gAutoCItems(nItem).nKind )
    end if

    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )

    dim as long nGlyphWidth = pWindow->ScaleX( AUTOCOMPLETE_GLYPH_WIDTH )
    dim as long nPadRight   = pWindow->ScaleX( AUTOCOMPLETE_PAD_RIGHT )

    ' Kind glyph. Drawn in the GUI font (not SYMBOLFONT) because these are plain
    ' geometric characters -- see the wszAutoCGlyph* definitions in modDeclares.bi.
    dim as RECT rcGlyph = p->rc
    rcGlyph.right = rcGlyph.left + nGlyphWidth
    p->b->SetForeColor( glyphclr )
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( AutoC_KindGlyph( gAutoCItems(nItem).nKind ), @rcGlyph, DT_CENTER )

    ' Detail (right-aligned, dimmed). Painted BEFORE the name so the name's rect can be
    ' clipped short of it and the two can never overlap on a narrow popup.
    dim as long nDetailWidth = 0
    dim as DWSTRING wszDetail = gAutoCItems(nItem).wszDetail
    if len(wszDetail) then
        dim as RECT rcDetail = p->rc
        rcDetail.right -= nPadRight
        rcDetail.left = rcGlyph.right
        p->b->SetForeColor( detailclr )
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->PaintText( wszDetail, @rcDetail, DT_RIGHT or DT_END_ELLIPSIS )
        nDetailWidth = gAutoCDetailWidth + pWindow->ScaleX( AUTOCOMPLETE_COLUMN_GAP )
    end if

    ' Name.
    dim as RECT rcText = p->rc
    rcText.left  = rcGlyph.right
    rcText.right = rcText.right - nPadRight - nDetailWidth
    if rcText.right < rcText.left then rcText.right = rcText.left
    p->b->SetForeColor( foreclr )
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( gAutoCItems(nItem).wszName, @rcText, DT_LEFT or DT_END_ELLIPSIS )

end sub


' ========================================================================================
' frmAutoComplete_TooltipCallback
'
' Suppresses tooltips outright. PsListTree creates a tooltip for every instance and, with
' NO callback set, falls back to the row's own text (PsListTree_ResolveTooltipText) -- so
' every row would pop a tip repeating the name already under the cursor. Returning an
' empty string is how a host says "no tip for this row"; the completion list wants that
' for all of them, since the row IS the text and the detail column carries the rest.
' ========================================================================================
private function frmAutoComplete_TooltipCallback( byval hCtl as HWND, byval row as integer ) as DWSTRING
    return ""
end function


' ========================================================================================
' frmAutoComplete_MessageCallback
'
' The click path. The owner-drawn PsListTree surface takes keyboard focus in its own
' WM_LBUTTONDOWN -- SetFocus on its container, BEFORE this callback runs -- exactly as the
' old system listbox did in its default handling. Either way that pulls focus off Scintilla,
' which kills the caret and stops the pump filter from seeing keystrokes, so we put focus
' straight back on the editor here and consume the message. The control takes no capture (it
' implements no drag gesture), so there is none to strand by swallowing the up.
'
' The model selection is still applied: PsListTree_ApplyClickSelection runs BEFORE this
' callback is invoked. So the down selects, and the up commits.
' ========================================================================================
private function frmAutoComplete_MessageCallback( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

    select case m->uMsg

        case WM_LBUTTONDOWN
            ' Undo the surface's focus grab: the editor must keep focus for the pump filter.
            if ghAutoCEdit then SetFocus( ghAutoCEdit )
            return true          ' select only; never let the list keep focus

        case WM_LBUTTONUP
            dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then frmAutoComplete_Commit( pDoc )
            return true

    end select

    return false
end function


' ========================================================================================
' Decide where the popup goes.
'
' Split out of AutoC_Layout as PURE arithmetic - no window handles, no DC - so the
' self-test below can assert the three cases that are impossible to check by eye: the
' ordinary below-the-caret placement, the flip above when the popup would run off the
' bottom, and the clamp when it would run off the right.
' ========================================================================================
private sub AutoC_PlacePopup( _
            byval anchorX as long, _
            byval anchorY as long, _
            byval nLineHeight as long, _
            byval winW as long, _
            byval winH as long, _
            byref rcWork as RECT, _
            byref outX as long, _
            byref outY as long, _
            byref outFlipped as boolean _
            )

    ' Below the caret's own line by default, so the popup never covers what is being
    ' typed.
    outX = anchorX
    outY = anchorY + nLineHeight
    outFlipped = false

    if outY + winH > rcWork.bottom then
        dim as long yAbove = anchorY - winH
        ' Only flip if the popup genuinely fits above; otherwise pin it to the bottom
        ' edge rather than pushing it off the top of the screen.
        if yAbove >= rcWork.top then
            outY = yAbove
            outFlipped = true
        else
            outY = rcWork.bottom - winH
        end if
    end if

    if outX + winW > rcWork.right then outX = rcWork.right - winW
    if outX < rcWork.left then outX = rcWork.left
    if outY < rcWork.top then outY = rcWork.top

end sub


' ========================================================================================
' Measure the candidate set and size/place the popup against the caret.
'
' Geometry is DERIVED here and nowhere else. Widths are measured with the same font the
' painter uses; the visible row count is capped at AUTOCOMPLETE_VISIBLE_ROWS; and the
' popup flips above the caret line when it would otherwise leave the monitor work area.
' ========================================================================================
private function AutoC_Layout( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false
    if gAutoCVisCount = 0 then return false

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMAUTOCOMPLETE)
    if pWindow = 0 then return false

    dim as HWND hEdit = pDoc->hWndActiveScintilla
    if hEdit = 0 then return false

    dim as HDC hDC = GetDC( HWND_FRMAUTOCOMPLETE )
    if hDC = 0 then return false
    dim as HFONT hOldFont = SelectObject( hDC, ghFont(GUIFONT_10) )

    ' Only the rows that can actually be shown are measured -- a 1000-item project list
    ' would otherwise pay for 991 measurements nobody sees. A longer name further down
    ' simply ellipsizes, which is the same thing the width clamp does anyway.
    dim as long nMeasure = gAutoCVisCount
    if nMeasure > AUTOCOMPLETE_VISIBLE_ROWS then nMeasure = AUTOCOMPLETE_VISIBLE_ROWS

    dim as long maxNameW = 0, maxDetailW = 0
    for i as long = 0 to nMeasure - 1
        dim as long nItem = gAutoCVisible(i)
        dim as SIZE sz
        dim as long nChars = len( gAutoCItems(nItem).wszName )
        if nChars > 0 then
            if GetTextExtentPoint32W( hDC, gAutoCItems(nItem).wszName.vptr, nChars, @sz ) then
                if sz.cx > maxNameW then maxNameW = sz.cx
            end if
        end if
        nChars = len( gAutoCItems(nItem).wszDetail )
        if nChars > 0 then
            if GetTextExtentPoint32W( hDC, gAutoCItems(nItem).wszDetail.vptr, nChars, @sz ) then
                if sz.cx > maxDetailW then maxDetailW = sz.cx
            end if
        end if
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( HWND_FRMAUTOCOMPLETE, hDC )

    gAutoCDetailWidth = maxDetailW

    dim as long nRows = gAutoCVisCount
    if nRows > AUTOCOMPLETE_VISIBLE_ROWS then nRows = AUTOCOMPLETE_VISIBLE_ROWS

    ' The scrollbar is only budgeted for when the list actually overflows -- PsListTree
    ' auto-hides it, and reserving the strip unconditionally would leave a dead gutter on
    ' every short list.
    dim as long nScrollW = 0
    if gAutoCVisCount > AUTOCOMPLETE_VISIBLE_ROWS then
        nScrollW = pWindow->ScaleX( SCROLLBAR_WIDTH_PANEL )
    end if

    dim as long nGap = 0
    if maxDetailW > 0 then nGap = pWindow->ScaleX( AUTOCOMPLETE_COLUMN_GAP )

    dim as long contentW = pWindow->ScaleX( AUTOCOMPLETE_GLYPH_WIDTH ) + maxNameW + _
                           nGap + maxDetailW + pWindow->ScaleX( AUTOCOMPLETE_PAD_RIGHT ) + nScrollW
    dim as long nMinW = pWindow->ScaleX( AUTOCOMPLETE_MIN_WIDTH )
    dim as long nMaxW = pWindow->ScaleX( AUTOCOMPLETE_MAX_WIDTH )
    if contentW < nMinW then contentW = nMinW
    if contentW > nMaxW then contentW = nMaxW

    ' The ONE place the unscaled row height becomes pixels. The listbox is handed the
    ' unscaled figure below and scales it itself, so both must agree here or the window
    ' ends up a different height from the rows inside it.
    dim as long nRowPixels = pWindow->ScaleY( gAutoCRowHeight )

    dim as long nBorder = 1
    dim as long winW = contentW + (nBorder * 2)
    dim as long winH = (nRows * nRowPixels) + (nBorder * 2)

    ' Anchor on the START of the word being completed, not the caret: as the user types
    ' the popup then stays put instead of walking right one character at a time.
    dim as long nAnchorPos = pDoc->AutoCStartPos - len( pDoc->wszMatchWord )
    if nAnchorPos < 0 then nAnchorPos = 0

    dim as POINT pt
    pt.x = SciExec( hEdit, SCI_POINTXFROMPOSITION, 0, nAnchorPos )
    pt.y = SciExec( hEdit, SCI_POINTYFROMPOSITION, 0, nAnchorPos )
    ClientToScreen( hEdit, @pt )

    dim as long nAnchorLine = SciExec( hEdit, SCI_LINEFROMPOSITION, nAnchorPos, 0 )
    dim as long nLineHeight = SciExec( hEdit, SCI_TEXTHEIGHT, nAnchorLine, 0 )
    if nLineHeight <= 0 then nLineHeight = nRowPixels

    dim as long x = pt.x
    dim as long y = pt.y + nLineHeight
    dim as boolean bFlipped = false

    dim as RECT rcTarget = type( x, y, x + winW, y + winH )
    dim as MONITORINFO mi
    mi.cbSize = sizeof(MONITORINFO)
    dim as HMONITOR hMon = MonitorFromRect( @rcTarget, MONITOR_DEFAULTTONEAREST )
    if GetMonitorInfo( hMon, @mi ) then
        AutoC_PlacePopup( pt.x, pt.y, nLineHeight, winW, winH, mi.rcWork, x, y, bFlipped )
    end if

    if gAutoCTrace then
        dim as string sTrace = "AUTOC layout: rows=" & gAutoCVisCount & "/" & nRows & _
            " rowH=" & gAutoCRowHeight & "(unscaled)/" & nRowPixels & "px" & _
            "  nameW=" & maxNameW & " detailW=" & maxDetailW & " sbW=" & nScrollW & _
            "  win=" & winW & "x" & winH & _
            "  anchor=" & pt.x & "," & pt.y & " lineH=" & nLineHeight & _
            "  placed=" & x & "," & y & iif(bFlipped, " FLIPPED", "") & _
            "  work=" & mi.rcWork.left & "," & mi.rcWork.top & "," & _
                        mi.rcWork.right & "," & mi.rcWork.bottom
        print sTrace
    end if

    PsListTree_SetRowHeight( ghAutoCList, gAutoCRowHeight )
    SetWindowPos( HWND_FRMAUTOCOMPLETE, HWND_TOP, x, y, winW, winH, SWP_NOACTIVATE )
    SetWindowPos( ghAutoCList, 0, nBorder, nBorder, winW - (nBorder * 2), winH - (nBorder * 2), _
                  SWP_NOACTIVATE or SWP_NOZORDER )

    return true
end function


' ========================================================================================
' Push the currently filtered set into the listbox.
' ========================================================================================
private sub AutoC_PopulateListBox()
    PsListTree_BeginUpdate( ghAutoCList )
    PsListTree_Clear( ghAutoCList )
    for i as long = 0 to gAutoCVisCount - 1
        PsListTree_AddString( ghAutoCList, gAutoCItems( gAutoCVisible(i) ).wszName )
    next
    PsListTree_EndUpdate( ghAutoCList )
    if gAutoCVisCount > 0 then PsListTree_SetCurSel( ghAutoCList, 0 )
end sub


' ========================================================================================
' Rebuild gAutoCVisible() from the document's current match word.
'
' Returns FALSE when nothing matches, which is the caller's cue to cancel: a popup with
' no rows is worse than no popup.
' ========================================================================================
function frmAutoComplete_ReFilter( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false
    if gAutoCCount = 0 then return false

    ' wszMatchWord is already uppercase everywhere it is produced (see
    ' ShowAutocompleteList), and wszSort is the uppercased name, so this is a plain
    ' case-insensitive prefix test with no per-row ucase().
    dim as DWSTRING wszMatch = ucase( pDoc->wszMatchWord )
    dim as long nLen = len( wszMatch )

    redim gAutoCVisible( 0 to gAutoCCount - 1 ) as long
    gAutoCVisCount = 0
    for i as long = 0 to gAutoCCount - 1
        dim as boolean bMatch = true
        if nLen then bMatch = ( left( gAutoCItems(i).wszSort, nLen ) = wszMatch )
        if bMatch then
            gAutoCVisible( gAutoCVisCount ) = i
            gAutoCVisCount += 1
        end if
    next

    if gAutoCVisCount = 0 then return false

    AutoC_PopulateListBox()
    if AutoC_Layout( pDoc ) = false then return false
    AfxRedrawWindow( HWND_FRMAUTOCOMPLETE )

    return true
end function


' ========================================================================================
' Show the popup for a freshly built candidate set.
' ========================================================================================
function frmAutoComplete_Show( _
            byval pDoc as clsDocument ptr, _
            items() as AUTOC_ITEM, _
            byval nCount as long _
            ) as boolean

    if pDoc = 0 then return false
    if nCount <= 0 then
        frmAutoComplete_Cancel()
        return false
    end if
    if HWND_FRMAUTOCOMPLETE = 0 then return false

    ' Take our own copy: the caller's array is a local that dies with its function, and
    ' the popup outlives the call by design.
    redim gAutoCItems( 0 to nCount - 1 ) as AUTOC_ITEM
    for i as long = 0 to nCount - 1
        gAutoCItems(i) = items(i)
    next
    gAutoCCount = nCount

    ghAutoCEdit = pDoc->hWndActiveScintilla

    if frmAutoComplete_ReFilter( pDoc ) = false then
        frmAutoComplete_Cancel()
        return false
    end if

    ShowWindow( HWND_FRMAUTOCOMPLETE, SW_SHOWNOACTIVATE )
    gAutoCOpen = true

    ' The editor must hold focus for the pump filter to see the keys: an Output pane
    ' (Notes, Compiler Log) may have had it when the completion was triggered.
    SetFocus( ghAutoCEdit )

    return true
end function


' ========================================================================================
' Close the popup. Silent and idempotent -- safe to call from any cancellation path.
' ========================================================================================
sub frmAutoComplete_Cancel()
    if HWND_FRMAUTOCOMPLETE = 0 then exit sub
    if gAutoCOpen = false then exit sub
    gAutoCOpen  = false
    ghAutoCEdit = 0
    ShowWindow( HWND_FRMAUTOCOMPLETE, SW_HIDE )
end sub


function frmAutoComplete_IsOpen() as boolean
    return gAutoCOpen
end function


' ========================================================================================
' Insert the highlighted candidate.
'
' This is the old SCN_AUTOCSELECTION handler: the replacement range is the word the user
' has typed so far, which starts len(wszMatchWord) characters before the position the
' popup opened at -- NOT wherever the caret happens to be now.
' ========================================================================================
function frmAutoComplete_Commit( byval pDoc as clsDocument ptr ) as boolean
    if pDoc = 0 then return false
    if gAutoCOpen = false then return false

    dim as long nRow = PsListTree_GetCurSel( ghAutoCList )
    dim as boolean bValidRow = (nRow >= 0) andalso (nRow < gAutoCVisCount)
    if bValidRow = false then
        frmAutoComplete_Cancel()
        return false
    end if
    dim as DWSTRING wszText = gAutoCItems( gAutoCVisible(nRow) ).wszName

    dim as HWND hEdit = pDoc->hWndActiveScintilla

    ' Close BEFORE touching the document: the insertion fires SCN_MODIFIED, and a still-
    ' open popup would try to re-filter itself against text it is in the middle of
    ' changing.
    frmAutoComplete_Cancel()

    dim as long nStartPos     = pDoc->AutoCStartPos
    dim as long nPos          = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    dim as long nLenMatchWord = len( pDoc->wszMatchWord )

    if ucase(wszText) <> ucase(pDoc->wszMatchWord) then
        dim as string sText = wszText
        SciExec( hEdit, SCI_SETSEL, nStartPos - nLenMatchWord, nPos )
        SciExec( hEdit, SCI_REPLACESEL, 0, cast(LPARAM, strptr(sText)) )
        nPos = SciExec( hEdit, SCI_GETCURRENTPOS, 0, 0 )
    end if
    SciExec( hEdit, SCI_SETSEL, nPos, nPos )

    pDoc->AutoCompleteType = AUTOCOMPLETE_NONE

    ' If a codetip was showing before the popup covered it, put it back.
    ShowCodetip( pDoc )

    return true
end function


' ========================================================================================
' Message pump filter. See rule 1 in frmAutoComplete.bi: this consumes ONLY the
' navigation and commit keys. Every other message -- above all, ordinary typing -- must
' fall through to Scintilla, which is what keeps the incremental filtering alive.
' ========================================================================================
function frmAutoComplete_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if gAutoCOpen = false then return false
    if IsWindowVisible( HWND_FRMAUTOCOMPLETE ) = 0 then return false

    select case pMsg->message

    ' Outside click. Returns FALSE so the click still reaches whatever it landed on --
    ' the same convention as PsPopupMenu_FilterMessage.
    case WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_NCLBUTTONDOWN, WM_MBUTTONDOWN
        dim as boolean bInPopup = (pMsg->hwnd = HWND_FRMAUTOCOMPLETE) orelse _
                                  (IsChild( HWND_FRMAUTOCOMPLETE, pMsg->hwnd ) <> 0)
        if bInPopup = false then frmAutoComplete_Cancel()
        return false

    ' A wheel gesture over the editor scrolls the text out from under the popup, which
    ' would leave it anchored to nothing.
    case WM_MOUSEWHEEL, WM_MOUSEHWHEEL
        dim as boolean bInPopup = (pMsg->hwnd = HWND_FRMAUTOCOMPLETE) orelse _
                                  (IsChild( HWND_FRMAUTOCOMPLETE, pMsg->hwnd ) <> 0)
        if bInPopup = false then frmAutoComplete_Cancel()
        return false

    ' "(" accepts the highlighted row when that row is a callable that actually TAKES
    ' parameters, then lets the character through so Scintilla inserts it -- and the
    ' SCN_CHARADDED for "(" raises the calltip through the ordinary path. Nothing here
    ' shows the calltip itself; not consuming the character is what makes that work.
    '
    ' It must be caught on WM_CHAR, not in SCN_CHARADDED: by the time Scintilla notifies,
    ' the "(" is already IN the document, and frmAutoComplete_Commit's replacement range
    ' runs to the caret -- it would swallow the very character that triggered it.
    '
    ' This is the one survivor of Scintilla's three fillups. "=" and "." no longer accept
    ' anything, and "(" only does so when there is a parameter list to help fill in.
    case WM_CHAR
        if pMsg->wParam = asc("(") then
            dim as long nRow = PsListTree_GetCurSel( ghAutoCList )
            dim as boolean bValidRow = (nRow >= 0) andalso (nRow < gAutoCVisCount)
            dim as boolean bAccept = false
            if bValidRow then bAccept = gAutoCItems( gAutoCVisible(nRow) ).bHasParams

            if bAccept then
                dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
                if pDoc then
                    frmAutoComplete_Commit( pDoc )
                else
                    frmAutoComplete_Cancel()
                end if
            else
                ' A bare keyword, a TYPE, a variable, or a no-argument proc: "(" is just
                ' a character being typed and the completion ends.
                frmAutoComplete_Cancel()
            end if
        end if
        return false

    case WM_KEYDOWN
        dim as long nRow   = PsListTree_GetCurSel( ghAutoCList )
        dim as long nPage  = AUTOCOMPLETE_VISIBLE_ROWS - 1
        if nPage < 1 then nPage = 1

        select case pMsg->wParam

        case VK_DOWN
            if nRow < gAutoCVisCount - 1 then PsListTree_SetCurSel( ghAutoCList, nRow + 1 )
            return true

        case VK_UP
            if nRow > 0 then PsListTree_SetCurSel( ghAutoCList, nRow - 1 )
            return true

        case VK_NEXT
            dim as long n = nRow + nPage
            if n > gAutoCVisCount - 1 then n = gAutoCVisCount - 1
            PsListTree_SetCurSel( ghAutoCList, n )
            return true

        case VK_PRIOR
            dim as long n = nRow - nPage
            if n < 0 then n = 0
            PsListTree_SetCurSel( ghAutoCList, n )
            return true

        case VK_HOME
            PsListTree_SetCurSel( ghAutoCList, 0 )
            return true

        case VK_END
            PsListTree_SetCurSel( ghAutoCList, gAutoCVisCount - 1 )
            return true

        case VK_RETURN, VK_TAB
            ' The ONLY commit keys. Scintilla's fillups ("(", "=", ".") deliberately no
            ' longer accept an item -- they type through and the popup closes on the
            ' rebuild that finds nothing matching.
            dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                frmAutoComplete_Commit( pDoc )
            else
                frmAutoComplete_Cancel()
            end if
            return true

        case VK_ESCAPE
            frmAutoComplete_Cancel()
            return true

        case VK_LEFT, VK_RIGHT
            ' Deliberately NOT consumed: the caret still moves. But it is moving out of
            ' the word the popup is filtering on, and the popup is anchored to that
            ' word's start, so the completion ends here. (Up/Down/Home/End belong to the
            ' LIST while it is open - that is the standard completion contract - which is
            ' why only the horizontal pair does this.)
            frmAutoComplete_Cancel()
            return false

        end select

        ' Everything else falls through: this is the rule that separates this filter from
        ' its menu siblings.
        return false

    end select

    return false
end function


' ========================================================================================
' Row height, in UNSCALED units -- PsListTree_SetRowHeight DPI-scales it downstream, so do
' NOT pre-scale what you pass in. Takes effect on the next popup (AutoC_Layout is where
' the whole window is sized, and it runs per show/re-filter).
' ========================================================================================
sub frmAutoComplete_SetRowHeight( byval nHeight as long )
    if nHeight < 1 then exit sub
    gAutoCRowHeight = nHeight
end sub

function frmAutoComplete_GetRowHeight() as long
    return gAutoCRowHeight
end function


' ========================================================================================
' Re-apply the editor theme. Called whenever the app theme changes.
' ========================================================================================
sub frmAutoComplete_SyncTheme()
    if ghAutoCList = 0 then exit sub
    PsListTree_SetBackColor( ghAutoCList, ghEditor.BackColorText )
    PsListTree_SetScrollBarColors( ghAutoCList, ghEditor.BackColorScrollBar, _
                                 ghEditor.ForeColorScrollBar, ghEditor.ForeColorScrollBar )
    AfxRedrawWindow( HWND_FRMAUTOCOMPLETE )
end sub


' ========================================================================================
' frmAutoComplete_WndProc
' ========================================================================================
private function frmAutoComplete_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    ' The popup must never take activation, and with it focus, from the editor. The
    ' click is still delivered; Scintilla keeps its caret and its keyboard.
    case WM_MOUSEACTIVATE
        return MA_NOACTIVATE

    case WM_ERASEBKGND
        return true

    case WM_PAINT
        ' The PsListTree child covers everything but a one-pixel ring, so this paints only
        ' the border. Deliberately NOT DPI-scaled: a hairline should stay a hairline.
        dim ps as PAINTSTRUCT
        dim as HDC hDC = BeginPaint( hwnd, @ps )
        dim as RECT rc
        GetClientRect( hwnd, @rc )
        dim as HBRUSH hBrush = CreateSolidBrush( ghEditor.Divider )
        if hBrush then
            FrameRect( hDC, @rc, hBrush )
            DeleteObject( hBrush )
        end if
        EndPaint( hwnd, @ps )
        return 0

    end select

    return DefWindowProcW( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Geometry self-test, run with TIKO_AUTOC_SELFTEST=1. Drives the REAL placement
' function with synthetic anchors and work areas, so the cases that cannot be checked by
' eye - and would need a caret parked at an exact screen position to reproduce - are
' asserted as numbers instead.
' ========================================================================================
private sub AutoC_SelfTestCheck( byval sName as string, byval bOk as boolean, byval sDetail as string )
    dim as string s
    if bOk then s = "PASS  " else s = "FAIL  "
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private sub AutoC_RunSelfTest()
    print ""
    print "=== frmAutoComplete geometry self-test ==="

    ' A 1920x1040 work area with a 40px taskbar at the bottom of a 1080 screen.
    dim as RECT rcWork = type( 0, 0, 1920, 1040 )
    dim as long lineH = 18
    dim as long winW = 300
    dim as long winH = 200
    dim as long x, y
    dim as boolean bFlip

    ' 1. Ordinary case: room below, so the popup sits one line under the anchor and
    '    keeps the anchor's x.
    AutoC_PlacePopup( 400, 300, lineH, winW, winH, rcWork, x, y, bFlip )
    AutoC_SelfTestCheck( "below: x follows anchor", (x = 400), "x=" & x )
    AutoC_SelfTestCheck( "below: y = anchor + lineH", (y = 318), "y=" & y )
    AutoC_SelfTestCheck( "below: not flipped", (bFlip = false), "flip=" & bFlip )

    ' 2. Caret near the bottom: 900+18+200 = 1118 > 1040, and 900-200 = 700 >= 0, so the
    '    popup flips ABOVE and its BOTTOM lands exactly on the anchor line's top.
    AutoC_PlacePopup( 400, 900, lineH, winW, winH, rcWork, x, y, bFlip )
    AutoC_SelfTestCheck( "flip: flagged", (bFlip = true), "flip=" & bFlip )
    AutoC_SelfTestCheck( "flip: y = anchor - winH", (y = 700), "y=" & y )
    AutoC_SelfTestCheck( "flip: bottom meets anchor", (y + winH = 900), "bottom=" & (y + winH) )

    ' 3. No room either way (a very tall popup on a short work area): pin to the bottom
    '    edge rather than flipping off the top.
    AutoC_PlacePopup( 400, 100, lineH, winW, 1000, rcWork, x, y, bFlip )
    AutoC_SelfTestCheck( "no-room: not flipped", (bFlip = false), "flip=" & bFlip )
    AutoC_SelfTestCheck( "no-room: pinned to work bottom", (y + 1000 = 1040), "y=" & y )
    AutoC_SelfTestCheck( "no-room: never above work top", (y >= rcWork.top), "y=" & y )

    ' 4. Caret near the right edge: 1800 + 300 = 2100 > 1920, so x clamps so the popup's
    '    right edge meets the work area's.
    AutoC_PlacePopup( 1800, 300, lineH, winW, winH, rcWork, x, y, bFlip )
    AutoC_SelfTestCheck( "clamp: right edge meets work right", (x + winW = 1920), "x=" & x )

    ' 5. A popup wider than the work area still starts at the left edge rather than at a
    '    negative x (the clamp order matters: right-clamp first, then left).
    AutoC_PlacePopup( 100, 300, lineH, 2500, winH, rcWork, x, y, bFlip )
    AutoC_SelfTestCheck( "clamp: over-wide starts at work left", (x = 0), "x=" & x )

    ' 6. A secondary monitor to the LEFT of the primary has negative coordinates -- the
    '    clamps must be relative to the work area, not to zero.
    dim as RECT rcLeft = type( -1920, 0, 0, 1040 )
    AutoC_PlacePopup( -1900, 300, lineH, winW, winH, rcLeft, x, y, bFlip )
    AutoC_SelfTestCheck( "negative monitor: x unchanged", (x = -1900), "x=" & x )
    AutoC_PlacePopup( -100, 300, lineH, winW, winH, rcLeft, x, y, bFlip )
    AutoC_SelfTestCheck( "negative monitor: clamps to its right", (x + winW = 0), "x=" & x )

    ' --- CalltipHasParams: the test that decides whether "(" accepts a row ---------------
    ' (Parsed procs answer this structurally via gSymDb.ProcHasParams; the FB intrinsics
    '  only have pre-formatted calltip TEXT, so they go through this parse.)
    AutoC_SelfTestCheck( "params: intrinsic with one arg", _
        CalltipHasParams( "LEN(expression)" ) = true, "LEN(expression)" )
    AutoC_SelfTestCheck( "params: intrinsic with several", _
        CalltipHasParams( "MID(str, start [, length])" ) = true, "MID(...)" )
    AutoC_SelfTestCheck( "params: empty arg list", _
        CalltipHasParams( "Foo()" ) = false, "Foo()" )
    AutoC_SelfTestCheck( "params: empty list with return type", _
        CalltipHasParams( "Foo() as long" ) = false, "Foo() as long" )
    AutoC_SelfTestCheck( "params: byref arg with return type", _
        CalltipHasParams( "Foo(byref x as long) as string" ) = true, "Foo(byref x...)" )
    AutoC_SelfTestCheck( "params: whitespace-only list", _
        CalltipHasParams( "Foo(   )" ) = false, "Foo(   )" )
    AutoC_SelfTestCheck( "params: bare keyword, no parens", _
        CalltipHasParams( "DIM" ) = false, "DIM" )
    AutoC_SelfTestCheck( "params: no calltip at all", _
        CalltipHasParams( "" ) = false, "(empty)" )
    ' Unbalanced text must not report parameters off a stray ")" before the "(".
    AutoC_SelfTestCheck( "params: close before open", _
        CalltipHasParams( ") junk (" ) = false, ") junk (" )

    print "=== end frmAutoComplete self-test ==="
    print ""
end sub


' ========================================================================================
' Create the singleton popup. Called once at startup from frmMain_Show.
' ========================================================================================
sub frmAutoComplete_Init()

    dim pWindow as CWindow ptr = new CWindow

    ' WS_POPUP owned by the main window (above it in z-order, hidden with it on
    ' minimize). WS_EX_NOACTIVATE plus the MA_NOACTIVATE answer keep it from ever taking
    ' activation; WS_EX_TOOLWINDOW keeps it out of the taskbar and Alt-Tab.
    HWND_FRMAUTOCOMPLETE = pWindow->Create( HWND_FRMMAIN, "", @frmAutoComplete_WndProc, _
            0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE )

    ' CWindow registers a unique class per instance, so replacing the class style cannot
    ' leak to other windows. CS_DBLCLKS is deliberately absent: a rapid double-click on a
    ' row would otherwise deliver a dblclk instead of the second down, and the click path
    ' here commits on the FIRST up anyway.
    pWindow->ClassStyle = CS_DROPSHADOW

    ghAutoCList = PsListTree_Create( HWND_FRMAUTOCOMPLETE, IDC_FRMAUTOCOMPLETE_LISTBOX )
    PsListTree_SetPaintCallback( ghAutoCList, @frmAutoComplete_PaintCallback )
    PsListTree_SetMessageCallback( ghAutoCList, @frmAutoComplete_MessageCallback )
    PsListTree_SetTooltipCallback( ghAutoCList, @frmAutoComplete_TooltipCallback )
    PsListTree_PreventDoubleClick( ghAutoCList )
    PsListTree_SetFont( ghAutoCList, ghFont(GUIFONT_10) )
    PsListTree_SetScrollBarWidth( ghAutoCList, SCROLLBAR_WIDTH_PANEL )
    frmAutoComplete_SyncTheme()

    ' Show the CONTAINER. Its inner LISTBOX is created without WS_VISIBLE and shows
    ' itself on the first populate (verified: the style gains WS_VISIBLE during
    ' PsListTree_EndUpdate), which is why frmAutoComplete_Show populates before it shows
    ' the popup.
    ShowWindow( ghAutoCList, SW_SHOW )

    gAutoCTrace = (environ("TIKO_AUTOC_TRACE") = "1")
    if environ("TIKO_AUTOC_SELFTEST") = "1" then AutoC_RunSelfTest()

end sub
