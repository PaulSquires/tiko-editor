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

' The resolved theme as ONE structure -- theme.editor.keyword1.forecolor and friends. Loaded
' first because the generated key table below names its members.
#include once "modThemeTypes.bi"


type THEME_TYPE
    keyValue as DWSTRING
    keyData as DWSTRING
    foreground as COLORREF
    background as COLORREF
    bold as long
    italic as long
    underline as long
    ' Alpha is carried ALONGSIDE the COLORREF, never folded into its high byte -- that byte
    ' is palette-relative (0x01) / palette-index (0x02) to GDI, so a 255 stuffed in there is
    ' actively wrong. Same rule PsBufferPaint already states for its own colours. A value of
    ' "r,g,b" leaves these at 255 and simply means opaque.
    foregroundAlpha as ubyte = 255
    backgroundAlpha as ubyte = 255
    ' Presence flags. A COLORREF of zero is legitimately black, so without these there is
    ' no way to tell "the key was absent" from "the key was explicitly 0,0,0". The ui.*
    ' fallback rule (Theme_ResolveFallbacks) has to be able to tell those apart, and so
    ' does the self-test's bind-table completeness check.
    hasFore as boolean
    hasBack as boolean
    hasFlags as boolean          ' any one of bold/italic/underline was seen
    hasForeAlpha as boolean      ' an explicit 4th component was supplied
    hasBackAlpha as boolean
    ' WHICH MACRO a channel's value came from, "%NAME", or "" for a literal.
    '
    ' The parser used to expand %MACRO and throw the name away, so the writer had nothing to
    ' write back and the first save of a hand-authored theme silently flattened every macro
    ' into literal r,g,b. Keeping the name is what makes the format round-trip.
    '
    ' Editing a channel CLEARS its name (see Theme_SetColor): the edited key detaches to a
    ' literal and the macro -- and every other key still using it -- is left alone. That is
    ' the deliberate rule; the inverse action, re-attaching a key to a macro, is a separate
    ' explicit command rather than something an edit does by accident.
    foreMacro as DWSTRING
    backMacro as DWSTRING
    ' Did this channel come from the ROLE layer rather than from the file?
    '
    ' Without this the writer cannot tell "the author wrote this colour" from "we filled it
    ' in from role.accent", because role resolution sets hasFore/hasBack exactly as a parse
    ' does. It would then emit every role-derived value as a literal, and the first save of a
    ' roles-only theme would flatten the whole palette into 140 hard-coded lines -- the same
    ' failure the %MACRO round-trip exists to prevent, one layer up.
    foreFromRole as boolean
    backFromRole as boolean
end type


' ----------------------------------------------------------------------------------------
' The theme's %MACRO definitions, in the order the file declared them. This is the palette:
' a handful of named colours the keys refer to. Rebuilt on every parse.
' ----------------------------------------------------------------------------------------
type THEMEMACRO_TYPE
    name  as DWSTRING        ' including the leading '%'
    value as DWSTRING        ' the raw right-hand side, e.g. "40,44,52"
end type
dim shared gThemeMacros(any) as THEMEMACRO_TYPE
dim shared gTheme(any) as THEME_TYPE
dim shared gThemeFilenames(any) as DWSTRING

' Which theme is CURRENTLY IN MEMORY. Deliberately not gConfig.ThemeShortFilename: during a
' live preview the Options dialog has staged the user's pick in gOptWork and gConfig still
' names the old theme, so anything deriving from the filename -- the DWM dark caption above
' all -- has to read this instead or it lags the preview and only catches up on OK.
dim shared gThemeCurrentShortFilename as DWSTRING

' Keys the parser accepted but could do nothing with -- a mistyped third segment
' ("editor.text.forground") parses perfectly happily and then sets nothing at all, which
' is invisible without this. Rebuilt on every parse; asserted empty by the self-test.
dim shared gThemeParseWarnings(any) as DWSTRING


''
''  SUBSYSTEMS
''
''  Which parts of the app a given theme key feeds. A live colour edit re-applies only the
''  subsystems its key touches rather than the whole app, which is what keeps a drag cheap.
''
enum THEME_SUBSYS
    THSUB_NONE          = 0
    THSUB_SCINTILLA     = &h00000001   ' clsDocument.ApplyProperties over gApp.pDocList
    THSUB_EDITORCHROME  = &h00000002   ' editor scrollbars, splitters, frmMain background
    THSUB_PANEL         = &h00000004   ' explorer / functions / bookmarks + panel menu
    THSUB_TOPTABS       = &h00000008   ' tab bar, tabs menu, info / find / replace strips
    THSUB_MENUBAR       = &h00000010   ' PsMenuBar itself
    THSUB_POPUP         = &h00000020   ' every registered PsPopupMenu
    THSUB_STATUSBAR     = &h00000040   ' main status bar (+ the compile icons)
    THSUB_OUTPUT        = &h00000080   ' frmOutput panes, textboxes, scrollbar, selectbar
    THSUB_AUTOCOMPLETE  = &h00000100
    THSUB_OPTIONSDLG    = &h00000200   ' frmOptions shell + rows + the panel-hosted pages
    THSUB_HELPCENTER    = &h00000400   ' the WebView2 help window's CAPTION only
    THSUB_DEBUG         = &h00000800
    THSUB_SEARCHSYMBOL  = &h00001000
    THSUB_FRAME         = &h00002000   ' DWM caption + frmMain client
    THSUB_ALL           = &h00003FFF
end enum


''
''  KEY -> DESTINATION BINDING
''
''  One row per theme key, holding pointers to the gh* fields that key feeds AND the
''  subsystem mask it drives. Deliberately ONE table rather than two: a key with no row here
''  never loads at all, including at startup, so a missing entry fails loudly on first run
''  instead of silently refusing to update during a live edit.
''
''  theme.general.description / .version are the two exceptions -- they carry DWSTRING payloads
''  rather than colours; the key table carries them through its ofsText channel.
''
' THEMEBIND_TYPE and gThemeBind are GONE. The hand-written bind table's only job was
' filling the ten gh* globals; with every read site migrated to the theme root, the
' generated key table (modThemeKeys.bi) is the single answer to what is in a theme.

' ghGeneral is GONE. Its five fields moved to where their KEYS always said they belonged,
' which the single struct made obvious and the old flat one hid:
'     description  -> theme.general.description
'     version      -> theme.general.version
'     iconfail     -> theme.compile.iconfail
'     iconsuccess  -> theme.compile.iconsuccess
'     debugchanged -> theme.output.valuechanged
'
' That last one is the debugger's "this value changed on the last step" colour. It has its
' own key rather than reusing iconsuccess: that one is 0,128,0 in both shipped themes, a
' signal colour for a status glyph and too dark to read as small text on the Output
' background. It wants to catch the eye without shouting -- it appears on several rows at
' once while stepping a loop.

' ghMain is GONE -- migrated to theme.main.panel. Deleting the group is what turns fbc into
' the checklist: a read site left behind becomes "error 41: Variable not declared" rather
' than quietly carrying on against a struct nothing fills any more.

' ghEditor is GONE -- migrated to theme.editor.*, the last of the ten.

' ghPanel is GONE -- migrated to theme.panel.*

' ghMenuBar is GONE -- migrated to theme.menubar.text.* / .texthot.*
' ghPopup is GONE -- migrated to theme.topmenu.*. Note the namespace: these keys have
' always fed the POPUP menus rather than the horizontal bar, which the flat gh* naming
' actively disguised (ghPopup fed by topmenu.*, ghMenuBar by menubar.*).

' ghTopTabs is GONE -- migrated to theme.toptabs.*

' ghStatusBar is GONE -- migrated to theme.statusbar.text.* / .texthot.*

' ghOutput is GONE -- migrated to theme.output.*

' Chrome that is not the editor but has always been dressed FROM the editor keys, because
' there was nowhere else to put it: the app-wide hairline, the fill/text of every PsTextBox,
' and the Options scroll panels. Splitting the theme UI into "Syntax" and "Interface" pages
' makes that borrowing untenable -- a user retuning code colours would silently restyle the
' Options dialog. These keys give the chrome a home of its own.
'
' NOTHING READS THESE YET. Theme_ResolveFallbacks fills them from the editor.* values when a
' theme does not define them, so every existing theme keeps working byte-for-byte; repointing
' the ~20 consumers is a separate, reviewable patch and deliberately not part of this one.
' ghUI is GONE -- migrated to theme.ui.*. Its four field/scrollbar members finally have
' consumers: see the chrome cleanup in Phase 4.


''
''  WHICH CHANNEL OF A KEY IS BEING READ OR WRITTEN
''
enum THEME_CHANNEL
    THEMECH_FORE   = 0
    THEMECH_BACK   = 1
    THEMECH_BOLD   = 2
    THEMECH_ITALIC = 3
    THEMECH_UNDER  = 4
end enum



''
''  RETRIEVE THEME ITEM FROM ARRAY
''
function getThemePtr( byval lookupKey as string ) as THEME_TYPE ptr
    ' we do a simple linear search for the theme because there will be a small
    ' number of theme elements. Maybe in the future put this in a hash array for
    ' faster lookups should we need the performance boost.
    ' in case search fails return pointer to this dummy type to avoid null ptr
    static dummy as THEME_TYPE    
    for i as long = lbound(gTheme) to ubound(gTheme)
        if gTheme(i).keyValue = lookupKey then
            return @gTheme(i)
        end if
    next
    return @dummy
end function


''
''  FIND A gTheme() ENTRY BY KEY, OR -1
''
''  The mutation path must use THIS and not getThemePtr. getThemePtr answers a miss with a
''  pointer to a single shared `static dummy` that is never reset -- harmless while everything
''  is read-only, but the first write through it would poison every later miss too.
''
function Theme_FindEntry( byval lookupKey as string ) as long
    for i as long = lbound(gTheme) to ubound(gTheme)
        if gTheme(i).keyValue = lookupKey then return i
    next
    return -1
end function


''
''  FIND A gTheme() ENTRY BY KEY, CREATING AN EMPTY ONE IF ABSENT
''
private function Theme_EnsureEntry( byval lookupKey as string ) as long
    dim as long n = Theme_FindEntry( lookupKey )
    if n >= 0 then return n
    n = ubound(gTheme) + 1
    redim preserve gTheme(n) as THEME_TYPE
    gTheme(n).keyValue = lookupKey
    return n
end function


''
''  ui.* FALLBACK
''
''  One row per (destination key, channel) that can be inherited from an editor.* key. The
''  table is read in BOTH directions: forwards here, and backwards by the theme writer, which
''  emits the legacy editor.* line alongside the ui.* one so an older tiko still renders a
''  theme this version wrote.
''
type THEMEFALLBACK_TYPE
    dstKey as zstring * 40
    srcKey as zstring * 40
    nChan  as long
end type

dim shared as THEMEFALLBACK_TYPE gThemeFallback(0 to 5) = { _
    ( "ui.divider",   "editor.divider",   THEMECH_FORE ), _
    ( "ui.field",     "editor.text",      THEMECH_FORE ), _
    ( "ui.field",     "editor.text",      THEMECH_BACK ), _
    ( "ui.scrollbar", "editor.scrollbar", THEMECH_FORE ), _
    ( "ui.scrollbar", "editor.scrollbar", THEMECH_BACK ), _
    ( "ui.accent",    "toptabs.iconselected", THEMECH_BACK ) }


