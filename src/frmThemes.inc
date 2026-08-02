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

'  frmThemes.inc  --  the Themes dialog shell. See frmThemes.bi for the design.
'
'  TWO MUTUALLY EXCLUSIVE VIEWS OF ONE WINDOW, exactly the frmOptionsLocal pattern:
'
'    SELECT  (gThemesShowEdit = false) -- a 3-column themes list (Theme / Description / Active)
'            with Edit / Clone / Delete / Set-as-active stacked beside it, and Close in the footer.
'            The list previews LIVE: arrowing through it recolours the application behind the
'            dialog. Only "Set as active" commits gConfig.ThemeShortFilename.
'
'    EDIT    (gThemesShowEdit = true) -- the Syntax|Interface bar + the colour page (in
'            frmThemesPage.inc) + a Back button. The list is hidden; the editor takes the full
'            width. Back returns to SELECT.
'
'  The COLOUR PAGE itself lives in frmThemesPage.inc, included at the bottom.

#include once "frmThemes.bi"
#include once "frmInputBox.bi"     ' frmInputBox_Show -- the description edit dialog (its .inc is
                                  ' included after us in tiko.bas, so we need its declarations here)

dim shared ghThemesList      as HWND     ' PsListTree -- the themes folder, 3 columns
dim shared ghThemesBar       as HWND     ' PsSelectBar -- Syntax | Interface (EDIT view)
dim shared ghThemesEdit      as HWND     ' SELECT view buttons
dim shared ghThemesClone     as HWND
dim shared ghThemesDelete    as HWND
dim shared ghThemesSetActive as HWND
dim shared ghThemesClose     as HWND
dim shared ghThemesBack      as HWND     ' EDIT view -- header, returns to SELECT
dim shared ghThemesSave      as HWND     ' EDIT view -- footer, writes the .theme file
dim shared ghThemesRevert    as HWND     ' EDIT view -- footer, reloads from disk
dim shared ghThemesDescEdit  as HWND     ' EDIT view -- header, edit-description icon button
dim shared ghThemesDescTip   as HWND     ' themed PsTooltip on the edit-description button
dim shared ghThemesPageBrush as HBRUSH
dim shared ghThemesHeadFont  as HFONT    ' larger bold face for the "Themes" heading word

dim shared gThemesShowEdit   as boolean = false
dim shared gThemesCurPage    as long = THEMEPAGE_NONE
dim shared gThemesPageBuilt(THEMEPAGE_COUNT - 1) as boolean

' Forward -- the preview/commit handlers below re-push the (now different) theme's colours into
' the dialog's own controls, and ApplyShellTheme is defined further down.
declare sub frmThemes_ApplyShellTheme()


' ========================================================================================
' A PsMessageBox dressed in the current theme. The colour page's fork prompt and the delete
' confirmation both use it. Kept as its own forwarder rather than calling frmOptions' --
' HWND_FRMOPTIONS is 0 when this dialog is opened on its own.
'
' THE DEFAULT BUTTON WAS BROKEN HERE AND SILENTLY SO. Both branches passed an ID to
' PsMessageBox_SetDefaultButton, which takes an INDEX and rejects out-of-range values by
' exiting quietly -- so "default to Cancel" never took effect. TikoMsgBox takes the index.
' ========================================================================================
function frmThemes_ThemedMsgBox( byval wszText as DWSTRING, _
                                 byval wszCaption as DWSTRING, _
                                 byval nIcon as long, _
                                 byval nButtons as long ) as long

    dim as long nDefault = 0
    if nButtons = TMB_OKCANCEL then nDefault = 1     ' Cancel: it guards a destructive action

    return TikoMsgBox( HWND_FRMTHEMES, wszText, wszCaption, _
                       nIcon, nButtons, nDefault, OptionsFont_Base() )
end function


' ========================================================================================
' The Back-with-unsaved-edits prompt: Save / Discard / Cancel. Returns IDYES (save),
' IDNO (discard) or IDCANCEL (stay).
'
' Same silent default-button bug as above -- it passed IDYES (6) as an index into three
' buttons, so Save was never actually the default.
' ========================================================================================
private function frmThemes_AskSaveChanges() as long
    return TikoMsgBox( HWND_FRMTHEMES, _
                       L(394,"Save changes to this theme before leaving?"), _
                       L(329,"Themes"), _
                       TMB_ICON_QUESTION, TMB_SAVE_DISCARD_CANCEL, 0, OptionsFont_Base() )
end function


' ========================================================================================
' The themes folder.
' ========================================================================================
private function frmThemes_ThemesFolder() as DWSTRING
    return AfxGetExePathName & "settings\themes\"
end function


' Pixel width of a run of text in a given font -- used to flow the multi-colour heading
' segments one after the other. The DC is the caller's window DC; the font matches what the
' buffer will actually paint with, so the measured advance and the drawn glyphs agree.
private function frmThemes_TextW( byval hdc as HDC, byval hFont as HFONT, byval wsz as DWSTRING ) as long
    dim as HGDIOBJ hOld = SelectObject( hdc, hFont )
    dim as SIZE sz
    GetTextExtentPoint32W( hdc, wsz, len(wsz), @sz )
    SelectObject( hdc, hOld )
    return sz.cx
end function


' Blend c1 toward c2 by weight pct (weight of c1, 0..100). Used to dim the header's secondary
' text a notch below theme.panel.text.forecolor without inventing a new theme key.
private function frmThemes_Blend( byval c1 as COLORREF, byval c2 as COLORREF, byval pct as long ) as COLORREF
    dim as long r = (GetRValue(c1) * pct + GetRValue(c2) * (100 - pct)) \ 100
    dim as long g = (GetGValue(c1) * pct + GetGValue(c2) * (100 - pct)) \ 100
    dim as long b = (GetBValue(c1) * pct + GetBValue(c2) * (100 - pct)) \ 100
    return RGB( r, g, b )
end function


' ========================================================================================
' The invariant: at least one theme must always exist. If the folder has emptied (the user
' deleted the last theme), reseed default_dark.theme from the shipped backup. Called after
' every delete, before the list is reloaded.
' ========================================================================================
private sub frmThemes_EnsureThemeExists()
    dim as DWSTRING wszFolder = frmThemes_ThemesFolder()
    dim as long nCount = 0
    dim hSearch as HANDLE
    dim WFD as WIN32_FIND_DATAW

    hSearch = FindFirstFile( wszFolder & "*.theme", @WFD )
    if hSearch <> INVALID_HANDLE_VALUE then
        do
            if (WFD.cFileName <> ".") andalso (WFD.cFileName <> "..") then nCount += 1
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    if nCount = 0 then
        AfxCopyFile( AfxGetExePathName & "settings\backup_default_dark.theme", _
                     wszFolder & "default_dark.theme", false )
    end if
