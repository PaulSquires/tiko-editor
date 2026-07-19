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

' ========================================================================================
' EnableTopMenuItem: basically my internal version of the EnableMenuItem api
' ========================================================================================
function EnableTopMenuItem( byval nID as long, byval bEnabled as boolean ) as long
    dim p as TOPMENU_TYPE ptr = getTopMenuPtr(nID)
    if p = 0 then exit function
    if bEnabled = false then 
        p->isDisabled = true
    else   
        p->isDisabled = false
    end if
    function = 0
end function


' ========================================================================================
' CheckMarkTopMenuItem: Show a checkmark to the left of the menu entry
' ========================================================================================
function CheckMarkTopMenuItem( byval nID as long, byval bChecked as boolean ) as long
    dim p as TOPMENU_TYPE ptr = getTopMenuPtr(nID)
    if p = 0 then exit function
    p->isChecked = bChecked
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
function frmMain_ChangeTopMenuStates() as long
    
    select case gMenuBar(gActiveMenuBarIndex).id
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


''
''
function AddProjectFileTypesToMenu( _
            byval hPopUpMenu as HMENU, _
            byval pDoc as clsDocument ptr, _
            byval fSeparator as boolean, _
            byval fExplorerTreeview as boolean _
            ) as long

    dim as HMENU hPopUpSubMenu
    dim as long msg, msgfirst, msglast
    
    if fSeparator then AppendMenu( hPopUpMenu, MF_SEPARATOR, 0, "" )

    msg = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
    msgfirst = msg
    AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, msg, L(206,"Main file") 
    msg = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
    AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, msg, L(181,"Header file") 

    if gApp.IsProjectActive then
        msg = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, msg, L(205,"Module file") 
        msg = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, msg, L(209,"Resource file") 
    end if

    msg = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
    msglast = msg
    AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, msg, L(204,"Normal file") 
    
    dim nFileType as long
    select case pDoc->ProjectFileType
        case FILETYPE_MAIN:     nFileType = iif( fExplorerTreeview, IDM_SETFILEMAIN_EXPLORERTREEVIEW, IDM_SETFILEMAIN )
        case FILETYPE_RESOURCE: nFileType = iif( fExplorerTreeview, IDM_SETFILERESOURCE_EXPLORERTREEVIEW, IDM_SETFILERESOURCE )
        case FILETYPE_HEADER:   nFileType = iif( fExplorerTreeview, IDM_SETFILEHEADER_EXPLORERTREEVIEW, IDM_SETFILEHEADER )
        case FILETYPE_MODULE:   nFileType = iif( fExplorerTreeview, IDM_SETFILEMODULE_EXPLORERTREEVIEW, IDM_SETFILEMODULE )
        case FILETYPE_NORMAL:   nFileType = iif( fExplorerTreeview, IDM_SETFILENORMAL_EXPLORERTREEVIEW, IDM_SETFILENORMAL )
    end select

    ' Some of our available choices are affected by the fact that multiple selections
    ' in the explorer listbox may exist. In that case, we do a default FileType selection
    ' only when the selection count is 1
    dim as long nSelCount = SendMessage( HWND_FRMEXPLORER_LISTBOX, LB_GETSELCOUNT, 0, 0 )
    dim as long bAllowCheckMark = true
    if (fExplorerTreeview = true) and (nSelCount > 1) then bAllowCheckMark = false 
    if bAllowCheckMark then
        CheckMenuRadioItem( hPopupMenu, msgfirst, msglast, nFileType, MF_BYCOMMAND )
    end if
    
    function = 0
end function


''
''
function CreateTabsInfoFileTypeContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        hPopUpMenu = CreatePopupMenu()
        AddProjectFileTypesToMenu( hPopUpMenu, pDoc, false, false ) 
    end if
    function = hPopupMenu
end function


