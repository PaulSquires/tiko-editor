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
'  The COLOUR PAGE itself lives in frmThemesPage.inc, included at the bottom: it moved here
'  wholesale from frmOptionsTheme.inc and its only real change was the name.

#include once "frmThemes.bi"

dim shared ghThemesList     as HWND      ' PsListTree -- the themes folder
dim shared ghThemesBar      as HWND      ' PsSelectBar -- Syntax | Interface
dim shared ghThemesOK       as HWND
dim shared ghThemesCancel   as HWND
dim shared ghThemesPageBrush as HBRUSH

dim shared gThemesCurPage   as long = THEMEPAGE_NONE
dim shared gThemesPageBuilt(THEMEPAGE_COUNT - 1) as boolean

' Did the user press OK? The dialog previews LIVE, so dismissing it has to put gTheme() back.
' Both exit routes end in WM_CLOSE, so this is what tells them apart once the pump returns.
dim shared gThemesCommitted as boolean


' ========================================================================================
' A PsMessageBox dressed in the current theme. The colour page's fork prompt uses it, so it
' had to come across with the page -- frmOptions' version is not reachable from a dialog that
' can be opened without frmOptions.
' ========================================================================================
function frmThemes_ThemedMsgBox( byval wszText as DWSTRING, _
                                 byval wszCaption as DWSTRING, _
                                 byval nIconKind as long, _
                                 byval nPreset as long ) as long

    dim as HWND h = PsMessageBox_Create( HWND_FRMTHEMES )
    PsMessageBox_SetCaption( h, wszCaption )
    PsMessageBox_SetText( h, wszText )
    PsMessageBox_SetIconKind( h, nIconKind )
    PsMessageBox_SetFont( h, OptionsFont_Base() )
    PsMessageBox_SetCloseGlyphFont( h, ghFont(SYMBOLFONT_10) )

    dim as HFONT hGlyph = 0
    dim pWin as CWindow ptr = AfxCWindowPtr( HWND_FRMTHEMES )
    if pWin then hGlyph = pWin->CreateFont( SYMBOLFONT, 22 )
    if hGlyph then PsMessageBox_SetGlyphFont( h, hGlyph )

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
        PsMessageBox_SetDefaultButton( h, IDCANCEL )
    else
        PsMessageBox_AddButton( h, L(0,"OK"), IDOK )
        PsMessageBox_SetDefaultButton( h, IDOK )
    end if

    dim as long nResult = PsMessageBox_DoModal( h )
    if hGlyph then DeleteObject( hGlyph )
    return nResult

end function


' ========================================================================================
' The themes folder. Moved from frmOptionsColors, which no longer has anything to do with
' themes -- that page is about fonts now.
'
' gThemeFilenames is the PARALLEL array of filenames; the list shows each theme's
' description. Both are rebuilt together, so a row index means the same thing in each.
' ========================================================================================
private function frmThemes_LoadThemes() as long
    if ghThemesList = 0 then return -1

    PsListTree_BeginUpdate( ghThemesList )
    PsListTree_Clear( ghThemesList )
    erase gThemeFilenames

    dim as long idxMatch = -1
    dim as long idx = 0
    dim hSearch as HANDLE
    dim WFD as WIN32_FIND_DATAW
    dim as DWSTRING wszPath = AfxGetExePathName + "settings\themes\"

    hSearch = FindFirstFile( wszPath + "*", @WFD )
    if hSearch <> INVALID_HANDLE_VALUE then
        do
            if (WFD.cFileName <> ".") andalso (WFD.cFileName <> "..") then
                dim as DWSTRING wszFull = wszPath & WFD.cFileName
                dim as DWSTRING wszDisplay = GetThemeDescription( wszFull )
                idx = PsListTree_AddString( ghThemesList, wszDisplay )
                redim preserve gThemeFilenames(idx) as DWSTRING
                gThemeFilenames(idx) = WFD.cFileName
                if ucase(gThemesWorkFile) = ucase(WFD.cFileName) then idxMatch = idx
            end if
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    PsListTree_EndUpdate( ghThemesList )
    function = idxMatch
