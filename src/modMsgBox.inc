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

'  modMsgBox.inc  --  implementation of the one themed message box. See modMsgBox.bi for why
'                     this is split off and why it has to survive being called before the
'                     application's fonts, palette and localization exist.

#pragma once


' ========================================================================================
' A localized word with a REAL English fallback.
'
' L(id, "default") discards its second argument -- it is #Define L(e,s) LL(e), so an id with
' no entry renders as an empty string (CLAUDE.md, and the 29 blank captions that rule already
' cost). Everywhere else in tiko that is a bug to fix in english.lang; here it is a legitimate
' state, because two of the callers run BEFORE LoadLocalizationFile. A box whose buttons have
' no words on them is unusable, so the literal is used when the lookup comes back empty.
' ========================================================================================
private function MsgBox_Word( byval nId as long, byval wszFallback as DWSTRING ) as DWSTRING
    dim as DWSTRING wsz = LL(nId)
    if len(wsz) = 0 then return wszFallback
    return wsz
end function


' ========================================================================================
' Has the theme been read yet?
'
' Every gh* palette is a plain global struct, so before LoadThemeFile it is all zeroes -- i.e.
' black text on a black background. There is no "loaded" flag to consult, so the test is the
' symptom itself: identical fore and back means nothing on the box would be legible. Only the
' four startup boxes can ever see this.
' ========================================================================================
private function MsgBox_ThemeIsUsable() as boolean
    if ghPanel.BackColor <> ghPanel.ForeColor then return true
    return false
end function


' ========================================================================================
' The text font.
'
' Normally ghFont(GUIFONT_11), tiko's own UI font. Before frmMain_Show has built the cache
' there is nothing there, so the system's MESSAGE font is used -- lfMessageFont is the face
' Windows itself draws a message box in, and it arrives already correct for the system DPI, so
' this needs no scaling of its own. Same reasoning as PsTooltip_GetSystemFont reading
' lfStatusFont: a box in a face nothing else on the machine uses reads as broken.
'
' Returns 0 in hOwned when the cached font was used; the caller frees whatever comes back in it.
' ========================================================================================
private function MsgBox_ResolveTextFont( byref hOwned as HFONT ) as HFONT
    hOwned = 0
    if ghFont(GUIFONT_11) then return ghFont(GUIFONT_11)

    dim as NONCLIENTMETRICSW ncm
    ncm.cbSize = sizeof(NONCLIENTMETRICSW)
    if SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), @ncm, 0 ) then
        hOwned = CreateFontIndirectW( @ncm.lfMessageFont )
    end if
    return hOwned
end function


' ========================================================================================
' The large icon glyph font, created per box and freed by the caller -- frmBuildConfig's and
' modFileWatch's pattern, because no cached ghFont is anywhere near 22pt.
'
' The parent's CWindow is used when there is one, so the point size lands at the parent's DPI.
' With no parent (the startup boxes) there is no CWindow to ask, and the height is computed
' from the desktop DC instead. Returns 0 if it cannot be made, which PsMessageBox tolerates.
' ========================================================================================
private function MsgBox_CreateGlyphFont( byval hParent as HWND ) as HFONT
    if hParent then
        dim pWin as CWindow ptr = AfxCWindowPtr( hParent )
        if pWin then return pWin->CreateFont( SYMBOLFONT, 22 )
    end if

    dim as HDC hDC = GetDC( 0 )
    if hDC = 0 then return 0
    dim as long nDpiY = GetDeviceCaps( hDC, LOGPIXELSY )
    ReleaseDC( 0, hDC )
    if nDpiY = 0 then nDpiY = 96

    dim as LOGFONTW lf
    lf.lfHeight  = -MulDiv( 22, nDpiY, 72 )
    lf.lfWeight  = FW_NORMAL
    lf.lfCharSet = DEFAULT_CHARSET
    lf.lfQuality = CLEARTYPE_QUALITY
    lf.lfFaceName = SYMBOLFONT
    return CreateFontIndirectW( @lf )