''
''  MATERIALISE ANY ui.* KEY THE THEME DID NOT SUPPLY
''
''  Runs after the parse and before the fill. It creates a REAL gTheme() entry rather than
''  papering over the gap at read time, so the fill, the routing, the mutation API and the
''  writer never have to know that fallback exists. Idempotent, and safe to re-run after any
''  edit -- an explicitly-supplied value is never overwritten, which is exactly what the
''  has* presence flags are for (a value of 0,0,0 is legitimate black, not "absent").
''
''
''  THE ROLE PALETTE: NAME, ENUM AND BUILT-IN DEFAULT
''
''  Filled at RUNTIME, never as a static initialiser. CLAUDE.md records gas64 silently
''  corrupting static wstring array initialisers, and a table of role names is exactly the
''  shape that bug takes.
''
''  The defaults are the dark palette, chosen so a theme file that defines NOTHING still
''  renders a coherent editor rather than a black rectangle. That is what makes a roles-only
''  theme legal, and it is also what finally removes the app's dependency on
''  default_dark.theme existing on disk.
''
type THEMEROLEDEF_TYPE
    name    as zstring * 32      ' the on-disk key, e.g. "role.background"
    nRole   as long
    clrDflt as COLORREF
end type
dim shared gThemeRoleDef(any) as THEMEROLEDEF_TYPE

private sub ThemeRole_Add( byval k as zstring ptr, byval r as long, byval c as COLORREF )
    dim as long n = ubound(gThemeRoleDef) + 1
    redim preserve gThemeRoleDef(n) as THEMEROLEDEF_TYPE
    gThemeRoleDef(n).name    = *k
    gThemeRoleDef(n).nRole   = r
    gThemeRoleDef(n).clrDflt = c
end sub

sub Theme_BuildRoleTable()

    static as boolean bBuilt
    if bBuilt then exit sub
    bBuilt = true
    erase gThemeRoleDef

    '                name                      role                     default (r,g,b)
    ThemeRole_Add( "role.background",       THROLE_BACKGROUND,       BGR( 40, 44, 52) )
    ThemeRole_Add( "role.backgroundalt",    THROLE_BACKGROUNDALT,    BGR( 33, 37, 43) )
    ThemeRole_Add( "role.backgroundraised", THROLE_BACKGROUNDRAISED, BGR( 33, 37, 43) )
    ThemeRole_Add( "role.foreground",       THROLE_FOREGROUND,       BGR(215,218,224) )
    ThemeRole_Add( "role.foregrounddim",    THROLE_FOREGROUNDDIM,    BGR(157,165,180) )
    ThemeRole_Add( "role.border",           THROLE_BORDER,           BGR( 62, 68, 81) )
    ThemeRole_Add( "role.accent",           THROLE_ACCENT,           BGR( 49, 73,123) )
    ThemeRole_Add( "role.accentfore",       THROLE_ACCENTFORE,       BGR( 97,175,239) )
    ThemeRole_Add( "role.hot",              THROLE_HOT,              BGR( 62, 68, 81) )
    ThemeRole_Add( "role.selection",        THROLE_SELECTION,        BGR( 62, 68, 81) )
    ThemeRole_Add( "role.error",            THROLE_ERROR,            BGR(224,108,117) )
    ThemeRole_Add( "role.warning",          THROLE_WARNING,          BGR(229,192,123) )
    ThemeRole_Add( "role.success",          THROLE_SUCCESS,          BGR(152,195,121) )
    ThemeRole_Add( "role.keyword",          THROLE_KEYWORD,          BGR(198,120,221) )
    ThemeRole_Add( "role.apikeyword",       THROLE_APIKEYWORD,       BGR( 86,182,194) )
    ThemeRole_Add( "role.userkeyword",      THROLE_USERKEYWORD,      BGR(229,192,123) )
    ThemeRole_Add( "role.comment",          THROLE_COMMENT,          BGR( 92, 99,112) )
    ThemeRole_Add( "role.string",           THROLE_STRING,           BGR(152,195,121) )
    ThemeRole_Add( "role.number",           THROLE_NUMBER,           BGR(209,154,102) )
    ThemeRole_Add( "role.operator",         THROLE_OPERATOR,         BGR(171,178,191) )
    ThemeRole_Add( "role.preprocessor",     THROLE_PREPROCESSOR,     BGR(198,120,221) )

end sub


''
''  RESOLVE THE PALETTE
''
''  A role named in the file wins; otherwise the built-in default. Writes straight into the
''  root's role array, which is what every widget fallback below then reads.
''
sub Theme_ResolveRoles()

    Theme_BuildRoleTable()

    for i as long = lbound(gThemeRoleDef) to ubound(gThemeRoleDef)
        dim as long r = gThemeRoleDef(i).nRole
        dim as long n = Theme_FindEntry( gThemeRoleDef(i).name )
        ' A role carries ONE colour and the file states it as a foreground.
        if (n >= 0) andalso gTheme(n).hasFore then
            theme.role.clr(r) = gTheme(n).foreground
        else
            theme.role.clr(r) = gThemeRoleDef(i).clrDflt
        end if
    next

end sub


' Theme_ResolveKeyRoles is defined further down, after the key table it walks.
declare sub Theme_ResolveKeyRoles()



sub Theme_ResolveFallbacks()

    ' The palette first: everything below can inherit from it.
    Theme_ResolveRoles()

    for i as long = lbound(gThemeFallback) to ubound(gThemeFallback)

        dim as long nSrc = Theme_FindEntry( gThemeFallback(i).srcKey )
        if nSrc < 0 then continue for

        dim as long nDst = Theme_EnsureEntry( gThemeFallback(i).dstKey )

        if gThemeFallback(i).nChan = THEMECH_FORE then
            if gTheme(nDst).hasFore = false then
                gTheme(nDst).foreground      = gTheme(nSrc).foreground
                gTheme(nDst).foregroundAlpha = gTheme(nSrc).foregroundAlpha
                gTheme(nDst).hasForeAlpha    = gTheme(nSrc).hasForeAlpha
                gTheme(nDst).foreMacro       = gTheme(nSrc).foreMacro
                gTheme(nDst).hasFore         = true
            end if
        else
            if gTheme(nDst).hasBack = false then
                gTheme(nDst).background      = gTheme(nSrc).background
                gTheme(nDst).backgroundAlpha = gTheme(nSrc).backgroundAlpha
                gTheme(nDst).hasBackAlpha    = gTheme(nSrc).hasBackAlpha
                gTheme(nDst).backMacro       = gTheme(nSrc).backMacro
                gTheme(nDst).hasBack         = true
            end if
        end if

    next

    ' ...and finally the general net: anything still absent takes its declared role.
    Theme_ResolveKeyRoles()

end sub


''
''  RESOLVE A THEME SHORT FILENAME TO A FULL PATH
''
''  Applies the historical fallback chain: the requested theme, then default_dark, then the
''  backup copy in settings\ (which is also copied back into themes\ so the next run finds
''  it there). Returns "" only if even the backup is missing.
''
function Theme_ResolvePath( byval wszShortFilename as DWSTRING ) as DWSTRING

    dim as DWSTRING wszFilename = PsExePath & "settings\themes\" & wszShortFilename
    if PsFileExists(wszFilename) = 0 then
        ' default to the visual studio dark theme
        wszFilename = PsExePath & "settings\themes\default_dark.theme"
        if PsFileExists(wszFilename) = 0 then
            ' If the theme still does not exist then most likely the Themes folder is
            ' missing or the theme was renamed. Use the backup version in the settings folder.
            wszFilename = PsExePath & "settings\backup_default_dark.theme"
            ' Attempt to copy the file for next time
            dim as DWSTRING wszThemeDirFilename = PsExePath & "settings\themes\default_dark.theme"
            if PsFileCopy( wszFilename, wszThemeDirFilename ) then
            gConfig.ThemeShortFilename = "default_dark.theme"
            end if
        end if
        if PsFileExists(wszFilename) = 0 then return ""
    end if

    return wszFilename

end function


''
''  PARSE A THEME DISK FILE INTO gTheme()
''
''  File -> gTheme() and nothing else: no global fill, no repaint, no DWM. Keeping this half
''  free of side effects is what lets the theme editor re-parse, mutate and re-apply without
''  ever touching a window. Returns TRUE on error -- LoadThemeFile's original convention.
''
function Theme_ParseFile( byval wszFullPath as DWSTRING ) as boolean

    redim as DWSTRING lines_(any)
    if TextFile_ReadLines(wszFullPath, lines_()) = false then return true   ' error

    ' Ensure to delete any existing gTheme data
    erase gTheme
    erase gThemeParseWarnings
    erase gThemeMacros

    
    dim as DWSTRING wst, keyValue, keyData
        
    for nLine_ as long = lbound(lines_) to ubound(lines_)
        wst = lines_(nLine_)

        wst = trim(AfxStrExtract( 1, wst, "#"))     ' remove comments
        If PsLen(wst) = 0 Then continue for

        keyValue = PsTrim(PsStrParse(wst, 1, ":"))
        keyData = PsTrim(PsStrParse(wst, 2, ":"))
        
        ' is this a replaceable parameter
        if PsLeft(wst, 1) = "%" then
            ' Kept in a dynamic table now rather than a fixed dim(100), so there is no bound
            ' to overrun -- and, more to the point, so the definitions survive the parse and
            ' the writer can put them back.
            dim as long nm = ubound(gThemeMacros) + 1
            redim preserve gThemeMacros(nm) as THEMEMACRO_TYPE
            gThemeMacros(nm).name  = keyValue
            gThemeMacros(nm).value = keyData
            continue for
        end if

        ' if keyData is a replaceable parameter then substitute it now, REMEMBERING which
        ' macro it was so the value can be written back as %NAME rather than flattened.
        dim as DWSTRING wszMacroUsed
        if PsLeft(keyData, 1) = "%" then
            for i as long = lbound(gThemeMacros) to ubound(gThemeMacros)
                if gThemeMacros(i).name = keyData then
                    wszMacroUsed = keyData
                    keyData = gThemeMacros(i).value
                    exit for
                end if
            next
            ' An unresolved %NAME is a typo or a forward reference (macros must be declared
            ' before use). Say so rather than letting val() quietly read it as 0,0,0 black.
            if PsLen(wszMacroUsed) = 0 then
                redim preserve gThemeParseWarnings(ubound(gThemeParseWarnings) + 1)
                gThemeParseWarnings(ubound(gThemeParseWarnings)) = "undefined macro: " & keyData
            end if
        end if
        
        ' is the KeyData a true or false value
        if keyData = "true" then keyData = "1"
        if keyData = "false" then keyData = "0"
        
        ' determine number of keys in the keyvalue. There can be 2 or 3.
        ' if 2 then simply store the data in keyData. if 3 then break the
        ' data into the component properties.
        dim keys(3) as string   ' always have maximum of 3 keys
        dim as long numKeys = PsStrParseCount(keyValue, ".")  ' actual number of keys retrieved
        for i as long = 1 to numKeys
            keys(i) = PsStrParse(keyValue, i, ".").Utf8
        next
        ' there will always be at least 2 elements to the key and
        ' we use that to lookup the theme type structure.
        keyValue = keys(1) & "." & keys(2)
        
        ' generate the color value. An optional 4th component is alpha (0-255); its absence
        ' means opaque. The component COUNT is what distinguishes the two -- val("") is 0,
        ' i.e. fully transparent, which is the exact opposite of what "r,g,b" means.
        dim as long r, g, b, clr
        r = PsVal(PsStrParse(keyData, 1, ","))
        g = PsVal(PsStrParse(keyData, 2, ","))
        b = PsVal(PsStrParse(keyData, 3, ","))
        clr = BGR(r, g, b)

        dim as long nAlpha = 255
        dim as boolean bHasAlpha = false
        if PsStrParseCount(keyData, ",") >= 4 then
            nAlpha = PsVal(PsStrParse(keyData, 4, ","))
            if nAlpha < 0 then nAlpha = 0
            if nAlpha > 255 then nAlpha = 255
            bHasAlpha = true
        end if
        
        ' determine if the key already exists and just needs
        ' the properties added to it, or if the structure needs
        ' to be created.
        dim nFound as long = -1
        for i as long = lbound(gTheme) to ubound(gTheme)
            if gTheme(i).keyValue = keyValue then
            nFound = i: exit for
            end if
        next
        
        ' add the new key/value pair if not found
        if nFound = -1 then
            nFound = ubound(gTheme) + 1
            redim preserve gTheme(nFound) as THEME_TYPE
        end if   

        gTheme(nFound).keyValue = keyValue
        gTheme(nFound).keyData = keyData

        ' role.* IS A COLOUR DESPITE HAVING ONLY TWO SEGMENTS.
        '
        ' A role carries exactly one colour, so "role.accent: 49,73,123" is the natural
        ' syntax and the third segment would be noise on every line of a palette. But the
        ' two-segment path below is the TEXT path -- it is what general.description takes --
        ' so without this arm a role parses perfectly happily, stores its digits as a string,
        ' sets no colour, and silently falls through to the built-in default. That is exactly
        ' what happened: a full palette was added to both shipped themes and changed nothing
        ' at all, with a clean build and 636 assertions passing.
        if (numKeys = 2) andalso (keys(1) = "role") then
            gTheme(nFound).foreground = clr
            gTheme(nFound).hasFore    = true
            if bHasAlpha then
                gTheme(nFound).foregroundAlpha = cubyte(nAlpha)
                gTheme(nFound).hasForeAlpha    = true
            end if
            gTheme(nFound).foreMacro = wszMacroUsed
            continue for
        end if

        select case keys(3)
            case "foreground"
                gTheme(nFound).foreground = clr
                gTheme(nFound).hasFore = true
                gTheme(nFound).foregroundAlpha = cubyte(nAlpha)
                gTheme(nFound).hasForeAlpha = bHasAlpha
                gTheme(nFound).foreMacro = wszMacroUsed
            case "background"
                gTheme(nFound).background = clr
                gTheme(nFound).hasBack = true
                gTheme(nFound).backgroundAlpha = cubyte(nAlpha)
                gTheme(nFound).hasBackAlpha = bHasAlpha
                gTheme(nFound).backMacro = wszMacroUsed
            case "bold":       gTheme(nFound).bold = val(keyData)        : gTheme(nFound).hasFlags = true
            case "italic":     gTheme(nFound).italic = val(keyData)      : gTheme(nFound).hasFlags = true
            case "underline":  gTheme(nFound).underline = val(keyData)   : gTheme(nFound).hasFlags = true
            case else
                ' A two-part key (general.description, general.version) has no third
                ' segment by design and has already stored its payload in keyData above.
                ' A three-part key landing here is a typo, and is otherwise silent.
                if numKeys >= 3 then
                    redim preserve gThemeParseWarnings(ubound(gThemeParseWarnings) + 1)
                    gThemeParseWarnings(ubound(gThemeParseWarnings)) = keyValue & "." & keys(3)
                end if
        end select
    next
    

    return false