end function


' Re-read the folder and reselect whatever gThemesWorkFile now names. The colour page calls
' this after forking a built-in, which drops a new file in while the list is already built.
sub frmThemes_RefreshThemeList()
    if ghThemesList = 0 then exit sub
    dim as long idx = frmThemes_LoadThemes()
    if idx >= 0 then
        PsListTree_SetCurSel( ghThemesList, idx )
        PsListTree_SetTopIndex( ghThemesList, idx )
    end if
    AfxRedrawWindow( ghThemesList )
end sub


private sub frmThemes_ThemeListPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF backclr, foreclr
    dim as boolean bLit = (p->isSelected orelse p->isHot)
    if bLit then
        backclr = ghPanel.BackColorHot
        foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor
        foreclr = ghPanel.ForeColor
    end if

    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(12)
    p->b->SetFont( OptionsFont_Base() )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub


' PREVIEW FOR REAL: load into memory and schedule a full apply, so arrowing through this list
' recolours the whole editor behind the dialog. gConfig is NOT touched -- the pick stays in
' gThemesWorkFile, and Cancel restores the snapshot taken in frmThemes_Show.
private sub frmThemes_ThemeListSelChange( byval hList as HWND, byval row as integer )
    if (row < lbound(gThemeFilenames)) orelse (row > ubound(gThemeFilenames)) then exit sub
    gThemesWorkFile = gThemeFilenames(row)
    Theme_PreviewFile( gThemeFilenames(row) )

    ' A different theme means a different set of colours under the same keys, so the colour
    ' page's swatches and its picker are stale.
    frmThemes_ApplyTheme()
    if HWND_FRMTHEMES_PAGE then AfxRedrawWindow( HWND_FRMTHEMES_PAGE )
    AfxRedrawWindow( HWND_FRMTHEMES )
end sub


