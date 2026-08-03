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
        ' The untitled workspace's backing file. Derived from the exe path, never persisted,
        ' and deliberately per-install -- C:\dev\tiko and C:\dev\tiko_editor each get their
        ' own. Owned by Workspace_EstablishUntitled; IsProjectNamed compares against it.
        UntitledProjectFilename   as DWSTRING

        DateFileTime              as FILETIME
        
        SettingsVersion           as DWSTRING
        Tools(any)                as TYPE_TOOLS
        Builds(any)               as TYPE_BUILDS
        Cat(any)                  as TYPE_CATEGORIES
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
        wszLastActiveProject      as DWSTRING
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
        ' Unused Symbols report. The toggles are a bitmask of (1 shl UNUSED_KIND); the
        ' default 63 is all six kinds on, which is what the report is for.
        UnusedKindMask            as long = 63
        UnusedSortCol             as long = 0
        UnusedSortDesc            as boolean = false
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
        ' Watch open documents for a change made by another program and re-read them without
        ' asking. Notepad++ calls this pair "File Status Auto-Detection" + "Update silently".
        ' DEFAULT OFF: a silent reload discards nothing only while the buffer is unmodified,
        ' so opting in is the user's call, not ours.
        DetectExternalFileChanges as long = false
        ' Delete spaces and tabs sitting at the end of a line when the document is written
        ' to disk. DEFAULT OFF: it rewrites lines the user never touched, which shows up as
        ' noise in a diff of a file shared with someone else, so opting in is their call.
        StripTrailingWhitespace   as long = false
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
        ' The CODE FORMATTER's rule set (modFormat.bi). Deliberately a nested struct rather
        ' than 16 loose fields: the Format Options dialog stages a whole copy of it for
        ' Cancel, and one assignment is both cheaper and impossible to get half-right.
        '
        ' NOTE this is entirely SEPARATE from KeywordCase above, which is display-only --
        ' it maps to SCI_STYLESETCASE and changes how keywords are RENDERED without
        ' touching a byte of the buffer. Format.CaseKeywords rewrites the file. Two
        ' settings that can legitimately disagree, and merging them would mean a display
        ' preference silently deciding what gets written to disk.
        Format                    as FORMAT_RULES
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
        ' The Output panel's FLOATING (undocked) state. OutputFloating is the dock state and is
        ' ORTHOGONAL to ShowOutputPanel above, which means "the Output surface is visible,
        ' wherever it happens to live" -- a floating panel can be hidden and a docked one shown.
        ' The five geometry fields are SCALED screen coordinates, the same convention as
        ' StartupLeft/HelpStartupLeft above; a zero Right/Bottom is the "no stored geometry"
        ' sentinel that frmOutputFloat_CreateWindow heals with a default size.
        OutputFloating            as long = 0
        OutputFloatLeft           as long = 0
        OutputFloatTop            as long = 0
        OutputFloatRight          as long = 0
        OutputFloatBottom         as long = 0
        OutputFloatMaximized      as long = false
        ' The debugger window. The five geometry fields are SCALED screen coordinates, the
        ' Startup/HelpStartup convention; a zero Right/Bottom is the "never saved" sentinel
        ' that leaves frmDebug_Show's centred default in place.
        '
        ' The three splitters are NOT pixels. They are a PERCENT x 100 of the span each bar
        ' divides, because this window is resizable and can reopen at a different size, on a
        ' different monitor, at a different DPI -- where a stored pixel offset either sits
        ' against an edge or lands outside the pane entirely. ShowPanelWidth above is stored
        ' in pixels and gets away with it only because its window is a fixed-width dock.
        DebugLeft                 as long = 0
        DebugTop                  as long = 0
        DebugRight                as long = 0
        DebugBottom               as long = 0
        DebugMaximized            as long = false
        DebugSplitMain            as long = FRMDEBUG_DEFPCTMAIN
        DebugSplitLeft            as long = FRMDEBUG_DEFPCTLEFT
        DebugSplitRight           as long = FRMDEBUG_DEFPCTRIGHT
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

        declare constructor()
        declare function SetCategoryDefaults() as long
        declare function LoadKeywords() as long
        declare function SaveKeywords() as long
        declare function WriteMRU() as long
        declare function WriteMRUProjects() as long
        ' Wrapper + body, same as LoadConfigFile: the flag is owned by the wrapper so no
        ' exit path in the body can strand it.
        declare function SaveConfigFile() as long
        declare function SaveConfigFileBody() as long
        ' LoadConfigFile is a THIN WRAPPER that sets gApp.PreventConfigLoad, runs the body,
        ' and clears the flag on every path including an early error return. Do not move
        ' the flag back into the body: it used to live there, and the one early return that
        ' bypassed the clear left external config-change detection dead for the session.
        declare function LoadConfigFile( byval isHotReload as boolean = false ) as long
        declare function LoadConfigFileBody( byval isHotReload as boolean ) as long
        declare function ProjectSaveToFile() as boolean
        declare function ProjectLoadFromFile( byval wszFile as DWSTRING ) as boolean    
        declare function LoadCodetipsFB() as boolean
        declare function LoadCodetips() as long
        declare function ReloadConfigFileTest() as boolean    
end type
