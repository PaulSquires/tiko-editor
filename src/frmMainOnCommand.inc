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

#include once "frmMainOnCommand.bi"

#include once "frmOutput.bi"
#include once "clsApp.bi"
#include once "clsDocument.bi"
#include once "frmMain.bi"
#include once "frmFunctions.bi"


' ========================================================================================
' Route the standard editing commands to a focused text field rather than to the active
' document.
'
' Ctrl+Z / Ctrl+X / Ctrl+C / Ctrl+V are GLOBAL accelerators (modKeyBindings.inc), so the
' pump's TranslateAccelerator chain turns them into WM_COMMAND before the keystroke can
' reach whatever control has the focus -- and the handlers then act on
' pDoc->hWndActiveScintilla unconditionally. The result was that Ctrl+Z typed into the
' Output panel's Notes box, or into any PsTextBox on a dialog hosted by frmMain, silently
' undid an edit in the DOCUMENT behind it.
'
' The test is the focus window's CLASS, not a PsTextBox lookup: focus sits on the RichEdit
' child two levels down, and resolving its container would mean reading the UserData area
' of an arbitrary window (an unchecked GetWindowLongPtr -- see the registry note in
' PsOptionButton). The RichEdit answers WM_UNDO/WM_CUT/WM_COPY/WM_PASTE itself, and
' PsTextBox's subclass still gets first look, so numeric-mode paste vetting is unaffected.
' This deliberately covers the help viewer's RichEdit too -- undo and copy there were
' hitting the document just the same.
' ========================================================================================
private function frmMain_RouteEditToFocusedField( byval id as long ) as boolean

    dim as HWND hFocus = GetFocus()
    if hFocus = 0 then return false

    dim as wstring * 64 wszClass
    GetClassNameW( hFocus, @wszClass, 64 )
    if ucase(wszClass) <> ucase("RichEdit50W") then return false

    select case id
        case IDM_UNDO,  WM_UNDO  : SendMessageW( hFocus, WM_UNDO,  0, 0 )
        case IDM_CUT,   WM_CUT   : SendMessageW( hFocus, WM_CUT,   0, 0 )
        case IDM_COPY,  WM_COPY  : SendMessageW( hFocus, WM_COPY,  0, 0 )
        case IDM_PASTE, WM_PASTE : SendMessageW( hFocus, WM_PASTE, 0, 0 )
        case else                : return false
    end select

    return true
end function


