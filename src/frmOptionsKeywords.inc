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

'  frmOptionsKeywords.inc  --  the three Keywords pages (see frmOptionsKeywords.bi).

#include once "frmOptionsKeywords.bi"
#include once "frmOptions.bi"          ' HWND_FRMOPTIONS_PAGE, OPTPAGE_KEYWORDS*
#include once "modOptionsRows.bi"      ' shared padding + theme helpers
#include once "clsConfig.bi"


#define KW_COUNT   3
#define KW_MAXTEXT (1024 * 100)        ' 100K, as the old pages allowed


' One multiline editor per page, plus a single shared scrollbar (only one page shows at a
' time). Handles are 0 until their page is first visited.
dim shared ghKwText(KW_COUNT - 1) as HWND
dim shared ghKwScroll as HWND
dim shared gKwBuilt as boolean


' ========================================================================================
' Page <-> slot mapping.
' ========================================================================================
function frmOptionsKeywords_OwnsPage( byval nPage as long ) as boolean
    select case nPage
        case OPTPAGE_KEYWORDS, OPTPAGE_KEYWORDSWINAPI, OPTPAGE_KEYWORDSEXTRA : return true
    end select
    function = false
end function

private function frmOptionsKeywords_Index( byval nPage as long ) as long
    select case nPage
        case OPTPAGE_KEYWORDS:        return 0
        case OPTPAGE_KEYWORDSWINAPI:  return 1
        case OPTPAGE_KEYWORDSEXTRA:   return 2
    end select
    function = -1
end function


' ========================================================================================
' Forget all state. The textboxes and the scrollbar live on the scroll panel page and die
' with it, so their handles are stale on the next open.
' ========================================================================================
sub frmOptionsKeywords_Reset()
    for i as long = 0 to KW_COUNT - 1
        ghKwText(i) = 0
    next
    ghKwScroll = 0
    gKwBuilt   = false
end sub


' ========================================================================================
' Which keyword editor is currently visible (or 0).
' ========================================================================================
private function frmOptionsKeywords_VisibleText() as HWND
    for i as long = 0 to KW_COUNT - 1
        if ghKwText(i) then
            if IsWindowVisible( ghKwText(i) ) then return ghKwText(i)
        end if
    next
    function = 0
end function


' ========================================================================================
' The gConfig keyword blob for a slot. Returned by value; these are `string` in clsConfig.
' ========================================================================================
private function frmOptionsKeywords_ConfigText( byval idx as long ) as string
    select case idx
        case 0: return gConfig.FBKeywords
        case 1: return gConfig.WinApiKeywords
        case 2: return gConfig.ExtraKeywords
    end select
    function = ""
end function


' ========================================================================================
' Callbacks.
' ========================================================================================
' User edited the text (SetText stays silent). Mark the keyword set dirty, exactly as the
' old EN_CHANGE handler did -- nothing is read here; the harvest on OK does that.
private sub frmOptionsKeywords_TextChanged( byval hTextBox as HWND )
    gConfig.bKeywordsDirty = true
end sub

' The visible editor's scroll state changed: refresh the shared scrollbar's range.
private sub frmOptionsKeywords_TextScrollChanged( byval hTextBox as HWND )
    if IsWindowVisible( hTextBox ) = 0 then exit sub
    if ghKwScroll = 0 then exit sub
    dim as integer nTotal, nPerPage, nFirst
    PsTextBox_GetVScrollInfo( hTextBox, nTotal, nPerPage, nFirst )
    PsVScrollBar_SetRange( ghKwScroll, nTotal, nPerPage, nFirst )
end sub

' The user dragged the shared scrollbar: scroll whichever editor is visible.
private sub frmOptionsKeywords_ScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    dim as HWND hTb = frmOptionsKeywords_VisibleText()
    if hTb then PsTextBox_ScrollToLine( hTb, newPos )
end sub


' ========================================================================================
' Theme one keyword editor. NOT OptionsTheme_ApplyTextBox: that forces the base UI font,
' and the keyword editors deliberately use a fixed-pitch face.
' ========================================================================================
private sub frmOptionsKeywords_ThemeText( byval hTb as HWND )
    if hTb = 0 then exit sub
    PsTextBox_SetFont( hTb, ghFont(INFOFONT_11) )      ' Consolas 11 (fixed pitch)
    PsTextBox_SetForeColor( hTb, theme.ui.field.forecolor )
    PsTextBox_SetBackColor( hTb, theme.ui.field.backcolor )
    PsTextBox_SetBorderColor( hTb, theme.ui.divider )
    PsTextBox_SetFocusBorderColor( hTb, theme.panel.texthot.forecolor )
    ThemeTextBoxContextMenu( hTb )
end sub