end function


''
''  DEFAULT SUBSYSTEM MASK FOR A KEY, FROM ITS NAMESPACE
''
''  Almost every key is answered correctly by its first segment alone, which is the point:
''  the fewer masks are hand-written, the fewer can be hand-written WRONG. Only the genuinely
''  cross-cutting keys pass an explicit mask to ThemeBind_Add.
''
''  These err GENEROUS on purpose. Over-inclusion costs one redundant re-apply of a
''  subsystem that did not need it; under-inclusion is a colour that silently refuses to
''  update, which is invisible to every assertion in the self-test.
''
private function Theme_SubsysForNamespace( byval k as string ) as ulong

    dim as string ns = PsStrParse( k, 1, "." ).Utf8

    select case ns
        ' frmAutoComplete derives its row colours from the editor styles (keyword2, keyword3,
        ' numbers, comments, selection, currentline), so the whole namespace feeds it.
        case "editor"    : return THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE
        ' the panel namespace dresses the Options dialog and the symbol search popup as well as the panels.
        case "panel"     : return THSUB_PANEL or THSUB_OPTIONSDLG or THSUB_SEARCHSYMBOL
        case "toptabs"   : return THSUB_TOPTABS
        case "menubar"   : return THSUB_MENUBAR
        case "topmenu"   : return THSUB_POPUP
        case "statusbar" : return THSUB_STATUSBAR
        case "output"    : return THSUB_OUTPUT
        case "main"      : return THSUB_FRAME
        case "compile"   : return THSUB_STATUSBAR      ' the build success/fail icons live there
        case "ui"        : return THSUB_ALL
        case "general"   : return THSUB_NONE           ' description/version, not colours
    end select

    return THSUB_NONE

end function


''
''  ADD ONE BIND ROW
''
' ThemeBind_Add and Theme_BuildBindTable are GONE -- see Theme_BuildKeyTable below,
' which generates the same information from modThemeKeys.bi and cannot drift from it.


''
''  ============================ THE GENERATED KEY TABLE ============================
''
''  The replacement for Theme_BuildBindTable above, generated from modThemeKeys.bi rather
''  than hand-written. Both exist at once for now: the hand-written table still fills the
''  legacy gh* globals, this one fills the new "theme" root, and the self-test asserts the
''  two agree key for key. Once the 1,266 read sites have moved, the hand-written table and
''  the gh* globals go and this is all that is left.
''
''  OFFSETS, NOT POINTERS, and that is the whole reason for a second table rather than an
''  extra column on the first. A pointer can only ever address the ONE live instance; an
''  offset addresses whichever THEMEROOT_TYPE it is handed, so the identical code fills the
''  live theme, the Cancel snapshot, the Reset baseline and the built-in defaults.
''
''  SENTINEL IS -1, NEVER 0. Offset 0 is a real member (general.description). The legacy
''  table's null-pointer-means-absent test does not survive the translation.
''
type THEMEKEY_TYPE
    keyValue     as zstring * 40
    ofsText      as long          ' a DWSTRING payload rather than a colour
    ofsFore      as long
    ofsBack      as long
    ofsBold      as long
    ofsItalic    as long
    ofsUnder     as long
    ofsForeAlpha as long
    ofsBackAlpha as long
    ' The role each channel falls back to when the file does not name this key.
    nRoleFore    as long
    nRoleBack    as long
    nCat         as long          ' the theme editor's group header
    nSubsys      as ulong
    ' Can an alpha on this key actually reach a pixel? Having an alpha destination IS the
    ' definition, so it is derived rather than declared -- the colour picker asks this to
    ' decide whether to offer an alpha slider at all.
    bAlphaCapable as boolean
end type
dim shared gThemeKeys(any) as THEMEKEY_TYPE


''
''  BUILD IT
''
''  modThemeKeys.bi is included WITHOUT "once" -- see the warning in its header. It is
''  expanded again elsewhere under different row-macro bodies, and a "once" at any single
''  site breaks every later expansion silently, with a clean build and no warning.
''
private sub Theme_BuildKeyTable()

    static as boolean bBuilt
    if bBuilt then exit sub
    bBuilt = true

    erase gThemeKeys

    ' Every row runs this first: grow the array and fill the fields common to all shapes,
    ' defaulting every channel to "absent". Each row macro then claims only its own channels.
    #macro THKEY_BEGIN( k, cat, sub_, rF, rB )
        scope
            dim as long n = ubound(gThemeKeys) + 1
            redim preserve gThemeKeys(n) as THEMEKEY_TYPE
            with gThemeKeys(n)
                .keyValue     = k
                .ofsText      = -1
                .ofsFore      = -1
                .ofsBack      = -1
                .ofsBold      = -1
                .ofsItalic    = -1
                .ofsUnder     = -1
                .ofsForeAlpha = -1
                .ofsBackAlpha = -1
                .nRoleFore    = rF
                .nRoleBack    = rB
                .nCat         = cat
                ' Zero means "derive from the namespace", exactly as the legacy table does.
                '
                ' The parentheses around sub_ are LOAD-BEARING. A macro argument is pasted in
                ' as raw tokens, and the two multi-subsystem keys pass an expression:
                '   THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE or THSUB_OPTIONSDLG
                ' In FreeBASIC "=" binds TIGHTER than "or", so an unparenthesised sub_ = 0
                ' parses as "A or B or C or (D = 0)" -- a mask silently missing three of its
                ' four bits, which means a live edit of editor.text would repaint Scintilla
                ' and nothing else. Caught by assertion 22c, not by the compiler.
                .nSubsys      = iif( (sub_) = 0, Theme_SubsysForNamespace( k ), (sub_) )
    #endmacro

    #macro THKEY_END()
                .bAlphaCapable = ((.ofsForeAlpha >= 0) orelse (.ofsBackAlpha >= 0))
            end with
        end scope
    #endmacro

    #macro THTEXTKEY( k, path, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsText = offsetof( THEMEROOT_TYPE, path )
        THKEY_END()
    #endmacro

    #macro THSTYLEKEY( k, path, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsFore   = offsetof( THEMEROOT_TYPE, path.forecolor )
            .ofsBack   = offsetof( THEMEROOT_TYPE, path.backcolor )
            .ofsBold   = offsetof( THEMEROOT_TYPE, path.bold )
            .ofsItalic = offsetof( THEMEROOT_TYPE, path.italic )
            .ofsUnder  = offsetof( THEMEROOT_TYPE, path.underline )
        THKEY_END()
    #endmacro

    #macro THPAIRKEY( k, path, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsFore = offsetof( THEMEROOT_TYPE, path.forecolor )
            .ofsBack = offsetof( THEMEROOT_TYPE, path.backcolor )
        THKEY_END()
    #endmacro

    #macro THPAIRAKEY( k, path, apath, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsFore      = offsetof( THEMEROOT_TYPE, path.forecolor )
            .ofsBack      = offsetof( THEMEROOT_TYPE, path.backcolor )
            .ofsBackAlpha = offsetof( THEMEROOT_TYPE, apath )
        THKEY_END()
    #endmacro

    #macro THFOREKEY( k, path, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsFore = offsetof( THEMEROOT_TYPE, path )
        THKEY_END()
    #endmacro

    #macro THFOREAKEY( k, path, apath, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsFore      = offsetof( THEMEROOT_TYPE, path )
            .ofsForeAlpha = offsetof( THEMEROOT_TYPE, apath )
        THKEY_END()
    #endmacro

    #macro THBACKKEY( k, path, cat, sub_, rF, rB )
        THKEY_BEGIN( k, cat, sub_, rF, rB )
            .ofsBack = offsetof( THEMEROOT_TYPE, path )
        THKEY_END()
    #endmacro

    #include "app/modThemeKeys.bi"

    #undef THTEXTKEY
    #undef THSTYLEKEY
    #undef THPAIRKEY
    #undef THPAIRAKEY
    #undef THFOREKEY
    #undef THFOREAKEY
    #undef THBACKKEY
    #undef THKEY_BEGIN
    #undef THKEY_END

end sub


''
''  WRITE ONE CHANNEL THROUGH AN OFFSET
''
''  A negative offset means the key has no such channel and the write is skipped -- the
''  translation of the legacy table's null-pointer test.
''
private sub ThemeRoot_PokeColor( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long, _
                                 byval v as COLORREF )
    if ofs < 0 then exit sub
    *cast( COLORREF ptr, cast( any ptr, pRoot ) + ofs ) = v
