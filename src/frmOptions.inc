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

'  frmOptions.inc  --  the Environment Options dialog shell.
'
'  SHAPE
'      +---------------------+----------------------------------------+
'      |  PsListBox (nav)     |  page title band  (fixed, not scrolled) |
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
dim shared gOptPageBuilt(OPTPAGE_COUNT - 1) as boolean


' ========================================================================================
' Kinds of page, in order of preference:
'   1. table-driven      OptionsRows_HasPage(nPage)     -- General, Code Editor
'   2. panel-hosted      frmOptions_PanelHostedOwns()   -- Compiler, Localization, Themes+Fonts, Keywords
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
        case OPTPAGE_GENERAL:         return L(226,"General Options")
        case OPTPAGE_EDITOR:          return L(110,"Code Editor")
        case OPTPAGE_COLORS:          return L(111,"Themes and Fonts")
        case OPTPAGE_COMPILER:        return L(112,"Compiler Setup")
        case OPTPAGE_LOCAL:           return L(129,"Localization")
        case OPTPAGE_KEYWORDS:        return L(177,"FreeBASIC Keywords")
        case OPTPAGE_KEYWORDSWINAPI:  return L(435,"Windows Api Keywords")
        case OPTPAGE_KEYWORDSEXTRA:   return L(439,"Extra Keywords")
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

    ' If a localization file is currently being edited then save it now.
    frmOptionsLocal_LocalEditCheck

    ' If the localization has changed then note that a restart is needed. The current file
    ' now lives in gLocalCurrentFile (was the FRAMELOCALIZATION groupbox caption).
    if (ucase(gLocalCurrentFile) <> ucase(gConfig.LocalizationFile)) orelse (gLocalPhrasesEdit = true) then
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
                L(108,"Changes will be applied the next time the application is run."), _
                L(109,"Warning"), MBX_ICON_INFO, MBX_BTN_OK )
    end if

    function = 0
End function


