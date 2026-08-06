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

#include once "modOptionsRows.bi"
#include once "frmOptions.bi"     ' IDC_FRMOPTIONS_FIRSTROW, HWND_FRMOPTIONS_PAGE
#include once "app/clsConfig.bi"


' Row metrics, in UNSCALED units -- everything below runs them through AfxScaleX/Y.
' There is no divider: rows are separated by whitespace alone (see modOptionsRows.bi). The
' padding constants (OPTROW_PADLEFT etc.) are shared and live in modOptionsRows.bi.
#define OPTROW_HEIGHT        40
#define OPTROW_NUMWIDTH      96      ' PsNumericUpDown has no natural width; give it one


' ========================================================================================
' The dialog's base font. See the block comment in modOptionsRows.bi.
' ========================================================================================
sub OptionsFont_InitFromEnv()
    select case environ("TIKO_OPTIONS_FONT")
        case "10":  gOptBaseFontIdx = GUIFONT_10
        case "11":  gOptBaseFontIdx = GUIFONT_11
        case "12":  gOptBaseFontIdx = GUIFONT_12
    end select
end sub

function OptionsFont_Base() as HFONT
    function = ghFont( gOptBaseFontIdx )
end function

' The nominal point size behind the current base font -- for the comparison report only.
function OptionsFont_PointSize() as long
    select case gOptBaseFontIdx
        case GUIFONT_11:  return 11
        case GUIFONT_12:  return 12
    end select
    function = 10
end function


' ========================================================================================
' Height of one line of the base font, measured rather than assumed. Returns 0 if the DC
' cannot be had, which the callers treat as "contributes nothing to the row height".
' ========================================================================================
private function OptionsRows_TextHeight( byval hPanel as HWND, byval hFont as HFONT ) as long
    dim as HDC hDC = GetDC( hPanel )
    if hDC = 0 then return 0
    dim as HFONT hOld = SelectObject( hDC, hFont )
    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    SelectObject( hDC, hOld )
    ReleaseDC( hPanel, hDC )
    function = tm.tmHeight
end function


' ========================================================================================
' Look a row up by the control that raised a notification. Linear is fine: the table is
' a few dozen rows and this runs on user interaction only.
' ========================================================================================
private function OptionsRows_FindByCtrl( byval hCtrl as HWND ) as long
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = hCtrl then return i
    next
    function = -1
end function


' ========================================================================================
' Is a row currently gated off by its parent row's field being false?
' ========================================================================================
private function OptionsRows_IsRowEnabled( byval idx as long ) as boolean
    if (idx < lbound(gOptRows)) orelse (idx > ubound(gOptRows)) then return false
    dim as any ptr pParent = gOptRows(idx).pParentFld
    if pParent = 0 then return true
    function = iif( *cast(long ptr, pParent) <> 0, true, false )
end function


' ========================================================================================
' Re-evaluate every gated row on a page after one of its fields changed.
' ========================================================================================
private sub OptionsRows_SyncEnabled( byval nPage as long )
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage <> nPage then continue for
        if gOptRows(i).pParentFld = 0 then continue for
        if gOptRows(i).hCtrl = 0 then continue for
        dim as boolean bEnable = OptionsRows_IsRowEnabled(i)
        select case gOptRows(i).nKind
            case OPT_TOGGLE:   PsToggle_SetEnabled( gOptRows(i).hCtrl, bEnable )
            case OPT_NUMERIC:  PsNumericUpDown_SetEnabled( gOptRows(i).hCtrl, bEnable )
            case OPT_COMBO:    PsComboBox_SetEnabled( gOptRows(i).hCtrl, bEnable )
        end select
        ' The control repaints itself, but the row's TITLE is painted by the PAGE
        ' (OptionsRows_PaintPage picks its colour from OptionsRows_IsRowEnabled), so the page
        ' has to be told the label is stale. Without this the gated control greys and ungreys
        ' while its label keeps whatever colour it had when the page was last painted.
        ' Parent is taken off the control rather than from a page lookup so this stays correct
        ' whether the page sits on the PsScrollPanel or on the plain container -- rcLabel is in
        ' that window's client coordinates either way.
        dim as HWND hPageWnd = GetParent( gOptRows(i).hCtrl )
        if hPageWnd then
            dim as RECT rcLabel = gOptRows(i).rcLabel
            InvalidateRect( hPageWnd, @rcLabel, true )
        end if
    next
end sub


' ========================================================================================
' Change notifications. Every one of these fires on USER action only -- the controls'
' own setters are silent (the family rule), so seeding a control from gOptWork cannot
' re-enter these.
'
' THE TYPE TRAP: isChecked is a boolean but the field is a long, because the config file
' writer emits it verbatim and must see 0/1 rather than true/false.
' ========================================================================================
private sub OptionsRows_ToggleChanged( byval hToggle as HWND, byval isChecked as boolean )
    dim as long idx = OptionsRows_FindByCtrl( hToggle )
    if idx = -1 then exit sub
    if gOptRows(idx).pField = 0 then exit sub
    *cast(long ptr, gOptRows(idx).pField) = iif( isChecked, 1, 0 )
    OptionsRows_SyncEnabled( gOptRows(idx).nPage )
end sub

private sub OptionsRows_NumericChanged( byval hCtrl as HWND, byval nValue as double )
    dim as long idx = OptionsRows_FindByCtrl( hCtrl )
    if idx = -1 then exit sub
    if gOptRows(idx).pField = 0 then exit sub
    *cast(long ptr, gOptRows(idx).pField) = clng(nValue)
end sub

