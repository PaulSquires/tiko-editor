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

'  frmOptionsColors.inc  --  the "Themes and Fonts" page (see frmOptionsColors.bi).

#include once "frmOptionsColors.bi"
#include once "frmOptions.bi"          ' HWND_FRMOPTIONS
#include once "modOptionsRows.bi"      ' gOptWork, shared padding + theme helpers
#include once "clsConfig.bi"


#define COLORS_LISTHEIGHT_THEMES  130
#define COLORS_LISTHEIGHT_FONTS   160
#define COLORS_NUMWIDTH            96
#define COLORS_COMBOWIDTH         180
#define COLORS_SECTIONGAP          14
#define COLORS_LABELGAP             4
#define COLORS_ROWHEIGHT           40


type COLORS_LABEL
    rc      as RECT
    caption as DWSTRING
end type


dim shared ghColorThemes   as HWND      ' CListBox
dim shared ghColorFonts    as HWND      ' CListBox (per-font preview)
dim shared ghColorSize     as HWND      ' CComboBox
dim shared ghColorCharset  as HWND      ' CComboBox
dim shared ghColorSpacing  as HWND      ' CNumericUpDown

dim shared gColorFontNames(any) as DWSTRING     ' collected by the EnumFontFamilies callback
dim shared gColorLabels(any)    as COLORS_LABEL
dim shared gColorBuilt as boolean