end sub

private sub ThemeRoot_PokeBool( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long, _
                                byval v as boolean )
    if ofs < 0 then exit sub
    *cast( boolean ptr, cast( any ptr, pRoot ) + ofs ) = v
end sub

private sub ThemeRoot_PokeLong( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long, _
                                byval v as long )
    if ofs < 0 then exit sub
    *cast( long ptr, cast( any ptr, pRoot ) + ofs ) = v
end sub

' byval, and never "byref as const" -- AfxNova's DWSTRING copy constructor is not
' const-correct and a const reference that is then copied corrupts the heap. See Learnings.md.
private sub ThemeRoot_PokeText( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long, _
                                byval v as DWSTRING )
    if ofs < 0 then exit sub
    *cast( DWSTRING ptr, cast( any ptr, pRoot ) + ofs ) = v
end sub


''
''  THE ROLE PALETTE, AS THE THEME EDITOR SEES IT
''
''  Index-addressed so a list control can walk it without knowing the enum.
''
function Theme_RoleCount() as long
    Theme_BuildRoleTable()
    return ubound(gThemeRoleDef) - lbound(gThemeRoleDef) + 1
end function

function Theme_RoleName( byval idx as long ) as DWSTRING
    Theme_BuildRoleTable()
    if (idx < lbound(gThemeRoleDef)) orelse (idx > ubound(gThemeRoleDef)) then return ""
    return gThemeRoleDef(idx).name
end function

function Theme_RoleColor( byval idx as long ) as COLORREF
    Theme_BuildRoleTable()
    if (idx < lbound(gThemeRoleDef)) orelse (idx > ubound(gThemeRoleDef)) then return 0
    return theme.role.clr( gThemeRoleDef(idx).nRole )
end function

' Is this role NAMED IN THE FILE, or is it coming from the built-in default? The editor marks
' the difference so a user can see which of the twenty-one this theme actually chose.
function Theme_RoleIsExplicit( byval idx as long ) as boolean
    Theme_BuildRoleTable()
    if (idx < lbound(gThemeRoleDef)) orelse (idx > ubound(gThemeRoleDef)) then return false
    dim as long n = Theme_FindEntry( gThemeRoleDef(idx).name )
    if n < 0 then return false
    return gTheme(n).hasFore
end function


''
''  RE-RESOLVE EVERY KEY THAT INHERITS FROM A ROLE
''
''  MUST BE CALLED AFTER EDITING A ROLE, and this is the whole reason it exists: role
''  resolution only fills a channel whose has* flag is FALSE, so once a key has taken its
''  role's colour it is indistinguishable from an authored one and a later change to that
''  role would reach nothing at all. The edit would appear to do nothing, silently, which is
''  the exact shape of bug this branch has hit repeatedly.
''
''  So the role-derived values are UN-RESOLVED first -- and only those, which is what the
''  foreFromRole/backFromRole provenance is for. An authored colour is never disturbed.
''
sub Theme_ReapplyRoles()

    for i as long = lbound(gTheme) to ubound(gTheme)
        if gTheme(i).foreFromRole then
            gTheme(i).hasFore      = false
            gTheme(i).foreFromRole = false
        end if
        if gTheme(i).backFromRole then
            gTheme(i).hasBack      = false
            gTheme(i).backFromRole = false
        end if
    next

    Theme_ResolveRoles()
    Theme_ResolveKeyRoles()

end sub


''
''  SET ONE ROLE AND RE-RESOLVE. Returns the subsystems to repaint -- always everything,
''  because a role feeds keys across every namespace by design.
''
function Theme_SetRole( byval idx as long, byval clr as COLORREF ) as ulong

    Theme_BuildRoleTable()
    if (idx < lbound(gThemeRoleDef)) orelse (idx > ubound(gThemeRoleDef)) then return THSUB_NONE

    dim as long n = Theme_EnsureEntry( gThemeRoleDef(idx).name )
    gTheme(n).foreground   = clr
    gTheme(n).hasFore      = true
    gTheme(n).foreFromRole = false      ' a role is authored, never inherited from itself

    Theme_ReapplyRoles()
    return THSUB_ALL

end function


''
''  READ ONE CHANNEL BACK THROUGH AN OFFSET
''
''  Public because the self-test's whole job in this phase is to compare what the generated
''  offsets address against what the hand-written pointer table addresses. There is no other
''  way to reach a member the caller knows only by offset.
''
function ThemeRoot_PeekColor( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long ) as COLORREF
    if pRoot = 0 orelse ofs < 0 then return 0
    return *cast( COLORREF ptr, cast( any ptr, pRoot ) + ofs )
end function

function ThemeRoot_PeekBool( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long ) as boolean
    if pRoot = 0 orelse ofs < 0 then return false
    return *cast( boolean ptr, cast( any ptr, pRoot ) + ofs )
end function

function ThemeRoot_PeekLong( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long ) as long
    if pRoot = 0 orelse ofs < 0 then return 0
    return *cast( long ptr, cast( any ptr, pRoot ) + ofs )
end function

function ThemeRoot_PeekText( byval pRoot as THEMEROOT_TYPE ptr, byval ofs as long ) as DWSTRING
    if pRoot = 0 orelse ofs < 0 then return ""
    return *cast( DWSTRING ptr, cast( any ptr, pRoot ) + ofs )
end function


''
''  FIND A KEY'S ROW IN THE GENERATED TABLE, or -1.
''
function Theme_KeyIndex( byval lookupKey as string ) as long
    Theme_BuildKeyTable()
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        if gThemeKeys(i).keyValue = lookupKey then return i
    next
    return -1
end function


''
''  HOW MANY ROWS DID THE TABLE ACTUALLY GENERATE?
''
''  Exists so the self-test can catch the "#include once" trap, which is otherwise completely
''  silent: the build stays clean and the table simply comes out empty.
''
function Theme_KeyCount() as long
    Theme_BuildKeyTable()
    return ubound(gThemeKeys) - lbound(gThemeKeys) + 1
end function


''
''  POUR THE PARSED THEME INTO A ROOT STRUCTURE
''
''  Takes the destination rather than assuming the live one, which is the point of holding
''  offsets: the same call fills the snapshot, the baseline or the defaults.
''
sub Theme_FillRoot( byval pRoot as THEMEROOT_TYPE ptr )

    if pRoot = 0 then exit sub
    Theme_BuildKeyTable()

    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        dim as THEMEKEY_TYPE ptr pk = @gThemeKeys(i)
        dim as THEME_TYPE ptr pTheme = getThemePtr( pk->keyValue )

        ThemeRoot_PokeText ( pRoot, pk->ofsText,   pTheme->keyData )
        ThemeRoot_PokeColor( pRoot, pk->ofsFore,   pTheme->foreground )
        ThemeRoot_PokeColor( pRoot, pk->ofsBack,   pTheme->background )
        ThemeRoot_PokeBool ( pRoot, pk->ofsBold,   (pTheme->bold <> 0) )
        ThemeRoot_PokeBool ( pRoot, pk->ofsItalic, (pTheme->italic <> 0) )
        ThemeRoot_PokeBool ( pRoot, pk->ofsUnder,  (pTheme->underline <> 0) )

        ' Alpha only lands when the theme actually asked for one. Without the has* test a
        ' plain "r,g,b" would drive the destination to the 255 default and silently take
        ' over from whatever constant the consumer uses today. Same rule as Theme_FillGlobals.
        if pk->ofsForeAlpha >= 0 then
            ThemeRoot_PokeLong( pRoot, pk->ofsForeAlpha, _
                iif( pTheme->hasForeAlpha, clng(pTheme->foregroundAlpha), -1 ) )
        end if
        if pk->ofsBackAlpha >= 0 then
            ThemeRoot_PokeLong( pRoot, pk->ofsBackAlpha, _
                iif( pTheme->hasBackAlpha, clng(pTheme->backgroundAlpha), -1 ) )
        end if
    next

end sub


''
''  MATERIALISE ANY KEY THE THEME DID NOT SUPPLY, FROM ITS ROLE
''
''  Runs after the legacy ui.* table below and fills only what is STILL absent, so the order
''  is explicit-value -> ui.* inheritance -> role -> built-in default. That is a defined
''  precedence rather than a race: each step writes only where has* is false.
''
''  Keeping the ui.* table underneath the roles rather than deleting it is deliberate and is
''  a departure from the original plan. The shipped themes define no ui.* keys, so ui.divider
''  and ui.accent are materialised today by inheritance from editor.divider and
''  toptabs.iconselected. Replacing that with role.border / role.accent would change both --
''  33 consumers between them -- and this step is supposed to move no pixels at all.
''
sub Theme_ResolveKeyRoles()

    Theme_BuildKeyTable()

    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)

        ' Text payloads have no colour to inherit.
        if gThemeKeys(i).ofsText >= 0 then continue for

        dim as long rF = gThemeKeys(i).nRoleFore
        dim as long rB = gThemeKeys(i).nRoleBack
        dim as boolean bNoRoles = ((rF = THROLE_NONE) andalso (rB = THROLE_NONE))
        if bNoRoles then continue for

        dim as long n = Theme_EnsureEntry( gThemeKeys(i).keyValue )

        ' Only a channel the key actually HAS may be filled -- otherwise a role would
        ' invent a background for a hairline and the writer would start emitting it.
        dim as boolean bWantF = (rF <> THROLE_NONE)
        dim as boolean bHasF   = (gThemeKeys(i).ofsFore >= 0)
        dim as boolean bFreeF  = (gTheme(n).hasFore = false)
        if bWantF andalso bHasF andalso bFreeF then
            gTheme(n).foreground   = theme.role.clr(rF)
            gTheme(n).hasFore      = true
            gTheme(n).foreFromRole = true
        end if
        dim as boolean bWantB = (rB <> THROLE_NONE)
        dim as boolean bHasB   = (gThemeKeys(i).ofsBack >= 0)
        dim as boolean bFreeB  = (gTheme(n).hasBack = false)
        if bWantB andalso bHasB andalso bFreeB then
            gTheme(n).background   = theme.role.clr(rB)
            gTheme(n).hasBack      = true
            gTheme(n).backFromRole = true
        end if

    next

end sub


''
''  HAND A CHANNEL BACK TO ITS ROLE                        (the other half of "override")
''
''  Without this a palette can only DECAY: every edit detaches a key permanently, so a theme
''  drifts one key at a time into literals and the palette stops reaching anything. CLAUDE.md
''  states that outright as the reason PsColorPicker's Assign button mattered -- and this is
''  the same capability, moved out of the picker (which only knows about colours) and onto the
''  page (which knows about roles).
''
''  Clears the channel and lets the resolver refill it, so the key ends up holding its role's
''  current colour AND marked as inherited -- which is what makes it follow later palette
''  edits and stops the writer emitting it.
''
function Theme_InheritChannel( byval lookupKey as string, byval nChannel as long ) as ulong

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then return THSUB_NONE

    dim as long k = Theme_KeyIndex( lookupKey )
    if k < 0 then return THSUB_NONE

    ' A channel with no declared role has nothing to inherit FROM; refusing is better than
    ' clearing it and leaving the key black.
    if nChannel = THEMECH_FORE then
        if gThemeKeys(k).nRoleFore = THROLE_NONE then return THSUB_NONE
        gTheme(n).hasFore      = false
        gTheme(n).foreFromRole = false
        gTheme(n).foreMacro    = ""
    else
        if gThemeKeys(k).nRoleBack = THROLE_NONE then return THSUB_NONE
        gTheme(n).hasBack      = false
        gTheme(n).backFromRole = false
        gTheme(n).backMacro    = ""
    end if

    Theme_ResolveKeyRoles()
    ' Read the mask straight off the row rather than calling Theme_SubsystemsForKey, which is
    ' defined further down this file -- fbc is single-pass and k is already the row index.
    return gThemeKeys(k).nSubsys

