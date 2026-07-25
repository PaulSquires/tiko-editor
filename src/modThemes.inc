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

'https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmsetwindowattribute
'declare function DwmSetWindowAttribute lib "Dwmapi.dll" alias "DwmSetWindowAttribute" ( _
'    hwnd as HWND, dwAttribute as DWORD, pvAttribute as LPCVOID, cbAttribute as DWORD) as HRESULT
'const DWMWA_USE_IMMERSIVE_DARK_MODE = 20


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
end type
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
    THSUB_HELPVIEWER    = &h00000400
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
''  ghGeneral.description / .version are the two exceptions -- they carry DWSTRING payloads
''  rather than colours and are filled by hand in Theme_FillGlobals.
''
type THEMEBIND_TYPE
    keyValue      as zstring * 40
    pFore         as COLORREF ptr    ' 0 when the key has no such channel
    pBack         as COLORREF ptr
    pBold         as boolean ptr
    pItalic       as boolean ptr
    pUnder        as boolean ptr
    pForeAlpha    as long ptr        ' 0 unless alpha has somewhere real to land
    pBackAlpha    as long ptr
    nSubsys       as ulong
    bAlphaCapable as boolean         ' can alpha on this key actually reach a pixel?
end type
dim shared gThemeBind(any) as THEMEBIND_TYPE

type GENERAL_HANDLES_TYPE
    description as DWSTRING
    version as DWSTRING
    iconfail as COLORREF      
    iconsuccess as COLORREF      
end type
dim shared ghGeneral as GENERAL_HANDLES_TYPE

type MAIN_HANDLES_TYPE
    BackColor as COLORREF
end type
dim shared ghMain as MAIN_HANDLES_TYPE

type EDITOR_HANDLES_TYPE
    ForeColorCaret as COLORREF
    BackColorCurrentLine as COLORREF
    ForeColorText as COLORREF
    BackColorText as COLORREF
    TextBold as boolean
    TextItalic as boolean
    TextUnderline as boolean
    ForeColorNumbers as COLORREF
    BackColorNumbers as COLORREF
    NumbersBold as boolean
    NumbersItalic as boolean
    NumbersUnderLine as boolean
    ForeColorComments as COLORREF
    BackColorComments as COLORREF
    CommentsBold as boolean
    CommentsItalic as boolean
    CommentsUnderline as boolean
    ForeColorKeyword as COLORREF
    BackColorKeyword as COLORREF
    KeywordBold as boolean
    KeywordItalic as boolean
    KeywordUnderline as boolean
    ForeColorKeyword2 as COLORREF
    BackColorKeyword2 as COLORREF
    KeywordBold2 as boolean
    KeywordItalic2 as boolean
    KeywordUnderline2 as boolean
    ForeColorKeyword3 as COLORREF
    BackColorKeyword3 as COLORREF
    KeywordBold3 as boolean
    KeywordItalic3 as boolean
    KeywordUnderline3 as boolean
    ForeColorFoldmargin as COLORREF
    ForeColorFoldsymbol as COLORREF
    BackColorFoldsymbol as COLORREF
    ForeColorLinenumbers as COLORREF
    BackColorLinenumbers as COLORREF
    LinenumbersBold as boolean
    LinenumbersItalic as boolean
    LinenumbersUnderline as boolean
    ForeColorOperators as COLORREF
    BackColorOperators as COLORREF
    OperatorsBold as boolean
    OperatorsItalic as boolean
    OperatorsUnderline as boolean
    ForeColorIndentguides as COLORREF
    BackColorIndentguides as COLORREF
    ForeColorPreprocessor as COLORREF
    BackColorPreprocessor as COLORREF
    PreprocessorBold as boolean
    PreprocessorItalic as boolean
    PreprocessorUnderline as boolean
    ForeColorSelection as COLORREF
    BackColorSelection as COLORREF
    ' -1 means "the theme did not ask for an alpha", and every consumer then keeps doing
    ' exactly what it does today -- Scintilla's own default for the selection, and the
    ' hardcoded 80/90 for the two occurrence indicators. That sentinel is what lets alpha
    ' land in Phase 1 without changing a single pixel until a theme opts in.
    AlphaSelection as long = -1
    ForeColorStrings as COLORREF
    BackColorStrings as COLORREF
    StringsBold as boolean
    StringsItalic as boolean
    StringsUnderline as boolean
    ForeColorBracegood as COLORREF
    ForeColorBracebad as COLORREF
    ForeColorOccurrence as COLORREF
    AlphaOccurrence as long = -1     ' see AlphaSelection
    BackColorScrollBar as COLORREF
    ForeColorScrollBar as COLORREF
    Divider as COLORREF
    ForeColorBookmark as COLORREF
    BackColorBookmark as COLORREF
    ForeColorBreakpoint as COLORREF
    BackColorBreakpoint as COLORREF
    ForeColorDebugLine as COLORREF
    BackColorDebugLine as COLORREF
end type
dim shared ghEditor as EDITOR_HANDLES_TYPE

type PANEL_HANDLES_TYPE
    BackColorScrollBar as COLORREF
    ForeColorScrollBar as COLORREF
    BackColor as COLORREF
    ForeColor as COLORREF
    BackColorHot as COLORREF
    ForeColorHot as COLORREF
    BackColorButton as COLORREF
    ForeColorButton as COLORREF
    BackColorButtonHot as COLORREF
    ForeColorButtonHot as COLORREF
    BackColorFilenameIcon as COLORREF
    ForeColorFilenameIcon as COLORREF
    BackColorFunctionIcon as COLORREF
    ForeColorFunctionIcon as COLORREF
    BackColorTypeIcon as COLORREF
    ForeColorTypeIcon as COLORREF
    ForeColorTextSearch as COLORREF
    ForeColorTextSearchHot as COLORREF
end type
dim shared ghPanel as PANEL_HANDLES_TYPE

