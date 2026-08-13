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

#include once "app/clsConfig.bi"
'' frmDebug.bi, NAMED HERE BECAUSE THIS FILE USES IT. It arrived transitively
'' until clsDocument.bi stopped including a header it never used -- and the
'' moment that went, this file stopped compiling, which is what a hidden
'' transitive dependency looks like when it is finally exposed.
''
'' clsConfig.inc is shell-side and legitimately needs the debugger's layout
'' constants (FRMDEBUG_DEFPCT*, FRMDEBUG_PCTSCALE) and frmDebug_CaptureState.
'' Its HEADER, app/clsConfig.bi, needs none of them -- that is the split.
#include once "frmDebug.bi"
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

' ReloadConfigFileTest has to rebuild the tool accelerators now that a hot reload can
' actually change gConfig.Tools. Safe to include this early -- the header states its own
' contract that nothing in it may name a Ps* type, for exactly this reason.
#include once "frmUserTools.bi"

''
''  THE CONSTRUCTOR IS NOT HERE. It is in app/clsConfig.inc, and that is deliberate.
''
''  app/clsConfig.bi carries `dim shared gConfig as clsConfig`, so including the header
''  INSTANTIATES the object -- which means every consumer of the app layer has to be able to
''  LINK a constructor. With it in this file, that meant needing the shell: AfxGuid,
''  HWND_FRMMAIN and the forty-odd frm* headers above.
''
''  Nothing else moved. The ~90 methods below read and write real files, rebuild accelerator
''  tables and talk to frmMain, and none of them is reached by merely declaring the object.
''
''  Found by phase 7c's shell binary, the first thing ever to link the app layer, which
''  failed on exactly one symbol: CLSCONFIG::CLSCONFIG(). See app/clsConfig.inc.
''

''
''
function clsConfig.LoadKeywords() as long
    if PsFileExists(this.FBKeywordsFilename) = false then
        ' The FB keywords file does not exist. Try copying the "default" keywords over
        ' to the main file that the user can modify.
        if PsFileExists(this.FBKeywordsDefaultFilename) then
            if PsFileCopy( this.FBKeywordsDefaultFilename, this.FBKeywordsFilename, true ) = false then
                exit function
            end if
        end if
    end if
    if PsFileExists(this.FBKeywordsFilename) = false then exit function

    this.FBKeywords     = TextFile_ReadAll( this.FBKeywordsFilename ).Utf8
    this.WinApiKeywords = TextFile_ReadAll( this.WinApiKeywordsFilename ).Utf8
    this.ExtraKeywords  = TextFile_ReadAll( this.ExtraKeywordsFilename ).Utf8

    function = 0
end function

''
''
function clsConfig.SaveKeywords() as long
    ' WriteLine appended a CRLF, so these keep it -- the files are compared by hand and a
    ' vanished trailing newline is the kind of diff that looks like data loss.
    TextFile_WriteAnsi( this.FBKeywordsFilename,     this.FBKeywords     + vbcrlf )
    TextFile_WriteAnsi( this.WinApiKeywordsFilename, this.WinApiKeywords + vbcrlf )
    TextFile_WriteAnsi( this.ExtraKeywordsFilename,  this.ExtraKeywords  + vbcrlf )
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

    redim as DWSTRING lines(any)
    if TextFile_ReadLines( this.FBCodetipsFilename, lines() ) = false then return true  ' error

    for nLine as long = lbound(lines) to ubound(lines)
        wst = lines(nLine)
        wst = PsTrim(wst)
        if PsLen(wst) = 0 then continue for

        i = PsInStr(wst, "=")
        if i then
            redim preserve gFBIntrinsics( 0 to nIntrinsic )
            gFBIntrinsics(nIntrinsic).wszName    = PsLeft(wst, i-1)
            gFBIntrinsics(nIntrinsic).wszNameUp  = PsUCase( PsLeft(wst, i-1) )
            gFBIntrinsics(nIntrinsic).wszCalltip = PsMid(wst, i+1)
            nIntrinsic += 1
        end if
    next

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
    wszFontName = PsUCase(wszFontName)
    for i as long = lbound(gFontnames) to ubound(gFontNames)
        if wszFontName = PsUCase(gFontNames(i)) then return true
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
' Same shape as LoadConfigFile, and for the same reason: the body has an early "return
' true" when the file cannot be created, which used to strand gApp.PreventConfigLoad -- an
' unwritable settings.ini (read-only volume, a locked file, no disk space) killed external
' config-change detection for the session just as surely as an unreadable one did.
function clsConfig.SaveConfigFile() as long
    gApp.PreventConfigLoad = true
    dim as long nResult = this.SaveConfigFileBody()
    gApp.PreventConfigLoad = false
    return nResult
end function