''
''
function CreateStatusBarBuildConfigContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    
    dim as long curSelected
    dim as DWSTRING wszCurrentBuild = frmBuildConfig_GetSelectedBuildDescription()
    
    hPopUpMenu = CreatePopupMenu()
    for i as long = lbound(gConfig.Builds) to ubound(gConfig.Builds)
        if gConfig.Builds(i).ExcludeInPopup then continue for
        dim as long idMenu = IDM_BUILDCONFIG_POPUP + i
        dim as DWSTRING wszCaption = gConfig.Builds(i).wszDescription
        if wszCaption = wszCurrentBuild then curSelected = idMenu
        gConfig.Builds(i).idMenu = idMenu
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, idMenu, wszCaption
    next

    CheckMenuRadioItem( hPopupMenu, IDM_BUILDCONFIG_POPUP, _
            IDM_BUILDCONFIG_POPUP + ubound(gConfig.Builds), curSelected, MF_BYCOMMAND )

    function = hPopupMenu
end function


''
''
function CreateStatusBarFileEncodingContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        hPopUpMenu = CreatePopupMenu()
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, IDM_ANSI, "ANSI"
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, IDM_UTF8, "UTF-8"
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, IDM_UTF8BOM, "UTF-8 (BOM)"
        AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, IDM_UTF16BOM, "UTF-16 (BOM)"

        dim nEncoding as long
        select case pDoc->FileEncoding
            case FILE_ENCODING_ANSI:      nEncoding = IDM_ANSI
            case FILE_ENCODING_UTF8:      nEncoding = IDM_UTF8
            case FILE_ENCODING_UTF8_BOM:  nEncoding = IDM_UTF8BOM
            case FILE_ENCODING_UTF16_BOM: nEncoding = IDM_UTF16BOM
        end select
        CheckMenuRadioItem( hPopupMenu, IDM_ANSI, IDM_UTF16BOM, nEncoding, MF_BYCOMMAND )
    end if

    function = hPopupMenu
end function


''
''
function CreateStatusBarSpacesContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        hPopUpMenu = CreatePopupMenu()
        for i as long = 0 to 8
            dim as long idMenu = IDM_SPACES + i
            dim as DWSTRING wszCaption = str(i)
            if i = val(gConfig.TabSize) then wszCaption = wszCaption & "  (Current Tab Size)"
            AppendMenu hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, idMenu, wszCaption
        next
        CheckMenuRadioItem( hPopupMenu, IDM_SPACES, IDM_SPACES+8, IDM_SPACES+gConfig.TabSize, MF_BYCOMMAND )
    end if

    function = hPopupMenu
end function



''
''
function CreateStatusBarLineEndingsContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    
    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc then
        hPopUpMenu = CreatePopupMenu()
        AppendMenu hPopUpMenu, MF_ENABLED, IDM_EOLTOCRLF, L(36,"Convert to CRLF")
        AppendMenu hPopUpMenu, MF_ENABLED, IDM_EOLTOLF, L(38,"Convert to LF")
    end if

    function = hPopupMenu
end function

''
''
function CreateTopTabCtlContextMenu( byval idx as long ) as HMENU
    dim hPopUpMenu as HMENU
    dim hPopUpSubMenu as HMENU
    dim as DWSTRING wszText
    dim as long CuriTab = gTTabCtl.GetCurSel()
    dim as long MaxiTab = gTTabCtl.GetItemCount - 1

    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILECLOSE,  L(5,"Close") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVE,   L(7,"Save") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVEAS, L(8,"Save As") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILERENAME, L(21,"Rename") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILEDUPLICATE, L(22,"Duplicate") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopUpMenu, iif(MaxiTab > 0, MF_ENABLED,MF_DISABLED), IDM_FILECLOSEALL, L(6,"Close All")
    AppendMenu hPopUpMenu, iif(MaxiTab > 0, MF_ENABLED,MF_DISABLED), IDM_FILECLOSEALLOTHERS, L(354,"Close Others")
    AppendMenu hPopUpMenu, iif(CuriTab > 0, MF_ENABLED,MF_DISABLED), IDM_CLOSEALLBACKWARD, L(355,"Close Tabs on the Left")
    AppendMenu hPopUpMenu, iif(MaxiTab > CuriTab, MF_ENABLED,MF_DISABLED), IDM_CLOSEALLFORWARD, L(356,"Close Tabs on the Right") 

    dim pDoc as clsDocument ptr = gTTabCtl.GetDocumentPtr(idx)
    if pDoc then
        if gApp.IsProjectActive = true then
            AppendMenu hPopUpMenu, MF_SEPARATOR, 0, ""
            wszText = L(190, "Remove from project")
            AppendMenu hPopUpMenu, MF_ENABLED, IDM_REMOVEFILEFROMPROJECT, wszText
        end if
        AddProjectFileTypesToMenu(hPopUpMenu, pDoc, true, false)
    end if

    function = hPopupMenu
