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

'  frmOptions.inc  --  the Options dialog shell.
'
'  SHAPE
'      +---------------------+----------------------------------------+
'      |  PsListTree (nav)     |  page title band  (fixed, not scrolled) |
'      |  grouped, fixed     +----------------------------------------+
'      |  width, NO splitter |  PsScrollPanel                          |
'      |                     |    GetPanel() is the parent of every    |
'      |                     |    page's controls                      |
'      +---------------------+----------------------------------------+
'      |                                 [ PsButton OK ] [ PsButton .. ] |
'      +--------------------------------------------------------------+
'
'  THIS DIALOG RUNS ITS OWN MESSAGE LOOP and cannot use CWindow.DoEvents, because that
'  loop has no *_FilterMessage hooks and this dialog hosts controls that require them.
'  The loop lives at the bottom of frmOptions_Show.
'
'  EVERY PAGE IS LAZY NOW
'      Two page mechanisms remain: table-driven rows (modOptionsRows.inc, reading/writing
'      gOptWork) and panel-hosted modules (frmOptionsCompiler / ...Local / ...Colors /
'      ...Keywords). Both build their controls on the scroll panel's page window on first
'      visit. There are no eager child forms left; frmOptions_SaveEditorOptions commits the
'      staging copy and asks each panel-hosted module for anything it owns outside it (the
'      localization file, the keyword blobs).

#include once "frmOptions.bi"
#include once "modOptionsRows.bi"
#include once "frmOptionsCompiler.bi"
#include once "frmOptionsColors.bi"
#include once "frmOptionsLocal.bi"
#include once "frmOptionsKeywords.bi"
#include once "frmMain.bi"

#include once "clsConfig.bi"


' The page currently on show, and whether each table-driven page has been built yet.
dim shared gOptCurrentPage as long = OPTPAGE_NONE
' The search box's live text. BuildNavList shows only pages whose title contains it (empty =
' all). It filters PAGE TITLES only for now -- per-setting search is a larger, separate feature.
dim shared gOptNavFilter as DWSTRING
dim shared gOptPageBuilt(OPTPAGE_COUNT - 1) as boolean


' ========================================================================================
' Kinds of page, in order of preference:
'   1. table-driven      OptionsRows_HasPage(nPage)     -- General, Code Editor
'   2. panel-hosted      frmOptions_PanelHostedOwns()   -- Compiler, Localization, Editor Font, Keywords
' Every page is now one of these two; there are no eager legacy child forms left. The
' dispatch functions below (ShowPage / layout / paint callbacks) branch table-first.
'
' A page is panel-hosted if any module claims it. Keep this in step with the branches below.
' ========================================================================================
private function frmOptions_PanelHostedOwns( byval nPage as long ) as boolean
    if frmOptionsCompiler_OwnsPage( nPage ) then return true
    if frmOptionsLocal_OwnsPage( nPage ) then return true
    if frmOptionsColors_OwnsPage( nPage ) then return true
    if frmOptionsKeywords_OwnsPage( nPage ) then return true
    function = false
end function


' ========================================================================================
' Dispatch the build / show / hide / layout / paint of a panel-hosted page to whichever
' module owns it. Keep in step with frmOptions_PanelHostedOwns above.
' ========================================================================================
private sub frmOptions_PanelBuild( byval nPage as long, byval hPage as HWND )
    if frmOptionsCompiler_OwnsPage( nPage ) then frmOptionsCompiler_Build( hPage )
    if frmOptionsLocal_OwnsPage( nPage ) then frmOptionsLocal_Build( hPage )
    if frmOptionsColors_OwnsPage( nPage ) then frmOptionsColors_Build( hPage )
    if frmOptionsKeywords_OwnsPage( nPage ) then frmOptionsKeywords_Build( nPage, hPage )
end sub

private sub frmOptions_PanelShow( byval nPage as long, byval bShow as boolean )
    if frmOptionsCompiler_OwnsPage( nPage ) then frmOptionsCompiler_Show( bShow )
    if frmOptionsLocal_OwnsPage( nPage ) then frmOptionsLocal_Show( bShow )
    if frmOptionsColors_OwnsPage( nPage ) then frmOptionsColors_Show( bShow )
    if frmOptionsKeywords_OwnsPage( nPage ) then frmOptionsKeywords_Show( nPage, bShow )
end sub

private function frmOptions_PanelLayout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long
    if frmOptionsCompiler_OwnsPage( nPage ) then return frmOptionsCompiler_Layout( hPage, cxPanel )
    if frmOptionsLocal_OwnsPage( nPage ) then return frmOptionsLocal_Layout( hPage, cxPanel )
    if frmOptionsColors_OwnsPage( nPage ) then return frmOptionsColors_Layout( hPage, cxPanel )
    if frmOptionsKeywords_OwnsPage( nPage ) then return frmOptionsKeywords_Layout( nPage, hPage, cxPanel )
    function = 0
end function

private sub frmOptions_PanelPaint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
    if frmOptionsCompiler_OwnsPage( nPage ) then frmOptionsCompiler_Paint( p )
    if frmOptionsLocal_OwnsPage( nPage ) then frmOptionsLocal_Paint( p )
    if frmOptionsColors_OwnsPage( nPage ) then frmOptionsColors_Paint( p )
    if frmOptionsKeywords_OwnsPage( nPage ) then frmOptionsKeywords_Paint( nPage, p )
end sub


' ========================================================================================
' The localized title shown in the page's header band.
' ========================================================================================
private function frmOptions_PageTitle( byval nPage as long ) as DWSTRING
    select case nPage
        case OPTPAGE_GENERAL:         return L(186,"General Options")
        case OPTPAGE_EDITOR:          return L(106,"Code Editor")
        case OPTPAGE_EDITOR2:         return L(103,"Advanced Code Editor")
        case OPTPAGE_COLORS:          return L(319,"Editor Font")
        case OPTPAGE_COMPILER:        return L(107,"Compiler Setup")
        case OPTPAGE_LOCAL:           return L(124,"Localization")
        case OPTPAGE_KEYWORDS:        return L(141,"FreeBASIC Keywords")
        case OPTPAGE_KEYWORDSWINAPI:  return L(275,"Windows Api Keywords")
        case OPTPAGE_KEYWORDSEXTRA:   return L(277,"Extra Keywords")
    end select
    ' RETURN throughout, never `function =`: fbc rejects mixing the two styles in a
    ' procedure returning an object with a constructor, and DWSTRING is one.
    return ""
end function


' ========================================================================================
' Save all options to the gConfig class.
'
' Everything except the three Keywords pages is committed from the staging copy now; the
' Keywords widget harvest is the last remnant of the old per-page GetDlgItem harvest.
' ========================================================================================
function frmOptions_SaveEditorOptions() as long

    dim bChangesNextRun as boolean

    ' Pages owned by the staging copy: General Options, Code Editor (the old Code Editor
    ' and Advanced Code Editor pages merged), and Compiler Setup. All committed at once;
    ' Cancel simply never calls this.
    OptionsWork_Commit()

    ' Arm or disarm the external-change poll to match what was just committed. It has to
    ' happen here rather than at startup only, or turning the option on would not take effect
    ' until the next run -- and turning it off would leave the timer running.
    FileWatch_ApplyConfig()

    ' If a localization file is currently being edited then save it now.
    frmOptionsLocal_LocalEditCheck

    ' If the localization has changed then note that a restart is needed -- meaning either a
    ' different language was SELECTED, or the selected language's own file was rewritten.
    '
    ' It used to ask `gLocalPhrasesEdit = true` instead of the second test, which was a proxy
    ' for "the strings this session is using have changed" and stopped being one when editing
    ' and selecting became separate actions. Creating a new language, or fixing a typo in one
    ' the editor is not running in, changes nothing about this session and must not claim it does.
    dim as boolean bLangSwitched = (ucase(gLocalCurrentFile) <> ucase(gConfig.LocalizationFile))
    if bLangSwitched orelse (gLocalSelectedEdited = true) then
        bChangesNextRun = true
    end if
    gConfig.LocalizationFile = gLocalCurrentFile

    ' Themes and Fonts, and Compiler Setup, are committed by OptionsWork_Commit above
    ' (staging copy).


    ' KEYWORDS. Harvest the (built) editors into gConfig and persist -- gated on the dirty
    ' flag exactly as before. An unvisited keyword page has no editor and keeps its value.
    if gConfig.bKeywordsDirty then
        frmOptionsKeywords_Harvest()
        gConfig.SaveKeywords
    end if

    ' Output the config settings to disk file
    gConfig.SaveConfigFile

    ' If Occurrences Highlighting is false then ensure that any highlights on the screen will be removed.
    dim pDoc as clsDocument ptr
    pDoc = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        frmMain_HighlightWord( pDoc, "" )
    end if

    if bChangesNextRun then
        frmOptions_ThemedMsgBox( _
                L(104,"Changes will be applied the next time the application is run."), _
                L(105,"Warning"), TMB_ICON_INFO, TMB_OK )
    end if

    function = 0
End function


' ========================================================================================
' Paint one row of the navigation list.
' ========================================================================================
private sub frmOptions_NavPaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr, foreclr
    if p->isSelected then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    elseif p->isHot then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        ' Idle rows read in the nav panel's own shade -- the separator between nav and page now.
        backclr = theme.toptabs.texthot.backcolor
        foreclr = theme.panel.text.forecolor
    end if

    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    ' The list is flat, so there is no header branch here -- every row is a page.
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(16)
    p->b->SetFont( OptionsFont_Base() )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub


' ========================================================================================
' The user picked a different navigation row (mouse or keyboard).
' ========================================================================================
private sub frmOptions_NavSelChangeCallback( byval hList as HWND, byval row as integer )
    if row < 0 then exit sub
    frmOptions_ShowPage( PsListTree_GetItemData( hList, row ) )
end sub


' ========================================================================================
' THE PLAIN PAGE CONTAINER -- the parent of EVERY page.
'
' NOTHING SCROLLS. There used to be a PsScrollPanel for the Code Editor page, the one page long
' enough to need it; it was converted to a plain page like the rest once Tab Size and Case moved
' off it, so this window is now the sole container. Whatever a page lays out is what the user
' sees: a page that does not fit CLIPS rather than quietly growing a scrollbar.
'
' That is the point rather than a limitation. A page that must fit is a page whose height can be
' ASSERTED -- see frmOptions_RunFitSelfTest -- and a fixed-size dialog means there is exactly one
' number to assert against instead of "whatever the user dragged the window to".
'
' IT IS NOT A Ps* CONTROL and should not become one. It exists to provide the four services the
' scroll panel's page window was providing to those nine pages, and nothing else:
'
'   1. a themed background, painted through the same SCP_PAINTINFO the pages' _Paint callbacks
'      already take -- so not one page's signature changed when they moved off the scroll panel
'   2. WM_CTLCOLOR*, so a plain EDIT or STATIC on a page is themed rather than system-white
'   3. forwarding unclaimed WM_COMMAND / WM_NOTIFY to the dialog, which is where the handlers are
'   4. WS_EX_CONTROLPARENT, without which IsDialogMessage cannot Tab into the page at all
'
' Point 4 is the one worth stating twice: this window IS a container, so it needs the flag that
' PsButton and its focus-taking siblings must NOT have. Same rule, opposite answer -- the test is
' "does this window have children the dialog manager should find?", and here it does.
' ========================================================================================
dim shared ghOptPageBrush       as HBRUSH


' Which window is this page's controls' parent? A pure function of the page id, and it has to be:
' a page is BUILT onto a parent once and lives there, so the answer must never change for a given
' page between one call and the next.
private function frmOptions_PageParent( byval nPage as long ) as HWND
    ' ONE CONTAINER, no exceptions. The Code Editor page used to be the sole tenant of a
    ' PsScrollPanel; it is a plain non-scrolling page like every other now, so nothing scrolls
    ' and there is nothing to choose between.
    return HWND_FRMOPTIONS_PAGE
end function


private function frmOptions_PageWndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer.
        return true

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )

        ' The SAME payload the scroll panel hands its callback, filled by hand. That is what
        ' let all nine pages move here without touching their _Paint functions.
        dim p as SCP_PAINTINFO
        p.hScrollPanel = hwnd
        p.hPanel       = hwnd
        p.b            = @b
        GetClientRect( hwnd, @p.rcClient )
        p.nPos         = 0

        b.SetBackColor( theme.panel.text.backcolor )
        b.PaintRect( @p.rcClient )

        if OptionsRows_HasPage( gOptCurrentPage ) then
            OptionsRows_PaintPage( gOptCurrentPage, @p )
        elseif frmOptions_PanelHostedOwns( gOptCurrentPage ) then
            frmOptions_PanelPaint( gOptCurrentPage, @p )
        end if

        b.EndDoubleBuffer()
        return 0

    case WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORLISTBOX
        ' Without this a plain EDIT or STATIC on a page comes up system-white on a dark panel.
        ' The brush is cached and rebuilt by frmOptions_ApplyTheme, because returning a brush
        ' created here would leak one per repaint.
        SetBkColor( cast(HDC, wParam), theme.panel.text.backcolor )
        SetTextColor( cast(HDC, wParam), theme.panel.text.forecolor )
        if ghOptPageBrush = 0 then ghOptPageBrush = CreateSolidBrush( theme.panel.text.backcolor )
        return cast( LRESULT, ghOptPageBrush )

    case WM_COMMAND, WM_NOTIFY, WM_HSCROLL, WM_VSCROLL
        ' The handlers live on the dialog, not here. Forwarded rather than handled so a page's
        ' controls behave identically whichever container they happen to be parented to.
        return SendMessageW( GetParent(hwnd), uMsg, wParam, lParam )

    case WM_NCDESTROY
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' Lay out the current page on the plain container. The scroll panel drives its own page through a
' callback; this one has nobody to do that, so the dialog does it -- on every show and every time
' the container is sized.
'
' THE RETURNED HEIGHT IS DELIBERATELY IGNORED HERE. On a scrolling page it sets the scroll range;
' on a fixed one there is nothing to do with it, and pretending otherwise would invite a
' "shrink to fit" that quietly made controls the wrong size. Overflow is a defect to be asserted,
' not absorbed.
private sub frmOptions_LayoutPlainPage()
    if HWND_FRMOPTIONS_PAGE = 0 then exit sub
    if gOptCurrentPage = OPTPAGE_NONE then exit sub

    dim as RECT rc
    GetClientRect( HWND_FRMOPTIONS_PAGE, @rc )
    dim as long cx = rc.right - rc.left
    if cx <= 0 then exit sub

    if OptionsRows_HasPage( gOptCurrentPage ) then
        OptionsRows_LayoutPage( gOptCurrentPage, HWND_FRMOPTIONS_PAGE, cx )
    elseif frmOptions_PanelHostedOwns( gOptCurrentPage ) then
        frmOptions_PanelLayout( gOptCurrentPage, HWND_FRMOPTIONS_PAGE, cx )
    end if
end sub


' See frmOptions.bi. The private layout above cannot be called from the panel modules (they are
' included before this file); this is the public door.
sub frmOptions_RelayoutCurrentPage()
    frmOptions_LayoutPlainPage()
    if HWND_FRMOPTIONS_PAGE then AfxRedrawWindow( HWND_FRMOPTIONS_PAGE )
end sub


