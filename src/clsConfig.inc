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


''  CONFIGURATION MODULE

#include once "clsConfig.bi"
#include once "modRoutines.bi"
#include once "frmFunctions.bi"
#include once "frmBookmarks.bi"
#include once "frmExplorer.bi"
#include once "frmOutput.bi"
#include once "frmBuildConfig.bi"
#include once "frmMain.bi"
#include once "modMRU.bi"
#include once "frmTopTabs.bi"
#include once "frmOutput.bi"
#include once "frmMainOnCommand.bi"
                
''
''  CONSTRUCTOR
''
constructor clsConfig
    dim as DWSTRING exe_path = AfxGetExePathName 
    ConfigFilename            = exe_path & "settings\settings.ini" 
    FBKeywordsFilename        = exe_path & "settings\keywords\freebasic_keywords.txt" 
    WinApiKeywordsFilename    = exe_path & "settings\keywords\winapi_keywords.txt" 
    ExtraKeywordsFilename     = exe_path & "settings\keywords\extra_keywords.txt" 
    FBKeywordsDefaultFilename = exe_path & "settings\keywords\freebasic_keywords_default.txt" 
    FBCodetipsFilename        = exe_path & "settings\keywords\codetips.ini"
    DefaultSessionFilename    = exe_path & "settings\default.session"
    UntitledProjectFilename   = exe_path & "settings\default" & APPEXTENSION
end constructor

''
''
function clsConfig.LoadKeywords() as long
    if AfxFileExists(this.FBKeywordsFilename) = false then 
        ' The FB keywords file does not exist. Try copying the "default" keywords over
        ' to the main file that the user can modify.
        if AfxFileExists(this.FBKeywordsDefaultFilename) then 
            if AfxCopyFile( this.FBKeywordsDefaultFilename.sptr, this.FBKeywordsFilename.sptr, true ) = false then
                exit function
            end if
        end if
    end if
    if AfxFileExists(this.FBKeywordsFilename) = false then exit function

    dim pStream as CTextStream
    if pStream.Open(this.FBKeywordsFilename) = S_OK then 
        this.FBKeywords = pStream.ReadAll
        pStream.Close
    end if

    if pStream.Open(this.WinApiKeywordsFilename) = S_OK then 
        this.WinApiKeywords = pStream.ReadAll
        pStream.Close
    end if

    if pStream.Open(this.ExtraKeywordsFilename) = S_OK then 
        this.ExtraKeywords = pStream.ReadAll
        pStream.Close
    end if

    function = 0
end function

''
''
function clsConfig.SaveKeywords() as long
    dim pStream as CTextStream
    if pStream.Create(this.FBKeywordsFilename) = S_OK then
        pStream.WriteLine this.FBKeywords
        pStream.Close
    end if
    if pStream.Create(this.WinApiKeywordsFilename) = S_OK then 
        pStream.WriteLine this.WinApiKeywords
        pStream.Close
    end if
    if pStream.Create(this.ExtraKeywordsFilename) = S_OK then 
        pStream.WriteLine this.ExtraKeywords
        pStream.Close
    end if
    function = 0
end function
    
''
''
function clsConfig.LoadCodetipsFB() as boolean

    dim as DWSTRING wst
    dim as long i

    ' Populate gFBIntrinsics(), the sorted lookup FindIntrinsicCalltip binary
    ' searches for built-in calltips (LEN, MID, ...). These names never appear
    ' in any scanned include, so they live outside the symbol database.
    erase gFBIntrinsics
    dim as long nIntrinsic = 0

    dim pStream as CTextStream
    if pStream.Open(this.FBCodetipsFilename) <> S_OK then return true  ' error

    do until pStream.EOS
        wst = pStream.ReadLine
        wst = trim(wst)
        if len(wst) = 0 then continue do

        i = instr(wst, "=")
        if i then
            redim preserve gFBIntrinsics( 0 to nIntrinsic )
            gFBIntrinsics(nIntrinsic).wszName    = left(wst, i-1)
            gFBIntrinsics(nIntrinsic).wszNameUp  = ucase( left(wst, i-1) )
            gFBIntrinsics(nIntrinsic).wszCalltip = mid(wst, i+1)
            nIntrinsic += 1
        end if
    loop
    pStream.Close

    ' sort gFBIntrinsics ascending by wszNameUp for FindIntrinsicCalltip's binary search
    for a as long = 1 to nIntrinsic - 1
        dim as FBINTRINSIC_TIP tmp = gFBIntrinsics(a)
        dim as long b = a - 1
        do while (b >= 0) andalso (gFBIntrinsics(b).wszNameUp > tmp.wszNameUp)
            gFBIntrinsics(b + 1) = gFBIntrinsics(b)
            b -= 1
        loop
        gFBIntrinsics(b + 1) = tmp
    next

    return false  ' no error
end function


''
''
function clsConfig.LoadCodetips() as long
    ' reduced to the FB intrinsics load - the WinAPI/WinFBX codetip tables died
    ' with the old parser database (real WinAPI symbols now come from the scans)
    this.LoadCodetipsFB()
    function = 0
end function

' ========================================================================================
' Enumerate the names of all the fonts. Note the difference between how to enumerate them
' (%TMPF_FIXED_PITCH has the bit cleared).
' %TMPF_FIXED_PITCH for fixed pitch fonts (like in PB edit)
' %TMPF_TRUETYPE OR %TMPF_VECTOR for true type and vector fonts
' %TMPF_DEVICE for device fonts (like printer fonts)
' Exclude what you don't want to include in list.
' ========================================================================================
function fonts_EnumFontName( _
            byref lf as LOGFONTW, _
            byref tm as TEXTMETRIC, _
            ByVal FontType as long, _
            HWnd as HWnd _
            ) as long
    dim as long ub = ubound(gFontNames)
    redim preserve gFontNames(ub + 1) as DWSTRING
    gFontNames(ub + 1) = lf.lfFaceName 
    function = true
end function

' ========================================================================================
' Search the gFontNames() array to see if the font name exists
' ========================================================================================
function isFontNameExist( byval wszFontName as DWSTRING ) as boolean
    wszFontName = ucase(wszFontName)
    for i as long = lbound(gFontnames) to ubound(gFontNames)
        if wszFontName = ucase(gFontNames(i)) then return true   
    next
    return false
end function

' ========================================================================================
' Get a list of all font names installed on this system. We check this list to ensure
' that the currently user selected font is actually available on the system that they
' are using the editor under.
' function returns true of the defined font name was valid on the system so no changes
' were made to the config file fontname value.
' ========================================================================================
function doFontSanityCheck() as long
    erase gFontNames
    dim hDC as HDC = GetDC(0)
    EnumFontFamilies( hDC, ByVal 0, Cast(FONTENUMPROCW, @fonts_EnumFontName), 0 )
    ReleaseDC( 0, hDC )
    
    ' Is the font defined in the config file okay?
    if isFontNameExist( gConfig.EditorFontname ) then return true

    'print "Defined font not found. Finding an alternative..."
    
    ' Test other valid Windows fonts   
    if isFontNameExist( "Consolas" ) then gConfig.EditorFontname = "Consolas": return false
    if isFontNameExist( "MonoLisa" ) then gConfig.EditorFontname = "MonoLisa": return false
    if isFontNameExist( "Apercu Mono" ) then gConfig.EditorFontname = "Apercu Mono": return false
    if isFontNameExist( "Fira Code" ) then gConfig.EditorFontname = "Fira Code": return false
    if isFontNameExist( "Dank Mono" ) then gConfig.EditorFontname = "Dank Mono": return false
    if isFontNameExist( "Hack" ) then gConfig.EditorFontname = "Hack": return false
    if isFontNameExist( "Courier New" ) then gConfig.EditorFontname = "Courier New": return false

    function = false
end function


