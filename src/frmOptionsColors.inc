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

'  frmOptionsColors.inc  --  the "Editor Font" page (see frmOptionsColors.bi).

#include once "frmOptionsColors.bi"
#include once "frmOptions.bi"          ' HWND_FRMOPTIONS
#include once "modOptionsRows.bi"      ' gOptWork, shared padding + theme helpers
#include once "clsConfig.bi"


' MEASURED AGAINST THE FIXED VIEWPORT. The page needed 928px of the 892 available at 175%, so
' 21 unscaled px had to go. It comes out of the THEMES list rather than the fonts list, which is
' the whole of the judgement here: a handful of .theme files ship and the folder is rarely long,
' while the font list is every family on the machine and is the one people actually scroll.
#define COLORS_LISTHEIGHT_THEMES  104
#define COLORS_LISTHEIGHT_FONTS   160
#define COLORS_NUMWIDTH            96
#define COLORS_COMBOWIDTH         180
#define COLORS_SECTIONGAP          11      ' x2 on this page
#define COLORS_LABELGAP             4
#define COLORS_ROWHEIGHT           40




dim shared ghColorFonts    as HWND      ' PsListTree (per-font preview)
dim shared ghColorSize     as HWND      ' PsComboBox
dim shared ghColorCharset  as HWND      ' PsComboBox
dim shared ghColorSpacing  as HWND      ' PsNumericUpDown
dim shared ghColorTabSize  as HWND      ' PsNumericUpDown -- moved here from the Code Editor page
dim shared ghColorCase     as HWND      ' PsComboBox      -- moved here from the Code Editor page

dim shared gColorFontNames as DWSTRINGLIST     ' collected by the EnumFontFamilies callback
dim shared gColorLabels as PAGELABELLIST
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
    gColorFontNames.Push( nm )
    function = 1
end function


' ========================================================================================
' Fill the size PsComboBox for a given font. (Same size sets as the old page.)
' ========================================================================================
private sub frmOptionsColors_FillSizes( byval hCombo as HWND, byval wszFont as DWSTRING )
    PsComboBox_Clear( hCombo )
    select case ucase(wszFont)
        case "FIXEDSYS"
            PsComboBox_AddItem( hCombo, "9" )
        case "TERMINAL"
            PsComboBox_AddItem( hCombo, "5" ) : PsComboBox_AddItem( hCombo, "6" )
            PsComboBox_AddItem( hCombo, "12" ): PsComboBox_AddItem( hCombo, "14" )
        case else
            dim as string sizes = "8 9 10 11 12 14 16 18 20 22 24 26 28 32 36"
            for i as long = 1 to AfxStrParseCount( sizes, " " )
                PsComboBox_AddItem( hCombo, AfxStrParse( sizes, i, " " ) )
            next
    end select
end sub


private sub frmOptionsColors_FillCharsets( byval hCombo as HWND )
    dim as string cs = "Default,Ansi,Arabic,Baltic,Chinese Big 5,East Europe,GB 2312,Greek," & _
                       "Hangul,Hebrew,Johab,Mac,OEM,Russian,Shiftjis,Symbol,Thai,Turkish,Vietnamese"
    for i as long = 1 to AfxStrParseCount( cs, "," )
        PsComboBox_AddItem( hCombo, AfxStrParse( cs, i, "," ) )
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
    ghColorFonts   = 0
    ghColorSize    = 0
    ghColorCharset = 0
    ghColorSpacing = 0
    ghColorTabSize = 0
    ghColorCase    = 0
    gColorFontNames.Clear( )
    gColorLabels.Clear( )
end sub


' ========================================================================================
' Selection callbacks (user action only). Themes/fonts store into gOptWork.
' ========================================================================================

private sub frmOptionsColors_FontSelChange( byval hList as HWND, byval row as integer )
    if row < 0 then exit sub
    gOptWork.EditorFontname = PsListTree_GetText( hList, row )
end sub

private sub frmOptionsColors_SizeSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if idxNew < 0 then exit sub
    gOptWork.EditorFontSize = PsComboBox_GetItemText( hCombo, idxNew )
end sub

private sub frmOptionsColors_CharsetSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if idxNew < 0 then exit sub
    gOptWork.EditorFontCharSet = PsComboBox_GetItemText( hCombo, idxNew )
end sub

' Tab Size and Case were table-driven rows on the Code Editor page; here they are native
' controls, so their callbacks write the staging fields by hand -- the same longs the row
' machinery wrote, so OptionsWork_Commit is unchanged.
private sub frmOptionsColors_TabSizeChanged( byval hCtrl as HWND, byval nValue as double )
    gOptWork.TabSize = clng( nValue )
