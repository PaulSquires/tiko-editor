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

' =========================================================================================
'  THE THEME KEY TABLE  --  one row per key, expanded more than once.
'
'  THIS FILE IS INCLUDED WITHOUT "once", ON PURPOSE, AND MUST STAY THAT WAY AT EVERY SITE.
'
'  It is expanded repeatedly, each time under a different set of row-macro bodies: once to
'  build the bind table, later to seed the built-in defaults. A "#include once" ANYWHERE
'  breaks it SILENTLY -- the build stays clean, no warning is issued, and the second pass
'  simply produces nothing.
'
'  Measured in the Phase 0 spike, and the direction is counter-intuitive enough to write
'  down: it is the FIRST include that arms the trap. Marking a LATER pass "once" on its own
'  is completely harmless, because fbc only skips a "once" include of a file already MARKED
'  once. So adding "once" to a second pass appears to prove the rule is superstition -- right
'  up until somebody adds it to the first pass too and the bind table comes out empty.
'
'  ---------------------------------------------------------------------------------------
'  ROW MACROS. Which one a key uses is decided by the channels it actually has:
'
'    THTEXTKEY ( key, path, cat, sub )          a DWSTRING payload, not a colour
'    THSTYLEKEY( key, path, cat, sub )          fore + back + bold/italic/underline
'    THPAIRKEY ( key, path, cat, sub )          fore + back
'    THFOREKEY ( key, path, cat, sub )          foreground only
'    THBACKKEY ( key, path, cat, sub )          background only
'    THPAIRAKEY( key, path, apath, cat, sub )   fore + back, with a BACKGROUND alpha
'    THFOREAKEY( key, path, apath, cat, sub )   foreground, with a FOREGROUND alpha
'
'  Seven names rather than one macro with a shape argument. A THPAIR has no bold member, so
'  a single macro would have to run a preprocessor #if INSIDE a macro body to decide whether
'  to emit offsetof(..., path.bold). That behaviour is unproven in fbc and buys nothing --
'  the table still reads as one list either way.
'
'  ARGUMENTS
'    key    the ON-DISK key string. Deliberately still the legacy spelling (editor.keyword,
'           not editor.keyword1) so every existing .theme file keeps loading byte for byte.
'           The MEMBER path may be renamed freely; the two are decoupled precisely so that
'           tidying the code cannot invalidate a user's saved theme.
'    path   the member path relative to THEME_TYPE. Substituted after a '.', so it composes
'           with no token pasting.
'    apath  the member holding the alpha, for the two keys where alpha reaches a real pixel.
'    cat    the group header the theme editor files this key under.
'    sub    the subsystems a live edit of this key must repaint. 0 = derive from the
'           namespace, which is right for all but the seven app-wide keys below.
' =========================================================================================


' ---- general: text payloads, on neither editor page --------------------------------------
THTEXTKEY("general.description", general.description, THEMECAT_OTHER, THSUB_NONE, THROLE_NONE, THROLE_NONE )
THTEXTKEY("general.version",     general.version,     THEMECAT_OTHER, THSUB_NONE, THROLE_NONE, THROLE_NONE )

' ---- compile / main ----------------------------------------------------------------------
THFOREKEY("compile.iconsuccess", compile.iconsuccess, THEMECAT_COMPILE, 0, THROLE_SUCCESS, THROLE_NONE )
THFOREKEY("compile.iconfail",    compile.iconfail,    THEMECAT_COMPILE, 0, THROLE_ERROR, THROLE_NONE )
THBACKKEY("main.panel",          main.panel,          THEMECAT_MAIN,    0, THROLE_NONE, THROLE_BACKGROUND )

' ---- editor: the ten syntax styles --------------------------------------------------------
' editor.text also dresses the autocomplete popup and the Options textboxes, so it reaches
' well beyond Scintilla and cannot take the namespace default.
THSTYLEKEY("editor.text",         editor.text,         THEMECAT_SYNTAX, _
            THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE or THSUB_OPTIONSDLG, THROLE_FOREGROUND, THROLE_BACKGROUND )
THSTYLEKEY("editor.comments",     editor.comments,     THEMECAT_SYNTAX, 0, THROLE_COMMENT, THROLE_BACKGROUND )
THSTYLEKEY("editor.keyword",      editor.keyword1,     THEMECAT_SYNTAX, 0, THROLE_KEYWORD, THROLE_BACKGROUND )
THSTYLEKEY("editor.keyword2",     editor.keyword2,     THEMECAT_SYNTAX, 0, THROLE_APIKEYWORD, THROLE_BACKGROUND )
THSTYLEKEY("editor.keyword3",     editor.keyword3,     THEMECAT_SYNTAX, 0, THROLE_USERKEYWORD, THROLE_BACKGROUND )
THSTYLEKEY("editor.numbers",      editor.numbers,      THEMECAT_SYNTAX, 0, THROLE_NUMBER, THROLE_BACKGROUND )
THSTYLEKEY("editor.operators",    editor.operators,    THEMECAT_SYNTAX, 0, THROLE_OPERATOR, THROLE_BACKGROUND )
THSTYLEKEY("editor.preprocessor", editor.preprocessor, THEMECAT_SYNTAX, 0, THROLE_PREPROCESSOR, THROLE_BACKGROUND )
THSTYLEKEY("editor.strings",      editor.strings,      THEMECAT_SYNTAX, 0, THROLE_STRING, THROLE_BACKGROUND )
THSTYLEKEY("editor.linenumbers",  editor.linenumbers,  THEMECAT_MARGINS, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUND )

' ---- editor: cursor and selection ---------------------------------------------------------
THFOREKEY("editor.caret",        editor.caret,        THEMECAT_CURSOR, 0, THROLE_FOREGROUND, THROLE_NONE )
THBACKKEY("editor.currentline",  editor.currentline,  THEMECAT_CURSOR, 0, THROLE_NONE, THROLE_BACKGROUNDALT )
THPAIRKEY("editor.indentguides", editor.indentguides, THEMECAT_CURSOR, 0, THROLE_BORDER, THROLE_BACKGROUND )
' The two alpha-bearing keys. SCI_SETSELALPHA takes the selection's BACKGROUND alpha and
' SCI_INDICSETALPHA the occurrence indicator's FOREGROUND alpha -- the only two places in the
' editor where an alpha reaches a pixel at all.
THPAIRAKEY("editor.selection",    editor.selection,    editor.selectionalpha, _
            THEMECAT_CURSOR, 0, THROLE_FOREGROUND, THROLE_SELECTION )
THFOREAKEY("editor.occurrence",   editor.occurrence,   editor.occurrencealpha, _
            THEMECAT_MARKERS, 0, THROLE_ACCENT, THROLE_NONE )

' ---- editor: margins and folding ----------------------------------------------------------
THFOREKEY("editor.foldmargin",   editor.foldmargin,   THEMECAT_MARGINS, 0, THROLE_BACKGROUNDALT, THROLE_NONE )
THPAIRKEY("editor.foldsymbol",   editor.foldsymbol,   THEMECAT_MARGINS, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDALT )

' ---- editor: markers and highlights -------------------------------------------------------
THFOREKEY("editor.bracegood",    editor.bracegood,    THEMECAT_MARKERS, 0, THROLE_SUCCESS, THROLE_NONE )
THFOREKEY("editor.bracebad",     editor.bracebad,     THEMECAT_MARKERS, 0, THROLE_ERROR, THROLE_NONE )
THPAIRKEY("editor.bookmark",     editor.bookmark,     THEMECAT_MARKERS, 0, THROLE_ACCENT, THROLE_BACKGROUNDALT )
THPAIRKEY("editor.breakpoint",   editor.breakpoint,   THEMECAT_MARKERS, 0, THROLE_ERROR, THROLE_BACKGROUNDALT )
THPAIRKEY("editor.debugline",    editor.debugline,    THEMECAT_MARKERS, 0, THROLE_FOREGROUND, THROLE_WARNING )

' ---- editor: chrome -----------------------------------------------------------------------
' editor.scrollbar dresses the Options scroll panels and the autocomplete list too.
THPAIRKEY("editor.scrollbar",    editor.scrollbar,    THEMECAT_EDCHROME, _
            THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE or THSUB_OPTIONSDLG, THROLE_FOREGROUNDDIM, THROLE_BACKGROUND )
' editor.divider is THE app-wide hairline: menu separators, the tab-bar border, every Ps*
' control border, message-box frames, the autocomplete frame. Read in 20 files.
THFOREKEY("editor.divider",      editor.divider,      THEMECAT_EDCHROME, THSUB_ALL, THROLE_BORDER, THROLE_NONE )

' ---- ui: chrome split out of the editor namespace, all app-wide ---------------------------
THFOREKEY("ui.divider",          ui.divider,          THEMECAT_SHARED, THSUB_ALL, THROLE_BORDER, THROLE_NONE )
THPAIRKEY("ui.field",            ui.field,            THEMECAT_SHARED, THSUB_ALL, THROLE_FOREGROUND, THROLE_BACKGROUND )
THPAIRKEY("ui.scrollbar",        ui.scrollbar,        THEMECAT_SHARED, THSUB_ALL, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDALT )
' Bound to the BACKGROUND channel: the accent is a fill, never a foreground.
THBACKKEY("ui.accent",           ui.accent,           THEMECAT_SHARED, THSUB_ALL, THROLE_NONE, THROLE_ACCENT )

