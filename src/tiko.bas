' ========================================================================================
' tiko editor 
' Windows FreeBASIC Editor (Windows 64 bit)
' Paul Squires (2016-2026)
' ========================================================================================

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


#define UNICODE
#define _WIN32_WINNT &h0602  

#include once "windows.bi"
#include once "vbcompat.bi"
#include once "win\shobjidl.bi"
#include once "win\TlHelp32.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxFile.inc"
#include once "AfxNova\AfxRichEdit.inc"
#include once "AfxNova\AfxGdiplus.inc"
#include once "AfxNova\AfxCom.inc" 
#include once "AfxNova\CImageCtx.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\CWinHttpRequest.inc"
' The Help Center pane. Note this brings in AfxWebView2.bi, which resolves the five loader
' entrypoints out of WebView2Loader.dll at RUNTIME by plain name -- there is no import lib
' and no link flag, so the DLL has to sit beside tiko.exe (see _copy_webview2.bat).
#include once "AfxNova\CWebView2.inc"

using AfxNova

'' Phase 7a scaffolding -- see the header. Deleted when the DWSTRING swap lands.
#include once "PsCompat.bi"

' ----------------------------------------------------------------------------------------
' PsCore, UNDER A NAMESPACE. AfxNova and PsCore both declare a type called DWSTRING, and
' the unqualified name can only mean one of them -- which is why the full swap is 1008
' errors and belongs with the shell conversion.
'
' A namespace makes both available at once: `DWSTRING` stays AfxNova's for tiko's 1631
' existing sites, and `PsC.DWSTRING` is PsCore's, for code that needs what PsCore's has
' and AfxNova's does not. The encoder needs exactly that -- a string whose LENGTH is
' authoritative rather than its terminator, so an embedded NUL survives the round trip.
'
' This is scaffolding of the same kind as PsCompat.bi. When the type swap lands, the
' namespace comes off and the PsC. prefixes go with it.
' ----------------------------------------------------------------------------------------
'' modScintilla.bi FIRST, and the ORDER IS LOAD-BEARING. tiko #Defines all 117
'' SCI_* constants and 19 SCK_*; PsPlatform declares them as `const` behind
'' #ifndef guards. Guards only work in this direction -- with PsScintilla.bi
'' first the const already exists and tiko's #Define becomes the duplicate,
'' which no guard on the library side can prevent.
#include once "modScintilla.bi"

'' THE C BINDINGS GO OUTSIDE THE NAMESPACE, and this is not a style choice.
'' fbc mangles an `extern "C"` block declared inside a namespace as
'' PSC::bl_context_save, which matches nothing in libblend2d -- five undefined
'' references, at LINK time only, with a clean compile. PsEncoding never hit
'' this because it is pure FreeBASIC; PsSciView is not.
''
'' `#include once` makes the copies inside the namespace below no-ops, so the
'' FreeBASIC code -- which is what needs PsCore's DWSTRING -- still gets the
'' namespace and the C entry points stay global.
#include once "bind/Blend2D.bi"
#include once "bind/FreeType.bi"
#include once "bind/HarfBuzz.bi"
#include once "scintilla/PsScintilla.bi"

namespace PsC
    #include once "core/PsEncoding.inc"
    '' Phase 7d: the editor. PsSciView is a PsWidget in a PsSurface, driven
    '' through PsPlatform's Win32 host bridge -- see frmSciHost.bi.
    #include once "ui/core/PsDispatch.inc"
    #include once "ui/core/PsPaintWalk.inc"
    #include once "scintilla/PsTextEngineC.inc"
    #include once "scintilla/PsSciView.inc"
    #include once "scintilla/PsSciNotify.inc"
    #include once "platform/win32host/PsWin32Host.inc"
end namespace


#define APPNAME             wstr("Tiko Editor")
#define APPNAMESHORT        wstr("Tiko")
#define APPCLASSNAME        wstr("tiko_editor_class")
#define APPVERSION          wstr("1.3.2") 
#define APPEXTENSION        wstr(".tiko") 
#define APPBITS             wstr(" (64-bit)")
#define RUNBATCHFILE        wstr("_tiko_runbatch.bat")
#define QUICKRUNBAS         wstr("_tiko_quickrun.bas")
#define QUICKRUNEXE         wstr("_tiko_quickrun.exe")

#define APPCOPYRIGHT   wstr("Paul Squires, PlanetSquires Software, Copyright (C) 2016-2026") 
dim shared as DWSTRING gwszDefaultToolchain = "FreeBASIC-1.10.1-winlibs-gcc-9.3.0"