end function


''
''
function CreateScintillaContextMenu() as HMENU

    dim pDoc as clsDocument ptr = gTTabCtl.GetActiveDocumentPtr()
    if pDoc = 0 then exit function

    dim hPopupMenu    as HMENU
    dim hPopupSubMenu as HMENU
    dim pt            as point
    
    dim as HWND hEdit = pDoc->hWndActiveScintilla

    ' Ensure that the window under the cursor is actually a Scintilla window
    ' and not another window like a tab in the top tabcontrol.
    GetCursorPos(@pt)
    ScreenToClient(HWND_FRMMAIN, @pt)

    if RealChildWindowFrompoint(HWND_FRMMAIN, pt) <> hEdit then exit function
    
    hPopupMenu = CreatePopupMenu()

    ' Determine if the text under the cursor is an #Include file. if yes, then add a
    ' menu option to allow to open it.
    if IsCurrentLineIncludeFilename() then
        AppendMenu hPopupMenu, MF_ENABLED, IDM_OPENINCLUDE, L(75,"Open") + chr(32, 34) + gApp.IncludeFilename + chr(34)
        AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    end if
    
    AppendMenu hPopupMenu, MF_ENABLED, IDM_UNDO, L(15,"Undo")
    AppendMenu hPopupMenu, MF_ENABLED, IDM_REDO, L(16,"Redo")
    AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopupMenu, MF_ENABLED, IDM_CUT, L(17,"Cut")
    AppendMenu hPopupMenu, MF_ENABLED, IDM_COPY, L(18,"Copy")
    AppendMenu hPopupMenu, MF_ENABLED, IDM_PASTE, L(19,"Paste")
    AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopupMenu, MF_ENABLED, IDM_DEBUG_TOGGLEBREAKPOINT, L(313,"Toggle Breakpoint") 
    if gApp.IsDebuggerActive = true then
        AppendMenu hPopupMenu, MF_ENABLED, IDM_DEBUG_RUNTOCURSOR, L(320,"Run to Cursor") 
    end if
    AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopupMenu, MF_ENABLED, IDM_INDENTBLOCK, L(23,"Block Indent") 
    AppendMenu hPopupMenu, MF_ENABLED, IDM_UNINDENTBLOCK, L(24,"Block Unindent")
    AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopupMenu, MF_ENABLED, IDM_COMMENTBLOCK, L(25,"Block Comment") 
    AppendMenu hPopupMenu, MF_ENABLED, IDM_UNCOMMENTBLOCK, L(26,"Block Uncomment")
    AppendMenu hPopupMenu, MF_SEPARATOR, 0, ""
    AppendMenu hPopupMenu, MF_ENABLED, IDM_SELECTALL, L(40,"Select All")

    function = hPopupMenu

end function