' ========================================================================================
' The scroll panel's viewport changed width: re-flow the current page and report how tall
' it now is. cxPanel is handed to us -- do not read the width back off the window.
'
' ONLY OPTPAGE_EDITOR REACHES THIS NOW. The other nine pages are on the plain container and are
' laid out by frmOptions_LayoutPlainPage. The dispatch below is left general rather than being
' narrowed to the editor page, so that moving a page back onto the scroll panel is a one-line
' change to frmOptions_PageParent and nothing else.
' ========================================================================================
private function frmOptions_PageLayoutCallback( byval hPanel as HWND, byval cxPanel as integer ) as integer

    if gOptCurrentPage = OPTPAGE_NONE then return 0

    if OptionsRows_HasPage( gOptCurrentPage ) then
        return OptionsRows_LayoutPage( gOptCurrentPage, hPanel, cxPanel )
    end if

    if frmOptions_PanelHostedOwns( gOptCurrentPage ) then
        return frmOptions_PanelLayout( gOptCurrentPage, hPanel, cxPanel )
    end if

    function = 0
end function


' ========================================================================================
' Switch the visible page. Every page is built on first visit (table-driven or
' panel-hosted) -- there are no eager legacy child forms left.
' ========================================================================================
sub frmOptions_ShowPage( byval nPage as long )

    if nPage < 0 then exit sub
    if nPage >= OPTPAGE_COUNT then exit sub
    if nPage = gOptCurrentPage then exit sub

    dim as HWND hPanel = frmOptions_PageParent( nPage )
    if hPanel = 0 then exit sub

    ' Hide the outgoing page.
    if gOptCurrentPage <> OPTPAGE_NONE then
        if OptionsRows_HasPage( gOptCurrentPage ) then
            OptionsRows_ShowPage( gOptCurrentPage, false )
        elseif frmOptions_PanelHostedOwns( gOptCurrentPage ) then
            frmOptions_PanelShow( gOptCurrentPage, false )
        end if
    end if

    gOptCurrentPage = nPage
    OptionsDialogLastOpened = nPage

    ' Build the incoming page on first visit (every page is lazy now).
    if OptionsRows_HasPage( nPage ) then
        if gOptPageBuilt(nPage) = false then
            OptionsRows_BuildPage( nPage, hPanel )
            gOptPageBuilt(nPage) = true
        end if
        OptionsRows_ShowPage( nPage, true )
    elseif frmOptions_PanelHostedOwns( nPage ) then
        if gOptPageBuilt(nPage) = false then
            frmOptions_PanelBuild( nPage, hPanel )
            gOptPageBuilt(nPage) = true
        end if
        frmOptions_PanelShow( nPage, true )
    end if

    ' The page container is shown once at creation and never hidden -- there is only one now.
    ' Nothing drives its layout for it, so the dialog does.
    frmOptions_LayoutPlainPage()

    AfxRedrawWindow( hPanel )
    AfxRedrawWindow( HWND_FRMOPTIONS )

end sub


