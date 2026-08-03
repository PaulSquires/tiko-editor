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

'  modOptionsRows.bi  --  the data-driven settings rows behind the Options
'                         dialog, plus the staging copy of the settings they edit.
'
'  WHY A TABLE
'    Most of what the Options dialog does is the same thing forty-odd times: a title on the
'    left, one control right-aligned. Those rows are described by data (see gOptRows) and
'    built, laid out, painted and read back by one set of routines here. The pages that are
'    NOT lists of settings -- Themes and Fonts, Compiler Setup, Localization, the three
'    Keywords editors -- keep their own hand-written frmOptions* files and are not here.
'
'  NO RULES, NO SECTION HEADINGS
'    Rows are separated by whitespace alone. An earlier version drew a hairline under every
'    row and it read as clutter, so the divider is gone; there is deliberately no
'    section-header row kind either. A page is a flat list of settings, however long -- the
'    old "Code Editor" and "Advanced Code Editor" pages are now one scrolling page rather
'    than two, and they are not sub-divided.
'
'  WHY A STAGING COPY
'    Row descriptors point at fields of gOptWork, never at gConfig. The dialog seeds
'    gOptWork from gConfig when it opens and copies it back only when OK is pressed, which
'    is what makes Cancel a no-op and lets a page be built lazily -- a page the user never
'    visits still contributes its gConfig-seeded value, because the value does not live in
'    a widget.
'
'    gOptWork deliberately holds ONLY the settings this dialog edits, and only those the
'    table has taken over so far. It is NOT a copy of clsConfig: that class carries
'    variable-length arrays (Tools, Builds, Cat) whose memberwise copy is not safe.
'
'  TWO TYPE TRAPS, BOTH IN THE COMMIT DIRECTION
'    1. The boolean settings are `as long` in clsConfig ON PURPOSE -- see the comment on
'       AskExit in clsConfig.bi: the config file writer emits the field verbatim, so a real
'       boolean would put "true"/"false" into the settings file instead of 0/1 and silently
'       change its format. PsToggle_GetChecked returns a BOOLEAN, so every store through
'       this table goes through iif(..., 1, 0).
'    2. TabSize / RightEdgePosition are DWSTRING in clsConfig but numbers here, because
'       PsNumericUpDown deals in doubles. OptionsWork_Load/Commit convert at the boundary
'       and nowhere else.
'    Both are asserted by the self-test.

#pragma once


' ----------------------------------------------------------------------------------------
' Shared page geometry, UNSCALED (run through AfxScaleX/Y at use). Shared so the
' hand-written pages line their controls up with the table-driven rows -- same left edge,
' same right padding, same top inset.
' ----------------------------------------------------------------------------------------
#define OPTROW_PADLEFT       24
#define OPTROW_PADRIGHT      24
#define OPTROW_LABELGAP      16
#define OPTROW_PADTOP        12
#define OPTROW_PADBOTTOM     20


' ----------------------------------------------------------------------------------------
' Page identifiers. Stored as the itemData of the navigation PsListTree's item rows and as
' the nPage field of every row descriptor. Session-only (OptionsDialogLastOpened is not
' persisted to disk), so these renumber freely.
'
' There is no separate "Advanced Code Editor" page any more: its settings are on
' OPTPAGE_EDITOR, below the ones that were already there.
' ----------------------------------------------------------------------------------------
#define OPTPAGE_NONE            -1
#define OPTPAGE_GENERAL          0
#define OPTPAGE_EDITOR           1
#define OPTPAGE_COLORS           2
#define OPTPAGE_COMPILER         3
#define OPTPAGE_LOCAL            4
#define OPTPAGE_KEYWORDS         5
#define OPTPAGE_KEYWORDSWINAPI   6
#define OPTPAGE_KEYWORDSEXTRA    7
' Syntax and Interface colours are NOT options pages any more -- they moved to frmThemes,
' which owns everything about colour. See frmThemes.bi.
' ADVANCED CODE EDITOR -- restored. Appended rather than slotted in beside OPTPAGE_EDITOR
' because these ids are persisted in OptionsDialogLastOpened and handed to PsListTree as item
' data; renumbering would reopen the dialog on a different page than the user left it on. The
' order the user SEES is the nav list's business (frmOptions_BuildNavList), not the enum's.
#define OPTPAGE_EDITOR2          8
#define OPTPAGE_COUNT            9