'TODO: Refactor AutoSave functionality. Until then, just disable it in the editor.
#define ENABLE_AUTOSAVE false

' Comment out the following define in order to disable logging.
'#define LOGGING_ENABLED
#include once "app/logging.bas"

#include once "app/fbcParser.bi"
' The debug engine, as a DLL (C:\dev\debugParser -- see _copy_debugparser.bat). This is its
' ONLY header. It reads the debug information fbc embeds with -g and a gas backend, which gdb
' cannot read at all, and drives the debuggee against it. Up here beside the other DLL header
' because clsDocument.ToggleBreakPoint calls into it.
#include once "app/debugParser.bi"
#include once "frmSciHost.bi"
#include once "clsDocument.bi"
#include once "modDeclares.bi"
' Declarations only, and deliberately naming no Ps* type: clsConfig.inc calls
' NavHistory_Clear from both of its session-load paths, well ahead of the frm* block. The
' implementation goes in after modRoutines.inc, whose OpenSelectedDocument it drives.
#include once "app/modNavHistory.bi"
' Same split, same reason: clsDocument.inc calls FindProject_OnDocumentClosing from
' DestroyScintillaWindows, well ahead of the modFindProject.inc that implements it. This
' header names clsDocument ptr and SCNOTIFICATION but no Ps* type.
#include once "modFindProject.bi"
' Formatter Scintilla-bridge declarations. Named clsDocument ptr, no Ps* type. Up here
' because frmMainOnNotify/frmMainEdit/frmMainOnCommand all call into it well before the
' implementation can be compiled -- that needs PsMessageBox and lands near the end.
#include once "modFormatApply.bi"
#include once "frmFormatOptions.bi"
#include once "PsBufferPaint.bi"
#include once "clsTopTabCtl.bi"
' Ahead of clsConfig.bi because clsConfig embeds a FORMAT_RULES. Declarations only, and it
' names no Ps* type, no clsDocument and nothing from Scintilla -- the formatter engine is
' pure text in, text out, which is what lets the Options preview, the self-test and the
' offline Format Project path all drive the exact same code the editor does.
#include once "app/modFormat.bi"
#include once "app/clsConfig.bi"
#include once "app/modProjectFolders.bi"
#include once "clsApp.bi"
#include once "app/clsSymbolDb.bi"
#include once "clsScanMgr.bi"
#include once "app/modUnusedSymbols.bi"
#include once "frmUnusedSymbols.bi"
' Declared up here rather than beside its .inc because modCodetips.inc (which BUILDS the
' AUTOC_ITEM array) is included well before the frm* block.
#include once "frmAutoComplete.bi"
' Same split, same reason: modCodetips.inc calls Codetip_Show, and it is included at :103,
' well before PsTooltip.inc. This header names no Ps* type; the implementation goes in
' immediately after PsTooltip.inc.
#include once "modCodetipTip.bi"
' Up here rather than beside its .inc because clsConfig.inc calls
' frmHelpCenter_CaptureState() from the top of SaveConfigFile. This header names CWebView2
' (hence its position after the AfxNova include above) but no Ps* type.
#include once "frmHelpCenter.bi"
' Same reason: clsConfig.inc calls frmOutputFloat_IsFloating / _CaptureState from the
' frmOutput_CaptureState guard, and that runs from the top of SaveConfigFile. This header
' deliberately names no Ps* type, so it can sit here ahead of the whole Ps* block.
#include once "frmOutputFloat.bi"

'  Global classes
dim shared gApp     as clsApp
dim shared gConfig  as clsConfig
dim shared gTTabCtl as clsTopTabCtl