type MENUBAR_HANDLES_TYPE
    BackColor as COLORREF
    BackColorHot as COLORREF
    ForeColor as COLORREF
    ForeColorHot as COLORREF
end type
dim shared ghMenuBar as MENUBAR_HANDLES_TYPE

type POPUPMENU_HANDLES_TYPE
    PanelColor as COLORREF
    BackColor as COLORREF
    BackColorHot as COLORREF
    BackColorDisabled as COLORREF
    ForeColor as COLORREF
    ForeColorHot as COLORREF
    ForeColorDisabled as COLORREF
end type
dim shared ghPopup as POPUPMENU_HANDLES_TYPE

type TOPTABS_HANDLES_TYPE
    BackColor as COLORREF
    BackColorHot as COLORREF
    ForeColor as COLORREF
    ForeColorHot as COLORREF
    Divider as COLORREF
    CloseBackColorHot as COLORREF
    IconSelectedBackColor as COLORREF
end type
dim shared ghTopTabs as TOPTABS_HANDLES_TYPE

type STATUSBAR_HANDLES_TYPE
    BackColor as COLORREF
    BackColorHot as COLORREF
    ForeColor as COLORREF
    ForeColorHot as COLORREF
end type
dim shared ghStatusBar as STATUSBAR_HANDLES_TYPE

type OUTPUT_HANDLES_TYPE
    BackColor as COLORREF
    BackColorHot as COLORREF
    ForeColor as COLORREF
    ForeColorHot as COLORREF
    Divider as COLORREF
    CloseBackColorHot as COLORREF
    BackColorScrollBar as COLORREF
    ForeColorScrollBar as COLORREF
    ScrollBarDivider as COLORREF
end type
dim shared ghOutput as OUTPUT_HANDLES_TYPE

' Chrome that is not the editor but has always been dressed FROM the editor keys, because
' there was nowhere else to put it: the app-wide hairline, the fill/text of every PsTextBox,
' and the Options scroll panels. Splitting the theme UI into "Syntax" and "Interface" pages
' makes that borrowing untenable -- a user retuning code colours would silently restyle the
' Options dialog. These keys give the chrome a home of its own.
'
' NOTHING READS THESE YET. Theme_ResolveFallbacks fills them from the editor.* values when a
' theme does not define them, so every existing theme keeps working byte-for-byte; repointing
' the ~20 consumers is a separate, reviewable patch and deliberately not part of this one.
type UI_HANDLES_TYPE
    Divider as COLORREF
    ForeColorField as COLORREF
    BackColorField as COLORREF
    ForeColorScrollBar as COLORREF
    BackColorScrollBar as COLORREF
    ' The theme's ACCENT -- the one colour that means "this is on / selected / active".
    ' Every control that needs it was reaching into some unrelated group for it (the toggle
    ' used a panel button grey, which is why an ON switch read as dimmer than an OFF one).
    AccentColor as COLORREF
end type
dim shared ghUI as UI_HANDLES_TYPE


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
sub Theme_ResolveFallbacks()

    for i as long = lbound(gThemeFallback) to ubound(gThemeFallback)

        dim as long nSrc = Theme_FindEntry( gThemeFallback(i).srcKey )
        if nSrc < 0 then continue for

        dim as long nDst = Theme_EnsureEntry( gThemeFallback(i).dstKey )

        if gThemeFallback(i).nChan = THEMECH_FORE then
            if gTheme(nDst).hasFore = false then
                gTheme(nDst).foreground      = gTheme(nSrc).foreground
                gTheme(nDst).foregroundAlpha = gTheme(nSrc).foregroundAlpha
                gTheme(nDst).hasForeAlpha    = gTheme(nSrc).hasForeAlpha
                gTheme(nDst).hasFore         = true
            end if
        else
            if gTheme(nDst).hasBack = false then
                gTheme(nDst).background      = gTheme(nSrc).background
                gTheme(nDst).backgroundAlpha = gTheme(nSrc).backgroundAlpha
                gTheme(nDst).hasBackAlpha    = gTheme(nSrc).hasBackAlpha
                gTheme(nDst).hasBack         = true
            end if
        end if

    next

end sub