' ========================================================================================
' GetThemeDescription -- the theme file's general.description, or its filename. (Unchanged
' standalone helper carried over from the old page.)
' ========================================================================================
function GetThemeDescription( byval wszFilename as DWSTRING ) as string
    ' default to the theme description being the filename
    function = AfxGetFileName(wszFilename)

    dim pStream as CTextStream
    if pStream.Open(wszFilename) <> S_OK then exit function  'error

    dim as DWSTRING wst, keyValue, keyData

    do until pStream.EOS
        wst = pStream.ReadLine
        wst = trim(AfxStrExtract( 1, wst, "#"))     ' remove comments
        if len(wst) = 0 then continue Do
        keyValue = trim(AfxStrParse(wst, 1, ":"))
        keyData = trim(AfxStrParse(wst, 2, ":"))
        if keyValue = "general.description" then
            function = keyData
            exit do
        end if
    loop

    pStream.Close
end function


' ========================================================================================
' EnumFontFamilies callback: collect every (non-vertical) face name into gColorFontNames.
' ========================================================================================
private function frmOptionsColors_EnumFontProc( _
            byref lf as LOGFONTW, _
            byref tm as TEXTMETRIC, _
            byval FontType as long, _
            byval lParam as LPARAM _
            ) as long
    dim as DWSTRING nm = lf.lfFaceName
    ' Skip the "@Name" vertical-writing faces; the old owner-drawn combo showed them, but
    ' they are never wanted as an editor font and only clutter the list.
    if (len(nm) = 0) orelse (left(nm, 1) = "@") then return 1
    dim as long idx = ubound(gColorFontNames) + 1
    redim preserve gColorFontNames(idx) as DWSTRING
    gColorFontNames(idx) = nm
    function = 1
end function


' ========================================================================================
' Fill the size CComboBox for a given font. (Same size sets as the old page.)
' ========================================================================================
private sub frmOptionsColors_FillSizes( byval hCombo as HWND, byval wszFont as DWSTRING )
    CComboBox_Clear( hCombo )
    select case ucase(wszFont)
        case "FIXEDSYS"
            CComboBox_AddItem( hCombo, "9" )
        case "TERMINAL"
            CComboBox_AddItem( hCombo, "5" ) : CComboBox_AddItem( hCombo, "6" )
            CComboBox_AddItem( hCombo, "12" ): CComboBox_AddItem( hCombo, "14" )
        case else
            dim as string sizes = "8 9 10 11 12 14 16 18 20 22 24 26 28 32 36"
            for i as long = 1 to AfxStrParseCount( sizes, " " )
                CComboBox_AddItem( hCombo, AfxStrParse( sizes, i, " " ) )
            next
    end select
end sub


private sub frmOptionsColors_FillCharsets( byval hCombo as HWND )
    dim as string cs = "Default,Ansi,Arabic,Baltic,Chinese Big 5,East Europe,GB 2312,Greek," & _
                       "Hangul,Hebrew,Johab,Mac,OEM,Russian,Shiftjis,Symbol,Thai,Turkish,Vietnamese"
    for i as long = 1 to AfxStrParseCount( cs, "," )
        CComboBox_AddItem( hCombo, AfxStrParse( cs, i, "," ) )
    next
end sub


' ========================================================================================
' Panel hooks.
' ========================================================================================
function frmOptionsColors_OwnsPage( byval nPage as long ) as boolean
    function = iif( nPage = OPTPAGE_COLORS, true, false )
end function

sub frmOptionsColors_Reset()
    gColorBuilt    = false
    ghColorThemes  = 0
    ghColorFonts   = 0
    ghColorSize    = 0
    ghColorCharset = 0
    ghColorSpacing = 0
    erase gColorFontNames
    erase gColorLabels
end sub


' ========================================================================================
' Selection callbacks (user action only). Themes/fonts store into gOptWork.
' ========================================================================================
private sub frmOptionsColors_ThemeSelChange( byval hList as HWND, byval row as integer )
    if (row >= lbound(gThemeFilenames)) andalso (row <= ubound(gThemeFilenames)) then
        gOptWork.ThemeShortFilename = gThemeFilenames(row)
    end if
end sub

private sub frmOptionsColors_FontSelChange( byval hList as HWND, byval row as integer )
    if row < 0 then exit sub
    gOptWork.EditorFontname = CListBox_GetText( hList, row )
end sub

private sub frmOptionsColors_SizeSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if idxNew < 0 then exit sub
    gOptWork.EditorFontSize = CComboBox_GetItemText( hCombo, idxNew )
end sub

private sub frmOptionsColors_CharsetSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if idxNew < 0 then exit sub
    gOptWork.EditorFontCharSet = CComboBox_GetItemText( hCombo, idxNew )
end sub

private sub frmOptionsColors_SpacingChanged( byval hCtrl as HWND, byval nValue as double )
    gOptWork.FontExtraSpace = clng( nValue )
end sub


' ========================================================================================
' Paint one theme row (plain text in the base font).
' ========================================================================================
private sub frmOptionsColors_ThemePaint( byval p as CLISTBOX_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = ghPanel.BackColorHot : foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor : foreclr = ghPanel.ForeColor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( OptionsFont_Base() )
    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(8)
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
end sub


' ========================================================================================
' Paint one font row -- the face name drawn IN that face. A font is created per row per
' repaint (only visible rows paint) and freed after being deselected, exactly as the old
' owner-drawn combo did.
' ========================================================================================
private sub frmOptionsColors_FontPaint( byval p as CLISTBOX_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = ghPanel.BackColorHot : foreclr = ghPanel.ForeColorHot
    else
        backclr = ghPanel.BackColor : foreclr = ghPanel.ForeColor
    end if
    p->b->SetBackColor( backclr )
    p->b->SetForeColor( foreclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += AfxScaleX(8)

    dim pWin as CWindow ptr = AfxCWindowPtr( HWND_FRMOPTIONS )
    dim as HFONT hFont = 0
    if pWin then hFont = pWin->CreateFont( p->wszCaption, OptionsFont_PointSize() )
    if hFont then
        p->b->SetFont( hFont )
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
        ' Deselect before deleting: the buffer's DC must not hold the font we free.
        p->b->SetFont( OptionsFont_Base() )
        DeleteObject( hFont )
    else
        p->b->SetFont( OptionsFont_Base() )
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    end if
end sub


' ========================================================================================
' Load the themes list (description text; gThemeFilenames holds the parallel filenames).
' Returns the row matching the stored theme, or -1.
' ========================================================================================
private function frmOptionsColors_LoadThemes() as long
    CListBox_BeginUpdate( ghColorThemes )
    CListBox_Clear( ghColorThemes )
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
                idx = CListBox_AddString( ghColorThemes, wszDisplay )
                redim preserve gThemeFilenames(idx) as DWSTRING
                gThemeFilenames(idx) = WFD.cFileName
                if ucase(gOptWork.ThemeShortFilename) = ucase(WFD.cFileName) then idxMatch = idx
            end if
        loop while FindNextFile( hSearch, @WFD )
        FindClose( hSearch )
    end if

    CListBox_EndUpdate( ghColorThemes )
    function = idxMatch
end function


' ========================================================================================
' Load the fonts list: enumerate, sort case-insensitively, drop consecutive duplicates.
' Returns the row matching the stored font, or -1.
' ========================================================================================
private function frmOptionsColors_LoadFonts() as long

    erase gColorFontNames
    dim hDC as HDC = GetDC(0)
    EnumFontFamilies( hDC, byval 0, cast(FONTENUMPROCW, @frmOptionsColors_EnumFontProc), 0 )
    ReleaseDC( 0, hDC )

    ' Insertion sort (case-insensitive). A few hundred names, once per dialog session.
    for i as long = lbound(gColorFontNames) + 1 to ubound(gColorFontNames)
        dim as DWSTRING key = gColorFontNames(i)
        dim as DWSTRING keyU = ucase(key)
        dim as long j = i - 1
        do while (j >= lbound(gColorFontNames)) andalso (ucase(gColorFontNames(j)) > keyU)
            gColorFontNames(j+1) = gColorFontNames(j)
            j -= 1
        loop
        gColorFontNames(j+1) = key
    next

    dim as long idxMatch = -1
    dim as DWSTRING wszWanted = ucase( gOptWork.EditorFontname )
    dim as DWSTRING wszPrev = ""

    CListBox_BeginUpdate( ghColorFonts )
    CListBox_Clear( ghColorFonts )
    for i as long = lbound(gColorFontNames) to ubound(gColorFontNames)
        if ucase(gColorFontNames(i)) = ucase(wszPrev) then continue for   ' drop duplicate
        dim as long r = CListBox_AddString( ghColorFonts, gColorFontNames(i) )
        if ucase(gColorFontNames(i)) = wszWanted then idxMatch = r
        wszPrev = gColorFontNames(i)
    next
    CListBox_EndUpdate( ghColorFonts )

    function = idxMatch
end function


' ========================================================================================
' Build the controls onto hPage. Idempotent.
' ========================================================================================
sub frmOptionsColors_Build( byval hPage as HWND )

    if gColorBuilt then exit sub

    ' Themes list.
    ghColorThemes = CListBox_Create( hPage, IDC_FRMOPTIONSCOLORS_LSTTHEMES )
    CListBox_SetPaintCallback( ghColorThemes, @frmOptionsColors_ThemePaint )
    CListBox_SetSelChangeCallback( ghColorThemes, @frmOptionsColors_ThemeSelChange )
    CListBox_SetFont( ghColorThemes, OptionsFont_Base() )
    CListBox_SetScrollBarWidth( ghColorThemes, SCROLLBAR_WIDTH_PANEL )

    ' Fonts list (per-font preview).
    ghColorFonts = CListBox_Create( hPage, IDC_FRMOPTIONSCOLORS_LSTFONTS )
    CListBox_SetPaintCallback( ghColorFonts, @frmOptionsColors_FontPaint )
    CListBox_SetSelChangeCallback( ghColorFonts, @frmOptionsColors_FontSelChange )
    CListBox_SetFont( ghColorFonts, OptionsFont_Base() )
    CListBox_SetScrollBarWidth( ghColorFonts, SCROLLBAR_WIDTH_PANEL )

    ' Size and character-set combos.
    ghColorSize = CComboBox_Create( hPage, IDC_FRMOPTIONSCOLORS_COMBOSIZE )
    CComboBox_SetFont( ghColorSize, OptionsFont_Base() )
    CComboBox_SetSelChangeCallback( ghColorSize, @frmOptionsColors_SizeSelChange )
    OptionsRows_RegisterCombo( ghColorSize )

    ghColorCharset = CComboBox_Create( hPage, IDC_FRMOPTIONSCOLORS_COMBOCHARSET )
    CComboBox_SetFont( ghColorCharset, OptionsFont_Base() )
    CComboBox_SetSelChangeCallback( ghColorCharset, @frmOptionsColors_CharsetSelChange )
    OptionsRows_RegisterCombo( ghColorCharset )

    ' Extra line spacing.
    ghColorSpacing = CNumericUpDown_Create( hPage, IDC_FRMOPTIONSCOLORS_NUMSPACING )
    CNumericUpDown_SetFont( ghColorSpacing, OptionsFont_Base() )
    CNumericUpDown_SetDecimalPlaces( ghColorSpacing, 0 )
    CNumericUpDown_SetRange( ghColorSpacing, 0, 20 )
    CNumericUpDown_SetValue( ghColorSpacing, gOptWork.FontExtraSpace )
    CNumericUpDown_SetValueChangedCallback( ghColorSpacing, @frmOptionsColors_SpacingChanged )
    gOptWork.FontExtraSpace = clng( CNumericUpDown_GetValue( ghColorSpacing ) )   ' take the clamp back

    ' ---- populate lists and seed selections ------------------------------------------
    dim as long idxTheme = frmOptionsColors_LoadThemes()
    if idxTheme >= 0 then
        CListBox_SetCurSel( ghColorThemes, idxTheme )
        CListBox_SetTopIndex( ghColorThemes, idxTheme )
    end if

    dim as long idxFont = frmOptionsColors_LoadFonts()
    if idxFont >= 0 then
        CListBox_SetCurSel( ghColorFonts, idxFont )
        CListBox_SetTopIndex( ghColorFonts, idxFont )
    end if

    ' Size list depends on the font; filled once (matching the old page, which did not
    ' refresh it when the font changed).
    frmOptionsColors_FillSizes( ghColorSize, gOptWork.EditorFontname )
    dim as long idxSize = CComboBox_FindItemByText( ghColorSize, gOptWork.EditorFontSize )
    if idxSize >= 0 then CComboBox_SetCurSel( ghColorSize, idxSize )

    frmOptionsColors_FillCharsets( ghColorCharset )
    dim as long idxCs = CComboBox_FindItemByText( ghColorCharset, gOptWork.EditorFontCharSet )
    if idxCs >= 0 then CComboBox_SetCurSel( ghColorCharset, idxCs )

    gColorBuilt = true
    frmOptionsColors_ApplyTheme()
end sub


' ========================================================================================
' Theme the controls.
' ========================================================================================
sub frmOptionsColors_ApplyTheme()
    if gColorBuilt = false then exit sub

    for i as long = 0 to 1
        dim as HWND h = iif( i = 0, ghColorThemes, ghColorFonts )
        CListBox_SetBackColor( h, ghPanel.BackColor )
        CListBox_SetScrollBarColors( h, ghPanel.BackColorScrollBar, _
                                     ghPanel.ForeColorScrollBar, ghPanel.ForeColorScrollBar )
    next

    dim as CCOMBOBOX_COLORS cc
    dim as CPOPUPMENU_COLORS pc
    OptionsTheme_FillCombo( @cc, @pc )
    for i as long = 0 to 1
        dim as HWND h = iif( i = 0, ghColorSize, ghColorCharset )
        CComboBox_SetColors( h, @cc )
        CComboBox_SetListColors( h, @pc )
        CComboBox_Refresh( h )
    next

    dim as CNUMERICUPDOWN_COLORS nc
    OptionsTheme_FillNumeric( @nc )
    CNumericUpDown_SetColors( ghColorSpacing, @nc )
    CNumericUpDown_Refresh( ghColorSpacing )
end sub


' ========================================================================================
' Show / hide every control.
' ========================================================================================
sub frmOptionsColors_Show( byval bShow as boolean )
    if gColorBuilt = false then exit sub
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghColorThemes, nCmd )
    ShowWindow( ghColorFonts, nCmd )
    ShowWindow( ghColorSize, nCmd )
    ShowWindow( ghColorCharset, nCmd )
    ShowWindow( ghColorSpacing, nCmd )
end sub


' ========================================================================================
' Record one drawn label.
' ========================================================================================
private sub frmOptionsColors_AddLabel( byval wszCaption as DWSTRING, _
                                       byval nLeft as long, byval nTop as long, _
                                       byval nRight as long, byval nBottom as long )
    dim as long idx = ubound(gColorLabels) + 1
    redim preserve gColorLabels(idx) as COLORS_LABEL
    gColorLabels(idx).rc = type<RECT>( nLeft, nTop, nRight, nBottom )
    gColorLabels(idx).caption = wszCaption
end sub


' ========================================================================================
' Lay out; return content height.
' ========================================================================================
function frmOptionsColors_Layout( byval hPage as HWND, byval cxPanel as long ) as long

    if gColorBuilt = false then return 0

    dim as long padLeft  = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight = AfxScaleX(OPTROW_PADRIGHT)
    dim as long labelGap = AfxScaleX(OPTROW_LABELGAP)
    dim as long contentR = cxPanel - padRight
    dim as long fullW    = contentR - padLeft
    if fullW < 0 then fullW = 0

    dim as long textH = 0
    dim as HDC hDC = GetDC( hPage )
    if hDC then
        dim as HFONT hOld = SelectObject( hDC, OptionsFont_Base() )
        dim as TEXTMETRICW tm : GetTextMetricsW( hDC, @tm )
        SelectObject( hDC, hOld )
        ReleaseDC( hPage, hDC )
        textH = tm.tmHeight
    end if

    dim as long labelGapY  = AfxScaleY(COLORS_LABELGAP)
    dim as long sectionGap = AfxScaleY(COLORS_SECTIONGAP)
    dim as long y = AfxScaleY(OPTROW_PADTOP)

    erase gColorLabels

    ' Themes.
    frmOptionsColors_AddLabel( L(422,"Themes") & ":", padLeft, y, contentR, y + textH )
    y += textH + labelGapY
    SetWindowPos( ghColorThemes, 0, padLeft, y, fullW, AfxScaleY(COLORS_LISTHEIGHT_THEMES), SWP_NOZORDER or SWP_NOACTIVATE )
    y += AfxScaleY(COLORS_LISTHEIGHT_THEMES) + sectionGap

    ' Fonts.
    frmOptionsColors_AddLabel( L(136,"Font (applies to all styles)"), padLeft, y, contentR, y + textH )
    y += textH + labelGapY
    SetWindowPos( ghColorFonts, 0, padLeft, y, fullW, AfxScaleY(COLORS_LISTHEIGHT_FONTS), SWP_NOZORDER or SWP_NOACTIVATE )
    y += AfxScaleY(COLORS_LISTHEIGHT_FONTS) + sectionGap

    ' A control right-aligned on a labelled row (label left, control right).
    #macro CTLROW( hCtl, cap, ctlW, ctlH )
        scope
            dim as long rowH = AfxScaleY(COLORS_ROWHEIGHT)
            if (ctlH) + AfxScaleY(8) > rowH then rowH = (ctlH) + AfxScaleY(8)
            if textH + AfxScaleY(8) > rowH then rowH = textH + AfxScaleY(8)
            dim as long cx = contentR - (ctlW)
            dim as long cy = y + (rowH - (ctlH)) \ 2
            SetWindowPos( (hCtl), 0, cx, cy, (ctlW), (ctlH), SWP_NOZORDER or SWP_NOACTIVATE )
            dim as long lr = cx - labelGap
            if lr < padLeft then lr = padLeft
            frmOptionsColors_AddLabel( (cap), padLeft, y, lr, y + rowH )
            y += rowH
        end scope
    #endmacro

    dim as long cw, ch
    CComboBox_GetIdealSize( ghColorSize, cw, ch )
    if cw < AfxScaleX(COLORS_COMBOWIDTH) then cw = AfxScaleX(COLORS_COMBOWIDTH)
    CTLROW( ghColorSize, L(450,"Size"), cw, ch )

    CComboBox_GetIdealSize( ghColorCharset, cw, ch )
    if cw < AfxScaleX(COLORS_COMBOWIDTH) then cw = AfxScaleX(COLORS_COMBOWIDTH)
    CTLROW( ghColorCharset, L(451,"Character Set"), cw, ch )

    CNumericUpDown_GetIdealSize( ghColorSpacing, cw, ch )
    if cw < AfxScaleX(COLORS_NUMWIDTH) then cw = AfxScaleX(COLORS_NUMWIDTH)
    CTLROW( ghColorSpacing, L(421,"Extra line spacing"), cw, ch )

    function = y + AfxScaleY(OPTROW_PADBOTTOM)
end function


' ========================================================================================
' Paint the drawn labels.
' ========================================================================================
sub frmOptionsColors_Paint( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    if gColorBuilt = false then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( ghPanel.BackColor )
    p->b->SetForeColor( ghPanel.ForeColor )
    for i as long = lbound(gColorLabels) to ubound(gColorLabels)
        dim as RECT rc = gColorLabels(i).rc
        p->b->PaintText( gColorLabels(i).caption, @rc, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub
