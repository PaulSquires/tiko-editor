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
end type
dim shared gTheme(any) as THEME_TYPE
dim shared gThemeFilenames(any) as DWSTRING

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
    ForeColorStrings as COLORREF
    BackColorStrings as COLORREF
    StringsBold as boolean
    StringsItalic as boolean
    StringsUnderline as boolean
    ForeColorBracegood as COLORREF
    ForeColorBracebad as COLORREF
    ForeColorOccurrence as COLORREF
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
''  LOAD THEME DISK FILE
''
function LoadThemeFile() as boolean

    dim as DWSTRING wszFilename = AfxGetExePathName & "settings\themes\" & gConfig.ThemeShortFilename
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
    end if
    
    dim as long nvalue = false
    if instr(gConfig.ThemeShortFilename, "_dark") then nvalue = true
    ' Attempt to apply the standard Windows dark theme to the non-client areas of the main form.
    DwmSetWindowAttribute(HWND_FRMMAIN, DWMWA_USE_IMMERSIVE_DARK_MODE, @nvalue, sizeof(nvalue))


    dim pStream as CTextStream   
    if pStream.Open(wszFilename) <> S_OK then return true   ' error

    ' Ensure to delete any existing gTheme data
    erase gTheme
    
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
            param(nextParam) = keyValue
            param_value(nextParam) = keyData
            nextParam += 1
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
        
        ' generate the color value
        dim as long r, g, b, clr
        r = val(AfxStrParse(keyData, 1, ","))
        g = val(AfxStrParse(keyData, 2, ","))
        b = val(AfxStrParse(keyData, 3, ","))
        clr = BGR(r, g, b)
        
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
            case "foreground": gTheme(nFound).foreground = clr
            case "background": gTheme(nFound).background = clr
            case "bold":       gTheme(nFound).bold = val(keyData)
            case "italic":     gTheme(nFound).italic = val(keyData)
            case "underline":  gTheme(nFound).underline = val(keyData)         
        end select
    loop
    
    pStream.Close
    
    ' Load the TYPE's for each of the forms that require theme data   
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)

    dim as THEME_TYPE ptr pTheme
    
    pTheme = getThemePtr("general.description")
    ghGeneral.description = pTheme->keyData
    pTheme = getThemePtr("general.version")
    ghGeneral.version = pTheme->keyData
    pTheme = getThemePtr("compile.iconsuccess")
    ghGeneral.iconsuccess = pTheme->foreground
    pTheme = getThemePtr("compile.iconfail")
    ghGeneral.iconfail = pTheme->foreground

    ' set some global brush handles so that we don't always have to be
    ' recreating them every time they are used.
    pTheme = getThemePtr("main.panel")
    ghMain.BackColor = pTheme->background

    pTheme = getThemePtr("editor.text")
    ghEditor.BackColorText = pTheme->background
    ghEditor.ForeColorText = pTheme->foreground
    ghEditor.TextBold = pTheme->bold
    ghEditor.TextItalic = pTheme->italic
    ghEditor.TextUnderline = pTheme->underline
    pTheme = getThemePtr("editor.linenumbers")
    ghEditor.BackColorLinenumbers = pTheme->background
    ghEditor.ForeColorLinenumbers = pTheme->foreground
    ghEditor.LinenumbersBold = pTheme->bold
    ghEditor.LinenumbersItalic = pTheme->italic
    ghEditor.LinenumbersUnderline = pTheme->underline
    pTheme = getThemePtr("editor.foldmargin")
    ghEditor.ForeColorFoldmargin = pTheme->foreground
    pTheme = getThemePtr("editor.indentguides")
    ghEditor.BackColorIndentguides = pTheme->background
    ghEditor.ForeColorIndentguides = pTheme->foreground
    pTheme = getThemePtr("editor.caret")
    ghEditor.ForeColorCaret = pTheme->foreground
    pTheme = getThemePtr("editor.currentline")
    ghEditor.BackColorCurrentline = pTheme->background
    pTheme = getThemePtr("editor.selection")
    ghEditor.BackColorSelection = pTheme->background
    ghEditor.ForeColorSelection = pTheme->foreground
    pTheme = getThemePtr("editor.comments")
    ghEditor.BackColorComments = pTheme->background
    ghEditor.ForeColorComments = pTheme->foreground
    ghEditor.CommentsBold = pTheme->bold
    ghEditor.CommentsItalic = pTheme->italic
    ghEditor.CommentsUnderline = pTheme->underline
    pTheme = getThemePtr("editor.keyword")
    ghEditor.BackColorKeyword = pTheme->background
    ghEditor.ForeColorKeyword = pTheme->foreground
    ghEditor.KeywordBold = pTheme->bold
    ghEditor.KeywordItalic = pTheme->italic
    ghEditor.KeywordUnderline = pTheme->underline
    pTheme = getThemePtr("editor.keyword2")
    ghEditor.BackColorKeyword2 = pTheme->background
    ghEditor.ForeColorKeyword2 = pTheme->foreground
    ghEditor.KeywordBold2 = pTheme->bold
    ghEditor.KeywordItalic2 = pTheme->italic
    ghEditor.KeywordUnderline2 = pTheme->underline
    pTheme = getThemePtr("editor.keyword3")
    ghEditor.BackColorKeyword3 = pTheme->background
    ghEditor.ForeColorKeyword3 = pTheme->foreground
    ghEditor.KeywordBold3 = pTheme->bold
    ghEditor.KeywordItalic3 = pTheme->italic
    ghEditor.KeywordUnderline3 = pTheme->underline
    pTheme = getThemePtr("editor.numbers")
    ghEditor.BackColorNumbers = pTheme->background
    ghEditor.ForeColorNumbers = pTheme->foreground
    ghEditor.NumbersBold = pTheme->bold
    ghEditor.NumbersItalic = pTheme->italic
    ghEditor.NumbersUnderline = pTheme->underline
    pTheme = getThemePtr("editor.operators")
    ghEditor.BackColorOperators = pTheme->background
    ghEditor.ForeColorOperators = pTheme->foreground
    ghEditor.OperatorsBold = pTheme->bold
    ghEditor.OperatorsItalic = pTheme->italic
    ghEditor.OperatorsUnderline = pTheme->underline
    pTheme = getThemePtr("editor.preprocessor")
    ghEditor.BackColorPreprocessor = pTheme->background
    ghEditor.ForeColorPreprocessor = pTheme->foreground
    ghEditor.PreprocessorBold = pTheme->bold
    ghEditor.PreprocessorItalic = pTheme->italic
    ghEditor.PreprocessorUnderline = pTheme->underline
    pTheme = getThemePtr("editor.strings")
    ghEditor.BackColorStrings = pTheme->background
    ghEditor.ForeColorStrings = pTheme->foreground
    ghEditor.StringsBold = pTheme->bold
    ghEditor.StringsItalic = pTheme->italic
    ghEditor.StringsUnderline = pTheme->underline
    pTheme = getThemePtr("editor.foldsymbol")
    ghEditor.BackColorFoldsymbol = pTheme->background
    ghEditor.ForeColorFoldsymbol = pTheme->foreground
    pTheme = getThemePtr("editor.bracegood")
    ghEditor.ForeColorBracegood = pTheme->foreground
    pTheme = getThemePtr("editor.bracebad")
    ghEditor.ForeColorBracebad = pTheme->foreground
    pTheme = getThemePtr("editor.occurrence")
    ghEditor.ForeColorOccurrence = pTheme->foreground
    pTheme = getThemePtr("editor.scrollbar")
    ghEditor.ForeColorScrollBar = pTheme->foreground
    ghEditor.BackColorScrollBar = pTheme->background
    pTheme = getThemePtr("editor.divider")
    ghEditor.Divider = pTheme->foreground
    pTheme = getThemePtr("editor.bookmark")
    ghEditor.ForeColorBookmark = pTheme->foreground
    ghEditor.BackColorBookmark = pTheme->background
    pTheme = getThemePtr("editor.breakpoint")
    ghEditor.ForeColorBreakpoint = pTheme->foreground
    ghEditor.BackColorBreakpoint = pTheme->background
    pTheme = getThemePtr("editor.debugline")
    ghEditor.ForeColorDebugline = pTheme->foreground
    ghEditor.BackColorDebugline = pTheme->background

    pTheme = getThemePtr("panel.panel")
    pTheme = getThemePtr("panel.text")
    ghPanel.BackColor = pTheme->background
    ghPanel.ForeColor = pTheme->foreground
    pTheme = getThemePtr("panel.texthot")
    ghPanel.BackColorHot = pTheme->background
    ghPanel.ForeColorHot = pTheme->foreground
    pTheme = getThemePtr("panel.scrollbar")
    ghPanel.BackColorScrollBar = pTheme->background
    ghPanel.ForeColorScrollBar = pTheme->foreground
    pTheme = getThemePtr("panel.button")
    ghPanel.BackColorButton = pTheme->background
    ghPanel.ForeColorButton = pTheme->foreground
    pTheme = getThemePtr("panel.buttonhot")
    ghPanel.BackColorButtonHot = pTheme->background
    ghPanel.ForeColorButtonHot = pTheme->foreground
    pTheme = getThemePtr("panel.filenameicon")
    ghPanel.BackColorFilenameIcon = pTheme->background
    ghPanel.ForeColorFilenameIcon = pTheme->foreground
    pTheme = getThemePtr("panel.functionicon")
    ghPanel.BackColorFunctionIcon = pTheme->background
    ghPanel.ForeColorFunctionIcon = pTheme->foreground
    pTheme = getThemePtr("panel.typeicon")
    ghPanel.BackColorTypeIcon = pTheme->background
    ghPanel.ForeColorTypeIcon = pTheme->foreground
    pTheme = getThemePtr("panel.textsearch")
    ghPanel.ForeColorTextSearch = pTheme->foreground
    pTheme = getThemePtr("panel.textsearchhot")
    ghPanel.ForeColorTextSearchHot = pTheme->foreground

    pTheme = getThemePtr("topmenu.panel")
    ghPopup.PanelColor = pTheme->background
    pTheme = getThemePtr("topmenu.text")
    ghPopup.BackColor = pTheme->background
    ghPopup.ForeColor = pTheme->foreground
    pTheme = getThemePtr("topmenu.texthot")
    ghPopup.BackColorHot = pTheme->background
    ghPopup.ForeColorHot = pTheme->foreground
    pTheme = getThemePtr("topmenu.textdisabled")
    ghPopup.BackColorDisabled = pTheme->background
    ghPopup.ForeColorDisabled = pTheme->foreground
        
    'pTheme = getThemePtr("menubar.panel")
    pTheme = getThemePtr("menubar.text")
    ghMenuBar.BackColor = pTheme->background
    ghMenuBar.ForeColor = pTheme->foreground
    pTheme = getThemePtr("menubar.texthot")
    ghMenuBar.BackColorHot = pTheme->background
    ghMenuBar.ForeColorHot = pTheme->foreground
    
    pTheme = getThemePtr("toptabs.panel")
    pTheme = getThemePtr("toptabs.text")
    ghTopTabs.BackColor = pTheme->background
    ghTopTabs.ForeColor = pTheme->foreground
    pTheme = getThemePtr("toptabs.texthot")
    ghTopTabs.BackColorHot = pTheme->background
    ghTopTabs.ForeColorHot = pTheme->foreground
    pTheme = getThemePtr("toptabs.divider")
    ghTopTabs.Divider = pTheme->foreground
    pTheme = getThemePtr("toptabs.closehot")
    ghTopTabs.CloseBackColorHot = pTheme->background
    pTheme = getThemePtr("toptabs.iconselected")
    ghTopTabs.IconSelectedBackColor = pTheme->background

    pTheme = getThemePtr("statusbar.panel")
    pTheme = getThemePtr("statusbar.text")
    ghStatusBar.BackColor = pTheme->background
    ghStatusBar.ForeColor = pTheme->foreground
    pTheme = getThemePtr("statusbar.texthot")
    ghStatusBar.BackColorHot = pTheme->background
    ghStatusBar.ForeColorHot = pTheme->foreground

    pTheme = getThemePtr("output.panel")
    pTheme = getThemePtr("output.text")
    ghOutput.BackColor = pTheme->background
    ghOutput.ForeColor = pTheme->foreground
    pTheme = getThemePtr("output.texthot")
    ghOutput.BackColorHot = pTheme->background
    ghOutput.ForeColorHot = pTheme->foreground
    pTheme = getThemePtr("output.divider")
    ghOutput.Divider = pTheme->foreground
    pTheme = getThemePtr("output.closehot")
    ghOutput.CloseBackColorHot = pTheme->background
    pTheme = getThemePtr("output.scrollbar")
    ghOutput.ForeColorScrollBar = pTheme->foreground
    ghOutput.BackColorScrollBar = pTheme->background
    pTheme = getThemePtr("output.scrollbardivider")
    ghOutput.ScrollBarDivider = pTheme->foreground

    function = false

end function

