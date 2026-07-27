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

#include once "modMenus.bi"
#include once "modContextMenus.bi" ' the standalone popup menus (implementation is
                                   ' included late in tiko.bas -- see that file)
#include once "PsVScrollBar.bi"    ' required by PsListTree.bi
#include once "PsColumnHeader.bi"  ' likewise: PsListTree.bi names HDR_* callback typedefs
#include once "PsListTree.bi"       ' PsListTree_GetSelCount (PsListTree.inc is included later)
#include once "PsMenuBar.bi"       ' the topmenu lives on PsMenuBar/PsPopupMenu controls
                                  ' (implementations are included later in tiko.bas)

' ========================================================================================
' EnableTopMenuItem: basically my internal version of the EnableMenuItem api.
' Drives the PsMenuBar dropdown trees directly: the by-id setter searches a dropdown
' and recurses into its submenus, so trying each bar dropdown covers every menu item.
' ========================================================================================
function EnableTopMenuItem( byval nID as long, byval bEnabled as boolean ) as long
    for i as long = 0 to PsMenuBar_GetItemCount( HWND_FRMMAIN_MENUBAR ) - 1
        dim as HWND hDropDown = PsMenuBar_GetDropDown( HWND_FRMMAIN_MENUBAR, i )
        if hDropDown then
            if PsPopupMenu_SetItemEnabled( hDropDown, nID, bEnabled ) then exit for
        end if
    next
    function = 0
end function


' ========================================================================================
' CheckMarkTopMenuItem: Show a checkmark to the left of the menu entry
' ========================================================================================
function CheckMarkTopMenuItem( byval nID as long, byval bChecked as boolean ) as long
    for i as long = 0 to PsMenuBar_GetItemCount( HWND_FRMMAIN_MENUBAR ) - 1
        dim as HWND hDropDown = PsMenuBar_GetDropDown( HWND_FRMMAIN_MENUBAR, i )
        if hDropDown then
            if PsPopupMenu_SetItemChecked( hDropDown, nID, bChecked ) then exit for
        end if
    next
    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_FileTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_FILE_START to IDM_FILE_END
        EnableTopMenuItem( idMenu, false )
    next

    ' Items that will always be enabled.
    EnableTopMenuItem( IDM_FILENEW, true )
    EnableTopMenuItem( IDM_FILEOPEN, true )
    EnableTopMenuItem( IDM_FILEOPENASTEMPLATE, true )
    EnableTopMenuItem( IDM_MRU, true )
    EnableTopMenuItem( IDM_LOADSESSION, true )
    EnableTopMenuItem( IDM_MRUSESSION, true )
    EnableTopMenuItem( IDM_KEYBOARDSHORTCUTS, true )
    EnableTopMenuItem( IDM_USERTOOLS, true )
    EnableTopMenuItem( IDM_USERTOOLSDIALOG, true )
    EnableTopMenuItem( IDM_OPTIONS, true )
    EnableTopMenuItem( IDM_OPTIONSDIALOG, true )
    EnableTopMenuItem( IDM_THEMES, true )
    EnableTopMenuItem( IDM_BUILDCONFIG, true )
    EnableTopMenuItem( IDM_EXIT, true )

    if gTTabCtl.GetItemCount() then
        for idMenu as long = IDM_FILECLOSE to IDM_MRUSESSION
            EnableTopMenuItem( idMenu, true )
        next
    end if

    if gApp.IsSessionActive = false then 
        EnableTopMenuItem( IDM_CLOSESESSION, false )
    end if

    if gApp.IsProjectActive then 
        EnableTopMenuItem( IDM_SAVESESSION, false )
    end if
            
    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_EditTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_EDIT_START to IDM_EDIT_END
        EnableTopMenuItem( idMenu, false )
    next

    ' Enable or disable specific edit menu items (based on the currently active document)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    ' A code editing window is active
    if len(pDoc->GetSelText) then
        EnableTopMenuItem( IDM_CUT, true )
        EnableTopMenuItem( IDM_COPY, true )
    end if
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla 
    if SendMessage( hEdit, SCI_CANUNDO, 0, 0) then EnableTopMenuItem( IDM_UNDO, true )
    if SendMessage( hEdit, SCI_CANREDO, 0, 0) then EnableTopMenuItem( IDM_REDO, true )
    if SendMessage( hEdit, SCI_CANPASTE, 0, 0) then EnableTopMenuItem( IDM_PASTE, true )
    for idMenu as long = IDM_DELETELINE to IDM_selectALL
        EnableTopMenuItem( idMenu, true )
    next 

    function = 0