end sub

private sub frmOptionsColors_CaseSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    if idxNew < 0 then exit sub
    ' The COMBO stores the INDEX (0 Lower / 1 Upper / 2 Mixed / 3 Original), not the text --
    ' KeywordCase is a long, unlike the Size/Charset combos which store their text.
    gOptWork.KeywordCase = idxNew
end sub

private sub frmOptionsColors_SpacingChanged( byval hCtrl as HWND, byval nValue as double )
    gOptWork.FontExtraSpace = clng( nValue )
end sub


' ========================================================================================
' Paint one theme row (plain text in the base font).
' ========================================================================================


' ========================================================================================
' Paint one font row -- the face name drawn IN that face. A font is created per row per
' repaint (only visible rows paint) and freed after being deselected, exactly as the old
' owner-drawn combo did.
' ========================================================================================
private sub frmOptionsColors_FontPaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as COLORREF backclr, foreclr
    if p->isSelected orelse p->isHot then
        backclr = theme.panel.texthot.backcolor : foreclr = theme.panel.texthot.forecolor
    else
        ' Idle rows match the nav panel's lighter shade (see frmOptions nav list).
        backclr = theme.toptabs.texthot.backcolor : foreclr = theme.panel.text.forecolor
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
' Re-read the themes folder and reselect whatever gOptWork now names. Called by the colour
' pages after they fork a built-in theme, which drops a new file into that folder while this
' page may already be built. Harmless before the page exists.
' ========================================================================================



' ========================================================================================
' Load the themes list (description text; gThemeFilenames holds the parallel filenames).
' Returns the row matching the stored theme, or -1.
' ========================================================================================


' ========================================================================================
' Load the fonts list: enumerate, sort case-insensitively, drop consecutive duplicates.
' Returns the row matching the stored font, or -1.
' ========================================================================================
private function frmOptionsColors_LoadFonts() as long

    gColorFontNames.Clear( )
    dim hDC as HDC = GetDC(0)
    EnumFontFamilies( hDC, byval 0, cast(FONTENUMPROCW, @frmOptionsColors_EnumFontProc), 0 )
    ReleaseDC( 0, hDC )

    ' Case-insensitive, and STABLE so that two faces differing only in case keep the order
    ' the enumeration reported them in -- the duplicate drop below relies on equal names
    ' being adjacent, not on which of them survives.
    '
    ' This was an eighth hand-written sort, an insertion sort missed by the Phase 3 sweep
    ' because it sits inline in a loader rather than in a named Sort/Compare procedure.
    FB.StableSortBy( gColorFontNames, _
        function( byref a as DWSTRING, byref b as DWSTRING ) as boolean
            return ucase( a ) < ucase( b )
        end function )

    dim as long idxMatch = -1
    dim as DWSTRING wszWanted = ucase( gOptWork.EditorFontname )
    dim as DWSTRING wszPrev = ""

    PsListTree_BeginUpdate( ghColorFonts )
    PsListTree_Clear( ghColorFonts )
    for each wszFace in gColorFontNames
        if ucase(wszFace) = ucase(wszPrev) then continue for   ' drop duplicate
        dim as long r = PsListTree_AddString( ghColorFonts, wszFace )
        if ucase(wszFace) = wszWanted then idxMatch = r
        wszPrev = wszFace
    next
    PsListTree_EndUpdate( ghColorFonts )

    function = idxMatch
end function