#include once "modCWindow.inc"
#include once "modThemes.inc"
' Declarations only. The implementation is included near the very end, once every
' frmXxx_ApplyTheme it dispatches to exists -- see the header of modThemeApply.bi.
#include once "modThemeApply.bi"
' Declarations only, and deliberately naming no Ps* type -- the implementation is included at
' the end, once PsMessageBox and OptionsTheme_FillButton exist.
#include once "modFileWatch.bi"
' Same split, same reason: TikoMsgBox is called from modCompile.inc (:115) and from tiko.bas's
' own startup path, both of which are ahead of PsMessageBox.inc. This header names no Ps* type;
' the implementation is included at the end beside modFileWatch.inc.
#include once "modMsgBox.bi"
#include once "app/modProjectFolders.inc"
' The shared key=value line decode. Ahead of clsConfig.inc, whose two parsers use it, and
' declarations-and-one-pure-function only -- it names no Ps* type and no global.
#include once "app/modIniParse.bi"
#include once "modIniParse.inc"
#include once "clsConfig.inc"
#include once "PsBufferPaint.inc"
' modRoutines was a 2,250-line junk drawer. These three are PURE MOVES out of it -- see
' each file's header. Order is load-bearing for the first two: modEncoding holds the four
' PRIVATE conversion helpers (undeclared in any .bi), and modRoutines' own GetFileToString
' and modCompileErrors both call into them, so they must be DEFINED first.
#include once "modPaths.inc"
' The encoding conversion logic, then the dialogs it asks through. The .bi comes
' first because modEncoding.inc CALLS one of them: the logic decides whether to
' ask, the UI decides what asking looks like.
#include once "modEncodingUi.bi"
#include once "modEncoding.inc"
#include once "modEncodingUi.inc"
#include once "modRoutines.inc"
#include once "modUpdateCheck.inc"
' The formatter engine. After clsConfig.inc (it reads gConfig's keyword list to build its
' casing vocabulary) and after modRoutines.inc for PsExePath. It deliberately calls
' NOTHING else in tiko -- no document, no window, no Scintilla.
#include once "modFormat.inc"
' After modRoutines.inc: NavHistory_Goto drives OpenSelectedDocument.
#include once "modNavHistory.inc"
#include once "frmSciHost.inc"
#include once "clsDocument.inc"
' Encoding conversion self-test. After modRoutines.inc (Doc_EncodeForDisk/GetFileToString)
' and clsDocument.inc (the clsDocument type it instantiates for the disk round-trip).
#include once "app/modEncodingSelfTest.bi"
#include once "modEncodingSelfTest.inc"
' Atomic-save self-test. After modRoutines.inc, which owns Doc_WriteToDisk. It writes to
' %TEMP% rather than staying pure, deliberately: the contract it asserts -- that a FAILED
' write leaves the file already on disk intact -- cannot be reached without a real file.
#include once "app/modSaveSelfTest.bi"
#include once "modSaveSelfTest.inc"
#include once "clsApp.inc"
#include once "app/clsSymbolDb.inc"
#include once "clsScanMgr.inc"
' After clsSymbolDb.inc (it reads the reference counts through gSymDb's
' accessors) and after modRoutines.inc, whose OpenSelectedDocument the window
' drives. The MODEL only; the window comes in with the frm* block.
#include once "modUnusedSymbols.inc"
#include once "clsTopTabCtl.inc"
#include once "modAutoInsert.inc"
' Build state and the command-line composers. Ahead of modCompile.inc and
' modCompileErrors.inc, both of which call into it.
#include once "app/modBuildService.inc"
#include once "modCompile.inc"
#include once "modCompileErrors.inc"
#include once "modMenus.inc"
#include once "modCodetips.inc"
#include once "app/modMenuDefinitions.inc"
#include once "modMRU.inc"
' The Find in Project search engine and result model. No windows and no Ps* types, so it
' needs only clsApp/clsDocument/modScintilla, all already in scope. Ahead of
' modFindReplace.inc because the Find bar drives the project search.
#include once "modFindProject.inc"
#include once "modFindReplace.inc"
#include once "app/modFuzzy.inc"
' The keyboard shortcut MODEL (gKeys, the defaults table, keybindings.ini, the accelerator
' build, the key vocabulary). No UI and no control dependencies -- it needs only modDeclares
' (IDM_* / IDC_MENUBAR_*), CWindow and CTextStream, all of which are already in scope. It
' must precede frmKeyboardEdit / frmAssignKey / frmUserTools / frmBuildConfig, which call
' into the key vocabulary, and frmMain, which builds the table at startup.
#include once "modKeyBindings.inc"