end function


''
''  IS THIS CHANNEL CURRENTLY OVERRIDDEN, i.e. would "inherit" change anything?
''
''  The UI asks this to enable its control. Two conditions, and both matter: the key must HAVE
''  a role to go back to, and it must not already be following it.
''
function Theme_ChannelIsOverridden( byval lookupKey as string, byval nChannel as long ) as boolean

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then return false
    dim as long k = Theme_KeyIndex( lookupKey )
    if k < 0 then return false

    if nChannel = THEMECH_FORE then
        if gThemeKeys(k).nRoleFore = THROLE_NONE then return false
        if gThemeKeys(k).ofsFore < 0 then return false
        return (gTheme(n).foreFromRole = false)
    end if

    if gThemeKeys(k).nRoleBack = THROLE_NONE then return false
    if gThemeKeys(k).ofsBack < 0 then return false
    return (gTheme(n).backFromRole = false)

end function



''
''  WHICH SUBSYSTEMS DOES THIS KEY FEED?
''
function Theme_SubsystemsForKey( byval lookupKey as string ) as ulong
    Theme_BuildKeyTable()
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        if gThemeKeys(i).keyValue = lookupKey then return gThemeKeys(i).nSubsys
    next
    return THSUB_NONE
end function


''
''  CAN AN ALPHA ON THIS KEY ACTUALLY REACH A PIXEL?
''
''  The colour-picker UI asks this to decide whether to offer an alpha slider at all. False
''  for every Scintilla style and every GDI-drawn caption, which are opaque by construction.
''
function Theme_IsAlphaCapable( byval lookupKey as string ) as boolean
    Theme_BuildKeyTable()
    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)
        if gThemeKeys(i).keyValue = lookupKey then return gThemeKeys(i).bAlphaCapable
    next
    return false
end function


''
''  SET ONE COLOUR CHANNEL IN MEMORY
''
''  Returns the subsystem mask the key feeds, so the caller can hand it straight to
''  Theme_RequestApply. Deliberately does NOT fill the globals and does NOT repaint -- a drag
''  produces hundreds of these and the caller decides when the work happens. Pass nAlpha = -1
''  to leave the existing alpha alone.
''
function Theme_SetColor( byval lookupKey as string, _
                         byval nChannel as long, _
                         byval clr as COLORREF, _
                         byval nAlpha as long = -1 ) as ulong

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then n = Theme_EnsureEntry( lookupKey )

    select case nChannel
        case THEMECH_FORE
            gTheme(n).foreground = clr
            gTheme(n).hasFore = true
            ' DETACH. This key stops referring to the macro; the macro itself and every
            ' other key using it are untouched. Re-attaching is a separate explicit action,
            ' never a side effect of picking a colour.
            gTheme(n).foreMacro    = ""
        gTheme(n).foreFromRole = false
            if nAlpha >= 0 then
                gTheme(n).foregroundAlpha = cubyte(iif(nAlpha > 255, 255, nAlpha))
                gTheme(n).hasForeAlpha = true
            end if
        case THEMECH_BACK
            gTheme(n).background = clr
            gTheme(n).hasBack = true
            gTheme(n).backMacro    = ""
        gTheme(n).backFromRole = false                ' detach -- see THEMECH_FORE above
            if nAlpha >= 0 then
                gTheme(n).backgroundAlpha = cubyte(iif(nAlpha > 255, 255, nAlpha))
                gTheme(n).hasBackAlpha = true
            end if
        case else
            return THSUB_NONE
    end select

    return Theme_SubsystemsForKey( lookupKey )

end function


''
''  SET ONE BOLD / ITALIC / UNDERLINE FLAG IN MEMORY
''
function Theme_SetFlag( byval lookupKey as string, _
                        byval nChannel as long, _
                        byval nValue as long ) as ulong

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then n = Theme_EnsureEntry( lookupKey )

    select case nChannel
        case THEMECH_BOLD:   gTheme(n).bold = nValue
        case THEMECH_ITALIC: gTheme(n).italic = nValue
        case THEMECH_UNDER:  gTheme(n).underline = nValue
        case else:           return THSUB_NONE
    end select
    gTheme(n).hasFlags = true

    return Theme_SubsystemsForKey( lookupKey )

end function


''
''  IS THE CURRENT THEME A DARK ONE?
''
''  Reads gThemeCurrentShortFilename, NOT gConfig.ThemeShortFilename, and that distinction is
''  the whole reason this exists. During a live preview the Options dialog has staged the
''  user's pick in gOptWork and gConfig still names the OLD theme -- so the three call sites
''  that used to test gConfig would leave the title bar on the previous theme for the entire
''  preview and then snap to the right colour on OK, which reads as a bug.
''
''  Still a filename test. A proper general.appearance key is Phase 2's job.
''
function Theme_IsDark() as boolean
    ' Prefer an explicit declaration by the theme itself.
    dim as long n = Theme_FindEntry( "general.appearance" )
    if n >= 0 then
        dim as string s = lcase(PsTrim(gTheme(n).keyData))
        if s = "light" then return false
        if s = "dark" then return true
    end if
    ' Fall back to the historical filename sniff, which is all the shipped themes carry.
    return (instr(gThemeCurrentShortFilename, "_dark") > 0)
end function


''
''  APPLY THE WINDOWS DARK CAPTION TO A TOP-LEVEL WINDOW
''
sub Theme_ApplyDarkCaption( byval hwnd as HWND )
    if IsWindow(hwnd) = 0 then exit sub
    dim as long nvalue = iif( Theme_IsDark(), 1, 0 )
    DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, @nvalue, sizeof(nvalue) )
end sub


''
''  SNAPSHOT / RESTORE                                   (Cancel in the Options dialog)
''
''  Only gTheme() is captured, never the nine typed globals. Those are a pure function of the
''  array via the bind table, so restoring them separately would create a second path that can
''  drift from Theme_FillGlobals -- this way "restore" and "apply" are literally the same code.
''
''  The copy is ELEMENT-WISE and must stay that way. THEME_TYPE holds two DWSTRINGs, and
''  element-wise assignment invokes DWSTRING's operator let, which deep-copies the buffer. A
''  memcpy would alias both buffers and the second destructor would double-free. CLAUDE.md
''  already records heap corruption from DWSTRING copy semantics, and notes the symptom
''  surfaces on a later unrelated allocation -- it would not point back here.
''
' Defined below -- Theme_Restore refills the globals through the same path a load does.
declare sub Theme_FillGlobals()

dim shared gThemeSnapEntries(any) as THEME_TYPE
' The palette is snapshotted too: previewing another theme REPLACES it, so without this a
' Cancel after a theme switch would restore the keys and leave the wrong macros beside them.
dim shared gThemeSnapMacros(any) as THEMEMACRO_TYPE
dim shared gThemeSnapFilename as DWSTRING
dim shared gThemeSnapValid as boolean

sub Theme_Snapshot()

    erase gThemeSnapEntries

    if ubound(gTheme) >= lbound(gTheme) then
        redim gThemeSnapEntries( lbound(gTheme) to ubound(gTheme) ) as THEME_TYPE
        for i as long = lbound(gTheme) to ubound(gTheme)
            gThemeSnapEntries(i) = gTheme(i)
        next
    end if

    erase gThemeSnapMacros
    if ubound(gThemeMacros) >= lbound(gThemeMacros) then
        redim gThemeSnapMacros( lbound(gThemeMacros) to ubound(gThemeMacros) ) as THEMEMACRO_TYPE
        for i as long = lbound(gThemeMacros) to ubound(gThemeMacros)
            gThemeSnapMacros(i) = gThemeMacros(i)
        next
    end if

    gThemeSnapFilename = gThemeCurrentShortFilename
    gThemeSnapValid = true

end sub


''
''  PUT THE SNAPSHOT BACK
''
''  Data half only: it refills the globals but does NOT repaint, because the apply layer lives
''  in modThemeApply.inc which is included long after this file. The caller pairs this with
''  Theme_ApplyAll(). No-op when nothing was snapshotted.
''
sub Theme_Restore()

    if gThemeSnapValid = false then exit sub

    erase gTheme

    if ubound(gThemeSnapEntries) >= lbound(gThemeSnapEntries) then
        redim gTheme( lbound(gThemeSnapEntries) to ubound(gThemeSnapEntries) ) as THEME_TYPE
        for i as long = lbound(gThemeSnapEntries) to ubound(gThemeSnapEntries)
            gTheme(i) = gThemeSnapEntries(i)
        next
    end if

    erase gThemeMacros
    if ubound(gThemeSnapMacros) >= lbound(gThemeSnapMacros) then
        redim gThemeMacros( lbound(gThemeSnapMacros) to ubound(gThemeSnapMacros) ) as THEMEMACRO_TYPE
        for i as long = lbound(gThemeSnapMacros) to ubound(gThemeSnapMacros)
            gThemeMacros(i) = gThemeSnapMacros(i)
        next
    end if

    gThemeCurrentShortFilename = gThemeSnapFilename

    Theme_ResolveFallbacks()
    Theme_FillGlobals()

end sub


''
''  DROP THE SNAPSHOT                                                  (OK was pressed)
''
sub Theme_DiscardSnapshot()
    erase gThemeSnapEntries
    erase gThemeSnapMacros
    gThemeSnapValid = false
end sub


''
''  BASELINE -- "the value this key had when this theme was loaded"
''
''  Distinct from the snapshot above, and the difference matters. The SNAPSHOT is the state
''  when the Options dialog opened, and exists so Cancel can undo everything. The BASELINE is
''  the state when the current theme was loaded, and exists so a per-key Reset button can undo
''  one experiment without touching the rest.
''
''  They diverge the moment the user switches theme inside the dialog: the snapshot still
''  holds the theme they arrived with, while the baseline follows the one they are now
''  editing. Resetting a key against the snapshot after a theme switch would hand back a
''  colour from a completely different theme, which is why this is a second array and not a
''  reuse of the first.
''
dim shared gThemeBaseEntries(any) as THEME_TYPE

sub Theme_CaptureBaseline()
    erase gThemeBaseEntries
    if ubound(gTheme) < lbound(gTheme) then exit sub
    redim gThemeBaseEntries( lbound(gTheme) to ubound(gTheme) ) as THEME_TYPE
    ' Element-wise, for the DWSTRING deep-copy reason spelled out above Theme_Snapshot.
    for i as long = lbound(gTheme) to ubound(gTheme)
        gThemeBaseEntries(i) = gTheme(i)
    next
end sub


''
''  IS THIS CHANNEL DIFFERENT FROM WHAT THE THEME LOADED WITH?
''
''  Lets the UI grey out a Reset that would do nothing.
''
function Theme_IsChannelModified( byval lookupKey as string, byval nChannel as long ) as boolean

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then return false

    for i as long = lbound(gThemeBaseEntries) to ubound(gThemeBaseEntries)
        if gThemeBaseEntries(i).keyValue <> lookupKey then continue for
        if nChannel = THEMECH_FORE then
            if gTheme(n).foreground <> gThemeBaseEntries(i).foreground then return true
            if gTheme(n).foregroundAlpha <> gThemeBaseEntries(i).foregroundAlpha then return true
        else
            if gTheme(n).background <> gThemeBaseEntries(i).background then return true
            if gTheme(n).backgroundAlpha <> gThemeBaseEntries(i).backgroundAlpha then return true
        end if
        return false
    next

    return false

end function


''
''  PUT ONE CHANNEL BACK TO ITS BASELINE
''
''  Returns the subsystem mask to re-apply, or THSUB_NONE when there was nothing to undo.
''
function Theme_ResetChannel( byval lookupKey as string, byval nChannel as long ) as ulong

    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then return THSUB_NONE

    for i as long = lbound(gThemeBaseEntries) to ubound(gThemeBaseEntries)
        if gThemeBaseEntries(i).keyValue <> lookupKey then continue for
        ' THE PROVENANCE FLAG IS PART OF THE BASELINE. Restoring the colour alone left a key
        ' that had been INHERITED, then edited, then reset sitting at the right colour but
        ' marked authored -- so the writer would start emitting it as a literal and it would
        ' stop following its role. Reset means "as the file had it", and the file had it
        ' inherited.
        if nChannel = THEMECH_FORE then
            gTheme(n).foreground      = gThemeBaseEntries(i).foreground
            gTheme(n).foregroundAlpha = gThemeBaseEntries(i).foregroundAlpha
            gTheme(n).hasForeAlpha    = gThemeBaseEntries(i).hasForeAlpha
            gTheme(n).foreFromRole    = gThemeBaseEntries(i).foreFromRole
        else
            gTheme(n).background      = gThemeBaseEntries(i).background
            gTheme(n).backgroundAlpha = gThemeBaseEntries(i).backgroundAlpha
            gTheme(n).hasBackAlpha    = gThemeBaseEntries(i).hasBackAlpha
            gTheme(n).backFromRole    = gThemeBaseEntries(i).backFromRole
        end if
        return Theme_SubsystemsForKey( lookupKey )
    next

    return THSUB_NONE

end function


''
''  COPY gTheme() INTO THE NINE TYPED GLOBALS
''
''  Pure and cheap: no file I/O, no window calls. Everything downstream reads the gh*
''  globals rather than gTheme(), so this is the one place the array becomes visible to the
''  rest of the app -- and re-running it is how a live edit reaches the screen.
''
sub Theme_FillGlobals()

    ' A thin wrapper over Theme_FillRoot now. The name is kept because three call sites use
    ' it and it still says what it does: make the parsed theme visible to the app.
    '
    ' What it used to do -- walk the hand-written bind table writing ten gh* globals, plus
    ' hand-copy the two DWSTRING payloads the table could not hold -- is all gone. The key
    ' table carries the payloads through its ofsText channel and Theme_FillRoot does the rest.
    Theme_FillRoot( @theme )

end sub


''
''  FORMAT ONE COLOUR AS THE FILE SEES IT
''
''  The exact inverse of the parse at the top of this file: COLORREF is 0x00BBGGRR, so the
''  low byte is RED. Written with explicit shifts rather than GetRValue/GetGValue/GetBValue
''  because a swapped red and blue here is the definition of wrong-but-plausible -- it
''  round-trips perfectly for every grey, which is most of a theme file.
''
function Theme_FormatColor( byval clr as COLORREF, _
                            byval nAlpha as long = -1 ) as string
    dim as long r = (clr       ) and &hFF
    dim as long g = (clr shr  8) and &hFF
    dim as long b = (clr shr 16) and &hFF
    dim as string s = str(r) & "," & str(g) & "," & str(b)
    if nAlpha >= 0 then s &= "," & str(nAlpha)
    return s
end function


''
''  TURN A THEME FILE'S COLOUR TEXT BACK INTO A COLORREF.  "r,g,b" or "r,g,b,a".
''
''  The inverse of Theme_FormatColor, and deliberately a SEPARATE function rather than a
''  refactor of the parser's inline version: the parser is doing four other things on the same
''  values and pulling that apart would be a rewrite of the hot path to serve one caller.
''  Written to the same rules -- the component COUNT is what distinguishes "opaque" from an
''  explicit alpha, because val("") is 0, i.e. fully TRANSPARENT, the exact opposite of what
''  "r,g,b" means.
''
''  nAlpha comes back as -1 for "no alpha was supplied", which is the sentinel Theme_SetColor
''  already takes.
''
private function Theme_ColorFromText( byval wszText as DWSTRING, _
                                      byref clr as COLORREF, _
                                      byref nAlpha as long _
                                      ) as boolean

    clr = 0
    nAlpha = -1

    dim as string s = PsTrim( wszText ).Utf8
    if len(s) = 0 then return false
    if PsStrParseCount( s, "," ) < 3 then return false

    dim as long r = PsVal( PsStrParse( s, 1, "," ) )
    dim as long g = PsVal( PsStrParse( s, 2, "," ) )
    dim as long b = PsVal( PsStrParse( s, 3, "," ) )
    if r < 0 then r = 0
    if r > 255 then r = 255
    if g < 0 then g = 0
    if g > 255 then g = 255
    if b < 0 then b = 0
    if b > 255 then b = 255
    clr = BGR( r, g, b )

    if PsStrParseCount( s, "," ) >= 4 then
        nAlpha = PsVal( PsStrParse( s, 4, "," ) )
        if nAlpha < 0 then nAlpha = 0
        if nAlpha > 255 then nAlpha = 255
    end if

    return true

end function


''
''  ATTACH ONE CHANNEL TO A NAMED %MACRO.  The deliberate counterpart to detach-on-edit.
''
''  Theme_SetColor CLEARS a channel's macro name, so every edit erodes the palette and nothing
''  rebuilds it -- a theme would drift toward all-literals and the macro list would become a
''  museum. This is what makes the palette something a user can restore rather than only spend.
''
''  It routes the VALUE through Theme_SetColor and then re-attaches the name, rather than writing
''  both itself. That is not a detour: Theme_SetColor is the one place a channel's value, its
''  has* flags and its subsystem mask are decided, and a second writer would be a second set of
''  rules to keep in step. The detach it performs on the way through is simply undone here.
''
''  A macro carries NO alpha -- the format has one value per macro and "%NAME,200" is not
''  something the parser accepts -- so a macro whose own text happens to have four components
''  still assigns its alpha to the key, and the WRITER will then emit that key as a literal
''  rather than as %NAME. That is the honest outcome and it is documented on Theme_ChannelText.
''
''  Returns the subsystem mask, or THSUB_NONE when the macro or the key is unknown. Fills no
''  globals and repaints nothing -- the caller decides when, exactly as Theme_SetColor does.
''
function Theme_AssignMacro( byval lookupKey as string, _
                            byval nChannel as long, _
                            byval wszMacro as DWSTRING _
                            ) as ulong

    dim as long nm = -1
    for i as long = lbound(gThemeMacros) to ubound(gThemeMacros)
        if gThemeMacros(i).name = wszMacro then nm = i : exit for
    next
    if nm < 0 then return THSUB_NONE

    dim as COLORREF clr
    dim as long nAlpha
    if Theme_ColorFromText( gThemeMacros(nm).value, clr, nAlpha ) = false then return THSUB_NONE

    dim as ulong nMask = Theme_SetColor( lookupKey, nChannel, clr, nAlpha )
    if nMask = THSUB_NONE then return THSUB_NONE

    ' Re-attach. Theme_SetColor cleared the name on the way past; this is the whole point of the
    ' function.
    dim as long n = Theme_FindEntry( lookupKey )
    if n >= 0 then
        if nChannel = THEMECH_FORE then
            gTheme(n).foreMacro = wszMacro
        elseif nChannel = THEMECH_BACK then
            gTheme(n).backMacro = wszMacro
        end if
    end if

    return nMask

end function


''
''  How many named colours the current theme defines, and what one of them is. Thin accessors so
''  a UI does not have to reach into gThemeMacros and re-implement the text parse.
''
function Theme_MacroCount() as long
    if ubound(gThemeMacros) < lbound(gThemeMacros) then return 0
    return ubound(gThemeMacros) - lbound(gThemeMacros) + 1
end function

function Theme_MacroAt( byval idx as long, _
                        byref wszName as DWSTRING, _
                        byref clr as COLORREF _
                        ) as boolean

    wszName = ""
    clr = 0
    if (idx < 0) orelse (idx > (ubound(gThemeMacros) - lbound(gThemeMacros))) then return false
    dim as long i = lbound(gThemeMacros) + idx
    dim as long nAlpha
    if Theme_ColorFromText( gThemeMacros(i).value, clr, nAlpha ) = false then return false
    wszName = gThemeMacros(i).name
    return true
end function


''
''  Which %MACRO is this channel currently attached to, or "" for a literal. Read by the UI so it
''  can show the user what a key refers to before they change it.
''
function Theme_ChannelMacro( byval lookupKey as string, byval nChannel as long ) as DWSTRING
    dim as DWSTRING wszOut
    dim as long n = Theme_FindEntry( lookupKey )
    if n < 0 then return wszOut
    if nChannel = THEMECH_FORE then
        wszOut = gTheme(n).foreMacro
    elseif nChannel = THEMECH_BACK then
        wszOut = gTheme(n).backMacro
    end if
    return wszOut
end function


''
''  HOW ONE CHANNEL SHOULD BE WRITTEN
''
''  "%NAME" while the key is still attached to a macro, otherwise literal r,g,b[,a]. The
''  attachment is dropped by Theme_SetColor the moment the channel is edited, so a key the
''  user has retuned writes as a literal while its former stablemates keep the macro.
''
''  A macro reference deliberately carries NO alpha: the format has one value per macro, and
''  "%NAME,200" is not something the parser accepts. A channel that gains an explicit alpha
''  is therefore written out in full.
''
private function Theme_ChannelText( byval wszMacro as DWSTRING, _
                                    byval clr as COLORREF, _
                                    byval nAlpha as long ) as DWSTRING
    ' wszMacro is ignored. It used to be returned as "%NAME" so an attached key round
    ' tripped; with %MACRO retired there is nothing to round trip back into, and every
    ' channel is written as a literal. The parameter is kept so the call sites -- which
    ' still carry the parsed macro name -- do not all have to change in the same commit.
    return Theme_FormatColor( clr, nAlpha )
end function


''
''  STRIP WHAT THE PARSER CANNOT READ BACK
''
''  A ':' would truncate the value (key/data split on the FIRST colon) and a '#' would start
''  a comment. Only ever matters for general.description, which is free text.
''
private function Theme_SanitizeText( byval sText as string ) as string
    dim as string s = sText
    for i as long = 0 to len(s) - 1
        if (s[i] = asc(":")) orelse (s[i] = asc("#")) then s[i] = asc(" ")
    next
    return trim(s)
end function