end function


' ========================================================================================
' Show a themed modal message box. See modMsgBox.bi for the contract.
' ========================================================================================
function TikoMsgBox( byval hParent      as HWND, _
                     byval wszText      as DWSTRING, _
                     byval wszCaption   as DWSTRING, _
                     byval nIcon        as long = TMB_ICON_INFO, _
                     byval nButtons     as long = TMB_OK, _
                     byval nDefaultIndex as long = 0, _
                     byval hTextFont    as HFONT = 0 ) as long

    dim as HWND h = PsMessageBox_Create( hParent )
    if h = 0 then return IDCANCEL

    PsMessageBox_SetCaption( h, wszCaption )
    PsMessageBox_SetText( h, wszText )

    ' tiko's TMB_* vocabulary -> the control's. Kept as a select rather than an arithmetic
    ' offset so the two enums are free to diverge.
    dim as long nKind = MBX_ICON_NONE
    select case nIcon
        case TMB_ICON_INFO     : nKind = MBX_ICON_INFO
        case TMB_ICON_WARNING  : nKind = MBX_ICON_WARNING
        case TMB_ICON_ERROR    : nKind = MBX_ICON_ERROR
        case TMB_ICON_QUESTION : nKind = MBX_ICON_QUESTION
    end select
    PsMessageBox_SetIconKind( h, nKind )

    ' A caller-supplied font wins and is NOT owned here -- it is one of tiko's cached HFONTs.
    dim as HFONT hOwnedText = 0
    dim as HFONT hFont = hTextFont
    if hFont = 0 then hFont = MsgBox_ResolveTextFont( hOwnedText )
    if hFont then
        PsMessageBox_SetFont( h, hFont )
        PsMessageBox_SetCaptionFont( h, hFont )
    end if
    if ghFont(SYMBOLFONT_10) then PsMessageBox_SetCloseGlyphFont( h, ghFont(SYMBOLFONT_10) )

    ' Only made when there is a glyph to draw with it.
    dim as HFONT hGlyph = 0
    if nKind <> MBX_ICON_NONE then
        hGlyph = MsgBox_CreateGlyphFont( hParent )
        if hGlyph then PsMessageBox_SetGlyphFont( h, hGlyph )
    end if

    ' ---- colours ---------------------------------------------------------------------------
    dim as MBX_COLORS mc
    dim as PSBUTTON_COLORS bc

    if MsgBox_ThemeIsUsable() then
        mc.BorderColor      = ghEditor.Divider
        mc.CaptionBackColor = ghPanel.BackColor
        mc.CaptionForeColor = ghPanel.ForeColorHot
        mc.CloseBackColor   = ghPanel.BackColor
        mc.CloseForeColor   = ghPanel.ForeColor
        mc.BackColor        = ghPanel.BackColor
        mc.ForeColor        = ghPanel.ForeColor
        mc.FooterBackColor  = ghPanel.BackColor
        mc.DividerColor     = ghEditor.Divider
        OptionsTheme_FillButton( @bc )
    else
        ' Startup, before any theme exists. GetSysColor rather than a hardcoded dark set, so
        ' the box follows whatever the machine is set to instead of guessing at it.
        dim as COLORREF clrFace = GetSysColor( COLOR_BTNFACE )
        dim as COLORREF clrText = GetSysColor( COLOR_BTNTEXT )
        dim as COLORREF clrEdge = GetSysColor( COLOR_ACTIVEBORDER )
        mc.BorderColor      = clrEdge
        mc.CaptionBackColor = clrFace
        mc.CaptionForeColor = clrText
        mc.CloseBackColor   = clrFace
        mc.CloseForeColor   = clrText
        mc.BackColor        = clrFace
        mc.ForeColor        = clrText
        mc.FooterBackColor  = clrFace
        mc.DividerColor     = clrEdge
        bc.BackColor            = clrFace
        bc.ForeColor            = clrText
        bc.BorderColor          = clrEdge
        bc.IconColor            = clrText
        bc.BackColorHot         = GetSysColor( COLOR_BTNHIGHLIGHT )
        bc.ForeColorHot         = clrText
        bc.BorderColorHot       = clrText
        bc.IconColorHot         = clrText
        bc.BackColorPressed     = GetSysColor( COLOR_BTNSHADOW )
        bc.ForeColorPressed     = clrText
        bc.BorderColorPressed   = clrText
        bc.IconColorPressed     = clrText
        bc.BackColorDisabled    = clrFace
        bc.ForeColorDisabled    = GetSysColor( COLOR_GRAYTEXT )
        bc.BorderColorDisabled  = clrEdge
        bc.IconColorDisabled    = GetSysColor( COLOR_GRAYTEXT )
    end if

    PsMessageBox_SetColors( h, @mc )
    PsMessageBox_SetButtonColors( h, @bc )

    ' ---- buttons ---------------------------------------------------------------------------
    ' The ids the callers test against are the Win32 ones, so nothing this replaced needed a
    ' change to its result handling. The cancel id is the rightmost button.
    dim as long nCount = 1
    select case nButtons
        case TMB_OKCANCEL
            PsMessageBox_AddButton( h, MsgBox_Word( 0, "OK" ),     IDOK )
            PsMessageBox_AddButton( h, MsgBox_Word( 1, "Cancel" ), IDCANCEL )
            PsMessageBox_SetCancelID( h, IDCANCEL )
            nCount = 2
        case TMB_YESNO
            PsMessageBox_AddButton( h, MsgBox_Word( 98, "Yes" ), IDYES )
            PsMessageBox_AddButton( h, MsgBox_Word( 99, "No" ),  IDNO )
            PsMessageBox_SetCancelID( h, IDNO )
            nCount = 2
        case TMB_YESNOCANCEL
            PsMessageBox_AddButton( h, MsgBox_Word( 98, "Yes" ),   IDYES )
            PsMessageBox_AddButton( h, MsgBox_Word( 99, "No" ),    IDNO )
            PsMessageBox_AddButton( h, MsgBox_Word( 1, "Cancel" ), IDCANCEL )
            PsMessageBox_SetCancelID( h, IDCANCEL )
            nCount = 3
        case TMB_SAVE_DISCARD_CANCEL
            ' Save/Discard rather than Yes/No, but the same three ids, so a caller's IDYES /
            ' IDNO / IDCANCEL handling is identical to the YESNOCANCEL case.
            PsMessageBox_AddButton( h, MsgBox_Word( 7,   "Save" ),    IDYES )
            PsMessageBox_AddButton( h, MsgBox_Word( 553, "Discard" ), IDNO )
            PsMessageBox_AddButton( h, MsgBox_Word( 1,   "Cancel" ),  IDCANCEL )
            PsMessageBox_SetCancelID( h, IDCANCEL )
            nCount = 3
        case else
            PsMessageBox_AddButton( h, MsgBox_Word( 0, "OK" ), IDOK )
            PsMessageBox_SetCancelID( h, IDOK )
    end select

    ' A button INDEX, not an id -- and CLAMPED, because PsMessageBox_SetDefaultButton rejects an
    ' out-of-range index by exiting silently. Three of the builders folded in here passed an id
    ' and lost their intended default that way, with nothing to show for it. See modMsgBox.bi.
    if (nDefaultIndex < 0) orelse (nDefaultIndex >= nCount) then nDefaultIndex = 0
    PsMessageBox_SetDefaultButton( h, nDefaultIndex )

    dim as long nResult = PsMessageBox_DoModal( h )

    if hGlyph then DeleteObject( hGlyph )
    if hOwnedText then DeleteObject( hOwnedText )

    return nResult

end function
