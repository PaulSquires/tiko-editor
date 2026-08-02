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

''
''  THE THEME, AS ONE STRUCTURE
''
''  Replaces the ten separate gh* globals (ghEditor, ghPanel, ghOutput, ghUI, ghPopup,
''  ghGeneral, ghTopTabs, ghStatusBar, ghMenuBar, ghMain), which between them carried ad-hoc
''  field names -- ForeColorKeyword2, BackColorHot, PanelColor -- read at 1,266 sites in 49
''  files. Here every colour is reached by one predictable dotted path:
''
''      theme.editor.keyword1.forecolor
''      theme.panel.texthot.backcolor
''      theme.topmenu.text.forecolor
''
''  WRITTEN BY HAND, DELIBERATELY, and this is the one part of the theme engine that is not
''  generated. The key table (modThemeKeys.bi) generates the bind rows, the defaults, the
''  categories and the roles precisely because those repeat ~200 times and drift when they are
''  maintained separately. The type declarations do not repeat -- there are eleven of them,
''  they are the part a human most wants to read, and generating members from a flat table
''  would need a #define executed from inside a macro body to track the nesting. Measured in
''  the Phase 0 spike: everything else the table does is proven; that one is not, and it buys
''  nothing.
''
''  THREE SHAPES, and which one a key uses is decided by the channels it actually has:
''      THSTYLE   fore + back + bold/italic/underline   the ten syntax styles
''      THPAIR    fore + back                           every "text / texthot" pair
''      COLORREF  one channel                           a hairline, a caret, a single fill
''
''  A single-channel key stays a plain COLORREF rather than being padded into a THPAIR with a
''  dead half: the bind table addresses fore and back independently, so main.panel can bind
''  its ONE colour to the background channel and editor.caret can bind its one to the
''  foreground, without either carrying a field nothing writes.
''

#include once "AfxNova/DWSTRING.bi"


' Group-header categories for the theme editor's key lists. Stable ids, deliberately NOT
' localized strings -- grouping is structure and must not change meaning with the UI
' language. THEMECAT_OTHER is the unclassified bucket the self-test asserts stays empty.
'
' Lives here rather than in frmThemes.bi because the KEY TABLE now carries each key's
' category, and the key table is engine vocabulary loaded long before any dialog.
enum
    THEMECAT_OTHER = 0
    THEMECAT_MAIN
    THEMECAT_PANEL
    THEMECAT_TOPTABS
    THEMECAT_MENUBAR
    THEMECAT_POPUP
    THEMECAT_STATUSBAR
    THEMECAT_OUTPUT
    THEMECAT_COMPILE
    THEMECAT_SHARED
    THEMECAT_SYNTAX
    THEMECAT_CURSOR
    THEMECAT_MARGINS
    THEMECAT_MARKERS
    THEMECAT_EDCHROME
end enum


''
''  THE ROLE PALETTE
''
''  A small set of SEMANTIC colours every widget key falls back to. Two layers:
''
''      explicit key in the file  ->  the key's declared role  ->  built-in default
''
''  What it buys: a theme file that defines only the roles is COMPLETE and valid, so a new
''  theme is ~20 lines instead of 140; and a widget key added later is themed for free in
''  every existing theme rather than reading as black until each file is edited.
''
''  This generalizes Theme_ResolveFallbacks, which did the same thing for exactly four ui.*
''  keys via a six-row hand-written table. That table is GONE -- the two must not both claim
''  a key or they race and last-writer-wins.
''
''  THE THREE SYNTAX KEYWORD ROLES ARE NAMED FOR WHAT THEY ACTUALLY ARE, not for what the
''  equivalent slot is called in a VS Code or Base16 palette. tiko feeds Scintilla three
''  keyword lists (clsDocument.inc): list 0 is the FreeBASIC language, list 1 is the Win32
''  API, list 2 is the user's own extra words. Calling list 1 "type" and list 2 "function"
''  would be a guess dressed as a mapping, and every theme author after us would inherit it.
''
enum THEME_ROLE
    THROLE_NONE = 0             ' this channel has no role; absent means absent

    ' --- surfaces and text ---
    THROLE_BACKGROUND           ' the editor / app base surface
    THROLE_BACKGROUNDALT        ' secondary surface: panels, status bar, output
    THROLE_BACKGROUNDRAISED     ' floats above the app: menus, popups, tooltips
    THROLE_FOREGROUND           ' primary text
    THROLE_FOREGROUNDDIM        ' secondary, disabled, line numbers
    THROLE_BORDER               ' hairlines and dividers
    THROLE_ACCENT               ' active / selected / focused FILL
    ' The accent as a readable STROKE: a glyph, a tick, a focus ring, a hairline.
    '
    ' Separate from THROLE_ACCENT because CLAUDE.md states the rule outright -- the accent
    ' is a fill, never a foreground -- and the reason is measurable: ui.accent resolves to
    ' toptabs.iconselected, which is a dark navy in default_dark and a PALE TINT in
    ' default_light. A colour chosen to sit behind an icon is too dark to read as ink on one
    ' theme and invisible as ink on the other. Reached for exactly once already, for the
    ' Unused Symbols check glyph, and it came out too dark on sight.
    THROLE_ACCENTFORE
    THROLE_HOT                  ' hover fill
    THROLE_SELECTION            ' selection fill

    ' --- signal ---
    THROLE_ERROR
    THROLE_WARNING
    THROLE_SUCCESS

    ' --- syntax ---
    THROLE_KEYWORD              ' -> editor.keyword   (FreeBASIC language keywords)
    THROLE_APIKEYWORD           ' -> editor.keyword2  (Win32 API names)
    THROLE_USERKEYWORD          ' -> editor.keyword3  (the user's extra keyword list)
    THROLE_COMMENT
    THROLE_STRING
    THROLE_NUMBER
    THROLE_OPERATOR
    THROLE_PREPROCESSOR

    THROLE_COUNT