''
''  WRITE gTheme() OUT AS A .theme FILE
''
''  Canonical form, ordered by the bind table, so two saves of the same theme produce
''  byte-identical files and a diff between two themes lines up. Literal r,g,b -- macros are
''  an authoring convenience the parser expands away, and there is nothing left to expand
''  back into by the time we get here.
''
''  ON NOT DUAL-EMITTING THE LEGACY KEYS
''    The design sketch had this write each ui.* key AND its editor.* ancestor with the same
''    value, so an older tiko would still render a theme this version wrote. That is wrong on
''    reflection: editor.divider now means "the divider inside the editor" and ui.divider means
''    "the app-wide hairline", and they are separately editable. Emitting both would put two
''    lines with the same key and different values in one file, which the parser resolves by
''    last-one-wins -- fragile, and it would silently overwrite the editor's own value.
''    Instead every key is written exactly once with its own value. An older tiko ignores the
''    ui.* lines entirely and falls back to editor.*, which is precisely the behaviour it had
''    before the split existed.
''
''  Keys with no bind row (the six dead ones -- panel.panel, editor.window and friends) are
''  NOT written. They had no destination in the original code either.
''
function Theme_WriteFile( byval wszFullPath as DWSTRING, _
                          byval wszDescription as DWSTRING ) as boolean

    Theme_BuildKeyTable()

    dim as long f = freefile
    if open( wszFullPath for output as #f ) <> 0 then return true

    dim as string sLine

    sLine = "# tiko theme file. Written by tiko -- edits are safe, but the layout is"
    print #f, sLine
    sLine = "# regenerated whenever the theme is saved from the Options dialog."
    print #f, sLine
    sLine = "# Colors are red,green,blue (0-255), optionally red,green,blue,alpha."
    print #f, sLine
    print #f, ""

    ' THE ROLE PALETTE, and it MUST be written or the theme loses it.
    '
    ' The writer walks the KEY table, and roles are not in it -- so without this block a save
    ' emitted no role.* lines at all. For a theme whose keys are all explicit that is
    ' invisible (default_dark forked and rendered identically); for a ROLES-ONLY theme it is
    ' catastrophic, because every key is fromRole and therefore skipped below, so the saved
    ' file would come back very nearly empty. Found by forking a theme and reading what came
    ' out, not by any assertion -- assertion 23b checked that a roles-only save stayed SHORT
    ' and was perfectly happy with short-because-empty.
    '
    ' Only EXPLICIT roles are written. One left at its built-in default is not part of this
    ' theme's palette and writing it would freeze today's default into the file forever.
    scope
        dim as long nWritten
        for i as long = 0 to Theme_RoleCount() - 1
            if Theme_RoleIsExplicit(i) = false then continue for
            if nWritten = 0 then
                sLine = "# The palette. Every key that does not name a colour of its own"
                print #f, sLine
                sLine = "# inherits from one of these."
                print #f, sLine
            end if
            sLine = DWSTRING( Theme_RoleName(i) & ": " & Theme_FormatColor( Theme_RoleColor(i), -1 ) ).Utf8
            print #f, sLine
            nWritten += 1
        next
        if nWritten > 0 then print #f, ""
    end scope

    ' NO %MACRO BLOCK. The writer used to echo the palette back here so a hand-authored
    ' theme kept its named colours; the ROLE layer replaced that job outright and does it
    ' better, because a macro was pure text substitution carrying no meaning -- nothing
    ' could inherit from one, so every key still had to appear in every file. A role says
    ' WHICH colour this is, and every key falls back to one automatically.
    '
    ' The parser still ACCEPTS %MACRO, so an existing hand-authored theme keeps loading and
    ' the first save through this function migrates it to literals. That is deliberate: a
    ' one-way migration costs a branch in the parser and breaks nobody's file.

    sLine = "general.description: " & Theme_SanitizeText( wszDescription )
    print #f, sLine
    dim as long nVer = Theme_FindEntry( "general.version" )
    dim as string sVer = "1.0.0"
    if nVer >= 0 then
        if PsLen(gTheme(nVer).keyData) > 0 then sVer = Theme_SanitizeText( gTheme(nVer).keyData )
    end if
    sLine = "general.version: " & sVer
    print #f, sLine

    ' Declared explicitly rather than left to the "_dark" filename sniff, which cannot survive
    ' the user renaming a theme and gets a fork of default_dark wrong the moment they give it
    ' a light background. Derived from the editor background's luminance so it self-corrects
    ' as the theme is edited, rather than freezing whatever the sniff happened to say.
    dim as long nBg = Theme_FindEntry( "editor.text" )
    dim as string sAppearance = "dark"
    if nBg >= 0 then
        dim as COLORREF c = gTheme(nBg).background
        dim as long lum = ( 299 * ((c) and &hFF) _
                          + 587 * ((c shr  8) and &hFF) _
                          + 114 * ((c shr 16) and &hFF) ) \ 1000
        if lum >= 128 then sAppearance = "light"
    end if
    sLine = "general.appearance: " & sAppearance
    print #f, sLine
    print #f, ""

    dim as string sLastNamespace

    ' Driven by the GENERATED KEY TABLE, not the bind table.
    '
    ' This was the bind table until the migration emptied it, and the consequence was data
    ' loss rather than a cosmetic gap: the writer emits exactly the keys it is asked to walk,
    ' so every namespace whose bind rows had gone silently stopped being written to the file
    ' at all. Saving a theme would have dropped panel.*, output.*, toptabs.* and the rest --
    ' with a clean build and a working editor. Caught by the write/read round-trip assertion.
    Theme_BuildKeyTable()

    for i as long = lbound(gThemeKeys) to ubound(gThemeKeys)

        dim as string sKey = gThemeKeys(i).keyValue
        dim as long n = Theme_FindEntry( sKey )
        if n < 0 then continue for
        ' general.* carries its payload in keyData and is emitted by hand above.
        if gThemeKeys(i).ofsText >= 0 then continue for

        ' Blank line between namespaces, purely so the file stays readable by hand.
        dim as string sNamespace = PsStrParse( sKey, 1, "." ).Utf8
        if (len(sLastNamespace) > 0) andalso (sNamespace <> sLastNamespace) then print #f, ""
        sLastNamespace = sNamespace

        dim as boolean bWriteF = (gThemeKeys(i).ofsFore >= 0)
        bWriteF = (bWriteF andalso gTheme(n).hasFore)
        bWriteF = (bWriteF andalso (gTheme(n).foreFromRole = false))
        if bWriteF then
            dim as long nA = -1
            if gTheme(n).hasForeAlpha then nA = clng(gTheme(n).foregroundAlpha)
            sLine = sKey & ".foreground: " & _
                    Theme_ChannelText( gTheme(n).foreMacro, gTheme(n).foreground, nA ).Utf8
            print #f, sLine
        end if

        dim as boolean bWriteB = (gThemeKeys(i).ofsBack >= 0)
        bWriteB = (bWriteB andalso gTheme(n).hasBack)
        bWriteB = (bWriteB andalso (gTheme(n).backFromRole = false))
        if bWriteB then
            dim as long nA = -1
            if gTheme(n).hasBackAlpha then nA = clng(gTheme(n).backgroundAlpha)
            sLine = sKey & ".background: " & _
                    Theme_ChannelText( gTheme(n).backMacro, gTheme(n).background, nA ).Utf8
            print #f, sLine
        end if

        ' Flags are written for every key that HAS them bound, present or not, so a syntax
        ' style always shows its full triplet rather than a partial one.
        if gThemeKeys(i).ofsBold >= 0 then
            sLine = sKey & ".bold: "      & iif( gTheme(n).bold      <> 0, "true", "false" )
            print #f, sLine
            sLine = sKey & ".italic: "    & iif( gTheme(n).italic    <> 0, "true", "false" )
            print #f, sLine
            sLine = sKey & ".underline: " & iif( gTheme(n).underline <> 0, "true", "false" )
            print #f, sLine
        end if

    next

    close #f

    return false

end function


''
''  DUMP EVERY THEME GLOBAL TO A TEXT FILE          (TIKO_THEME_TRACE=1)
''
''  Names every field explicitly rather than walking the bind table, and that is the whole
''  point: it is an INDEPENDENT oracle. The bind-table rewrite is verified by diffing this
''  file before and against after, so a dump generated FROM the table would only ever prove
''  the table agrees with itself.
''
''  Each line is "field=value"; colours are 6-digit hex COLORREFs, flags are 0/1. Every line
''  is built into a `string` before it is written -- printing a DWSTRING, or a
''  string & wstring concat, interleaves nulls.
''
sub Theme_DumpGlobals( byval wszOutPath as DWSTRING )

    dim as long f = freefile
    if open( wszOutPath for output as #f ) <> 0 then exit sub

    dim as string sLine

    #macro DUMPCLR( nm, v )
        sLine = nm & "=" & hex(v, 6)
        print #f, sLine
    #endmacro
    #macro DUMPFLG( nm, v )
        sLine = nm & "=" & str(iif(v, 1, 0))
        print #f, sLine
    #endmacro
    #macro DUMPTXT( nm, v )
        scope
            '' .Utf8 rather than the implicit wstring->string, which goes through the ANSI
            '' codepage. This is a diagnostic dump -- see docs/port/type-swap-scope.md.
            dim as DWSTRING wszV = v
            sLine = nm & "=" & wszV.Utf8
            print #f, sLine
        end scope
    #endmacro

    DUMPTXT( "general.description",        theme.general.description )
    DUMPTXT( "general.version",            theme.general.version )
    DUMPCLR( "general.iconfail",           theme.compile.iconfail )
    DUMPCLR( "general.iconsuccess",        theme.compile.iconsuccess )

    DUMPCLR( "main.panel",                 theme.main.panel )

    DUMPCLR( "editor.ForeColorCaret",         theme.editor.caret )
    DUMPCLR( "editor.BackColorCurrentLine",   theme.editor.currentline )
    DUMPCLR( "editor.ForeColorText",          theme.editor.text.forecolor )
    DUMPCLR( "editor.BackColorText",          theme.editor.text.backcolor )
    DUMPFLG( "editor.TextBold",               theme.editor.text.bold )
    DUMPFLG( "editor.TextItalic",             theme.editor.text.italic )
    DUMPFLG( "editor.TextUnderline",          theme.editor.text.underline )
    DUMPCLR( "editor.ForeColorNumbers",       theme.editor.numbers.forecolor )
    DUMPCLR( "editor.BackColorNumbers",       theme.editor.numbers.backcolor )
    DUMPFLG( "editor.NumbersBold",            theme.editor.numbers.bold )
    DUMPFLG( "editor.NumbersItalic",          theme.editor.numbers.italic )
    DUMPFLG( "editor.NumbersUnderLine",       theme.editor.numbers.underline )
    DUMPCLR( "editor.ForeColorComments",      theme.editor.comments.forecolor )
    DUMPCLR( "editor.BackColorComments",      theme.editor.comments.backcolor )
    DUMPFLG( "editor.CommentsBold",           theme.editor.comments.bold )
    DUMPFLG( "editor.CommentsItalic",         theme.editor.comments.italic )
    DUMPFLG( "editor.CommentsUnderline",      theme.editor.comments.underline )
    DUMPCLR( "editor.ForeColorKeyword",       theme.editor.keyword1.forecolor )
    DUMPCLR( "editor.BackColorKeyword",       theme.editor.keyword1.backcolor )
    DUMPFLG( "editor.KeywordBold",            theme.editor.keyword1.bold )
    DUMPFLG( "editor.KeywordItalic",          theme.editor.keyword1.italic )
    DUMPFLG( "editor.KeywordUnderline",       theme.editor.keyword1.underline )
    DUMPCLR( "editor.ForeColorKeyword2",      theme.editor.keyword2.forecolor )
    DUMPCLR( "editor.BackColorKeyword2",      theme.editor.keyword2.backcolor )
    DUMPFLG( "editor.KeywordBold2",           theme.editor.keyword2.bold )
    DUMPFLG( "editor.KeywordItalic2",         theme.editor.keyword2.italic )
    DUMPFLG( "editor.KeywordUnderline2",      theme.editor.keyword2.underline )
    DUMPCLR( "editor.ForeColorKeyword3",      theme.editor.keyword3.forecolor )
    DUMPCLR( "editor.BackColorKeyword3",      theme.editor.keyword3.backcolor )
    DUMPFLG( "editor.KeywordBold3",           theme.editor.keyword3.bold )
    DUMPFLG( "editor.KeywordItalic3",         theme.editor.keyword3.italic )
    DUMPFLG( "editor.KeywordUnderline3",      theme.editor.keyword3.underline )
    DUMPCLR( "editor.ForeColorFoldmargin",    theme.editor.foldmargin )
    DUMPCLR( "editor.ForeColorFoldsymbol",    theme.editor.foldsymbol.forecolor )
    DUMPCLR( "editor.BackColorFoldsymbol",    theme.editor.foldsymbol.backcolor )
    DUMPCLR( "editor.ForeColorLinenumbers",   theme.editor.linenumbers.forecolor )
    DUMPCLR( "editor.BackColorLinenumbers",   theme.editor.linenumbers.backcolor )
    DUMPFLG( "editor.LinenumbersBold",        theme.editor.linenumbers.bold )
    DUMPFLG( "editor.LinenumbersItalic",      theme.editor.linenumbers.italic )
    DUMPFLG( "editor.LinenumbersUnderline",   theme.editor.linenumbers.underline )
    DUMPCLR( "editor.ForeColorOperators",     theme.editor.operators.forecolor )
    DUMPCLR( "editor.BackColorOperators",     theme.editor.operators.backcolor )
    DUMPFLG( "editor.OperatorsBold",          theme.editor.operators.bold )
    DUMPFLG( "editor.OperatorsItalic",        theme.editor.operators.italic )
    DUMPFLG( "editor.OperatorsUnderline",     theme.editor.operators.underline )
    DUMPCLR( "editor.ForeColorIndentguides",  theme.editor.indentguides.forecolor )
    DUMPCLR( "editor.BackColorIndentguides",  theme.editor.indentguides.backcolor )
    DUMPCLR( "editor.ForeColorPreprocessor",  theme.editor.preprocessor.forecolor )
    DUMPCLR( "editor.BackColorPreprocessor",  theme.editor.preprocessor.backcolor )
    DUMPFLG( "editor.PreprocessorBold",       theme.editor.preprocessor.bold )
    DUMPFLG( "editor.PreprocessorItalic",     theme.editor.preprocessor.italic )
    DUMPFLG( "editor.PreprocessorUnderline",  theme.editor.preprocessor.underline )
    DUMPCLR( "editor.ForeColorSelection",     theme.editor.selection.forecolor )
    DUMPCLR( "editor.BackColorSelection",     theme.editor.selection.backcolor )
    DUMPCLR( "editor.ForeColorStrings",       theme.editor.strings.forecolor )
    DUMPCLR( "editor.BackColorStrings",       theme.editor.strings.backcolor )
    DUMPFLG( "editor.StringsBold",            theme.editor.strings.bold )
    DUMPFLG( "editor.StringsItalic",          theme.editor.strings.italic )
    DUMPFLG( "editor.StringsUnderline",       theme.editor.strings.underline )
    DUMPCLR( "editor.ForeColorBracegood",     theme.editor.bracegood )
    DUMPCLR( "editor.ForeColorBracebad",      theme.editor.bracebad )
    DUMPCLR( "editor.ForeColorOccurrence",    theme.editor.occurrence )
    DUMPCLR( "editor.BackColorScrollBar",     theme.editor.scrollbar.backcolor )
    DUMPCLR( "editor.ForeColorScrollBar",     theme.editor.scrollbar.forecolor )
    DUMPCLR( "editor.Divider",                theme.editor.divider )
    DUMPCLR( "editor.edge",                   theme.editor.edge )
    DUMPCLR( "editor.whitespace.fore",        theme.editor.whitespace.forecolor )
    DUMPCLR( "editor.whitespace.back",        theme.editor.whitespace.backcolor )
    DUMPCLR( "editor.additionalselection",    theme.editor.additionalselection )
    DUMPCLR( "editor.additionalcaret",        theme.editor.additionalcaret )

    DUMPCLR( "tooltip.text.fore",             theme.tooltip.text.forecolor )
    DUMPCLR( "tooltip.text.back",             theme.tooltip.text.backcolor )
    DUMPCLR( "tooltip.border",                theme.tooltip.border )
    DUMPCLR( "tooltip.title",                 theme.tooltip.title )
    DUMPCLR( "tooltip.glyph",                 theme.tooltip.glyph )
    DUMPCLR( "editor.ForeColorBookmark",      theme.editor.bookmark.forecolor )
    DUMPCLR( "editor.BackColorBookmark",      theme.editor.bookmark.backcolor )
    DUMPCLR( "editor.ForeColorBreakpoint",    theme.editor.breakpoint.forecolor )
    DUMPCLR( "editor.BackColorBreakpoint",    theme.editor.breakpoint.backcolor )
    DUMPCLR( "editor.ForeColorDebugLine",     theme.editor.debugline.forecolor )
    DUMPCLR( "editor.BackColorDebugLine",     theme.editor.debugline.backcolor )

    DUMPCLR( "panel.BackColorScrollBar",      theme.panel.scrollbar.backcolor )
    DUMPCLR( "panel.ForeColorScrollBar",      theme.panel.scrollbar.forecolor )
    DUMPCLR( "panel.BackColor",               theme.panel.text.backcolor )
    DUMPCLR( "panel.ForeColor",               theme.panel.text.forecolor )
    DUMPCLR( "panel.BackColorHot",            theme.panel.texthot.backcolor )
    DUMPCLR( "panel.ForeColorHot",            theme.panel.texthot.forecolor )
    DUMPCLR( "panel.BackColorButton",         theme.panel.button.backcolor )
    DUMPCLR( "panel.ForeColorButton",         theme.panel.button.forecolor )
    DUMPCLR( "panel.BackColorButtonHot",      theme.panel.buttonhot.backcolor )
    DUMPCLR( "panel.ForeColorButtonHot",      theme.panel.buttonhot.forecolor )
    DUMPCLR( "panel.BackColorFilenameIcon",   theme.panel.filenameicon.backcolor )
    DUMPCLR( "panel.ForeColorFilenameIcon",   theme.panel.filenameicon.forecolor )
    DUMPCLR( "panel.BackColorFunctionIcon",   theme.panel.functionicon.backcolor )
    DUMPCLR( "panel.ForeColorFunctionIcon",   theme.panel.functionicon.forecolor )
    DUMPCLR( "panel.BackColorTypeIcon",       theme.panel.typeicon.backcolor )
    DUMPCLR( "panel.ForeColorTypeIcon",       theme.panel.typeicon.forecolor )
    DUMPCLR( "panel.ForeColorTextSearch",     theme.panel.textsearch )
    DUMPCLR( "panel.ForeColorTextSearchHot",  theme.panel.textsearchhot )

    DUMPCLR( "menubar.BackColor",             theme.menubar.text.backcolor )
    DUMPCLR( "menubar.BackColorHot",          theme.menubar.texthot.backcolor )
    DUMPCLR( "menubar.ForeColor",             theme.menubar.text.forecolor )
    DUMPCLR( "menubar.ForeColorHot",          theme.menubar.texthot.forecolor )

    DUMPCLR( "popup.PanelColor",              theme.topmenu.panel )
    DUMPCLR( "popup.BackColor",               theme.topmenu.text.backcolor )
    DUMPCLR( "popup.BackColorHot",            theme.topmenu.texthot.backcolor )
    DUMPCLR( "popup.BackColorDisabled",       theme.topmenu.textdisabled.backcolor )
    DUMPCLR( "popup.ForeColor",               theme.topmenu.text.forecolor )
    DUMPCLR( "popup.ForeColorHot",            theme.topmenu.texthot.forecolor )
    DUMPCLR( "popup.ForeColorDisabled",       theme.topmenu.textdisabled.forecolor )

    DUMPCLR( "toptabs.BackColor",             theme.toptabs.text.backcolor )
    DUMPCLR( "toptabs.BackColorHot",          theme.toptabs.texthot.backcolor )
    DUMPCLR( "toptabs.ForeColor",             theme.toptabs.text.forecolor )
    DUMPCLR( "toptabs.ForeColorHot",          theme.toptabs.texthot.forecolor )
    DUMPCLR( "toptabs.Divider",               theme.toptabs.divider )
    DUMPCLR( "toptabs.CloseBackColorHot",     theme.toptabs.closehot )
    DUMPCLR( "toptabs.IconSelectedBackColor", theme.toptabs.iconselected )

    DUMPCLR( "statusbar.BackColor",           theme.statusbar.text.backcolor )
    DUMPCLR( "statusbar.BackColorHot",        theme.statusbar.texthot.backcolor )
    DUMPCLR( "statusbar.ForeColor",           theme.statusbar.text.forecolor )
    DUMPCLR( "statusbar.ForeColorHot",        theme.statusbar.texthot.forecolor )

    DUMPCLR( "output.BackColor",              theme.output.text.backcolor )
    DUMPCLR( "output.BackColorHot",           theme.output.texthot.backcolor )
    DUMPCLR( "output.ForeColor",              theme.output.text.forecolor )
    DUMPCLR( "output.ForeColorHot",           theme.output.texthot.forecolor )
    DUMPCLR( "output.Divider",                theme.output.divider )
    DUMPCLR( "output.CloseBackColorHot",      theme.output.closehot )
    DUMPCLR( "output.BackColorScrollBar",     theme.output.scrollbar.backcolor )
    DUMPCLR( "output.ForeColorScrollBar",     theme.output.scrollbar.forecolor )
    DUMPCLR( "output.ScrollBarDivider",       theme.output.scrollbardivider )

    DUMPCLR( "ui.Divider",                    theme.ui.divider )
    DUMPCLR( "ui.ForeColorField",             theme.ui.field.forecolor )
    DUMPCLR( "ui.BackColorField",             theme.ui.field.backcolor )
    DUMPCLR( "ui.ForeColorScrollBar",         theme.ui.scrollbar.forecolor )
    DUMPCLR( "ui.BackColorScrollBar",         theme.ui.scrollbar.backcolor )
    DUMPCLR( "ui.AccentColor",                theme.ui.accent )

    ' The two alpha sentinels. -1 means the theme asked for no alpha and the consumer keeps
    ' whatever constant it uses today.
    DUMPFLG( "editor.AlphaSelection_isSet",   (theme.editor.selectionalpha >= 0) )
    DUMPFLG( "editor.AlphaOccurrence_isSet",  (theme.editor.occurrencealpha >= 0) )

    ' Anything the parser accepted but could not act on.
    for i as long = lbound(gThemeParseWarnings) to ubound(gThemeParseWarnings)
        sLine = DWSTRING( "!warning=" & gThemeParseWarnings(i) ).Utf8
        print #f, sLine
    next

    close #f

end sub


''
''  LOAD THEME DISK FILE
''
''  Back-compat wrapper over the three halves above. Same name, same inverted return
''  convention (TRUE = error), same call sites.
''
function LoadThemeFile() as boolean

    dim as DWSTRING wszPath = Theme_ResolvePath( gConfig.ThemeShortFilename )
    if PsLen(wszPath) = 0 then return true

    ' Reads gConfig after Theme_ResolvePath, which may have rewritten ThemeShortFilename when
    ' it fell back to the backup copy.
    gThemeCurrentShortFilename = gConfig.ThemeShortFilename

    if Theme_ParseFile( wszPath ) then return true

    Theme_ResolveFallbacks()
    Theme_FillGlobals()
    Theme_CaptureBaseline()

    ' AFTER the parse, not before it as this used to be: Theme_IsDark now consults the
    ' theme's own general.appearance key, which does not exist until the file is read. A
    ' failed parse therefore leaves the caption alone, which is correct -- nothing loaded.
    Theme_ApplyDarkCaption( HWND_FRMMAIN )

    if environ("TIKO_THEME_TRACE") = "1" then
        Theme_DumpGlobals( PsExePath & "theme_dump.txt" )
    end if

    return false

end function

