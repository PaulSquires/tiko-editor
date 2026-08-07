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

#include once "frmPanelMenu.bi"


' This bar's tooltip. A PsTooltip rather than the comctl32 tip built by hand here before: it
' is themed, it wraps without the hand-sent TTM_SETMAXTIPWIDTH that used to follow the create,
' and it adds no pump obligation. One tip for the window; the hot rect is carried by the TOOL
' INDEX, set from WM_MOUSEMOVE, so the hit-test runs once on the message rather than a second
' time against a cursor that has since moved.
dim shared ghFindTip as HWND

' The bar's seven icon buttons are three PsIconPanels, split where the strip itself is split:
'   FIELD - Match Case, Whole Word. These sit INSIDE the rounded find frame.
'   NAV   - Toggle Replace, Selection, a separator, Prev, Next.
'   CLOSE - the X, alone at the far right because the results text ("0/0") sits between it
'           and NAV, and a PsIconPanel's items are contiguous by construction.
' Three windows rather than one is what the layout is: the control lays its cells out in
' supplied order and never inserts a gap.
'
' THE VERTICAL DIVIDER IS THE NAV PANEL'S IP_KIND_SEPARATOR, not a hand-drawn line and not a
' gFind rect -- gFind.rcDividerLine is gone. A separator is not hit-testable, so it also
' cannot be reported to the tooltip by accident.
' Collapse/Expand All, shown only on the Find in Project tab -- a one-item panel of its own
' because a PsIconPanel's item set is STATIC by contract, so it cannot be added to the field
' panel and then removed again for ordinary find.
dim shared ghFindIconsFold  as HWND
' Whether the find field is currently painted in the "found nothing" colour. Cached so the
' layout pass, which runs often, does not re-push and re-invalidate on every call.
dim shared gFindTextIsError as boolean = false
dim shared ghFindIconsField as HWND
dim shared ghFindIconsNav   as HWND
dim shared ghFindIconsClose as HWND

' These are BOTH the PsTooltip tool index AND each item's PsIconPanel id. Item INDEX cannot
' serve here the way it does on the info bar: the items are spread over three panels and the
' nav panel has a separator in the middle of its own indices.
enum
    FINDTIP_NONE = -1
    FINDTIP_TOGGLE = 0
    FINDTIP_MATCHCASE
    FINDTIP_WHOLEWORD
    FINDTIP_UPARROW
    FINDTIP_DOWNARROW
    FINDTIP_SELECTION
    FINDTIP_CLOSE
    FINDTIP_FOLDALL
end enum


' ========================================================================================
' Supply the tooltip text for the currently hot rect.
' ========================================================================================
' Runs once per hover, immediately before the tip is sized, which is why the theme push lives
' here (see modThemeApply.bi). Returning "" means no tip for the rest of this hover.
private function frmFind_TipText( byval hTip as HWND, _
                                         byval hAttached as HWND, _
                                         byval idx as long ) as DWSTRING
    Theme_ApplyTooltip( hTip )
    select case idx
        case FINDTIP_TOGGLE:     return frmPanelMenu_FormatTooltip( IDM_TOGGLEREPLACE )
        case FINDTIP_MATCHCASE:  return L(137,"Match Case")
        case FINDTIP_WHOLEWORD:  return L(136,"Match Whole Words")
        case FINDTIP_UPARROW:    return frmPanelMenu_FormatTooltip( IDM_FINDPREVACCEL )
        case FINDTIP_DOWNARROW:  return frmPanelMenu_FormatTooltip( IDM_FINDNEXTACCEL )
        case FINDTIP_SELECTION:  return L(130,"Selection")
        case FINDTIP_CLOSE:      return L(135,"Close") & " (Esc)"
        case FINDTIP_FOLDALL
            ' The tip names what the click WILL do, which is the opposite of the state.
            if frmFindInProject_AllCollapsed() then return L(392,"Expand All Search Results")
            return L(391,"Collapse All Search Results")
    end select
    return ""
end function