' ----------------------------------------------------------------------------------------
' What kind of control a row carries.
'   OPT_TOGGLE   PsToggle,         staging field is a long 0/1
'   OPT_NUMERIC  PsNumericUpDown,  staging field is a long (see type trap 2)
'   OPT_COMBO    PsComboBox,       staging field is a long, the selected index
' ----------------------------------------------------------------------------------------
enum OPT_KIND
    OPT_TOGGLE = 0
    OPT_NUMERIC
    OPT_COMBO
end enum


' ----------------------------------------------------------------------------------------
' One settings row. rc/rcLabel are DERIVED by OptionsRows_LayoutPage and never set from
' outside -- the family rule. They are stored rather than recomputed so that the paint
' callback and the self-test both read exactly the numbers the layout produced.
' ----------------------------------------------------------------------------------------
type OPTIONS_ROW
    nPage      as long    = OPTPAGE_NONE
    nKind      as long    = OPT_TOGGLE
    nLabelID   as long    = 0        ' index into LL(), i.e. the L() macro's first argument
    pField     as any ptr = 0        ' -> the long field inside gOptWork this row edits
    pParentFld as any ptr = 0        ' optional -> a long field in gOptWork that gates this
                                     '    row's enabled state (0 = always enabled)
    nMin       as long    = 0        ' OPT_NUMERIC only
    nMax       as long    = 0        ' OPT_NUMERIC only
    wszItems   as DWSTRING           ' OPT_COMBO only: the choices, chr(9) delimited
    hCtrl      as HWND    = 0        ' filled in by OptionsRows_BuildPage
    rc         as RECT               ' the whole row, page coordinates
    rcLabel    as RECT               ' the title's rect inside that row
end type


' ----------------------------------------------------------------------------------------
' The staging copy. One field per setting the TABLE has taken over -- pages still on the
' old hand-written path are absent on purpose and are still harvested from their widgets
' by frmOptions_SaveEditorOptions.
' ----------------------------------------------------------------------------------------
type OPTIONS_WORK
    ' --- General Options ---
    MultipleInstances     as long
    CompileAutosave       as long
    AskExit               as long
    RestoreSession        as long
    CompactMenus          as long
    CheckUpdates          as long
    ' --- Code Editor (the old Code Editor page) ---
    SyntaxHighlighting    as long
    ConfineCaret          as long
    HighlightCurrentLine  as long
    TabIndentSpaces       as long
    LeftMargin            as long
    FoldMargin            as long
    LineNumbering         as long
    IndentGuides          as long
    RightEdge             as long
    RightEdgePosition     as long    ' DWSTRING in clsConfig -- see type trap 2
    PositionMiddle        as long
    ClickToggleBreakpoint as long
    TabSize               as long    ' DWSTRING in clsConfig -- see type trap 2
    KeywordCase           as long    ' combo index
    ' --- Code Editor (the old Advanced Code Editor page, merged in) ---
    Codetips              as long
    AutoComplete          as long
    CharacterAutoComplete as long
    AutoIndentation       as long
    ForNextVariable       as long
    BraceHighlight        as long
    OccurrenceHighlight   as long
    DetectExternalFileChanges as long
    StripTrailingWhitespace as long
    ' The combo's selected INDEX -- which is also the FILE_ENCODING_* value, because the
    ' row's item list is built by walking those constants in order. See the row's own
    ' comment in OptionsRows_Init and Doc_EncodingName in modRoutines.inc.
    NewFileEncoding       as long
    ' --- Compiler Setup (hand-written page, frmOptionsCompiler.inc) ---
    ' CompilerToolchain is the selected toolchain FOLDER name, from which Commit rebuilds
    ' both FBWINCompiler32/64. Seeded in Load from the current gConfig path so an unvisited
    ' page round-trips unchanged; "" means leave the compiler paths alone.
    CompilerToolchain     as DWSTRING
    CompilerSwitches      as DWSTRING
    CompilerIncludes      as DWSTRING
    RunViaCommandWindow   as long
    DisableCompileBeep    as long
    ' --- Themes and Fonts (hand-written page, frmOptionsColors.inc) ---
    ThemeShortFilename    as DWSTRING
    EditorFontname        as DWSTRING
    EditorFontSize        as DWSTRING
    EditorFontCharSet     as DWSTRING
    FontExtraSpace        as long       ' DWSTRING in clsConfig -- see type trap 2