' The body. Non-zero on error, 0 on success.
function clsConfig.SaveConfigFileBody() as long

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

    ' The Help Center HAS NO GEOMETRY TO CAPTURE any more -- it opens in the user's
    ' browser, which owns its own window. The HelpStartup* fields below are left in place
    ' and inert rather than deleted: removing them would change the settings file's shape
    ' for every existing install to retire four numbers that cost nothing.

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

    ' ONE BUFFER, ONE WRITE. This was 166 WriteLines straight at a CTextStream, so an
    ' error partway through left a truncated settings.ini on disk that looked saved.
    ' TextFile_WriteUtf16 emits the same bytes -- UTF-16LE with a BOM -- in a single call.
    dim as DWSTRING wszOut

    wszOut &= "' SETTINGS FILE" & vbcrlf
    wszOut &= "" & vbcrlf
    wszOut &= "SettingsVersion="       & APPVERSION & vbcrlf
    wszOut &= "" & vbcrlf
    wszOut &= "[Editor]" & vbcrlf
    wszOut &= "AskExit="               & this.AskExit & vbcrlf
    wszOut &= "AutoSaveFiles="         & this.AutoSaveFiles & vbcrlf
    wszOut &= "AutoSaveInterval="      & this.AutoSaveInterval & vbcrlf
    wszOut &= "RestoreSession="        & this.RestoreSession & vbcrlf
    if this.RestoreSession = 0 then this.wszLastActiveProject = ""
    wszOut &= "LastActiveProjectFile=" & ProcessToCurdriveApp(this.wszLastActiveProject) & vbcrlf
    wszOut &= "CompactMenus="          & this.CompactMenus & vbcrlf
    wszOut &= "MultipleInstances="     & this.MultipleInstances & vbcrlf
    wszOut &= "CompileAutosave="       & this.CompileAutosave & vbcrlf
    wszOut &= "CheckUpdates="          & this.CheckUpdates & vbcrlf
    wszOut &= "SyntaxHighlighting="    & this.SyntaxHighlighting & vbcrlf
    wszOut &= "Codetips="              & this.Codetips & vbcrlf
    wszOut &= "AutoComplete="          & this.AutoComplete & vbcrlf
    wszOut &= "CharacterAutoComplete=" & this.CharacterAutoComplete & vbcrlf
    wszOut &= "RightEdge="             & this.RightEdge & vbcrlf
    wszOut &= "RightEdgePosition="     & this.RightEdgePosition & vbcrlf
    wszOut &= "LeftMargin="            & this.LeftMargin & vbcrlf
    wszOut &= "FoldMargin="            & this.FoldMargin & vbcrlf
    wszOut &= "AutoIndentation="       & this.AutoIndentation & vbcrlf
    wszOut &= "FornextVariable="       & this.FornextVariable & vbcrlf
    wszOut &= "ConfineCaret="          & this.ConfineCaret & vbcrlf
    wszOut &= "LineNumbering="         & this.LineNumbering & vbcrlf
    wszOut &= "HighlightCurrentLine="  & this.HighlightCurrentLine & vbcrlf
    wszOut &= "IndentGuides="          & this.IndentGuides & vbcrlf
    wszOut &= "TabIndentSpaces="       & this.TabIndentSpaces & vbcrlf
    wszOut &= "UnicodeEncoding="       & this.UnicodeEncoding & vbcrlf
    wszOut &= "NewFileEncoding="       & this.NewFileEncoding & vbcrlf
    wszOut &= "KeywordCase="           & this.KeywordCase & vbcrlf
    ' The code formatter's rule set. Every field is `as long` for the reason the AskExit
    ' note above gives -- these are written verbatim, and a real boolean would emit
    ' "true"/"false" where the reader expects 0/1.
    wszOut &= "FmtReindent="           & this.Format.Reindent & vbcrlf
    wszOut &= "FmtIndentCase="         & this.Format.IndentCase & vbcrlf
    wszOut &= "FmtContinuationIndent=" & this.Format.ContinuationIndent & vbcrlf
    wszOut &= "FmtCaseKeywords="       & this.Format.CaseKeywords & vbcrlf
    wszOut &= "FmtCaseDirectives="     & this.Format.CaseDirectives & vbcrlf
    wszOut &= "FmtCaseTypes="          & this.Format.CaseTypes & vbcrlf
    wszOut &= "FmtSpaceOperators="     & this.Format.SpaceAroundOperators & vbcrlf
    wszOut &= "FmtSpaceAfterComma="    & this.Format.SpaceAfterComma & vbcrlf
    wszOut &= "FmtParenSpacing="       & this.Format.ParenSpacing & vbcrlf
    wszOut &= "FmtSpacesBeforeComment=" & this.Format.SpacesBeforeComment & vbcrlf
    wszOut &= "FmtTrimTrailing="       & this.Format.TrimTrailing & vbcrlf
    wszOut &= "FmtMaxBlankLines="      & this.Format.MaxBlankLines & vbcrlf
    wszOut &= "FmtBlankLinesAroundProc=" & this.Format.BlankLinesAroundProc & vbcrlf
    wszOut &= "FmtFormatOnEnter="      & this.Format.FormatOnEnter & vbcrlf
    wszOut &= "FmtFormatOnPaste="      & this.Format.FormatOnPaste & vbcrlf
    wszOut &= "LocalizationFile="      & this.LocalizationFile & vbcrlf
    wszOut &= "TabSize="               & this.TabSize & vbcrlf
    wszOut &= "PositionMiddle="        & this.PositionMiddle & vbcrlf
    wszOut &= "ClickToggleBreakpoint=" & this.ClickToggleBreakpoint & vbcrlf
    wszOut &= "BraceHighlight="        & this.BraceHighlight & vbcrlf
    wszOut &= "OccurrenceHighlight="   & this.OccurrenceHighlight & vbcrlf
    wszOut &= "DetectExternalFileChanges=" & this.DetectExternalFileChanges & vbcrlf
    wszOut &= "StripTrailingWhitespace=" & this.StripTrailingWhitespace & vbcrlf
    wszOut &= "EditorFontname="        & this.EditorFontname & vbcrlf
    wszOut &= "EditorFontsize="        & this.EditorFontsize & vbcrlf
    wszOut &= "EditorFontCharSet="     & this.EditorFontCharSet & vbcrlf
    wszOut &= "FontExtraSpace="        & this.FontExtraSpace & vbcrlf
    wszOut &= "Theme="                 & this.ThemeShortFilename & vbcrlf
    wszOut &= "ExplorerPositionRight=" & this.ExplorerPositionRight & vbcrlf
    wszOut &= "" & vbcrlf
    ' Note: Main window StartUp positions are in scaled screen coordinates.
    wszOut &= "[Startup]" & vbcrlf
    wszOut &= "StartupLeft="              & this.StartupLeft & vbcrlf
    wszOut &= "StartupTop="               & this.StartupTop & vbcrlf
    wszOut &= "StartupRight="             & this.StartupRight & vbcrlf
    wszOut &= "StartupBottom="            & this.StartupBottom & vbcrlf
    wszOut &= "StartupMaximized="         & this.StartupMaximized & vbcrlf
    ' Note: HelpViewer StartUp positions are in scaled screen coordinates.
    wszOut &= "HelpStartupLeft="          & this.HelpStartupLeft & vbcrlf
    wszOut &= "HelpStartupTop="           & this.HelpStartupTop & vbcrlf
    wszOut &= "HelpStartupRight="         & this.HelpStartupRight & vbcrlf
    wszOut &= "HelpStartupBottom="        & this.HelpStartupBottom & vbcrlf
    wszOut &= "HelpStartupMaximized="     & this.HelpStartupMaximized & vbcrlf
    ' Note: SidePanel and OutputPanel positions are in unscaled screen coordinates.
    wszOut &= "ShowPanel="                & this.ShowPanel & vbcrlf
    wszOut &= "ShowPanelWidth="           & this.ShowPanelWidth & vbcrlf
    wszOut &= "ShowOutputPanel="          & this.ShowOutputPanel & vbcrlf
    wszOut &= "ShowOutputPanelHeight="    & this.ShowOutputPanelHeight & vbcrlf
    wszOut &= "ShowOutputPanelMinimized=" & this.ShowOutputPanelMinimized & vbcrlf
    ' Written from this.ShowOutputPanelIndex, which frmMain refreshes from the tab bar
    ' (PsSelectBar_GetCurSel) just before the config is saved.
    wszOut &= "ShowOutputPanelIndex="     & this.ShowOutputPanelIndex & vbcrlf
    wszOut &= "UnusedKindMask="           & this.UnusedKindMask & vbcrlf
    wszOut &= "UnusedSortCol="            & this.UnusedSortCol & vbcrlf
    wszOut &= "UnusedSortDesc="           & this.UnusedSortDesc & vbcrlf
    wszOut &= "UnusedMinimizeOnOpen="     & this.UnusedMinimizeOnOpen & vbcrlf
    ' Note: the floating Output window's positions are in scaled screen coordinates (the
    ' Startup/HelpStartup convention above, NOT the unscaled one the two lines above use).
    wszOut &= "OutputFloating="           & this.OutputFloating & vbcrlf
    wszOut &= "OutputFloatLeft="          & this.OutputFloatLeft & vbcrlf
    wszOut &= "OutputFloatTop="           & this.OutputFloatTop & vbcrlf
    wszOut &= "OutputFloatRight="         & this.OutputFloatRight & vbcrlf
    wszOut &= "OutputFloatBottom="        & this.OutputFloatBottom & vbcrlf
    wszOut &= "OutputFloatMaximized="     & this.OutputFloatMaximized & vbcrlf
    ' Note: the debugger window's positions are in scaled screen coordinates (the
    ' Startup/HelpStartup convention), but its three splitters are a PERCENT x 10000 of the
    ' span each divides -- see clsConfig.bi for why they are not pixels.
    wszOut &= "DebugLeft="                & this.DebugLeft & vbcrlf
    wszOut &= "DebugTop="                 & this.DebugTop & vbcrlf
    wszOut &= "DebugRight="               & this.DebugRight & vbcrlf
    wszOut &= "DebugBottom="              & this.DebugBottom & vbcrlf
    wszOut &= "DebugMaximized="           & this.DebugMaximized & vbcrlf
    wszOut &= "DebugSplitMain="           & this.DebugSplitMain & vbcrlf
    wszOut &= "DebugSplitLeft="           & this.DebugSplitLeft & vbcrlf
    wszOut &= "DebugSplitRight="          & this.DebugSplitRight & vbcrlf
    wszOut &= "" & vbcrlf

    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    this.FBWINCompiler32  = ProcessToCurdriveApp(this.FBWINCompiler32)
    this.FBWINCompiler64  = ProcessToCurdriveApp(this.FBWINCompiler64)

    ' Iterate the optional semi colon separated list of compiler include paths
    ' Do not modify the this.CompilerIncludes directly because that will put the
    ' {CURDRIVE} macro back into the path names causing future compiles to fail
    ' because that macro only gets resolved on LoadConfig.
    dim as DWSTRING allIncludes = PsStrTrimAny(this.CompilerIncludes, ";")
    dim as DWSTRING SaveCompilerIncludes
    dim as DWSTRING includePath
    dim as long numpaths = PsStrParseCount(allIncludes, ";")
    for i as long = 1 to numpaths
        includePath = PsTrim(PsStrParse(allIncludes, i, ";"))
        if PsLen(includePath) then
            includePath = ProcessToCurdriveApp(includePath)
            SaveCompilerIncludes = SaveCompilerIncludes + includePath + ";"
        end if
    next

    wszOut &= "[Compiler]" & vbcrlf
    wszOut &= "FBWINCompiler32="       & this.FBWINCompiler32 & vbcrlf
    wszOut &= "FBWINCompiler64="       & this.FBWINCompiler64 & vbcrlf
    wszOut &= "CompilerBuild="         & this.CompilerBuild & vbcrlf
    wszOut &= "CompilerSwitches="      & this.CompilerSwitches & vbcrlf
    wszOut &= "CompilerIncludes="      & SaveCompilerIncludes & vbcrlf
    wszOut &= "RunViaCommandWindow="   & this.RunViaCommandWindow & vbcrlf
    wszOut &= "DisableCompileBeep="    & this.DisableCompileBeep & vbcrlf

    wszOut &= "" & vbcrlf
    wszOut &= "[Categories]" & vbcrlf
    dim as long nNext = 0
    for i as long = lbound(this.Cat) to ubound(this.Cat)
        if PsLeft(this.Cat(i).wszDescription, 2) <> "%%" then
            if PsTrim(this.Cat(i).wszDescription) <> "" then
                wszOut &= "CATEGORY_" & right("00" & str(nNext), 2) & "="
                wszOut &= this.Cat(i).idFileType & "|-|"
                wszOut &= this.Cat(i).wszDescription & vbcrlf
                nNext = nNext + 1
            end if
        end if
    next

    wszOut &= "" & vbcrlf
    wszOut &= "[UserTools]" & vbcrlf
    for i as long = lbound(this.Tools) to ubound(this.Tools)
        wszOut &= "USERTOOL_" & right("00" & str(i), 2) & "="
        wszOut &= this.Tools(i).wszDescription & "|-|"
        wszOut &= ProcessToCurdriveApp(this.Tools(i).wszCommand) & "|-|"
        wszOut &= this.Tools(i).wszParameters & "|-|"
        wszOut &= this.Tools(i).wszKey & "|-|"
        wszOut &= ProcessToCurdriveApp(this.Tools(i).wszWorkingFolder) & "|-|"
        wszOut &= this.Tools(i).IsCtrl & "|-|"
        wszOut &= this.Tools(i).IsAlt & "|-|"
        wszOut &= this.Tools(i).IsShift & "|-|"
        wszOut &= this.Tools(i).IsPromptRun & "|-|"
        wszOut &= this.Tools(i).IsMinimized & "|-|"
        wszOut &= this.Tools(i).IsWaitFinish & "|-|"
        wszOut &= this.Tools(i).IsDisplayMenu & "|-|"
        wszOut &= wstr(this.Tools(i).Action) & vbcrlf
    next

    wszOut &= "" & vbcrlf
    wszOut &= "[Builds]" & vbcrlf
    for i as long = lbound(this.Builds) to ubound(this.Builds)
        wszOut &= "BUILD_" & right("00" & str(i), 2) & "="
        wszOut &= this.Builds(i).id & "|-|"
        wszOut &= this.Builds(i).wszDescription & "|-|"
        wszOut &= this.Builds(i).wszOptions & "|-|"
        wszOut &= this.Builds(i).IsDefault & "|-|"
        wszOut &= this.Builds(i).Is32bit & "|-|"
        wszOut &= this.Builds(i).Is64bit & "|-|"
        wszOut &= this.Builds(i).ExcludeInPopup & "|-|"
        wszOut &= this.Builds(i).IsCtrl & "|-|"
        wszOut &= this.Builds(i).IsAlt & "|-|"
        wszOut &= this.Builds(i).IsShift & "|-|"
        wszOut &= this.Builds(i).wszKey & vbcrlf
    next


    wszOut &= "" & vbcrlf
    wszOut &= "[MRU]" & vbcrlf
    for i as long = 0 To 9
        this.MRU(i) = ProcessToCurdriveApp(this.MRU(i))
        wszOut &= "MRU_" & right("00" & str(i), 2) & "=" & this.MRU(i) & vbcrlf
    next

    wszOut &= "" & vbcrlf
    wszOut &= "[MRUPROJECTS]" & vbcrlf
    for i as long = 0 To 9
        this.MRUProject(i) = ProcessToCurdriveApp(this.MRUProject(i))
        wszOut &= "MRUPROJECT_" & right("00" & str(i), 2) & "=" & this.MRUProject(i) & vbcrlf
    next

    ' The [Notes] block is no longer WRITTEN. Notes belong to the workspace and are stored
    ' in its own .tiko file; the reader below survives only to migrate an existing
    ' settings.ini's notes into the untitled workspace once, so the section ages out of
    ' settings.ini on the first save after that.

    if TextFile_WriteUtf16( this.ConfigFilename, wszOut ) = false then return true   ' error

    ' Save the last write time so that it can be checked in the message loop
    ' in order to detect external changes to it.
    this.DateFileTime = PsFileWriteTime( this.ConfigFilename )

    ' (The wrapper clears gApp.PreventConfigLoad; it is deliberately not touched here.)
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
    WritePrivateProfileString( wszSectionName.Wz(), null, null, this.ConfigFilename.Wz() )

    ' Write the new values.
    for i as long = 0 To 9
        this.MRU(i) = ProcessToCurdriveApp(this.MRU(i))
        wszKeyName = "MRU_" & right("00" & str(i), 2)
        wszValue   = this.MRU(i)
        WritePrivateProfileString(wszSectionName.Wz(), wszKeyName.Wz(), wszValue.Wz(), this.ConfigFilename.Wz())
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
    WritePrivateProfileString( wszSectionName.Wz(), null, null, this.ConfigFilename.Wz() )

    ' Write the new values.
    for i as long = 0 To 9
        this.MRUProject(i) = ProcessToCurdriveApp(this.MRUProject(i))
        wszKeyName = "MRUPROJECT_" & right("00" & str(i), 2)
        wszValue   = this.MRUProject(i)
        WritePrivateProfileString( wszSectionName.Wz(), wszKeyName.Wz(), wszValue.Wz(), this.ConfigFilename.Wz() )
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
        this.Cat(CATINDEX_MAIN).wszDescription = L(173, "Main")

        this.Cat(CATINDEX_RESOURCE).idFileType = FILETYPE_RESOURCE
        this.Cat(CATINDEX_RESOURCE).wszDescription = L(174, "Resource")

        this.Cat(CATINDEX_HEADER).idFileType = FILETYPE_HEADER
        this.Cat(CATINDEX_HEADER).wszDescription = L(140, "Header")

        this.Cat(CATINDEX_MODULE).idFileType = FILETYPE_MODULE
        this.Cat(CATINDEX_MODULE).wszDescription = L(172, "Module")

        this.Cat(CATINDEX_NORMAL).idFileType = FILETYPE_NORMAL
        this.Cat(CATINDEX_NORMAL).wszDescription = L(171, "Normal")
    end if

    function = 0
end function


''
''  LOAD CONFIGURATION FROM DISK FILE
''
' A HOT RELOAD MUST NOT RESURRECT THE RESTORE SLOT. wszLastActiveProject is live workspace
' state, not user-editable configuration: it is owned by ProjectSaveToFile and cleared by
' Workspace_EstablishUntitled. ReloadConfigFileTest fires whenever settings.ini's timestamp
' moves, and re-reading LastActiveProjectFile from disk put the CLOSED project's path back
' into memory -- so Project > Close followed by exit wrote that path out again and the
' project reopened on the next launch, exactly as if it had never been closed.
' The geometry fields are already exempted from a hot reload for the same class of reason.
' The flag is set and cleared HERE, around a call, so no exit path inside the body can
' leak it. It used to be set at the top of the body and cleared at the bottom, and the one
' early "return true" between them -- an unopenable settings.ini -- left it latched. Its
' only reader is ReloadConfigFileTest, which short-circuits on it, so a single transient
' failure (an antivirus scan, a second instance mid-write, a network hiccup) permanently
' disabled external config-change detection for the rest of the session, silently.
function clsConfig.LoadConfigFile( byval isHotReload as boolean = false ) as long
    gApp.PreventConfigLoad = true
    dim as long nResult = this.LoadConfigFileBody( isHotReload )
    gApp.PreventConfigLoad = false
    return nResult
end function


' The body. Returns non-zero on error, 0 on success -- an inverted convention it has always
' had; no caller currently checks it.
function clsConfig.LoadConfigFileBody( byval isHotReload as boolean ) as long
    dim as DWSTRING wszLiveLastProject = this.wszLastActiveProject

    dim as DWSTRING wst, wKey, wData
    dim nData          as long
    dim i              as long
    dim bReadingNote   as boolean

    ' Builds AND Tools are rebuilt from the file on every load, including a hot reload.
    '
    ' Tools was NOT erased here before, and because its assignments were nested inside
    ' "if i > ubound(this.Tools)", the second and every later load skipped every
    ' USERTOOL_ line: user tools silently kept their pre-edit values while the file on
    ' disk said otherwise. Erasing is what makes them mirror the file.
    '
    ' CATEGORIES ARE DELIBERATELY NOT ERASED, but not for the reason the old comment
    ' here gave. It claimed "the structure contains references to existing nodes";
    ' TYPE_CATEGORIES is two DWSTRINGs and holds no references at all -- that comment is
    ' stale. The real reason is SetCategoryDefaults: it only fills the array when it is
    ' EMPTY and runs once at startup, so erasing here would leave a settings.ini with no
    ' CATEGORY_ lines with no categories at all and nothing to restore them. The values
    ' are refreshed in place instead, which a hot reload could not do before.
    erase this.Builds
    erase this.Tools

    ' A MISSING config file needs no branch of its own any more. Its only consumer was the
    ' default-Tools seeding below, which is gone -- everything else here defaults through
    ' the field initialisers in clsConfig.bi.
    if PsFileExists(this.ConfigFilename) <> 0 then
        ' The wrapper owns gApp.PreventConfigLoad, so this early return can no longer
        ' strand it -- which is exactly what it used to do.
        redim as DWSTRING lines(any)
        if TextFile_ReadLines( this.ConfigFilename, lines() ) = false then return true   ' error

        gApp.NonProjectNotes = ""

        for nLine as long = lbound(lines) to ubound(lines)
            wst = lines(nLine)

            ' One shared decode (modIniParse). The three loops that used to carry their
            ' own copy of this had already drifted -- see that file's header.
            dim as long nKind = Ini_ClassifyLine( wst, wKey, wData )
            if nKind = INI_LINE_SKIP then continue for
            if nKind = INI_LINE_NOTES_START then bReadingNote = true  : continue for
            if nKind = INI_LINE_NOTES_END   then bReadingNote = false : continue for
            ' BEFORE the key=value test, deliberately: a note containing an "=" is text,
            ' not configuration.
            if bReadingNote then
                gApp.NonProjectNotes = gApp.NonProjectNotes + wst + vbcrlf
                continue for
            end IF
            if nKind <> INI_LINE_KEYVALUE then continue for

            nData = PsVal(wData)


            ' SIZING AND ASSIGNMENT ARE SEPARATE, in all three indexed blocks below.
            '
            ' They used to be nested -- the assignments sat INSIDE "if i > ubound(...)",
            ' so an entry was only stored when its index grew the array. Any index the
            ' array already covered was silently dropped: a duplicate key, a
            ' non-ascending index in a hand-edited file, and -- for Tools, which is now
            ' erased above but was not -- EVERY entry on every reload after the first.
            if PsLeft(wKey, 9) = "CATEGORY_" then  ' is this a CATEGORY entry
            i = PsVal(PsRight(wKey,2))
            if i >= 0 then
                if i > ubound(this.Cat) then redim preserve this.Cat(i)
                ' trim() is not cosmetic. idFileType is compared against the FILETYPE_*
                ' string digits to decide which Explorer header a document belongs under,
                ' so one stray space in a hand-edited "CATEGORY_01= 1|-|Main" makes that
                ' category match nothing and silently empties it.
                this.Cat(i).idFileType     = PsTrim(PsStrParse(wData, 1, "|-|"))
                this.Cat(i).wszDescription = PsStrParse(wData, 2, "|-|")
                continue for
            end if
            end if

            if PsLeft(wKey, 9) = "USERTOOL_" then  ' is this a User Tool entry
            i = PsVal(PsRight(wKey,2))
                if i >= 0 then
                    if i > ubound(this.Tools) then redim preserve this.Tools(i)
                    this.Tools(i).wszDescription   = PsStrParse(wData, 1, "|-|")
                    this.Tools(i).wszCommand       = ProcessFromCurdriveApp( PsStrParse(wData, 2, "|-|") )
                    this.Tools(i).wszParameters    = PsStrParse(wData, 3, "|-|")
                    this.Tools(i).wszKey           = PsStrParse(wData, 4, "|-|")
                    this.Tools(i).wszWorkingFolder = ProcessFromCurdriveApp( PsStrParse(wData, 5, "|-|") )
                    this.Tools(i).IsCtrl           = PsVal(PsStrParse(wData, 6, "|-|"))
                    this.Tools(i).IsAlt            = PsVal(PsStrParse(wData, 7, "|-|"))
                    this.Tools(i).IsShift          = PsVal(PsStrParse(wData, 8, "|-|"))
                    this.Tools(i).IsPromptRun      = PsVal(PsStrParse(wData, 9, "|-|"))
                    this.Tools(i).IsMinimized      = PsVal(PsStrParse(wData, 10, "|-|"))
                    this.Tools(i).IsWaitFinish     = PsVal(PsStrParse(wData, 11, "|-|"))
                    this.Tools(i).IsDisplayMenu    = PsVal(PsStrParse(wData, 12, "|-|"))
                    this.Tools(i).Action           = PsVal(PsStrParse(wData, 13, "|-|"))
                    continue for
                end If
            end If

            if PsLeft(wKey, 6) = "BUILD_" then  ' is this a BUILD entry
            i = PsVal(PsRight(wKey,2))
                if i >= 0 then
                    if i > ubound(this.Builds) then redim preserve this.Builds(i)
                    this.Builds(i).id             = PsStrParse(wData, 1, "|-|").Utf8
                    this.Builds(i).wszDescription = PsStrParse(wData, 2, "|-|")
                    this.Builds(i).wszOptions     = PsStrParse(wData, 3, "|-|")
                    this.Builds(i).IsDefault      = PsVal(PsStrParse(wData, 4, "|-|"))
                    this.Builds(i).Is32bit        = PsVal(PsStrParse(wData, 5, "|-|"))
                    this.Builds(i).Is64bit        = PsVal(PsStrParse(wData, 6, "|-|"))
                    this.Builds(i).ExcludeInPopup = PsVal(PsStrParse(wData, 7, "|-|"))
                    this.Builds(i).IsCtrl         = PsVal(PsStrParse(wData, 8, "|-|"))
                    this.Builds(i).IsAlt          = PsVal(PsStrParse(wData, 9, "|-|"))
                    this.Builds(i).IsShift        = PsVal(PsStrParse(wData, 10, "|-|"))
                    this.Builds(i).wszKey         = PsStrParse(wData, 11, "|-|")
                    continue for
                end If
            end If

            if PsLeft(wKey, 4) = "MRU_" then  ' is this an MRU entry
            i = PsVal(PsRight(wKey,2))
                if (i >= 0) And (i <= 9) then
                    this.MRU(i) = ProcessFromCurdriveApp(wData)
                    continue for
                end If
            end If

            if PsLeft(wKey, 11) = "MRUPROJECT_" then  ' is this an MRU Project entry
            i = PsVal(PsRight(wKey,2))
                if (i >= 0) And (i <= 9) then
                    this.MRUProject(i) = ProcessFromCurdriveApp(wData)
                    continue for
                end If
            end If

            ' MRUSESSION_ keys from an older settings.ini fall through to the select
            ' below, match nothing, and are dropped -- the [MRUSESSIONS] section ages out
            ' on the first save.

            ' .Utf8 ON THE SELECTOR. `select case <DWSTRING>` works today only because
            ' AfxNova's DWSTRING extends WSTRING and converts implicitly; PsCore's does
            ' not, and this one line is what 120 of clsConfig's errors under the swap
            ' reduce to -- one per `case`, all from the same selector.
            '
            ' NO ENCODING CHANGE HERE, and that is checked rather than assumed: the
            ' implicit conversion goes through the ANSI codepage and .Utf8 does not, but
            ' every `case` label below is an ASCII key name, so a value that differs
            ' between the two encodings matches nothing either way.
            select case wKey.Utf8
            case "SettingsVersion":          this.SettingsVersion       = wData
            case "AskExit":                  this.AskExit               = nData
            case "RestoreSession":           this.RestoreSession        = nData
            ' The old key is still accepted on read. It always doubled as the last-project
            ' slot, so honouring it means an existing settings.ini reopens the project the
            ' user had open rather than silently coming up empty once. Write-side is the
            ' new key only, so the old one ages out on the first save.
            case "LastActiveProjectFile":    this.wszLastActiveProject  = ProcessFromCurdriveApp(wData)
            case "LastActiveSessionFile":    this.wszLastActiveProject  = ProcessFromCurdriveApp(wData)
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
            case "StripTrailingWhitespace":  this.StripTrailingWhitespace = nData
            case "LocalizationFile":         this.LocalizationFile      = ProcessFromCurdriveApp(wData)
            case "TabSize":                  this.TabSize               = wData
            case "UnicodeEncoding":          this.UnicodeEncoding       = nData
            ' Clamped rather than trusted: this value is fed straight to a document's
            ' FileEncoding, and an out-of-range one would reach the save encoder's
            ' select-case and fall through to no branch at all. A hand-edited or
            ' garbage key falls back to the compiled-in default instead.
            case "NewFileEncoding"
                if (nData >= FILE_ENCODING_ANSI) andalso (nData <= FILE_ENCODING_UTF16BE_BOM) then
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
            ' The code formatter's rule set. A key missing from an older settings.ini
            ' simply leaves the compiled-in default from FORMAT_RULES in place, which is
            ' why every one of these defaults to a conservative value.
            case "FmtReindent":              this.Format.Reindent             = nData
            case "FmtIndentCase":            this.Format.IndentCase           = nData
            case "FmtContinuationIndent":    this.Format.ContinuationIndent   = nData
            case "FmtCaseKeywords":          this.Format.CaseKeywords         = nData
            case "FmtCaseDirectives":        this.Format.CaseDirectives       = nData
            case "FmtCaseTypes":             this.Format.CaseTypes            = nData
            case "FmtSpaceOperators":        this.Format.SpaceAroundOperators = nData
            case "FmtSpaceAfterComma":       this.Format.SpaceAfterComma      = nData
            case "FmtParenSpacing":          this.Format.ParenSpacing         = nData
            case "FmtSpacesBeforeComment":   this.Format.SpacesBeforeComment  = nData
            case "FmtTrimTrailing":          this.Format.TrimTrailing         = nData
            case "FmtMaxBlankLines":         this.Format.MaxBlankLines        = nData
            case "FmtBlankLinesAroundProc":  this.Format.BlankLinesAroundProc = nData
            case "FmtFormatOnEnter":         this.Format.FormatOnEnter        = nData
            case "FmtFormatOnPaste":         this.Format.FormatOnPaste        = nData
            case "FBWINCompiler32":          this.FBWINCompiler32       = ProcessFromCurdriveApp(wData)
            case "FBWINCompiler64":          this.FBWINCompiler64       = ProcessFromCurdriveApp(wData)
            case "CompilerBuild":            this.CompilerBuild         = wData
            case "CompilerSwitches":         this.CompilerSwitches      = wData
            case "CompilerIncludes":         this.CompilerIncludes      = wData
            case "RunViaCommandWindow":      this.RunViaCommandWindow   = nData
            case "DisableCompileBeep":       this.DisableCompileBeep    = nData
            end select

            ' Only reload data affect instance states for heights and positions if
            ' this is not a hot reload. A hot reload is when the message loop detects
            ' that the config file has been externally modified (eg by a User Tool)
            ' and then will be reloaded on the fly.
            if isHotReload = false then
                select case wKey.Utf8
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
                    case "UnusedMinimizeOnOpen":     this.UnusedMinimizeOnOpen  = cbool(nData)
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

        next
    end if

    ' Put the LIVE restore slot back. See the note on this function's header: a hot reload
    ' must not resurrect a project the user has closed.
    if isHotReload then this.wszLastActiveProject = wszLiveLastProject

    ' Iterate the optional semi colon separated list of compiler include paths
    dim as DWSTRING allIncludes = PsStrTrimAny(this.CompilerIncludes, ";")
    this.CompilerIncludes = ""
    dim as DWSTRING includePath
    dim as long numpaths = PsStrParseCount(allIncludes, ";")
    for i as long = 1 to numpaths
        includePath = PsTrim(PsStrParse(allIncludes, i, ";"))
        if PsLen(includePath) then
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

    ' NO DEFAULT USER TOOLS. A new config file used to be seeded with two entries --
    ' "ASCII Chart" and "GUID Generator", pointing at asciichart32.exe and GUIDgen32.exe
    ' under settings\tools -- but both executables have been removed from the distribution,
    ' so seeding them gave a fresh install a Tools menu whose every entry failed to launch.
    ' The Tools list now starts empty and is whatever the user adds.

    ' Set some defaults if the config file was missing or corrupt
    ' if no builds exist then create some default ones... We do this whether or not
    ' the config file was there to be read.
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
    if PsLen(this.FBWINCompiler32) = 0 then
        wszText = PsExePath & "toolchains\" & gwszDefaultToolchain & "\fbc32.exe"
        if PsFileExists(wszText) then this.FBWINCompiler32 = wszText
    end if
    if PsLen(this.FBWINCompiler64) = 0 then
        wszText = PsExePath & "toolchains\" & gwszDefaultToolchain & "\fbc64.exe"
        if PsFileExists(wszText) then this.FBWINCompiler64 = wszText
    end if

    ' do some checks to see if the editor font exists. It is possible that the
    ' user could be accessing the editor from a different computer of using it under
    ' Wine where the previously selected font no longer exists.
    doFontSanityCheck()

    ' Save the last write time so that it can be checked in the message loop
    ' in order to detect external changes to it.
    this.DateFileTime = PsFileWriteTime( this.ConfigFilename )

    ' (The wrapper clears gApp.PreventConfigLoad; it is deliberately not touched here.)
    function = 0
end function



' ========================================================================================
' Save all options for the Project to a diskfile
' ========================================================================================
function clsConfig.ProjectSaveToFile() as boolean

    dim as DWSTRING cwzRelative, wszText
    dim as long nCount
    dim as long i

    dim pDoc as clsDocument ptr

    ' Paths are stored RELATIVE to the project file, but only for a project the user named
    ' and placed. The untitled workspace lives in settings\ under the exe, while its files
    ' live wherever the user works -- relativizing against it would turn every entry into a
    ' "..\..\" chain, and any file on another volume cannot be relativized at all. So the
    ' untitled workspace stores absolute paths, which is exactly what the session file it
    ' replaces did.
    '
    ' The {CURDRIVE} encoding below is NOT conditional and does not need to be:
    ' ProcessToCurdriveProject keys off ProjectFilename's drive, and the untitled project
    ' sits in the exe's own directory, so it already resolves to the App variant's answer.
    dim as boolean bRelativize = gApp.IsProjectNamed()

    ' One buffer, one write -- see SaveConfigFileBody for why.
    dim as DWSTRING wszOut

    wszOut &= "' PROJECT FILE" & vbcrlf
    wszOut &= "ProjectBuild="       & gApp.ProjectBuild & vbcrlf
    wszOut &= "ProjectOther32="     & gApp.ProjectOther32 & vbcrlf
    wszOut &= "ProjectOther64="     & gApp.ProjectOther64 & vbcrlf
    wszOut &= "ProjectCommandLine=" & gApp.ProjectCommandLine & vbcrlf

    ' Save all of the loaded tabs first. Not GetCurSel -- see clsTopTabCtl.SaveActiveTabIndex.
    wszOut &= "ActiveTab=" & gTTabCtl.SaveActiveTabIndex() & vbcrlf

    nCount = gTTabCtl.GetItemCount

    for i = 0 To nCount - 1
        if gTTabCtl.IsValidTab(i) = false then continue for
        pDoc = gTTabCtl.GetpDoc(i)
        if pDoc = 0 then continue for

        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then
            pDoc->SetDocInfo()
            wszText = pDoc->DiskFilename
            if bRelativize then
                cwzRelative = PsPathRelativeTo( wszText, PsPathDirWithSep(gApp.ProjectFilename) )
                if PsLen(cwzRelative) then
                    if PsPathIsRelative(cwzRelative) then wszText = cwzRelative
                end if
            end if
            wszText = ProcessToCurdriveProject(wszText)
            wszOut &= "File=" & wszText & vbcrlf
            wszOut &= "FileType=" & pDoc->ProjectFileType & vbcrlf
            wszOut &= "FileFolder=" & pDoc->docData.wszFolder & vbcrlf
            wszOut &= "TabIndex=" & "-1"     ' use -1 rather than true & vbcrlf
            wszOut &= "Bookmarks=" & pDoc->docData.wszBookmarks & vbcrlf
            wszOut &= "BreakPoints=" & pDoc->docData.wszBreakPoints & vbcrlf
            wszOut &= "FoldPoints=" & pDoc->docData.wszFoldPoints & vbcrlf
            wszOut &= "FirstLine=" & pDoc->docData.nFirstLine & vbcrlf
            wszOut &= "Position=" & pDoc->docData.nPosition & vbcrlf
            wszOut &= "FirstLine1=" & pDoc->docData.nFirstLine1 & vbcrlf
            wszOut &= "Position1=" & pDoc->docData.nPosition1 & vbcrlf
            wszOut &= "SplitMode=" & pDoc->docData.nSplitMode & vbcrlf
            wszOut &= "SplitPositionX=" & pDoc->docData.nSplitPositionX & vbcrlf
            wszOut &= "SplitPositionY=" & pDoc->docData.nSplitPositionY & vbcrlf
            wszOut &= "FocusEdit=" & pDoc->docData.nFocusEdit & vbcrlf
            wszOut &= "FileEnd=[-]" & vbcrlf
        end if
    next

    ' Save all other non-displayed documents for the project

    pDoc = gApp.pDocList
    do until pDoc = 0
        ' Only deal with files that are no longer "new"
        if pDoc->IsNewFlag = false then
            if gTTabCtl.GetTabIndexByDocumentPtr(pDoc) = -1 then
                wszText = pDoc->DiskFilename
                if bRelativize then
                    cwzRelative = PsPathRelativeTo( wszText, PsPathDirWithSep(gApp.ProjectFilename) )
                    ' The len() test is NOT optional. PsPathRelativeTo returns "" when
                    ' PathRelativePathToW fails -- a file on another volume, most obviously --
                    ' and PsPathIsRelative("") is TRUE. Without the guard wszText becomes ""
                    ' and the document is written as "File=", i.e. SILENTLY DROPPED from the
                    ' project. The tabbed loop above has always had this guard; this one did not.
                    if PsLen(cwzRelative) then
                        if PsPathIsRelative(cwzRelative) then wszText = cwzRelative
                    end if
                end if
                wszText = ProcessToCurdriveProject(wszText)
                wszOut &= "File=" & wszText & vbcrlf
                wszOut &= "FileType=" & pDoc->ProjectFileType & vbcrlf
            wszOut &= "FileFolder=" & pDoc->docData.wszFolder & vbcrlf
                wszOut &= "TabIndex=" & "0" & vbcrlf
                wszOut &= "Bookmarks=" & pDoc->docData.wszBookmarks & vbcrlf
                wszOut &= "BreakPoints=" & pDoc->docData.wszBreakPoints & vbcrlf
                wszOut &= "FoldPoints=" & pDoc->docData.wszFoldPoints & vbcrlf
                wszOut &= "FirstLine=" & pDoc->docData.nFirstLine & vbcrlf
                wszOut &= "Position=" & pDoc->docData.nPosition & vbcrlf
                wszOut &= "FirstLine1=" & pDoc->docData.nFirstLine1 & vbcrlf
                wszOut &= "Position1=" & pDoc->docData.nPosition1 & vbcrlf
                wszOut &= "SplitMode=" & pDoc->docData.nSplitMode & vbcrlf
                wszOut &= "SplitPositionX=" & pDoc->docData.nSplitPositionX & vbcrlf
                wszOut &= "SplitPositionY=" & pDoc->docData.nSplitPositionY & vbcrlf
                wszOut &= "FocusEdit=" & pDoc->docData.nFocusEdit & vbcrlf
                wszOut &= "FileEnd=[-]" & vbcrlf
            end If
        end if
        pDoc = pDoc->pDocnext
    loop

    ' The user's subfolders, one record each.
    '
    ' Written INDEPENDENTLY of the files rather than being derived from their FileFolder
    ' lines, because an EMPTY folder has no file to be derived from -- and creating a folder
    ' before putting anything in it is the obvious order to work in. Without these records
    ' such a folder would silently disappear on the next open.
    '
    ' A distinct key from the per-file "FileFolder": record assembly is driven by FileEnd,
    ' so a bare "Folder=" line out here would be absorbed into whichever file record happened
    ' to be pending. Emitted after the file block for readability only -- the reader is
    ' order-independent.
    for i = lbound(gProjectFolders) to ubound(gProjectFolders)
        wszOut &= "ProjectFolder=" & gProjectFolders(i).catIndex & _ & vbcrlf
                          "|-|" & gProjectFolders(i).wszPath
    next

    ' Notes belong to the workspace, so they are always written.
    dim wszNotes as DWSTRING = wstr("NOTES-START") + vbcrlf + _
                            gApp.ProjectNotes + vbcrlf + _
                            wstr("NOTES-END") + vbcrlf
    wszOut &= "" & vbcrlf
    wszOut &= "[Notes]" & vbcrlf
    wszOut &= wszNotes & vbcrlf

    if TextFile_WriteUtf16( gApp.ProjectFilename, wszOut ) = false then return false

    ' The restore slot names the NAMED project to reopen, and only that. Writing the
    ' untitled workspace's own path here makes the slot permanently name settings\
    ' default.tiko, after which no project is ever restored again -- the next launch sees
    ' the untitled path, treats it as "nothing named to reopen", and comes up untitled
    ' forever. An untitled workspace clears the slot instead: there is no named project to
    ' go back to, which is exactly what an empty slot means.
    if gApp.IsProjectNamed() then
        this.wszLastActiveProject = gApp.ProjectFilename
    else
        this.wszLastActiveProject = ""
    end if

    function = true   ' successful save
end function


' ========================================================================================
' Load all options for the Project from a diskfile
' ========================================================================================
function clsConfig.ProjectLoadFromFile( byval wszFile as DWSTRING ) as boolean

    dim pDoc as clsDocument ptr
    dim as DWSTRING wst, wKey, wData, wszFilename, wszFolder

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

    if PsFileExists(wszFile) = 0 then exit function

    ' OPEN AND VALIDATE BEFORE TOUCHING ANY GLOBAL STATE.
    '
    ' This used to sit twenty lines further down, AFTER ProjectFilename and ProjectName had
    ' been overwritten, ProjectNotes blanked and ProjectFolders_Clear() run -- and its
    ' "return false" then left all of that in place. A .tiko that exists but cannot be
    ' opened (held by another process, on a share that just dropped, permissions) therefore
    ' destroyed the folder tree of the workspace the user still had open, pointed the app at
    ' a project it had failed to read, and returned with the WAIT cursor still up because
    ' the restore at the end was never reached.
    '
    ' Nothing below this line can fail in a way that leaves the previous workspace half
    ' dismantled, because nothing below it is reachable unless the file is readable.
    redim as DWSTRING lines(any)
    if TextFile_ReadLines( wszFile, lines() ) = false then return false

    dim as HCURSOR hCurSave = GetCursor()
    SetCursor( LoadCursor(0, IDC_WAIT) )

    gApp.ProjectFilename = wszFile
    gApp.ProjectName = PsPathName(gApp.ProjectFilename )
    gApp.ProjectNotes = ""

    ' Folders belong to the workspace being opened, not to the one being left behind. Without
    ' this the previous project's folders would survive into this one and be written back out
    ' to its .tiko on the next save.
    ProjectFolders_Clear()

    ' Variable length array to hold sequence of all project files
    dim docData(any) as PROJECT_FILELOAD_DATA

    gApp.IsProjectLoading = true

    for nLine as long = lbound(lines) to ubound(lines)
        wst = lines(nLine)

        ' One shared decode (modIniParse), same as LoadConfigFileBody.
        dim as long nKind = Ini_ClassifyLine( wst, wKey, wData )
        if nKind = INI_LINE_SKIP then continue for
        if nKind = INI_LINE_NOTES_START then bReadingNote = true  : continue for
        if nKind = INI_LINE_NOTES_END   then bReadingNote = false : continue for
        ' BEFORE the key=value test: a note containing an "=" is text, not a setting.
        if bReadingNote then
            gApp.ProjectNotes = gApp.ProjectNotes + wst + vbcrlf
            continue for
        end IF
        if nKind <> INI_LINE_KEYVALUE then continue for

        select case PsUCase(wData).Utf8
            case "TRUE":   nData = true
            case "FALSE":  nData = false
            case Else:     nData = PsVal(wData)
        end select

        wData = ProcessFromCurdriveProject(wData)

        select case wKey.Utf8
            case "ProjectBuild":        gApp.ProjectBuild       = wData.Utf8
            case "ProjectOther32":      gApp.ProjectOther32     = wData
            case "ProjectOther64":      gApp.ProjectOther64     = wData
            case "ProjectCommandLine":  gApp.ProjectCommandLine = wData
            case "ActiveTab":           nActiveTab              = nData
            case "File":                wszFilename             = wData
            case "FileType":            wszFileType             = wData
            case "FileFolder":          wszFolder               = wData

            case "ProjectFolder":
            ' <catIndex>|-|<path>. Add rather than append blindly: ProjectFolders_Add is
            ' idempotent and creates missing ancestors, so a hand-edited file listing
            ' "a/b" without "a" still loads as a well-formed tree instead of an orphan.
            dim as long nSepPos = PsInStr( wData, "|-|" )
            if nSepPos > 0 then
                ProjectFolders_Add( PsVal( PsLeft(wData, nSepPos - 1) ), PsMid(wData, nSepPos + 3) )
            end if
            case "Bookmarks":           sBookmarks              = wData.Utf8
            case "BreakPoints":         sBreakPoints            = wData.Utf8
            case "FoldPoints":          sFoldPoints             = wData.Utf8
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
            if PsPathIsRelative(wszFilename) then
                wszFilename = PsPathJoin( PsPathDirWithSep(gApp.ProjectFilename), wszFilename)
            end if
            if PsFileExists(wszFilename) then

                nFileIndex = nFileIndex + 1
                if nFileIndex > ubound(docData) then
                    redim preserve docData( ubound(docData) + 100 )
                end If

                with docData(nFileIndex)
                    .bIsValidData    = true
                    .wszFilename     = wszFilename
                    .wszFileType     = wszFileType
                    .wszFolder       = wszFolder
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
            ' Reset OUTSIDE the PsFileExists guard, unlike the block above. That block sits
            ' inside it, so a record naming a file that no longer exists leaves its state
            ' standing and the NEXT record inherits it -- a pre-existing bug, left alone
            ' here, but deliberately not joined: a missing file must not be able to hand its
            ' folder to whatever record follows it.
            wszFolder = ""

        end select

    next


    ' Load/Display all of the project files
    for i as long = lbound(docData) to ubound(docData)

        if PsFileExists( docData(i).wszFilename ) = false then continue for

        pDoc = gApp.CreateEmptyDocument()
        pDoc->LoadDiskFile( docData(i).wszFilename )
        ' Never let a loaded document stay uncategorized. The Explorer groups strictly by
        ' category, so a FILETYPE_UNDEFINED document matches no header and DISAPPEARS from
        ' the list while still being in the workspace. Existing project files really do
        ' carry FileType=0 entries -- tiko.tiko has one -- so this is repairing data on
        ' disk, not guarding against a case that cannot happen. The session loader has
        ' always applied this same fallback; the project loader never did.
        if docData(i).wszFileType = FILETYPE_UNDEFINED then
            pDoc->ProjectFiletype = FILETYPE_NORMAL
        else
            pDoc->ProjectFiletype = docData(i).wszFileType
        end if
        pDoc->docData = docData(i)
        if docData(i).bLoadInTab then
            pDoc->AssignTextBuffer()
            iTab = gTTabCtl.AddTab( pDoc )  ' Add the new document to the top tabcontrol
        end if
    next

    gApp.IsProjectLoading = false

    gApp.hIconPanel = 0
    gApp.wszLastOpenFolder = ""

    ' Load all of the filenames into the Explorer listbox.
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

    this.wszLastActiveProject = gApp.ProjectFilename

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

    dim as PsFileTime ft

    ' Compare the disk file date time to the value currently stored in static variable.
    ft = PsFileWriteTime(this.ConfigFilename)
    if ft <> this.DateFileTime then
        this.LoadConfigFile(true)  ' this function also updates the _DateFileTime value
        ' THE FONT FILE, before the walk below -- same rule as the options dialog. Miss
        ' this one and the editor font follows the dialog but not an edit to settings.ini,
        ' which is the harder of the two to notice.
        SciHost_ApplyConfiguredFont()
        ' Apply the newly saved options to all open Scintilla windows
        dim pDoc as clsDocument ptr = gApp.pDocList
        do until pDoc = 0
            pDoc->ApplyProperties
            pDoc = pDoc->pDocnext
        loop

        ' The accelerator tables read gConfig.Tools and gConfig.Builds, and BOTH arrays are
        ' now rebuilt from the file by the load above. They were not before -- Tools was
        ' never erased, so a hot reload could not change it and the tables stayed correct
        ' by accident. Now they have to be rebuilt or the shortcuts describe the previous
        ' contents of a file that has already changed.
        frmUserTools_CreateAcceleratorTable()
        frmBuildConfig_CreateAcceleratorTable()
    end if

    function = 0
end function