' ========================================================================================
' Build the editor for one page (and the shared scrollbar, once). Idempotent per page.
' ========================================================================================
sub frmOptionsKeywords_Build( byval nPage as long, byval hPage as HWND )

    dim as long idx = frmOptionsKeywords_Index( nPage )
    if idx < 0 then exit sub

    ' The shared scrollbar, created the first time any keyword page is visited.
    if ghKwScroll = 0 then
        ghKwScroll = PsVScrollBar_Create( hPage, IDC_FRMOPTIONSKEYWORDS_VSCROLL )
        PsVScrollBar_SetScrollCallback( ghKwScroll, @frmOptionsKeywords_ScrollCallback )
    end if

    if ghKwText(idx) = 0 then
        dim as HWND hTb = PsTextBox_Create( hPage, IDC_FRMOPTIONSKEYWORDS_TXTBASE + idx, true )   ' multiline
        PsTextBox_SetLimitText( hTb, KW_MAXTEXT )
        PsTextBox_SetChangeCallback( hTb, @frmOptionsKeywords_TextChanged )
        PsTextBox_SetScrollChangedCallback( hTb, @frmOptionsKeywords_TextScrollChanged )
        ' Seed from gConfig (silent; the change callback does not fire for SetText).
        PsTextBox_SetText( hTb, wstr( frmOptionsKeywords_ConfigText( idx ) ) )
        ghKwText(idx) = hTb
        frmOptionsKeywords_ThemeText( hTb )
    end if

    gKwBuilt = true
    frmOptionsKeywords_ApplyTheme()
end sub


' ========================================================================================
' Theme everything built so far.
' ========================================================================================
sub frmOptionsKeywords_ApplyTheme()
    if gKwBuilt = false then exit sub
    for i as long = 0 to KW_COUNT - 1
        frmOptionsKeywords_ThemeText( ghKwText(i) )
    next
    if ghKwScroll then
        PsVScrollBar_SetColors( ghKwScroll, theme.panel.scrollbar.backcolor, _
                               theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )
    end if
end sub


' ========================================================================================
' Show / hide the editor for one page and the shared scrollbar.
' ========================================================================================
sub frmOptionsKeywords_Show( byval nPage as long, byval bShow as boolean )
    dim as long idx = frmOptionsKeywords_Index( nPage )
    if idx < 0 then exit sub
    if ghKwText(idx) = 0 then exit sub
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghKwText(idx), nCmd )
    if ghKwScroll then ShowWindow( ghKwScroll, nCmd )
    if bShow then frmOptionsKeywords_TextScrollChanged( ghKwText(idx) )
end sub


' ========================================================================================
' Lay the visible page's editor out to fill the viewport, with the shared scrollbar in a
' strip on its right. Returns the viewport height, so the page exactly fills the pane and
' the outer PsScrollPanel never scrolls -- the editor scrolls its own text instead.
' ========================================================================================
function frmOptionsKeywords_Layout( byval nPage as long, byval hPage as HWND, byval cxPanel as long ) as long

    dim as long idx = frmOptionsKeywords_Index( nPage )
    if idx < 0 then return 0
    if ghKwText(idx) = 0 then return 0

    dim as long padLeft   = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight  = AfxScaleX(OPTROW_PADRIGHT)
    dim as long padTop    = AfxScaleY(OPTROW_PADTOP)
    dim as long padBottom = AfxScaleY(OPTROW_PADBOTTOM)
    dim as long sbW       = AfxScaleX(SCROLLBAR_WIDTH_PANEL)
    dim as long gap       = AfxScaleX(4)

    dim as RECT rcVP : GetClientRect( HWND_FRMOPTIONS_PAGE, @rcVP )
    dim as long viewH = rcVP.bottom - rcVP.top
    if viewH <= 0 then viewH = AfxScaleY(FRMOPTIONS_CLIENT_H - FRMOPTIONS_TITLE_H - FRMOPTIONS_FOOTER_H)

    dim as long tbX = padLeft
    dim as long tbY = padTop
    dim as long tbH = viewH - padTop - padBottom
    if tbH < 0 then tbH = 0
    dim as long tbW = cxPanel - padRight - padLeft - sbW - gap
    if tbW < 0 then tbW = 0

    SetWindowPos( ghKwText(idx), 0, tbX, tbY, tbW, tbH, SWP_NOZORDER or SWP_NOACTIVATE )
    if ghKwScroll then
        SetWindowPos( ghKwScroll, 0, tbX + tbW + gap, tbY, sbW, tbH, SWP_NOZORDER or SWP_NOACTIVATE )
    end if

    ' Sync the scrollbar to the freshly-sized editor.
    frmOptionsKeywords_TextScrollChanged( ghKwText(idx) )

    function = viewH
end function


' ========================================================================================
' No drawn labels: the editor fills the page. Kept for dispatch symmetry.
' ========================================================================================
sub frmOptionsKeywords_Paint( byval nPage as long, byval p as SCP_PAINTINFO ptr )
end sub


' ========================================================================================
' Harvest the built editors into gConfig. Called from OK, gated on bKeywordsDirty by the
' caller. A page never visited has a 0 handle and keeps its existing gConfig value.
' ========================================================================================
sub frmOptionsKeywords_Harvest()
    ' Guard on IsWindow as well as the handle: harvesting a destroyed editor would read ""
    ' and blank a good keyword file. A live editor always reads back its real text.
    if (ghKwText(0) <> 0) andalso (IsWindow(ghKwText(0)) <> 0) then gConfig.FBKeywords     = PsTextBox_GetText( ghKwText(0) )
    if (ghKwText(1) <> 0) andalso (IsWindow(ghKwText(1)) <> 0) then gConfig.WinApiKeywords = PsTextBox_GetText( ghKwText(1) )
    if (ghKwText(2) <> 0) andalso (IsWindow(ghKwText(2)) <> 0) then gConfig.ExtraKeywords  = PsTextBox_GetText( ghKwText(2) )
end sub