' ========================================================================================
' Build the controls onto hPage. Idempotent.
' ========================================================================================
sub frmOptionsColors_Build( byval hPage as HWND )

    if gColorBuilt then exit sub

    ' Themes list.

    ' Fonts list (per-font preview).
    ghColorFonts = PsListTree_Create( hPage, IDC_FRMOPTIONSCOLORS_LSTFONTS )
    PsListTree_SetPaintCallback( ghColorFonts, @frmOptionsColors_FontPaint )
    PsListTree_SetSelChangeCallback( ghColorFonts, @frmOptionsColors_FontSelChange )
    PsListTree_SetFont( ghColorFonts, OptionsFont_Base() )
    PsListTree_SetScrollBarWidth( ghColorFonts, SCROLLBAR_WIDTH_PANEL )

    ' Size and character-set combos.
    ghColorSize = PsComboBox_Create( hPage, IDC_FRMOPTIONSCOLORS_COMBOSIZE )
    PsComboBox_SetFont( ghColorSize, OptionsFont_Base() )
    PsComboBox_SetSelChangeCallback( ghColorSize, @frmOptionsColors_SizeSelChange )
    OptionsRows_RegisterCombo( ghColorSize )

    ghColorCharset = PsComboBox_Create( hPage, IDC_FRMOPTIONSCOLORS_COMBOCHARSET )
    PsComboBox_SetFont( ghColorCharset, OptionsFont_Base() )
    PsComboBox_SetSelChangeCallback( ghColorCharset, @frmOptionsColors_CharsetSelChange )
    OptionsRows_RegisterCombo( ghColorCharset )

    ' Extra line spacing.
    ghColorSpacing = PsNumericUpDown_Create( hPage, IDC_FRMOPTIONSCOLORS_NUMSPACING )
    PsNumericUpDown_SetFont( ghColorSpacing, OptionsFont_Base() )
    PsNumericUpDown_SetDecimalPlaces( ghColorSpacing, 0 )
    PsNumericUpDown_SetRange( ghColorSpacing, 0, 20 )
    PsNumericUpDown_SetValue( ghColorSpacing, gOptWork.FontExtraSpace )
    PsNumericUpDown_SetValueChangedCallback( ghColorSpacing, @frmOptionsColors_SpacingChanged )
    gOptWork.FontExtraSpace = clng( PsNumericUpDown_GetValue( ghColorSpacing ) )   ' take the clamp back

    ' Tab Size -- range and decimal places match the old OPT_NUMERIC row (1..64, integer).
    ghColorTabSize = PsNumericUpDown_Create( hPage, IDC_FRMOPTIONSCOLORS_NUMTABSIZE )
    PsNumericUpDown_SetFont( ghColorTabSize, OptionsFont_Base() )
    PsNumericUpDown_SetDecimalPlaces( ghColorTabSize, 0 )
    PsNumericUpDown_SetRange( ghColorTabSize, 1, 64 )
    PsNumericUpDown_SetValue( ghColorTabSize, gOptWork.TabSize )
    PsNumericUpDown_SetValueChangedCallback( ghColorTabSize, @frmOptionsColors_TabSizeChanged )
    gOptWork.TabSize = clng( PsNumericUpDown_GetValue( ghColorTabSize ) )   ' take the clamp back

    ' Case -- the four keyword-case choices, in the order the old row listed them so the stored
    ' index still means the same thing.
    ghColorCase = PsComboBox_Create( hPage, IDC_FRMOPTIONSCOLORS_COMBOCASE )
    PsComboBox_SetFont( ghColorCase, OptionsFont_Base() )
    PsComboBox_AddItem( ghColorCase, L(125,"Lower Case") )
    PsComboBox_AddItem( ghColorCase, L(126,"Upper Case") )
    PsComboBox_AddItem( ghColorCase, L(210,"Proper Case") )
    PsComboBox_AddItem( ghColorCase, L(127,"Original Case") )
    PsComboBox_SetCurSel( ghColorCase, gOptWork.KeywordCase )
    PsComboBox_SetSelChangeCallback( ghColorCase, @frmOptionsColors_CaseSelChange )
    OptionsRows_RegisterCombo( ghColorCase )

    ' ---- populate lists and seed selections ------------------------------------------
    ' NO THEMES LIST ANY MORE. It moved to frmThemes, which owns everything about colour;
    ' this page is fonts.
    dim as long idxFont = frmOptionsColors_LoadFonts()
    if idxFont >= 0 then
        PsListTree_SetCurSel( ghColorFonts, idxFont )
        PsListTree_SetTopIndex( ghColorFonts, idxFont )
    end if

    ' Size list depends on the font; filled once (matching the old page, which did not
    ' refresh it when the font changed).
    frmOptionsColors_FillSizes( ghColorSize, gOptWork.EditorFontname )
    dim as long idxSize = PsComboBox_FindItemByText( ghColorSize, gOptWork.EditorFontSize )
    if idxSize >= 0 then PsComboBox_SetCurSel( ghColorSize, idxSize )

    frmOptionsColors_FillCharsets( ghColorCharset )
    dim as long idxCs = PsComboBox_FindItemByText( ghColorCharset, gOptWork.EditorFontCharSet )
    if idxCs >= 0 then PsComboBox_SetCurSel( ghColorCharset, idxCs )

    gColorBuilt = true
    frmOptionsColors_ApplyTheme()
end sub


' ========================================================================================
' Theme the controls.
' ========================================================================================
sub frmOptionsColors_ApplyTheme()
    if gColorBuilt = false then exit sub

    PsListTree_SetBackColor( ghColorFonts, theme.toptabs.texthot.backcolor )
    OptionsTheme_ApplyListBorder( ghColorFonts )
    PsListTree_SetScrollBarColors( ghColorFonts, theme.panel.scrollbar.backcolor, _
                                   theme.panel.scrollbar.forecolor, theme.panel.scrollbar.forecolor )

    dim as PSCOMBOBOX_COLORS cc
    dim as PSPOPUPMENU_COLORS pc
    OptionsTheme_FillCombo( @cc, @pc )
    dim as HWND hCombos(0 to 2) = { ghColorSize, ghColorCharset, ghColorCase }
    for i as long = 0 to 2
        PsComboBox_SetColors( hCombos(i), @cc )
        PsComboBox_SetListColors( hCombos(i), @pc )
        PsComboBox_Refresh( hCombos(i) )
    next

    dim as PSNUMERICUPDOWN_COLORS nc
    OptionsTheme_FillNumeric( @nc )
    PsNumericUpDown_SetColors( ghColorSpacing, @nc )
    PsNumericUpDown_Refresh( ghColorSpacing )
    PsNumericUpDown_SetColors( ghColorTabSize, @nc )
    PsNumericUpDown_Refresh( ghColorTabSize )
end sub


' ========================================================================================
' Show / hide every control.
' ========================================================================================
sub frmOptionsColors_Show( byval bShow as boolean )
    if gColorBuilt = false then exit sub
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    ShowWindow( ghColorFonts, nCmd )
    ShowWindow( ghColorSize, nCmd )
    ShowWindow( ghColorCharset, nCmd )
    ShowWindow( ghColorSpacing, nCmd )
    ShowWindow( ghColorTabSize, nCmd )
    ShowWindow( ghColorCase, nCmd )
end sub


' ========================================================================================
' Record one drawn label.
' ========================================================================================
private sub frmOptionsColors_AddLabel( byval wszCaption as DWSTRING, _
                                       byval nLeft as long, byval nTop as long, _
                                       byval nRight as long, byval nBottom as long )
    AddPageLabel( gColorLabels, wszCaption, nLeft, nTop, nRight, nBottom )
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

    gColorLabels.Clear( )

    ' THE THEMES LIST IS GONE (it moved to frmThemes). The label and the space it reserved went
    ' with it, so the Font section is now the first thing on the page -- which is why the page
    ' was renamed "Editor Font".
    ' Fonts.
    frmOptionsColors_AddLabel( L(129,"Font (applies to all styles)"), padLeft, y, contentR, y + textH )
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
    PsComboBox_GetIdealSize( ghColorSize, cw, ch )
    if cw < AfxScaleX(COLORS_COMBOWIDTH) then cw = AfxScaleX(COLORS_COMBOWIDTH)
    CTLROW( ghColorSize, L(285,"Size"), cw, ch )

    PsComboBox_GetIdealSize( ghColorCharset, cw, ch )
    if cw < AfxScaleX(COLORS_COMBOWIDTH) then cw = AfxScaleX(COLORS_COMBOWIDTH)
    CTLROW( ghColorCharset, L(286,"Character Set"), cw, ch )

    PsNumericUpDown_GetIdealSize( ghColorSpacing, cw, ch )
    if cw < AfxScaleX(COLORS_NUMWIDTH) then cw = AfxScaleX(COLORS_NUMWIDTH)
    CTLROW( ghColorSpacing, L(268,"Extra line spacing"), cw, ch )

    ' Tab Size and Case, at the bottom of the page (moved from Code Editor).
    PsNumericUpDown_GetIdealSize( ghColorTabSize, cw, ch )
    if cw < AfxScaleX(COLORS_NUMWIDTH) then cw = AfxScaleX(COLORS_NUMWIDTH)
    CTLROW( ghColorTabSize, L(122,"Tab Size"), cw, ch )

    PsComboBox_GetIdealSize( ghColorCase, cw, ch )
    if cw < AfxScaleX(COLORS_COMBOWIDTH) then cw = AfxScaleX(COLORS_COMBOWIDTH)
    CTLROW( ghColorCase, L(123,"Case"), cw, ch )

    function = y + AfxScaleY(OPTROW_PADBOTTOM)
end function


' ========================================================================================
' Paint the drawn labels.
' ========================================================================================
sub frmOptionsColors_Paint( byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub
    if gColorBuilt = false then exit sub
    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( theme.panel.text.backcolor )
    p->b->SetForeColor( theme.panel.text.forecolor )
    PaintPageLabels( p, gColorLabels )
end sub