end sub


' ========================================================================================
' Fill the 3-column list from the themes folder. gThemeFilenames is the PARALLEL array of
' short filenames; a row index means the same thing in it. Columns are added once in _Show;
' this only clears and refills the rows.
'
'   THEMECOL_NAME   the filename without its .theme extension (the theme's stable identity)
'   THEMECOL_DESC   general.description from inside the file (the human-readable text)
'   THEMECOL_ACTIVE the "Active" marker, written by frmThemes_MarkActiveRow
'
' Returns the row index of the currently ACTIVE theme (gConfig.ThemeShortFilename), or -1.
' ========================================================================================
private function frmThemes_LoadThemes() as long
    if ghThemesList = 0 then return -1

    PsListTree_BeginUpdate( ghThemesList )
    PsListTree_Clear( ghThemesList )
    erase gThemeFilenames

    dim as long idxActive = -1
    dim as long idx = 0
    dim hSearch as HANDLE
    dim WFD as WIN32_FIND_DATAW
    dim as DWSTRING wszPath = frmThemes_ThemesFolder()

    hSearch = FindFirstFile( wszPath & "*.theme", @WFD )
    if hSearch <> INVALID_HANDLE_VALUE then
        do
            if (WFD.cFileName <> ".") andalso (WFD.cFileName <> "..") then
                dim as DWSTRING wszFull = wszPath & WFD.cFileName
                dim as DWSTRING wszDesc = GetThemeDescription( wszFull )

                dim as string sBase = WFD.cFileName
                if lcase(right(sBase, 6)) = ".theme" then sBase = left(sBase, len(sBase) - 6)
                dim as DWSTRING wszRow = sBase & chr(9) & wszDesc

                idx = ListBox_AddTabbedRow( ghThemesList, wszRow )
                redim preserve gThemeFilenames(idx) as DWSTRING
                gThemeFilenames(idx) = WFD.cFileName
                if ucase(gConfig.ThemeShortFilename) = ucase(WFD.cFileName) then idxActive = idx
            end if
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    PsListTree_EndUpdate( ghThemesList )
    function = idxActive
end function


' ========================================================================================
' Write the "Active" marker onto the row whose file IS the active theme, and clear it from
' every other row. The active theme (gConfig.ThemeShortFilename) is distinct from the
' selected/previewed row -- browsing the list previews without activating. Copied from
' frmOptionsLocal_MarkActiveRow, including the BeginUpdate/EndUpdate discipline: SetCellText
' writes the model without repainting, so EndUpdate's Refresh is what puts it on screen.
' ========================================================================================
private sub frmThemes_MarkActiveRow()
    if ghThemesList = 0 then exit sub
    PsListTree_BeginUpdate( ghThemesList )
    for i as long = lbound(gThemeFilenames) to ubound(gThemeFilenames)
        dim as boolean bActive = (ucase(gThemeFilenames(i)) = ucase(gConfig.ThemeShortFilename))
        dim as DWSTRING wszMark = ""
        if bActive then wszMark = L(335,"Active")
        PsListTree_SetCellText( ghThemesList, i, THEMECOL_ACTIVE, wszMark )
    next
    PsListTree_EndUpdate( ghThemesList )
end sub


' ========================================================================================
' Reload the folder, mark the active row, and select gThemesWorkFile (falling back to the
' active theme, then the first row). SILENT -- SetCurSel never fires SelChange, so this does
' NOT preview; callers preview when they want to.
' ========================================================================================
' Edit and Delete are DISABLED for the shipped default_* themes -- a built-in cannot be modified
' or removed; the user must Clone it first. Clone and Set-as-active stay enabled. Called on every
' selection change and list refresh.
private sub frmThemes_UpdateSelButtons()
    if ghThemesList = 0 then exit sub
    dim as long sel = PsListTree_GetCurSel( ghThemesList )
    dim as boolean bProtected = false
    if (sel >= lbound(gThemeFilenames)) andalso (sel <= ubound(gThemeFilenames)) then
        bProtected = frmThemes_IsProtectedTheme( gThemeFilenames(sel) )
    end if
    if ghThemesEdit   then EnableWindow( ghThemesEdit,   iif( bProtected, false, true ) )
    if ghThemesDelete then EnableWindow( ghThemesDelete, iif( bProtected, false, true ) )
end sub


sub frmThemes_RefreshThemeList()
    if ghThemesList = 0 then exit sub

    dim as long idxActive = frmThemes_LoadThemes()
    frmThemes_MarkActiveRow()

    dim as long sel = -1
    for i as long = lbound(gThemeFilenames) to ubound(gThemeFilenames)
        if ucase(gThemeFilenames(i)) = ucase(gThemesWorkFile) then sel = i : exit for
    next
    if sel < 0 then sel = idxActive
    if (sel < 0) andalso (ubound(gThemeFilenames) >= 0) then sel = 0

    if sel >= 0 then
        PsListTree_SetCurSel( ghThemesList, sel )
        PsListTree_SetTopIndex( ghThemesList, sel )
        gThemesWorkFile = gThemeFilenames(sel)
    end if
    frmThemes_UpdateSelButtons()
    AfxRedrawWindow( ghThemesList )
end sub


' ========================================================================================
' List painters and callbacks.
' ========================================================================================
private function frmThemes_NoTooltip( byval hList as HWND, byval row as integer, byval col as integer ) as DWSTRING
    return ""
end function

private function frmThemes_NoHeaderTooltip( byval hHeader as HWND, byval idx as integer ) as DWSTRING
    return ""
end function


private sub frmThemes_ThemeListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    ' NO HOT-TRACKING: only the selected row lights, not the hovered one. A themes list is a
    ' short pick list, not a browsing surface, and a highlight chasing the cursor over it reads
    ' as noise. (isHot is deliberately ignored.)
    dim as COLORREF backclr, foreclr
    if p->isSelected then
        backclr = theme.panel.texthot.backcolor
        foreclr = theme.panel.texthot.forecolor
    else
        backclr = theme.panel.text.backcolor
        foreclr = theme.panel.text.forecolor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )

    if p->columnCount <= 0 then
        dim as RECT rcText = p->rc
        rcText.left += AfxScaleX(12)
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
        exit sub
    end if

    for i as integer = 0 to p->columnCount - 1
        dim as RECT rcText = p->cells[i].rc
        rcText.left += AfxScaleX(12)
        ' The Active marker column is centred; the two text columns are left-aligned.
        dim as long nFmt = DT_LEFT or DT_END_ELLIPSIS
        if i = THEMECOL_ACTIVE then nFmt = DT_CENTER or DT_END_ELLIPSIS
        p->b->PaintText( p->cells[i].wszText, @rcText, nFmt )
    next