end type


dim shared gOptWork as OPTIONS_WORK
dim shared gOptRows(any) as OPTIONS_ROW

' Every live PsComboBox in the dialog. The pump must offer each one the message, because
' PsComboBox_FilterMessage takes a control handle rather than working off a global -- see
' the pump in frmOptions_Show. Safe to hold stale handles; the filter tolerates them.
dim shared gOptCombos(any) as HWND


' ----------------------------------------------------------------------------------------
' The dialog's BASE FONT -- one index into ghFont(), used by the row labels, the navigation
' list, the buttons and every row control. It exists as a single variable so the size can be
' changed in one place, and so the comparison in OptionsRows_RunSelfTest is a one-variable
' experiment rather than an eyeballed impression.
'
' Set from the TIKO_OPTIONS_FONT environment variable (10, 11 or 12) by
' OptionsFont_InitFromEnv; anything else leaves the shipped default alone.
'
' The page TITLE band deliberately does NOT follow this. It is pinned to GUIFONTBOLD_11 --
' the bold match for the 11pt default base -- so a TIKO_OPTIONS_FONT comparison run changes
' exactly one thing (the body), not two. If the base default ever changes again, the title
' band's bold face has to be re-matched to it.
' ----------------------------------------------------------------------------------------
dim shared gOptBaseFontIdx as long = GUIFONT_11

declare sub      OptionsFont_InitFromEnv()
declare function OptionsFont_Base() as HFONT
declare function OptionsFont_PointSize() as long

' Shared theme builders, so the hand-written pages (frmOptionsCompiler, and the Localization
' and Keywords pages to follow) dress their PsToggle / PsButton / PsTextBox exactly like the
' table-driven rows do, from one place. The controls COPY colors at Set time, so these are
' re-applied after every build and on a theme change.
declare sub      OptionsTheme_FillToggle( byval tc as PSTOGGLE_COLORS ptr )
declare sub      OptionsTheme_FillButton( byval bc as PSBUTTON_COLORS ptr )
declare sub      OptionsTheme_FillNumeric( byval nc as PSNUMERICUPDOWN_COLORS ptr )
declare sub      OptionsTheme_FillCombo( byval cc as PSCOMBOBOX_COLORS ptr, byval pc as PSPOPUPMENU_COLORS ptr )
declare sub      OptionsTheme_ApplyTextBox( byval hTextBox as HWND )
' Give a PsListTree the same 1px frame OptionsTheme_ApplyTextBox gives a PsTextBox, so a
' list and a text field stacked on the same page read as the same kind of control. One
' helper rather than two call sites, so the two borders cannot drift apart.
declare sub      OptionsTheme_ApplyListBorder( byval hList as HWND )

declare sub      OptionsRows_Init()
declare sub      OptionsWork_Load()
declare sub      OptionsWork_Commit()
declare function OptionsRows_BuildPage( byval nPage as long, byval hPanel as HWND ) as long
declare function OptionsRows_LayoutPage( byval nPage as long, byval hPanel as HWND, byval cxPanel as long ) as long
declare sub      OptionsRows_PaintPage( byval nPage as long, byval p as SCP_PAINTINFO ptr )
declare sub      OptionsRows_ShowPage( byval nPage as long, byval bShow as boolean )
declare sub      OptionsRows_ApplyTheme()
declare function OptionsRows_HasPage( byval nPage as long ) as boolean
declare sub      OptionsRows_RegisterCombo( byval hCombo as HWND )
declare sub      OptionsRows_ResetCombos()
declare sub      OptionsRows_RunSelfTest( byval hPanel as HWND )
declare sub      OptionsRows_RunBindTest()