' ========================================================================================
' The page container. Same job as frmOptions' plain page window and for the same reasons:
' the colour page needs its OWN coordinate space (its controls are placed from 0,0) and the
' four services a page expects -- a themed background delivered through SCP_PAINTINFO,
' WM_CTLCOLOR*, forwarding of unclaimed commands, and WS_EX_CONTROLPARENT so Tab reaches it.
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

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )

        dim p as SCP_PAINTINFO
        p.hScrollPanel = hwnd
        p.hPanel       = hwnd
        p.b            = @b
        GetClientRect( hwnd, @p.rcClient )
        p.nPos         = 0

        b.SetBackColor( ghPanel.BackColor )
        b.PaintRect( @p.rcClient )
        if gThemesCurPage <> THEMEPAGE_NONE then frmThemes_Paint( gThemesCurPage, @p )

        b.EndDoubleBuffer()
        return 0

    case WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORLISTBOX
        SetBkColor( cast(HDC, wParam), ghPanel.BackColor )
        SetTextColor( cast(HDC, wParam), ghPanel.ForeColor )
        if ghThemesPageBrush = 0 then ghThemesPageBrush = CreateSolidBrush( ghPanel.BackColor )
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
' Switch the visible colour page. Built on first visit, like the Options pages were.
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
' Position the shell's children.
' ========================================================================================
private sub frmThemes_PositionWindows( byval hwnd as HWND )

    dim as RECT rc
    GetClientRect( hwnd, @rc )
    if (rc.right <= 0) orelse (rc.bottom <= 0) then exit sub

    dim as long listW   = AfxScaleX(FRMTHEMES_LIST_W)
    dim as long barH    = AfxScaleY(FRMTHEMES_BAR_H)
    dim as long footerH = AfxScaleY(FRMTHEMES_FOOTER_H)
    dim as long margin  = AfxScaleX(FRMTHEMES_MARGIN)
    dim as long btnW    = AfxScaleX(FRMTHEMES_BTN_W)
    dim as long btnH    = AfxScaleY(FRMTHEMES_BTN_H)

    dim as long bodyH = rc.bottom - footerH
    if bodyH < 0 then bodyH = 0

    SetWindowPos( ghThemesList, 0, 0, 0, listW, bodyH, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemesBar, 0, listW, 0, rc.right - listW, barH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long pageH = bodyH - barH
    if pageH < 0 then pageH = 0
    SetWindowPos( HWND_FRMTHEMES_PAGE, 0, listW, barH, rc.right - listW, pageH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )

    if gThemesCurPage <> THEMEPAGE_NONE then
        frmThemes_Layout( gThemesCurPage, HWND_FRMTHEMES_PAGE, rc.right - listW )
    end if

    dim as long btnY = rc.bottom - footerH + (footerH - btnH) \ 2
    SetWindowPos( ghThemesCancel, 0, rc.right - margin - btnW, btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghThemesOK, 0, rc.right - margin - (btnW * 2) - AfxScaleX(8), btnY, btnW, btnH, _
                  SWP_NOZORDER or SWP_NOACTIVATE )
end sub


' ========================================================================================
' Push the current theme's colours into the shell's own controls. The colour page themes
' itself through frmThemes_ApplyTheme, which lives in frmThemesPage.inc.
' ========================================================================================
private sub frmThemes_ApplyShellTheme()

    if ghThemesPageBrush then DeleteObject( ghThemesPageBrush )
    ghThemesPageBrush = CreateSolidBrush( ghPanel.BackColor )

    if ghThemesList then
        PsListTree_SetBackColor( ghThemesList, ghPanel.BackColor )
        PsListTree_SetFont( ghThemesList, OptionsFont_Base() )
    end if

    if ghThemesBar then
        dim as PSSELECTBAR_COLORS sc
        sc.BackColor        = ghPanel.BackColor
        sc.BackColorHot     = ghPanel.BackColor
        sc.BackColorSelect  = ghPanel.BackColor
        sc.ForeColor        = ghPanel.ForeColor
        sc.ForeColorHot     = ghPanel.ForeColorHot
        sc.ForeColorSelect  = ghPanel.ForeColorHot
        sc.IndicatorColor   = ghUI.AccentColor
        PsSelectBar_SetColors( ghThemesBar, @sc )
        PsSelectBar_SetFont( ghThemesBar, OptionsFont_Base() )
        PsSelectBar_Refresh( ghThemesBar )
    end if

    dim as PSBUTTON_COLORS bc
    OptionsTheme_FillButton( @bc )
    if ghThemesOK then
        PsButton_SetColors( ghThemesOK, @bc )
        PsButton_SetFont( ghThemesOK, OptionsFont_Base() )
        PsButton_Refresh( ghThemesOK )
    end if
    if ghThemesCancel then
        PsButton_SetColors( ghThemesCancel, @bc )
        PsButton_SetFont( ghThemesCancel, OptionsFont_Base() )
        PsButton_Refresh( ghThemesCancel )
    end if

    frmThemes_ApplyTheme()
end sub


' ========================================================================================
' OK. Everything the dialog can change is committed here and nowhere else.
' ========================================================================================
private sub frmThemes_ApplyAndClose( byval hwnd as HWND )

    ' Persist colour edits BEFORE the reload below, or that reload would read the file back
    ' over the top of them and silently discard the whole session's work. A protected
    ' built-in can never reach here: the first edit forks it and repoints gThemesWorkFile.
    if frmThemes_WasEdited() then
        if frmThemes_IsProtectedTheme( gThemesWorkFile ) = false then
            dim as DWSTRING wszPath = AfxGetExePathName & "settings\themes\" & gThemesWorkFile
            if Theme_WriteFile( wszPath, ghGeneral.description ) then
                frmThemes_ThemedMsgBox( L(454,"The new theme file could not be written."), _
                                        L(486,"Themes"), MBX_ICON_ERROR, MBX_BTN_OK )
            end if
        end if
    end if

    gConfig.ThemeShortFilename = gThemesWorkFile
    gConfig.SaveConfigFile()

    ' The dialog has been previewing live, so gTheme() already holds the pick. Reload from
    ' disk anyway now that gConfig is written: it makes what ships exactly what a fresh start
    ' would load, and after a colour edit it is a genuine round-trip check of what we wrote.
    LoadThemeFile()

    Theme_DiscardSnapshot()
    gThemesCommitted = true

    Theme_ApplyAll()
    DestroyWindow( hwnd )
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
        DisableAllModeless()
        return 0

    case WM_ERASEBKGND
        return true

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )
        dim as RECT rc
        GetClientRect( hwnd, @rc )
        b.SetBackColor( ghPanel.BackColor )
        b.PaintRect( @rc )

        ' The rule above the footer, so the buttons read as a band rather than as two
        ' controls floating on the page.
        dim as RECT rcDiv
        SetRect( @rcDiv, 0, rc.bottom - AfxScaleY(FRMTHEMES_FOOTER_H), _
                 rc.right, rc.bottom - AfxScaleY(FRMTHEMES_FOOTER_H) + AfxScaleY(1) )
        b.SetBackColor( ghEditor.Divider )
        b.PaintRect( @rcDiv )

        b.EndDoubleBuffer()
        return 0

    case WM_SIZE
        frmThemes_PositionWindows( hwnd )
        AfxRedrawWindow( hwnd )
        return 0

    case WM_COMMAND
        select case loword(wParam)
        case IDCANCEL
            SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end select

    case WM_CLOSE
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