end function
    

' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_SearchTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_SEARCH_START to IDM_SEARCH_END
        EnableTopMenuItem( idMenu, false )
    next

    ' if any documents exist in the editor then enable FindInProject
    if gApp.GetDocumentCount then EnableTopMenuItem( IDM_FINDINPROJECT, true )
        
    ' Enable or disable specific edit menu items (based on the currently active document)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    
    for idMenu as long = IDM_SEARCHSYMBOL to IDM_FINDPREVACCEL
        EnableTopMenuItem( idMenu, true )
    next

    for idMenu as long = IDM_REPLACE to IDM_GOTOLINE
        EnableTopMenuItem( idMenu, true )
    next

    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_ViewTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_VIEW_START to IDM_VIEW_END
        EnableTopMenuItem( idMenu, false )
    next

    EnableTopMenuItem( IDM_VIEWSIDEPANEL, true )
    EnableTopMenuItem( IDM_VIEWEXPLORER, true )
    EnableTopMenuItem( IDM_VIEWOUTPUT, true )
    EnableTopMenuItem( IDM_RESTOREMAIN, true )
    EnableTopMenuItem( IDM_EXPLORERPOSITION, true )

    CheckMarkTopMenuItem( IDM_VIEWSIDEPANEL, iif( IsWindowVisible(HWND_FRMPANEL), true, false ))
    CheckMarkTopMenuItem( IDM_VIEWEXPLORER, iif( IsWindowVisible(HWND_FRMEXPLORER), true, false ))
    CheckMarkTopMenuItem( IDM_VIEWOUTPUT, iif( IsWindowVisible(HWND_FRMOUTPUT), true, false ))
    CheckMarkTopMenuItem( IDM_FUNCTIONLIST, iif( IsWindowVisible(HWND_FRMFUNCTIONS), true, false ))
    CheckMarkTopMenuItem( IDM_BOOKMARKSLIST, iif( IsWindowVisible(HWND_FRMBOOKMARKS), true, false ))

    ' Enable or disable specific edit menu items (based on the currently active document)
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function
    
    EnableTopMenuItem( IDM_FUNCTIONLIST, true )
    EnableTopMenuItem( IDM_BOOKMARKSLIST, true )

    for idMenu as long = IDM_SPLITLEFTRIGHT to IDM_RESTOREMAIN
        EnableTopMenuItem( idMenu, true )
    next

    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_ProjectTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_PROJECT_START to IDM_PROJECT_END
        EnableTopMenuItem( idMenu, false )
    next

    EnableTopMenuItem( IDM_PROJECTNEW, true )
    EnableTopMenuItem( IDM_PROJECTOPEN, true )
    EnableTopMenuItem( IDM_MRUPROJECT, true )

    ' Check if a Project is open and active
    dim as long fEnabled = Iif( gApp.IsProjectActive, true, false )
    EnableTopMenuItem( IDM_PROJECTCLOSE, fEnabled )
    EnableTopMenuItem( IDM_PROJECTSAVEAS, fEnabled )
    EnableTopMenuItem( IDM_PROJECTSAVE, fEnabled )
    EnableTopMenuItem( IDM_PROJECTFILESADD, fEnabled )
    EnableTopMenuItem( IDM_PROJECTOPTIONS, fEnabled )

    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_CompileTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_COMPILE_START to IDM_COMPILE_END
        EnableTopMenuItem( idMenu, false )
    next

    dim as boolean fEnabled = iif( gApp.IsProjectActive orelse cbool(gTTabCtl.GetItemCount), true, false )
    EnableTopMenuItem( IDM_BUILDEXECUTE, fEnabled )
    EnableTopMenuItem( IDM_COMPILE, fEnabled )
    EnableTopMenuItem( IDM_REBUILDALL, fEnabled )
    EnableTopMenuItem( IDM_COMMANDLINE, fEnabled )

    if gApp.IsProjectActive then
        ' Enable the "Compile Module" menu item only if the current active file is a Module
        dim as clsDocument ptr pDoc = gTTabCtl.GetActiveDocumentPtr()
        if (pDoc <> null) andalso (pDoc->ProjectFileType = FILETYPE_MODULE) then
            EnableTopMenuItem( IDM_COMPILEMODULE, fEnabled )
        end if
    end if

    ' QuickRun should be enabled for non-Projects
    if gApp.IsProjectActive = false then 
        if gTTabCtl.GetItemCount then EnableTopMenuItem( IDM_QUICKRUN, true )
    end if
    
    ' Determine if an executable exists based on the current active file or project
    dim as DWSTRING wszRunFile = GetRunExecutableFilename()
    if AfxFileExists(wszRunFile) then EnableTopMenuItem( IDM_RUNEXE, true )
    
    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_DebugTopMenuStates() as long
    
    ' Start off by disabling everything in order to make things easier. 
    for idMenu as long = IDM_DEBUG_START to IDM_DEBUG_END
        EnableTopMenuItem( idMenu, false )
    next

    dim as boolean fEnabled = iif( gApp.IsProjectActive orelse cbool(gTTabCtl.GetItemCount), true, false )
    EnableTopMenuItem( IDM_DEBUG_STARTDEBUGGING, fEnabled )
    
    EnableTopMenuItem( IDM_DEBUG_STOPDEBUGGING, gApp.IsDebuggerActive )
    EnableTopMenuItem( IDM_DEBUG_STEPINTO, gApp.IsDebuggerActive )
    EnableTopMenuItem( IDM_DEBUG_STEPOVER, gApp.IsDebuggerActive )
    EnableTopMenuItem( IDM_DEBUG_STEPOUT, gApp.IsDebuggerActive )
    EnableTopMenuItem( IDM_DEBUG_RUNTOCURSOR, gApp.IsDebuggerActive )
    
    fEnabled = iif( cbool(gTTabCtl.GetItemCount), true, false )
    EnableTopMenuItem( IDM_DEBUG_TOGGLEBREAKPOINT, fEnabled )
    EnableTopMenuItem( IDM_DEBUG_DELETEALLBREAKPOINTS, fEnabled )

    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_HelpTopMenuStates() as long
    
    ' All Help menu items are always enabled
    for idMenu as long = IDM_HELP_START to IDM_HELP_END
        EnableTopMenuItem( idMenu, true )
    next

    function = 0