' ========================================================================================
' Is this button showing its "on" state? Three of the seven latch.
' ========================================================================================
' THE ITEMS ARE IP_KIND_COMMAND, NOT IP_KIND_TOGGLE, AND THAT IS DELIBERATE. A latching
' toggle would keep its own isSelected, which would be a SECOND copy of state the search
' engine already owns and reads (gFind.nMatchCase / nWholeWord / nSelection in
' modFindReplace.inc) -- and one that FindControls_Show writes directly when it turns
' Selection on for a multi-line selection, without going anywhere near this control. Asking
' the model at paint time cannot go stale; a latch synced from four call sites can.
' ========================================================================================
' A project search that matched NOTHING turns the phrase red.
'
' Only on the Find in Project tab: the in-document find reports "0/0" in the results area
' beside the bar, which the project search has no equivalent of once the list is empty --
' an empty results surface looks the same whether you have searched or not.
' ========================================================================================
sub frmFind_ApplyErrorState( byval bForce as boolean = false )
    dim as HWND hCtl = GetDlgItem( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    if IsWindow( hCtl ) = 0 then exit sub

    ' Hoisted to booleans: mixing these into one andalso chain is warning 38.
    dim as boolean bOnTab     = frmFindInProject_IsActive()
    dim as boolean bSearched  = gFip.bValid
    dim as boolean bHadPhrase = cbool( PsLen(gFip.wszPhrase) > 0 )
    dim as boolean bNoHits    = cbool( gFip.nLiveMatches = 0 )
    ' AND THE FIELD STILL HOLDS THE PHRASE THAT FAILED. The red reports the outcome of a
    ' SEARCH, so the moment the user edits the text it is describing something that has not
    ' been searched for -- and saying "no results" about a phrase nobody has looked for yet
    ' is just wrong. Editing clears it; the next Enter decides again.
    dim as boolean bSameText  = cbool( WindowText( hCtl ) = gFip.wszPhrase )

    dim as boolean bErr = false
    if bOnTab andalso bSearched andalso bHadPhrase andalso bNoHits andalso bSameText then
        bErr = true
    end if

    if (bErr = gFindTextIsError) andalso (bForce = false) then exit sub
    gFindTextIsError = bErr

    PsTextBox_SetForeColor( hCtl, iif( bErr, theme.compile.iconfail, theme.toptabs.texthot.forecolor ) )
    AfxRedrawWindow( hCtl )
end sub


private function frmFind_IsLatched( byval id as long ) as boolean
    select case id
        case FINDTIP_MATCHCASE:  return cbool( gFind.nMatchCase <> 0 )
        case FINDTIP_WHOLEWORD:  return cbool( gFind.nWholeWord <> 0 )
        case FINDTIP_SELECTION:  return cbool( gFind.nSelection <> 0 )
        ' Toggle Replace stays lit for as long as the Replace bar is open, so the button
        ' reports a mode rather than blinking once per press. Same reasoning as the three
        ' above: the model already owns the state, so asking it cannot go stale.
        case FINDTIP_TOGGLE:     return gFind.bShowReplacePanel
    end select
    return false
end function


' ========================================================================================
' Draw one icon button, instead of PsIconPanel's built-in painter.
' ========================================================================================
' Shared by all three panels. Three things the built-in painter cannot do here:
'   * The latched look comes from gFind, not from the item (see frmFind_IsLatched), and it
'     is IconSelectedBackColor -- a theme key the control knows nothing about.
'   * The bar's hover INVERTS: the strip's field colour is BackColorHot, so a hovered
'     unselected button goes to the tab area's normal BackColor.
'   * The separator is inset vertically by 4px, the way the hand-drawn divider was.
' The glyph font is read back from the panel, which is what keeps this one callback correct
' for the field panel (GUIFONT_9) and the nav/close panels (SYMBOLFONT_9) at the same time.
private sub frmFind_IconPaint( byval p as PSICONPANEL_PAINTINFO ptr )
    ' Fill the whole cell first: p->rc carries the item's padding, and anything left
    ' unpainted shows the panel background through.
    p->b->SetBackColor( theme.toptabs.texthot.backcolor )
    p->b->PaintRect( @p->rc )

    if p->itemKind = IP_KIND_SEPARATOR then
        dim pWindow as CWindow ptr = AfxCWindowPtr( p->hIconPanel )
        dim as long nInset = iif( pWindow, pWindow->ScaleY(4), 4 )
        dim as long x = (p->rc.left + p->rc.right) \ 2
        p->b->SetPenColor( theme.editor.divider )
        p->b->PaintLine( 1, x, p->rc.top + nInset, x, p->rc.bottom - nInset )
        exit sub
    end if

    dim as long id = PsIconPanel_GetItemID( p->hIconPanel, p->itemID )
    if frmFind_IsLatched( id ) then
        p->b->SetBackColor( theme.toptabs.iconselected )
    else
        p->b->SetBackColor( iif( p->isHot orelse p->isPressed, _
                                 theme.toptabs.text.backcolor, theme.toptabs.texthot.backcolor ) )
    end if
    p->b->SetForeColor( theme.toptabs.texthot.forecolor )
    p->b->SetFont( PsIconPanel_GetFont( p->hIconPanel ) )
    p->b->PaintIconButton( p->wszGlyph, @p->rc )
end sub


' ========================================================================================
' Carry the hot item into the bar's PsTooltip.
' ========================================================================================
' One tip serves the whole strip -- it is attached to the PARENT, and PsTooltip's
' cursor-over test accepts a hit on any descendant. But a panel's client area is the
' parent's dead zone, so all three panels report from here. The tool index is the item's
' ID (see the enum); a separator is never hit-tested, so m->idx = -1 is the only "none".
' Returns FALSE always: this observes, it never suppresses.
private function frmFind_IconMessage( byval m as PSICONPANEL_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
        case WM_MOUSEMOVE
            dim as long idTip = FINDTIP_NONE
            if m->idx >= 0 then idTip = PsIconPanel_GetItemID( m->hIconPanel, m->idx )
            PsTooltip_SetToolIndex( ghFindTip, idTip )
        case WM_MOUSELEAVE
            PsTooltip_SetToolIndex( ghFindTip, FINDTIP_NONE )
    end select
    return false
end function


' ========================================================================================
' A matched press+release on one of the icon buttons.
' ========================================================================================
' Fires only for a completed gesture -- press a button, slide off, release and nothing
' happens. The old WM_LBUTTONUP arm acted on wherever the cursor was at the UP, whatever
' the DOWN had hit. The bodies are the old arms verbatim.
private sub frmFind_IconClick( byval hIconPanel as HWND, byval idx as long, byval id as long )
    select case id
        case FINDTIP_TOGGLE
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_REPLACE, 0), 0 )

        ' BOTH OF THESE NEED THE PROJECT BRANCH, and it is not an extra: on the Find in
        ' Project tab FindReplace_HighlightSearches does NOTHING AT ALL. Its first act is
        ' GetActiveDocumentPtr, which that tab answers null to by design, so it returns
        ' before touching anything -- and the results list went on showing the matches for
        ' the OLD flags with the icon lit for the new ones. The project search is a whole
        ' re-run, not a re-highlight, so it cannot share that call.
        case FINDTIP_MATCHCASE
            gFind.nMatchCase = not gFind.nMatchCase
            if frmFindInProject_IsActive() then
                frmFindInProject_RerunSearch()
            else
                FindReplace_HighlightSearches()
            end if
            AfxRedrawWindow( hIconPanel )

        case FINDTIP_WHOLEWORD
            gFind.nWholeWord = not gFind.nWholeWord
            if frmFindInProject_IsActive() then
                frmFindInProject_RerunSearch()
            else
                FindReplace_HighlightSearches()
            end if
            AfxRedrawWindow( hIconPanel )

        case FINDTIP_FOLDALL
            ' Fold or unfold every file in the results. The glyph is re-pointed by
            ' frmFind_PositionWindows, which the refresh below ends up calling.
            ' SetAllCollapsed refreshes the surface; the glyph is re-pointed on the next
            ' layout pass, which frmMain_PositionWindows drives.
            frmFindInProject_SetAllCollapsed( not frmFindInProject_AllCollapsed() )
            frmMain_PositionWindows()
            AfxRedrawWindow( hIconPanel )

        case FINDTIP_UPARROW
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDPREV, 0), 0 )

        case FINDTIP_DOWNARROW
            PostMessage( HWND_FRMMAIN, WM_COMMAND, MAKEWPARAM(IDM_FINDNEXT, 0), 0 )

        case FINDTIP_SELECTION
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc = 0 then exit sub
            dim as boolean isOriginalSelectionMultiline = _
                (pDoc->CurrentSelection.endline - pDoc->CurrentSelection.startline)
            if pDoc->CurrentSelection.isInitialized = false then isOriginalSelectionMultiline = false
            if (isOriginalSelectionMultiline) orelse (pDoc->HasMarkerHighlight) then
                gFind.nSelection = not gFind.nSelection
            else
                gFind.nSelection = false
            end if
            if gFind.nSelection then
                pDoc->SetMarkerHighlight
            else
                pDoc->RemoveMarkerHighlight
            end if
            FindReplace_HighlightSearches()
            AfxRedrawWindow( hIconPanel )

        case FINDTIP_CLOSE
            FindReplaceControls_Hide()
    end select
