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

#pragma once


type TYPE_BUILDS
    id               as string    ' GUID
    wszDescription   as DWSTRING
    IsDefault        as long      ' 0:False, 1:True
    Is32bit          as long      ' 0:False, 1:True
    Is64bit          as long      ' 0:False, 1:True
    ExcludeInPopup   as long      ' 0:False, 1:True  (do not show in statusbar popup menu)
    wszOptions       as DWSTRING     ' Compiler options (manual and selected from listbox)
    idMenu           as long      ' Used to match selected build from statusbar popup menu 
                                ' because some items can be excluded from the popup.
    IsCtrl           as long
    IsAlt            as long
    IsShift          as long
    wszKey           as DWSTRING
end type

type TYPE_TOOLS
    wszDescription   as DWSTRING
    wszCommand       as DWSTRING
    wszParameters    as DWSTRING
    wszKey           as DWSTRING
    wszWorkingFolder as DWSTRING
    IsCtrl           as long
    IsAlt            as long
    IsShift          as long
    IsPromptRun      as long
    IsMinimized      as long
    IsWaitFinish     as long
    IsDisplayMenu    as long
    Action           as long 
end type

type TYPE_CATEGORIES
    idFileType       as DWSTRING    ' GUID or special node value (FILETYPE_*)
    wszDescription   as DWSTRING
    bShow            as boolean = true
end type

' NOTE: These node types are different values than the FileType defines from
' the clsDocument.bi file so we could not reuse those equates. These nodetype
' equates defined the order in which the files will be displayed in the 
' explorer listbox.
    #define CATINDEX_FILES             0
    #define CATINDEX_MAIN              1
    #define CATINDEX_RESOURCE          2
    #define CATINDEX_HEADER            3
    #define CATINDEX_MODULE            4
    #define CATINDEX_NORMAL            5

type clsConfig
    public:
        ConfigFilename            as DWSTRING 
        FBKeywordsFilename        as DWSTRING 
        WinApiKeywordsFilename    as DWSTRING 
        ExtraKeywordsFilename     as DWSTRING 
        FBKeywordsDefaultFilename as DWSTRING 
        FBCodetipsFilename        as DWSTRING
        DefaultSessionFilename    as DWSTRING 
        HelpTableOfContents       as DWSTRING
         
        DateFileTime              as FILETIME
        
        SettingsVersion           as DWSTRING
        Tools(any)                as TYPE_TOOLS
        ToolsTemp(any)            as TYPE_TOOLS  
        Builds(any)               as TYPE_BUILDS
        Cat(any)                  as TYPE_CATEGORIES
        CatTemp(any)              as TYPE_CATEGORIES
        ExplorerPositionRight     as long = false
        FBKeywords                as string
        WinApiKeywords            as string
        ExtraKeywords             as string
        bKeywordsDirty            as boolean = true       ' not saved to file
        AskExit                   as long = false         ' use long so true/False string not written to file
        AutoSaveFiles             as long = false
        AutoSaveInterval          as long = 10            ' seconds between autosave checks
        idAutoSaveTimer           as long = 999           ' id of Autosave timer
        RestoreSession            as long = true
        wszLastActiveSession      as DWSTRING
        CompactMenus              as long = false
        CheckUpdates              as long = true
        ShowPanel                 as long = true
        ShowPanelWidth            as long = 250
        ' The Output panel's persisted state is this PAIR, not one tri-state: all four
        ' combinations are meaningful, and (0,1) -- closed, but reopens minimized -- is the
        ' one a single enum could not express. Do not collapse them.
        '   1,0 open at the user height   1,1 open, minimized
        '   0,0 closed, reopens at height 0,1 closed, reopens minimized
        ' All four fields are refreshed together by frmOutput_CaptureState(), which
        ' SaveConfigFile calls before writing -- so every save path persists a coherent set
        ' rather than a blend of two sessions. Nothing else may write them.
        ShowOutputPanel           as long = true
        ' ALWAYS UNSCALED, in memory as well as on disk -- it is scaled only at the point of
        ' use (frmOutput_RestorePanel, frmOutput_Show). It used to be scaled in place after
        ' the window was created and unscaled again in frmMain_OnClose, which meant every
        ' save that was not the final one wrote a scaled number into a field documented as
        ' unscaled, and the height grew by the DPI factor on every launch.
        ShowOutputPanelHeight     as long = OUTPUT_TABS_HEIGHT * 5 ' user set height
        ShowOutputPanelMinimized  as long = true          ' if window is at minimum height (toggled "minimized")
        ShowOutputPanelIndex      as long = 0             ' persisted copy of the Output tab bar's
                                                          ' current panel (default: Compiler Results).
                                                          ' The bar itself owns it at runtime --
                                                          ' PsSelectBar_Get/SetCurSel.
        SyntaxHighlighting        as long = true
        Codetips                  as long = true
        AutoComplete              as long = true
        CharacterAutoComplete     as long = false
        RightEdge                 as long = false
        RightEdgePosition         as DWSTRING = "80"
        LeftMargin                as long = true
        FoldMargin                as long = false
        AutoIndentation           as long = true
        ForNextVariable           as long = false
        ConfineCaret              as long = true
        LineNumbering             as long = true
        HighlightCurrentLine      as long = true
        IndentGuides              as long = false
        PositionMiddle            as long = false         ' position found text to middle of screen
        ClickToggleBreakpoint     as long = false         ' left margin click toggles Breakpoint instead of Bookmark
        BraceHighlight            as long = false
        OccurrenceHighlight       as long = false
        TabIndentSpaces           as long = true
        MultipleInstances         as long = true
        CompileAutosave           as long = true
        UnicodeEncoding           as long = false
        ' The encoding a File>New document starts life with -- one of the FILE_ENCODING_*
        ' values from clsDocument.bi (which tiko.bas includes ahead of this file). It is a
        ' NEW key rather than a repurposing of UnicodeEncoding above: every settings.ini
        ' already on disk carries "UnicodeEncoding=0", which read as an encoding would mean
        ' ANSI, silently changing the default for every existing install.
        NewFileEncoding           as long = FILE_ENCODING_UTF8
        TabSize                   as DWSTRING = "4"
        LocalizationFile          as DWSTRING = "english.lang"
        EditorFontname            as DWSTRING = "Consolas"
        EditorFontCharSet         as DWSTRING = "Default"
        EditorFontsize            as DWSTRING = "11"
        FontExtraSpace            as DWSTRING = "2"
        ThemeShortFilename        as DWSTRING = "default_dark.theme"
        KeywordCase               as long = 3  ' "Original Case"
        StartupLeft               as long = 0
        StartupTop                as long = 0
        StartupRight              as long = 0
        StartupBottom             as long = 0
        StartupMaximized          as long = false
        HelpStartupLeft           as long = 0
        HelpStartupTop            as long = 0
        HelpStartupRight          as long = 0
        HelpStartupBottom         as long = 0
        HelpStartupMaximized      as long = false
        HelpLeftPanelWidth        as long = 0
        FBWINCompiler32           as DWSTRING
        FBWINCompiler64           as DWSTRING
        CompilerBuild             as DWSTRING     ' Build GUID
        CompilerSwitches          as DWSTRING
        CompilerIncludes          as DWSTRING
        CompilerHelpfile          as DWSTRING
        RunViaCommandWindow       as long = false
        DisableCompileBeep        as long = false
        MRU(9)                    as DWSTRING
        MRUProject(9)             as DWSTRING
        MRUSession(9)             as DWSTRING
                                
        declare constructor()
        declare function SetCategoryDefaults() as long
        declare function LoadKeywords() as long
        declare function SaveKeywords() as long
        declare function WriteMRU() as long
        declare function WriteMRUProjects() as long
        declare function WriteMRUSessions() as long
        declare function SaveConfigFile() as long
        declare function LoadConfigFile( byval isHotReload as boolean = false ) as long
        declare function CloseSessionFile( byref wszSessionFile as wstring ) as boolean    
        declare function SaveSessionFile( byref wszSessionFile as wstring ) as boolean    
        declare function LoadSessionFile( byref wszSessionFile as wstring ) as boolean   
        declare function SaveDefaultSessionFile() as boolean    
        declare function LoadDefaultSessionFile() as boolean    
        declare function ProjectSaveToFile() as boolean    
        declare function ProjectLoadFromFile( byval wszFile as DWSTRING ) as boolean    
        declare function LoadCodetipsFB() as boolean
        declare function LoadCodetips() as long
        declare function ReloadConfigFileTest() as boolean    
end type