' ========================================================================================
' Paint one row of the navigation list.
' ========================================================================================
private sub frmOptions_NavPaintCallback( byval p as PSLISTBOX_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr, foreclr
    if p->isSelected then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    elseif p->isHot then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
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
    frmOptions_ShowPage( PsListBox_GetItemData( hList, row ) )
end sub


' ========================================================================================
' Paint the scroll panel's page: background, then whatever owns the current page.
' ========================================================================================
private sub frmOptions_PagePaintCallback( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    p->b->SetBackColor( ghPanel.BackColor )
    p->b->PaintRect( @p->rcClient )
    if OptionsRows_HasPage( gOptCurrentPage ) then
        OptionsRows_PaintPage( gOptCurrentPage, p )
    elseif frmOptions_PanelHostedOwns( gOptCurrentPage ) then
        frmOptions_PanelPaint( gOptCurrentPage, p )
    end if
end sub


' ========================================================================================
' The scroll panel's viewport changed width: re-flow the current page and report how tall
' it now is. cxPanel is handed to us -- do not read the width back off the window.
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

    dim as HWND hPanel = PsScrollPanel_GetPanel( HWND_FRMOPTIONS_SCROLL )
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

    ' Re-flow for the current width and go back to the top of the page.
    PsScrollPanel_SetPos( HWND_FRMOPTIONS_SCROLL, 0 )
    PsScrollPanel_Recalc( HWND_FRMOPTIONS_SCROLL )
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

    SetWindowPos( HWND_FRMOPTIONS_NAV, 0, 0, 0, navW, bodyH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long scrollX = navW
    dim as long scrollY = titleH
    dim as long scrollW = rc.right - navW
    dim as long scrollH = bodyH - titleH
    if scrollW < 0 then scrollW = 0
    if scrollH < 0 then scrollH = 0
    SetWindowPos( HWND_FRMOPTIONS_SCROLL, 0, scrollX, scrollY, scrollW, scrollH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

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

    buffer.SetBackColor( ghPanel.BackColor )
    buffer.PaintRect( @rc )

    dim as long navW    = AfxScaleX(FRMOPTIONS_NAV_W)
    dim as long titleH  = AfxScaleY(FRMOPTIONS_TITLE_H)
    dim as long footerH = AfxScaleY(FRMOPTIONS_FOOTER_H)

    ' Page title band.
    dim as RECT rcTitle
    rcTitle.left   = navW + AfxScaleX(OPTROW_PADLEFT)
    rcTitle.top    = 0
    rcTitle.right  = rc.right - AfxScaleX(OPTROW_PADRIGHT)
    rcTitle.bottom = titleH
    buffer.SetForeColor( ghPanel.ForeColorHot )
    ' Pinned bold match for the 11pt base font (see modOptionsRows.bi); does not follow the
    ' TIKO_OPTIONS_FONT override.
    buffer.SetFont( ghFont(GUIFONTBOLD_11) )
    buffer.PaintText( frmOptions_PageTitle(gOptCurrentPage), @rcTitle, DT_LEFT or DT_END_ELLIPSIS )

    ' The one rule left in the dialog: the vertical split between the navigation list and
    ' the page. A hairline -- width 1, deliberately not DPI-scaled. There is deliberately
    ' NO rule above the footer buttons and none between settings rows.
    buffer.SetForeColor( ghEditor.Divider )
    buffer.PaintLine( 1, navW, 0, navW, rc.bottom - footerH )

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

    ' Reload the theme file in case it has changed
    LoadThemeFile()
    ' Refresh main background in case theme changed and no file open
    AfxRedrawWindow( HWND_FRMMAIN )
    ' Refresh the frmOutput controls (listviews and RichEdits)
    AfxRedrawWindow( HWND_FRMOUTPUT )
    frmOutput_SetControlColors()
    frmOutput_PositionWindows()
    ' Refresh the menubar. The rest of the GUI will refresh via frmMain_PositionWindows
    AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )
    frmMenuBar_PositionWindows()

    ' Reposition main windows in case hide toolbar/statusbar selected
    frmMain_PositionWindows

    ' Apply the newly saved options to all open Scintilla windows
    dim pDoc as clsDocument Ptr = gApp.pDocList
    do until pDoc = 0
        pDoc->ApplyProperties
        pDoc = pDoc->pDocNext
    loop

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


' ========================================================================================
' Keep the dialog usable when dragged small.
' ========================================================================================
private function frmOptions_OnGetMinMaxInfo( byval hwnd as HWND, byval pmmi as MINMAXINFO ptr ) as LRESULT
    if pmmi = 0 then return 0
    ' The stored minimum is a CLIENT size; grow it by the frame so the constraint applies
    ' to the window rect Windows actually checks.
    dim as RECT rc
    rc.left = 0: rc.top = 0
    rc.right  = AfxScaleX(FRMOPTIONS_MIN_W)
    rc.bottom = AfxScaleY(FRMOPTIONS_MIN_H)
    AdjustWindowRectEx( @rc, GetWindowLongPtr(hwnd, GWL_STYLE), false, GetWindowLongPtr(hwnd, GWL_EXSTYLE) )
    pmmi->ptMinTrackSize.x = rc.right - rc.left
    pmmi->ptMinTrackSize.y = rc.bottom - rc.top
    function = 0
end function


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
            DisableAllModeless()
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

        case WM_GETMINMAXINFO
            return frmOptions_OnGetMinMaxInfo( hwnd, cast(MINMAXINFO ptr, lParam) )

        case WM_COMMAND
            return frmOptions_OnCommand( hwnd, loword(wParam), cast(HWND, lParam), hiword(wParam) )

        case WM_CLOSE
            ' Enables parent window keeping parent's zorder
            EnableAllModeless()
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
                                  byval nIconKind as long, _
                                  byval nPreset as long ) as long

    dim as HWND h = PsMessageBox_Create( HWND_FRMOPTIONS )
    PsMessageBox_SetCaption( h, wszCaption )
    PsMessageBox_SetText( h, wszText )
    PsMessageBox_SetIconKind( h, nIconKind )
    PsMessageBox_SetFont( h, OptionsFont_Base() )
    PsMessageBox_SetCloseGlyphFont( h, ghFont(SYMBOLFONT_10) )

    ' A larger icon glyph than any cached ghFont, made for this box and freed after DoModal.
    dim as HFONT hGlyph = 0
    dim pWin as CWindow ptr = AfxCWindowPtr( HWND_FRMOPTIONS )
    if pWin then hGlyph = pWin->CreateFont( SYMBOLFONT, 22 )
    if hGlyph then PsMessageBox_SetGlyphFont( h, hGlyph )

    ' Colours from the theme. Border/divider are the editor divider; the bands are the
    ' panel; the close-hot red and the icon accents keep their semantic defaults.
    dim as MBX_COLORS mc
    mc.BorderColor      = ghEditor.Divider
    mc.CaptionBackColor = ghPanel.BackColor
    mc.CaptionForeColor = ghPanel.ForeColorHot
    mc.CloseBackColor   = ghPanel.BackColor
    mc.CloseForeColor   = ghPanel.ForeColor
    mc.BackColor        = ghPanel.BackColor
    mc.ForeColor        = ghPanel.ForeColor
    mc.FooterBackColor  = ghPanel.BackColor
    mc.DividerColor     = ghEditor.Divider
    PsMessageBox_SetColors( h, @mc )

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    PsMessageBox_SetButtonColors( h, @bc )

    if nPreset = MBX_BTN_OKCANCEL then
        PsMessageBox_AddButton( h, L(0,"OK"), IDOK )
        PsMessageBox_AddButton( h, L(1,"Cancel"), IDCANCEL )
        ' Default to Cancel: this box only ever guards a destructive action.
        PsMessageBox_SetDefaultButton( h, 1 )
    else
        PsMessageBox_AddButton( h, L(0,"OK"), IDOK )
    end if

    dim as long nResult = PsMessageBox_DoModal( h )
    if hGlyph then DeleteObject( hGlyph )
    function = nResult
end function


' ========================================================================================
' Theme the shell's own controls.
' ========================================================================================
private sub frmOptions_ApplyTheme( byval hwnd as HWND )

    PsListBox_SetBackColor( HWND_FRMOPTIONS_NAV, ghPanel.BackColor )
    PsListBox_SetScrollBarColors( HWND_FRMOPTIONS_NAV, ghPanel.BackColorScrollBar, _
                                 ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )

    PsScrollPanel_SetColors( HWND_FRMOPTIONS_SCROLL, ghPanel.BackColor, ghPanel.ForeColor )
    PsScrollPanel_SetScrollBarColors( HWND_FRMOPTIONS_SCROLL, ghPanel.BackColorScrollBar, _
                                     ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )

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

    ' Match the caption to the theme, the same test LoadThemeFile() uses for frmMain.
    dim as long nDark = iif( instr(gConfig.ThemeShortFilename, "_dark") > 0, true, false )
    DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, @nDark, sizeof(nDark) )

end sub


' ========================================================================================
' Populate the grouped navigation list. Item rows carry their page id as itemData.
' ========================================================================================
private sub frmOptions_BuildNavList( byval hList as HWND )

    PsListBox_BeginUpdate( hList )
    PsListBox_Clear( hList )

    ' FLAT -- no PsListBox_AddHeader calls anywhere. Eight pages read fine as one list, and
    ' merging Advanced Code Editor into Code Editor had left three of the five groups
    ' holding a single item each.
    PsListBox_AddString( hList, L(226,"General Options"),      OPTPAGE_GENERAL )
    PsListBox_AddString( hList, L(110,"Code Editor"),          OPTPAGE_EDITOR )
    PsListBox_AddString( hList, L(111,"Themes and Fonts"),     OPTPAGE_COLORS )
    PsListBox_AddString( hList, L(112,"Compiler Setup"),       OPTPAGE_COMPILER )
    PsListBox_AddString( hList, L(129,"Localization"),         OPTPAGE_LOCAL )
    PsListBox_AddString( hList, L(177,"FreeBASIC Keywords"),   OPTPAGE_KEYWORDS )
    PsListBox_AddString( hList, L(435,"Windows Api Keywords"), OPTPAGE_KEYWORDSWINAPI )
    PsListBox_AddString( hList, L(439,"Extra Keywords"),       OPTPAGE_KEYWORDSEXTRA )

    PsListBox_EndUpdate( hList )

end sub


' ========================================================================================
' Find the navigation row that carries a page id, or -1.
' ========================================================================================
private function frmOptions_NavRowForPage( byval hList as HWND, byval nPage as long ) as long
    for i as long = 0 to PsListBox_GetCount(hList) - 1
        if PsListBox_GetItemData( hList, i ) = nPage then return i
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

    HWND_FRMOPTIONS = pWindow->Create( hWndParent, L(113,"Environment Options"), @frmOptions_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_THICKFRAME or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT or WS_EX_LEFT )
    pWindow->SetClientSize( FRMOPTIONS_CLIENT_W, FRMOPTIONS_CLIENT_H )
    pWindow->Center( pWindow->hWindow, hWndParent )

    ' ---- navigation list -------------------------------------------------------------
    HWND_FRMOPTIONS_NAV = PsListBox_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_NAVLIST )
    PsListBox_SetPaintCallback( HWND_FRMOPTIONS_NAV, @frmOptions_NavPaintCallback )
    PsListBox_SetSelChangeCallback( HWND_FRMOPTIONS_NAV, @frmOptions_NavSelChangeCallback )
    PsListBox_SetFont( HWND_FRMOPTIONS_NAV, OptionsFont_Base() )
    ' Row height is in UNSCALED units (PsListBox scales what it is handed) and has to grow
    ' with the base font, or a 12pt list clips its own rows.
    PsListBox_SetRowHeight( HWND_FRMOPTIONS_NAV, 26 + ((OptionsFont_PointSize() - 10) * 2) )
    frmOptions_BuildNavList( HWND_FRMOPTIONS_NAV )

    ' ---- scrolling page area ---------------------------------------------------------
    HWND_FRMOPTIONS_SCROLL = PsScrollPanel_Create( HWND_FRMOPTIONS, IDC_FRMOPTIONS_SCROLLPANEL )
    PsScrollPanel_SetPaintCallback( HWND_FRMOPTIONS_SCROLL, @frmOptions_PagePaintCallback )
    PsScrollPanel_SetLayoutCallback( HWND_FRMOPTIONS_SCROLL, @frmOptions_PageLayoutCallback )
    PsScrollPanel_SetScrollBarWidth( HWND_FRMOPTIONS_SCROLL, SCROLLBAR_WIDTH_PANEL )
    ' Auto-scroll-to-focus is OFF, deliberately. It fires on the reveal timer when a control
    ' takes focus -- including a PsComboBox taking focus on its button-DOWN. For a combo whose
    ' body sits partly below the fold, that scroll slides the button up and out from under the
    ' still-held cursor onto the just-opened dropdown, and the release then selects a row and
    ' closes the list. The cost of turning it off is that Tab-ing to a control below the fold
    ' no longer auto-reveals it; on a dialog this size that is a fair trade for usable combos.
    PsScrollPanel_SetAutoScrollToFocus( HWND_FRMOPTIONS_SCROLL, false )

    dim as HWND hPage = PsScrollPanel_GetPanel( HWND_FRMOPTIONS_SCROLL )

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
    ShowWindow( HWND_FRMOPTIONS_SCROLL, SW_SHOW )

    ' No pages are created here any more -- every page (table-driven and panel-hosted) is
    ' built lazily on its first visit.

    frmOptions_ApplyTheme( HWND_FRMOPTIONS )
    frmOptions_PositionWindows( HWND_FRMOPTIONS )

    ' Restore the most recently viewed page, defaulting to the first.
    dim as long nStartPage = OptionsDialogLastOpened
    if (nStartPage < 0) orelse (nStartPage >= OPTPAGE_COUNT) then nStartPage = OPTPAGE_GENERAL
    frmOptions_ShowPage( nStartPage )

    dim as long nNavRow = frmOptions_NavRowForPage( HWND_FRMOPTIONS_NAV, nStartPage )
    if nNavRow >= 0 then PsListBox_SetCurSel( HWND_FRMOPTIONS_NAV, nNavRow )

    OptionsRows_RunSelfTest( hPage )

    ShowWindow( HWND_FRMOPTIONS, SW_SHOW )
    UpdateWindow( HWND_FRMOPTIONS )

    ' IsDialogMessage ignores Tab until a child already holds the focus, so the first
    ' control has to be focused explicitly -- an ordinary CWindow gets no dialog manager
    ' to do it. See the WS_EX_CONTROLPARENT section of Learnings.md.
    SetFocus( HWND_FRMOPTIONS_NAV )

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

    ' delete the frmOptions CWindow class manually allocated memory
    if pWindow then delete(pWindow)

    ' No child forms to delete: every page is panel-hosted or table-driven, and all of
    ' those controls sit on the scroll panel's page and self-free when it is destroyed.

    HWND_FRMOPTIONS_NAV    = 0
    HWND_FRMOPTIONS_SCROLL = 0
    OptionsRows_ResetCombos()
    erase gOptRows

    function = 0
end function