end sub


private sub frmThemes_HeaderPaint( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    if p = 0 then exit sub
    ' A DISTINCT DARK header band: darker than the list rows (which paint theme.panel.text.backcolor), with
    ' the lighter theme.panel.text.forecolor text on top so the column titles read clearly.
    dim as COLORREF backclr = frmThemes_Blend( theme.panel.text.backcolor, RGB(0,0,0), 72 )
    if p->isHot orelse p->isResizeHot then backclr = theme.panel.text.backcolor
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( theme.panel.text.forecolor )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(12)
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )

    dim as RECT rcDiv = p->rc
    rcDiv.left = rcDiv.right - AfxScaleX(1)
    p->b->SetBackColor( theme.ui.divider )
    p->b->PaintRect( @rcDiv )
end sub


' PREVIEW FOR REAL: arrowing/clicking a row recolours the whole editor behind the dialog.
' gConfig is NOT touched -- the pick stays in gThemesWorkFile; Close restores the snapshot,
' and only "Set as active" commits.
private sub frmThemes_ThemeListSelChange( byval hList as HWND, byval row as integer )
    if (row < lbound(gThemeFilenames)) orelse (row > ubound(gThemeFilenames)) then exit sub
    gThemesWorkFile = gThemeFilenames(row)
    Theme_PreviewFile( gThemeFilenames(row) )
    Theme_ApplyAll()
    frmThemes_ApplyShellTheme()   ' previewed theme has different colours -- re-theme our own controls too
    frmThemes_UpdateSelButtons()  ' Edit/Delete off for a built-in default_* theme
    AfxRedrawWindow( HWND_FRMTHEMES )
end sub


' ENTER or double-click on a row = Edit, exactly as the localization language list does.
private sub frmThemes_ThemeListActivate( byval hList as HWND, byval row as integer )
    if ghThemesEdit then PsButton_Click( ghThemesEdit )
end sub


' ========================================================================================
' The colour page container (EDIT view). Unchanged from before the two-view split: the page
' needs its own coordinate space, and the four services a page expects -- a themed background
' via SCP_PAINTINFO, WM_CTLCOLOR*, forwarding of unclaimed commands, and WS_EX_CONTROLPARENT.
' ========================================================================================
private function frmThemes_PageWndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    case WM_ERASEBKGND
        return true

    case WM_LBUTTONDOWN
        ' The Fore/Back colour swatches are painted on the page (not child controls), so their
        ' clicks arrive here. If it hit one, open the colour popup for that channel.
        if frmThemes_SwatchClick( cast(short, loword(lParam)), cast(short, hiword(lParam)) ) then return 0

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )

        dim p as SCP_PAINTINFO
        p.hScrollPanel = hwnd
        p.hPanel       = hwnd
        p.b            = @b
        GetClientRect( hwnd, @p.rcClient )
        p.nPos         = 0

        b.SetBackColor( theme.panel.text.backcolor )
        b.PaintRect( @p.rcClient )
        if gThemesCurPage <> THEMEPAGE_NONE then frmThemes_Paint( gThemesCurPage, @p )

        b.EndDoubleBuffer()
        return 0

    case WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORLISTBOX
        SetBkColor( cast(HDC, wParam), theme.panel.text.backcolor )
        SetTextColor( cast(HDC, wParam), theme.panel.text.forecolor )
        if ghThemesPageBrush = 0 then ghThemesPageBrush = CreateSolidBrush( theme.panel.text.backcolor )
        return cast( LRESULT, ghThemesPageBrush )

    case WM_COMMAND, WM_NOTIFY, WM_HSCROLL, WM_VSCROLL
        return SendMessageW( GetParent(hwnd), uMsg, wParam, lParam )

    case WM_NCDESTROY
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Switch the visible colour page (Syntax / Interface). Built on first visit.
' ========================================================================================
private sub frmThemes_SelectPage( byval nPage as long )

    if (nPage < 0) orelse (nPage >= THEMEPAGE_COUNT) then exit sub
    if nPage = gThemesCurPage then exit sub
    if HWND_FRMTHEMES_PAGE = 0 then exit sub

    if gThemesCurPage <> THEMEPAGE_NONE then frmThemes_ShowPage( gThemesCurPage, false )

    gThemesCurPage = nPage

    if gThemesPageBuilt(nPage) = false then
        frmThemes_Build( nPage, HWND_FRMTHEMES_PAGE )
        gThemesPageBuilt(nPage) = true
    end if
    frmThemes_ShowPage( nPage, true )

    dim as RECT rc
    GetClientRect( HWND_FRMTHEMES_PAGE, @rc )
    frmThemes_Layout( nPage, HWND_FRMTHEMES_PAGE, rc.right - rc.left )

    AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
end sub


private sub frmThemes_BarSelChange( byval hBar as HWND, byval idxOld as long, byval idxNew as long )
    frmThemes_SelectPage( idxNew )
end sub


' ========================================================================================
' Show one view's controls and hide the other's. gThemesShowEdit is the switch.
' ========================================================================================
private sub frmThemes_ApplyViewVisibility()
    dim as long nSel  = iif( gThemesShowEdit, SW_HIDE, SW_SHOW )
    dim as long nEdit = iif( gThemesShowEdit, SW_SHOW, SW_HIDE )

    if ghThemesList      then ShowWindow( ghThemesList,      nSel )
    if ghThemesEdit      then ShowWindow( ghThemesEdit,      nSel )
    if ghThemesClone     then ShowWindow( ghThemesClone,     nSel )
    if ghThemesDelete    then ShowWindow( ghThemesDelete,    nSel )
    if ghThemesSetActive then ShowWindow( ghThemesSetActive, nSel )
    if ghThemesClose     then ShowWindow( ghThemesClose,     nSel )

    if ghThemesBar        then ShowWindow( ghThemesBar,        nEdit )
    if HWND_FRMTHEMES_PAGE then ShowWindow( HWND_FRMTHEMES_PAGE, nEdit )
    if ghThemesBack       then ShowWindow( ghThemesBack,       nEdit )
    if ghThemesSave       then ShowWindow( ghThemesSave,       nEdit )
    if ghThemesRevert     then ShowWindow( ghThemesRevert,     nEdit )
    if ghThemesDescEdit   then ShowWindow( ghThemesDescEdit,   nEdit )
end sub