''
''  RESOLVE A THEME SHORT FILENAME TO A FULL PATH
''
''  Applies the historical fallback chain: the requested theme, then default_dark, then the
''  backup copy in settings\ (which is also copied back into themes\ so the next run finds
''  it there). Returns "" only if even the backup is missing.
''
function Theme_ResolvePath( byval wszShortFilename as DWSTRING ) as DWSTRING

    dim as DWSTRING wszFilename = AfxGetExePathName & "settings\themes\" & wszShortFilename
    if AfxFileExists(wszFilename) = 0 then
        ' default to the visual studio dark theme
        wszFilename = AfxGetExePathName & "settings\themes\default_dark.theme"
        if AfxFileExists(wszFilename) = 0 then
            ' If the theme still does not exist then most likely the Themes folder is
            ' missing or the theme was renamed. Use the backup version in the settings folder.
            wszFilename = AfxGetExePathName & "settings\backup_default_dark.theme"
            ' Attempt to copy the file for next time
            dim as DWSTRING wszThemeDirFilename = AfxGetExePathName & "settings\themes\default_dark.theme"
            if AfxCopyFile( wszFilename, wszThemeDirFilename ) then
            gConfig.ThemeShortFilename = "default_dark.theme"
            end if
        end if
        if AfxFileExists(wszFilename) = 0 then return ""
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

    dim pStream as CTextStream
    if pStream.Open(wszFullPath) <> S_OK then return true   ' error

    ' Ensure to delete any existing gTheme data
    erase gTheme
    erase gThemeParseWarnings

    dim param(100) as DWSTRING
    dim param_value(100) as DWSTRING
    dim as long nextParam = 0
    
    dim as DWSTRING wst, keyValue, keyData
        
    Do Until pStream.EOS
        wst = pStream.ReadLine

        wst = trim(AfxStrExtract( 1, wst, "#"))     ' remove comments
        If Len(wst) = 0 Then Continue Do

        keyValue = trim(AfxStrParse(wst, 1, ":"))
        keyData = trim(AfxStrParse(wst, 2, ":"))
        
        ' is this a replaceable parameter
        if left(wst, 1) = "%" then
            ' param()/param_value() are fixed size (see the dim above). Writing past the
            ' end would corrupt the stack, and a hand-edited theme is free to declare as
            ' many macros as it likes -- so drop the excess and say so.
            if nextParam > ubound(param) then
                redim preserve gThemeParseWarnings(ubound(gThemeParseWarnings) + 1)
                gThemeParseWarnings(ubound(gThemeParseWarnings)) = "macro table full, ignored: " & keyValue
            else
                param(nextParam) = keyValue
                param_value(nextParam) = keyData
                nextParam += 1
            end if
            continue do
        end if

        ' if keyData is a replaceable parameter then substitute it now
        if left(keyData, 1) = "%" then
            for i as long = lbound(param) to ubound(param)
            if param(i) = keyData then
                keyData = param_value(i)
                exit for
            end if   
            next
        end if
        
        ' is the KeyData a true or false value
        if keyData = "true" then keyData = "1"
        if keyData = "false" then keyData = "0"
        
        ' determine number of keys in the keyvalue. There can be 2 or 3.
        ' if 2 then simply store the data in keyData. if 3 then break the
        ' data into the component properties.
        dim keys(3) as string   ' always have maximum of 3 keys
        dim as long numKeys = AfxStrParseCount(keyValue, ".")  ' actual number of keys retrieved
        for i as long = 1 to numKeys
            keys(i) = AfxStrParse(keyValue, i, ".")
        next
        ' there will always be at least 2 elements to the key and
        ' we use that to lookup the theme type structure.
        keyValue = keys(1) & "." & keys(2)
        
        ' generate the color value. An optional 4th component is alpha (0-255); its absence
        ' means opaque. The component COUNT is what distinguishes the two -- val("") is 0,
        ' i.e. fully transparent, which is the exact opposite of what "r,g,b" means.
        dim as long r, g, b, clr
        r = val(AfxStrParse(keyData, 1, ","))
        g = val(AfxStrParse(keyData, 2, ","))
        b = val(AfxStrParse(keyData, 3, ","))
        clr = BGR(r, g, b)

        dim as long nAlpha = 255
        dim as boolean bHasAlpha = false
        if AfxStrParseCount(keyData, ",") >= 4 then
            nAlpha = val(AfxStrParse(keyData, 4, ","))
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
        select case keys(3)
            case "foreground"
                gTheme(nFound).foreground = clr
                gTheme(nFound).hasFore = true
                gTheme(nFound).foregroundAlpha = cubyte(nAlpha)
                gTheme(nFound).hasForeAlpha = bHasAlpha
            case "background"
                gTheme(nFound).background = clr
                gTheme(nFound).hasBack = true
                gTheme(nFound).backgroundAlpha = cubyte(nAlpha)
                gTheme(nFound).hasBackAlpha = bHasAlpha
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
    loop
    
    pStream.Close

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

    dim as string ns = AfxStrParse( k, 1, "." )

    select case ns
        ' frmAutoComplete derives its row colours from the editor styles (keyword2, keyword3,
        ' numbers, comments, selection, currentline), so the whole namespace feeds it.
        case "editor"    : return THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE
        ' ghPanel dresses the Options dialog and the symbol search popup as well as the panels.
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
private sub ThemeBind_Add( _
        byval k as zstring ptr, _
        byval pF as COLORREF ptr = 0, _
        byval pB as COLORREF ptr = 0, _
        byval pBo as boolean ptr = 0, _
        byval pIt as boolean ptr = 0, _
        byval pUn as boolean ptr = 0, _
        byval nSub as ulong = 0, _
        byval pFA as long ptr = 0, _
        byval pBA as long ptr = 0 )

    dim as long n = ubound(gThemeBind) + 1
    redim preserve gThemeBind(n) as THEMEBIND_TYPE

    gThemeBind(n).keyValue   = *k
    gThemeBind(n).pFore      = pF
    gThemeBind(n).pBack      = pB
    gThemeBind(n).pBold      = pBo
    gThemeBind(n).pItalic    = pIt
    gThemeBind(n).pUnder     = pUn
    gThemeBind(n).pForeAlpha = pFA
    gThemeBind(n).pBackAlpha = pBA

    ' Zero means "derive from the namespace".
    if nSub = 0 then nSub = Theme_SubsysForNamespace( *k )
    gThemeBind(n).nSubsys = nSub

    ' Alpha is claimed only where a consumer exists TODAY. Scintilla styles and GDI text are
    ' opaque by construction, and PsBufferPaint's SetXxxColorA overloads are called by no
    ' control yet -- so an alpha slider on any other key would move a number that reaches no
    ' pixel. Having an alpha destination IS the definition of being alpha-capable.
    dim as boolean bHasFA = (pFA <> 0)
    dim as boolean bHasBA = (pBA <> 0)
    gThemeBind(n).bAlphaCapable = (bHasFA orelse bHasBA)

end sub


''
''  BUILD THE KEY -> DESTINATION TABLE
''
''  Transcribed from the hand-written copy block this replaced. A key with no row here never
''  loads at all, which is deliberate: the failure is visible on the first run rather than
''  only during a live edit.
''
''  Deliberately ABSENT, and asserted absent by the self-test's dead-key list: panel.panel,
''  menubar.panel, toptabs.panel, statusbar.panel, output.panel and editor.window. All six
''  are present in the shipped theme files and all six had no destination in the original
''  code either -- four were looked up into a pointer that the next line overwrote before
''  reading, one was commented out, and editor.window was never referenced at all.
''
private sub Theme_BuildBindTable()

    static as boolean bBuilt
    if bBuilt then exit sub
    bBuilt = true

    erase gThemeBind

    ' ---- general / compile / main ----------------------------------------------------
    ThemeBind_Add( "compile.iconsuccess", @ghGeneral.iconsuccess )
    ThemeBind_Add( "compile.iconfail",    @ghGeneral.iconfail )
    ThemeBind_Add( "main.panel",          0, @ghMain.BackColor )

    ' ---- editor: syntax styles (fore + back + bold/italic/underline) ------------------
    ' editor.text also dresses the autocomplete popup, the Options textboxes and the help
    ' viewer, so it reaches well beyond Scintilla.
    ThemeBind_Add( "editor.text", @ghEditor.ForeColorText, @ghEditor.BackColorText, _
                   @ghEditor.TextBold, @ghEditor.TextItalic, @ghEditor.TextUnderline, _
                   THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE or _
                   THSUB_OPTIONSDLG or THSUB_HELPVIEWER )
    ThemeBind_Add( "editor.comments", @ghEditor.ForeColorComments, @ghEditor.BackColorComments, _
                   @ghEditor.CommentsBold, @ghEditor.CommentsItalic, @ghEditor.CommentsUnderline )
    ThemeBind_Add( "editor.keyword", @ghEditor.ForeColorKeyword, @ghEditor.BackColorKeyword, _
                   @ghEditor.KeywordBold, @ghEditor.KeywordItalic, @ghEditor.KeywordUnderline )
    ThemeBind_Add( "editor.keyword2", @ghEditor.ForeColorKeyword2, @ghEditor.BackColorKeyword2, _
                   @ghEditor.KeywordBold2, @ghEditor.KeywordItalic2, @ghEditor.KeywordUnderline2 )
    ThemeBind_Add( "editor.keyword3", @ghEditor.ForeColorKeyword3, @ghEditor.BackColorKeyword3, _
                   @ghEditor.KeywordBold3, @ghEditor.KeywordItalic3, @ghEditor.KeywordUnderline3 )
    ThemeBind_Add( "editor.numbers", @ghEditor.ForeColorNumbers, @ghEditor.BackColorNumbers, _
                   @ghEditor.NumbersBold, @ghEditor.NumbersItalic, @ghEditor.NumbersUnderLine )
    ThemeBind_Add( "editor.operators", @ghEditor.ForeColorOperators, @ghEditor.BackColorOperators, _
                   @ghEditor.OperatorsBold, @ghEditor.OperatorsItalic, @ghEditor.OperatorsUnderline )
    ThemeBind_Add( "editor.preprocessor", @ghEditor.ForeColorPreprocessor, @ghEditor.BackColorPreprocessor, _
                   @ghEditor.PreprocessorBold, @ghEditor.PreprocessorItalic, @ghEditor.PreprocessorUnderline )
    ThemeBind_Add( "editor.strings", @ghEditor.ForeColorStrings, @ghEditor.BackColorStrings, _
                   @ghEditor.StringsBold, @ghEditor.StringsItalic, @ghEditor.StringsUnderline )
    ThemeBind_Add( "editor.linenumbers", @ghEditor.ForeColorLinenumbers, @ghEditor.BackColorLinenumbers, _
                   @ghEditor.LinenumbersBold, @ghEditor.LinenumbersItalic, @ghEditor.LinenumbersUnderline )

    ' ---- editor: surface, margins, markers --------------------------------------------
    ThemeBind_Add( "editor.caret",        @ghEditor.ForeColorCaret )
    ThemeBind_Add( "editor.currentline",  0, @ghEditor.BackColorCurrentLine )
    ThemeBind_Add( "editor.foldmargin",   @ghEditor.ForeColorFoldmargin )
    ThemeBind_Add( "editor.foldsymbol",   @ghEditor.ForeColorFoldsymbol, @ghEditor.BackColorFoldsymbol )
    ThemeBind_Add( "editor.indentguides", @ghEditor.ForeColorIndentguides, @ghEditor.BackColorIndentguides )
    ThemeBind_Add( "editor.bracegood",    @ghEditor.ForeColorBracegood )
    ThemeBind_Add( "editor.bracebad",     @ghEditor.ForeColorBracebad )
    ThemeBind_Add( "editor.bookmark",     @ghEditor.ForeColorBookmark, @ghEditor.BackColorBookmark )
    ThemeBind_Add( "editor.breakpoint",   @ghEditor.ForeColorBreakpoint, @ghEditor.BackColorBreakpoint )
    ThemeBind_Add( "editor.debugline",    @ghEditor.ForeColorDebugLine, @ghEditor.BackColorDebugLine )

    ' The two alpha-bearing keys. SCI_SETSELALPHA takes the selection's background alpha and
    ' SCI_INDICSETALPHA the occurrence indicator's -- the only two places in the editor where
    ' an alpha reaches a pixel at all.
    ThemeBind_Add( "editor.selection", @ghEditor.ForeColorSelection, @ghEditor.BackColorSelection, _
                   0, 0, 0, 0, 0, @ghEditor.AlphaSelection )
    ThemeBind_Add( "editor.occurrence", @ghEditor.ForeColorOccurrence, 0, _
                   0, 0, 0, 0, @ghEditor.AlphaOccurrence )

    ' editor.scrollbar dresses the Options scroll panels and the autocomplete list too.
    ThemeBind_Add( "editor.scrollbar", @ghEditor.ForeColorScrollBar, @ghEditor.BackColorScrollBar, _
                   0, 0, 0, _
                   THSUB_SCINTILLA or THSUB_EDITORCHROME or THSUB_AUTOCOMPLETE or THSUB_OPTIONSDLG )

    ' editor.divider is THE app-wide hairline: menu separators, tab-bar border, every Ps*
    ' control border built by OptionsTheme_Fill*, message-box frames, the autocomplete frame.
    ' Read in 20 files, so it feeds everything.
    ThemeBind_Add( "editor.divider", @ghEditor.Divider, 0, 0, 0, 0, THSUB_ALL )

    ' ---- ui: chrome split out of the editor namespace ----------------------------------
    ' Filled by Theme_ResolveFallbacks when a theme does not define them. All three are
    ' app-wide, hence THSUB_ALL.
    ThemeBind_Add( "ui.divider",   @ghUI.Divider,            0,                  0, 0, 0, THSUB_ALL )
    ThemeBind_Add( "ui.field",     @ghUI.ForeColorField,     @ghUI.BackColorField,     0, 0, 0, THSUB_ALL )
    ThemeBind_Add( "ui.scrollbar", @ghUI.ForeColorScrollBar, @ghUI.BackColorScrollBar, 0, 0, 0, THSUB_ALL )
    ThemeBind_Add( "ui.accent",    0, @ghUI.AccentColor, 0, 0, 0, THSUB_ALL )

    ' ---- panel ------------------------------------------------------------------------
    ThemeBind_Add( "panel.text",           @ghPanel.ForeColor, @ghPanel.BackColor )
    ThemeBind_Add( "panel.texthot",        @ghPanel.ForeColorHot, @ghPanel.BackColorHot )
    ThemeBind_Add( "panel.scrollbar",      @ghPanel.ForeColorScrollBar, @ghPanel.BackColorScrollBar )
    ThemeBind_Add( "panel.button",         @ghPanel.ForeColorButton, @ghPanel.BackColorButton )
    ThemeBind_Add( "panel.buttonhot",      @ghPanel.ForeColorButtonHot, @ghPanel.BackColorButtonHot )
    ThemeBind_Add( "panel.filenameicon",   @ghPanel.ForeColorFilenameIcon, @ghPanel.BackColorFilenameIcon )
    ThemeBind_Add( "panel.functionicon",   @ghPanel.ForeColorFunctionIcon, @ghPanel.BackColorFunctionIcon )
    ThemeBind_Add( "panel.typeicon",       @ghPanel.ForeColorTypeIcon, @ghPanel.BackColorTypeIcon )
    ThemeBind_Add( "panel.textsearch",     @ghPanel.ForeColorTextSearch )
    ThemeBind_Add( "panel.textsearchhot",  @ghPanel.ForeColorTextSearchHot )

    ' ---- topmenu (feeds ghPopup, not ghMenuBar) ----------------------------------------
    ThemeBind_Add( "topmenu.panel",        0, @ghPopup.PanelColor )
    ThemeBind_Add( "topmenu.text",         @ghPopup.ForeColor, @ghPopup.BackColor )
    ThemeBind_Add( "topmenu.texthot",      @ghPopup.ForeColorHot, @ghPopup.BackColorHot )
    ThemeBind_Add( "topmenu.textdisabled", @ghPopup.ForeColorDisabled, @ghPopup.BackColorDisabled )

    ' ---- menubar -----------------------------------------------------------------------
    ThemeBind_Add( "menubar.text",         @ghMenuBar.ForeColor, @ghMenuBar.BackColor )
    ThemeBind_Add( "menubar.texthot",      @ghMenuBar.ForeColorHot, @ghMenuBar.BackColorHot )

    ' ---- toptabs -----------------------------------------------------------------------
    ThemeBind_Add( "toptabs.text",         @ghTopTabs.ForeColor, @ghTopTabs.BackColor )
    ThemeBind_Add( "toptabs.texthot",      @ghTopTabs.ForeColorHot, @ghTopTabs.BackColorHot )
    ThemeBind_Add( "toptabs.divider",      @ghTopTabs.Divider )
    ThemeBind_Add( "toptabs.closehot",     0, @ghTopTabs.CloseBackColorHot )
    ThemeBind_Add( "toptabs.iconselected", 0, @ghTopTabs.IconSelectedBackColor )

    ' ---- statusbar ---------------------------------------------------------------------
    ThemeBind_Add( "statusbar.text",       @ghStatusBar.ForeColor, @ghStatusBar.BackColor )
    ThemeBind_Add( "statusbar.texthot",    @ghStatusBar.ForeColorHot, @ghStatusBar.BackColorHot )

    ' ---- output ------------------------------------------------------------------------
    ThemeBind_Add( "output.text",             @ghOutput.ForeColor, @ghOutput.BackColor )
    ThemeBind_Add( "output.texthot",          @ghOutput.ForeColorHot, @ghOutput.BackColorHot )
    ThemeBind_Add( "output.divider",          @ghOutput.Divider )
    ThemeBind_Add( "output.closehot",         0, @ghOutput.CloseBackColorHot )
    ThemeBind_Add( "output.scrollbar",        @ghOutput.ForeColorScrollBar, @ghOutput.BackColorScrollBar )
    ThemeBind_Add( "output.scrollbardivider", @ghOutput.ScrollBarDivider )