end enum


' The resolved palette. One COLORREF per role, in enum order, so a role id indexes it.
type THEME_ROLES
    clr( 0 to THROLE_COUNT - 1 ) as COLORREF
end type


' A syntax style: colour plus the three attribute flags.
type THSTYLE
    forecolor as COLORREF
    backcolor as COLORREF
    bold      as boolean
    italic    as boolean
    underline as boolean
end type

' The fore/back pattern that repeats across every namespace.
type THPAIR
    forecolor as COLORREF
    backcolor as COLORREF
end type


' ---------------------------------------------------------------------------------------
' general -- the only non-colour namespace. Text payloads, not channels.
' ---------------------------------------------------------------------------------------
type THEME_GENERAL
    description as DWSTRING
    version     as DWSTRING
end type


' ---------------------------------------------------------------------------------------
' compile -- the two build-status glyph colours in the status bar.
' ---------------------------------------------------------------------------------------
type THEME_COMPILE
    iconsuccess as COLORREF
    iconfail    as COLORREF
end type


' ---------------------------------------------------------------------------------------
' main -- frmMain's own client area. One fill.
' ---------------------------------------------------------------------------------------
type THEME_MAIN
    panel as COLORREF
end type


' ---------------------------------------------------------------------------------------
' editor -- Scintilla, its margins and markers, plus the chrome that has always been dressed
' from these keys (the autocomplete popup, the Options textboxes, every Ps* border).
' ---------------------------------------------------------------------------------------
type THEME_EDITOR
    ' the ten syntax styles
    text         as THSTYLE
    comments     as THSTYLE
    keyword1     as THSTYLE
    keyword2     as THSTYLE
    keyword3     as THSTYLE
    numbers      as THSTYLE
    operators    as THSTYLE
    preprocessor as THSTYLE
    strings      as THSTYLE
    linenumbers  as THSTYLE

    ' surface and cursor
    caret        as COLORREF
    currentline  as COLORREF
    selection    as THPAIR
    ' -1 means "the theme did not ask for an alpha", and every consumer then keeps doing
    ' exactly what it does today -- Scintilla's own default for the selection, and the
    ' hardcoded 80/90 for the two occurrence indicators. That sentinel is what lets alpha
    ' land without changing a single pixel until a theme opts in.
    selectionalpha as long = -1
    occurrence     as COLORREF
    occurrencealpha as long = -1

    ' margins and folding
    foldmargin   as COLORREF
    foldsymbol   as THPAIR
    indentguides as THPAIR

    ' markers and highlights
    bracegood    as COLORREF
    bracebad     as COLORREF
    bookmark     as THPAIR
    breakpoint   as THPAIR
    debugline    as THPAIR

    ' editor chrome
    scrollbar    as THPAIR
    divider      as COLORREF
    ' The right-margin column guide. SCI_SETEDGECOLOUR was never sent anywhere, so this
    ' drew in Scintilla's built-in #C0C0C0 in every theme, light or dark.
    edge         as COLORREF
    ' Visible whitespace dots. SCI_SETWHITESPACEFORE/BACK were never sent either.
    whitespace   as THPAIR
    ' Multi-cursor. SCI_SETADDITIONALSELBACK / SETADDITIONALCARETFORE were never sent, so
    ' every selection after the first drew in Scintilla's defaults.
    additionalselection as COLORREF
    additionalcaret     as COLORREF
end type


' ---------------------------------------------------------------------------------------
' tooltip -- hover tips, code tips and the debugger's data tips.
'
' These had no keys of their own: the five-line recipe was hand-copied into frmThemes,
' frmUserTools, frmDebug and modCodetipTip, and the last two had already drifted apart on
' the title colour. One home for it now.
' ---------------------------------------------------------------------------------------
type THEME_TOOLTIP
    text   as THPAIR
    border as COLORREF
    title  as COLORREF
    glyph  as COLORREF
end type


' ---------------------------------------------------------------------------------------
' ui -- chrome that is NOT the editor. Split out so retuning code colours stops silently
' restyling every dialog.
' ---------------------------------------------------------------------------------------
type THEME_UI
    divider   as COLORREF
    field     as THPAIR
    scrollbar as THPAIR
    ' The theme's ACCENT -- the one colour that means "this is on / selected / active".
    ' Bound to the BACKGROUND channel, matching the legacy ui.accent row: it is a fill, never
    ' a foreground. See the note in the theme file about ui.accent falling back to
    ' toptabs.iconselected, which is a pale tint in the light theme and unreadable as text.
    accent    as COLORREF
end type


' ---------------------------------------------------------------------------------------
' panel -- explorer / functions / bookmarks, and the panel menu icon strip.
' ---------------------------------------------------------------------------------------
type THEME_PANEL
    text           as THPAIR
    texthot        as THPAIR
    scrollbar      as THPAIR
    button         as THPAIR
    buttonhot      as THPAIR
    filenameicon   as THPAIR
    functionicon   as THPAIR
    typeicon       as THPAIR
    textsearch     as COLORREF
    textsearchhot  as COLORREF
end type


' ---------------------------------------------------------------------------------------
' topmenu -- every registered PsPopupMenu. NOTE the name: these keys feed the POPUP menus,
' not the menu bar, and always have. menubar.* below is the horizontal bar itself.
' ---------------------------------------------------------------------------------------
type THEME_TOPMENU
    panel        as COLORREF
    text         as THPAIR
    texthot      as THPAIR
    textdisabled as THPAIR
end type


' ---------------------------------------------------------------------------------------
' menubar -- the horizontal PsMenuBar.
' ---------------------------------------------------------------------------------------
type THEME_MENUBAR
    text    as THPAIR
    texthot as THPAIR
end type


' ---------------------------------------------------------------------------------------
' toptabs -- the document tab bar, its menu, and the info / find / replace strips.
' ---------------------------------------------------------------------------------------
type THEME_TOPTABS
    text         as THPAIR
    texthot      as THPAIR
    divider      as COLORREF
    closehot     as COLORREF
    iconselected as COLORREF
end type


' ---------------------------------------------------------------------------------------
' statusbar -- the main status bar.
' ---------------------------------------------------------------------------------------
type THEME_STATUSBAR
    text    as THPAIR
    texthot as THPAIR
end type


' ---------------------------------------------------------------------------------------
' output -- the Output panel, its float window, and the debugger panes.
' ---------------------------------------------------------------------------------------
type THEME_OUTPUT
    text             as THPAIR
    texthot          as THPAIR
    divider          as COLORREF
    closehot         as COLORREF
    scrollbar        as THPAIR
    scrollbardivider as COLORREF
    ' The debugger's "this value changed on the last step" colour. Its own key rather than a
    ' reuse of compile.iconsuccess: that one is 0,128,0 in both shipped themes, a signal
    ' colour for a status glyph and too dark to read as small text on the Output background.
    valuechanged     as COLORREF
end type


' ---------------------------------------------------------------------------------------
' THE ROOT
'
' Ordinary global VALUE, not a pointer: the dotted path reads as written with no "->", it
' cannot be null, and there is no initialisation-order hazard for the code that paints before
' a theme has loaded. Extra instances (snapshot for Cancel, baseline for Reset, the built-in
' defaults) are separate instances of the same type, and whole-struct assignment deep-copies
' the DWSTRING members -- measured over 2,000 cycles in the Phase 0 spike, and already relied
' on today by Theme_Snapshot.
'
' NEVER take this type byref-as-const. AfxNova's DWSTRING copy constructor is not
' const-correct and a const reference that is then copied corrupts the process heap
' (0xC0000374), surfacing at a later unrelated allocation. See Learnings.md.
'
' NEVER memcpy / clear / ZeroMemory one either -- that would blow away the DWSTRING buffers
' without freeing them. Reset is an assignment from the defaults instance.
' ---------------------------------------------------------------------------------------
' Named THEMEROOT_TYPE, not THEME_TYPE: that name is already taken by the parsed per-key
' record in modThemes.inc, which is a different thing entirely (one file line, with its
' presence flags and macro names). This is the resolved result those entries are poured into.
type THEMEROOT_TYPE
    ' The palette every other member falls back to. Resolved first.
    role      as THEME_ROLES
    general   as THEME_GENERAL
    compile   as THEME_COMPILE
    main      as THEME_MAIN
    editor    as THEME_EDITOR
    ui        as THEME_UI
    panel     as THEME_PANEL
    topmenu   as THEME_TOPMENU
    menubar   as THEME_MENUBAR
    toptabs   as THEME_TOPTABS
    statusbar as THEME_STATUSBAR
    output    as THEME_OUTPUT
    tooltip   as THEME_TOOLTIP
end type

dim shared theme as THEMEROOT_TYPE