private sub OptionsRows_ComboChanged( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    dim as long idx = OptionsRows_FindByCtrl( hCombo )
    if idx = -1 then exit sub
    if gOptRows(idx).pField = 0 then exit sub
    *cast(long ptr, gOptRows(idx).pField) = idxNew
end sub


' ========================================================================================
' Append one row to the table.
' ========================================================================================
private sub OptionsRows_Add( byval nPage as long, _
                             byval nKind as long, _
                             byval nLabelID as long, _
                             byval pField as any ptr, _
                             byval pParentFld as any ptr = 0, _
                             byval nMin as long = 0, _
                             byval nMax as long = 0, _
                             byval wszItems as DWSTRING = "" )
    dim as long idx = ubound(gOptRows) + 1
    redim preserve gOptRows(idx) as OPTIONS_ROW
    gOptRows(idx).nPage      = nPage
    gOptRows(idx).nKind      = nKind
    gOptRows(idx).nLabelID   = nLabelID
    gOptRows(idx).pField     = pField
    gOptRows(idx).pParentFld = pParentFld
    gOptRows(idx).nMin       = nMin
    gOptRows(idx).nMax       = nMax
    gOptRows(idx).wszItems   = wszItems
    gOptRows(idx).hCtrl      = 0
end sub


' ========================================================================================
' Build the row table.
'
' Built at RUNTIME rather than as a static initialiser: fbc cannot be relied on to take
' @gOptWork.Field in a `dim shared` initialiser list, and the combo item strings have to
' be localized, which means running after LL() is loaded.
'
' The English text in the trailing comments is the same string the old dialog passed as
' the L() macro's (ignored) second argument -- kept here so the ids stay greppable.
' ========================================================================================
sub OptionsRows_Init()

    erase gOptRows

    ' ---- General Options -------------------------------------------------------------
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE, 114, @gOptWork.MultipleInstances ) ' "Allow multiple instances"
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE, 116, @gOptWork.CompileAutosave )   ' "Autosave files before compiling"
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE, 212, @gOptWork.AskExit )           ' "Ask before exiting the editor"
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE, 269, @gOptWork.RestoreSession )    ' "Restore previous session/project files on startup"
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE, 128, @gOptWork.CompactMenus )      ' "Use Compact Menu Interface"
    OptionsRows_Add( OPTPAGE_GENERAL, OPT_TOGGLE,  87, @gOptWork.CheckUpdates )      ' "Check for Updates"

    ' ---- Code Editor -----------------------------------------------------------------
    ' Everything the old Code Editor page carried, in its original order.
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 109, @gOptWork.SyntaxHighlighting )    ' "Syntax highlighting"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 111, @gOptWork.ConfineCaret )          ' "Confine caret to text"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 112, @gOptWork.HighlightCurrentLine )  ' "Highlight current line"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 113, @gOptWork.TabIndentSpaces )       ' "Treat Tab as spaces"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 117, @gOptWork.LeftMargin )            ' "Show left margin"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 118, @gOptWork.FoldMargin )            ' "Show fold margin"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 119, @gOptWork.LineNumbering )         ' "Show line numbers"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 120, @gOptWork.IndentGuides )          ' "Show Indentation guides"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 238, @gOptWork.RightEdge )             ' "Show right edge column"
    ' The right-edge column number is meaningless while the column is hidden, so the row
    ' is gated on the toggle above it -- the same relationship the old page expressed by
    ' putting the field directly underneath its checkbox.
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_NUMERIC, 239, @gOptWork.RightEdgePosition, _
                     @gOptWork.RightEdge, 1, 500 )                                      ' "Edge margin position"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 260, @gOptWork.PositionMiddle )        ' "Position searches to middle of screen"
    OptionsRows_Add( OPTPAGE_EDITOR, OPT_TOGGLE, 263, @gOptWork.ClickToggleBreakpoint ) ' "Margin click toggles Breakpoint rather than Bookmark"
    ' TAB SIZE and CASE were here. They are font-shaped settings, not editing behaviour, so
    ' they moved to the bottom of the Editor Font page (frmOptionsColors) as native controls.
    ' OptionsWork_Load/Commit still marshal gOptWork.TabSize / .KeywordCase either way -- those
    ' fields are independent of which page edits them.

    ' ---- Advanced Code Editor --------------------------------------------------------
    ' A SEPARATE PAGE AGAIN, holding exactly what the old frmOptionsEditor2 held, in its
    ' original order. It was merged into Code Editor when the dialog was rebuilt, on the
    ' grounds that one scrolling list read better than two pages; a fixed-size dialog that
    ' scrolls only the Editor page turns that trade the other way, because twenty rows do not
    ' fit and thirteen do. The boundary is not a new judgement -- it is the one the original
    ' dialog drew, recovered from the deleted form rather than re-invented.
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 110, @gOptWork.Codetips )              ' "Enable CodeTips"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE,  91, @gOptWork.AutoComplete )          ' "Enable Autocomplete"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 265, @gOptWork.CharacterAutoComplete ) ' "Enable Character Autocompletion"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 115, @gOptWork.AutoIndentation )       ' "Enable Auto Indentation"
    ' Gated, exactly as the old page gated it with EnableWindow.
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 264, @gOptWork.ForNextVariable, _
                     @gOptWork.AutoIndentation )                                         ' "Append loop variable to For/Next statement"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 261, @gOptWork.BraceHighlight )        ' "Enable Brace Highlighting"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 266, @gOptWork.OccurrenceHighlight )   ' "Enable Occurrences Highlighting"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE,  93, @gOptWork.DetectExternalFileChanges ) ' "Detect external file changes and silently reload"
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_TOGGLE, 396, @gOptWork.StripTrailingWhitespace )   ' "Strip line ending whitespace when saving"

    ' The encoding a File>New document starts life with. THE FIRST OPT_COMBO ROW IN THE
    ' TABLE -- the kind has been fully implemented and unused since the dialog was rebuilt.
    '
    ' The item list is built by walking FILE_ENCODING_ANSI..FILE_ENCODING_UTF16_BOM through
    ' Doc_EncodingName, which is what makes "the selected index is the encoding value" true
    ' by construction rather than by a mapping table someone has to keep in step. Nothing
    ' here may reorder the items; the constants decide the order.
    dim as DWSTRING wszEncItems
    for nEnc as long = FILE_ENCODING_ANSI to FILE_ENCODING_UTF16_BOM
        if nEnc > FILE_ENCODING_ANSI then wszEncItems += chr(9)
        wszEncItems += Doc_EncodingName( nEnc )
    next
    OptionsRows_Add( OPTPAGE_EDITOR2, OPT_COMBO, 80, @gOptWork.NewFileEncoding, _
                     0, 0, 0, wszEncItems )                                          ' "Default encoding for new files"

end sub