end sub


' ========================================================================================
' Push the cell metrics into one panel. Both values are ALREADY DPI-SCALED.
' ========================================================================================
' Cells are square and as tall as the find field, and both of those depend on the bar's
' current size -- so this runs from PositionWindows on every resize, not once at create.
' Padding is zeroed: the icons butt up against each other exactly as the old rects did.
private sub frmFind_SetIconMetrics( byval hPanel as HWND, byval nIconW as long, byval nIconH as long )
    if IsWindow(hPanel) = 0 then exit sub
    PsIconPanel_SetIconSize( hPanel, nIconW, nIconH )
    PsIconPanel_SetPadding( hPanel, 0, 0 )
end sub


' ========================================================================================
' Position all child windows. Called manually and/or by WM_SIZE
' ========================================================================================
function frmFind_PositionWindows() as LRESULT
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_FIND)
    if pWindow = 0 then exit function

    ' Get the entire client area
    dim as Rect rc: GetClientRect( HWND_FRMMAIN_FIND, @rc )

    dim as long iconWidth
    dim as long iconHeight 
    dim as long nPadding = 10   ' 5 left 5 right
    dim as long vmargin = pWindow->ScaleY(4)
    dim as long nLeft
    dim as long nTop
    dim as long nWidth 
    dim as long nHeight
    dim as boolean isProjectSearch = frmFindInProject_IsActive()

    dim as HWND hCtlFind = GetDlgItem(HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND)

    ' FIND CONTROLS
    gTTabCtl.rcFindTextRect = rc
    gTTabCtl.rcFindTextRect.left      = pWindow->ScaleY(10)
    gTTabCtl.rcFindTextRect.top       = 0
    gTTabCtl.rcFindTextRect.right     = rc.right - pWindow->ScaleX(300)
    gTTabCtl.rcFindTextRect.bottom    = pWindow->ScaleY(TOPTABS_FIND_HEIGHT) - pWindow->ScaleY(8)
     
    rc      = gTTabCtl.rcFindTextRect
    nLeft   = rc.left + vmargin + pWindow->ScaleX(10)
    nTop    = rc.top + vmargin
    nWidth  = (rc.right - rc.left - vmargin*2 - pWindow->ScaleX(10))
    nHeight = (rc.bottom - rc.top - vmargin*2)

    ' Make the width of the textbox smaller because we want to visually fit the
    ' Match Case and Whole Word icons into the Find rect also. nHeight
    ' will be our icon height and width as well.
    iconWidth = nHeight
    iconHeight = nHeight
    nWidth = nWidth - (iconWidth * 2) - pWindow->ScaleX(4)
    
    ' DiskFilename text. Vertical centering re-derives inside the control on WM_SIZE.
    SetWindowPos( hCtlFind, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

    ' The icon cells are square and as tall as the field, and the panels declare them --
    ' which is why the widths below are asked of the control rather than multiplied out
    ' here. Every setter takes RAW PIXELS (the family rule: only the Create-time defaults
    ' are scaled for us), so iconWidth/nHeight go in already scaled.
    frmFind_SetIconMetrics( ghFindIconsFold,  iconWidth, nHeight )
    frmFind_SetIconMetrics( ghFindIconsField, iconWidth, nHeight )
    frmFind_SetIconMetrics( ghFindIconsNav,   iconWidth, nHeight )
    frmFind_SetIconMetrics( ghFindIconsClose, iconWidth, nHeight )

    ' FOLD panel: Collapse/Expand All. It sits entirely OUTSIDE the rounded find frame, to
    ' its left, and is LINED UP WITH THE GROUP-HEADER TWISTIES on the results surface below
    ' -- it does the same job for every group at once, so the two want to be in one column.
    ' That column is the surface's own left inset plus the header's internal padding; see
    ' FIP_INDENT and the nPad in FipSurface_PaintHeader.
    '
    ' Only the Find in Project tab has anything to fold, so on an ordinary find it is hidden
    ' and the find frame keeps the whole width it has always had.
    if isProjectSearch then
        dim as long nFoldW = PsIconPanel_GetIdealWidth( ghFindIconsFold )
        ' Centre the PANEL on the twisty column, so its glyph -- which the control centres in
        ' a cell of its own width -- lands on the same X as the header chevrons below.
        dim as long xFold  = frmFindInProject_TwistyColumnX() - (nFoldW \ 2)
        if xFold < 0 then xFold = 0
        SetWindowPos( ghFindIconsFold, 0, xFold, nTop, nFoldW, nHeight, _
                      SWP_NOZORDER or SWP_SHOWWINDOW )
        ' The glyph names the STATE's opposite: what the next click will do.
        PsIconPanel_SetGlyph( ghFindIconsFold, 0, _
                iif( frmFindInProject_AllCollapsed(), wszIconChevronDown, wszIconChevronUp ) )

        ' Push the whole find frame right so the icon is beside it, never inside it.
        dim as long nShift = (xFold + nFoldW + pWindow->ScaleX(8)) - gTTabCtl.rcFindTextRect.left
        if nShift > 0 then
            gTTabCtl.rcFindTextRect.left += nShift
            nLeft  += nShift
            nWidth -= nShift
        end if
    else
        SetWindowPos( ghFindIconsFold, 0, 0, 0, 0, 0, _
                      SWP_NOZORDER or SWP_NOMOVE or SWP_NOSIZE or SWP_HIDEWINDOW )
    end if

    ' Re-place the textbox now that the fold icon may have moved the frame.
    SetWindowPos( hCtlFind, 0, nLeft, nTop, nWidth, nHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

    ' The separator carries the whole gap the old rcDividerLine did: half a cell of air
    ' either side of a hairline rule, so the pair of chevrons stays exactly one icon width
    ' clear of the Selection button.
    dim as long nSepPadL = (iconWidth - 1) \ 2
    PsIconPanel_SetSeparatorWidth( ghFindIconsNav, 1 )
    PsIconPanel_SetItemPadding( ghFindIconsNav, 2, nSepPadL, iconWidth - 1 - nSepPadL )

    ' FIELD panel: Match Case + Whole Word, inside the rounded find frame, hard against
    ' the textbox's right edge.
    SetWindowPos( ghFindIconsField, 0, _
                  nLeft + nWidth, nTop, PsIconPanel_GetIdealWidth( ghFindIconsField ), nHeight, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' NAV panel: Toggle Replace, Selection, the divider, Prev, Next -- outside the frame.
    dim as long xNav = rc.right + pWindow->ScaleX(8)
    dim as long nNavW = PsIconPanel_GetIdealWidth( ghFindIconsNav )
    SetWindowPos( ghFindIconsNav, 0, _
                  xNav, nTop, nNavW, nHeight, _
                  SWP_NOZORDER or SWP_SHOWWINDOW )

    ' NOTE on isProjectSearch: it hides the Close button, which SWP_HIDEWINDOW below still
    ' does. It also used to collapse the Selection cell to zero width, and that is NOT
    ' reproduced -- a PsIconPanel's cells are DECLARED and its item set is static, so there
    ' is no zero-width cell and no DeleteItem to reach for. The flag is hardwired false
    ' today (the code that set it is commented out above), so nothing changes; if Project
    ' Search comes back, Selection wants PsIconPanel_SetEnabled, not a collapsed cell.

    ' CLOSE panel. Sits 4px lower than the rest, as the hand-drawn X always has.
    ' Project Search hides it; rcResults still needs to know where it would have been.
    GetClientRect( HWND_FRMMAIN_FIND, @rc )
    dim as long xCloseRight = rc.right - (pWindow->ScaleX(SCROLLBAR_WIDTH_EDITOR) \ 2)
    dim as long nCloseW = PsIconPanel_GetIdealWidth( ghFindIconsClose )
    dim as long yClose = nTop + pWindow->ScaleY(4)
    SetWindowPos( ghFindIconsClose, 0, _
                  xCloseRight - nCloseW, yClose, nCloseW, nHeight - pWindow->ScaleY(4), _
                  SWP_NOZORDER or iif( isProjectSearch, SWP_HIDEWINDOW, SWP_SHOWWINDOW ) )

    ' Results from search eg. "1 of 10"
    gFind.rcResults.left       = xNav + nNavW + pWindow->ScaleX(4)
    gFind.rcResults.top        = nTop
    gFind.rcResults.right      = xCloseRight - nCloseW
    gFind.rcResults.bottom     = nTop + nHeight

    ' Also covers LEAVING the tab: this runs on every tab switch, so a field left red by a
    ' fruitless project search goes back to normal for an ordinary Ctrl+F.
    frmFind_ApplyErrorState()

    AfxRedrawWindow( HWND_FRMMAIN_FIND )

    function = 0
end function


' ========================================================================================
' PsTextBox callbacks for the Find textbox. The control owns the cue banner, colors,
' vertical centering, single-line enforcement and the Cut/Copy/Paste context menu that
' the old RichEdit subclass hand-rolled here. ENTER/TAB handling stays in
' handleKeysFindReplace (modMsgPump), which intercepts those keys before dispatch.
' ========================================================================================
sub frmFind_TextBoxChange( byval hTextBox as HWND )
    ' User edits only (was the EN_CHANGE path): re-run the incremental search.
    gFind.txtFind = PsTextBox_GetText( hTextBox )

    ' Editing takes the "found nothing" red off immediately: the phrase in the field is no
    ' longer the one that failed. It comes back, or does not, on the next Enter.
    frmFind_ApplyErrorState()
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        FindReplace_HighlightSearches()
        dim as hwnd hEdit = pDoc->hWndActiveScintilla
        dim as long startPos = SendMessage( hEdit, SCI_GETANCHOR, 0, 0) - 1
        FindReplace_NextSelection(startPos, true, true)
    end if
    SetFocus( hTextBox )   ' the search may have moved focus to Scintilla; take it back
end sub

sub frmFind_TextBoxFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    ' Was the EN_SETFOCUS path: repaint the panel chrome around the textbox.
    if bGotFocus then AfxRedrawWindow( HWND_FRMMAIN_FIND )
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmFind
' ========================================================================================
function frmFind_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT

    if state <> SIZE_MINIMIZED then
        frmFind_PositionWindows()
    end If

    function = 0
end function


' ========================================================================================
' frmFind Window procedure
' ========================================================================================
function frmFind_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select Case uMsg
        HANDLE_MSG (hwnd, WM_SIZE, frmFind_OnSize)

    case WM_DESTROY
        ' The tip is a WS_POPUP owned by this window, not a child, so it does not go with the
        ' parent -- destroying it here is also what removes it from the shared clock's registry.
        if IsWindow(ghFindTip) then DestroyWindow( ghFindTip )
        ghFindTip = 0

    case WM_NCDESTROY
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        HWND_FRMMAIN_FIND = 0
        
    case WM_ERASEBKGND
        return true

    case WM_MOUSEMOVE
        dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN_FIND)
        if pWindow = 0 then exit function

        ' TME_LEAVE only. TME_HOVER existed solely to drive the old WM_MOUSEHOVER tooltip arm;
        ' PsTooltip does its own dwell timing off the shared clock.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)

        ' Nothing this window still paints itself has a tip -- the field frame and the
        ' results text never had one -- so being over the PARENT means no tip at all. The
        ' seven that do have tips are the panels', reported from frmFind_IconMessage.
        PsTooltip_SetToolIndex( ghFindTip, FINDTIP_NONE )

        AfxRedrawWindow( hwnd )


    case WM_MOUSELEAVE
        PsTooltip_SetToolIndex( ghFindTip, FINDTIP_NONE )
        AfxRedrawWindow( hwnd )


    ' No WM_LBUTTONUP arm: every button on this bar belongs to a PsIconPanel now and
    ' reports through frmFind_IconClick, on a matched press+release.

    case WM_PAINT
        dim as PsBufferPaint b
        
        b.BeginDoubleBuffer(hwnd)

        ' Set the default colors. The strip's field color is BackColorHot, so a
        ' hovered (unselected) button inverts to the tab area's normal BackColor.
        b.SetColors( theme.toptabs.texthot.forecolor, theme.toptabs.texthot.backcolor )

        ' Clear the entire back client area
        b.PaintClientRect()

        ' Only paint the bottom line if Replace is not active.
        b.SetPenColor( theme.editor.divider )
        if gFind.bShowReplacePanel = false then
            b.PaintLine( 1, b.rcClient.left, b.rcClient.bottom, b.rcClient.right, b.rcClient.bottom )
        end if

        ' Draw the Find controls.
        b.SetPenColor( theme.editor.divider )

        b.PaintRoundBorderRect( @gTTabCtl.rcFindTextRect )

        ' The seven icon buttons and the vertical divider are the three PsIconPanels' own
        ' client areas (frmFind_IconPaint). All that is left to this window is the frame
        ' around the find field and the results count beside the chevrons.
        b.SetFont( ghFont(GUIFONT_9) )
        b.SetBackColor( theme.toptabs.texthot.backcolor )
        b.PaintText( gFind.wszResults, @gFind.rcResults, DT_LEFT )

        b.EndDoubleBuffer()
        
        return 0
    
    end select
    
    ' for messages that we don't deal with
    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmFind_Show
' ========================================================================================
function frmFind_Show( byval hwndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CPanelWindow ptr = new CPanelWindow

    HWND_FRMMAIN_FIND = pWindow->Create( hwndParent, @frmFind_WndProc, _
        0, 0, 0, TOPTABS_FIND_HEIGHT, _
        WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_CONTROLPARENT or WS_EX_LEFT or WS_EX_LTRREADING or WS_EX_RIGHTSCROLLBAR)

    ' Disable background erasing by only assigning the one style
    pWindow->ClassStyle = CS_DBLCLKS

    ' The Find textbox is the reusable PsTextBox control (a RichEdit50W child inside --
    ' regular Edit controls can not easily have their text vertically centered).
    dim as HWND hCtlFind = PsTextBox_Create( HWND_FRMMAIN_FIND, IDC_FRMFIND_TXTFIND )
    PsTextBox_SetFont( hCtlFind, ghFont(INFOFONT_11) )
    PsTextBox_SetForeColor( hCtlFind, theme.toptabs.texthot.forecolor )
    PsTextBox_SetBackColor( hCtlFind, theme.toptabs.texthot.backcolor )
    PsTextBox_SetBorderWidth( hCtlFind, 0 )                       ' flat, as before
    PsTextBox_SetCueBannerText( hCtlFind, L(134,"Find") & "..." )
    PsTextBox_SetCueBannerColor( hCtlFind, theme.toptabs.text.forecolor )
    PsTextBox_SetChangeCallback( hCtlFind, @frmFind_TextBoxChange )
    PsTextBox_SetFocusCallback( hCtlFind, @frmFind_TextBoxFocus )
    ThemeTextBoxContextMenu( hCtlFind )

    ' The three icon strips. IP_KIND_COMMAND throughout -- see frmFind_IsLatched for why
    ' the three latching buttons are not IP_KIND_TOGGLE. Item ids are the FINDTIP_* values,
    ' which is what lets one click callback and one message callback serve all three panels.
    ' Cell metrics are pushed per resize by frmFind_SetIconMetrics, not here.
    '
    ' No tooltip TEXT and no tooltip CALLBACK on any of them: the tips are the bar's
    ' PsTooltip, so each control's own comctl32 tip is left with nothing to say.
    ghFindIconsFold = PsIconPanel_Create( HWND_FRMMAIN_FIND, IDC_FRMFIND_ICONSFOLD )
        PsIconPanel_SetFont( ghFindIconsFold, ghFont(SYMBOLFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghFindIconsFold, @frmFind_IconPaint )
        PsIconPanel_SetMessageCallback( ghFindIconsFold, @frmFind_IconMessage )
        PsIconPanel_SetClickCallback( ghFindIconsFold, @frmFind_IconClick )
        PsIconPanel_AddItem( ghFindIconsFold, IP_KIND_COMMAND, wszIconChevronUp, FINDTIP_FOLDALL )

    ghFindIconsField = PsIconPanel_Create( HWND_FRMMAIN_FIND, IDC_FRMFIND_ICONSFIELD )
        PsIconPanel_SetFont( ghFindIconsField, ghFont(GUIFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghFindIconsField, @frmFind_IconPaint )
        PsIconPanel_SetMessageCallback( ghFindIconsField, @frmFind_IconMessage )
        PsIconPanel_SetClickCallback( ghFindIconsField, @frmFind_IconClick )
        PsIconPanel_AddItem( ghFindIconsField, IP_KIND_COMMAND, wszIconMatchCase, FINDTIP_MATCHCASE )
        PsIconPanel_AddItem( ghFindIconsField, IP_KIND_COMMAND, wszIconWholeWord, FINDTIP_WHOLEWORD )

    ghFindIconsNav = PsIconPanel_Create( HWND_FRMMAIN_FIND, IDC_FRMFIND_ICONSNAV )
        PsIconPanel_SetFont( ghFindIconsNav, ghFont(SYMBOLFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghFindIconsNav, @frmFind_IconPaint )
        PsIconPanel_SetMessageCallback( ghFindIconsNav, @frmFind_IconMessage )
        PsIconPanel_SetClickCallback( ghFindIconsNav, @frmFind_IconClick )
        PsIconPanel_AddItem( ghFindIconsNav, IP_KIND_COMMAND, wszIconToggleReplace, FINDTIP_TOGGLE )
        PsIconPanel_AddItem( ghFindIconsNav, IP_KIND_COMMAND, wszIconSelection, FINDTIP_SELECTION )
        PsIconPanel_AddSeparator( ghFindIconsNav )     ' index 2 -- the vertical divider
        PsIconPanel_AddItem( ghFindIconsNav, IP_KIND_COMMAND, wszIconChevronLeft, FINDTIP_UPARROW )
        PsIconPanel_AddItem( ghFindIconsNav, IP_KIND_COMMAND, wszIconChevronRight, FINDTIP_DOWNARROW )

    ghFindIconsClose = PsIconPanel_Create( HWND_FRMMAIN_FIND, IDC_FRMFIND_ICONSCLOSE )
        PsIconPanel_SetFont( ghFindIconsClose, ghFont(SYMBOLFONT_9) )
        PsIconPanel_SetPaintItemCallback( ghFindIconsClose, @frmFind_IconPaint )
        PsIconPanel_SetMessageCallback( ghFindIconsClose, @frmFind_IconMessage )
        PsIconPanel_SetClickCallback( ghFindIconsClose, @frmFind_IconClick )
        PsIconPanel_AddItem( ghFindIconsClose, IP_KIND_COMMAND, wszIconClose, FINDTIP_CLOSE )

    Theme_ApplyFindIcons()

    ' Create ALSO attaches. 200px was the old TTM_SETMAXTIPWIDTH, kept so a long tip still
    ' wraps at the same place; colours and fonts are pushed per hover from
    ' frmFind_TipText, so nothing set here can go stale on a theme change.
    ghFindTip = PsTooltip_Create( HWND_FRMMAIN_FIND )
    PsTooltip_SetToolIndex( ghFindTip, FINDTIP_NONE )
    PsTooltip_SetMaxWidth( ghFindTip, 200 )
    PsTooltip_SetTooltipCallback( ghFindTip, @frmFind_TipText )

    function = 0
    
end function