end function


' ========================================================================================
' Enable/Disable top menu items depending on application state
' ========================================================================================
function frmMain_ChangeTopMenuStates( byval nBarID as long ) as long

    select case nBarID
        case IDC_MENUBAR_FILE:     frmMain_FileTopMenuStates()
        case IDC_MENUBAR_EDIT:     frmMain_EditTopMenuStates()
        case IDC_MENUBAR_SEARCH:   frmMain_SearchTopMenuStates()
        case IDC_MENUBAR_VIEW:     frmMain_ViewTopMenuStates()
        case IDC_MENUBAR_PROJECT:  frmMain_ProjectTopMenuStates()
        case IDC_MENUBAR_COMPILE:  frmMain_CompileTopMenuStates()
        case IDC_MENUBAR_DEBUG:    frmMain_DebugTopMenuStates()
        case IDC_MENUBAR_HELP:     frmMain_HelpTopMenuStates()
    end select
    
    function = 0
end function


' ========================================================================================
' The standalone popup menus that used to be built here as HMENUs (and shown with
' TrackPopupMenu) live in modContextMenus.inc now, on the PsPopupMenu control. Gone with
' them: AddProjectFileTypesToMenu and the eleven Create*ContextMenu builders, plus two
' that had no callers at all (CreateBookmarksHeaderNodeContextMenu and
' CreateBookmarksBookmarkNodeContextMenu).
'
' What stays in this file is the TOPMENU state machinery -- EnableTopMenuItem /
' CheckMarkTopMenuItem and the per-menu frmMain_*TopMenuStates functions -- which already
' drives PsMenuBar/PsPopupMenu directly.
' ========================================================================================