' ========================================================================================
' The toolchain folder name embedded in a "...\Toolchains\<folder>\fbc32.exe" path, or ""
' if the path does not have that shape. Case is preserved; Windows paths are
' case-insensitive so a later match against the on-disk folder still succeeds.
' ========================================================================================
private function OptionsWork_ToolchainFromPath( byval wszPath as DWSTRING ) as DWSTRING
    if PsLen(wszPath) = 0 then return ""
    ' The folder is the directory name immediately containing the exe.
    dim as DWSTRING wszDir = PsPathDirWithSep(wszPath )   ' ...\Toolchains\<folder>\
    ' Strip the trailing backslash, then take the final path segment.
    dim as DWSTRING w = wszDir
    do while (PsLen(w) > 0) andalso (PsRight(w, 1) = "\")
        w = PsLeft(w, PsLen(w) - 1)
    loop
    dim as long nSlash = PsInStrRev( w, "\" )
    if nSlash = 0 then return ""
    return PsMid( w, nSlash + 1 )
end function


' ========================================================================================
' Seed the staging copy from gConfig. Called after gConfig.LoadConfigFile().
'
' The two DWSTRING-to-number conversions happen HERE and in Commit, nowhere else.
' ========================================================================================
sub OptionsWork_Load()
    gOptWork.MultipleInstances     = gConfig.MultipleInstances
    gOptWork.CompileAutosave       = gConfig.CompileAutosave
    gOptWork.AskExit               = gConfig.AskExit
    gOptWork.RestoreSession        = gConfig.RestoreSession
    gOptWork.CompactMenus          = gConfig.CompactMenus
    gOptWork.CheckUpdates          = gConfig.CheckUpdates

    gOptWork.SyntaxHighlighting    = gConfig.SyntaxHighlighting
    gOptWork.ConfineCaret          = gConfig.ConfineCaret
    gOptWork.HighlightCurrentLine  = gConfig.HighlightCurrentLine
    gOptWork.TabIndentSpaces       = gConfig.TabIndentSpaces
    gOptWork.LeftMargin            = gConfig.LeftMargin
    gOptWork.FoldMargin            = gConfig.FoldMargin
    gOptWork.LineNumbering         = gConfig.LineNumbering
    gOptWork.IndentGuides          = gConfig.IndentGuides
    gOptWork.RightEdge             = gConfig.RightEdge
    gOptWork.RightEdgePosition     = PsVal( gConfig.RightEdgePosition )
    gOptWork.PositionMiddle        = gConfig.PositionMiddle
    gOptWork.ClickToggleBreakpoint = gConfig.ClickToggleBreakpoint
    gOptWork.TabSize               = PsVal( gConfig.TabSize )
    gOptWork.KeywordCase           = gConfig.KeywordCase

    gOptWork.Codetips              = gConfig.Codetips
    gOptWork.AutoComplete          = gConfig.AutoComplete
    gOptWork.CharacterAutoComplete = gConfig.CharacterAutoComplete
    gOptWork.AutoIndentation       = gConfig.AutoIndentation
    gOptWork.ForNextVariable       = gConfig.ForNextVariable
    gOptWork.BraceHighlight        = gConfig.BraceHighlight
    gOptWork.OccurrenceHighlight   = gConfig.OccurrenceHighlight
    gOptWork.DetectExternalFileChanges = gConfig.DetectExternalFileChanges
    gOptWork.StripTrailingWhitespace = gConfig.StripTrailingWhitespace
    gOptWork.NewFileEncoding       = gConfig.NewFileEncoding

    ' The help file path is stored curdrive-relative and expanded for display, matching
    ' what the old compiler page did in its own Show.
    gOptWork.CompilerSwitches      = gConfig.CompilerSwitches
    gOptWork.CompilerIncludes      = gConfig.CompilerIncludes
    gOptWork.RunViaCommandWindow   = gConfig.RunViaCommandWindow
    gOptWork.DisableCompileBeep    = gConfig.DisableCompileBeep
    ' The toolchain FOLDER name, parsed out of the stored 32-bit compiler path
    ' (...\Toolchains\<folder>\fbc32.exe). Seeding it here means an unvisited Compiler
    ' page commits the identical path back rather than a blank.
    gOptWork.CompilerToolchain     = OptionsWork_ToolchainFromPath( gConfig.FBWINCompiler32 )

    gOptWork.ThemeShortFilename    = gConfig.ThemeShortFilename
    gOptWork.EditorFontname        = gConfig.EditorFontname
    gOptWork.EditorFontSize        = gConfig.EditorFontSize
    gOptWork.EditorFontCharSet     = gConfig.EditorFontCharSet
    gOptWork.FontExtraSpace        = PsVal( gConfig.FontExtraSpace )
end sub


' ========================================================================================
' Copy the staging values back into gConfig. Called from OK, BEFORE SaveConfigFile.
' ========================================================================================
sub OptionsWork_Commit()
    gConfig.MultipleInstances     = gOptWork.MultipleInstances
    gConfig.CompileAutosave       = gOptWork.CompileAutosave
    gConfig.AskExit               = gOptWork.AskExit
    gConfig.RestoreSession        = gOptWork.RestoreSession
    gConfig.CompactMenus          = gOptWork.CompactMenus
    gConfig.CheckUpdates          = gOptWork.CheckUpdates

    gConfig.SyntaxHighlighting    = gOptWork.SyntaxHighlighting
    gConfig.ConfineCaret          = gOptWork.ConfineCaret
    gConfig.HighlightCurrentLine  = gOptWork.HighlightCurrentLine
    gConfig.TabIndentSpaces       = gOptWork.TabIndentSpaces
    gConfig.LeftMargin            = gOptWork.LeftMargin
    gConfig.FoldMargin            = gOptWork.FoldMargin
    gConfig.LineNumbering         = gOptWork.LineNumbering
    gConfig.IndentGuides          = gOptWork.IndentGuides
    gConfig.RightEdge             = gOptWork.RightEdge
    ' str() on a positive fb LONG has no leading space, so this round-trips the "80"
    ' the config file already held.
    gConfig.RightEdgePosition     = str(gOptWork.RightEdgePosition)
    gConfig.PositionMiddle        = gOptWork.PositionMiddle
    gConfig.ClickToggleBreakpoint = gOptWork.ClickToggleBreakpoint
    gConfig.TabSize               = str(gOptWork.TabSize)
    gConfig.KeywordCase           = gOptWork.KeywordCase

    gConfig.Codetips              = gOptWork.Codetips
    gConfig.AutoComplete          = gOptWork.AutoComplete
    gConfig.CharacterAutoComplete = gOptWork.CharacterAutoComplete
    gConfig.AutoIndentation       = gOptWork.AutoIndentation
    gConfig.ForNextVariable       = gOptWork.ForNextVariable
    gConfig.BraceHighlight        = gOptWork.BraceHighlight
    gConfig.OccurrenceHighlight   = gOptWork.OccurrenceHighlight
    gConfig.DetectExternalFileChanges = gOptWork.DetectExternalFileChanges
    gConfig.StripTrailingWhitespace = gOptWork.StripTrailingWhitespace
    ' No conversion: the combo index and the FILE_ENCODING_* value are the same number by
    ' construction, and OptionsRows_BuildPage has already read back whatever the control
    ' actually accepted, so an out-of-range stored value cannot survive to here.
    gConfig.NewFileEncoding       = gOptWork.NewFileEncoding

    gConfig.CompilerSwitches      = gOptWork.CompilerSwitches
    gConfig.CompilerIncludes      = gOptWork.CompilerIncludes
    gConfig.RunViaCommandWindow   = gOptWork.RunViaCommandWindow
    gConfig.DisableCompileBeep    = gOptWork.DisableCompileBeep
    ' Only rebuild the compiler paths when a toolchain is actually named. "" means the
    ' page was never resolved to a folder (empty install, or an unvisited page whose
    ' stored path had no toolchains segment), and the existing paths are left untouched --
    ' the same thing the old page did when its listbox had no selection.
    if PsLen(gOptWork.CompilerToolchain) > 0 then
        dim as DWSTRING wszBase = PsExePath + "Toolchains\" + gOptWork.CompilerToolchain
        gConfig.FBWINCompiler32 = wszBase + "\fbc32.exe"
        gConfig.FBWINCompiler64 = wszBase + "\fbc64.exe"
    end if

    gConfig.ThemeShortFilename     = gOptWork.ThemeShortFilename
    gConfig.EditorFontname         = gOptWork.EditorFontname
    gConfig.EditorFontSize         = gOptWork.EditorFontSize
    gConfig.EditorFontCharSet      = gOptWork.EditorFontCharSet
    gConfig.FontExtraSpace         = str(gOptWork.FontExtraSpace)
end sub


' ========================================================================================
' Does the table own any row on this page? (False = the page is still hand-written.)
' ========================================================================================
function OptionsRows_HasPage( byval nPage as long ) as boolean
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage = nPage then return true
    next
    function = false
end function


' ========================================================================================
' Register / forget the dialog's PsComboBox controls, for the message pump.
' ========================================================================================
sub OptionsRows_RegisterCombo( byval hCombo as HWND )
    if hCombo = 0 then exit sub
    dim as long idx = ubound(gOptCombos) + 1
    redim preserve gOptCombos(idx) as HWND
    gOptCombos(idx) = hCombo
end sub

sub OptionsRows_ResetCombos()
    erase gOptCombos
end sub


' ========================================================================================
' Shared theme builders. See modOptionsRows.bi.
' ========================================================================================
sub OptionsTheme_FillToggle( byval tc as PSTOGGLE_COLORS ptr )
    if tc = 0 then exit sub
    ' The pill's own background must match the page it sits on, or its corners show a halo.
    tc->BackColor                   = theme.panel.text.backcolor
    tc->FocusRingColor              = theme.panel.texthot.forecolor
    ' ON: filled with the theme ACCENT, borderless (border = fill).
    '
    ' This used to be theme.panel.buttonhot.backcolor -- a grey barely distinct from the page. The
    ' knob mapping below (page-dark when ON, bright when OFF) is Windows 11's, and it works
    ' there only because an ON track is unmistakably accent-coloured. With a grey track the
    ' one strong signal left was the knob, so an OFF switch read as BRIGHTER than an ON one
    ' and the whole control inverted at a glance. Reported 2026-07-25 after it caused a
    ' settings value to be misread as its opposite.
    tc->TrackColorOn                = theme.ui.accent
    tc->TrackColorOnHot             = theme.ui.accent
    tc->TrackColorOnDisabled        = theme.panel.button.backcolor
    tc->TrackBorderColorOn          = tc->TrackColorOn
    tc->TrackBorderColorOnHot       = tc->TrackColorOnHot
    tc->TrackBorderColorOnDisabled  = tc->TrackColorOnDisabled
    ' BRIGHT knob when ON. Windows 11 puts a dark knob on its accent track, and that reads
    ' correctly there only because its accent is far brighter than this theme's: measured,
    ' tiko's accent sits at luminance 71 against a page of 36, while the OFF knob below was
    ' 217. Keeping Windows' mapping left OFF the loudest thing on the row no matter what the
    ' track did. iOS/Material's accent-track-plus-white-knob is unambiguous at any accent
    ' brightness, so the family follows that instead.
    tc->KnobColorOn                 = theme.panel.text.forecolor
    tc->KnobColorOnHot              = theme.panel.texthot.forecolor
    tc->KnobColorOnDisabled         = theme.panel.scrollbar.forecolor
    ' OFF: outlined, the track barely distinct from the page.
    tc->TrackColorOff               = theme.panel.text.backcolor
    tc->TrackColorOffHot            = theme.panel.texthot.backcolor
    tc->TrackColorOffDisabled       = theme.panel.text.backcolor
    tc->TrackBorderColorOff         = theme.ui.divider
    tc->TrackBorderColorOffHot      = theme.panel.texthot.forecolor
    tc->TrackBorderColorOffDisabled = theme.ui.divider
    ' ...and a SUBDUED knob when OFF. This was theme.panel.text.forecolor -- the brightest colour on
    ' the page -- which is what made an OFF switch shout louder than an ON one.
    tc->KnobColorOff                = theme.panel.scrollbar.forecolor
    tc->KnobColorOffHot             = theme.panel.text.forecolor
    tc->KnobColorOffDisabled        = theme.ui.divider
end sub

sub OptionsTheme_FillButton( byval bc as PSBUTTON_COLORS ptr )
    if bc = 0 then exit sub
    bc->BackColor           = theme.panel.button.backcolor
    bc->ForeColor           = theme.panel.button.forecolor
    bc->BorderColor         = theme.ui.divider
    bc->IconColor           = theme.panel.button.forecolor
    bc->BackColorHot        = theme.panel.buttonhot.backcolor
    bc->ForeColorHot        = theme.panel.buttonhot.forecolor
    bc->BorderColorHot      = theme.panel.buttonhot.backcolor
    bc->IconColorHot        = theme.panel.buttonhot.forecolor
    bc->BackColorPressed    = theme.panel.buttonhot.backcolor
    bc->ForeColorPressed    = theme.panel.buttonhot.forecolor
    bc->BorderColorPressed  = theme.panel.buttonhot.backcolor
    bc->IconColorPressed    = theme.panel.buttonhot.forecolor
    bc->BackColorDisabled   = theme.panel.text.backcolor
    bc->ForeColorDisabled   = theme.ui.divider
    bc->BorderColorDisabled = theme.ui.divider
    bc->IconColorDisabled   = theme.ui.divider
end sub

sub OptionsTheme_ApplyTextBox( byval hTextBox as HWND )
    if hTextBox = 0 then exit sub
    PsTextBox_SetFont( hTextBox, OptionsFont_Base() )
    PsTextBox_SetForeColor( hTextBox, theme.ui.field.forecolor )
    PsTextBox_SetBackColor( hTextBox, theme.ui.field.backcolor )
    PsTextBox_SetBorderColor( hTextBox, theme.ui.divider )
    PsTextBox_SetFocusBorderColor( hTextBox, theme.panel.texthot.forecolor )
    ' The control's own Cut/Copy/Paste popup follows tiko's theme through the same registry
    ' the topmenu dropdowns use.
    ThemeTextBoxContextMenu( hTextBox )
end sub

' The list counterpart of the border ApplyTextBox sets: same width, same color, same square
' corners, so a PsListTree and a PsTextBox on one page carry identical chrome. There is no
' focus-border equivalent -- PsTextBox switches to FocusBorderColor when its editor has the
' caret, and a list has no caret to signal.
sub OptionsTheme_ApplyListBorder( byval hList as HWND )
    if hList = 0 then exit sub
    PsListTree_SetBorderColor( hList, theme.ui.divider )
    PsListTree_SetBorderWidth( hList, 1 )
end sub

sub OptionsTheme_FillNumeric( byval nc as PSNUMERICUPDOWN_COLORS ptr )
    if nc = 0 then exit sub
    nc->BackColor               = theme.panel.text.backcolor
    nc->BorderColor             = theme.ui.divider
    nc->FocusBorderColor        = theme.panel.texthot.forecolor
    nc->BorderColorDisabled     = theme.ui.divider
    nc->DividerColor            = theme.ui.divider
    nc->DividerColorDisabled    = theme.ui.divider
    nc->ValueBackColor          = theme.ui.field.backcolor
    nc->ValueForeColor          = theme.ui.field.forecolor
    nc->ValueBackColorDisabled  = theme.panel.text.backcolor
    nc->ValueForeColorDisabled  = theme.ui.divider
    nc->ButtonBackColor         = theme.panel.button.backcolor
    nc->ButtonBackColorHot      = theme.panel.buttonhot.backcolor
    nc->ButtonBackColorPressed  = theme.panel.buttonhot.backcolor
    nc->ButtonBackColorDisabled = theme.panel.text.backcolor
    nc->GlyphColor              = theme.panel.button.forecolor
    nc->GlyphColorHot           = theme.panel.buttonhot.forecolor
    nc->GlyphColorPressed       = theme.panel.buttonhot.forecolor
    nc->GlyphColorDisabled      = theme.ui.divider
end sub

sub OptionsTheme_FillCombo( byval cc as PSCOMBOBOX_COLORS ptr, byval pc as PSPOPUPMENU_COLORS ptr )
    if cc <> 0 then
        cc->BackColor            = theme.panel.button.backcolor
        cc->ForeColor            = theme.panel.button.forecolor
        cc->BorderColor          = theme.ui.divider
        cc->ChevronColor         = theme.panel.button.forecolor
        cc->BackColorHot         = theme.panel.buttonhot.backcolor
        cc->ForeColorHot         = theme.panel.buttonhot.forecolor
        cc->BorderColorHot       = theme.panel.buttonhot.backcolor
        cc->ChevronColorHot      = theme.panel.buttonhot.forecolor
        cc->BackColorOpen        = theme.panel.buttonhot.backcolor
        cc->ForeColorOpen        = theme.panel.buttonhot.forecolor
        cc->BorderColorOpen      = theme.panel.buttonhot.backcolor
        cc->ChevronColorOpen     = theme.panel.buttonhot.forecolor
        cc->BackColorDisabled    = theme.panel.text.backcolor
        cc->ForeColorDisabled    = theme.ui.divider
        cc->BorderColorDisabled  = theme.ui.divider
        cc->ChevronColorDisabled = theme.ui.divider
        cc->ForeColorPlaceholder = theme.ui.divider
        cc->FocusRingColor       = theme.panel.texthot.forecolor
    end if
    if pc <> 0 then
        pc->BackColor         = theme.topmenu.text.backcolor
        pc->ForeColor         = theme.topmenu.text.forecolor
        pc->BackColorHot      = theme.topmenu.texthot.backcolor
        pc->ForeColorHot      = theme.topmenu.texthot.forecolor
        pc->ForeColorDisabled = theme.topmenu.textdisabled.forecolor
        pc->BorderColor       = theme.topmenu.panel
        pc->SeparatorColor    = theme.ui.divider
    end if
end sub


' ========================================================================================
' Apply the current theme to every control the table owns. The controls COPY colors at
' Set time and never read tiko's theme globals, so this must be re-run after a build.
' ========================================================================================
sub OptionsRows_ApplyTheme()

    dim as PSTOGGLE_COLORS tc      : OptionsTheme_FillToggle( @tc )
    dim as PSNUMERICUPDOWN_COLORS nc : OptionsTheme_FillNumeric( @nc )
    dim as PSCOMBOBOX_COLORS cc
    dim as PSPOPUPMENU_COLORS pc
    OptionsTheme_FillCombo( @cc, @pc )

    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for
        select case gOptRows(i).nKind
            case OPT_TOGGLE
                PsToggle_SetColors( gOptRows(i).hCtrl, @tc )
                PsToggle_Refresh( gOptRows(i).hCtrl )
            case OPT_NUMERIC
                PsNumericUpDown_SetColors( gOptRows(i).hCtrl, @nc )
                PsNumericUpDown_Refresh( gOptRows(i).hCtrl )
            case OPT_COMBO
                PsComboBox_SetColors( gOptRows(i).hCtrl, @cc )
                PsComboBox_SetListColors( gOptRows(i).hCtrl, @pc )
                PsComboBox_Refresh( gOptRows(i).hCtrl )
        end select
    next
end sub


' ========================================================================================
' Create the controls for one page. Idempotent: a page already built is left alone, which
' is what makes lazy page creation safe to call on every visit.
'
' Controls are parented to the SCROLL PANEL'S PAGE WINDOW, never to the container -- see
' the PsScrollPanel contract.
' ========================================================================================
function OptionsRows_BuildPage( byval nPage as long, byval hPanel as HWND ) as long

    dim as long nBuilt = 0

    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage <> nPage then continue for
        if gOptRows(i).hCtrl <> 0 then continue for

        dim as long nSeed = *cast(long ptr, gOptRows(i).pField)

        select case gOptRows(i).nKind

            case OPT_TOGGLE
                dim as HWND hCtrl = PsToggle_Create( hPanel, IDC_FRMOPTIONS_FIRSTROW + i )
                PsToggle_SetCheckChangedCallback( hCtrl, @OptionsRows_ToggleChanged )
                ' Seed from the staging copy. SetChecked is silent, so this cannot
                ' re-enter the change handler.
                PsToggle_SetChecked( hCtrl, iif( nSeed <> 0, true, false ) )
                gOptRows(i).hCtrl = hCtrl
                nBuilt += 1

            case OPT_NUMERIC
                dim as HWND hCtrl = PsNumericUpDown_Create( hPanel, IDC_FRMOPTIONS_FIRSTROW + i )
                PsNumericUpDown_SetFont( hCtrl, OptionsFont_Base() )
                PsNumericUpDown_SetDecimalPlaces( hCtrl, 0 )
                PsNumericUpDown_SetRange( hCtrl, gOptRows(i).nMin, gOptRows(i).nMax )
                PsNumericUpDown_SetValue( hCtrl, nSeed )
                PsNumericUpDown_SetValueChangedCallback( hCtrl, @OptionsRows_NumericChanged )
                ' SetRange clamps, and the clamp is silent -- so read the value BACK and
                ' store it, or a stored setting outside the range would survive in
                ' gOptWork while the control shows something else.
                *cast(long ptr, gOptRows(i).pField) = clng( PsNumericUpDown_GetValue(hCtrl) )
                gOptRows(i).hCtrl = hCtrl
                nBuilt += 1

            case OPT_COMBO
                dim as HWND hCtrl = PsComboBox_Create( hPanel, IDC_FRMOPTIONS_FIRSTROW + i )
                PsComboBox_SetFont( hCtrl, OptionsFont_Base() )
                dim as long nItems = PsStrParseCount( gOptRows(i).wszItems, chr(9) )
                for j as long = 1 to nItems
                    PsComboBox_AddItem( hCtrl, PsStrParse( gOptRows(i).wszItems, j, chr(9) ) )
                next
                PsComboBox_SetCurSel( hCtrl, nSeed )
                PsComboBox_SetSelChangeCallback( hCtrl, @OptionsRows_ComboChanged )
                ' Same reasoning as the numeric clamp above: SetCurSel rejects an index
                ' outside the list, silently, so take back what the control actually holds.
                *cast(long ptr, gOptRows(i).pField) = PsComboBox_GetCurSel( hCtrl )
                ' The pump has to offer every live combo its messages.
                OptionsRows_RegisterCombo( hCtrl )
                gOptRows(i).hCtrl = hCtrl
                nBuilt += 1

        end select
    next

    if nBuilt then
        OptionsRows_ApplyTheme()
        OptionsRows_SyncEnabled( nPage )
    end if

    function = nBuilt
end function


' ========================================================================================
' Show or hide every control belonging to a page.
' ========================================================================================
sub OptionsRows_ShowPage( byval nPage as long, byval bShow as boolean )
    dim as long nCmd = iif( bShow, SW_SHOW, SW_HIDE )
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage <> nPage then continue for
        if gOptRows(i).hCtrl = 0 then continue for
        ShowWindow( gOptRows(i).hCtrl, nCmd )
    next
end sub


' ========================================================================================
' Lay a page out for a given page width and return its content height in pixels.
'
' cxPanel is HANDED to us by PsScrollPanel's layout callback; do not read it back off the
' window, which may be one call stale.
'
' Geometry (all rects derived here and stored on the row, never set from outside):
'     row      : full page width, OPTROW_HEIGHT tall, stacked from the top, no divider
'     control  : right-aligned at cxPanel - padRight, vertically centred in the row
'     label    : from padLeft to the control's left edge, less the gap
' ========================================================================================
function OptionsRows_LayoutPage( byval nPage as long, byval hPanel as HWND, byval cxPanel as long ) as long

    dim as long padLeft   = AfxScaleX(OPTROW_PADLEFT)
    dim as long padRight  = AfxScaleX(OPTROW_PADRIGHT)
    dim as long labelGap  = AfxScaleX(OPTROW_LABELGAP)
    dim as long padTop    = AfxScaleY(OPTROW_PADTOP)
    dim as long rowHeight = AfxScaleY(OPTROW_HEIGHT)
    dim as long y         = padTop

    ' The row title has to fit too, not just the control -- measured once per pass with the
    ' base font. Without this a larger base font would clip its own labels while every
    ' control-height assertion still passed.
    dim as long textH = OptionsRows_TextHeight( hPanel, OptionsFont_Base() )

    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage <> nPage then continue for
        if gOptRows(i).hCtrl = 0 then continue for

        dim as long ctrlW, ctrlH
        select case gOptRows(i).nKind
            case OPT_TOGGLE
                PsToggle_GetIdealSize( gOptRows(i).hCtrl, ctrlW, ctrlH )
            case OPT_NUMERIC
                ' The spinner has no natural width -- its ideal size is only a minimum --
                ' so the column width is declared here and only the height is taken.
                PsNumericUpDown_GetIdealSize( gOptRows(i).hCtrl, ctrlW, ctrlH )
                if ctrlW < AfxScaleX(OPTROW_NUMWIDTH) then ctrlW = AfxScaleX(OPTROW_NUMWIDTH)
            case OPT_COMBO
                ' The combo sizes itself to its WIDEST item, so picking never resizes it.
                PsComboBox_GetIdealSize( gOptRows(i).hCtrl, ctrlW, ctrlH )
            case else
                ctrlW = 0: ctrlH = 0
        end select

        ' The row is at least as tall as the taller of the control and the title text.
        dim as long thisRowH = rowHeight
        if ctrlH + AfxScaleY(8) > thisRowH then thisRowH = ctrlH + AfxScaleY(8)
        if textH + AfxScaleY(8) > thisRowH then thisRowH = textH + AfxScaleY(8)

        with gOptRows(i)
            .rc.left   = 0
            .rc.top    = y
            .rc.right  = cxPanel
            .rc.bottom = y + thisRowH

            dim as long ctrlX = cxPanel - padRight - ctrlW
            dim as long ctrlY = y + (thisRowH - ctrlH) \ 2
            SetWindowPos( .hCtrl, 0, ctrlX, ctrlY, ctrlW, ctrlH, SWP_NOZORDER or SWP_NOACTIVATE )

            .rcLabel.left   = padLeft
            .rcLabel.top    = y
            .rcLabel.right  = ctrlX - labelGap
            .rcLabel.bottom = .rc.bottom
            ' A page squeezed narrower than the control plus its padding must not produce
            ' an inverted label rect -- DrawText would then paint outside it.
            if .rcLabel.right < .rcLabel.left then .rcLabel.right = .rcLabel.left
        end with

        y += thisRowH
    next

    function = y + AfxScaleY(OPTROW_PADBOTTOM)
end function


' ========================================================================================
' Paint one page: the row titles, and nothing else. The page BACKGROUND is filled by the
' caller before this runs, and there are deliberately no rules between rows.
'
' Paint through p->b only -- never the screen DC (the PsScrollPanel contract).
' ========================================================================================
sub OptionsRows_PaintPage( byval nPage as long, byval p as SCP_PAINTINFO ptr )
    if p = 0 then exit sub

    p->b->SetFont( OptionsFont_Base() )
    p->b->SetBackColor( theme.panel.text.backcolor )

    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).nPage <> nPage then continue for
        if gOptRows(i).hCtrl = 0 then continue for

        dim as boolean bEnabled = OptionsRows_IsRowEnabled(i)
        p->b->SetForeColor( iif( bEnabled, theme.panel.text.forecolor, theme.ui.divider ) )

        dim as DWSTRING wszCaption = LL(gOptRows(i).nLabelID)
        dim as RECT rcText = gOptRows(i).rcLabel
        p->b->PaintText( wszCaption, @rcText, DT_LEFT or DT_END_ELLIPSIS )
    next
end sub


' ========================================================================================
' Geometry self-test. Gated on TIKO_OPTIONS_SELFTEST=1; call it from frmOptions_Show after
' the pages are built and laid out, before the message loop.
'
' Nothing here is eyeballed: every assertion reads the numbers the layout actually
' produced, or reads a value back out of a real control.
' ========================================================================================
' ========================================================================================
' HEADLESS binding check.                              (TIKO_OPTIONS_SELFTEST=1)
'
' Does row N's pField actually point at the gConfig field row N's LABEL claims? Nothing else
' verifies that: the row table, OptionsWork_Load/Commit and the seeding all read correctly in
' isolation, and a shift between them would show up only as toggles displaying their
' neighbour's value.
'
' The label id -> field pairing below is written INDEPENDENTLY of OptionsRows_Init rather than
' derived from it, which is the whole point -- an oracle copied from the thing it checks
' proves nothing. Needs no window, so it runs at startup rather than when the dialog opens.
' ========================================================================================
sub OptionsRows_RunBindTest()

    if environ("TIKO_OPTIONS_SELFTEST") <> "1" then exit sub

    dim as long nPass, nFail
    dim as string sLine

    OptionsRows_Init()
    OptionsWork_Load()

    #macro BINDCHK( labelid, cfgfield, nm )
        scope
            dim as long nFound = -1
            for i as long = lbound(gOptRows) to ubound(gOptRows)
                if gOptRows(i).nLabelID = labelid then nFound = i: exit for
            next
            if nFound < 0 then
                nFail += 1
                sLine = "  FAIL: no row carries label id " & str(labelid) & " (" & nm & ")"
                ? sLine
            elseif gOptRows(nFound).pField = 0 then
                nFail += 1
                sLine = "  FAIL: row for " & nm & " has a null field pointer"
                ? sLine
            elseif *cast(long ptr, gOptRows(nFound).pField) <> cfgfield then
                nFail += 1
                sLine = "  FAIL: " & nm & " row shows " & _
                        str(*cast(long ptr, gOptRows(nFound).pField)) & _
                        " but gConfig holds " & str(cfgfield) & " -- ROW/FIELD MISMATCH"
                ? sLine
            else
                nPass += 1
            end if
        end scope
    #endmacro

    ? "--- TIKO_OPTIONS_SELFTEST (row/field binding) ---"

    BINDCHK( 115, gConfig.MultipleInstances,    "Allow multiple instances" )
    BINDCHK( 117, gConfig.CompileAutosave,      "Autosave before compiling" )
    BINDCHK( 216, gConfig.AskExit,              "Ask before exiting" )
    BINDCHK( 275, gConfig.RestoreSession,       "Restore previous session" )
    BINDCHK( 129, gConfig.CompactMenus,         "Use Compact Menu Interface" )
    BINDCHK(  88, gConfig.CheckUpdates,         "Check for Updates" )
    BINDCHK( 110, gConfig.SyntaxHighlighting,   "Syntax highlighting" )
    BINDCHK( 120, gConfig.LineNumbering,        "Show line numbers" )
    BINDCHK(  81, gConfig.NewFileEncoding,      "Default encoding for new files" )

    ' ---- The new-file-encoding combo: index == FILE_ENCODING_* value ------------------
    ' The row's item list is built by walking the encoding constants through
    ' Doc_EncodingName, and OptionsRows_ComboChanged stores the raw selected index into
    ' gOptWork.NewFileEncoding, which Commit copies verbatim into gConfig. So the item
    ' ORDER is load-bearing: re-ordering it silently makes the dialog set a different
    ' encoding from the one the user picked, with nothing else to report it.
    scope
        dim as long idxRow = -1
        for i as long = lbound(gOptRows) to ubound(gOptRows)
            if gOptRows(i).pField = @gOptWork.NewFileEncoding then idxRow = i: exit for
        next
        if idxRow < 0 then
            nFail += 1
            ? "  FAIL: no row is bound to gOptWork.NewFileEncoding"
        else
            #macro ENCROWCHK( cond, msg )
                if (cond) then
                    nPass += 1
                else
                    nFail += 1
                    '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
                    dim as DWSTRING wszMsg = msg
                    sLine = "  FAIL: " & wszMsg.Utf8
                    ? sLine
                end if
            #endmacro

            ENCROWCHK( gOptRows(idxRow).nKind = OPT_COMBO, _
                "the new-file-encoding row is not an OPT_COMBO" )
            ENCROWCHK( gOptRows(idxRow).nPage = OPTPAGE_EDITOR2, _
                "the new-file-encoding row is not on the Advanced Code Editor page" )
            ENCROWCHK( len(LL(gOptRows(idxRow).nLabelID)) > 0, _
                "L(" & str(gOptRows(idxRow).nLabelID) & ") encoding label is blank" )

            dim as long nItems = PsStrParseCount( gOptRows(idxRow).wszItems, chr(9) )
            ENCROWCHK( nItems = (FILE_ENCODING_UTF16_BOM - FILE_ENCODING_ANSI + 1), _
                "encoding combo has " & str(nItems) & " items, expected one per FILE_ENCODING_*" )

            for e as long = FILE_ENCODING_ANSI to FILE_ENCODING_UTF16_BOM
                dim as DWSTRING wItem = PsStrParse( gOptRows(idxRow).wszItems, e + 1, chr(9) )
                ENCROWCHK( wItem = Doc_EncodingName(e), _
                    "encoding combo item " & str(e) & " is """ & wItem & """, expected """ & _
                    Doc_EncodingName(e) & """ -- index no longer equals the encoding value" )
            next
        end if
    end scope

    sLine = "--- options bind test: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    ? sLine

end sub


sub OptionsRows_RunSelfTest( byval hPanel as HWND )

    if environ("TIKO_OPTIONS_SELFTEST") <> "1" then exit sub

    dim as long nPass = 0, nFail = 0
    dim as string sMsg

    #macro OPTASSERT( cond, msg )
        if (cond) then
            nPass += 1
        else
            nFail += 1
            '' .Utf8 rather than the implicit ANSI-codepage conversion. Assertion text.
            dim as DWSTRING wszMsg = msg
            sMsg = "  FAIL: " & wszMsg.Utf8
            print sMsg
        end if
    #endmacro

    print "--- Options rows self-test ---"

    ' ---- LABEL / CONTROL PAIRING -------------------------------------------------------
    ' Does the title drawn on a row sit beside THAT row's control? Every data-side check
    ' passes (see OptionsRows_RunBindTest), so if a toggle appears to show its neighbour's
    ' value, a shift between the label rect and the control rect is what is left. Reported
    ' with the actual coordinates rather than a bare pass/fail, because the interesting case
    ' is "off by exactly one row height".
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for
        if IsWindow(gOptRows(i).hCtrl) = 0 then continue for

        dim as RECT rcCtrl
        GetWindowRect( gOptRows(i).hCtrl, @rcCtrl )
        MapWindowPoints( HWND_DESKTOP, GetParent(gOptRows(i).hCtrl), cast(POINT ptr, @rcCtrl), 2 )

        dim as long ctrlMidY  = (rcCtrl.top + rcCtrl.bottom) \ 2
        dim as boolean bInRow = (ctrlMidY >= gOptRows(i).rc.top) andalso _
                                (ctrlMidY <= gOptRows(i).rc.bottom)
        OPTASSERT( bInRow, "row " & str(i) & " (label id " & str(gOptRows(i).nLabelID) & _
                   ") control centre y=" & str(ctrlMidY) & " is OUTSIDE its own row band " & _
                   str(gOptRows(i).rc.top) & ".." & str(gOptRows(i).rc.bottom) & _
                   " -- label and control are paired with DIFFERENT rows" )

        ' And the title has to be on the same band as the control it labels.
        dim as long lblMidY = (gOptRows(i).rcLabel.top + gOptRows(i).rcLabel.bottom) \ 2
        OPTASSERT( abs(lblMidY - ctrlMidY) <= AfxScaleY(20), _
                   "row " & str(i) & " (label id " & str(gOptRows(i).nLabelID) & _
                   ") label centre y=" & str(lblMidY) & " but its control is at y=" & _
                   str(ctrlMidY) & " -- drawn against the wrong control" )
    next

    ' ---- the table itself -------------------------------------------------------------
    OPTASSERT( ubound(gOptRows) >= 0, "row table is empty" )
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        OPTASSERT( gOptRows(i).pField <> 0, "row " & str(i) & " has no staging field" )
        OPTASSERT( gOptRows(i).nLabelID > 0, "row " & str(i) & " has no label id" )
        OPTASSERT( len(LL(gOptRows(i).nLabelID)) > 0, _
                   "row " & str(i) & " label id " & str(gOptRows(i).nLabelID) & " is an empty string" )
        if gOptRows(i).nKind = OPT_NUMERIC then
            OPTASSERT( gOptRows(i).nMax > gOptRows(i).nMin, _
                       "row " & str(i) & " numeric range is empty" )
        end if
        if gOptRows(i).nKind = OPT_COMBO then
            OPTASSERT( PsStrParseCount(gOptRows(i).wszItems, chr(9)) > 1, _
                       "row " & str(i) & " combo has fewer than two choices" )
        end if
    next

    ' Every staging field must be distinct: two rows sharing one field would make the
    ' second silently overwrite the first.
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        for j as long = i + 1 to ubound(gOptRows)
            OPTASSERT( gOptRows(i).pField <> gOptRows(j).pField, _
                       "rows " & str(i) & " and " & str(j) & " share a staging field" )
        next
    next

    ' ---- per-page geometry ------------------------------------------------------------
    ' THE RULER COMES FROM THE ROW'S OWN PARENT, not from a container passed in. Only
    ' OPTPAGE_EDITOR lives on the PsScrollPanel now; every other page is on the plain
    ' container, which is WIDER because it reserves no scrollbar strip. Measuring all of them
    ' against the scroll panel's page width reported six perfectly good General rows as "not
    ' full width" and "overruns the right padding" -- the rows were right and the ruler was
    ' wrong, which is the failure mode an assertion is least likely to be doubted over.
    '
    ' Asking each control who its parent is removes the assumption rather than correcting it,
    ' so moving a page between containers cannot break this again.
    for nPage as long = 0 to OPTPAGE_COUNT - 1
        if OptionsRows_HasPage(nPage) = false then continue for

        dim as HWND hOwner = 0
        for i as long = lbound(gOptRows) to ubound(gOptRows)
            if gOptRows(i).nPage <> nPage then continue for
            if gOptRows(i).hCtrl = 0 then continue for
            hOwner = GetParent( gOptRows(i).hCtrl )
            exit for
        next
        if hOwner = 0 then continue for

        dim as RECT rcOwner
        GetClientRect( hOwner, @rcOwner )
        dim as long cxPanel = rcOwner.right - rcOwner.left
        OPTASSERT( cxPanel > 0, "page " & str(nPage) & " width is not positive" )

        dim as long nPrevBottom = -1
        dim as long nSeen = 0

        for i as long = lbound(gOptRows) to ubound(gOptRows)
            if gOptRows(i).nPage <> nPage then continue for
            if gOptRows(i).hCtrl = 0 then continue for
            nSeen += 1

            dim as RECT rcRow = gOptRows(i).rc
            dim as RECT rcLbl = gOptRows(i).rcLabel

            OPTASSERT( rcRow.bottom > rcRow.top, "page " & str(nPage) & " row " & str(i) & " has no height" )
            OPTASSERT( rcRow.right = cxPanel, "page " & str(nPage) & " row " & str(i) & " is not full width" )

            ' Rows tile downwards without overlapping or leaving gaps.
            if nPrevBottom >= 0 then
                OPTASSERT( rcRow.top = nPrevBottom, _
                           "page " & str(nPage) & " row " & str(i) & " top " & str(rcRow.top) & _
                           " does not meet previous bottom " & str(nPrevBottom) )
            end if
            nPrevBottom = rcRow.bottom

            ' The control sits inside the row, right-aligned, clear of the label.
            dim as RECT rcCtrl
            GetWindowRect( gOptRows(i).hCtrl, @rcCtrl )
            MapWindowPoints( HWND_DESKTOP, hOwner, cast(LPPOINT, @rcCtrl), 2 )

            OPTASSERT( rcCtrl.right <= cxPanel - AfxScaleX(OPTROW_PADRIGHT) + 1, _
                       "page " & str(nPage) & " row " & str(i) & " control overruns the right padding" )
            OPTASSERT( rcCtrl.top >= rcRow.top, "page " & str(nPage) & " row " & str(i) & " control above its row" )
            OPTASSERT( rcCtrl.bottom <= rcRow.bottom, "page " & str(nPage) & " row " & str(i) & " control below its row" )
            OPTASSERT( rcLbl.right <= rcCtrl.left, _
                       "page " & str(nPage) & " row " & str(i) & " label rect " & str(rcLbl.right) & _
                       " overlaps control left " & str(rcCtrl.left) )
            OPTASSERT( rcLbl.left = AfxScaleX(OPTROW_PADLEFT), _
                       "page " & str(nPage) & " row " & str(i) & " label not at the left padding" )
        next

        ' NOT an assertion. Pages are built lazily -- on first visit -- so a page the user has
        ' not opened yet legitimately has no controls, and this fired as a failure every time
        ' the dialog was opened without touring every page. There is nothing detectable here:
        ' "declared but never built" and "not visited yet" are the same state from inside this
        ' function. Reported so the count of examined pages is still visible.
        if nSeen = 0 then
            sMsg = "  (page " & str(nPage) & " not built yet -- lazy, skipped)"
            print sMsg
        end if
    next

    ' ---- staging round trip, and the long-not-boolean rule ----------------------------
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for

        dim as long ptr pFld = cast(long ptr, gOptRows(i).pField)
        dim as long nSaved = *pFld

        select case gOptRows(i).nKind

            case OPT_TOGGLE
                ' The control agrees with the field it was seeded from.
                OPTASSERT( PsToggle_GetChecked(gOptRows(i).hCtrl) = iif(nSaved <> 0, true, false), _
                           "row " & str(i) & " toggle does not match its staging field" )
                ' Drive the change handler exactly as the control would, then assert the
                ' field is a plain 0/1 long. A boolean stored here writes "true"/"false"
                ' to the settings file -- see the type trap in modOptionsRows.bi.
                OptionsRows_ToggleChanged( gOptRows(i).hCtrl, true )
                OPTASSERT( *pFld = 1, "row " & str(i) & " did not store 1 for checked (got " & str(*pFld) & ")" )
                OptionsRows_ToggleChanged( gOptRows(i).hCtrl, false )
                OPTASSERT( *pFld = 0, "row " & str(i) & " did not store 0 for unchecked (got " & str(*pFld) & ")" )

            case OPT_NUMERIC
                OPTASSERT( clng(PsNumericUpDown_GetValue(gOptRows(i).hCtrl)) = nSaved, _
                           "row " & str(i) & " numeric does not match its staging field" )
                ' The control must be holding a value inside its own declared range.
                OPTASSERT( nSaved >= gOptRows(i).nMin, "row " & str(i) & " numeric below its minimum" )
                OPTASSERT( nSaved <= gOptRows(i).nMax, "row " & str(i) & " numeric above its maximum" )
                OptionsRows_NumericChanged( gOptRows(i).hCtrl, gOptRows(i).nMax )
                OPTASSERT( *pFld = gOptRows(i).nMax, "row " & str(i) & " did not store the numeric value" )

            case OPT_COMBO
                OPTASSERT( PsComboBox_GetCurSel(gOptRows(i).hCtrl) = nSaved, _
                           "row " & str(i) & " combo does not match its staging field" )
                OPTASSERT( PsComboBox_GetCount(gOptRows(i).hCtrl) = PsStrParseCount(gOptRows(i).wszItems, chr(9)), _
                           "row " & str(i) & " combo item count does not match its declared choices" )
                OptionsRows_ComboChanged( gOptRows(i).hCtrl, nSaved, 0 )
                OPTASSERT( *pFld = 0, "row " & str(i) & " did not store the combo index" )

        end select

        ' Restore both the field and the control.
        *pFld = nSaved
        select case gOptRows(i).nKind
            case OPT_TOGGLE:   PsToggle_SetChecked( gOptRows(i).hCtrl, iif(nSaved <> 0, true, false) )
            case OPT_NUMERIC:  PsNumericUpDown_SetValue( gOptRows(i).hCtrl, nSaved )
            case OPT_COMBO:    PsComboBox_SetCurSel( gOptRows(i).hCtrl, nSaved )
        end select
    next

    ' ---- the gated rows actually gate --------------------------------------------------
    ' Enabled state is read back off the CONTROL (IsWindowEnabled), not off our own flag:
    ' the point is that the disable reached the window, not that we remembered to want it.
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for
        if gOptRows(i).pParentFld = 0 then continue for

        dim as long ptr pParent = cast(long ptr, gOptRows(i).pParentFld)
        dim as long nSavedParent = *pParent

        *pParent = 1
        OptionsRows_SyncEnabled( gOptRows(i).nPage )
        OPTASSERT( IsWindowEnabled(gOptRows(i).hCtrl) <> 0, _
                   "row " & str(i) & " stayed disabled with its parent on" )

        *pParent = 0
        OptionsRows_SyncEnabled( gOptRows(i).nPage )
        OPTASSERT( IsWindowEnabled(gOptRows(i).hCtrl) = 0, _
                   "row " & str(i) & " stayed enabled with its parent off" )

        *pParent = nSavedParent
        OptionsRows_SyncEnabled( gOptRows(i).nPage )
    next

    ' ---- the DWSTRING <-> number boundary ---------------------------------------------
    ' Commit writes str() into fields clsConfig declares as DWSTRING; a leading space or a
    ' float tail there would change the settings file's format.
    dim as long nSavedTab = gOptWork.TabSize
    gOptWork.TabSize = 4
    OPTASSERT( str(gOptWork.TabSize) = "4", "TabSize does not stringify as a bare integer" )
    gOptWork.TabSize = nSavedTab

    ' ---- BASE FONT COMPARISON ---------------------------------------------------------
    ' Measured, not eyeballed. For each candidate base font this reports the metrics that
    ' actually decide whether the dialog looks cramped: the line height, the row height it
    ' forces, and the resulting height of the longest page against the viewport.
    '
    ' The controls are NOT rebuilt per candidate -- their heights come from the font they
    ' were created with -- so the "row" column is the height the LAYOUT would choose from
    ' the text alone. That is the term the font actually moves; the control term is shown
    ' beside it so it is obvious which one is binding.
    dim as RECT rcVP : GetClientRect( HWND_FRMOPTIONS_PAGE, @rcVP )
    dim as long nViewport = rcVP.bottom - rcVP.top
    dim as long nCtrlMax = 0
    dim as long nRowsLongest = 0
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for
        ' BOTH halves of the old Code Editor page. It was the longest page when it carried all
        ' twenty rows; since the split it is thirteen, and measuring only that half would
        ' quietly stop reporting on the widest control in the dialog.
        dim as boolean bEditorPage = ((gOptRows(i).nPage = OPTPAGE_EDITOR) orelse _
                                      (gOptRows(i).nPage = OPTPAGE_EDITOR2))
        if bEditorPage = false then continue for
        nRowsLongest += 1
        dim as long cw, ch
        select case gOptRows(i).nKind
            case OPT_TOGGLE:   PsToggle_GetIdealSize( gOptRows(i).hCtrl, cw, ch )
            case OPT_NUMERIC:  PsNumericUpDown_GetIdealSize( gOptRows(i).hCtrl, cw, ch )
            case OPT_COMBO:    PsComboBox_GetIdealSize( gOptRows(i).hCtrl, cw, ch )
        end select
        if ch > nCtrlMax then nCtrlMax = ch
    next

    print ""
    print "--- Base font comparison (current base is " & str(OptionsFont_PointSize()) & "pt) ---"
    sMsg = "   pt   lineH   textRow   ctrlRow   rowUsed   page(" & str(nRowsLongest) & " rows)   viewport   scrolls"
    print sMsg

    dim as long nCandIdx(2) = { GUIFONT_10, GUIFONT_11, GUIFONT_12 }
    dim as long nCandPt(2)  = { 10, 11, 12 }

    for c as long = 0 to 2
        dim as long lineH   = OptionsRows_TextHeight( hPanel, ghFont(nCandIdx(c)) )
        dim as long textRow = lineH + AfxScaleY(8)
        dim as long ctrlRow = nCtrlMax + AfxScaleY(8)
        dim as long rowUsed = AfxScaleY(OPTROW_HEIGHT)
        if ctrlRow > rowUsed then rowUsed = ctrlRow
        if textRow > rowUsed then rowUsed = textRow
        dim as long pageH   = AfxScaleY(OPTROW_PADTOP) + (rowUsed * nRowsLongest) + AfxScaleY(OPTROW_PADBOTTOM)

        sMsg = "  " & right("   " & str(nCandPt(c)), 3) & _
               right("        " & str(lineH),   8) & _
               right("          " & str(textRow), 10) & _
               right("          " & str(ctrlRow), 10) & _
               right("          " & str(rowUsed), 10) & _
               right("               " & str(pageH), 15) & _
               right("           " & str(nViewport), 11) & _
               right("          " & iif(pageH > nViewport, "yes", "no"), 10)
        print sMsg
    next

    ' The comparison is only meaningful if the three faces really are different sizes --
    ' a failed CreateFont silently hands back a usable default and every row would match.
    dim as long h10 = OptionsRows_TextHeight( hPanel, ghFont(GUIFONT_10) )
    dim as long h11 = OptionsRows_TextHeight( hPanel, ghFont(GUIFONT_11) )
    dim as long h12 = OptionsRows_TextHeight( hPanel, ghFont(GUIFONT_12) )
    OPTASSERT( ghFont(GUIFONT_11) <> 0, "GUIFONT_11 was never created" )
    OPTASSERT( ghFont(GUIFONT_12) <> 0, "GUIFONT_12 was never created" )
    OPTASSERT( h11 > h10, "GUIFONT_11 is not taller than GUIFONT_10 (" & str(h11) & " vs " & str(h10) & ")" )
    OPTASSERT( h12 > h11, "GUIFONT_12 is not taller than GUIFONT_11 (" & str(h12) & " vs " & str(h11) & ")" )

    ' Whatever base is selected, no row may clip its own title.
    dim as long nBaseH = OptionsRows_TextHeight( hPanel, OptionsFont_Base() )
    for i as long = lbound(gOptRows) to ubound(gOptRows)
        if gOptRows(i).hCtrl = 0 then continue for
        OPTASSERT( (gOptRows(i).rcLabel.bottom - gOptRows(i).rcLabel.top) >= nBaseH, _
                   "row " & str(i) & " label rect is shorter than one line of the base font" )
    next

    print ""
    sMsg = "--- Options rows: " & str(nPass) & " passed, " & str(nFail) & " failed ---"
    print sMsg

end sub