''
''
function CreateExplorerContextMenu(byval pDoc as clsDocument ptr ) as HMENU
    
    dim wszText as wstring * MAX_PATH
    dim hPopUpMenu as HMENU = CreatePopupMenu()

    ' Some of our available chocies are affected by the fact that multiple selections
    ' in the explorer listbox may exist. In that case, we allow options like "Close"
    ' and also don't set a selected FileType.
    dim as long nSelCount = SendMessage( HWND_FRMEXPLORER_LISTBOX, LB_GETSELCOUNT, 0, 0 )

    ' if pDoc is zero then most likely we have right clicked on a Project or FILES root node
    if pDoc then
        if gApp.IsProjectActive then
            AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILEOPEN_EXPLORERLISTBOX,   L(75,"Open") 
            AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
            if (gTTabCtl.GetTabIndexByDocumentPtr( pDoc ) <> -1) orelse (nSelCount > 1) then
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILECLOSE_EXPLORERLISTBOX,  L(5,"Close") 
                AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
            end if
            if nSelCount = 1 then
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVE_EXPLORERLISTBOX,   L(7,"Save") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVEAS_EXPLORERLISTBOX, L(8,"Save As") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILERENAME_EXPLORERLISTBOX, L(21,"Rename") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILEDUPLICATE_EXPLORERLISTBOX, L(22,"Duplicate") 
                AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
            end if
            wszText = L(190,"Remove from project") 
            AppendMenu( hPopUpMenu, MF_ENABLED, IDM_REMOVEFILEFROMPROJECT_EXPLORERLISTBOX, wszText )
        Else
            AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILECLOSE_EXPLORERLISTBOX,  L(5,"Close") 
            if nSelCount = 1 then
                AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVE_EXPLORERLISTBOX,   L(7,"Save") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILESAVEAS_EXPLORERLISTBOX, L(8,"Save As") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILERENAME_EXPLORERLISTBOX, L(21,"Rename") 
                AppendMenu hPopUpMenu, MF_ENABLED, IDM_FILEDUPLICATE_EXPLORERLISTBOX, L(22,"Duplicate") 
            end if
        end if
        AddProjectFileTypesToMenu(hPopUpMenu, pDoc, true, true)
    end if

    function = hPopupMenu
    
end function

''
''
function CreateExplorerActionButtonContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    
    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_EXPLORER_EXPANDALL, L(441,"Expand All") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_EXPLORER_COLLAPSEALL, L(442,"Collapse All") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_CLOSEPANEL, L(161,"Close") 

    function = hPopupMenu
end function

''
''
function CreateFunctionsActionButtonContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    
    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FUNCTIONS_EXPANDALL, L(441,"Expand All") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FUNCTIONS_COLLAPSEALL, L(442,"Collapse All") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FUNCTIONS_VIEWASTREE, L(445,"View as Tree") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_FUNCTIONS_VIEWASLIST, L(446,"View as List") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_CLOSEPANEL, L(161,"Close") 

    function = hPopupMenu
end function

''
''
function CreateBookmarksActionButtonContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    
    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_BOOKMARKS_EXPANDALL, L(441,"Expand All") 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_BOOKMARKS_COLLAPSEALL, L(442,"Collapse All") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_BOOKMARKCLEARALLDOCS, L(53,"Clear Bookmarks") 
    AppendMenu hPopUpMenu, MF_SEPARATOR, 0, "" 
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_CLOSEPANEL, L(161,"Close") 

    function = hPopupMenu
end function

''
''
function CreateBookmarksHeaderNodeContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    
    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_CLEARALLBOOKMARKNODE, L(53,"Clear Bookmarks") 

    function = hPopupMenu
end function

''
''
function CreateBookmarksBookmarkNodeContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    
    hPopUpMenu = CreatePopupMenu()
    AppendMenu hPopUpMenu, MF_ENABLED, IDM_REMOVEBOOKMARKNODE, L(444,"Remove Bookmark") 

    function = hPopupMenu
end function

''
''
function CreateTopTabsActionButtonContextMenu() as HMENU
    dim hPopUpMenu as HMENU
    dim as DWSTRING wszCaption
    ' List all of the Top Tab open tabs in the popup menu. Simply use the array
    ' index as the ID because the calling program will use this ID to set the
    ' gTTabCtl.CurSel. Must add 1 to idMenu because Win32 returns 0 if the popup
    ' menu is cancelled.
    hPopUpMenu = CreatePopupMenu()
    for idMenu as long = 0 to gTTabCtl.GetItemCount - 1
        wszCaption = gTTabCtl.GetTabText(idMenu)
        AppendMenu( hPopUpMenu, MF_ENABLED or MFT_RADIOCHECK, idMenu + 1, wszCaption )
    next
    ' Check the item that is the currently active tab (1 based)
    CheckMenuRadioItem( hPopupMenu, 1, gTTabCtl.GetItemCount, gTTabCtl.GetCurSel() + 1, MF_BYCOMMAND )

    function = hPopupMenu
end function

