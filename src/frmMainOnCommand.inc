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
#include once "frmHelpViewer.bi"


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
    if gActiveMenuBarIndex > -1 then
        gActiveMenuBarIndex = -1
        AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )
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
            IDM_SAVESESSION, IDM_LOADSESSION,  _
            IDM_KEYBOARDSHORTCUTS, IDM_USERTOOLSDIALOG, IDM_OPTIONSDIALOG, IDM_BUILDCONFIG, _
            IDM_FIND, IDM_FINDINPROJECT, IDM_GOTOANYTHING, IDM_GOTOSYMBOL, _
            IDM_PROJECTOPEN, IDM_PROJECTSAVEAS, _
            IDM_PROJECTOPTIONS, IDM_COMMANDLINE, IDM_DEBUG_STARTDEBUGGING, IDM_ABOUT
            killAllPopupMenus()
    end select
    
    select case id
        ' show any topmenu that were activated via keyboard Alt+ sequences     
        case IDC_MENUBAR_FILE to IDC_MENUBAR_HELP
            killAllPopupMenus()
            gActiveMenuBarIndex = (id - IDC_MENUBAR_FILE)
            frmPopupMenu_Show( ID_POPUP, 0, HWND_FRMMAIN_MENUBAR, gMenuBar(gActiveMenuBarIndex).rcItem )
            setNextMenuItemTabIndex(false)
            AfxRedrawWindow( HWND_FRMMAIN_MENUBAR )
         
        case IDM_THEMES
            OptionsDialogLastOpened = 3
            frmOptions_Show( hwnd )
            
        case IDM_BUILDCONFIG_POPUP
            dim as HMENU hPopupMenu = CreateStatusBarBuildConfigContextMenu()
            dim as point pt: GetCursorPos @pt
            dim as long idMenu = TrackPopupMenu(hPopUpMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, byval null)
            DestroyMenu hPopUpMenu
            if (idMenu >= IDM_BUILDCONFIG_POPUP) andalso _
                (idMenu <= IDM_BUILDCONFIG_POPUP + ubound(gConfig.Builds)) then
                ' Set the active project or the active document to the selected build
                for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
                    if gConfig.Builds(i).idMenu = idMenu then 
                        If gApp.IsProjectActive then
                            gApp.ProjectBuild = gConfig.Builds(i).Id
                        else
                            dim pDocMain as clsDocument Ptr 
                            pDocMain = gTTabCtl.GetActiveDocumentPtr()
                            if pDocMain then pDocMain->DocumentBuild = gConfig.Builds(i).Id
                            gConfig.CompilerBuild = gConfig.Builds(i).Id
                        end if
                        frmMain_SetStatusbar
                        gConfig.SaveConfigFile
                        exit for
                    end if
                next
            end if
            return 0   

        case IDM_PROJECTFILETYPE
            dim as HMENU hPopupMenu = CreateTabsInfoFileTypeContextMenu()
            dim as point pt: GetCursorPos @pt
            TrackPopupMenu(hPopUpMenu, 0, pt.x, pt.y, 0, hwnd, byval null)
            DestroyMenu hPopUpMenu

        case IDM_FILEENCODING
            dim as HMENU hPopupMenu = CreateStatusBarFileEncodingContextMenu()
            dim as point pt: GetCursorPos @pt
            TrackPopupMenu(hPopUpMenu, TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, byval null)
            DestroyMenu hPopUpMenu
        
        case IDM_LINEENDINGS
            dim as HMENU hPopupMenu = CreateStatusBarLineEndingsContextMenu()
            dim as point pt: GetCursorPos @pt
            TrackPopupMenu(hPopUpMenu, TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, byval null)
            DestroyMenu hPopUpMenu

        case IDM_SPACES
            dim as HMENU hPopupMenu = CreateStatusBarSpacesContextMenu()
            dim as point pt: GetCursorPos @pt
            dim as long idMenu = TrackPopupMenu(hPopUpMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, byval null)
            DestroyMenu hPopUpMenu
            if (idMenu >= IDM_SPACES) andalso (idMenu <= IDM_SPACES + 8) then
                gConfig.TabSize = idMenu - IDM_SPACES
                gSBPanels(4).wszText = "Spaces: " & gConfig.TabSize
                frmStatusBar_PositionWindows
                dim pDoc as clsDocument ptr = gApp.pDocList
                do until pDoc = 0
                    pDoc->ApplyProperties
                    pDoc = pDoc->pDocNext
                loop
                return 0   
            end if
    
        case IDM_CLOSEPANEL
            ShowWindow( HWND_FRMPANEL, SW_HIDE )
            frmMain_PositionWindows()
            
        case IDM_OPENINCLUDE:           OpenSelectedDocument( gApp.IncludeFilename )
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
        case IDM_FILESAVE:           OnCommand_FileSave( hwnd, pDoc, pDoc->IsNewFlag, false )
        case IDM_FILESAVEAS:         OnCommand_FileSave( hwnd, pDoc, true, false )
        case IDM_FILERENAME:         OnCommand_FileRename( hwnd, pDoc )
        case IDM_FILECLOSE:          OnCommand_FileClose( hwnd, EFC_CLOSECURRENT )
        case IDM_FILESAVEALL:        OnCommand_FileSaveAll( hwnd ) 
        case IDM_LOADSESSION:        OnCommand_FileLoadSession( hwnd )   
        case IDM_SAVESESSION:        OnCommand_FileSaveSession( hwnd )   
        case IDM_CLOSESESSION:       OnCommand_FileCloseSession( hwnd )   
        case IDM_FILECLOSEALL:       OnCommand_FileClose( hwnd, EFC_CLOSEALL )
        case IDM_FILECLOSEALLOTHERS: OnCommand_FileClose( hwnd, EFC_CLOSEALLOTHERS )
        case IDM_CLOSEALLFORWARD:    OnCommand_FileClose( hwnd, EFC_CLOSEALLFORWARD )
        case IDM_CLOSEALLBACKWARD:   OnCommand_FileClose( hwnd, EFC_CLOSEALLBACKWARD )
        case IDM_KEYBOARDSHORTCUTS:  frmKeyboard_Show( hwnd )
        case IDM_USERTOOLSDIALOG:    frmUserTools_Show( hwnd ) 
        case IDM_OPTIONSDIALOG:      frmOptions_Show( hwnd )
        case IDM_BUILDCONFIG:        frmBuildConfig_Show( hwnd ) 
        case IDM_MRUCLEAR:           ClearMRUlist( id )
        case IDM_MRUSESSIONCLEAR:    ClearMRUlist( id )

        case IDM_MRUBASE to (IDM_MRUBASE + 10)
            OpenMRUFile( hwnd, id )

        case IDM_MRUSESSIONBASE to (IDM_MRUSESSIONBASE + 10)
            OpenMRUSessionFile( hwnd, id )

        case IDM_USERTOOLSBASE to (IDM_USERTOOLSBASE + ubound(gConfig.Tools))
            frmUserTools_ExecuteUserTool( null, id - IDM_USERTOOLSBASE )

        case IDM_BUILDCONFIGBASE to (IDM_BUILDCONFIGBASE + ubound(gConfig.Builds))
            ' Make sure that the incoming Tool Number is valid
            dim as long nBuildNum = id - IDM_BUILDCONFIGBASE
            if (nBuildNum < lbound(gConfig.Builds)) orelse (nBuildNum > ubound(gConfig.Builds)) then exit function
            ' Set the active project or the active document to the selected build
            If gApp.IsProjectActive then
                gApp.ProjectBuild = gConfig.Builds(nBuildNum).Id
            else
                dim pDocMain as clsDocument Ptr 
                pDocMain = gTTabCtl.GetActiveDocumentPtr()
                if pDocMain then pDocMain->DocumentBuild = gConfig.Builds(nBuildNum).Id
                gConfig.CompilerBuild = gConfig.Builds(nBuildNum).Id
            end if
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


        ''  SEARCH MENU  
        case IDM_GOTOANYTHING, _
             IDM_GOTOSYMBOL
             OnCommand_SearchGotoAnything( id )
        case IDM_FIND:               OnCommand_SearchFindDialog()
        case IDM_REPLACE:            OnCommand_SearchReplaceDialog()
        case IDM_FINDINPROJECT:      OnCommand_SearchFindInProject()
        case IDM_FINDNEXT, IDM_FINDPREV, _
            IDM_FINDNEXTACCEL, IDM_FINDPREVACCEL   ' F3/Shift+F3
            OnCommand_SearchFindActions( id, pDoc )
            
        case IDM_REPLACENEXT, IDM_REPLACEPREV, IDM_REPLACEALL
            OnCommand_SearchReplaceActions( id, pDoc )

        case IDM_DEFINITION:           OnCommand_SearchGotoDefinition( pDoc )
        case IDM_LASTPOSITION:         OnCommand_SearchGotoLastPosition()
        case IDM_GOTONEXTCOMPILEERROR: OnCommand_SearchGotoCompileError( true )
        case IDM_GOTOPREVCOMPILEERROR: OnCommand_SearchGotoCompileError( false )

        case IDM_GOTOHEADERFILE,  IDM_GOTOSOURCEFILE, IDM_GOTOMAINFILE, _ 
            IDM_GOTORESOURCEFILE, IDM_GOTOLINE,       IDM_GOTONEXTFUNCTION, _
            IDM_GOTOPREVFUNCTION, IDM_GOTONEXTTAB,    IDM_GOTOPREVTAB, _
            IDM_CLOSETAB
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
        case IDM_PROJECTNEW:            OnCommand_ProjectNew( hwnd )
        case IDM_PROJECTSAVE:           OnCommand_ProjectSave( hwnd, False )
        case IDM_PROJECTSAVEAS:         OnCommand_ProjectSave( hwnd, True )
        case IDM_PROJECTOPEN:           OnCommand_ProjectOpen( hwnd )
        case IDM_PROJECTFILESADD:       OnCommand_ProjectFilesAdd( hwnd )
        case IDM_PROJECTCLOSE:          OnCommand_ProjectClose( hwnd )

        case IDM_PROJECTOPTIONS:        frmProjectOptions_Show( hwnd, false ) 
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
            IDM_DEBUG_TOGGLEBREAKPOINT, IDM_DEBUG_DELETEALLBREAKPOINTS
            OnCommand_DebugCommon( id )

        ''   HELP MENU
        case IDM_HELP_FB
            ShowContextHelp(id) 
    
        case IDM_HELP_TIKO
            frmHelpViewer_Show( HWND_DESKTOP )

        case IDM_ABOUT
            DisableAllModeless()
            frmAbout_Show( hwnd )
            EnableAllModeless()
            frmMain_SetFocusToCurrentCodeWindow()
        
    end select

    function = 0
end function


