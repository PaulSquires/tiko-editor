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
#include once "clsDoubleBuffer.bi"
#include once "clsTopTabCtl.bi"
#include once "clsConfig.bi"
#include once "clsApp.bi"
#include once "clsSymbolDb.bi"
#include once "clsScanMgr.bi"
' Declared up here rather than beside its .inc because modCodetips.inc (which BUILDS the
' AUTOC_ITEM array) is included well before the frm* block.
#include once "frmAutoComplete.bi"

'  Global classes
dim shared gApp     as clsApp
dim shared gConfig  as clsConfig
dim shared gTTabCtl as clsTopTabCtl


#include once "modCWindow.inc"
#include once "modThemes.inc"
#include once "clsConfig.inc"
#include once "clsDoubleBuffer.inc"
#include once "modRoutines.inc"
#include once "clsDocument.inc"
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
#include once "modFindReplace.inc"
#include once "modFuzzy.inc"

' Custom controls
#include once "CVScrollBar.inc"
#include once "CHScrollBar.inc"
#include once "CColumnHeader.inc"
#include once "CListBox.inc"
#include once "CStatusBar.inc" 
#include once "CTabBar.inc"
#include once "CTextBox.inc"
#include once "CPopupMenu.inc"
#include once "CMenuBar.inc"
#include once "CSplitter.inc"
#include once "CIconPanel.inc"
#include once "CSelectBar.inc"

#include once "frmAbout.inc"
#include once "frmTopTabs.inc"
#include once "frmTopTabsMenu.inc"
#include once "frmTopTabsInfo.inc"
#include once "frmTopTabsFind.inc"
#include once "frmTopTabsReplace.inc"
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
#include once "frmKeyboardEdit.inc" 
#include once "frmKeyboard.inc" 
#include once "frmBuildConfig.inc" 
#include once "frmOutput.inc" 
#include once "frmOptionsGeneral.inc"
#include once "frmOptionsEditor.inc"
#include once "frmOptionsEditor2.inc"
#include once "frmOptionsColors.inc"
#include once "frmOptionsCompiler.inc"
#include once "frmOptionsLocal.inc"
#include once "frmOptionsKeywords.inc"
#include once "frmOptionsKeywordsWinApi.inc"
#include once "frmOptionsKeywordsExtra.inc"
#include once "frmOptions.inc"
#include once "frmGoto.inc"
#include once "frmHelpViewer.inc"
#include once "frmCommandLine.inc"
#include once "frmFindInProject.inc"
#include once "frmSearchSymbol.inc"
#include once "frmProjectOptions.inc"
#include once "frmMainOnCommand.inc"
#include once "frmMainOnNotify.inc"
#include once "frmUserTools.inc"
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
        MessageBox( 0, _
                    "English Localization file could not be loaded. Aborting application." + vbcrlf + _
                    wszLocalizationFile, _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        return 1
    end if
    
    
    ' Load the selected localization file
    wszLocalizationFile = AfxGetExePathName + "settings\languages\" + gConfig.LocalizationFile
    if LoadLocalizationFile(wszLocalizationFile, false) = false then
        MessageBox( 0, _
                    "Localization file could not be loaded." + vbcrlf + _
                    wszLocalizationFile, _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        return 1
    end if
    
    
    ' Load the Segoe Fluent Icons ttf file that is used for displaying the various
    ' icons used within the editor.
    dim as DWSTRING wszFontFile 
    wszFontFile = AfxGetExePathName + "\bin\SegoeFluentIcons.ttf"
    if AddFontResourceEx(wszFontFile.vptr, FR_PRIVATE, NULL) = 0 then
        MessageBox( 0, _
                    "Unable to load application font 'SegoeFluentIcons.ttf'. Aborting application." , _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
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
        MessageBox( 0, _
                    "Error loading Scintilla DLL's. Ensure C++ redistributable is installed:" + vbcrlf + _
                    "https://aka.ms/vs/17/release/vc_redist.x64.exe", _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        return 1
    end if
    gApp.pfnCreateLexerfn = cast(CreateLexerFn , GetProcAddress(pLibLexilla, "CreateLexer"))


    ' Load the HTML help library for displaying FreeBASIC help *.chm file
    gpHelpLib = dylibload( "hhctrl.ocx" )

    ' Load codetip files
    if gConfig.Codetips then gConfig.LoadCodetips

    ' Start the background fbcParser scan worker (owns all fbcParser.dll calls)
    gScanMgr.StartWorker()

    ' Show the main form
    function = frmMain_Show( 0 )


    ' Free the Scintilla, CaptureConsole and HTML help libraries
    dylibfree(pLibLexilla)
    dylibfree(pLibScintilla)
    dylibfree(gpHelpLib)
    
    ' Unload the font file
    if len(wszFontFile) then RemoveFontResource(wszFontFile)
    
    ' Uninitialize the COM library
    CoUninitialize

    LogClose()

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )

