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

' ----------------------------------------------------------------------------------------
' PsCore. THE TYPE SWAP HAS LANDED: `DWSTRING` here means PsCore's, everywhere, and both
' pieces of scaffolding that existed to postpone that are gone -- PsCompat.bi, whose Ps*
' functions were forwarders to fbc intrinsics and AfxStr, and `namespace PsC`, which existed
' only so two types called DWSTRING could be in scope at once.
'
' THE ORDER IS LOAD-BEARING, AND FOR A DIFFERENT REASON THAN BEFORE. These come AFTER
' AfxNova's headers deliberately: AfxNova also declares a DWSTRING, and the unqualified name
' means whichever was declared last. Moving this block up silently gives 1600 sites the other
' type, with no error anywhere -- see docs/port/type-swap-scope.md.
' ----------------------------------------------------------------------------------------
#include once "core/DWString.inc"
#include once "core/PsStr.inc"
#include once "core/PsPath.inc"
#include once "core/PsFile.inc"
#include once "core/PsEncoding.inc"

'' The way back from AfxNova, which is still linked and still owns the windows.
'' Its header is the argument for why one named function beats a cast at each site.
#include once "modAfxBridge.bi"

'' modScintilla.bi FIRST, and the ORDER IS LOAD-BEARING. tiko #Defines all 117
'' SCI_* constants and 19 SCK_*; PsPlatform declares them as `const` behind
'' #ifndef guards. Guards only work in this direction -- with PsScintilla.bi
'' first the const already exists and tiko's #Define becomes the duplicate,
'' which no guard on the library side can prevent.
#include once "app/modScintilla.bi"
'' Buffer reads and the whitespace strip. AFTER modScintilla.bi, which declares SciMsg and
'' the SC_* messages it uses, and this early because clsDocument's save path calls both.
#include once "app/modSciText.bi"

'' THE C BINDINGS GO OUTSIDE THE NAMESPACE, and this is not a style choice.
'' fbc mangles an `extern "C"` block declared inside a namespace as
'' PSC::bl_context_save, which matches nothing in libblend2d -- five undefined
'' references, at LINK time only, with a clean compile.
#include once "bind/Blend2D.bi"
#include once "bind/FreeType.bi"
#include once "bind/HarfBuzz.bi"
#include once "scintilla/PsScintilla.bi"

'' ----------------------------------------------------------------------------------------
'' `namespace PsC` SURVIVES THE TYPE SWAP, FOR THE OTHER JOB IT WAS DOING.
''
'' It was introduced to let two types called DWSTRING coexist. That reason is gone. But it
'' was also -- undocumented until the swap was attempted -- keeping PsCore's UI layer out of
'' tiko's: BOTH sides have a PsBufferPaint, and PsCore's paint backend and tiko's PsImage
'' both define PsBgrToArgb. Lifting these six headers to global scope produces 17
'' `Duplicated definition` errors and one `UDT's with methods must have unique names`, none
'' of which has anything to do with strings.
''
'' So the namespace stays and the PsC. prefixes in frmSciHost.* stay with it. The CORE
'' headers above are global, which is what makes DWSTRING one type again; only the UI is
'' fenced. Collapsing that fence is a rename job for whenever tiko's Ps* controls and
'' PsCore's converge, and it is not this phase.
'' ----------------------------------------------------------------------------------------
namespace PsC
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
' APPEXTENSION moved to app/modAppConstants.bi -- clsConfig's constructor builds
' UntitledProjectFilename out of it, and that constructor has to link without the shell.
' Included HERE, where the #define used to sit, so the effective order is unchanged for
' everything downstream. modDeclares.bi and frmDebug.bi include it too; #pragma once makes
' those no-ops and each names it so neither reads as a stray dependency.
#include once "app/modAppConstants.bi"
'' The app-host seam: how clsDocument and clsTopTabCtl ask the shell to make an editor view,
'' redraw one, or put a file dialog up, without naming Win32. Declaration and body together
'' and this early because the layer's own files reference gAppHost; the WIN32 BODIES that
'' fill it are a separate file, included far below where frmMain's functions exist.
#include once "app/modAppHost.bi"
#include once "app/modAppHost.inc"
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
'' Where a document's editor WINDOWS live. Must follow clsDocument.bi, which
'' declares the type it takes.
#include once "modDocViews.bi"
'' BEFORE modDeclares.bi, which is exactly where these declarations used to sit,
'' so the effective order is unchanged. Everything downstream that uses L() is
'' unaffected -- and the app layer can now reach it without the shell.
'' Whole-file text I/O on PsFile. Early, because modThemes and modIniParse use it
'' and are included well before clsConfig, where these started.
''
'' MOVED INTO app/ (2026-08-09). It was already 105 lines of pure PsCore with not one
'' AfxNova or Win32 token in it; it sat in src/ only because that is where it was written.
'' The shell binary needs it, and a file the app layer cannot reach is a file the shell
'' binary has to duplicate.
#include once "app/modSciText.inc"
#include once "app/modTextFile.inc"
#include once "app/modLocalization.bi"
'' The .lang loader, which fills the tables modLocalization.bi declares. It lived in
'' modRoutines.inc -- shell-side -- and that header said the app layer "needs to READ the
'' table, not to load it". True while the shell was the only binary. The shell BINARY has to
'' load it too, and the function was already PsCore-only, so it moved rather than being
'' duplicated. See app/modLocalization.inc.
#include once "app/modLocalization.inc"
'' The menu vocabulary. Before modDeclares.bi, which is where these declarations
'' used to live, so nothing downstream sees a different order.
'' app/modPaths.bi NAMED DIRECTLY. It was reached only through modRoutines.bi, a
'' SHELL header -- so an app-layer file that used FilenameOriginalCase or
'' ProcessFromCurdriveApp compiled inside tiko and not on its own, and the
'' standalone gate could not see the declaration at all. #pragma once makes the
'' existing include a no-op.
#include once "app/modPaths.bi"
#include once "app/modMenuIds.bi"
#include once "app/modMenuDefinitions.bi"
#include once "app/modAppState.bi"
#include once "modDeclares.bi"
' The 7c layout oracle. Declaration only and up here, because frmMain.inc calls it from the
' MSG_USER_PROCESS_COMMANDLINE handler; the body has to come in after frmMain.inc, since it
' drives frmMain_PositionWindows.
#include once "modLayoutDump.bi"
' The 7c PUMP oracle. Both halves up here and adjacent, unlike the layout oracle above: it is
' called from every message loop in the application, and the earliest of those is inside
' PsTextBox.bi -- so the bodies have to be in scope well before the frm* block. It depends on
' nothing but windows.bi, which is why it can sit this early.
#include once "modPumpTrace.bi"
#include once "modPumpTrace.inc"
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
'' The CONSTRUCTOR, and only the constructor. Immediately after its header because the header
'' carries `dim shared gConfig`, so including it instantiates the object and something has to
'' link the constructor. The other ~90 methods stay in the shell's clsConfig.inc at line 225.
#include once "app/clsConfig.inc"
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
' The Help Center: a URL builder and one ShellExecute since WebView2 was removed. It no
' longer needs to sit up here -- clsConfig no longer calls into it -- but the position is
' harmless and moving includes in this file has its own history.
#include once "frmHelpCenter.bi"
' Same reason: clsConfig.inc calls frmOutputFloat_IsFloating / _CaptureState from the
' frmOutput_CaptureState guard, and that runs from the top of SaveConfigFile. This header
' deliberately names no Ps* type, so it can sit here ahead of the whole Ps* block.
#include once "frmOutputFloat.bi"

'  Global classes
dim shared gApp     as clsApp
'' gConfig is declared in app/clsConfig.bi, beside its type.
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
#include once "modDocViews.inc"
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
#include once "app/modAppState.inc"
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
'' The DATA half first -- gKeys and the 112 defaults -- then the vocabulary that reads them.
'' app/modMenuDefinitions.inc walks gKeys for every menu accelerator label, so the array has
'' to be declared well before the frm* block either way; splitting it out only moved which
'' file says so. See app/modKeyBindings.bi.
#include once "app/modKeyBindings.inc"
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
'' MOVED OUT OF app/. It fills a PSTOOLTIP_COLORS, a type declared by
'' PsTooltip.bi, and its own header says it sits immediately after
'' PsTooltip.inc for that reason. That is UI code.
''
'' It was in app/ because the grep ratchet passed it -- no Win32 token appears
'' in it -- which is the weaker property the ratchet can check. The standalone
'' compile asked the real question and the answer was 17 errors, every one of
'' them PSTOOLTIP_COLORS.
#include once "modThemeTips.inc"
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
' AFTER frmMain.inc, not with the rest: it calls frmMain_PositionWindows and reads the child
' HWND globals, so it needs the whole shell in scope. Declared back at modLayoutDump.bi.
'' tiko's half of the app-host seam. HERE, near the end, because its bodies call
'' SciHost_Create, OnCommand_FileClose and the HWND_FRM* globals -- everything the shell
'' declares. app/modAppHost.* is the portable half and went in near the top.
'' The 401-line view styler, lifted out of clsDocument. Shell-side and staying: it is the
'' theme's, and the theme is not moving -- see its header. BEFORE modAppHostWin32.inc, which
'' is the only thing that calls it.
#include once "modViewStyle.inc"
#include once "modAppHostWin32.inc"
#include once "modLayoutDump.inc"


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

    ' The 7c pump oracle, armed before anything can open a window. HERE rather than in
    ' frmMain's message loop, which is where it was first put: the startup path can raise a
    ' message box -- a missing settings file, a failed theme load -- and those run their own
    ' pump before frmMain has one. Armed later, the trace would silently omit exactly the
    ' loops that ran earliest.
    PumpTrace_Init()

    ' The app-host seam, filled before anything can open a document. CHECKED rather than
    ' assumed: every field is required and there is no per-call fallback anywhere, so a
    ' missing one would surface as an empty editor or a silent no-op rather than an error.
    ' The check names the field, because "the host is incomplete" sends a reader through ten
    ' of them by hand.
    AppHostW32_Install()
    if AppHost_IsComplete() = false then
        MessageBoxW( 0, "AppHost." & AppHost_FirstMissing() & " is not set." & _
                        !"\n\nThis is a build error, not a configuration one.", _
                     "tiko", MB_OK or MB_ICONERROR )
        return 1
    end if
    ' BOTH records are checked, separately. One check over two records would pass a host
    ' that filled only one of them -- and "safely no-op-able" does not mean optional: a
    ' null field is a crash either way.
    if AppNotify_IsComplete() = false then
        MessageBoxW( 0, "AppNotify." & AppNotify_FirstMissing() & " is not set." & _
                        !"\n\nThis is a build error, not a configuration one.", _
                     "tiko", MB_OK or MB_ICONERROR )
        return 1
    end if

    ' ---- DLL SEARCH HARDENING, before anything can load a library --------------------
    ' By default LoadLibrary with a bare name searches the CURRENT DIRECTORY and then PATH.
    ' tiko is exposed to both: it loads Lexilla64/Scintilla64 by plain name, and
    ' code_Compile chdir's the whole process into whichever project is being built.
    '
    ' (WebView2Loader.dll used to be the third of these. It is gone with the Help Center's
    ' embedded pane -- see frmHelpCenter.bi.)
    '
    ' LOAD_LIBRARY_SEARCH_DEFAULT_DIRS drops the current directory and PATH, leaving the
    ' application directory, System32 and any explicitly added user directories -- which is
    ' where every DLL tiko actually wants already lives.
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
    if AddFontResourceEx(wszFontFile.Wz(), FR_PRIVATE, NULL) = 0 then
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
    dim as any ptr pLibLexilla   = dylibload( *(wszDllPath & "Lexilla64.dll").Wz() )
    dim as any ptr pLibScintilla = dylibload( *(wszDllPath & "Scintilla64.dll").Wz() )

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
    if PsLen(wszFontFile) then RemoveFontResource(*wszFontFile.Wz())

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