' ========================================================================================
' Position the children of BOTH views (the hidden set is positioned too -- cheaper than
' re-laying out on every view switch, and correct because visibility is separate).
' ========================================================================================
private sub frmThemes_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long margin  = AfxScaleX(FRMTHEMES_MARGIN)
    dim as long footerH = AfxScaleY(FRMTHEMES_FOOTER_H)
    dim as long barH    = AfxScaleY(FRMTHEMES_BAR_H)
    dim as long btnW    = AfxScaleX(FRMTHEMES_BTN_W)
    dim as long btnH    = AfxScaleY(FRMTHEMES_BTN_H)
    dim as long selBtnW = AfxScaleX(FRMTHEMES_SELBTN_W)
    dim as long selBtnH = AfxScaleY(FRMTHEMES_SELBTN_H)
    dim as long selGap  = AfxScaleY(FRMTHEMES_SELBTN_GAP)

    dim as long bodyBottom = rc.bottom - footerH
    if bodyBottom < 0 then bodyBottom = 0
    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2

    ' ---- SELECT view --------------------------------------------------------------------
    ' The list and the button column start below the drawn header band (heading + instruction),
    ' painted in WM_PAINT.
    dim as long top    = AfxScaleY(FRMTHEMES_HEADER_H)
    dim as long listW  = rc.right - margin - selBtnW - margin - margin   ' left margin, gap, button col
    if listW < 0 then listW = 0
    dim as long listH  = bodyBottom - top - margin
    if listH < 0 then listH = 0
    SetWindowPos( ghThemesList, 0, margin, top, listW, listH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long colX = rc.right - margin - selBtnW
    dim as long y    = top
    SetWindowPos( ghThemesEdit,   0, colX, y, selBtnW, selBtnH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += selBtnH + selGap
    SetWindowPos( ghThemesClone,  0, colX, y, selBtnW, selBtnH, SWP_NOZORDER or SWP_NOACTIVATE )
    y += selBtnH + selGap
    SetWindowPos( ghThemesDelete, 0, colX, y, selBtnW, selBtnH, SWP_NOZORDER or SWP_NOACTIVATE )
    ' "Set as active" is separated from the destructive/edit trio by a wider gap.
    y += selBtnH + selGap + selGap
    SetWindowPos( ghThemesSetActive, 0, colX, y, selBtnW, selBtnH, SWP_NOZORDER or SWP_NOACTIVATE )

    ' Close, footer right -- height matches the SELECT-view action buttons, centred in the footer.
    dim as long closeY = rc.bottom - footerH + (footerH - selBtnH) \ 2
    SetWindowPos( ghThemesClose, 0, rc.right - margin - btnW, closeY, btnW, selBtnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    ' ---- EDIT view ----------------------------------------------------------------------
    dim as long editHdrH = AfxScaleY(FRMTHEMES_EDITHDR_H)

    ' Header is a left-aligned stack whose left edge matches the element list below (padLeft in
    ' the page, which sits at x=0): the "Edit theme" title (DRAWN in WM_PAINT), the description
    ' field, then the Back button under it.
    ' "Edit theme" title and the description text are DRAWN in WM_PAINT. The edit-description
    ' icon sits just after the description text (measured), and the Back button below, with
    ' breathing room -- all left-aligned to the element list.
    dim as long listLeftX = AfxScaleX(OPTROW_PADLEFT)

    dim as HDC hdcM = GetDC( hwnd )
    dim as long descW = frmThemes_TextW( hdcM, ghFont(GUIFONT_11), theme.general.description )
    ReleaseDC( hwnd, hdcM )
    dim as long editW = AfxScaleX(28)
    dim as long editH = AfxScaleY(26)
    SetWindowPos( ghThemesDescEdit, 0, listLeftX + descW + AfxScaleX(10), AfxScaleY(38), _
                  editW, editH, SWP_NOZORDER or SWP_NOACTIVATE )

    SetWindowPos( ghThemesBack, 0, listLeftX, AfxScaleY(84), btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    ' Bar below the header, its LEFT edge aligned to the Back button (both at listLeftX), nudged
    ' down a touch; page below the bar.
    dim as long barY = editHdrH + AfxScaleY(12)
    SetWindowPos( ghThemesBar, 0, listLeftX, barY, rc.right - listLeftX, barH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    dim as long pageTop = barY + barH
    dim as long pageH   = bodyBottom - pageTop
    if pageH < 0 then pageH = 0
    SetWindowPos( HWND_FRMTHEMES_PAGE, 0, 0, pageTop, rc.right, pageH, SWP_NOZORDER or SWP_NOACTIVATE )
    if gThemesCurPage <> THEMEPAGE_NONE then
        frmThemes_Layout( gThemesCurPage, HWND_FRMTHEMES_PAGE, rc.right )
    end if

    ' Footer: Revert then Save on the right.
    SetWindowPos( ghThemesSave, 0, rc.right - margin - btnW, btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemesRevert, 0, rc.right - margin - (btnW * 2) - AfxScaleX(8), btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
end sub


' ========================================================================================
' Push the current theme's colours into the shell's own controls.
' ========================================================================================
' Theme the edit-description PsTooltip from the current dialog theme (frmUserTools' recipe).
private sub frmThemes_ThemeDescTip()
    if ghThemesDescTip = 0 then exit sub
    dim as PSTOOLTIP_COLORS tipc
    Theme_FillTooltipColors( tipc )
    PsTooltip_SetColors( ghThemesDescTip, @tipc )
end sub


private sub frmThemes_ApplyShellTheme()

    ' Caption bar follows the theme's light/dark-ness too (Theme_ApplyDarkCaption reads
    ' Theme_IsDark), so a light preview gets a light title bar rather than a stale dark one.
    if HWND_FRMTHEMES then Theme_ApplyDarkCaption( HWND_FRMTHEMES )

    if ghThemesPageBrush then DeleteObject( ghThemesPageBrush )
    ghThemesPageBrush = CreateSolidBrush( theme.panel.text.backcolor )

    if ghThemesList then
        PsListTree_SetBackColor( ghThemesList, theme.panel.text.backcolor )
        PsListTree_SetFont( ghThemesList, OptionsFont_Base() )
    end if

    if ghThemesBar then
        dim as PSSELECTBAR_COLORS sc
        sc.BackColor        = theme.panel.text.backcolor
        sc.BackColorHot     = theme.panel.text.backcolor
        sc.BackColorSelect  = theme.panel.text.backcolor
        sc.ForeColor        = theme.panel.text.forecolor
        sc.ForeColorHot     = theme.panel.texthot.forecolor
        sc.ForeColorSelect  = theme.panel.texthot.forecolor
        sc.IndicatorColor   = theme.ui.accent
        PsSelectBar_SetColors( ghThemesBar, @sc )
        PsSelectBar_SetFont( ghThemesBar, OptionsFont_Base() )
        PsSelectBar_Refresh( ghThemesBar )
    end if

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    dim hButtons(0 to 8) as HWND = { ghThemesEdit, ghThemesClone, ghThemesDelete, _
                                     ghThemesSetActive, ghThemesClose, ghThemesBack, _
                                     ghThemesSave, ghThemesRevert, ghThemesDescEdit }
    for i as long = lbound(hButtons) to ubound(hButtons)
        if hButtons(i) then
            PsButton_SetColors( hButtons(i), @bc )
            PsButton_SetFont( hButtons(i), OptionsFont_Base() )
            PsButton_Refresh( hButtons(i) )
        end if
    next
    ' The edit-description button paints its pencil GLYPH as a centred caption, so it wants the
    ' symbol font rather than the base UI font the loop just gave it.
    if ghThemesDescEdit then
        PsButton_SetFont( ghThemesDescEdit, ghFont(SYMBOLFONT_10) )
        PsButton_Refresh( ghThemesDescEdit )
    end if
    frmThemes_ThemeDescTip()   ' keep the tooltip in step with a live theme change

    frmThemes_ApplyTheme()
end sub


' ========================================================================================
' SELECT-view button handlers.
' ========================================================================================

' Edit: enter the colour editor for the selected theme. Does NOT activate it.
private sub frmThemes_EditClick( byval hButton as HWND, byval id as long )
    dim as long sel = PsListTree_GetCurSel( ghThemesList )
    if (sel < lbound(gThemeFilenames)) orelse (sel > ubound(gThemeFilenames)) then exit sub
    ' A built-in default_* theme cannot be edited -- Clone it first. The button is disabled for
    ' those, but ENTER / double-click routes through here via PsButton_Click, so guard again.
    if frmThemes_IsProtectedTheme( gThemeFilenames(sel) ) then exit sub
    gThemesWorkFile = gThemeFilenames(sel)

    ' Sync the description for the header display (Theme_PreviewFile does not refresh
    ' theme.general.description), and start a clean editing session.
    theme.general.description = GetThemeDescription( frmThemes_ThemesFolder() & gThemesWorkFile )
    frmThemes_ClearEdited()
    gThemesDescEdited = false

    gThemesShowEdit = true
    if gThemesCurPage = THEMEPAGE_NONE then
        PsSelectBar_SetCurSel( ghThemesBar, THEMEPAGE_SYNTAX )
        frmThemes_SelectPage( THEMEPAGE_SYNTAX )
    end if
    frmThemes_ApplyTheme()          ' refresh the page's swatches/picker for the previewed theme

    frmThemes_ApplyViewVisibility()
    frmThemes_PositionWindows( HWND_FRMTHEMES )
    AfxRedrawWindow( HWND_FRMTHEMES )
    SetFocus( ghThemesBar )
end sub


' Clone: Save-As for the new theme's disk filename, then duplicate the selected theme into it.
private sub frmThemes_CloneClick( byval hButton as HWND, byval id as long )
    dim as long sel = PsListTree_GetCurSel( ghThemesList )
    if (sel < lbound(gThemeFilenames)) orelse (sel > ubound(gThemeFilenames)) then exit sub

    dim as DWSTRING wszSrc    = gThemeFilenames(sel)
    dim as DWSTRING wszFolder = frmThemes_ThemesFolder()

    ' Prompt for the filename, pre-filled with a unique suggestion. The dialog defaults to the
    ' themes folder and the *.theme filter (modRoutines, keyed on IDC_THEMES_SAVEAS).
    dim wzFilename as wstring * MAX_PATH
    wzFilename = frmThemes_MakeForkName( wszSrc )
    dim pwszName as wstring ptr = AfxIFileSaveDialog( HWND_FRMTHEMES, @wzFilename, @wstr("theme"), IDC_THEMES_SAVEAS )
    if pwszName = 0 then exit sub                 ' cancelled

    ' A theme MUST live in settings\themes\, so take the chosen NAME (ignoring any folder the
    ' user browsed to) and force the .theme extension.
    dim as DWSTRING wszClone = AfxStrPathname( "NAMEX", *pwszName )
    CoTaskMemFree( pwszName )
    if lcase( AfxStrPathname( "EXTN", wszClone ) ) <> "theme" then
        wszClone = AfxStrPathname( "NAME", wszClone ) & ".theme"
    end if
    if ucase(wszClone) = ucase(wszSrc) then exit sub    ' would clobber the source

    dim as DWSTRING wszDest = wszFolder & wszClone

    ' bFailIfExists = false: the Save-As dialog already confirmed any overwrite.
    if AfxCopyFile( wszFolder & wszSrc, wszDest, false ) = false then
        frmThemes_ThemedMsgBox( L(299,"The new theme file could not be written."), _
                                L(329,"Themes"), TMB_ICON_ERROR, TMB_OK )
        exit sub
    end if

    gThemesWorkFile = wszClone

    ' Tag the copy's description with "(cloned)". Load it into gTheme (so Theme_WriteFile keeps
    ' the copied colours), overwrite the description, and write it back.
    dim as DWSTRING wszDesc = GetThemeDescription( wszDest ) & " " & L(396,"(cloned)")
    Theme_PreviewFile( wszClone )
    theme.general.description = wszDesc
    Theme_WriteFile( wszDest, wszDesc )

    frmThemes_RefreshThemeList()
    Theme_PreviewFile( wszClone )
    Theme_ApplyAll()
    frmThemes_ApplyShellTheme()
    AfxRedrawWindow( HWND_FRMTHEMES )
end sub


' Delete: confirm, remove the file, then hold the invariant -- reseed if the folder empties,
' and fall the active theme back to a survivor if the deleted one was active.
private sub frmThemes_DeleteClick( byval hButton as HWND, byval id as long )
    dim as long sel = PsListTree_GetCurSel( ghThemesList )
    if (sel < lbound(gThemeFilenames)) orelse (sel > ubound(gThemeFilenames)) then exit sub

    dim as DWSTRING wszFile = gThemeFilenames(sel)

    if frmThemes_ThemedMsgBox( L(390,"Delete this theme? This cannot be undone."), _
                               L(329,"Themes"), TMB_ICON_WARNING, TMB_OKCANCEL ) <> IDOK then
        exit sub
    end if

    dim as boolean bWasActive = (ucase(wszFile) = ucase(gConfig.ThemeShortFilename))
    AfxDeleteFile( frmThemes_ThemesFolder() & wszFile )
    frmThemes_EnsureThemeExists()

    ' Reload and pick a survivor near where the deleted row was.
    frmThemes_LoadThemes()
    dim as long newSel = sel
    if newSel > ubound(gThemeFilenames) then newSel = ubound(gThemeFilenames)
    if newSel < 0 then newSel = 0
    gThemesWorkFile = gThemeFilenames(newSel)

    if bWasActive then
        ' The active theme is gone -- promote the survivor, apply and commit it.
        gConfig.ThemeShortFilename = gThemesWorkFile
        gConfig.SaveConfigFile()
        LoadThemeFile()
        Theme_ApplyAll()
        Theme_DiscardSnapshot()
        Theme_Snapshot()
    else
        Theme_PreviewFile( gThemesWorkFile )
        Theme_ApplyAll()
    end if

    frmThemes_ApplyShellTheme()
    frmThemes_MarkActiveRow()
    PsListTree_SetCurSel( ghThemesList, newSel )
    PsListTree_SetTopIndex( ghThemesList, newSel )
    AfxRedrawWindow( ghThemesList )
    AfxRedrawWindow( HWND_FRMTHEMES )
end sub


' Set as active: the ONE commit point. Persist gConfig, load and apply, then re-snapshot so
' this becomes the baseline Close restores to.
private sub frmThemes_SetActiveClick( byval hButton as HWND, byval id as long )
    dim as long sel = PsListTree_GetCurSel( ghThemesList )
    if (sel < lbound(gThemeFilenames)) orelse (sel > ubound(gThemeFilenames)) then exit sub

    gThemesWorkFile = gThemeFilenames(sel)
    gConfig.ThemeShortFilename = gThemesWorkFile
    gConfig.SaveConfigFile()
    LoadThemeFile()
    Theme_ApplyAll()
    Theme_DiscardSnapshot()
    Theme_Snapshot()

    frmThemes_ApplyShellTheme()
    frmThemes_MarkActiveRow()
    AfxRedrawWindow( HWND_FRMTHEMES )
end sub


private sub frmThemes_CloseClick( byval hButton as HWND, byval id as long )
    SendMessageW( HWND_FRMTHEMES, WM_CLOSE, 0, 0 )
end sub


' Save: write the theme being edited to its .theme file (forking a built-in first). The heavy
' lifting is in frmThemes_SaveThemeFile (frmThemesPage.inc, where the fork machinery lives).
private sub frmThemes_SaveClick( byval hButton as HWND, byval id as long )
    if frmThemes_SaveThemeFile() then
        ' A fork may have repointed gThemesWorkFile; re-theme and repaint the header/footer.
        frmThemes_ApplyShellTheme()
        AfxRedrawWindow( HWND_FRMTHEMES )
    end if
end sub


' Revert: reload the theme from disk, discarding every edit since the last save.
private sub frmThemes_RevertClick( byval hButton as HWND, byval id as long )
    Theme_PreviewFile( gThemesWorkFile )
    theme.general.description = GetThemeDescription( frmThemes_ThemesFolder() & gThemesWorkFile )
    Theme_ApplyAll()
    frmThemes_ClearEdited()
    gThemesDescEdited = false

    frmThemes_ApplyShellTheme()
    frmThemes_ApplyTheme()                          ' refresh the page's swatches / picker
    AfxRedrawWindow( HWND_FRMTHEMES )
    if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
end sub


' Edit-description icon: pop up frmInputBox to change the theme's description text. A committed
' change marks the theme dirty (and forks a built-in at save time).
private sub frmThemes_DescEditClick( byval hButton as HWND, byval id as long )
    dim as DWSTRING wszDesc = theme.general.description
    if frmInputBox_Show( HWND_FRMTHEMES, L(329,"Themes"), L(387,"Description"), wszDesc ) = IDOK then
        theme.general.description = wszDesc
        gThemesDescEdited = true
        frmThemes_MarkEdited()
        frmThemes_PositionWindows( HWND_FRMTHEMES )   ' re-place the icon after the new text
        AfxRedrawWindow( HWND_FRMTHEMES )
    end if
end sub


' Back: leave the editor and return to the SELECT view. If there are unsaved edits, ask
' Save / Discard / Cancel first.
private sub frmThemes_GoBackClick( byval hButton as HWND, byval id as long )
    if frmThemes_WasEdited() then
        dim as long nAns = frmThemes_AskSaveChanges()
        if nAns = IDCANCEL then exit sub
        if nAns = IDYES then
            if frmThemes_SaveThemeFile() = false then exit sub   ' write failed / fork declined -> stay
        else
            ' Discard: reload the on-disk theme, dropping the in-memory edits.
            Theme_PreviewFile( gThemesWorkFile )
            Theme_ApplyAll()
            frmThemes_ClearEdited()
        end if
        gThemesDescEdited = false
    end if

    frmThemes_ApplyShellTheme()
    gThemesShowEdit = false
    frmThemes_ApplyViewVisibility()
    frmThemes_PositionWindows( HWND_FRMTHEMES )
    AfxRedrawWindow( HWND_FRMTHEMES )
    SetFocus( ghThemesList )
end sub


' ========================================================================================
' frmThemes window procedure
' ========================================================================================
function frmThemes_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    case WM_CREATE
        EnableWindow(GetParent(hwnd), false)
        return 0

    case WM_ERASEBKGND
        return true

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )
        dim as RECT rc
        GetClientRect( hwnd, @rc )
        b.SetBackColor( theme.panel.text.backcolor )
        b.PaintRect( @rc )

        ' SELECT view header band: a bold "Themes" on the first line, and the active theme named
        ' on the SECOND line under it.
        if gThemesShowEdit = false then
            ' Header text is indented by the same amount as the list's cell text (the AfxScaleX(12)
            ' inset used by the painters), so both lines start under the list's left content edge.
            dim as long mx    = AfxScaleX(FRMTHEMES_MARGIN)
            dim as long hx    = mx + AfxScaleX(12)
            dim as long yTop  = AfxScaleY(10)
            dim as long lineH = AfxScaleY(28)

            dim as HFONT hHead = ghThemesHeadFont
            if hHead = 0 then hHead = ghFont(GUIFONTBOLD_11)

            ' Line 1: the big bold "Themes".
            dim as DWSTRING s1 = L(329,"Themes")
            dim as RECT r1 : SetRect( @r1, hx, yTop, rc.right - mx, yTop + lineH )
            b.SetFont( hHead )
            b.SetForeColor( theme.panel.texthot.forecolor )
            b.PaintText( s1, @r1, DT_LEFT )

            ' Line 2, directly under it: the current active theme.
            dim as DWSTRING s2 = L(335,"Active") & ":  " & _
                                 GetThemeDescription( frmThemes_ThemesFolder() & gConfig.ThemeShortFilename )
            dim as RECT r2 : SetRect( @r2, hx, yTop + lineH, rc.right - mx, yTop + lineH + AfxScaleY(22) )
            b.SetFont( ghFont(GUIFONT_11) )
            b.SetForeColor( theme.panel.text.forecolor )
            b.PaintText( s2, @r2, DT_LEFT or DT_END_ELLIPSIS )
        else
            ' EDIT view header, left-aligned to the element list below it: bold "Edit theme",
            ' then the theme's actual description as read-only text under it.
            dim as long listLeftX = AfxScaleX(OPTROW_PADLEFT)
            dim as long rRight    = rc.right - AfxScaleX(FRMTHEMES_MARGIN)

            dim as HFONT hHead = ghThemesHeadFont
            if hHead = 0 then hHead = ghFont(GUIFONTBOLD_11)

            dim as DWSTRING sT = L(391,"Edit theme")
            dim as RECT rT : SetRect( @rT, listLeftX, AfxScaleY(8), rRight, AfxScaleY(34) )
            b.SetFont( hHead )
            b.SetForeColor( theme.panel.texthot.forecolor )
            b.PaintText( sT, @rT, DT_LEFT )

            dim as RECT rD : SetRect( @rD, listLeftX, AfxScaleY(40), rRight, AfxScaleY(62) )
            b.SetFont( ghFont(GUIFONT_11) )
            b.SetForeColor( theme.panel.text.forecolor )
            b.PaintText( theme.general.description, @rD, DT_LEFT or DT_END_ELLIPSIS )
        end if

        ' The rule above the footer, so the buttons read as a band.
        dim as RECT rcDiv
        SetRect( @rcDiv, 0, rc.bottom - AfxScaleY(FRMTHEMES_FOOTER_H), _
                 rc.right, rc.bottom - AfxScaleY(FRMTHEMES_FOOTER_H) + AfxScaleY(1) )
        b.SetBackColor( theme.ui.divider )
        b.PaintRect( @rcDiv )

        ' Unsaved-changes indicator, footer left, EDIT view only.
        if gThemesShowEdit andalso frmThemes_WasEdited() then
            dim as RECT rDirty
            SetRect( @rDirty, AfxScaleX(FRMTHEMES_MARGIN), rc.bottom - AfxScaleY(FRMTHEMES_FOOTER_H), _
                     rc.right \ 2, rc.bottom )
            b.SetFont( ghFont(GUIFONT_10) )
            b.SetForeColor( theme.ui.accent )
            b.PaintText( L(395,"Unsaved changes"), @rDirty, DT_LEFT or DT_VCENTER or DT_SINGLELINE )
        end if

        b.EndDoubleBuffer()
        return 0

    case WM_SIZE
        frmThemes_PositionWindows( hwnd )
        AfxRedrawWindow( hwnd )
        return 0

    case WM_COMMAND
        select case loword(wParam)
        case IDCANCEL
            ' Escape is handled in the pump (below); this is the fallback door.
            if gThemesShowEdit then
                frmThemes_GoBackClick( 0, 0 )
            else
                SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            end if
            return 0
        end select

    case WM_CLOSE
        ' Enables parent window keeping parent's zorder
        EnableWindow(GetParent(hwnd), true)
        DestroyWindow( hwnd )
        return 0

    case WM_DESTROY
        PostQuitMessage(0)
        return 0

    case WM_SYSCOMMAND
        if (wParam and &hFFF0) = SC_CLOSE then
            SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if

    case WM_NCDESTROY
        if ghThemesPageBrush then
            DeleteObject( ghThemesPageBrush )
            ghThemesPageBrush = 0
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr( hwnd )
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmThemes_Show -- modal, with its own nested pump. Parented to HWND_FRMMAIN.
' ========================================================================================
function frmThemes_Show( byval hWndParent as HWND ) as LRESULT

    frmThemes_Reset()
    for i as long = 0 to THEMEPAGE_COUNT - 1
        gThemesPageBuilt(i) = false
    next
    gThemesCurPage  = THEMEPAGE_NONE
    gThemesShowEdit = false

    ' The dialog stages the theme the application is currently using.
    gThemesWorkFile = gConfig.ThemeShortFilename

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    ' FIXED SIZE -- no WS_THICKFRAME. The colour page's three columns have to fit one known width.
    HWND_FRMTHEMES = pWindow->Create( hWndParent, L(329,"Themes"), @frmThemes_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT )
    pWindow->SetClientSize( FRMTHEMES_CLIENT_W, FRMTHEMES_CLIENT_H )
    pWindow->Center()

    Theme_ApplyDarkCaption( HWND_FRMTHEMES )

    ' A larger bold face for the "Themes" heading word. Owned by the CWindow (created through
    ' its font collection), so it is freed when the window is destroyed.
    ghThemesHeadFont = pWindow->CreateFont( GUIFONT, 15, FW_BOLD )

    ' ---- SELECT-view children -----------------------------------------------------------
    ghThemesList = PsListTree_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_LSTTHEMES )
    PsListTree_AddColumn( ghThemesList, L(386,"Theme"),       AfxScaleX(200) )
    PsListTree_AddColumn( ghThemesList, L(387,"Description"), AfxScaleX(240) )
    PsListTree_AddColumn( ghThemesList, "",                   AfxScaleX(80) )
    PsListTree_SetFillColumn( ghThemesList, THEMECOL_DESC )
    PsListTree_ShowHeader( ghThemesList, true )
    PsListTree_SetPaintCallback( ghThemesList, @frmThemes_ThemeListPaint )
    PsListTree_SetHeaderPaintCallback( ghThemesList, @frmThemes_HeaderPaint )
    PsListTree_SetSelChangeCallback( ghThemesList, @frmThemes_ThemeListSelChange )
    PsListTree_SetActivateCallback( ghThemesList, @frmThemes_ThemeListActivate )
    PsListTree_SetTooltipCallback( ghThemesList, @frmThemes_NoTooltip )
    PsListTree_SetHeaderTooltipCallback( ghThemesList, @frmThemes_NoHeaderTooltip )
    PsListTree_SetScrollBarWidth( ghThemesList, SCROLLBAR_WIDTH_PANEL )
    ' Rows 50% taller than the control's 22-unit default (UNSCALED -- the control scales it).
    PsListTree_SetRowHeight( ghThemesList, 33 )
    PsListTree_SetBorderWidth( ghThemesList, 1 )
    PsListTree_SetBorderColor( ghThemesList, theme.ui.divider )

    ghThemesEdit = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDEDIT )
    PsButton_SetText( ghThemesEdit, L(14,"Edit") )
    PsButton_SetClickCallback( ghThemesEdit, @frmThemes_EditClick )
    PsButton_SetDefault( ghThemesEdit, true )

    ghThemesClone = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDCLONE )
    PsButton_SetText( ghThemesClone, L(388,"Clone") )
    PsButton_SetClickCallback( ghThemesClone, @frmThemes_CloneClick )

    ghThemesDelete = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDDELETE )
    PsButton_SetText( ghThemesDelete, L(223,"Delete") )
    PsButton_SetClickCallback( ghThemesDelete, @frmThemes_DeleteClick )

    ghThemesSetActive = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDSETACTIVE )
    PsButton_SetText( ghThemesSetActive, L(389,"Set as active") )
    PsButton_SetClickCallback( ghThemesSetActive, @frmThemes_SetActiveClick )

    ghThemesClose = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDCLOSE )
    PsButton_SetText( ghThemesClose, L(136,"Close") )
    PsButton_SetClickCallback( ghThemesClose, @frmThemes_CloseClick )

    ' ---- EDIT-view children -------------------------------------------------------------
    ghThemesBar = PsSelectBar_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_SELECTBAR )
    PsSelectBar_AddPanel( ghThemesBar, L(306,"Syntax Colors"), THEMEPAGE_SYNTAX )
    PsSelectBar_AddPanel( ghThemesBar, L(307,"Interface Colors"), THEMEPAGE_INTERFACE )
    ' Zero the FIRST tab's left padding so its text starts at the bar's left edge (aligned with
    ' the Back button); -1 keeps its right padding at the bar default.
    PsSelectBar_SetPanelPadding( ghThemesBar, 0, 0, -1 )
    PsSelectBar_SetSelChangeCallback( ghThemesBar, @frmThemes_BarSelChange )

    scope
        dim pPage as CWindow ptr = new CWindow
        HWND_FRMTHEMES_PAGE = pPage->Create( HWND_FRMTHEMES, "", @frmThemes_PageWndProc, _
                                             0, 0, 0, 0, _
                                             WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                                             WS_EX_CONTROLPARENT )
    end scope

    ghThemesBack = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDGOBACK )
    ' A left chevron (U+2039) before the caption -- built with wchr so the source file's
    ' encoding cannot mangle it.
    PsButton_SetText( ghThemesBack, wchr(&h2039) & "  " & L(334,"Back") )
    PsButton_SetClickCallback( ghThemesBack, @frmThemes_GoBackClick )

    ' Edit-description icon (a pencil glyph); opens frmInputBox to change the description text.
    ' The glyph is the CENTRED CAPTION, not a left icon: PsButton pins an icon to the padding, so
    ' SetGlyphLeft cannot centre a lone icon, but the caption honours BTN_ALIGN_CENTER (its
    ' default). The button's font is the symbol font (set in ApplyShellTheme). The default 12px
    ' padding would collapse the caption span in a small button, so it is shrunk here.
    ghThemesDescEdit = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDDESCEDIT )
    PsButton_SetPadding( ghThemesDescEdit, AfxScaleX(4), AfxScaleY(2), AfxScaleX(4), AfxScaleY(2) )
    PsButton_SetText( ghThemesDescEdit, wchr(&hE70F) )
    PsButton_SetClickCallback( ghThemesDescEdit, @frmThemes_DescEditClick )

    ' A THEMED PsTooltip on the edit button (not PsButton's comctl32 one). Create also attaches.
    ghThemesDescTip = PsTooltip_Create( ghThemesDescEdit )
    PsTooltip_SetText( ghThemesDescTip, L(397,"Edit theme description") )
    PsTooltip_SetFonts( ghThemesDescTip, ghFont(GUIFONT_11), ghFont(GUIFONTBOLD_11), ghFont(SYMBOLFONT_10) )
    PsTooltip_SetStyle( ghThemesDescTip, TIP_STYLE_BALLOON )
    frmThemes_ThemeDescTip()

    ghThemesSave = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDSAVE )
    PsButton_SetText( ghThemesSave, L(7,"Save") )
    PsButton_SetClickCallback( ghThemesSave, @frmThemes_SaveClick )
    PsButton_SetDefault( ghThemesSave, true )

    ghThemesRevert = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDREVERT )
    PsButton_SetText( ghThemesRevert, L(392,"Revert") )
    PsButton_SetClickCallback( ghThemesRevert, @frmThemes_RevertClick )

    frmThemes_ApplyShellTheme()
    frmThemes_PositionWindows( HWND_FRMTHEMES )
    frmThemes_RefreshThemeList()

    ' Start in the SELECT view.
    frmThemes_ApplyViewVisibility()

    ShowWindow( HWND_FRMTHEMES, SW_SHOW )
    UpdateWindow( HWND_FRMTHEMES )

    SetFocus( ghThemesList )

    ' Capture the active theme so Close (and Back) can undo whatever the live preview did.
    Theme_Snapshot()

    ' ---- message loop -------------------------------------------------------------------
    ' NEARLY EMPTY. PsTextBox_FilterMessage covers the alpha spinner's PsNumericUpDown in the
    ' EDIT view. PsColorPicker contributes NOTHING -- no FilterMessage exists for it.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        if PsTextBox_FilterMessage( @uMsg ) then continue while

        ' The colour popup's dismissal (Escape / outside-click). Ahead of the dialog's own
        ' Escape so that, while the popup is open, Escape closes it rather than backing out.
        if frmThemes_ColorPopupFilterMessage( @uMsg ) then continue while

        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            ' In the editor, Escape backs out to the picker; in the picker it closes.
            if gThemesShowEdit then
                frmThemes_GoBackClick( 0, 0 )
            else
                SendMessageW( HWND_FRMTHEMES, WM_CLOSE, 0, 0 )
            end if
            continue while
        end if

        if IsDialogMessage( HWND_FRMTHEMES, @uMsg ) = 0 then
            TranslateMessage @uMsg
            DispatchMessage @uMsg
        end if
    wend

    ' The dialog previewed live; on the way out restore the last committed baseline (the active
    ' theme at open, or the last Set-as-active) and repaint the application behind.
    Theme_Restore()
    Theme_ApplyAll()
    Theme_DiscardSnapshot()

    frmThemes_Reset()
    ghThemesList = 0 : ghThemesBar = 0
    ghThemesEdit = 0 : ghThemesClone = 0 : ghThemesDelete = 0 : ghThemesSetActive = 0
    if ghThemesDescTip then
        if IsWindow( ghThemesDescTip ) then DestroyWindow( ghThemesDescTip )
        ghThemesDescTip = 0
    end if
    ghThemesClose = 0 : ghThemesBack = 0
    ghThemesSave = 0 : ghThemesRevert = 0 : ghThemesDescEdit = 0
    ghThemesHeadFont = 0     ' the CWindow freed the font itself on destroy
    HWND_FRMTHEMES = 0 : HWND_FRMTHEMES_PAGE = 0
    gThemesCurPage = THEMEPAGE_NONE
    gThemesShowEdit = false

    function = 0
end function


#include once "frmThemesPage.inc"