private sub frmThemes_OKClick( byval hButton as HWND, byval id as long )
    frmThemes_ApplyAndClose( HWND_FRMTHEMES )
end sub

private sub frmThemes_CancelClick( byval hButton as HWND, byval id as long )
    SendMessageW( HWND_FRMTHEMES, WM_CLOSE, 0, 0 )
end sub


' ========================================================================================
' frmThemes_Show -- modal, with its own nested pump.
' ========================================================================================
function frmThemes_Show( byval hWndParent as HWND ) as LRESULT

    frmThemes_Reset()
    for i as long = 0 to THEMEPAGE_COUNT - 1
        gThemesPageBuilt(i) = false
    next
    gThemesCurPage = THEMEPAGE_NONE

    ' The dialog stages the theme the application is currently using.
    gThemesWorkFile = gConfig.ThemeShortFilename

    dim pWindow as CWindow ptr = new CWindow
    pWindow->DPI = AfxCWindowOwnerPtr(hWndParent)->DPI

    ' FIXED SIZE -- no WS_THICKFRAME. The colour page's three columns have to fit one known
    ' width; a resizable dialog would make "does it fit" a question with no stable answer.
    HWND_FRMTHEMES = pWindow->Create( hWndParent, L(486,"Themes"), @frmThemes_WndProc, 0, 0, 0, 0, _
        WS_POPUP or WS_CAPTION or WS_SYSMENU or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
        WS_EX_DLGMODALFRAME or WS_EX_CONTROLPARENT )
    pWindow->SetClientSize( FRMTHEMES_CLIENT_W, FRMTHEMES_CLIENT_H )
    pWindow->Center()

    Theme_ApplyDarkCaption( HWND_FRMTHEMES )

    ' ---- children ---------------------------------------------------------------------
    ghThemesList = PsListTree_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_LSTTHEMES )
    PsListTree_SetPaintCallback( ghThemesList, @frmThemes_ThemeListPaint )
    PsListTree_SetSelChangeCallback( ghThemesList, @frmThemes_ThemeListSelChange )
    PsListTree_SetScrollBarWidth( ghThemesList, SCROLLBAR_WIDTH_PANEL )

    ghThemesBar = PsSelectBar_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_SELECTBAR )
    PsSelectBar_AddPanel( ghThemesBar, L(463,"Syntax Colors"), THEMEPAGE_SYNTAX )
    PsSelectBar_AddPanel( ghThemesBar, L(464,"Interface Colors"), THEMEPAGE_INTERFACE )
    PsSelectBar_SetSelChangeCallback( ghThemesBar, @frmThemes_BarSelChange )

    scope
        dim pPage as CWindow ptr = new CWindow
        HWND_FRMTHEMES_PAGE = pPage->Create( HWND_FRMTHEMES, "", @frmThemes_PageWndProc, _
                                             0, 0, 0, 0, _
                                             WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
                                             WS_EX_CONTROLPARENT )
    end scope

    ghThemesOK = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDOK )
    PsButton_SetText( ghThemesOK, L(0,"OK") )
    PsButton_SetClickCallback( ghThemesOK, @frmThemes_OKClick )
    PsButton_SetDefault( ghThemesOK, true )

    ghThemesCancel = PsButton_Create( HWND_FRMTHEMES, IDC_FRMTHEMES_CMDCANCEL )
    PsButton_SetText( ghThemesCancel, L(1,"Cancel") )
    PsButton_SetClickCallback( ghThemesCancel, @frmThemes_CancelClick )

    frmThemes_ApplyShellTheme()
    frmThemes_PositionWindows( HWND_FRMTHEMES )

    frmThemes_RefreshThemeList()

    ShowWindow( ghThemesList, SW_SHOW )
    ShowWindow( ghThemesBar, SW_SHOW )
    ShowWindow( HWND_FRMTHEMES_PAGE, SW_SHOW )
    ShowWindow( ghThemesOK, SW_SHOW )
    ShowWindow( ghThemesCancel, SW_SHOW )

    PsSelectBar_SetCurSel( ghThemesBar, THEMEPAGE_SYNTAX )
    frmThemes_SelectPage( THEMEPAGE_SYNTAX )

    ShowWindow( HWND_FRMTHEMES, SW_SHOW )
    UpdateWindow( HWND_FRMTHEMES )

    ' IsDialogMessage ignores Tab until a child already holds focus, so the first control is
    ' focused explicitly -- an ordinary CWindow gets no dialog manager to do it.
    SetFocus( ghThemesList )

    ' Capture the theme as it stands so Cancel can undo whatever the live preview did. Only
    ' gTheme() is captured -- the nine gh* globals are a pure function of it.
    gThemesCommitted = false
    Theme_Snapshot()

    ' ---- message loop -------------------------------------------------------------------
    ' NEARLY EMPTY, and deliberately so. PsTextBox_FilterMessage covers the alpha spinner's
    ' PsNumericUpDown, which wraps one. PsColorPicker contributes NOTHING: there is no
    ' PsColorPicker_FilterMessage and there never will be, which is the property that whole
    ' control was designed around.
    dim uMsg as MSG
    while GetMessage( @uMsg, null, 0, 0 )

        if PsTextBox_FilterMessage( @uMsg ) then continue while

        if (uMsg.message = WM_KEYDOWN) andalso (uMsg.wParam = VK_ESCAPE) then
            SendMessageW( HWND_FRMTHEMES, WM_CLOSE, 0, 0 )
            continue while
        end if

        if IsDialogMessage( HWND_FRMTHEMES, @uMsg ) = 0 then
            TranslateMessage @uMsg
            DispatchMessage @uMsg
        end if
    wend

    ' CANCEL. The dialog previewed live, so dismissing it has to put gTheme() back -- and
    ' then repaint, because the application behind has been recolouring all along.
    if gThemesCommitted = false then
        Theme_Restore()
        Theme_ApplyAll()
    end if

    EnableAllModeless()
    frmThemes_Reset()
    ghThemesList = 0 : ghThemesBar = 0 : ghThemesOK = 0 : ghThemesCancel = 0
    HWND_FRMTHEMES = 0 : HWND_FRMTHEMES_PAGE = 0
    gThemesCurPage = THEMEPAGE_NONE

    function = 0
end function


#include once "frmThemesPage.inc"