' ========================================================================================
' Process WM_COMMAND message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT
            
    ' Deselect any active menubar item
    if PsMenuBar_GetActiveIndex( HWND_FRMMAIN_MENUBAR ) <> -1 then
        PsMenuBar_SetActiveIndex( HWND_FRMMAIN_MENUBAR, -1 )
    end if
    
    dim as HWND hEdit 

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    If pDoc Then 
        select case codeNotify
            case SCEN_SETFOCUS
            pDoc->hWndActiveScintilla = hwndCtl

            ' Remove any markers set in the document that highlights
            ' entire lines (used for Selected text searching).
            pDoc->RemoveMarkerHighlight()

            ' Restore any original selection area that the Find/Replace dialog may have 
            ' changed due to its incremental search, but only if it is a multiline selection.
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                if (pDoc->CurrentSelection.isInitialized) andalso _
                    (pDoc->CurrentSelection.endline > pDoc->CurrentSelection.startline) then
                    dim as any ptr pSci = pDoc->GetActiveScintillaPtr()
                    SciMsg( pSci, SCI_SETSELECTIONSTART, pDoc->CurrentSelection.startpos, 0) 
                    SciMsg( pSci, SCI_SETSELECTIONEND, pDoc->CurrentSelection.endpos, 0) 
                    if pDoc->CurrentSelection.startpos = pDoc->CurrentSelection.endpos then
                        SciMsg( pSci, SCI_SETEMPTYSELECTION, pDoc->CurrentSelection.endpos, 0) 
                    end if
                end if
                pDoc->CurrentSelection.isInitialized = false
            end if


            case SCEN_KILLFOCUS
            ' Save the current selection from the active document. 
            ' This is needed because Find/Replace does incremental searches
            ' that set the selection points when new matches are found. When the Find/Replace dialog
            ' and setfocus returns to the edit window then this saved position is restored.
            dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            if pDoc then
                if pDoc->CurrentSelection.isInitialized = false then
                    dim as long startline, endline, startpos, endpos
                    pDoc->GetSelectedLineRange(startLine, endLine, startPos, endPos )
                    pDoc->CurrentSelection.startline = startline
                    pDoc->CurrentSelection.endline = endline
                    pDoc->CurrentSelection.startpos = startpos
                    pDoc->CurrentSelection.endpos = endpos
                    pDoc->CurrentSelection.isInitialized = true
                end if
            end if

        end select      

        hEdit = pDoc->hWndActiveScintilla
    end if   

    ' Kill any currently displayed topmenu whenever a popup dialog is about to be activated
    select case id
        case IDM_FILEOPEN, IDM_FILEOPENASTEMPLATE, IDM_FILESAVEAS, IDM_FILERENAME, _
            IDM_KEYBOARDSHORTCUTS, IDM_USERTOOLSDIALOG, IDM_OPTIONSDIALOG, IDM_BUILDCONFIG, _
            IDM_FIND, IDM_FINDINPROJECT, IDM_SEARCHSYMBOL, _
            IDM_PROJECTOPEN, IDM_PROJECTSAVEAS, _
            IDM_PROJECTOPTIONS, IDM_COMMANDLINE, IDM_DEBUG_STARTDEBUGGING, IDM_ABOUT, _
            IDM_FORMATOPTIONS, IDM_FORMATPROJECT, IDM_FORMATALLDOCS
            killAllPopupMenus()
    end select
    
    ' A focused text field owns the editing commands. It has to be tested BEFORE the
    ' dispatch below, which hands them to the active document's Scintilla with no regard
    ' for where the focus actually is.
    if frmMain_RouteEditToFocusedField( id ) then return 0

    select case id
        ' show any topmenu that were activated via keyboard Alt+ sequences
        case IDC_MENUBAR_FILE to IDC_MENUBAR_HELP
            PsMenuBar_OpenMenu( HWND_FRMMAIN_MENUBAR, id - IDC_MENUBAR_FILE, true )
         
        case IDM_THEMES
            ' WAS: OptionsDialogLastOpened = 3 then frmOptions_Show. Page 3 had drifted to
            ' Compiler Setup as the page enum changed under it, so the statusbar's Themes
            ' panel opened the wrong page entirely -- a hardcoded index nobody re-checked.
            ' It opens the dialog that actually owns themes now.
            frmThemes_Show( hwnd )
            
        ' The five statusbar / tab-info popups. Each Show* rebuilds its items from
        ' current state and returns immediately -- the menus are PsPopupMenu controls, not
        ' TrackPopupMenu, so what used to be the "act on the result" tail of these cases
        ' now lives in the popups' select callbacks (modContextMenus.inc). Two of them
        ' HAD to move: IDM_BUILDCONFIG_POPUP and IDM_SPACES are both the command that
        ' opens the menu AND the base of their own item ids, so routing those results
        ' back through WM_COMMAND would re-open the menu.
        case IDM_BUILDCONFIG_POPUP
            ShowBuildConfigContextMenu()
            return 0

        case IDM_PROJECTFILETYPE
            ShowTabsInfoFileTypeContextMenu()

        case IDM_FILEENCODING
            ShowFileEncodingContextMenu()

        case IDM_LINEENDINGS
            ShowLineEndingsContextMenu()

        case IDM_SPACES
            ShowSpacesContextMenu()
            return 0

        case IDM_CLOSEPANEL
            ShowWindow( HWND_FRMPANEL, SW_HIDE )
            frmMain_PositionWindows()
            
        case IDM_OPENINCLUDE
            OpenSelectedDocument( gApp.IncludeFilename )
            LoadExplorerFiles()
            LoadFunctionsFiles()
            LoadBookmarksFiles()
            
        case IDM_EXPLORER_EXPANDALL:    frmExplorer_ExpandAll()   
        case IDM_EXPLORER_COLLAPSEALL:  frmExplorer_CollapseAll()   
        case IDM_FUNCTIONS_EXPANDALL:   frmFunctions_ExpandAll()   
        case IDM_FUNCTIONS_COLLAPSEALL: frmFunctions_CollapseAll()   
        case IDM_BOOKMARKS_EXPANDALL:   frmBookmarks_ExpandAll()   
        case IDM_BOOKMARKS_COLLAPSEALL: frmBookmarks_CollapseAll()   
        case IDM_FUNCTIONS_VIEWASTREE:  frmFunctions_ViewAsTree()
        case IDM_FUNCTIONS_VIEWASLIST:  frmFunctions_ViewAsList()


        ''  FILE MENU
        case IDM_FILENEW:            OnCommand_FileNew( hwnd )
        case IDM_FILEOPEN:           OnCommand_FileOpen( hwnd, true )
        case IDM_FILEOPENASTEMPLATE: OnCommand_FileOpen( hwnd, true, true )
            ' pDoc->IsNewFlag is dereferenced IN THE ARGUMENT LIST, so the callee's own null
            ' guard cannot help. Reachable with no active document: an accelerator bypasses
            ' menu enablement entirely.
        case IDM_FILESAVE
            ' On the Find in Project tab there is no active document, but the focused excerpt
            ' is a view onto a real one -- and it is the file the user has just been editing,
            ' so Ctrl+S has to save THAT.
            ' SaveTarget, not ActiveEdit: the latter answers only while an excerpt actually
            ' holds the caret, so Ctrl+S was a silent no-op everywhere else on this tab.
            if pDoc = 0 then
                dim as clsDocument ptr pFipSave
                if frmFindInProject_SaveTarget( pFipSave ) then pDoc = pFipSave
            end if
            if pDoc = 0 then exit function
            OnCommand_FileSave( hwnd, pDoc, pDoc->IsNewFlag, false )
        case IDM_FILESAVEAS:         OnCommand_FileSave( hwnd, pDoc, true, false )
        case IDM_FILERENAME:         OnCommand_FileRename( hwnd, pDoc )
        case IDM_FILEDUPLICATE:      OnCommand_FileDuplicate( hwnd, pDoc )
        case IDM_FILECLOSE:          OnCommand_FileClose( hwnd, EFC_CLOSECURRENT )
        case IDM_FILESAVEALL:        OnCommand_FileSaveAll( hwnd ) 
        case IDM_FILECLOSEALL
            ' CLOSE ALL EMPTIES AN UNTITLED WORKSPACE, and that needs saying here rather than
            ' being left to OnCommand_FileClose's per-document rule. That loop walks TABS, so
            ' a workspace document with no tab -- which is every file restored from a project
            ' with TabIndex=-1 -- is never visited and would survive "Close All Files",
            ' reappearing in the Explorer on the next launch.
            '
            ' A NAMED project is untouched: closing tabs must never silently drop project
            ' members. Removal there is "Remove from project".
            if OnCommand_FileClose( hwnd, EFC_CLOSEALL ) then
                if gApp.IsProjectNamed() = false then
                    gApp.RemoveAllDocuments()
                    LoadExplorerFiles()
                    LoadFunctionsFiles()
                end if
            end if
        case IDM_FILECLOSEALLOTHERS: OnCommand_FileClose( hwnd, EFC_CLOSEALLOTHERS )
        case IDM_CLOSEALLFORWARD:    OnCommand_FileClose( hwnd, EFC_CLOSEALLFORWARD )
        case IDM_CLOSEALLBACKWARD:   OnCommand_FileClose( hwnd, EFC_CLOSEALLBACKWARD )
        case IDM_KEYBOARDSHORTCUTS:  frmKeyboard_Show( hwnd )
        case IDM_USERTOOLSDIALOG:    frmUserTools_Show( hwnd ) 
        case IDM_OPTIONSDIALOG:      frmOptions_Show( hwnd )
        case IDM_FORMATOPTIONS:      frmFormatOptions_Show( hwnd )
        case IDM_BUILDCONFIG:        frmBuildConfig_Show( hwnd ) 
        case IDM_MRUCLEAR:           ClearMRUlist( id )

        case IDM_MRUBASE to (IDM_MRUBASE + 10)
            OpenMRUFile( hwnd, id )

        case IDM_USERTOOLSBASE to (IDM_USERTOOLSBASE + ubound(gConfig.Tools))
            frmUserTools_ExecuteUserTool( null, id - IDM_USERTOOLSBASE )

        case IDM_BUILDCONFIGBASE to (IDM_BUILDCONFIGBASE + ubound(gConfig.Builds))
            ' Make sure that the incoming Tool Number is valid
            dim as long nBuildNum = id - IDM_BUILDCONFIGBASE
            if (nBuildNum < lbound(gConfig.Builds)) orelse (nBuildNum > ubound(gConfig.Builds)) then exit function
            ' The build belongs to the workspace; gConfig.CompilerBuild only seeds a new one.
            gApp.ProjectBuild = gConfig.Builds(nBuildNum).Id
            frmMain_SetStatusbar
            gConfig.SaveConfigFile

        case IDM_EXIT
            SendMessage( hwnd, WM_CLOSE, 0, 0 )


        ''  EDIT MENU
        case IDM_CUT, WM_CUT:        OnCommand_EditCut( pDoc, hEdit )
        case IDM_COPY, WM_COPY:      OnCommand_EditCopy( pDoc, hEdit )
        case IDM_PASTE, WM_PASTE:    OnCommand_EditPaste( pDoc, hEdit )
        case IDM_SELECTALL:          OnCommand_EditSelectAll( pDoc, hEdit )
        case IDM_INDENTBLOCK:        OnCommand_EditIndentBlock( pDoc, hEdit )
        case IDM_UNINDENTBLOCK:      OnCommand_EditUnIndentBlock( pDoc, hEdit )

        case IDM_ANSI, IDM_UTF8, IDM_UTF8BOM, IDM_UTF16BOM
            OnCommand_EditEncoding( id, pDoc )

        case IDM_DELETELINE,    IDM_INSERTFILE,     IDM_COMMENTBLOCK, _
            IDM_UNCOMMENTBLOCK, IDM_DUPLICATELINE,  IDM_MOVELINEUP, _
            IDM_MOVELINEDOWN,   IDM_TOUPPERCASE,    IDM_NEWLINEBELOWCURRENT, _
            IDM_TOLOWERCASE,    IDM_TOMIXEDCASE,    IDM_EOLTOCRLF, _
            IDM_EOLTOLF,        IDM_TABSTOSPACES, _
            IDM_SELECTLINE,     IDM_REDO,           IDM_UNDO, _
            WM_UNDO
            OnCommand_EditCommon( id, pDoc )

        ''  CODE FORMATTER
        case IDM_FORMATDOCUMENT:     OnCommand_FormatDocument()
        case IDM_FORMATSELECTION:    OnCommand_FormatSelection()
        case IDM_FORMATALLDOCS:      OnCommand_FormatAllOpenDocuments()
        case IDM_FORMATPROJECT:      OnCommand_FormatProject()


        ''  SEARCH MENU  
        case IDM_SEARCHSYMBOL:       OnCommand_SearchSymbol()
        case IDM_FIND:               OnCommand_SearchFindDialog()
        case IDM_REPLACE:            OnCommand_SearchReplaceDialog()
        case IDM_FINDINPROJECT:      OnCommand_SearchFindInProject()
        case IDM_FINDNEXT, IDM_FINDPREV, _
            IDM_FINDNEXTACCEL, IDM_FINDPREVACCEL   ' F3/Shift+F3
            OnCommand_SearchFindActions( id, pDoc )
            
        case IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
            OnCommand_SearchReplaceActions( id, pDoc )

        case IDM_GOTODEFINITION:       OnCommand_SearchGotoDefinition( pDoc )
        case IDM_GOBACK:               NavHistory_Back()
        case IDM_GOFORWARD:            NavHistory_Forward()
        case IDM_GOTONEXTCOMPILEERROR: OnCommand_SearchGotoCompileError( true )
        case IDM_GOTOPREVCOMPILEERROR: OnCommand_SearchGotoCompileError( false )

        case IDM_GOTOHEADERFILE,  IDM_GOTOSOURCEFILE,   IDM_GOTOMAINFILE, _
            IDM_GOTORESOURCEFILE, IDM_GOTONEXTFUNCTION, IDM_GOTOPREVFUNCTION, _
            IDM_GOTONEXTTAB,      IDM_GOTOPREVTAB,      IDM_CLOSETAB
            OnCommand_SearchGotoFile( id, pDoc )
            
        case IDM_BOOKMARKTOGGLE,   IDM_BOOKMARKNEXT,   IDM_BOOKMARKPREV, _
            IDM_BOOKMARKCLEARALL, IDM_BOOKMARKCLEARALLDOCS
            OnCommand_SearchBookmarks( id, pDoc )
        
        
        ''  VIEW MENU
        case IDM_VIEWSIDEPANEL:      OnCommand_ViewSidePanel():
        case IDM_VIEWEXPLORER:       OnCommand_ViewExplorer():
        case IDM_VIEWOUTPUT:         OnCommand_ViewOutput():
        case IDM_FUNCTIONLIST:       OnCommand_ViewFunctionList():
        case IDM_BOOKMARKSLIST:      OnCommand_ViewBookmarksList():
        case IDM_VIEWTODO:           OnCommand_ViewToDo():
        case IDM_VIEWNOTES:          OnCommand_ViewNotes():
        case IDM_RESTOREMAIN:        OnCommand_ViewRestoreMain():
        case IDM_EXPLORERPOSITION:   OnCommand_ViewExplorerPosition()
            
        case IDM_FOLDTOGGLE, IDM_FOLDBELOW, IDM_FOLDALL, IDM_UNFOLDALL
            OnCommand_ViewFold( id, pDoc )

        case IDM_ZOOMIN, IDM_ZOOMOUT, IDM_ZOOMRESET
            OnCommand_ViewZoom( id, pDoc )

        case IDM_SPLITLEFTRIGHT, IDM_SPLITTOPBOTTOM
            OnCommand_ViewSplit( id, pDoc )
        
        case IDM_SETFOCUSEDITOR:
            ' Default is Ctrl+tilde
            OnCommand_ViewToggleFocus( pDoc )

        
        ''  PROJECT MENU
        ''
        ''  THE FIVE PROJECT-ONLY COMMANDS ARE GUARDED HERE AS WELL AS GREYED IN THE MENU
        ''  (frmMain_ProjectTopMenuStates). A greyed PsMenuBar row is drawn state, not a
        ''  gate: an accelerator goes straight to WM_COMMAND and never passes the menu at
        ''  all, so without this Ctrl+F11 would add files to a workspace whose "Add Files
        ''  to Project..." row was sitting there disabled. Every one of the five is
        ''  user-bindable in the Keyboard dialog -- four merely ship with no default -- so
        ''  the guard is the set, not just the one that has a key today.
        case IDM_PROJECTNEW:            OnCommand_ProjectNew( hwnd )
        case IDM_PROJECTOPEN:           OnCommand_ProjectOpen( hwnd )
        case IDM_PROJECTSAVE:           if gApp.IsProjectNamed() then OnCommand_ProjectSave( hwnd, False )
        case IDM_PROJECTSAVEAS:         if gApp.IsProjectNamed() then OnCommand_ProjectSave( hwnd, True )
        case IDM_PROJECTFILESADD:       if gApp.IsProjectNamed() then OnCommand_ProjectFilesAdd( hwnd )
        case IDM_PROJECTCLOSE:          if gApp.IsProjectNamed() then OnCommand_ProjectClose( hwnd )

        case IDM_PROJECTOPTIONS:        if gApp.IsProjectNamed() then frmProjectOptions_Show( hwnd, false )
        case IDM_MRUPROJECTCLEAR:       ClearMRUlist( id )
        case IDM_REMOVEFILEFROMPROJECT: OnCommand_ProjectRemove( id, pDoc )
                
        case IDM_SETFILENORMAL, IDM_SETFILEMODULE, IDM_SETFILEMAIN, _
            IDM_SETFILERESOURCE, IDM_SETFILEHEADER
            OnCommand_ProjectSetFileType( id, pDoc )
            
        case IDM_MRUPROJECTBASE to (IDM_MRUPROJECTBASE + 10)
            OpenMRUProjectFile( hwnd, id )

        ''  COMPILE MENU          
        case IDM_BUILDEXECUTE, IDM_COMPILE, IDM_REBUILDALL, IDM_COMPILEMODULE, IDM_QUICKRUN, _
            IDM_RUNEXE, IDM_COMMANDLINE
            OnCommand_CompileCommon( id )

        ''  DEBUG MENU          
        case IDM_DEBUG_STARTDEBUGGING, IDM_DEBUG_STOPDEBUGGING, _
            IDM_DEBUG_STEPINTO, IDM_DEBUG_STEPOVER, IDM_DEBUG_STEPOUT, IDM_DEBUG_RUNTOCURSOR, _
            IDM_DEBUG_TOGGLEBREAKPOINT, IDM_DEBUG_DELETEALLBREAKPOINTS, IDM_DEBUG_UNUSEDSYMBOLS         
            OnCommand_DebugCommon( id )

        ''   HELP MENU
        case IDM_HELP_CENTER
            ' F1. The search term is the SELECTION if there is one, otherwise the word
            ' under the caret -- so F1 works with nothing selected, which is the ordinary
            ' way to ask about the identifier you are looking at. (GetWord is what the old
            ' .chm ShowContextHelp used for the same reason.)
            '
            ' The selection wins because it is the more explicit of the two: a user who
            ' has selected "PsListTree_SetFont" means that, not the word the caret happens
            ' to sit in. Both may be "" -- no document, or the caret on whitespace -- and
            ' that is a legal outcome: the window opens with the search box focused and
            ' empty.
            dim pDocHelp as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
            dim as DWSTRING wszHelpSel
            if pDocHelp then
                wszHelpSel = HelpCenter_NormalizeQuery( pDocHelp->GetSelText() )
                if len( wszHelpSel ) = 0 then
                    ' The fallback is filtered, the selection is not -- see
                    ' HelpCenter_IsSearchableWord. GetWord walks punctuation when the
                    ' caret is not in a word, and searching for "'" reads as broken.
                    dim as DWSTRING wszWord = HelpCenter_NormalizeQuery( pDocHelp->GetWord() )
                    if HelpCenter_IsSearchableWord( wszWord ) then wszHelpSel = wszWord
                end if
            end if
            ' POSTED, not called: see MSG_USER_SHOW_HELPCENTER in modDeclares.bi. Opening
            ' the window here works, and then the menu click that got us here re-activates
            ' frmMain and buries it.
            gHelpPendingQuery = wszHelpSel
            gHelpPendingSite  = HELPCENTER_SITE_DOCS
            PostMessage( HWND_FRMMAIN, MSG_USER_SHOW_HELPCENTER, 0, 0 )

        case IDM_HELP_TIKO
            ' tiko's OWN documentation, in the same window. No search term is passed and
            ' none would be used: this site carries its own search (Ctrl+K or "/") rather
            ' than the input#q the generated site puts in every page header, so the term
            ' would have nowhere to land. It is also the wrong question -- "help on the
            ' identifier under the caret" is what F1 is for.
            gHelpPendingQuery = ""
            gHelpPendingSite  = HELPCENTER_SITE_TIKO
            PostMessage( HWND_FRMMAIN, MSG_USER_SHOW_HELPCENTER, 0, 0 )

        case IDM_ABOUT
            frmAbout_Show( hwnd )
            frmMain_SetFocusToCurrentCodeWindow()
        
    end select

    function = 0
end function