''
''  SAVE CONFIGURATION TO DISK FILE
''
function clsConfig.SaveConfigFile() as long
    gApp.PreventConfigLoad = true
    
    ' Determine the current editor positioning 
    ' Do not save if editor is minimized
    if isiconic( HWND_FRMMAIN ) = 0 then
        dim WinPla as WINDOWPLACEMENT
        WinPla.Length = sizeof(WinPla)
        GetWindowPlacement( HWND_FRMMAIN, @WinPla )
        With this
            .StartupLeft   = WinPla.rcNormalPosition.Left 
            .StartupTop    = WinPla.rcNormalPosition.Top
            .StartupRight  = WinPla.rcNormalPosition.Right
            .StartupBottom = WinPla.rcNormalPosition.Bottom
            .StartupMaximized = iif( WinPla.showCmd = SW_MAXIMIZE, true, false )
        end With
        
        dim as RECT rc
        GetWindowRect( HWND_FRMMAIN, @rc )
        if this.StartupLeft <> rc.Left then this.StartupLeft = rc.Left
        if this.StartupTop <> rc.Top then this.StartupTop = rc.Top
        if this.StartupRight <> rc.Right then this.StartupRight = rc.Right
        if this.StartupBottom <> rc.Bottom then this.StartupBottom = rc.Bottom
    end if

    ' The Help Center's geometry, refreshed the same way and for the same reason.
    frmHelpCenter_CaptureState()

    ' Refresh the Output panel's four persisted fields from the live windows. This MUST
    ' happen on every save, not just the one in frmMain_OnClose: visibility and tab index
    ' used to be captured only on the way out, so any other save (Options OK, a theme
    ' change, opening a project, a session save) wrote this session's minimized flag beside
    ' the PREVIOUS session's visibility -- which is what a crash then made permanent.
    frmOutput_CaptureState()

    ' The debugger window's geometry and its three splitter positions, when it is open. Same
    ' reason as the two above: a save can happen at any time while a debug session is
    ' running, and it must record where the window is NOW.
    frmDebug_CaptureState()

    this.CompilerBuild = frmBuildConfig_GetSelectedBuildGUID()

    dim pStream as CTextStream   '(utf16)
    if pStream.Create(this.ConfigFilename, true, true) <> S_OK then return true   ' error
    
    pStream.WriteLine "' SETTINGS FILE"
    pStream.WriteLine ""
    pStream.WriteLine "SettingsVersion="       & APPVERSION
    pStream.WriteLine ""
    pStream.WriteLine "[Editor]"
    pStream.WriteLine "AskExit="               & this.AskExit
    pStream.WriteLine "AutoSaveFiles="         & this.AutoSaveFiles
    pStream.WriteLine "AutoSaveInterval="      & this.AutoSaveInterval
    pStream.WriteLine "RestoreSession="        & this.RestoreSession
    if this.RestoreSession = 0 then this.wszLastActiveSession = ""
    pStream.WriteLine "LastActiveSessionFile=" & ProcessToCurdriveApp(this.wszLastActiveSession)
    pStream.WriteLine "CompactMenus="          & this.CompactMenus
    pStream.WriteLine "MultipleInstances="     & this.MultipleInstances    
    pStream.WriteLine "CompileAutosave="       & this.CompileAutosave      
    pStream.WriteLine "CheckUpdates="          & this.CheckUpdates
    pStream.WriteLine "SyntaxHighlighting="    & this.SyntaxHighlighting   
    pStream.WriteLine "Codetips="              & this.Codetips             
    pStream.WriteLine "AutoComplete="          & this.AutoComplete
    pStream.WriteLine "CharacterAutoComplete=" & this.CharacterAutoComplete
    pStream.WriteLine "RightEdge="             & this.RightEdge
    pStream.WriteLine "RightEdgePosition="     & this.RightEdgePosition           
    pStream.WriteLine "LeftMargin="            & this.LeftMargin           
    pStream.WriteLine "FoldMargin="            & this.FoldMargin           
    pStream.WriteLine "AutoIndentation="       & this.AutoIndentation      
    pStream.WriteLine "FornextVariable="       & this.FornextVariable
    pStream.WriteLine "ConfineCaret="          & this.ConfineCaret         
    pStream.WriteLine "LineNumbering="         & this.LineNumbering        
    pStream.WriteLine "HighlightCurrentLine="  & this.HighlightCurrentLine 
    pStream.WriteLine "IndentGuides="          & this.IndentGuides         
    pStream.WriteLine "TabIndentSpaces="       & this.TabIndentSpaces      
    pStream.WriteLine "UnicodeEncoding="       & this.UnicodeEncoding
    pStream.WriteLine "NewFileEncoding="       & this.NewFileEncoding
    pStream.WriteLine "KeywordCase="           & this.KeywordCase
    pStream.WriteLine "LocalizationFile="      & this.LocalizationFile     
    pStream.WriteLine "TabSize="               & this.TabSize              
    pStream.WriteLine "PositionMiddle="        & this.PositionMiddle
    pStream.WriteLine "ClickToggleBreakpoint=" & this.ClickToggleBreakpoint
    pStream.WriteLine "BraceHighlight="        & this.BraceHighlight
    pStream.WriteLine "OccurrenceHighlight="   & this.OccurrenceHighlight
    pStream.WriteLine "DetectExternalFileChanges=" & this.DetectExternalFileChanges
    pStream.WriteLine "EditorFontname="        & this.EditorFontname       
    pStream.WriteLine "EditorFontsize="        & this.EditorFontsize       
    pStream.WriteLine "EditorFontCharSet="     & this.EditorFontCharSet    
    pStream.WriteLine "FontExtraSpace="        & this.FontExtraSpace
    pStream.WriteLine "Theme="                 & this.ThemeShortFilename
    pStream.WriteLine "ExplorerPositionRight=" & this.ExplorerPositionRight
    pStream.WriteLine ""
    ' Note: Main window StartUp positions are in scaled screen coordinates.
    pStream.WriteLine "[Startup]" 
    pStream.WriteLine "StartupLeft="              & this.StartupLeft          
    pStream.WriteLine "StartupTop="               & this.StartupTop           
    pStream.WriteLine "StartupRight="             & this.StartupRight         
    pStream.WriteLine "StartupBottom="            & this.StartupBottom        
    pStream.WriteLine "StartupMaximized="         & this.StartupMaximized     
    ' Note: HelpViewer StartUp positions are in scaled screen coordinates.
    pStream.WriteLine "HelpStartupLeft="          & this.HelpStartupLeft          
    pStream.WriteLine "HelpStartupTop="           & this.HelpStartupTop           
    pStream.WriteLine "HelpStartupRight="         & this.HelpStartupRight         
    pStream.WriteLine "HelpStartupBottom="        & this.HelpStartupBottom        
    pStream.WriteLine "HelpStartupMaximized="     & this.HelpStartupMaximized     
    ' Note: SidePanel and OutputPanel positions are in unscaled screen coordinates.
    pStream.WriteLine "ShowPanel="                & this.ShowPanel
    pStream.WriteLine "ShowPanelWidth="           & this.ShowPanelWidth    
    pStream.WriteLine "ShowOutputPanel="          & this.ShowOutputPanel
    pStream.WriteLine "ShowOutputPanelHeight="    & this.ShowOutputPanelHeight
    pStream.WriteLine "ShowOutputPanelMinimized=" & this.ShowOutputPanelMinimized
    ' Written from this.ShowOutputPanelIndex, which frmMain refreshes from the tab bar
    ' (PsSelectBar_GetCurSel) just before the config is saved.
    pStream.WriteLine "ShowOutputPanelIndex="     & this.ShowOutputPanelIndex
    pStream.WriteLine "UnusedKindMask="           & this.UnusedKindMask
    pStream.WriteLine "UnusedSortCol="            & this.UnusedSortCol
    pStream.WriteLine "UnusedSortDesc="           & this.UnusedSortDesc
    ' Note: the floating Output window's positions are in scaled screen coordinates (the
    ' Startup/HelpStartup convention above, NOT the unscaled one the two lines above use).
    pStream.WriteLine "OutputFloating="           & this.OutputFloating
    pStream.WriteLine "OutputFloatLeft="          & this.OutputFloatLeft
    pStream.WriteLine "OutputFloatTop="           & this.OutputFloatTop
    pStream.WriteLine "OutputFloatRight="         & this.OutputFloatRight
    pStream.WriteLine "OutputFloatBottom="        & this.OutputFloatBottom
    pStream.WriteLine "OutputFloatMaximized="     & this.OutputFloatMaximized
    ' Note: the debugger window's positions are in scaled screen coordinates (the
    ' Startup/HelpStartup convention), but its three splitters are a PERCENT x 10000 of the
    ' span each divides -- see clsConfig.bi for why they are not pixels.
    pStream.WriteLine "DebugLeft="                & this.DebugLeft
    pStream.WriteLine "DebugTop="                 & this.DebugTop
    pStream.WriteLine "DebugRight="               & this.DebugRight
    pStream.WriteLine "DebugBottom="              & this.DebugBottom
    pStream.WriteLine "DebugMaximized="           & this.DebugMaximized
    pStream.WriteLine "DebugSplitMain="           & this.DebugSplitMain
    pStream.WriteLine "DebugSplitLeft="           & this.DebugSplitLeft
    pStream.WriteLine "DebugSplitRight="          & this.DebugSplitRight
    pStream.WriteLine ""

    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    this.FBWINCompiler32  = ProcessToCurdriveApp(this.FBWINCompiler32)
    this.FBWINCompiler64  = ProcessToCurdriveApp(this.FBWINCompiler64)
    this.CompilerHelpfile = ProcessToCurdriveApp(this.CompilerHelpfile)

    ' Iterate the optional semi colon separated list of compiler include paths
    ' Do not modify the this.CompilerIncludes directly because that will put the
    ' {CURDRIVE} macro back into the path names causing future compiles to fail
    ' because that macro only gets resolved on LoadConfig.
    dim as DWSTRING allIncludes = rtrim(this.CompilerIncludes, ";")
    dim as DWSTRING SaveCompilerIncludes
    dim as DWSTRING includePath
    dim as long numpaths = AfxStrParseCount(allIncludes, ";")
    for i as long = 1 to numpaths
        includePath = trim(AfxStrParse(allIncludes, i, ";"))
        if len(includePath) then 
            includePath = ProcessToCurdriveApp(includePath)
            SaveCompilerIncludes = SaveCompilerIncludes + includePath + ";"
        end if
    next

    pStream.WriteLine "[Compiler]" 
    pStream.WriteLine "FBWINCompiler32="       & this.FBWINCompiler32      
    pStream.WriteLine "FBWINCompiler64="       & this.FBWINCompiler64      
    pStream.WriteLine "CompilerBuild="         & this.CompilerBuild     
    pStream.WriteLine "CompilerSwitches="      & this.CompilerSwitches     
    pStream.WriteLine "CompilerIncludes="      & SaveCompilerIncludes
    pStream.WriteLine "CompilerHelpfile="      & this.CompilerHelpfile     
    pStream.WriteLine "RunViaCommandWindow="   & this.RunViaCommandWindow
    pStream.WriteLine "DisableCompileBeep="    & this.DisableCompileBeep

    pStream.WriteLine ""
    pStream.WriteLine "[Categories]"  
    dim as long nNext = 0
    for i as long = lbound(this.Cat) to ubound(this.Cat)
        if left(this.Cat(i).wszDescription, 2) <> "%%" then
            if trim(this.Cat(i).wszDescription) <> "" then
                pStream.Write "CATEGORY_" & right("00" & str(nNext), 2) & "="
                pStream.Write this.Cat(i).idFileType & "|-|"
                pStream.WriteLine this.Cat(i).wszDescription
                nNext = nNext + 1
            end if
        end if   
    next
    
    pStream.WriteLine ""
    pStream.WriteLine "[UserTools]"  
    for i as long = lbound(this.Tools) to ubound(this.Tools)
        pStream.Write "USERTOOL_" & right("00" & str(i), 2) & "="
        pStream.Write this.Tools(i).wszDescription & "|-|"
        pStream.Write ProcessToCurdriveApp(this.Tools(i).wszCommand) & "|-|" 
        pStream.Write this.Tools(i).wszParameters & "|-|"
        pStream.Write this.Tools(i).wszKey & "|-|" 
        pStream.Write ProcessToCurdriveApp(this.Tools(i).wszWorkingFolder) & "|-|"
        pStream.Write this.Tools(i).IsCtrl & "|-|"
        pStream.Write this.Tools(i).IsAlt & "|-|"  
        pStream.Write this.Tools(i).IsShift & "|-|"
        pStream.Write this.Tools(i).IsPromptRun & "|-|"
        pStream.Write this.Tools(i).IsMinimized & "|-|"  
        pStream.Write this.Tools(i).IsWaitFinish & "|-|"
        pStream.Write this.Tools(i).IsDisplayMenu & "|-|"    
        pStream.WriteLine wstr(this.Tools(i).Action)   
    next

    pStream.WriteLine ""
    pStream.WriteLine "[Builds]"  
    for i as long = lbound(this.Builds) to ubound(this.Builds)
        pStream.Write "BUILD_" & right("00" & str(i), 2) & "="
        pStream.Write this.Builds(i).id & "|-|"
        pStream.Write this.Builds(i).wszDescription & "|-|"
        pStream.Write this.Builds(i).wszOptions & "|-|" 
        pStream.Write this.Builds(i).IsDefault & "|-|"  
        pStream.Write this.Builds(i).Is32bit & "|-|"  
        pStream.Write this.Builds(i).Is64bit & "|-|"  
        pStream.Write this.Builds(i).ExcludeInPopup & "|-|" 
        pStream.Write this.Builds(i).IsCtrl & "|-|"
        pStream.Write this.Builds(i).IsAlt & "|-|"  
        pStream.Write this.Builds(i).IsShift & "|-|"
        pStream.WriteLine this.Builds(i).wszKey
    next

    
    pStream.WriteLine ""
    pStream.WriteLine "[MRU]"  
    for i as long = 0 To 9
        this.MRU(i) = ProcessToCurdriveApp(this.MRU(i))
        pStream.WriteLine "MRU_" & right("00" & str(i), 2) & "=" & this.MRU(i) 
    next         
    
    pStream.WriteLine ""
    pStream.WriteLine "[MRUPROJECTS]"  
    for i as long = 0 To 9
        this.MRUProject(i) = ProcessToCurdriveApp(this.MRUProject(i))
        pStream.WriteLine "MRUPROJECT_" & right("00" & str(i), 2) & "=" & this.MRUProject(i) 
    next         

    pStream.WriteLine ""
    pStream.WriteLine "[MRUSESSIONS]"  
    for i as long = 0 To 9
        this.MRUSession(i) = ProcessToCurdriveApp(this.MRUSession(i))
        pStream.WriteLine "MRUSESSION_" & right("00" & str(i), 2) & "=" & this.MRUSession(i) 
    next         
    
    dim hCtl as hwnd = GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES)
    dim wszText as DWSTRING = wstr("NOTES-START") + vbcrlf + _
                            gApp.NonProjectNotes + vbcrlf + _
                            wstr("NOTES-END") + vbcrlf
    pStream.WriteLine ""
    pStream.WriteLine "[Notes]"
    pStream.WriteLine wszText

    pStream.Close 

    ' Save the last write time so that it can be checked in the message loop
    ' in order to detect external changes to it.
    this.DateFileTime = AfxGetFileLastWriteTime( this.ConfigFilename )
    
    gApp.PreventConfigLoad = false

    function = 0
end function


' Save the MRU to the configuration file. Only write the MRU items
' the ini file rather than overwriting the whole file. This enables
' the editor to work better with external tools that manually modify the
' settings.ini file.
function clsConfig.WriteMRU() as long 
    dim as DWSTRING wszKeyName, wszValue
    
    ' Delete the section name
    dim as DWSTRING wszSectionName = "MRU"
    WritePrivateProfileString( wszSectionName, null, null, this.ConfigFilename )
    
    ' Write the new values.
    for i as long = 0 To 9
        this.MRU(i) = ProcessToCurdriveApp(this.MRU(i))
        wszKeyName = "MRU_" & right("00" & str(i), 2)
        wszValue   = this.MRU(i) 
        WritePrivateProfileString(wszSectionName, wszKeyName, wszValue, this.ConfigFilename)
    next         
    
    function = 0
end function


' Save the MRU Projects to the configuration file. Only write the MRU items
' the ini file rather than overwriting the whole file. This enables
' the editor to work better with external tools that manually modify the
' settings.ini file.
function clsConfig.WriteMRUProjects() as long 
    dim as DWSTRING wszKeyName, wszValue
    
    ' Delete the section name
    dim as DWSTRING wszSectionName = "MRUPROJECTS"
    WritePrivateProfileString( wszSectionName, null, null, this.ConfigFilename )
    
    ' Write the new values.
    for i as long = 0 To 9
        this.MRUProject(i) = ProcessToCurdriveApp(this.MRUProject(i))
        wszKeyName = "MRUPROJECT_" & right("00" & str(i), 2) 
        wszValue   = this.MRUProject(i) 
        WritePrivateProfileString( wszSectionName, wszKeyName, wszValue, this.ConfigFilename )
    next         
    
    function = 0
end function


' Save the MRU Sessions to the configuration file. Only write the MRU items
' the ini file rather than overwriting the whole file. This enables
' the editor to work better with external tools that manually modify the
' settings.ini file.
function clsConfig.WriteMRUSessions() as long 
    dim as DWSTRING wszKeyName, wszValue
    
    ' Delete the section name
    dim as DWSTRING wszSectionName = "MRUSESSIONS"
    WritePrivateProfileString( wszSectionName, null, null, this.ConfigFilename )
    
    ' Write the new values.
    for i as long = 0 To 9
        this.MRUSession(i) = ProcessToCurdriveApp(this.MRUSession(i))
        wszKeyName = "MRUSESSION_" & right("00" & str(i), 2) 
        wszValue   = this.MRUSession(i) 
        WritePrivateProfileString( wszSectionName, wszKeyName, wszValue, this.ConfigFilename )
    next         
    
    function = 0
end function
    

''
''  Set default values for Explorer Categories should none exist
''
function clsConfig.SetCategoryDefaults() as long
    if ubound(this.Cat) = -1 then
        redim this.Cat(CATINDEX_NORMAL)

        this.Cat(CATINDEX_FILES).idFileType = FILETYPE_UNDEFINED
        this.Cat(CATINDEX_FILES).wszDescription = L(2, "File")
        
        this.Cat(CATINDEX_MAIN).idFileType = FILETYPE_MAIN
        this.Cat(CATINDEX_MAIN).wszDescription = L(175, "Main")
        
        this.Cat(CATINDEX_RESOURCE).idFileType = FILETYPE_RESOURCE
        this.Cat(CATINDEX_RESOURCE).wszDescription = L(176, "Resource")
        
        this.Cat(CATINDEX_HEADER).idFileType = FILETYPE_HEADER
        this.Cat(CATINDEX_HEADER).wszDescription = L(141, "Header")
        
        this.Cat(CATINDEX_MODULE).idFileType = FILETYPE_MODULE
        this.Cat(CATINDEX_MODULE).wszDescription = L(174, "Module")
        
        this.Cat(CATINDEX_NORMAL).idFileType = FILETYPE_NORMAL
        this.Cat(CATINDEX_NORMAL).wszDescription = L(173, "Normal")
    end if

    function = 0
end function


''
''  LOAD CONFIGURATION FROM DISK FILE
''
function clsConfig.LoadConfigFile( byval isHotReload as boolean = false ) as long
    gApp.PreventConfigLoad = true

    dim as DWSTRING wst, wKey, wData
    dim nData          as long  
    dim i              as long
    dim bReadingNote   as boolean
    dim bNewConfigFile as boolean

    ' Remove any existing Builds
    ' We can not recreate the Categories each time the config reloads because the
    ' structure contains references to existing nodes.
    erase this.Builds
    
    if AfxFileExists(this.ConfigFilename) = 0 then
        bNewConfigFile = true
    else
        dim pStream as CTextStream   '(utf16)
        if pStream.OpenUnicode(this.ConfigFilename) <> S_OK then return true   ' error

        gApp.NonProjectNotes = ""
        
        do until pStream.EOS
            wst = pStream.ReadLine

            if len(wst) = 0 then continue do
            if left(wst, 1) = "'" then continue do
            if left(wst, 1) = "[" then continue do
            
            if left(wst, 11) = "NOTES-START" then  
                bReadingNote = true
                continue do
            end if   
            if left(wst, 9) = "NOTES-END" then  
                bReadingNote = false
                continue do
            end if
            if bReadingNote then
                gApp.NonProjectNotes = gApp.NonProjectNotes + wst + vbcrlf
                continue do
            end IF

            i = instr(wst, "=")
            if i = 0 then continue do
            
            wKey = "": wData = "": nData = 0
            
            wKey  = left(wst, i-1)
            wData = mid(wst, i+1)
            nData = val(wData)
            

            if left(wKey, 9) = "CATEGORY_" then  ' is this a CATEGORY entry
            i = val(right(wKey,2))
            if i > ubound(this.Cat) then
                redim preserve this.Cat(i) 
                this.Cat(i).idFileType     = AfxStrParse(wData, 1, "|-|")
                this.Cat(i).wszDescription = AfxStrParse(wData, 2, "|-|")
                continue do
            end if
            end if
            
            if left(wKey, 9) = "USERTOOL_" then  ' is this a User Tool entry
            i = val(right(wKey,2))
                if i > ubound(this.Tools) then
                    redim preserve this.Tools(i)
                    this.Tools(i).wszDescription   = AfxStrParse(wData, 1, "|-|")
                    this.Tools(i).wszCommand       = ProcessFromCurdriveApp( AfxStrParse(wData, 2, "|-|") ) 
                    this.Tools(i).wszParameters    = AfxStrParse(wData, 3, "|-|")
                    this.Tools(i).wszKey           = AfxStrParse(wData, 4, "|-|")
                    this.Tools(i).wszWorkingFolder = ProcessFromCurdriveApp( AfxStrParse(wData, 5, "|-|") ) 
                    this.Tools(i).IsCtrl           = val(AfxStrParse(wData, 6, "|-|"))
                    this.Tools(i).IsAlt            = val(AfxStrParse(wData, 7, "|-|"))
                    this.Tools(i).IsShift          = val(AfxStrParse(wData, 8, "|-|"))
                    this.Tools(i).IsPromptRun      = val(AfxStrParse(wData, 9, "|-|"))
                    this.Tools(i).IsMinimized      = val(AfxStrParse(wData, 10, "|-|"))
                    this.Tools(i).IsWaitFinish     = val(AfxStrParse(wData, 11, "|-|"))
                    this.Tools(i).IsDisplayMenu    = val(AfxStrParse(wData, 12, "|-|"))
                    this.Tools(i).Action           = val(AfxStrParse(wData, 13, "|-|"))
                    continue do
                end If
            end If

            if left(wKey, 6) = "BUILD_" then  ' is this a BUILD entry
            i = val(right(wKey,2))
                if i > ubound(this.Builds) then
                    redim preserve this.Builds(i) 
                    this.Builds(i).id             = AfxStrParse(wData, 1, "|-|")
                    this.Builds(i).wszDescription = AfxStrParse(wData, 2, "|-|")
                    this.Builds(i).wszOptions     = AfxStrParse(wData, 3, "|-|")
                    this.Builds(i).IsDefault      = val(AfxStrParse(wData, 4, "|-|"))
                    this.Builds(i).Is32bit        = val(AfxStrParse(wData, 5, "|-|"))
                    this.Builds(i).Is64bit        = val(AfxStrParse(wData, 6, "|-|"))
                    this.Builds(i).ExcludeInPopup = val(AfxStrParse(wData, 7, "|-|"))
                    this.Builds(i).IsCtrl         = val(AfxStrParse(wData, 8, "|-|"))
                    this.Builds(i).IsAlt          = val(AfxStrParse(wData, 9, "|-|"))
                    this.Builds(i).IsShift        = val(AfxStrParse(wData, 10, "|-|"))
                    this.Builds(i).wszKey         = AfxStrParse(wData, 11, "|-|")
                    continue do
                end If
            end If

            if left(wKey, 4) = "MRU_" then  ' is this an MRU entry
            i = val(right(wKey,2))
                if (i >= 0) And (i <= 9) then 
                    this.MRU(i) = ProcessFromCurdriveApp(wData)
                    continue do
                end If
            end If
            
            if left(wKey, 11) = "MRUPROJECT_" then  ' is this an MRU Project entry
            i = val(right(wKey,2))
                if (i >= 0) And (i <= 9) then 
                    this.MRUProject(i) = ProcessFromCurdriveApp(wData)
                    continue do
                end If
            end If

            if left(wKey, 11) = "MRUSESSION_" then  ' is this an MRU Session entry
            i = val(right(wKey,2))
                if (i >= 0) And (i <= 9) then 
                    this.MRUSession(i) = ProcessFromCurdriveApp(wData)
                    continue do
                end If
            end If

            
            select case wKey
            case "SettingsVersion":          this.SettingsVersion       = wData
            case "AskExit":                  this.AskExit               = nData
            case "RestoreSession":           this.RestoreSession        = nData
            case "LastActiveSessionFile":    this.wszLastActiveSession  = ProcessFromCurdriveApp(wData)
            case "CompactMenus":             this.CompactMenus          = nData
            case "MultipleInstances":        this.MultipleInstances     = nData
            case "CompileAutosave":          this.CompileAutosave       = nData
            case "CheckUpdates":             this.CheckUpdates          = nData
            case "SyntaxHighlighting":       this.SyntaxHighlighting    = nData
            case "Codetips":                 this.Codetips              = nData
            case "AutoComplete":             this.AutoComplete          = nData
            case "CharacterAutoComplete":    this.CharacterAutoComplete = nData
            case "RightEdge":                this.RightEdge             = nData
            case "RightEdgePosition":        this.RightEdgePosition     = wData
            case "LeftMargin":               this.LeftMargin            = nData
            case "FoldMargin":               this.FoldMargin            = nData
            case "AutoIndentation":          this.AutoIndentation       = nData
            case "FornextVariable":          this.FornextVariable       = nData
            case "ConfineCaret":             this.ConfineCaret          = nData
            case "LineNumbering":            this.LineNumbering         = nData
            case "HighlightCurrentLine":     this.HighlightCurrentLine  = nData
            case "IndentGuides":             this.IndentGuides          = nData
            case "TabIndentSpaces":          this.TabIndentSpaces       = nData
            case "PositionMiddle":           this.PositionMiddle        = nData
            case "ClickToggleBreakpoint":    this.ClickToggleBreakpoint = nData
            case "BraceHighlight":           this.BraceHighlight        = nData
            case "OccurrenceHighlight":      this.OccurrenceHighlight   = nData
            case "DetectExternalFileChanges": this.DetectExternalFileChanges = nData
            case "LocalizationFile":         this.LocalizationFile      = ProcessFromCurdriveApp(wData)
            case "TabSize":                  this.TabSize               = wData
            case "UnicodeEncoding":          this.UnicodeEncoding       = nData
            ' Clamped rather than trusted: this value is fed straight to a document's
            ' FileEncoding, and an out-of-range one would reach the save encoder's
            ' select-case and fall through to no branch at all. A hand-edited or
            ' garbage key falls back to the compiled-in default instead.
            case "NewFileEncoding"
                if (nData >= FILE_ENCODING_ANSI) andalso (nData <= FILE_ENCODING_UTF16_BOM) then
                    this.NewFileEncoding = nData
                else
                    this.NewFileEncoding = FILE_ENCODING_UTF8
                end if
            case "EditorFontname":           this.EditorFontname        = wData
            case "EditorFontsize":           this.EditorFontsize        = wData
            case "EditorFontCharSet":        this.EditorFontCharSet     = wData
            case "FontExtraSpace":           this.FontExtraSpace        = wData
            case "Theme":                    this.ThemeShortFilename    = wData
            case "ExplorerPositionRight":    this.ExplorerPositionRight = nData
            case "KeywordCase":              this.KeywordCase           = nData
            case "FBWINCompiler32":          this.FBWINCompiler32       = ProcessFromCurdriveApp(wData)
            case "FBWINCompiler64":          this.FBWINCompiler64       = ProcessFromCurdriveApp(wData)
            case "CompilerBuild":            this.CompilerBuild         = wData
            case "CompilerSwitches":         this.CompilerSwitches      = wData
            case "CompilerIncludes":         this.CompilerIncludes      = wData
            case "CompilerHelpfile":         this.CompilerHelpfile      = ProcessFromCurdriveApp(wData)
            case "RunViaCommandWindow":      this.RunViaCommandWindow   = nData
            case "DisableCompileBeep":       this.DisableCompileBeep    = nData
            end select
        
            ' Only reload data affect instance states for heights and positions if
            ' this is not a hot reload. A hot reload is when the message loop detects
            ' that the config file has been externally modified (eg by a User Tool)
            ' and then will be reloaded on the fly.
            if isHotReload = false then
                select case wKey
                    case "StartupLeft":              this.StartupLeft           = nData
                    case "StartupTop":               this.StartupTop            = nData
                    case "StartupRight":             this.StartupRight          = nData
                    case "StartupBottom":            this.StartupBottom         = nData
                    case "StartupMaximized":         this.StartupMaximized      = nData
                    case "HelpStartupLeft":          this.HelpStartupLeft       = nData
                    case "HelpStartupTop":           this.HelpStartupTop        = nData
                    case "HelpStartupRight":         this.HelpStartupRight      = nData
                    case "HelpStartupBottom":        this.HelpStartupBottom     = nData
                    case "HelpStartupMaximized":     this.HelpStartupMaximized  = nData
                    case "ShowPanel":                this.ShowPanel             = nData
                    case "ShowPanelWidth":           this.ShowPanelWidth        = nData
                    case "ShowOutputPanel":          this.ShowOutputPanel       = nData
                    case "ShowOutputPanelHeight":    this.ShowOutputPanelHeight = nData
                    case "ShowOutputPanelMinimized": this.ShowOutputPanelMinimized = nData
                    ' A tab ID (an OUTPUT_TAB_* value), not a panel index -- see frmOutput.bi.
                    ' Loaded long before the tab bar exists, so it lands in the config field
                    ' only; frmOutput_Show applies it with frmOutput_SetCurTab once the
                    ' control has been created. Deliberately NOT validated here: only the bar
                    ' knows which ids actually exist, so validating here would mean keeping a
                    ' second copy of that set. SetCurTab heals a retired id (2, the removed
                    ' Search Results tab) to Compiler Results, and frmOutput_CaptureState
                    ' writes the healed value back on the next save.
                    case "ShowOutputPanelIndex":     this.ShowOutputPanelIndex  = nData
                    case "UnusedKindMask":           this.UnusedKindMask        = nData
                    case "UnusedSortCol":            this.UnusedSortCol         = nData
                    case "UnusedSortDesc":           this.UnusedSortDesc        = cbool(nData)
                    ' The floating Output window. These live inside the isHotReload guard for
                    ' the same reason every other geometry key does, and for one extra: a hot
                    ' reload firing while the panel is undocked would otherwise teleport a live
                    ' window to whatever rect happened to be on disk.
                    case "OutputFloating":           this.OutputFloating        = nData
                    case "OutputFloatLeft":          this.OutputFloatLeft       = nData
                    case "OutputFloatTop":           this.OutputFloatTop        = nData
                    case "OutputFloatRight":         this.OutputFloatRight      = nData
                    case "OutputFloatBottom":        this.OutputFloatBottom     = nData
                    case "OutputFloatMaximized":     this.OutputFloatMaximized  = nData
                    ' The debugger window, inside the guard for the same reason: a hot reload
                    ' during a live debug session must not move the window or its bars.
                    case "DebugLeft":                this.DebugLeft             = nData
                    case "DebugTop":                 this.DebugTop              = nData
                    case "DebugRight":               this.DebugRight            = nData
                    case "DebugBottom":              this.DebugBottom           = nData
                    case "DebugMaximized":           this.DebugMaximized        = nData
                    ' Clamped on the way IN, so a hand-edited or corrupt settings.ini heals
                    ' rather than producing a bar outside its own pane. 0 is a legal stored
                    ' value only in the sense that the range clamp will move it back.
                    case "DebugSplitMain":           this.DebugSplitMain  = iif( (nData > 0) andalso (nData < FRMDEBUG_PCTSCALE), nData, FRMDEBUG_DEFPCTMAIN )
                    case "DebugSplitLeft":           this.DebugSplitLeft  = iif( (nData > 0) andalso (nData < FRMDEBUG_PCTSCALE), nData, FRMDEBUG_DEFPCTLEFT )
                    case "DebugSplitRight":          this.DebugSplitRight = iif( (nData > 0) andalso (nData < FRMDEBUG_PCTSCALE), nData, FRMDEBUG_DEFPCTRIGHT )
                end select
            end if
            
        loop
        pStream.Close
    end if
    
    ' Iterate the optional semi colon separated list of compiler include paths
    dim as DWSTRING allIncludes = rtrim(this.CompilerIncludes, ";")
    this.CompilerIncludes = ""
    dim as DWSTRING includePath
    dim as long numpaths = AfxStrParseCount(allIncludes, ";")
    for i as long = 1 to numpaths
        includePath = trim(AfxStrParse(allIncludes, i, ";"))
        if len(includePath) then 
            includePath = ProcessFromCurdriveApp(includePath)
            this.CompilerIncludes = this.CompilerIncludes + includePath + ";"
        end if
    next

    ' Set some defaults if the config file was missing or corrupt
    ' if no Categories exist then create some default ones... 
    ' NOTE: The default for Categories are not set here because some
    ' category names depend on the translation from the Localization
    ' file and that file is not loaded until the Config file is read.
    ' We do the setting of default Categories in gConfig.SetCategoryDefaults
    ' in WinMain after the localization is correctly loaded.
    
    ' Set some defaults if the config file was missing.
    ' if no Tools exist then create some default ones...
    if (ubound(this.Tools) = -1) andalso (bNewConfigFile = true) then
        dim wszTools(1) as DWSTRING
        wszTools(0) = "ASCII Chart|-|.\settings\tools\asciichart32.exe|-||-|1|-||-|1|-|0|-|0|-|0|-|0|-|0|-|1|-|0"
        wszTools(1) = "GUID Generator|-|.\settings\tools\GUIDgen32.exe|-||-|2|-||-|1|-|0|-|0|-|0|-|0|-|0|-|1|-|0"
        for i = lbound(wszTools) to ubound(wszTools)
            if i > ubound(this.Tools) then
                redim preserve this.Tools(i)
                wData = wszTools(i)
                this.Tools(i).wszDescription   = AfxStrParse(wData, 1, "|-|")
                this.Tools(i).wszCommand       = ProcessFromCurdriveApp( AfxStrParse(wData, 2, "|-|") ) 
                this.Tools(i).wszParameters    = AfxStrParse(wData, 3, "|-|")
                this.Tools(i).wszKey           = AfxStrParse(wData, 4, "|-|")
                this.Tools(i).wszWorkingFolder = ProcessFromCurdriveApp( AfxStrParse(wData, 5, "|-|") ) 
                this.Tools(i).IsCtrl           = val(AfxStrParse(wData, 6, "|-|"))
                this.Tools(i).IsAlt            = val(AfxStrParse(wData, 7, "|-|"))
                this.Tools(i).IsShift          = val(AfxStrParse(wData, 8, "|-|"))
                this.Tools(i).IsPromptRun      = val(AfxStrParse(wData, 9, "|-|"))
                this.Tools(i).IsMinimized      = val(AfxStrParse(wData, 10, "|-|"))
                this.Tools(i).IsWaitFinish     = val(AfxStrParse(wData, 11, "|-|"))
                this.Tools(i).IsDisplayMenu    = val(AfxStrParse(wData, 12, "|-|"))
                this.Tools(i).Action           = val(AfxStrParse(wData, 13, "|-|"))
            end If
        next
    end if
            
    ' Set some defaults if the config file was missing or corrupt
    ' if no builds exist then create some default ones... We do this
    ' no matter if it is a bNewConfigFile or not.
    if ubound(this.Builds) = -1 then
        redim this.Builds(11) 
        this.Builds(0).id = AfxGuidText(AfxGuid())
        this.Builds(0).wszDescription = "Win32 GUI (Release)"
        this.Builds(0).wszOptions = "-s gui"
        this.Builds(0).Is32bit = 1
        this.Builds(0).Is64bit = 0
        this.Builds(0).ExcludeInPopup = 0
        
        this.Builds(1).id = AfxGuidText(AfxGuid())
        this.Builds(1).wszDescription = "Win32 GUI (Debug)"
        this.Builds(1).wszOptions = "-g -exx -s gui"
        this.Builds(1).Is32bit = 1
        this.Builds(1).Is64bit = 0
        this.Builds(1).ExcludeInPopup = 1
        
        this.Builds(2).id = AfxGuidText(AfxGuid())
        this.Builds(2).wszDescription = "Win32 Console (Release)"
        this.Builds(2).wszOptions = "-s console"
        this.Builds(2).Is32bit = 1
        this.Builds(2).Is64bit = 0
        this.Builds(2).ExcludeInPopup = 0
        
        this.Builds(3).id = AfxGuidText(AfxGuid())
        this.Builds(3).wszDescription = "Win32 Console (Debug)"
        this.Builds(3).wszOptions = "-g -exx -s console"
        this.Builds(3).Is32bit = 1
        this.Builds(3).Is64bit = 0
        this.Builds(3).ExcludeInPopup = 1

        this.Builds(4).id = AfxGuidText(AfxGuid())
        this.Builds(4).wszDescription = "Win32 Windows DLL"
        this.Builds(4).wszOptions = "-s gui -dll -export"
        this.Builds(4).Is32bit = 1
        this.Builds(4).Is64bit = 0
        this.Builds(4).ExcludeInPopup = 1

        this.Builds(5).id = AfxGuidText(AfxGuid())
        this.Builds(5).wszDescription = "Win32 Static Library"
        this.Builds(5).wszOptions = "-lib"
        this.Builds(5).Is32bit = 1
        this.Builds(5).Is64bit = 0
        this.Builds(5).ExcludeInPopup = 1

        this.Builds(6).id = AfxGuidText(AfxGuid())
        this.Builds(6).wszDescription = "Win64 GUI (Release)"
        this.Builds(6).wszOptions = "-s gui -gen gcc"
        this.Builds(6).Is32bit = 0
        this.Builds(6).Is64bit = 1
        this.Builds(6).ExcludeInPopup = 0
        
        this.Builds(7).id = AfxGuidText(AfxGuid())
        this.Builds(7).wszDescription = "Win64 GUI (Debug)"
        this.Builds(7).wszOptions = "-g -exx -s gui -gen gas64"
        this.Builds(7).Is32bit = 0
        this.Builds(7).Is64bit = 1
        this.Builds(7).ExcludeInPopup = 1
        
        this.Builds(8).id = AfxGuidText(AfxGuid())
        this.Builds(8).wszDescription = "Win64 Console (Release)"
        this.Builds(8).wszOptions = "-s console -gen gcc"
        this.Builds(8).IsDefault = 1
        this.Builds(8).Is32bit = 0
        this.Builds(8).Is64bit = 1
        this.Builds(8).ExcludeInPopup = 0

        this.Builds(9).id = AfxGuidText(AfxGuid())
        this.Builds(9).wszDescription = "Win64 Console (Debug)"
        this.Builds(9).wszOptions = "-g -exx -s console -gen gas64"
        this.Builds(9).Is32bit = 0
        this.Builds(9).Is64bit = 1
        this.Builds(9).ExcludeInPopup = 1

        this.Builds(10).id = AfxGuidText(AfxGuid())
        this.Builds(10).wszDescription = "Win64 Windows DLL"
        this.Builds(10).wszOptions = "-s gui -dll -export -gen gcc"
        this.Builds(10).Is32bit = 0
        this.Builds(10).Is64bit = 1
        this.Builds(10).ExcludeInPopup = 1

        this.Builds(11).id = AfxGuidText(AfxGuid())
        this.Builds(11).wszDescription = "Win64 Static Library"
        this.Builds(11).wszOptions = "-lib"
        this.Builds(11).Is32bit = 0
        this.Builds(11).Is64bit = 1
        this.Builds(11).ExcludeInPopup = 1

    end if

    ' Attempt to fill in any missing compiler paths and help files
    dim as DWSTRING wszText
    if len(this.FBWINCompiler32) = 0 then
        wszText = AfxGetExePathName & "toolchains\" & gwszDefaultToolchain & "\fbc32.exe"      
        if AfxFileExists(wszText) then this.FBWINCompiler32 = wszText
    end if
    if len(this.FBWINCompiler64) = 0 then
        wszText = AfxGetExePathName & "toolchains\" & gwszDefaultToolchain & "\fbc64.exe"      
        if AfxFileExists(wszText) then this.FBWINCompiler64 = wszText
    end if
    if len(this.CompilerHelpfile) = 0 then
        wszText = AfxGetExePathName & "settings\help\freebasic_manual.chm"
        if AfxFileExists(wszText) then this.CompilerHelpfile = wszText
    end if

    ' do some checks to see if the editor font exists. It is possible that the
    ' user could be accessing the editor from a different computer of using it under
    ' Wine where the previously selected font no longer exists.
    if doFontSanityCheck() = false then
'      print "Defined font was not found. Now using this font: ", gConfig.EditorFontname
    end if

    ' Save the last write time so that it can be checked in the message loop
    ' in order to detect external changes to it.
    this.DateFileTime = AfxGetFileLastWriteTime( this.ConfigFilename )

    gApp.PreventConfigLoad = false

    function = 0
end function


' ========================================================================================
' Save current session (open files) to a diskfile
' ========================================================================================
function clsConfig.SaveSessionFile( byref wszSessionFile as wstring ) as boolean    
    ' If the incoming file is a project then 
    if gApp.IsProjectActive then
        this.ProjectSaveToFile()
        return true
    end if

    dim as DWSTRING wszText 
    dim as long nCount 

    dim pDoc as clsDocument ptr

    dim pStream as CTextStream  ' (utf16)
    if pStream.Create( wszSessionFile, true, true ) <> S_OK then return false
    
    pStream.WriteLine "' SESSION FILE"
    
    ' Save all of the loaded tabs
    pStream.WriteLine "BuildConfig=" & this.CompilerBuild
    ' Not GetCurSel: the loop below skips tabs, so the file's index space is denser than the
    ' tab bar's. See clsTopTabCtl.SaveActiveTabIndex.
    pStream.WriteLine "ActiveTab="   & gTTabCtl.SaveActiveTabIndex()
    pStream.WriteLine "CommandLine=" & gApp.wszCommandLine

    nCount = gTTabCtl.GetItemCount()

    for i as long = 0 To nCount - 1
        pDoc = gTTabCtl.GetpDoc(i)
        if pDoc = 0 then continue for
        
        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then 
            wszText = ProcessToCurdriveApp(pDoc->DiskFilename)
            pStream.WriteLine "File=" & wszText
            pStream.WriteLine "FileType=" & pDoc->ProjectFileType
            pStream.WriteLine "TabIndex=" & -1     ' use -1 rather than true
            pStream.WriteLine "Bookmarks=" & pDoc->GetBookmarks()
            pStream.WriteLine "BreakPoints=" & pDoc->GetBreakPoints()
            pStream.WriteLine "FoldPoints=" & pDoc->GetFoldPoints()
            pStream.WriteLine "FirstLine=" & SendMessage( pDoc->hWindow(0), SCI_GETFIRSTVISIBLELINE, 0, 0) 
            pStream.WriteLine "Position=" & SendMessage( pDoc->hWindow(0), SCI_GETCURRENTPOS, 0, 0)  
            pStream.WriteLine "FirstLine1=" & SendMessage( pDoc->hWindow(1), SCI_GETFIRSTVISIBLELINE, 0, 0) 
            pStream.WriteLine "Position1=" & SendMessage( pDoc->hWindow(1), SCI_GETCURRENTPOS, 0, 0)  
            pStream.WriteLine "SplitMode=" & wstr(pDoc->EditorSplitMode)
            pStream.WriteLine "SplitPositionX=" & pDoc->SplitX
            pStream.WriteLine "SplitPositionY=" & pDoc->SplitY
            pStream.WriteLine "FocusEdit=" & iif(pDoc->hWndActiveScintilla = pDoc->hWindow(0), 0, 1)
            pStream.WriteLine "FileEnd=[-]" 
        end if
    next
            
    pStream.Close

    this.wszLastActiveSession = wszSessionFile

    function = true   ' successful save
end function


' ========================================================================================
' Close a  previously loaded session file
' ========================================================================================
function clsConfig.CloseSessionFile( byref wszSessionFile as wstring ) as boolean    
    if this.SaveSessionFile( wszSessionFile) = false then exit function
    if OnCommand_FileClose(HWND_FRMMAIN, EFC_CLOSEALL) = false then exit function
    gApp.IsSessionActive = false
    gApp.SessionFilename = ""
    function = true  ' successful close
end function


' ========================================================================================
' Load previously saved session file from a diskfile
' ========================================================================================
function clsConfig.LoadSessionFile( byref wszSessionFile as wstring ) as boolean    

    ' If the previously saved session file was actually a project file
    ' then open the project.
    if lcase(AfxGetFileExt(wszSessionFile)) = APPEXTENSION then
        if AfxFileExists(wszSessionFile) then 
            frmMain_OpenProjectSafely( HWND_FRMMAIN, wszSessionFile ) 
            return true
        end if
        return false
    end if    

    ' if a project is open then close any files that may be open in the editor. No need to
    ' do this if a new project is being created/opened because that function has already
    ' performed this operation. Doing so again will cause the Recent Files/Projects panel
    ' to flash on the screen.
    if gApp.IsProjectActive then 
        if OnCommand_ProjectClose(HWND_FRMMAIN) = false then exit function
    else   
        if OnCommand_FileClose(HWND_FRMMAIN, EFC_CLOSEALL) = false then exit function
    end if
    ' Clear any previous info from the Output windows
    frmOutput_ResetAllControls()
    
    ' Clear the navigation history. Its entries are filenames rather than document pointers
    ' now, so they can no longer dangle -- but they describe the workspace being left.
    NavHistory_Clear()

    dim pDoc as clsDocument ptr
    dim as DWSTRING wst, wKey, wData, wszFilename
        
    dim as string sBookmarks
    dim as string sBreakPoints
    dim as string sFoldPoints
    dim as long nData, i, iTab, nFocusEdit
    dim as long nFileIndex = -1
    dim as long nFirstLine, nPosition, nFirstLine1, nPosition1
    dim as long nSplitMode, nSplitPositionX, nSplitPositionY

    ' Info saved for each File and used to create the file once Fileend is found
    dim as long nTabIndex
    dim as long nActiveTab = -1
    dim as DWSTRING wszFileType
    dim as boolean bLoadInTab
    
    dim pStream as CTextStream  ' (utf16)
    if AfxFileExists(wszSessionFile) = 0 then exit function
    if pStream.OpenUnicode( wszSessionFile ) <> S_OK then return false

    gApp.SessionFilename = wszSessionFile
    gApp.SessionName = AfxStrPathname( "NAMEX", gApp.SessionFilename )

    dim as HCURSOR hCurSave = GetCursor()
    SetCursor( LoadCursor(0, IDC_WAIT) )
    
    do until pStream.EOS
        wst = pStream.ReadLine
        
        if len(wst) = 0 then continue do
        if left(wst, 1) = "'" then continue do
        if left(wst, 1) = "[" then continue do
        
        i = instr(wst, "=")
        if i = 0 then continue do
        
        wKey  = left(wst, i-1)
        wData = mid(wst, i+1)
        
        select case ucase(wData)
            case "TRUE":   nData = true
            case "FALSE":  nData = false
            case Else:     nData = val(wData)
        end select

        wData = ProcessFromCurdriveApp(wData)

        select case wKey
            case "BuildConfig":         this.CompilerBuild = wData
            case "CommandLine":         gApp.wszCommandLine = wData
            case "ActiveTab":           nActiveTab = nData
            
            case "File":                wszFilename = wData
            case "FileType":            wszFileType = wData
            
            case "TabIndex":            bLoadInTab  = nData
            case "Bookmarks":           sBookmarks  = str(wData)
            case "BreakPoints":         sBreakPoints = str(wData)
            case "FoldPoints":          sFoldPoints = str(wData)
            case "FirstLine":           nFirstLine  = nData
            case "Position":            nPosition   = nData
            case "FirstLine1":          nFirstLine1 = nData
            case "Position1":           nPosition1  = nData
            case "SplitMode":           nSplitMode  = nData
            case "SplitPositionX":      nSplitPositionX = nData
            case "SplitPositionY":      nSplitPositionY = nData
            case "FocusEdit":           nFocusEdit  = nData
            
            case "FileEnd":
            if AfxFileExists(wszFilename) then
                pDoc = gApp.CreateEmptyDocument()
                pDoc->LoadDiskFile( wszFilename )
                pDoc->AssignTextBuffer()
                pDoc->ProjectFileType = wszFileType
                if pDoc->ProjectFileType = FILETYPE_UNDEFINED then
                    pDoc->ProjectFileType = FILETYPE_NORMAL
                end if    
                iTab = gTTabCtl.AddTab( pDoc )  ' Add the new document to the top tabcontrol
                
                ' Set the saved positions
                pDoc->EditorSplitMode = nSplitMode
                pDoc->SplitY = nSplitPositionY
                pDoc->SplitX = nSplitPositionX
                SciExec( pDoc->hWindow(0), SCI_SETFIRSTVISIBLELINE, nFirstLine, 0) 
                SciExec( pDoc->hWindow(0), SCI_GOTOPOS, nPosition, 0) 
                SciExec( pDoc->hWindow(1), SCI_SETFIRSTVISIBLELINE, nFirstLine1, 0) 
                SciExec( pDoc->hWindow(1), SCI_GOTOPOS, nPosition1, 0) 
                pDoc->hWndActiveScintilla = pDoc->hWindow(nFocusEdit)
                pDoc->SetBookmarks(sBookmarks)
                pDoc->SetBreakPoints(sBreakPoints)
                pDoc->SetFoldPoints(sFoldPoints)
            end if
        
        end select
    
    loop                        
    pStream.Close
                        
    ' Do a sanity check on the active tab to ensure that it is in bounds thereby
    ' not throwing future array out of bounds gpf's.
    if gTTabCtl.IsValidTab(nActiveTab) = false then nActiveTab = 0


    ' Load all of the filenames into the Explorer listbox. Configure the node
    ' array to allow all types of files to be shown in the listbox
    for i as long = lbound(this.Cat) to ubound(this.Cat)
        this.Cat(i).bShow = true
    next
    LoadExplorerFiles()
    
    ' Load all of the bookmarks into the Bookmarks listbox. 
    LoadBookmarksFiles()

    ' Load all of the functions into the Function List listbox. 
    LoadFunctionsFiles()

    ' Display the active editing window
    if gTTabCtl.IsValidTab(nActiveTab) then
        gTTabCtl.SetCurSel( nActiveTab )
        gTTabCtl.DisplayScintilla( nActiveTab, true )
    
        ' Highlight the selected tab file in the Explorer listbox
        dim as clsDocument ptr pDoc = gTTabCtl.GetpDoc(nActiveTab)
        frmExplorer_SelectItemData( pDoc )
    end if
    
    frmMain_SetStatusbar
    SetCursor( hCurSave )

    gApp.IsSessionActive = true

    this.wszLastActiveSession = wszSessionFile
    
    frmMain_PositionWindows
    frmMain_SetFocusToCurrentCodeWindow
    
    UpdateMRUSessionList(wszSessionFile)

    function = true   ' successful open
end function


' ========================================================================================
' Save the DEFAULT session (open files) to a diskfile
' ========================================================================================
function clsConfig.SaveDefaultSessionFile() as boolean    
    return this.SaveSessionFile( this.DefaultSessionFilename )
end function


' ========================================================================================
' Load the DEFAULT session (open files) to a diskfile
' ========================================================================================
function clsConfig.LoadDefaultSessionFile() as boolean    
    return this.LoadSessionFile( this.DefaultSessionFilename )
end function


' ========================================================================================
' Save all options for the Project to a diskfile
' ========================================================================================
function clsConfig.ProjectSaveToFile() as boolean    

    dim as DWSTRING cwzRelative, wszText 
    dim as long nCount 
    dim as long i

    dim pDoc as clsDocument ptr

    dim pStream as CTextStream  ' (utf16)
    if pStream.Create(gApp.ProjectFilename, true, true) <> S_OK then return false
    
    pStream.WriteLine "' PROJECT FILE"
    pStream.WriteLine "ProjectBuild="       & gApp.ProjectBuild
    pStream.WriteLine "ProjectOther32="     & gApp.ProjectOther32
    pStream.WriteLine "ProjectOther64="     & gApp.ProjectOther64
    pStream.WriteLine "ProjectCommandLine=" & gApp.ProjectCommandLine

    ' Save all of the loaded tabs first. Not GetCurSel -- see clsTopTabCtl.SaveActiveTabIndex.
    pStream.WriteLine "ActiveTab=" & gTTabCtl.SaveActiveTabIndex()

    nCount = gTTabCtl.GetItemCount

    for i = 0 To nCount - 1
        if gTTabCtl.IsValidTab(i) = false then continue for
        pDoc = gTTabCtl.GetpDoc(i)
        if pDoc = 0 then continue for
        
        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then 
            pDoc->SetDocInfo()   
            wszText = pDoc->DiskFilename
            cwzRelative = AfxPathRelativePathTo( gApp.ProjectFilename, FILE_ATTRIBUTE_NORMAL, wszText, FILE_ATTRIBUTE_NORMAL)
            if len(cwzRelative) then
                if AfxPathIsRelative(cwzRelative) then wszText = cwzRelative
            end if
            wszText = ProcessToCurdriveProject(wszText)
            pStream.WriteLine "File=" & wszText
            pStream.WriteLine "FileType=" & pDoc->ProjectFileType
            pStream.WriteLine "TabIndex=" & "-1"     ' use -1 rather than true
            pStream.WriteLine "Bookmarks=" & pDoc->docData.wszBookmarks
            pStream.WriteLine "BreakPoints=" & pDoc->docData.wszBreakPoints
            pStream.WriteLine "FoldPoints=" & pDoc->docData.wszFoldPoints
            pStream.WriteLine "FirstLine=" & pDoc->docData.nFirstLine 
            pStream.WriteLine "Position=" & pDoc->docData.nPosition  
            pStream.WriteLine "FirstLine1=" & pDoc->docData.nFirstLine1  
            pStream.WriteLine "Position1=" & pDoc->docData.nPosition1  
            pStream.WriteLine "SplitMode=" & pDoc->docData.nSplitMode
            pStream.WriteLine "SplitPositionX=" & pDoc->docData.nSplitPositionX
            pStream.WriteLine "SplitPositionY=" & pDoc->docData.nSplitPositionY
            pStream.WriteLine "FocusEdit=" & pDoc->docData.nFocusEdit
            pStream.WriteLine "FileEnd=[-]" 
        end if
    next
            
    ' Save all other non-displayed documents for the project

    pDoc = gApp.pDocList
    do until pDoc = 0
        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then 
            if gTTabCtl.GetTabIndexByDocumentPtr(pDoc) = -1 then
                wszText = pDoc->DiskFilename
                cwzRelative = AfxPathRelativePathTo( gApp.ProjectFilename, FILE_ATTRIBUTE_NORMAL, wszText, FILE_ATTRIBUTE_NORMAL)
                ' The len() test is NOT optional. AfxPathRelativePathTo returns "" when
                ' PathRelativePathToW fails -- a file on another volume, most obviously --
                ' and AfxPathIsRelative("") is TRUE. Without the guard wszText becomes ""
                ' and the document is written as "File=", i.e. SILENTLY DROPPED from the
                ' project. The tabbed loop above has always had this guard; this one did not.
                if len(cwzRelative) then
                    if AfxPathIsRelative(cwzRelative) then wszText = cwzRelative
                end if
                wszText = ProcessToCurdriveProject(wszText)
                pStream.WriteLine "File=" & wszText
                pStream.WriteLine "FileType=" & pDoc->ProjectFileType
                pStream.WriteLine "TabIndex=" & "0"
                pStream.WriteLine "Bookmarks=" & pDoc->docData.wszBookmarks
                pStream.WriteLine "BreakPoints=" & pDoc->docData.wszBreakPoints
                pStream.WriteLine "FoldPoints=" & pDoc->docData.wszFoldPoints
                pStream.WriteLine "FirstLine=" & pDoc->docData.nFirstLine 
                pStream.WriteLine "Position=" & pDoc->docData.nPosition  
                pStream.WriteLine "FirstLine1=" & pDoc->docData.nFirstLine1  
                pStream.WriteLine "Position1=" & pDoc->docData.nPosition1  
                pStream.WriteLine "SplitMode=" & pDoc->docData.nSplitMode
                pStream.WriteLine "SplitPositionX=" & pDoc->docData.nSplitPositionX
                pStream.WriteLine "SplitPositionY=" & pDoc->docData.nSplitPositionY
                pStream.WriteLine "FocusEdit=" & pDoc->docData.nFocusEdit
                pStream.WriteLine "FileEnd=[-]"
            end If
        end if
        pDoc = pDoc->pDocnext
    loop

    ' if project is active then save NOTES to config file.
    if gApp.IsProjectActive then
        dim wszText as DWSTRING = wstr("NOTES-START") + vbcrlf + _
                                gApp.ProjectNotes + vbcrlf + _
                                wstr("NOTES-END") + vbcrlf
        pStream.WriteLine ""
        pStream.WriteLine "[Notes]"
        pStream.WriteLine wszText                       
    end IF

    pStream.Close

    this.wszLastActiveSession = gApp.ProjectFilename

    function = true   ' successful save
end function


' ========================================================================================
' Load all options for the Project from a diskfile
' ========================================================================================
function clsConfig.ProjectLoadFromFile( byval wszFile as DWSTRING ) as boolean    

    dim pDoc as clsDocument ptr
    dim as DWSTRING wst, wKey, wData, wszFilename
        
    dim sBookmarks as string
    dim sBreakPoints as string
    dim sFoldPoints as string
    dim as long nData, i, iTab, nFocusEdit
    dim as long nFileIndex = -1
    dim as long nFirstLine, nPosition, nFirstLine1, nPosition1, nSplitPosition
    dim as long nSplitMode, nSplitPositionX, nSplitPositionY

    ' Info saved for each File and used to create the file once Fileend is found
    dim as long nTabIndex
    dim as long nActiveTab = -1
    
    dim as DWSTRING wszFileType
    dim as boolean bLoadInTab
    dim as boolean bReadingNote
    
    if AfxFileExists(wszFile) = 0 then exit function

    dim as HCURSOR hCurSave = GetCursor()
    SetCursor( LoadCursor(0, IDC_WAIT) )

    gApp.ProjectFilename = wszFile
    gApp.ProjectName = AfxStrPathname( "NAMEX", gApp.ProjectFilename )
    gApp.ProjectNotes = ""
    
    dim pStream as CTextStream  ' (utf16)
    if pStream.OpenUnicode(gApp.ProjectFilename) <> S_OK then return false

    ' Variable length array to hold sequence of all project files
    dim docData(any) as PROJECT_FILELOAD_DATA
    
    gApp.IsProjectLoading = true 
            
    do until pStream.EOS
        wst = pStream.ReadLine
        
        if len(wst) = 0 then continue do
        if left(wst, 1) = "'" then continue do
        if left(wst, 1) = "[" then continue do
        
        if left(wst, 11) = "NOTES-START" then  
            bReadingNote = true
            continue do
        end if   
        if left(wst, 9) = "NOTES-END" then  
            bReadingNote = false
            continue do
        end if
        if bReadingNote then
            gApp.ProjectNotes = gApp.ProjectNotes + wst + vbcrlf
            continue do
        end IF

        i = instr(wst, "=")
        if i = 0 then continue do
        
        wKey  = left(wst, i-1)
        wData = mid(wst, i+1)
        
        select case ucase(wData)
            case "TRUE":   nData = true
            case "FALSE":  nData = false
            case Else:     nData = val(wData)
        end select

        wData = ProcessFromCurdriveProject(wData)

        select case wKey
            case "ProjectBuild":        gApp.ProjectBuild       = wData
            case "ProjectOther32":      gApp.ProjectOther32     = wData
            case "ProjectOther64":      gApp.ProjectOther64     = wData
            case "ProjectCommandLine":  gApp.ProjectCommandLine = wData
            case "ActiveTab":           nActiveTab              = nData
            case "File":                wszFilename             = wData
            case "FileType":            wszFileType             = wData
            case "Bookmarks":           sBookmarks              = str(wData)
            case "BreakPoints":         sBreakPoints            = str(wData)
            case "FoldPoints":          sFoldPoints             = str(wData)
            case "TabIndex":            bLoadInTab              = nData
            case "FirstLine":           nFirstLine              = nData
            case "Position":            nPosition               = nData
            case "FirstLine1":          nFirstLine1             = nData
            case "Position1":           nPosition1              = nData
            case "SplitMode":           nSplitMode              = nData
            case "SplitPositionX":      nSplitPositionX         = nData
            case "SplitPositionY":      nSplitPositionY         = nData
            case "FocusEdit":           nFocusEdit              = nData
            
            case "FileEnd":
            ' if this is a relative filename then convert it back.
            if AfxPathIsRelative(wszFilename) then 
                wszFilename = AfxPathCombine( AfxStrPathName("PATH", gApp.ProjectFilename), wszFilename)
            end if
            if AfxFileExists(wszFilename) then

                nFileIndex = nFileIndex + 1
                if nFileIndex > ubound(docData) then
                    redim preserve docData( ubound(docData) + 100 ) 
                end If

                with docData(nFileIndex)
                    .bIsValidData    = true
                    .wszFilename     = wszFilename
                    .wszFileType     = wszFileType
                    .bLoadInTab      = bLoadInTab
                    .wszBookmarks    = sBookmarks
                    .wszBreakPoints  = sBreakPoints
                    .wszFoldPoints   = sFoldPoints
                    .nFirstLine      = nFirstLine
                    .nPosition       = nPosition
                    .nFirstLine1     = nFirstLine1
                    .nPosition1      = nPosition1
                    .nSplitMode      = nSplitMode
                    .nSplitPositionX = nSplitPositionX
                    .nSplitPositionY = nSplitPositionY
                    .nFocusEdit      = nFocusEdit
                end with

                nFirstLine  = 0: nPosition  = 0
                nFirstLine1 = 0: nPosition1 = 0
                nFocusEdit  = 0: nSplitPositionX = 0: nSplitPositionY = 0: nSplitMode  = 0:
                sBookmarks  = "": sBreakPoints = "": sFoldPoints = ""
                bLoadInTab    = false
            end if
            
        end select
    
    loop                        
    pStream.Close
    

    ' Load/Display all of the project files
    for i as long = lbound(docData) to ubound(docData)
    
        if AfxFileExists( docData(i).wszFilename ) = false then continue for
        
        pDoc = gApp.CreateEmptyDocument()
        pDoc->LoadDiskFile( docData(i).wszFilename )
        pDoc->ProjectFiletype = docData(i).wszFileType
        pDoc->docData = docData(i)
        if docData(i).bLoadInTab then
            pDoc->AssignTextBuffer()
            iTab = gTTabCtl.AddTab( pDoc )  ' Add the new document to the top tabcontrol
        end if
    next

    gApp.IsProjectLoading = false
    gApp.IsProjectActive = true
    gApp.hIconPanel = 0
    gApp.wszLastOpenFolder = ""
    
    ' Load all of the filenames into the Explorer listbox. Configure the node
    ' array to allow all types of files to be shown in the listbox
    for i as long = lbound(this.Cat) to ubound(this.Cat)
        this.Cat(i).bShow = true
    next

    LoadExplorerFiles()

    ' Load all of the bookmarks into the Bookmarks listbox. 
    LoadBookmarksFiles()

    ' Load all of the functions into the Function List listbox. 
    LoadFunctionsFiles()

    ' Display the active editing window
    if gTTabCtl.IsValidTab(nActiveTab) = false then 
        ' active tab as stored in the config file is not valid. Could be that the
        ' filename no longer exists of trouble loading that particular project file.
        ' Try to use an alternate tab number
        nActiveTab = gTTabCtl.GetItemCount - 1
    end if
    gTTabCtl.SetFocusTab(nActiveTab)      

    ' if no active build configuration then assign the currently active selection
    if gApp.ProjectBuild = "" then 
        gApp.ProjectBuild = frmBuildConfig_GetSelectedBuildGUID()
    end if   

    frmMain_SetStatusbar()
    SetCursor( hCurSave )

    ' Show the correct notes and TODO data for this project
    frmOutput_ShowNotes()
    frmOutput_UpdateToDoListview()

    this.wszLastActiveSession = gApp.ProjectFilename

    ' Clear the navigation history. Its entries are filenames rather than document pointers
    ' now, so they can no longer dangle -- but they describe the workspace being left.
    NavHistory_Clear()

    frmMain_PositionWindows()
    frmMain_SetFocusToCurrentCodeWindow()

    function = true   ' successful open
end function


' ========================================================================================
' Reload the config file should it have been changed by an external program.
' ========================================================================================
function clsConfig.ReloadConfigFileTest() as boolean    

    if gApp.IsShutdown = true then return false
    if gApp.PreventConfigLoad = true then return false
    
    dim as FILETIME ft
    
    ' Compare the disk file date time to the value currently stored in static variable.
    ft = AfxGetFileLastWriteTime(this.ConfigFilename)
    if AfxFileTimeToVariantTime(ft) <> AfxFileTimeToVariantTime(this.DateFileTime) then
        this.LoadConfigFile(true)  ' this function also updates the _DateFileTime value
        ' Apply the newly saved options to all open Scintilla windows
        dim pDoc as clsDocument ptr = gApp.pDocList
        do until pDoc = 0
            pDoc->ApplyProperties
            pDoc = pDoc->pDocnext
        loop   
    end if
    
    function = 0
end function