end sub


''
''  WHICH SUBSYSTEMS DOES THIS KEY FEED?
''
function Theme_SubsystemsForKey( byval lookupKey as string ) as ulong
    Theme_BuildBindTable()
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        if gThemeBind(i).keyValue = lookupKey then return gThemeBind(i).nSubsys
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
    Theme_BuildBindTable()
    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        if gThemeBind(i).keyValue = lookupKey then return gThemeBind(i).bAlphaCapable
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
            if nAlpha >= 0 then
                gTheme(n).foregroundAlpha = cubyte(iif(nAlpha > 255, 255, nAlpha))
                gTheme(n).hasForeAlpha = true
            end if
        case THEMECH_BACK
            gTheme(n).background = clr
            gTheme(n).hasBack = true
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
        dim as string s = lcase(trim(gTheme(n).keyData))
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

    gThemeCurrentShortFilename = gThemeSnapFilename

    Theme_ResolveFallbacks()
    Theme_FillGlobals()

end sub


''
''  DROP THE SNAPSHOT                                                  (OK was pressed)
''
sub Theme_DiscardSnapshot()
    erase gThemeSnapEntries
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
        if nChannel = THEMECH_FORE then
            gTheme(n).foreground      = gThemeBaseEntries(i).foreground
            gTheme(n).foregroundAlpha = gThemeBaseEntries(i).foregroundAlpha
            gTheme(n).hasForeAlpha    = gThemeBaseEntries(i).hasForeAlpha
        else
            gTheme(n).background      = gThemeBaseEntries(i).background
            gTheme(n).backgroundAlpha = gThemeBaseEntries(i).backgroundAlpha
            gTheme(n).hasBackAlpha    = gThemeBaseEntries(i).hasBackAlpha
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

    Theme_BuildBindTable()

    dim as THEME_TYPE ptr pTheme

    ' The two DWSTRING payloads have no home in the bind table, which holds COLORREF
    ' pointers. They are the only hand-written rows left.
    pTheme = getThemePtr("general.description")
    ghGeneral.description = pTheme->keyData
    pTheme = getThemePtr("general.version")
    ghGeneral.version = pTheme->keyData

    for i as long = lbound(gThemeBind) to ubound(gThemeBind)
        dim as THEMEBIND_TYPE ptr pBind = @gThemeBind(i)
        pTheme = getThemePtr( pBind->keyValue )

        if pBind->pFore   <> 0 then *pBind->pFore   = pTheme->foreground
        if pBind->pBack   <> 0 then *pBind->pBack   = pTheme->background
        if pBind->pBold   <> 0 then *pBind->pBold   = (pTheme->bold <> 0)
        if pBind->pItalic <> 0 then *pBind->pItalic = (pTheme->italic <> 0)
        if pBind->pUnder  <> 0 then *pBind->pUnder  = (pTheme->underline <> 0)

        ' Alpha only lands when the theme actually asked for one. Without the has* test a
        ' plain "r,g,b" would drive the destination to the 255 default and silently take
        ' over from whatever constant the consumer uses today.
        if pBind->pForeAlpha <> 0 then
            if pTheme->hasForeAlpha then
                *pBind->pForeAlpha = clng(pTheme->foregroundAlpha)
            else
                *pBind->pForeAlpha = -1
            end if
        end if
        if pBind->pBackAlpha <> 0 then
            if pTheme->hasBackAlpha then
                *pBind->pBackAlpha = clng(pTheme->backgroundAlpha)
            else
                *pBind->pBackAlpha = -1
            end if
        end if
    next



        
    

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

    Theme_BuildBindTable()

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

    sLine = "general.description: " & Theme_SanitizeText( wszDescription )
    print #f, sLine
    dim as long nVer = Theme_FindEntry( "general.version" )
    dim as string sVer = "1.0.0"
    if nVer >= 0 then
        if len(gTheme(nVer).keyData) > 0 then sVer = Theme_SanitizeText( gTheme(nVer).keyData )
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

    for i as long = lbound(gThemeBind) to ubound(gThemeBind)

        dim as string sKey = gThemeBind(i).keyValue
        dim as long n = Theme_FindEntry( sKey )
        if n < 0 then continue for

        ' Blank line between namespaces, purely so the file stays readable by hand.
        dim as string sNamespace = AfxStrParse( sKey, 1, "." )
        if (len(sLastNamespace) > 0) andalso (sNamespace <> sLastNamespace) then print #f, ""
        sLastNamespace = sNamespace

        if (gThemeBind(i).pFore <> 0) andalso gTheme(n).hasFore then
            dim as long nA = -1
            if gTheme(n).hasForeAlpha then nA = clng(gTheme(n).foregroundAlpha)
            sLine = sKey & ".foreground: " & Theme_FormatColor( gTheme(n).foreground, nA )
            print #f, sLine
        end if

        if (gThemeBind(i).pBack <> 0) andalso gTheme(n).hasBack then
            dim as long nA = -1
            if gTheme(n).hasBackAlpha then nA = clng(gTheme(n).backgroundAlpha)
            sLine = sKey & ".background: " & Theme_FormatColor( gTheme(n).background, nA )
            print #f, sLine
        end if

        ' Flags are written for every key that HAS them bound, present or not, so a syntax
        ' style always shows its full triplet rather than a partial one.
        if gThemeBind(i).pBold <> 0 then
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
        sLine = nm & "=" & v
        print #f, sLine
    #endmacro

    DUMPTXT( "general.description",        ghGeneral.description )
    DUMPTXT( "general.version",            ghGeneral.version )
    DUMPCLR( "general.iconfail",           ghGeneral.iconfail )
    DUMPCLR( "general.iconsuccess",        ghGeneral.iconsuccess )

    DUMPCLR( "main.BackColor",             ghMain.BackColor )

    DUMPCLR( "editor.ForeColorCaret",         ghEditor.ForeColorCaret )
    DUMPCLR( "editor.BackColorCurrentLine",   ghEditor.BackColorCurrentLine )
    DUMPCLR( "editor.ForeColorText",          ghEditor.ForeColorText )
    DUMPCLR( "editor.BackColorText",          ghEditor.BackColorText )
    DUMPFLG( "editor.TextBold",               ghEditor.TextBold )
    DUMPFLG( "editor.TextItalic",             ghEditor.TextItalic )
    DUMPFLG( "editor.TextUnderline",          ghEditor.TextUnderline )
    DUMPCLR( "editor.ForeColorNumbers",       ghEditor.ForeColorNumbers )
    DUMPCLR( "editor.BackColorNumbers",       ghEditor.BackColorNumbers )
    DUMPFLG( "editor.NumbersBold",            ghEditor.NumbersBold )
    DUMPFLG( "editor.NumbersItalic",          ghEditor.NumbersItalic )
    DUMPFLG( "editor.NumbersUnderLine",       ghEditor.NumbersUnderLine )
    DUMPCLR( "editor.ForeColorComments",      ghEditor.ForeColorComments )
    DUMPCLR( "editor.BackColorComments",      ghEditor.BackColorComments )
    DUMPFLG( "editor.CommentsBold",           ghEditor.CommentsBold )
    DUMPFLG( "editor.CommentsItalic",         ghEditor.CommentsItalic )
    DUMPFLG( "editor.CommentsUnderline",      ghEditor.CommentsUnderline )
    DUMPCLR( "editor.ForeColorKeyword",       ghEditor.ForeColorKeyword )
    DUMPCLR( "editor.BackColorKeyword",       ghEditor.BackColorKeyword )
    DUMPFLG( "editor.KeywordBold",            ghEditor.KeywordBold )
    DUMPFLG( "editor.KeywordItalic",          ghEditor.KeywordItalic )
    DUMPFLG( "editor.KeywordUnderline",       ghEditor.KeywordUnderline )
    DUMPCLR( "editor.ForeColorKeyword2",      ghEditor.ForeColorKeyword2 )
    DUMPCLR( "editor.BackColorKeyword2",      ghEditor.BackColorKeyword2 )
    DUMPFLG( "editor.KeywordBold2",           ghEditor.KeywordBold2 )
    DUMPFLG( "editor.KeywordItalic2",         ghEditor.KeywordItalic2 )
    DUMPFLG( "editor.KeywordUnderline2",      ghEditor.KeywordUnderline2 )
    DUMPCLR( "editor.ForeColorKeyword3",      ghEditor.ForeColorKeyword3 )
    DUMPCLR( "editor.BackColorKeyword3",      ghEditor.BackColorKeyword3 )
    DUMPFLG( "editor.KeywordBold3",           ghEditor.KeywordBold3 )
    DUMPFLG( "editor.KeywordItalic3",         ghEditor.KeywordItalic3 )
    DUMPFLG( "editor.KeywordUnderline3",      ghEditor.KeywordUnderline3 )
    DUMPCLR( "editor.ForeColorFoldmargin",    ghEditor.ForeColorFoldmargin )
    DUMPCLR( "editor.ForeColorFoldsymbol",    ghEditor.ForeColorFoldsymbol )
    DUMPCLR( "editor.BackColorFoldsymbol",    ghEditor.BackColorFoldsymbol )
    DUMPCLR( "editor.ForeColorLinenumbers",   ghEditor.ForeColorLinenumbers )
    DUMPCLR( "editor.BackColorLinenumbers",   ghEditor.BackColorLinenumbers )
    DUMPFLG( "editor.LinenumbersBold",        ghEditor.LinenumbersBold )
    DUMPFLG( "editor.LinenumbersItalic",      ghEditor.LinenumbersItalic )
    DUMPFLG( "editor.LinenumbersUnderline",   ghEditor.LinenumbersUnderline )
    DUMPCLR( "editor.ForeColorOperators",     ghEditor.ForeColorOperators )
    DUMPCLR( "editor.BackColorOperators",     ghEditor.BackColorOperators )
    DUMPFLG( "editor.OperatorsBold",          ghEditor.OperatorsBold )
    DUMPFLG( "editor.OperatorsItalic",        ghEditor.OperatorsItalic )
    DUMPFLG( "editor.OperatorsUnderline",     ghEditor.OperatorsUnderline )
    DUMPCLR( "editor.ForeColorIndentguides",  ghEditor.ForeColorIndentguides )
    DUMPCLR( "editor.BackColorIndentguides",  ghEditor.BackColorIndentguides )
    DUMPCLR( "editor.ForeColorPreprocessor",  ghEditor.ForeColorPreprocessor )
    DUMPCLR( "editor.BackColorPreprocessor",  ghEditor.BackColorPreprocessor )
    DUMPFLG( "editor.PreprocessorBold",       ghEditor.PreprocessorBold )
    DUMPFLG( "editor.PreprocessorItalic",     ghEditor.PreprocessorItalic )
    DUMPFLG( "editor.PreprocessorUnderline",  ghEditor.PreprocessorUnderline )
    DUMPCLR( "editor.ForeColorSelection",     ghEditor.ForeColorSelection )
    DUMPCLR( "editor.BackColorSelection",     ghEditor.BackColorSelection )
    DUMPCLR( "editor.ForeColorStrings",       ghEditor.ForeColorStrings )
    DUMPCLR( "editor.BackColorStrings",       ghEditor.BackColorStrings )
    DUMPFLG( "editor.StringsBold",            ghEditor.StringsBold )
    DUMPFLG( "editor.StringsItalic",          ghEditor.StringsItalic )
    DUMPFLG( "editor.StringsUnderline",       ghEditor.StringsUnderline )
    DUMPCLR( "editor.ForeColorBracegood",     ghEditor.ForeColorBracegood )
    DUMPCLR( "editor.ForeColorBracebad",      ghEditor.ForeColorBracebad )
    DUMPCLR( "editor.ForeColorOccurrence",    ghEditor.ForeColorOccurrence )
    DUMPCLR( "editor.BackColorScrollBar",     ghEditor.BackColorScrollBar )
    DUMPCLR( "editor.ForeColorScrollBar",     ghEditor.ForeColorScrollBar )
    DUMPCLR( "editor.Divider",                ghEditor.Divider )
    DUMPCLR( "editor.ForeColorBookmark",      ghEditor.ForeColorBookmark )
    DUMPCLR( "editor.BackColorBookmark",      ghEditor.BackColorBookmark )
    DUMPCLR( "editor.ForeColorBreakpoint",    ghEditor.ForeColorBreakpoint )
    DUMPCLR( "editor.BackColorBreakpoint",    ghEditor.BackColorBreakpoint )
    DUMPCLR( "editor.ForeColorDebugLine",     ghEditor.ForeColorDebugLine )
    DUMPCLR( "editor.BackColorDebugLine",     ghEditor.BackColorDebugLine )

    DUMPCLR( "panel.BackColorScrollBar",      ghPanel.BackColorScrollBar )
    DUMPCLR( "panel.ForeColorScrollBar",      ghPanel.ForeColorScrollBar )
    DUMPCLR( "panel.BackColor",               ghPanel.BackColor )
    DUMPCLR( "panel.ForeColor",               ghPanel.ForeColor )
    DUMPCLR( "panel.BackColorHot",            ghPanel.BackColorHot )
    DUMPCLR( "panel.ForeColorHot",            ghPanel.ForeColorHot )
    DUMPCLR( "panel.BackColorButton",         ghPanel.BackColorButton )
    DUMPCLR( "panel.ForeColorButton",         ghPanel.ForeColorButton )
    DUMPCLR( "panel.BackColorButtonHot",      ghPanel.BackColorButtonHot )
    DUMPCLR( "panel.ForeColorButtonHot",      ghPanel.ForeColorButtonHot )
    DUMPCLR( "panel.BackColorFilenameIcon",   ghPanel.BackColorFilenameIcon )
    DUMPCLR( "panel.ForeColorFilenameIcon",   ghPanel.ForeColorFilenameIcon )
    DUMPCLR( "panel.BackColorFunctionIcon",   ghPanel.BackColorFunctionIcon )
    DUMPCLR( "panel.ForeColorFunctionIcon",   ghPanel.ForeColorFunctionIcon )
    DUMPCLR( "panel.BackColorTypeIcon",       ghPanel.BackColorTypeIcon )
    DUMPCLR( "panel.ForeColorTypeIcon",       ghPanel.ForeColorTypeIcon )
    DUMPCLR( "panel.ForeColorTextSearch",     ghPanel.ForeColorTextSearch )
    DUMPCLR( "panel.ForeColorTextSearchHot",  ghPanel.ForeColorTextSearchHot )

    DUMPCLR( "menubar.BackColor",             ghMenuBar.BackColor )
    DUMPCLR( "menubar.BackColorHot",          ghMenuBar.BackColorHot )
    DUMPCLR( "menubar.ForeColor",             ghMenuBar.ForeColor )
    DUMPCLR( "menubar.ForeColorHot",          ghMenuBar.ForeColorHot )

    DUMPCLR( "popup.PanelColor",              ghPopup.PanelColor )
    DUMPCLR( "popup.BackColor",               ghPopup.BackColor )
    DUMPCLR( "popup.BackColorHot",            ghPopup.BackColorHot )
    DUMPCLR( "popup.BackColorDisabled",       ghPopup.BackColorDisabled )
    DUMPCLR( "popup.ForeColor",               ghPopup.ForeColor )
    DUMPCLR( "popup.ForeColorHot",            ghPopup.ForeColorHot )
    DUMPCLR( "popup.ForeColorDisabled",       ghPopup.ForeColorDisabled )

    DUMPCLR( "toptabs.BackColor",             ghTopTabs.BackColor )
    DUMPCLR( "toptabs.BackColorHot",          ghTopTabs.BackColorHot )
    DUMPCLR( "toptabs.ForeColor",             ghTopTabs.ForeColor )
    DUMPCLR( "toptabs.ForeColorHot",          ghTopTabs.ForeColorHot )
    DUMPCLR( "toptabs.Divider",               ghTopTabs.Divider )
    DUMPCLR( "toptabs.CloseBackColorHot",     ghTopTabs.CloseBackColorHot )
    DUMPCLR( "toptabs.IconSelectedBackColor", ghTopTabs.IconSelectedBackColor )

    DUMPCLR( "statusbar.BackColor",           ghStatusBar.BackColor )
    DUMPCLR( "statusbar.BackColorHot",        ghStatusBar.BackColorHot )
    DUMPCLR( "statusbar.ForeColor",           ghStatusBar.ForeColor )
    DUMPCLR( "statusbar.ForeColorHot",        ghStatusBar.ForeColorHot )

    DUMPCLR( "output.BackColor",              ghOutput.BackColor )
    DUMPCLR( "output.BackColorHot",           ghOutput.BackColorHot )
    DUMPCLR( "output.ForeColor",              ghOutput.ForeColor )
    DUMPCLR( "output.ForeColorHot",           ghOutput.ForeColorHot )
    DUMPCLR( "output.Divider",                ghOutput.Divider )
    DUMPCLR( "output.CloseBackColorHot",      ghOutput.CloseBackColorHot )
    DUMPCLR( "output.BackColorScrollBar",     ghOutput.BackColorScrollBar )
    DUMPCLR( "output.ForeColorScrollBar",     ghOutput.ForeColorScrollBar )
    DUMPCLR( "output.ScrollBarDivider",       ghOutput.ScrollBarDivider )

    DUMPCLR( "ui.Divider",                    ghUI.Divider )
    DUMPCLR( "ui.ForeColorField",             ghUI.ForeColorField )
    DUMPCLR( "ui.BackColorField",             ghUI.BackColorField )
    DUMPCLR( "ui.ForeColorScrollBar",         ghUI.ForeColorScrollBar )
    DUMPCLR( "ui.BackColorScrollBar",         ghUI.BackColorScrollBar )
    DUMPCLR( "ui.AccentColor",                ghUI.AccentColor )

    ' The two alpha sentinels. -1 means the theme asked for no alpha and the consumer keeps
    ' whatever constant it uses today.
    DUMPFLG( "editor.AlphaSelection_isSet",   (ghEditor.AlphaSelection >= 0) )
    DUMPFLG( "editor.AlphaOccurrence_isSet",  (ghEditor.AlphaOccurrence >= 0) )

    ' Anything the parser accepted but could not act on.
    for i as long = lbound(gThemeParseWarnings) to ubound(gThemeParseWarnings)
        sLine = "!warning=" & gThemeParseWarnings(i)
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
    if len(wszPath) = 0 then return true

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
        Theme_DumpGlobals( AfxGetExePathName & "theme_dump.txt" )
    end if

    return false

end function

