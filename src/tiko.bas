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
#include once "logging.bas"

#include once "modScintilla.bi"

#include once "fbcParser.bi"
#include once "clsDocument.bi"
#include once "modDeclares.bi"
' Declarations only, and deliberately naming no Ps* type: clsConfig.inc calls
' NavHistory_Clear from both of its session-load paths, well ahead of the frm* block. The
' implementation goes in after modRoutines.inc, whose OpenSelectedDocument it drives.
#include once "modNavHistory.bi"
#include once "PsBufferPaint.bi"
#include once "clsTopTabCtl.bi"
#include once "clsConfig.bi"
#include once "clsApp.bi"
#include once "clsSymbolDb.bi"
#include once "clsScanMgr.bi"
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
#include once "clsConfig.inc"
#include once "PsBufferPaint.inc"
#include once "modRoutines.inc"
' After modRoutines.inc: NavHistory_Goto drives OpenSelectedDocument.
#include once "modNavHistory.inc"
#include once "clsDocument.inc"
' Encoding conversion self-test. After modRoutines.inc (Doc_EncodeForDisk/GetFileToString)
' and clsDocument.inc (the clsDocument type it instantiates for the disk round-trip).
#include once "modEncodingSelfTest.bi"
#include once "modEncodingSelfTest.inc"
#include once "clsApp.inc"
#include once "clsSymbolDb.inc"
#include once "clsScanMgr.inc"
#include once "clsTopTabCtl.inc"
#include once "modAutoInsert.inc"
#include once "modCompile.inc"
#include once "modCompileErrors.inc"
#include once "modMenus.inc"
#include once "modCodetips.inc"
#include once "modMenuDefinitions.inc"
#include once "modMRU.inc"
' The Find in Project search engine and result model. No windows and no Ps* types, so it
' needs only clsApp/clsDocument/modScintilla, all already in scope. Ahead of
' modFindReplace.inc because the Find bar drives the project search.
#include once "modFindProject.inc"
#include once "modFindReplace.inc"
#include once "modFuzzy.inc"
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
#include once "frmBookmarks.inc" 
#include once "frmFunctions.inc"
#include once "frmAutoComplete.inc"
#include once "frmKeyboardEdit.bi"
#include once "frmKeyboardEdit.inc"
#include once "frmKeyboard.inc"
#include once "frmOutput.inc"
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
#include once "modMsgPump.inc"
#include once "frmMainFile.inc"
#include once "frmMainEdit.inc"
#include once "frmMainSearch.inc"
#include once "frmMainView.inc"
#include once "frmMainProject.inc"
#include once "frmMainCompile.inc"
#include once "frmMainDebug.inc"
#include once "frmDebug.inc"
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

    ' Load configuration files
    gConfig.LoadConfigFile()
    gConfig.LoadKeywords()

    
    ' Attempt to load the english localization file. This is necessary because
    ' any non-english localization file will have missing entries filled by the
    ' english version.
    dim as DWSTRING wszLocalizationFile
    wszLocalizationFile = AfxGetExePathName + wstr("settings\languages\english.lang")
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
    wszLocalizationFile = AfxGetExePathName + "settings\languages\" + gConfig.LocalizationFile
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
    wszFontFile = AfxGetExePathName + "\bin\SegoeFluentIcons.ttf"
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
    dim as any ptr pLibLexilla = dylibload("bin\Lexilla64.dll")
    dim as any ptr pLibScintilla = dylibload("bin\Scintilla64.dll")

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
    if len(wszFontFile) then RemoveFontResource(wszFontFile)

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