' ---- panel --------------------------------------------------------------------------------
THPAIRKEY("panel.text",          panel.text,          THEMECAT_PANEL, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDALT )
THPAIRKEY("panel.texthot",       panel.texthot,       THEMECAT_PANEL, 0, THROLE_FOREGROUND, THROLE_HOT )
THPAIRKEY("panel.scrollbar",     panel.scrollbar,     THEMECAT_PANEL, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDALT )
THPAIRKEY("panel.button",        panel.button,        THEMECAT_PANEL, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDALT )
THPAIRKEY("panel.buttonhot",     panel.buttonhot,     THEMECAT_PANEL, 0, THROLE_FOREGROUND, THROLE_HOT )
THPAIRKEY("panel.filenameicon",  panel.filenameicon,  THEMECAT_PANEL, 0, THROLE_ACCENT, THROLE_BACKGROUNDALT )
THPAIRKEY("panel.functionicon",  panel.functionicon,  THEMECAT_PANEL, 0, THROLE_ACCENT, THROLE_BACKGROUNDALT )
THPAIRKEY("panel.typeicon",      panel.typeicon,      THEMECAT_PANEL, 0, THROLE_ACCENT, THROLE_BACKGROUNDALT )
THFOREKEY("panel.textsearch",    panel.textsearch,    THEMECAT_PANEL, 0, THROLE_ACCENT, THROLE_NONE )
THFOREKEY("panel.textsearchhot", panel.textsearchhot, THEMECAT_PANEL, 0, THROLE_ACCENT, THROLE_NONE )

' ---- topmenu: feeds the POPUP menus, not the bar -------------------------------------------
THBACKKEY("topmenu.panel",        topmenu.panel,        THEMECAT_POPUP, 0, THROLE_NONE, THROLE_BACKGROUNDRAISED )
THPAIRKEY("topmenu.text",         topmenu.text,         THEMECAT_POPUP, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDRAISED )
THPAIRKEY("topmenu.texthot",      topmenu.texthot,      THEMECAT_POPUP, 0, THROLE_FOREGROUND, THROLE_HOT )
THPAIRKEY("topmenu.textdisabled", topmenu.textdisabled, THEMECAT_POPUP, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDRAISED )

' ---- menubar -------------------------------------------------------------------------------
THPAIRKEY("menubar.text",        menubar.text,        THEMECAT_MENUBAR, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDALT )
THPAIRKEY("menubar.texthot",     menubar.texthot,     THEMECAT_MENUBAR, 0, THROLE_FOREGROUND, THROLE_HOT )

' ---- toptabs -------------------------------------------------------------------------------
THPAIRKEY("toptabs.text",        toptabs.text,        THEMECAT_TOPTABS, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDALT )
THPAIRKEY("toptabs.texthot",     toptabs.texthot,     THEMECAT_TOPTABS, 0, THROLE_FOREGROUND, THROLE_BACKGROUND )
THFOREKEY("toptabs.divider",     toptabs.divider,     THEMECAT_TOPTABS, 0, THROLE_BORDER, THROLE_NONE )
THBACKKEY("toptabs.closehot",    toptabs.closehot,    THEMECAT_TOPTABS, 0, THROLE_NONE, THROLE_HOT )
THBACKKEY("toptabs.iconselected", toptabs.iconselected, THEMECAT_TOPTABS, 0, THROLE_NONE, THROLE_ACCENT )

' ---- statusbar -----------------------------------------------------------------------------
THPAIRKEY("statusbar.text",      statusbar.text,      THEMECAT_STATUSBAR, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDALT )
THPAIRKEY("statusbar.texthot",   statusbar.texthot,   THEMECAT_STATUSBAR, 0, THROLE_FOREGROUND, THROLE_HOT )

' ---- output ---------------------------------------------------------------------------------
THPAIRKEY("output.text",             output.text,             THEMECAT_OUTPUT, 0, THROLE_FOREGROUND, THROLE_BACKGROUNDALT )
THPAIRKEY("output.texthot",          output.texthot,          THEMECAT_OUTPUT, 0, THROLE_FOREGROUND, THROLE_HOT )
THFOREKEY("output.divider",          output.divider,          THEMECAT_OUTPUT, 0, THROLE_BORDER, THROLE_NONE )
THBACKKEY("output.closehot",         output.closehot,         THEMECAT_OUTPUT, 0, THROLE_NONE, THROLE_HOT )
THPAIRKEY("output.scrollbar",        output.scrollbar,        THEMECAT_OUTPUT, 0, THROLE_FOREGROUNDDIM, THROLE_BACKGROUNDALT )
THFOREKEY("output.scrollbardivider", output.scrollbardivider, THEMECAT_OUTPUT, 0, THROLE_BORDER, THROLE_NONE )
THFOREKEY("output.valuechanged",     output.valuechanged,     THEMECAT_OUTPUT, 0, THROLE_WARNING, THROLE_NONE )