' Custom controls
#include once "PsVScrollBar.inc"
#include once "PsHScrollBar.inc"
#include once "PsColumnHeader.inc"
' PsPopupMenu + PsTextBox must precede PsListTree: its in-place label editor is a PsTextBox
' child (and PsTextBox uses PsPopupMenu for its context menu), so PsListTree.inc's calls need
' those declarations already in scope. (Moved up from below for the treeview sync.)
#include once "PsPopupMenu.inc"
#include once "PsTextBox.inc"
#include once "PsListTree.inc"
#include once "PsStatusBar.inc"
#include once "PsTabBar.inc"
#include once "PsMenuBar.inc"
#include once "PsSplitter.inc"
#include once "PsIconPanel.inc"
#include once "PsSelectBar.inc"
' Dependencies first: PsScrollPanel needs PsVScrollBar, PsComboBox needs PsPopupMenu,
' PsNumericUpDown needs PsTextBox (+ PsPopupMenu), PsMessageBox needs PsButton. All four
' of those are already included above, so only the ordering below matters.
#include once "PsToggle.inc"
#include once "PsButton.inc"
' Vendored for the Unused Symbols report's kind filters. Its dependencies
' (PsBufferPaint, PsTipHost/PsTooltip) are already here and byte-identical.
#include once "PsCheckBox.inc"
#include once "PsComboBox.inc"
#include once "PsScrollPanel.inc"
#include once "PsNumericUpDown.inc"
#include once "PsMessageBox.inc"
' PsColorPicker depends on nothing but PsBufferPaint (it owns no child window and no popup --
' that is the whole design), so its position here is only for tidiness beside its siblings.
#include once "PsColorPicker.inc"
' PsTooltip depends on nothing but PsBufferPaint. It does NOT subclass the control it serves
' and adds NO pump obligation, which is why it can be dropped in beside the comctl32 tooltips
' the other controls still use rather than replacing them all at once. First tiko user: the
' User Tools dialog's Parameters field, which needs a WRAPPED, multi-line tip -- something the
' comctl32 path only reaches by hand-sending TTM_SETMAXTIPWIDTH.
#include once "PsTooltip.inc"
' The two tooltip colour recipes, shared by all four tip owners. Here rather than in
' modThemeApply.inc because PSTOOLTIP_COLORS is PsTooltip's and every caller precedes
' the apply layer.
#include once "app/modThemeTips.inc"
' The code tip window. Immediately after PsTooltip.inc because it is a PsTooltip driven
' entirely by hand, and before frmAutoComplete.inc, which calls Codetip_Show on commit.
#include once "modCodetipTip.inc"

#include once "frmAbout.inc"
#include once "frmTopTabs.inc"
#include once "frmTopTabsMenu.inc"
#include once "frmTopTabsInfo.inc"
#include once "frmFind.inc"
#include once "frmReplace.inc"
#include once "frmMenuBar.inc"
#include once "frmStatusBar.inc"
#include once "frmEditorHScroll.inc" 
#include once "frmEditorVScroll.inc" 
#include once "frmPanel.inc" 
#include once "frmPanelMenu.inc" 
#include once "frmExplorer.inc"
#include once "modExplorerSelfTest.inc" 
#include once "frmBookmarks.inc" 
#include once "frmFunctions.inc"
#include once "frmAutoComplete.inc"
#include once "frmKeyboardEdit.bi"
#include once "frmKeyboardEdit.inc"
#include once "frmKeyboard.inc"
#include once "frmOutput.inc"
' Immediately after frmOutput.inc, whose functions it drives. It names PsBufferPaint, so it
' must follow that include; and it must precede modContextMenus.inc, modThemeApply.inc and
' frmMain.inc, all of which call into it.
#include once "frmOutputFloat.inc"
#include once "modOptionsRows.inc"
#include once "frmOptionsColors.inc"
#include once "frmOptionsCompiler.inc"
#include once "frmOptionsLocal.inc"
#include once "frmOptionsKeywords.inc"
#include once "frmOptions.inc"
' After frmOptions: frmThemes' colour page uses OptionsFont_Base and OptionsTheme_Fill*
' from the options modules, and nothing in frmOptions refers back to frmThemes.
#include once "frmThemes.inc"
' After frmOptions.inc / modOptionsRows.inc: its themed message box uses
' OptionsTheme_FillButton, and PsMessageBox is already in scope above.
#include once "frmHelpCenter.inc"
#include once "frmInputBox.inc"
#include once "frmFindInProject.inc"
#include once "frmSearchSymbol.inc"
#include once "frmProjectOptions.inc"
#include once "frmMainOnCommand.inc"
#include once "frmMainOnNotify.inc"
' The shared shortcut editor, then the two dialogs that open it. All three must follow
' modOptionsRows.inc (they dress their controls through OptionsTheme_Fill*) and
' frmKeyboardEdit.inc (frmAssignKey shares its capture rules), and frmAssignKey must
' precede frmUserTools and frmBuildConfig, which both call frmAssignKey_Show.
#include once "frmAssignKey.bi"
#include once "frmAssignKey.inc"
#include once "frmUserTools.inc"
' frmBuildConfig lands HERE, not up with the other frm* dialogs, and the position is
' load-bearing rather than tidy: it dresses its controls through OptionsTheme_Fill*
' (modOptionsRows.inc, above) and opens frmAssignKey. Its five exports are reached
' through frmBuildConfig.bi, which clsConfig.inc pulls in independently far earlier, so
' every caller still resolves -- FreeBASIC needs only the DECLARATION in scope.
#include once "frmBuildConfig.inc"
' Format Options. HERE, after modOptionsRows.inc, because it dresses its controls through
' the shared OptionsTheme_Fill* helpers -- the only thing it borrows from that module.
#include once "frmFormatOptions.inc"
#include once "modMsgPump.inc"
#include once "frmMainFile.inc"
#include once "frmMainEdit.inc"
#include once "frmMainSearch.inc"
#include once "frmMainView.inc"
#include once "frmMainProject.inc"
#include once "frmMainCompile.inc"
#include once "frmMainDebug.inc"
#include once "frmDebug.inc"
' After modUnusedSymbols.inc (the model) and modRoutines.inc (OpenSelectedDocument).
#include once "frmUnusedSymbols.inc"
' Late on purpose: the context menus' select callbacks call into the OnCommand_*
' handlers, the panel loaders and frmMenuBar_CreatePopup, so every one of those must
' already be declared. (Its declarations come in early via modContextMenus.bi.)
#include once "modContextMenus.inc"
' Must come after every frmXxx_ApplyTheme / _SyncTheme it routes to, and before frmMain.inc,
' which calls Theme_OnCoalesceTimer from WM_TIMER.
#include once "modThemeApply.inc"
' Needs PsMessageBox (its two boxes), OptionsTheme_FillButton (their button colours),
' OpenSelectedDocument / ReloadDocument / OnCommand_FileClose / OnCommand_ProjectRemove --
' so it lands here, and before frmMain.inc, which drives it from WM_TIMER and WM_ACTIVATEAPP.
#include once "modFileWatch.inc"
' Needs PsMessageBox and OptionsTheme_FillButton (modOptionsRows.inc), so it cannot go any
' earlier than this.
#include once "modMsgBox.inc"
' Formatter Scintilla bridge. HERE, at the end, because its command handlers call
' TikoMsgBox (modMsgBox.inc, immediately above) which needs PsMessageBox.
#include once "modFormatApply.inc"
#include once "frmMain.inc"