' ========================================================================================
' Position the shell's own children. Called from WM_SIZE and once after creation.
' ========================================================================================
private sub frmOptions_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long navW    = AfxScaleX(FRMOPTIONS_NAV_W)
    dim as long titleH  = AfxScaleY(FRMOPTIONS_TITLE_H)
    dim as long footerH = AfxScaleY(FRMOPTIONS_FOOTER_H)
    dim as long margin  = AfxScaleX(FRMOPTIONS_MARGIN)
    dim as long btnW    = AfxScaleX(FRMOPTIONS_BTN_W)
    dim as long btnH    = AfxScaleY(FRMOPTIONS_BTN_H)

    dim as long bodyH   = rc.bottom - footerH
    if bodyH < 0 then bodyH = 0

    ' The nav list starts LOWER than the window's top edge, aligned with the first control on
    ' the page beside it. A page's content begins at titleH (below the title band) plus the row
    ' padding; PsListTree draws its first row flush at its own client top, so giving the nav that
    ' same top makes "General Options" sit on the same line as the first control of whatever page
    ' is showing. The strip left above it is the dialog's own panel background -- seamless,
    ' because the nav rows paint that same colour.
    ' The search box sits in the nav column's slice of the title band (the corner that was
    ' blank), so it lines up with the page title on the right. The nav list stays below it,
    ' where it already aligned with the first control of the page.
    ' The FRAME spans [mx .. navW-mx] and is drawn by the dialog. The box lives INSIDE it: past
    ' the glyph gutter on the left, and inset by the border on the other three sides so it never
    ' paints over the frame.
    dim as long mx      = AfxScaleX(FRMOPTIONS_MARGIN)
    dim as long searchH = AfxScaleY(FRMOPTIONS_SEARCH_H)
    dim as long searchY = (titleH - searchH) \ 2
    dim as long glyphW  = AfxScaleX(FRMOPTIONS_SEARCH_GLYPHW)
    dim as long inset   = AfxScaleX(2)
    dim as long boxX = mx + glyphW
    dim as long boxW = (navW - mx - inset) - boxX
    if boxW < 0 then boxW = 0
    ' Shrunk from the top by 2px to follow the frame, whose top edge is nudged down the same
    ' amount in frmOptions_OnPaint -- the box top-inset now matches its bottom.
    dim as long searchShrink = AfxScaleY(2)
    SetWindowPos( HWND_FRMOPTIONS_SEARCH, 0, boxX, searchY + inset + searchShrink, _
                  boxW, searchH - (2 * inset) - searchShrink, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long navTop = titleH + AfxScaleY(OPTROW_PADTOP)
    dim as long navH   = bodyH - navTop
    if navH < 0 then navH = 0
    ' Inset the nav listbox by AfxScaleX(2) on BOTH sides within the nav column: pushed off the
    ' client left edge, and pulled the same amount off the divider on the right.
    dim as long navInset = AfxScaleX(2)
    dim as long navListW = navW - (navInset * 2)
    if navListW < 0 then navListW = 0
    SetWindowPos( HWND_FRMOPTIONS_NAV, 0, navInset, navTop, navListW, navH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long scrollX = navW
    dim as long scrollY = titleH
    dim as long scrollW = rc.right - navW
    dim as long scrollH = bodyH - titleH
    if scrollW < 0 then scrollW = 0
    if scrollH < 0 then scrollH = 0
    SetWindowPos( HWND_FRMOPTIONS_PAGE, 0, scrollX, scrollY, scrollW, scrollH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    frmOptions_LayoutPlainPage()

    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMOPTIONS_CMDCANCEL), 0, _
                  rc.right - margin - btnW, btnY, btnW, btnH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( GetDlgItem(hwnd, IDC_FRMOPTIONS_CMDOK), 0, _
                  rc.right - margin - (btnW * 2) - AfxScaleX(8), btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

end sub


' ========================================================================================
' Paint the shell's own chrome: the background, the page title band, and the rule above
' the footer. Everything else on screen belongs to a child control.
' ========================================================================================
private function frmOptions_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PAINTSTRUCT ps
    dim as HDC hdc = BeginPaint( hwnd, @ps )

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim buffer as PsBufferPaint
    buffer.BeginDoubleBuffer( hwnd, hdc, rc )

    buffer.SetBackColor( theme.panel.text.backcolor )
    buffer.PaintRect( @rc )

    dim as long navW    = AfxScaleX(FRMOPTIONS_NAV_W)
    dim as long titleH  = AfxScaleY(FRMOPTIONS_TITLE_H)
    dim as long footerH = AfxScaleY(FRMOPTIONS_FOOTER_H)

    ' Fill the WHOLE left panel column in its own lighter shade, so the nav list's inset strips,
    ' the search-box band above it, and the empty area below the last row all read the same --
    ' the nav listbox only paints its own (inset) client, which left dark gaps around it. This is
    ' the separator between the nav column and the page now that the vertical rule is gone.
    dim as RECT rcNavPanel
    rcNavPanel.left   = 0
    rcNavPanel.top    = 0
    rcNavPanel.right  = navW
    rcNavPanel.bottom = rc.bottom
    buffer.SetBackColor( theme.toptabs.texthot.backcolor )
    buffer.PaintRect( @rcNavPanel )

    ' Page title band.
    dim as RECT rcTitle
    rcTitle.left   = navW + AfxScaleX(OPTROW_PADLEFT)
    rcTitle.top    = 0
    rcTitle.right  = rc.right - AfxScaleX(OPTROW_PADRIGHT)
    rcTitle.bottom = titleH
    buffer.SetForeColor( theme.panel.texthot.forecolor )
    ' Pinned bold match for the 11pt base font (see modOptionsRows.bi); does not follow the
    ' TIKO_OPTIONS_FONT override.
    buffer.SetFont( ghFont(GUIFONTBOLD_11) )
    buffer.PaintText( frmOptions_PageTitle(gOptCurrentPage), @rcTitle, DT_LEFT or DT_END_ELLIPSIS )

    ' THE SEARCH FRAME. One rounded rect around the glyph AND the box, filled in the field
    ' colour so the borderless box (same colour) sits inside it invisibly, then the magnifying
    ' glass on top of the fill, then the border last so it stays crisp. The box is a child and
    ' paints over its own sub-region afterwards -- which is fine, it is the same colour and it is
    ' inset inside this border.
    dim as long mxG      = AfxScaleX(FRMOPTIONS_MARGIN)
    dim as long searchHG = AfxScaleY(FRMOPTIONS_SEARCH_H)
    dim as long searchYG = (titleH - searchHG) \ 2
    dim as long curveG   = AfxScaleY(8)

    dim as RECT rcFrame
    rcFrame.left   = mxG
    ' Top edge nudged down 2px -- the frame read a touch high against the box inside it.
    rcFrame.top    = searchYG + AfxScaleY(2)
    rcFrame.right  = navW - mxG
    rcFrame.bottom = searchYG + searchHG

    buffer.SetBackColor( theme.ui.field.backcolor )
    buffer.PaintRoundRect( @rcFrame, curveG )

    dim as RECT rcGlyph
    rcGlyph.left   = mxG
    rcGlyph.top    = searchYG
    rcGlyph.right  = mxG + AfxScaleX(FRMOPTIONS_SEARCH_GLYPHW)
    rcGlyph.bottom = searchYG + searchHG
    buffer.SetForeColor( theme.panel.text.forecolor )
    buffer.SetBackColor( theme.ui.field.backcolor )
    buffer.SetFont( ghFont(SYMBOLFONT_10) )
    buffer.PaintText( wchr(&hE721), @rcGlyph, DT_CENTER or DT_VCENTER or DT_SINGLELINE )

    buffer.SetPenColor( theme.ui.divider )
    buffer.PaintRoundOutline( @rcFrame, curveG, 1 )

    ' There is NO vertical rule between the nav list and the page any more -- the nav panel's
    ' own distinct background (theme.toptabs.texthot.backcolor) separates the two columns instead.
    ' There is also deliberately NO rule above the footer buttons and none between settings rows.

    ' No rule under the page title -- the dialog has no divider lines at all now; the nav
    ' column's own background separates it from the page.

    buffer.EndDoubleBuffer()
    EndPaint( hwnd, @ps )

    function = 0
end function


' ========================================================================================
' OK / Cancel. PsButton does not post WM_COMMAND to its parent, so the buttons route
' through their click callback; IDCANCEL still arrives as a WM_COMMAND because
' IsDialogMessage synthesises one for the Escape key.
' ========================================================================================

private sub frmOptions_ApplyAndClose( byval hwnd as HWND )

    frmOptions_SaveEditorOptions()

    ' NO COLOUR EDITS ARE PERSISTED HERE ANY MORE. frmThemes owns the theme's whole
    ' save/cancel lifecycle -- snapshot, live preview, file write and restore -- so this
    ' dialog no longer knows the theme engine exists beyond loading what gConfig names.

    ' The dialog has been previewing live, so gTheme() already holds the user's pick. Reload
    ' from disk anyway, now that SaveEditorOptions has written gConfig: it makes what ships
    ' exactly what a fresh start would load, rather than assuming the two agree -- and after
    ' a colour edit it is a genuine round-trip check of what we just wrote.
    LoadThemeFile()


    ' One call replaces the old hand-written fan-out (four AfxRedrawWindow calls, the two
    ' SyncThemes and the per-document ApplyProperties walk) -- all of that now lives in
    ' Theme_ApplySubsystems, which the live preview drives too, so there is one path instead
    ' of two that could disagree.
    Theme_ApplyAll()

    ' Still required, and NOT made redundant by the above: these are LAYOUT, not colour.
    ' Hiding the toolbar or the statusbar changes geometry no repaint can fix.
    frmOutput_PositionWindows()
    frmMenuBar_PositionWindows()
    frmMain_PositionWindows

    SendMessage( hwnd, WM_CLOSE, 0, 0 )
end sub


private sub frmOptions_ButtonClickCallback( byval hButton as HWND, byval id as long )
    select case id
        case IDC_FRMOPTIONS_CMDOK:      frmOptions_ApplyAndClose( HWND_FRMOPTIONS )
        case IDC_FRMOPTIONS_CMDCANCEL:  SendMessage( HWND_FRMOPTIONS, WM_CLOSE, 0, 0 )
    end select
end sub


private function frmOptions_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
        case IDCANCEL, IDC_FRMOPTIONS_CMDCANCEL
            SendMessage( hwnd, WM_CLOSE, 0, 0 )
            exit function
        case IDC_FRMOPTIONS_CMDOK
            frmOptions_ApplyAndClose( hwnd )
            exit function
    end select

    function = 0
end function


' THE DIALOG IS NOT RESIZABLE, so there is no WM_GETMINMAXINFO handler and no minimum size.
'
' It used to be: WS_THICKFRAME plus a minimum of 700x480, with every page re-flowing to whatever
' width it was dragged to. That is gone deliberately. A FIXED client turns "does this page fit?"
' from a question with no stable answer -- it depended on how the user had dragged the window --
' into one number that can be asserted, which is what frmOptions_SelfTest below does.
'
' The layouts still TAKE a width and still compute from it. That is not dead code: they are called
' with the fixed width, and at 175% DPI it is a different number than at 100%. Nothing assumes a
' constant.


' ========================================================================================
' DOES EVERY PAGE FIT?                                  (TIKO_OPTIONS_SELFTEST=1)
'
' The dialog is fixed-size and only OPTPAGE_EDITOR scrolls, so every other page has to fit its
' viewport or it CLIPS -- silently, with no scrollbar to hint that anything is missing.
'
' THIS CANNOT BE COMPUTED STATICALLY, which is why it is a runtime assertion rather than a
' comment. Each page's Layout accumulates a running `y` over controls whose heights come from
' GetIdealSize, the current font and the DPI; the answer at 100% is not the answer at 175%. So
' the suite builds every page, lays it out at the real width, and compares what it asks for
' against what it gets.
'
' IT VISITS THE PAGES THROUGH frmOptions_ShowPage, not by calling the layouts directly. A page
' has to be BUILT before it can be laid out, and building is what ShowPage owns -- reaching past
' it would measure a page whose controls do not exist yet and cheerfully report that nothing
' overflows.
' ========================================================================================
sub frmOptions_RunFitSelfTest()

    if environ("TIKO_OPTIONS_SELFTEST") <> "1" then exit sub

    dim as long nSaved = gOptCurrentPage

    dim as RECT rcClient
    GetClientRect( HWND_FRMOPTIONS, @rcClient )
    dim as long nViewH = rcClient.bottom - AfxScaleY(FRMOPTIONS_TITLE_H) - AfxScaleY(FRMOPTIONS_FOOTER_H)
    dim as long nViewW = rcClient.right - AfxScaleX(FRMOPTIONS_NAV_W)

    ? ""
    ? "--- TIKO_OPTIONS_SELFTEST (page fit) ---"
    dim as string sHdr = "viewport " & str(nViewW) & " x " & str(nViewH) & " px at this DPI"
    ? sHdr

    dim as long nOver = 0
    dim as long nBlank = 0

    for nPage as long = 0 to OPTPAGE_COUNT - 1

        frmOptions_ShowPage( nPage )

        dim as HWND hPageWnd = frmOptions_PageParent( nPage )
        if hPageWnd = 0 then continue for

        ' THE PAGE'S CONTAINER IS ACTUALLY VISIBLE. This is the assertion that would have caught
        ' the Code Editor page coming up blank: ShowPage had stopped calling ShowWindow(..SHOW)
        ' on the scroll panel, so the one scrolling page showed its title band and nothing else.
        ' The fit test builds and lays out a page whether or not it is shown, so height alone
        ' said everything was fine -- visibility is a separate fact and has to be its own check.
        dim as boolean bVisible = (IsWindowVisible( hPageWnd ) <> 0)
        dim as string sVis = iif( bVisible, "PASS  ", "FAIL  " )
        if bVisible = false then nBlank += 1
        ? sVis & "page " & str(nPage) & " " & str( frmOptions_PageTitle( nPage ) ) & " container is visible"

        dim as RECT rcPage
        GetClientRect( hPageWnd, @rcPage )
        dim as long cxPage = rcPage.right - rcPage.left
        if cxPage <= 0 then cxPage = nViewW

        dim as long nNeed = 0
        if OptionsRows_HasPage( nPage ) then
            nNeed = OptionsRows_LayoutPage( nPage, hPageWnd, cxPage )
        elseif frmOptions_PanelHostedOwns( nPage ) then
            nNeed = frmOptions_PanelLayout( nPage, hPageWnd, cxPage )
        end if

        ' NOTHING SCROLLS ANY MORE. Every page, Code Editor included, must fit the viewport.
        dim as boolean bMayScroll = false
        dim as boolean bFits = (nNeed <= nViewH)

        dim as string sVerdict
        if bFits then
            sVerdict = "PASS  "
        elseif bMayScroll then
            sVerdict = "scrolls"
        else
            sVerdict = "FAIL  "
            nOver += 1
        end if

        dim as string sLine = sVerdict & " page " & str(nPage) & " " & _
                              str( frmOptions_PageTitle( nPage ) ) & _
                              "   needs " & str(nNeed) & " of " & str(nViewH)
        ? sLine
    next

    dim as string sSum = "--- page fit: " & str(nOver) & " overflow a viewport they cannot scroll, " & _
                         str(nBlank) & " came up with no visible container ---"
    ? sSum
    ? ""

    ' Put the user back where they were. Leaving the dialog on whatever page the loop finished
    ' with would be a self-test with a side effect, which is its own kind of bug.
    if nSaved <> OPTPAGE_NONE then frmOptions_ShowPage( nSaved )

end sub


' Defined further down, next to the nav list it rebuilds; the focus self-test below calls it
' directly to prove that a change notification does not replace the search box.
declare sub frmOptions_SearchChanged( byval hTextBox as HWND )


' ========================================================================================
' Is hCandidate hContainer, or anything nested inside it?
'
' Needed because both windows this dialog asks about are CONTAINERS whose real tab stop is an
' inner child -- a PsTextBox is a shell around a RichEdit, a PsListTree a shell around the
' surface it paints -- and both carry WS_EX_CONTROLPARENT, so the dialog manager descends into
' them and hands back the inner child. GetFocus and GetNextDlgTabItem will never return the
' handle this file holds, so a direct handle comparison is wrong against a dialog that is
' behaving perfectly. (Same trap as frmKeyboard_IsWithin, and as PsNumericUpDown_HasFocus.)
' ========================================================================================
private function frmOptions_IsWithin( byval hCandidate as HWND, byval hContainer as HWND ) as boolean
    if (hCandidate = 0) orelse (hContainer = 0) then return false
    dim as HWND h = hCandidate
    do while h <> 0
        if h = hContainer then return true
        h = GetParent( h )
    loop
    return false
end function


' ========================================================================================
' Should this message move the focus from the search box down onto the nav list?
'
' A PURE FUNCTION of (message, key, who has focus), and that is the point: the pump it serves
' cannot be reached from a self-test -- the suite runs before the message loop starts and never
' pumps a message -- so the rule would otherwise be a comment with nothing checking it. Split
' out, its whole truth table is assertable.
'
' hFocus is passed in rather than read here for the same reason.
' ========================================================================================
private function frmOptions_IsSearchDownArrow( byval nMessage as UINT, _
                                               byval nKey as WPARAM, _
                                               byval hFocus as HWND ) as boolean
    ' Early returns rather than one andalso chain: the two comparisons yield non-booleans and
    ' frmOptions_IsWithin yields a boolean, and andalso-ing the kinds together is fbc warning 38.
    if nMessage <> WM_KEYDOWN then return false
    if nKey <> VK_DOWN then return false
    ' Only while the focus is IN the search box. Once the list has it, Down is the list's own
    ' row navigation and must not be stolen.
    return frmOptions_IsWithin( hFocus, HWND_FRMOPTIONS_SEARCH )
end function


' ========================================================================================
' WHERE DOES THE FOCUS START, AND WHERE DOES TAB GO?    (TIKO_OPTIONS_SELFTEST=1)
'
' Runs from frmOptions_Show AFTER the dialog is shown and the initial SetFocus has happened:
' half of these assertions read GetFocus, which before that point is not the dialog's at all.
'
' It exists because tab order here is not written down anywhere -- it is a side effect of the
' order the controls are created in, several hundred lines above, with nothing at the creation
' site to say so. Moving one Create call past another silently reorders the dialog, and the
' only symptom is that the box you expected to type into is not the one holding the caret.
'
' Every comparison is by CONTAINMENT rather than by handle: see frmOptions_IsWithin.
' ========================================================================================
private sub frmOptions_RunFocusSelfTest()

    if environ("TIKO_OPTIONS_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sLine

    #macro FOCASSERT( _cond_, _desc_ )
        if (_cond_) then
            nPass += 1
        else
            nFail += 1
            sLine = "  FAIL: " & _desc_
            ? sLine
        end if
    #endmacro

    ? ""
    ? "--- TIKO_OPTIONS_SELFTEST (focus + tab order) ---"

    dim as HWND hFirst  = GetNextDlgTabItem( HWND_FRMOPTIONS, 0, false )
    dim as HWND hSecond = GetNextDlgTabItem( HWND_FRMOPTIONS, hFirst, false )

    FOCASSERT( frmOptions_IsWithin( hFirst,  HWND_FRMOPTIONS_SEARCH ), _
               "the first tab stop is not the search box" )
    FOCASSERT( frmOptions_IsWithin( hSecond, HWND_FRMOPTIONS_NAV ), _
               "the second tab stop is not the nav list" )
    FOCASSERT( hFirst <> hSecond, "the first two tab stops are the same window" )

    ' The dialog OPENS on the search box.
    FOCASSERT( frmOptions_IsWithin( GetFocus(), HWND_FRMOPTIONS_SEARCH ), _
               "the dialog did not open with focus in the search box" )

    ' AND TYPING DOES NOT REPLACE THE BOX. The change callback used to re-create the control on
    ' every keystroke, so from the first character on HWND_FRMOPTIONS_SEARCH named a zero-sized
    ' window that was never shown -- at which point the Down-arrow containment test in the pump
    ' silently stops matching, and the theme and layout passes address the wrong window.
    '
    ' IT HAS TO DRIVE THE CALLBACK ITSELF to test that. The four assertions above all sample the
    ' dialog at open, before anyone has typed, and would stay green against the bug; so would a
    ' sized/visible check written here, for the same reason. PsTextBox's programmatic setters are
    ' silent by contract, so SetText cannot raise the callback either -- calling it directly is
    ' the only route, and it is exactly the path a keystroke takes. Harmless: with the filter
    ' empty it rebuilds the same nav list and restores the same selection.
    scope
        dim as HWND hBefore = HWND_FRMOPTIONS_SEARCH
        frmOptions_SearchChanged( HWND_FRMOPTIONS_SEARCH )
        FOCASSERT( HWND_FRMOPTIONS_SEARCH = hBefore, _
                   "the search box was replaced by its own change callback" )

        dim as RECT rc
        GetWindowRect( HWND_FRMOPTIONS_SEARCH, @rc )
        dim as boolean bSized   = ((rc.right - rc.left) > 0) andalso ((rc.bottom - rc.top) > 0)
        dim as boolean bVisible = (IsWindowVisible( HWND_FRMOPTIONS_SEARCH ) <> 0)
        FOCASSERT( bSized, "after a change notification the search box is zero-sized" )
        FOCASSERT( bVisible, "after a change notification the search box is not visible" )
    end scope

    ' ---- THE DOWN-ARROW RULE, over its whole truth table. ----
    ' Three of the four are NEGATIVES, and they are what stop this being a test that would pass
    ' against "always move the focus". The nav case is the one that matters most in use: with the
    ' focus already on the list, Down is the list's own row navigation.
    scope
        dim as HWND hInSearch = GetFocus()                                   ' the RichEdit, mid-open
        dim as HWND hInNav    = GetNextDlgTabItem( HWND_FRMOPTIONS, hFirst, false )

        FOCASSERT( frmOptions_IsWithin( hInSearch, HWND_FRMOPTIONS_SEARCH ), _
                   "the Down-arrow probe is vacuous -- its search handle is not in the search box" )
        FOCASSERT( frmOptions_IsWithin( hInNav, HWND_FRMOPTIONS_NAV ), _
                   "the Down-arrow probe is vacuous -- its nav handle is not in the nav list" )

        FOCASSERT( frmOptions_IsSearchDownArrow( WM_KEYDOWN, VK_DOWN, hInSearch ), _
                   "Down in the search box does not move to the nav list" )
        FOCASSERT( frmOptions_IsSearchDownArrow( WM_KEYDOWN, VK_DOWN, hInNav ) = false, _
                   "Down is stolen from the nav list's own row navigation" )
        FOCASSERT( frmOptions_IsSearchDownArrow( WM_KEYDOWN, VK_UP, hInSearch ) = false, _
                   "Up in the search box moves the focus, and should not" )
        FOCASSERT( frmOptions_IsSearchDownArrow( WM_KEYUP, VK_DOWN, hInSearch ) = false, _
                   "the release of the Down key moves the focus a second time" )
    end scope

    dim as string sSum = "--- focus + tab order: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sSum
    ? ""

end sub


' ========================================================================================
' frmOptions Window procedure
' ========================================================================================
function frmOptions_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

        case WM_CREATE
            ' This is a modal popup window so disable the parent window
            EnableWindow(GetParent(hwnd), false)
            return 0

        case WM_ERASEBKGND
            ' Everything is painted in WM_PAINT through a PsBufferPaint.
            return true

        case WM_PAINT
            return frmOptions_OnPaint( hwnd )

        case WM_SIZE
            frmOptions_PositionWindows( hwnd )
            AfxRedrawWindow( hwnd )
            return 0

        case WM_COMMAND
            return frmOptions_OnCommand( hwnd, loword(wParam), cast(HWND, lParam), hiword(wParam) )

        case WM_CLOSE
            ' Enables parent window keeping parent's zorder
            EnableWindow(GetParent(hwnd), true)
            DestroyWindow hwnd
            return 0

        case WM_DESTROY
            PostQuitMessage(0)
            return 0

    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' A theme-matched message box owned by the dialog. See frmOptions.bi.
'
' This runs a nested modal loop (PsMessageBox_DoModal) inside the dialog's own message loop
' -- a loop within a loop within frmMain's. The owner is HWND_FRMOPTIONS.
' ========================================================================================
function frmOptions_ThemedMsgBox( byval wszText as DWSTRING, _
                                  byval wszCaption as DWSTRING, _
                                  byval nIcon as long, _
                                  byval nButtons as long ) as long

    ' Per-dialog: the owner, and the Options page font rather than the application default.
    ' Everything else (colours, the 22pt icon glyph, the localized buttons) is TikoMsgBox's.
    '
    ' A two-button box here only ever guards a destructive action, so Cancel is the default.
    dim as long nDefault = 0
    if nButtons = TMB_OKCANCEL then nDefault = 1

    return TikoMsgBox( HWND_FRMOPTIONS, wszText, wszCaption, _
                       nIcon, nButtons, nDefault, OptionsFont_Base() )
end function


' ========================================================================================
' Theme the shell's own controls.
' ========================================================================================
private sub frmOptions_ApplyTheme( byval hwnd as HWND )

    ' The nav panel reads in its own distinct shade -- this is what separates it from the page
    ' now that the vertical divider is gone.
    PsListTree_SetBackColor( HWND_FRMOPTIONS_NAV, theme.toptabs.texthot.backcolor )
    PsListTree_SetScrollBarColors( HWND_FRMOPTIONS_NAV, theme.panel.scrollbar.backcolor, _
                                 theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )

    if HWND_FRMOPTIONS_SEARCH then
        ' A field-coloured box: editor background, dimmed cue, divider border, accent on focus.
        ' Its background matches the frame the dialog fills behind it, so the two read as one
        ' surface. The frame (fill + border) is drawn in frmOptions_OnPaint.
        PsTextBox_SetBackColor( HWND_FRMOPTIONS_SEARCH, theme.ui.field.backcolor )
        PsTextBox_SetOuterBackColor( HWND_FRMOPTIONS_SEARCH, theme.ui.field.backcolor )
        PsTextBox_SetForeColor( HWND_FRMOPTIONS_SEARCH, theme.ui.field.forecolor )
        PsTextBox_SetCueBannerColor( HWND_FRMOPTIONS_SEARCH, theme.panel.text.forecolor )
    end if

        ' The plain container's WM_CTLCOLOR brush is derived from the back colour, so it is rebuilt
    ' here rather than at first use -- otherwise a theme change would keep handing out a brush of
    ' the PREVIOUS background for the rest of the session.
    if ghOptPageBrush then DeleteObject( ghOptPageBrush )
    ghOptPageBrush = CreateSolidBrush( theme.panel.text.backcolor )
    if HWND_FRMOPTIONS_PAGE then AfxRedrawWindow( HWND_FRMOPTIONS_PAGE )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )

    dim as HWND hOK     = GetDlgItem( hwnd, IDC_FRMOPTIONS_CMDOK )
    dim as HWND hCancel = GetDlgItem( hwnd, IDC_FRMOPTIONS_CMDCANCEL )
    PsButton_SetColors( hOK, @bc )
    PsButton_SetColors( hCancel, @bc )
    PsButton_Refresh( hOK )
    PsButton_Refresh( hCancel )

    OptionsRows_ApplyTheme()
    frmOptionsCompiler_ApplyTheme()   ' no-op until the page has been built
    frmOptionsLocal_ApplyTheme()      ' no-op until the page has been built
    frmOptionsColors_ApplyTheme()     ' no-op until the page has been built
    frmOptionsKeywords_ApplyTheme()   ' no-op until the page has been built

    ' Match the caption to the theme currently IN MEMORY. Must not read gConfig: while this
    ' dialog is open the user's pick is staged in gOptWork and gConfig still names the old
    ' theme, so the caption would lag the live preview all the way to OK.
    Theme_ApplyDarkCaption( hwnd )

end sub


' ========================================================================================
' Populate the grouped navigation list. Item rows carry their page id as itemData.
' ========================================================================================
declare sub frmOptions_BuildNavList( byval hList as HWND )

' Re-select the row that maps to the current page, if the filter still shows it. Silent -- it
' does not switch pages, only restores the highlight after a rebuild.
private sub frmOptions_NavReselect()
    if HWND_FRMOPTIONS_NAV = 0 then exit sub
    dim as long n = PsListTree_GetCount( HWND_FRMOPTIONS_NAV )
    for i as long = 0 to n - 1
        if PsListTree_GetItemData( HWND_FRMOPTIONS_NAV, i ) = gOptCurrentPage then
            PsListTree_SetCurSel( HWND_FRMOPTIONS_NAV, i )
            exit sub
        end if
    next
end sub


' The search text changed: re-flow the nav to the matching pages. Does NOT change which page is
' shown -- typing narrows the list, clicking a result switches. If the only match is a single
' page a user would expect to land on it, but auto-switching while they are still typing would
' yank the page out from under them, so it is left to a click.
' IT DOES NOT RE-CREATE THE SEARCH BOX, and it used to. A copy of the whole creation block
' below sat here from the day the box was added, so every keystroke built ANOTHER PsTextBox --
' zero-sized, never shown, never positioned -- and pointed HWND_FRMOPTIONS_SEARCH at it. The
' visible box kept the caret and kept working, which is why nothing looked wrong; but from the
' first character on, that global named an invisible orphan, so anything reaching for the box
' by handle (the theme pass, the layout, the Down-arrow focus test in the pump) was addressing
' the wrong window. It also leaked one control per keystroke.
private sub frmOptions_SearchChanged( byval hTextBox as HWND )
    gOptNavFilter = PsTextBox_GetText( hTextBox )
    frmOptions_BuildNavList( HWND_FRMOPTIONS_NAV )
    frmOptions_NavReselect()
    AfxRedrawWindow( HWND_FRMOPTIONS_NAV )
end sub


private sub frmOptions_BuildNavList( byval hList as HWND )

    PsListTree_BeginUpdate( hList )
    PsListTree_Clear( hList )

    ' FLAT -- no PsListTree_AddHeader calls anywhere. The original dialog's five groups had
    ' three holding a single item each, which is what made a flat list read better; splitting
    ' Advanced Code Editor back out does not change that.
    '
    ' THE ORDER HERE IS THE ORDER THE USER SEES, and it is deliberately not the enum's order:
    ' OPTPAGE_EDITOR2 is id 10 (appended, so stored page ids stay valid) but belongs directly
    ' under Code Editor.
    ' One helper per row so the filter test lives in exactly one place. A page whose title does
    ' not contain the (lower-cased) filter is simply not added -- the list shrinks to the matches.
    dim as string sFilter = lcase( trim( gOptNavFilter ) )
    #macro NAVITEM( wszTitle, nPage )
        scope
            dim as DWSTRING wszT = (wszTitle)
            if (len(sFilter) = 0) orelse (instr( PsLCase(wszT), sFilter ) > 0) then
                PsListTree_AddString( hList, wszT, (nPage) )
            end if
        end scope
    #endmacro

    NAVITEM( L(186,"General Options"),      OPTPAGE_GENERAL )
    NAVITEM( L(106,"Code Editor"),          OPTPAGE_EDITOR )
    NAVITEM( L(103,"Advanced Code Editor"), OPTPAGE_EDITOR2 )
    NAVITEM( L(319,"Editor Font"),          OPTPAGE_COLORS )
    NAVITEM( L(107,"Compiler Setup"),       OPTPAGE_COMPILER )
    NAVITEM( L(124,"Localization"),         OPTPAGE_LOCAL )
    NAVITEM( L(141,"FreeBASIC Keywords"),   OPTPAGE_KEYWORDS )
    NAVITEM( L(275,"Windows Api Keywords"), OPTPAGE_KEYWORDSWINAPI )
    NAVITEM( L(277,"Extra Keywords"),       OPTPAGE_KEYWORDSEXTRA )

    PsListTree_EndUpdate( hList )

end sub


' ========================================================================================
' Find the navigation row that carries a page id, or -1.
' ========================================================================================
private function frmOptions_NavRowForPage( byval hList as HWND, byval nPage as long ) as long
    for i as long = 0 to PsListTree_GetCount(hList) - 1
        if PsListTree_GetItemData( hList, i ) = nPage then return i
    next
    function = -1
end function


' ========================================================================================
' frmOptions_Show
' ========================================================================================
function frmOptions_Show( byval hWndParent as HWND ) as LRESULT

    ' Reload the config file in case a user has automated a change to it since
    ' the application started (for example, changing the compiler path).
    gConfig.LoadConfigFile()

    ' Seed the staging copy BEFORE any control is created: every table-driven control is
    ' built from gOptWork, not from gConfig.
    OptionsFont_InitFromEnv()
    OptionsRows_Init()
    OptionsWork_Load()
    OptionsRows_ResetCombos()
    frmOptionsCompiler_Reset()
    frmOptionsLocal_Reset()
    frmOptionsLocal_SeedFromConfig()
    frmOptionsColors_Reset()
    frmOptionsKeywords_Reset()
    ' The keyword editors start clean each open; the dirty flag is raised only by a user
    ' edit and gates the harvest-and-save on OK.
    gConfig.bKeywordsDirty = false
    gOptCurrentPage = OPTPAGE_NONE
    for i as long = 0 to OPTPAGE_COUNT - 1
        gOptPageBuilt(i) = false
    next

    dim pWindow as CWindow ptr = new CWindow

    HWND_FRMOPTIONS = pWindow->Create( hWndParent, L(108,"Options"), @frmOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMOPTIONS_CLIENT_W, FRMOPTIONS_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- the search box ---------------------------------------------------------------
    ' CREATED BEFORE THE NAV LIST, and the order is the point rather than tidiness: creation
    ' order IS tab order, so this makes the search box the first tab stop and the nav list the
    ' second. That is what lets the dialog open on the box and one Tab land on the list -- the
    ' shape frmKeyboard has, which asserts both facts for the same reason this file now does.
    '
    ' It sits in the blank top-left corner (the nav column's slice of the title band).
    ' Single line; its pump obligation (PsTextBox_FilterMessage) is already met by the loop
    ' below. The magnifying glyph is drawn by the dialog in the gutter to its left -- a child
    ' cannot be drawn over, so the box is inset past the gutter rather than straddling it.
    HWND_FRMOPTIONS_SEARCH = PsTextBox_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_SEARCH )
    PsTextBox_SetFont( HWND_FRMOPTIONS_SEARCH, OptionsFont_Base() )
    ' NO BORDER of its own. The dialog draws one frame around the glyph AND the box together, so
    ' the magnifying glass reads as being INSIDE the field rather than in a gutter beside it --
    ' which a child window cannot achieve, because it paints over anything drawn under it. The
    ' box is square for the same reason: the rounding lives on the drawn frame.
    PsTextBox_SetBorderWidth( HWND_FRMOPTIONS_SEARCH, 0 )
    PsTextBox_SetCornerRadius( HWND_FRMOPTIONS_SEARCH, 0 )
    PsTextBox_SetMargins( HWND_FRMOPTIONS_SEARCH, AfxScaleX(4), AfxScaleX(4) )
    PsTextBox_SetCueBannerText( HWND_FRMOPTIONS_SEARCH, L(320,"Search settings...") )
    PsTextBox_SetChangeCallback( HWND_FRMOPTIONS_SEARCH, @frmOptions_SearchChanged )

    ' ---- navigation list -------------------------------------------------------------
    HWND_FRMOPTIONS_NAV = PsListTree_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_NAVLIST )
    PsListTree_SetPaintCallback( HWND_FRMOPTIONS_NAV, @frmOptions_NavPaintCallback )
    PsListTree_SetSelChangeCallback( HWND_FRMOPTIONS_NAV, @frmOptions_NavSelChangeCallback )
    PsListTree_SetFont( HWND_FRMOPTIONS_NAV, OptionsFont_Base() )
    ' Row height is in UNSCALED units (PsListTree scales what it is handed) and has to grow
    ' with the base font, or a 12pt list clips its own rows.
    PsListTree_SetRowHeight( HWND_FRMOPTIONS_NAV, 26 + ((OptionsFont_PointSize() - 10) * 2) )
    frmOptions_BuildNavList( HWND_FRMOPTIONS_NAV )

    ' ---- scrolling page area ---------------------------------------------------------
    ' The plain page container, created BEFORE the scroll panel so it sits under it in the
    ' z-order; they overlap exactly and only one is ever visible.
    '
    ' WS_EX_CONTROLPARENT IS LOAD-BEARING: this window really is a container, so IsDialogMessage
    ' has to be told to descend into it. Without it Tab stops at the nav list and never reaches
    ' any control on nine of the ten pages (C:\dev\Learnings.md).
    scope
        dim pPage as CWindow ptr = new CWindow
        HWND_FRMOPTIONS_PAGE = pPage->Create( HWND_FRMOPTIONS, "", @frmOptions_PageWndProc, _
                                              0, 0, 0, 0, _
                                              WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                                              WS_EX_CONTROLPARENT )
    end scope

    ' The page container is shown once, here, and never hidden -- it is the only container now.
    ' (The Code Editor page's PsScrollPanel is gone: every page is plain and non-scrolling.)
    ShowWindow( HWND_FRMOPTIONS_PAGE, SW_SHOW )
    dim as HWND hPage = HWND_FRMOPTIONS_PAGE

    ' ---- footer buttons --------------------------------------------------------------
    dim as HWND hOK = PsButton_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_CMDOK )
    PsButton_SetText( hOK, L(0,"OK") )
    PsButton_SetFont( hOK, OptionsFont_Base() )
    PsButton_SetDefault( hOK, true )
    PsButton_SetClickCallback( hOK, @frmOptions_ButtonClickCallback )

    dim as HWND hCancel = PsButton_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_CMDCANCEL )
    PsButton_SetText( hCancel, L(1,"Cancel") )
    PsButton_SetFont( hCancel, OptionsFont_Base() )
    PsButton_SetClickCallback( hCancel, @frmOptions_ButtonClickCallback )

    ShowWindow( hOK, SW_SHOW )
    ShowWindow( hCancel, SW_SHOW )
    ShowWindow( HWND_FRMOPTIONS_NAV, SW_SHOW )
    ShowWindow( HWND_FRMOPTIONS_SEARCH, SW_SHOW )
    ' NEITHER container is shown here -- frmOptions_ShowPage picks one below, and showing both
    ' would flash the scroll panel over a page that does not live on it.

    ' No pages are created here any more -- every page (table-driven and panel-hosted) is
    ' built lazily on its first visit.

    frmOptions_ApplyTheme( HWND_FRMOPTIONS )
    frmOptions_PositionWindows( HWND_FRMOPTIONS )

    ' Restore the most recently viewed page, defaulting to the first.
    dim as long nStartPage = OptionsDialogLastOpened
    if (nStartPage < 0) orelse (nStartPage >= OPTPAGE_COUNT) then nStartPage = OPTPAGE_GENERAL
    frmOptions_ShowPage( nStartPage )

    dim as long nNavRow = frmOptions_NavRowForPage( HWND_FRMOPTIONS_NAV, nStartPage )
    if nNavRow >= 0 then PsListTree_SetCurSel( HWND_FRMOPTIONS_NAV, nNavRow )

    ' FIT FIRST, and the order is load-bearing rather than tidy: frmOptions_RunFitSelfTest
    ' visits every page through ShowPage, which is what BUILDS them. Run the row and font
    ' tests before it and they silently skip whichever pages the user has not opened yet --
    ' which is how the base-font comparison came to report a zero-row page.
    frmOptions_RunFitSelfTest()
    OptionsRows_RunSelfTest( hPage )

    ShowWindow( HWND_FRMOPTIONS, SW_SHOW )
    UpdateWindow( HWND_FRMOPTIONS )

    ' FOCUS STARTS IN THE SEARCH BOX, so the dialog opens ready to be typed at -- the usual
    ' reason for opening it is to find one setting among a hundred. Down from here moves to the
    ' nav list (see the pump below), so the keyboard route to the pages costs one keystroke.
    '
    ' It also has to be set explicitly for a second, unrelated reason: IsDialogMessage ignores
    ' Tab until a child already holds the focus, and an ordinary CWindow gets no dialog manager
    ' to arrange that. See the WS_EX_CONTROLPARENT section of Learnings.md.
    SetFocus( HWND_FRMOPTIONS_SEARCH )

    ' AFTER the SetFocus above, deliberately: one of its assertions samples GetFocus, and running
    ' it any earlier would read the instant before anything had been focused at all -- a test
    ' that reports a failure the user would never see.
    frmOptions_RunFocusSelfTest()

    ' ---- message loop ----------------------------------------------------------------
    ' CWindow.DoEvents cannot be used: it has no *_FilterMessage hooks and this dialog
    ' hosts controls that require them.

    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        ' Each live PsComboBox is offered the message: the filter takes a control handle
        ' rather than working off a global, and is safe on a closed list or a stale
        ' handle. It also carries the close-then-reopen guard for a click on its own
        ' button, so skipping it breaks the dropdown.
        dim as boolean bEaten = false
        for i as long = lbound(gOptCombos) to ubound(gOptCombos)
            if PsComboBox_FilterMessage( gOptCombos(i), @uMsg ) then bEaten = true: exit for
        next
        if bEaten then continue while

        ' The PsTextBox right-click menu; also covers any embedded PsNumericUpDown, whose
        ' own filter just forwards here.
        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' The Localization page's phrase list edits its translation column in place, and that is
        ' what makes this call mandatory. Its position is load-bearing in BOTH directions: ahead
        ' of the Escape block below, or Escape during an edit closes the whole dialog instead of
        ' cancelling the edit; and ahead of IsDialogMessage, or Enter fires the OK button instead
        ' of committing. It is safe when nothing is editing, and safe to leave in place if that
        ' page ever stops editing.
        if PsListTree_FilterMessage( @uMsg ) then continue while

        ' DOWN ARROW IN THE SEARCH BOX MOVES TO THE NAV LIST, the way Tab does. Type a few
        ' letters, press Down, and you are on the results -- without lifting a hand to the mouse
        ' or hunting for Tab. Same gesture as the Keyboard Shortcuts dialog.
        '
        ' The rule itself lives in frmOptions_IsSearchDownArrow, where the self-test can reach it.
        '
        ' Safe to claim: the box is single-line, so Down does nothing there anyway. Its POSITION
        ' is load-bearing in both directions -- after the two filters above, so an open right-click
        ' menu or an in-place edit gets its own Down first; before the Escape block, so neither
        ' interferes with the other.
        if frmOptions_IsSearchDownArrow( uMsg.message, uMsg.wParam, GetFocus() ) then
            SetFocus( HWND_FRMOPTIONS_NAV )
            continue while
        end if

        ' ESCAPE cancels the dialog -- but only after the popup filters above, so an open
        ' combobox list or context menu closes on Escape first. This is handled explicitly
        ' rather than left to IsDialogMessage's IDCANCEL, because a MULTILINE PsTextBox (the
        ' keyword editors) claims every key via DLGC_WANTALLKEYS, so IsDialogMessage hands
        ' Escape to the RichEdit instead of treating it as cancel. The old keyword pages
        ' subclassed the edit to catch Escape for exactly this reason.
        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessage( HWND_FRMOPTIONS, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMOPTIONS, @uMsg ) = 0 then
            TranslateMessage @uMsg
            DispatchMessage @uMsg
        end if
    wend

    ' Cancelled (or closed by Escape / the X): undo the live preview. Done AFTER the pump so
    ' the repaint lands on the desktop the dialog has just vacated rather than on controls
    ' that are about to be destroyed.

    ' delete the frmOptions CWindow class manually allocated memory
    if pWindow then delete(pWindow)

    ' No child forms to delete: every page is panel-hosted or table-driven, and all of
    ' those controls sit on the scroll panel's page and self-free when it is destroyed.

    HWND_FRMOPTIONS_NAV    = 0
    HWND_FRMOPTIONS_SEARCH = 0
    HWND_FRMOPTIONS_PAGE   = 0
    OptionsRows_ResetCombos()
    erase gOptRows

    function = 0
end function