' ========================================================================================
' WinMain
' ========================================================================================
function WinMain( _
            byval hInstance     as HINSTANCE, _
            byval hPrevInstance as HINSTANCE, _
            byval szCmdLine     as zstring ptr, _
            byval nCmdShow      as long _
            ) as long


    LogInit( "_debug.txt" )

    ' ---- DLL SEARCH HARDENING, before anything can load a library --------------------
    ' By default LoadLibrary with a bare name searches the CURRENT DIRECTORY and then PATH.
    ' tiko is exposed to both: it loads Lexilla64/Scintilla64 by plain name, AfxWebView2
    ' resolves WebView2Loader.dll by plain name at runtime, and code_Compile chdir's the
    ' whole process into whichever project is being built.
    '
    ' LOAD_LIBRARY_SEARCH_DEFAULT_DIRS drops the current directory and PATH, leaving the
    ' application directory, System32 and any explicitly added user directories -- which is
    ' where every DLL tiko actually wants already lives (WebView2Loader.dll sits beside
    ' tiko.exe by _copy_webview2.bat, and the application directory IS searched).
    '
    ' Resolved through GetProcAddress rather than called directly: a direct call would put a
    ' static import to it in the exe, and an OS without the export would then fail to start
    ' rather than simply skipping the hardening. This must not raise tiko's minimum Windows.
    scope
        dim as any ptr hK32 = GetModuleHandleW( "kernel32.dll" )
        if hK32 then
            dim SetDefaultDllDirs as function( byval as DWORD ) as WINBOOL
            SetDefaultDllDirs = cast( any ptr, GetProcAddress( hK32, "SetDefaultDllDirectories" ) )
            if SetDefaultDllDirs then SetDefaultDllDirs( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS )
        end if
    end scope

    ' Load configuration files
    gConfig.LoadConfigFile()
    gConfig.LoadKeywords()

    
    ' Attempt to load the english localization file. This is necessary because
    ' any non-english localization file will have missing entries filled by the
    ' english version.
    dim as DWSTRING wszLocalizationFile
    wszLocalizationFile = PsExePath + wstr("settings\languages\english.lang")
    if LoadLocalizationFile(wszLocalizationFile, true) = false Then
        ' TMB_ICON_NONE, and it is not a style choice: SegoeFluentIcons.ttf is loaded further
        ' down (and one of these boxes is the report that it could not be), so a glyph here
        ' would draw as a missing-character box. The four startup boxes also run before
        ' frmMain_Show builds ghFont() or reads a theme -- TikoMsgBox falls back to the system
        ' message font and GetSysColor for exactly this window. See modMsgBox.bi.
        TikoMsgBox( 0, _
                    "English Localization file could not be loaded. Aborting application." + vbcrlf + _
                    wszLocalizationFile, _
                    "Error", TMB_ICON_NONE, TMB_OK )
        return 1
    end if
    
    
    ' Load the selected localization file
    wszLocalizationFile = PsExePath + "settings\languages\" + gConfig.LocalizationFile
    if LoadLocalizationFile(wszLocalizationFile, false) = false then
        TikoMsgBox( 0, _
                    "Localization file could not be loaded." + vbcrlf + _
                    wszLocalizationFile, _
                    "Error", TMB_ICON_NONE, TMB_OK )
        return 1
    end if
    
    
    ' Load the Segoe Fluent Icons ttf file that is used for displaying the various
    ' icons used within the editor.
    dim as DWSTRING wszFontFile 
    wszFontFile = PsExePath + "SegoeFluentIcons.ttf"
    if AddFontResourceEx(wszFontFile.vptr, FR_PRIVATE, NULL) = 0 then
        TikoMsgBox( 0, _
                    "Unable to load application font 'SegoeFluentIcons.ttf'. Aborting application." , _
                    "Error", TMB_ICON_NONE, TMB_OK )
        return 1
    end if


    ' If multiple editor instances is disallowed then bring the current active
    ' instance to the foreground and pass it whatever command line was intended
    ' for this instance.
    if gConfig.MultipleInstances = false Then
        if SpawnPreviousInstance() then return 0
    end if

    
    ' Load default Explorer Categories should none exist. Need to do it here
    ' rather than from within Config because the localization file must be 
    ' loaded first.
    gConfig.SetCategoryDefaults()


    ' Initialize the COM library
    CoInitialize(null)


    ' Load the Scintilla code editing dll
    ' BY FULL PATH, not by bare name. These live beside the exe and nowhere else, so
    ' naming the directory removes the search entirely rather than relying on the order
    ' being what we hardened it to above. Belt and braces, and it costs nothing.
    dim as DWSTRING wszDllPath = PsExePath()
    dim as any ptr pLibLexilla   = dylibload( wszDllPath & "Lexilla64.dll" )
    dim as any ptr pLibScintilla = dylibload( wszDllPath & "Scintilla64.dll" )

    if (pLibLexilla = 0) orelse (pLibScintilla = 0) then
        TikoMsgBox( 0, _
                    "Error loading Scintilla DLL's. Ensure C++ redistributable is installed:" + vbcrlf + _
                    "https://aka.ms/vs/17/release/vc_redist.x64.exe", _
                    "Error", TMB_ICON_NONE, TMB_OK )
        return 1
    end if
    gApp.pfnCreateLexerfn = cast(CreateLexerFn , GetProcAddress(pLibLexilla, "CreateLexer"))


    ' hhctrl.ocx is no longer loaded: the .chm help path went with ShowContextHelp when the
    ' Help Center replaced it.

    ' Load codetip files
    if gConfig.Codetips then gConfig.LoadCodetips

    ' Start the background fbcParser scan worker (owns all fbcParser.dll calls)
    gScanMgr.StartWorker()

    ' Initialize GDI+ (PsBufferPaint draws all geometry through it).
    ' Deliberately placed AFTER every early "return 1" above, so no failure path can
    ' skip the matching shutdown, and BEFORE the first window exists, because a
    ' PsBufferPaint built during the very first WM_PAINT already needs GDI+ running.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    ' Show the main form
    function = frmMain_Show( 0 )


    ' Free the Scintilla and CaptureConsole libraries
    dylibfree(pLibLexilla)
    dylibfree(pLibScintilla)

    
    ' Unload the font file
    if PsLen(wszFontFile) then RemoveFontResource(wszFontFile)

    ' Shut GDI+ down. frmMain_Show has returned, so every window is destroyed and every
    ' PsBufferPaint that painted into one has run its destructor -- no CGp* object can
    ' still be alive. This must also precede CoUninitialize, since GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    ' Uninitialize the COM library
    CoUninitialize

    LogClose()

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )

