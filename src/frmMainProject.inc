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

#include once "frmMain.bi"
#include once "clsDocument.bi"

' ========================================================================================
' Add one or more files to the project (but don't open them to Tabs)
' ========================================================================================
function OnCommand_ProjectFilesAdd( byval hwnd As HWnd ) as LRESULT
    OnCommand_FileOpen( hwnd, false )
    LoadExplorerFiles()
    function = 0
end function


' ========================================================================================
' Save project file to disk
' ========================================================================================
function OnCommand_ProjectSave( _
            byval hwnd as HWND, _
            byval bSaveAs as boolean = False _
            ) As LRESULT
            
    ' Save all dirty files
    OnCommand_FileSaveAll( hwnd )
    
    ' Do the actual saving to disk of the Project file
    gApp.SaveProject(bSaveAs)

    frmMain_SetFocusToCurrentCodeWindow

    function = 0
end function 


' ========================================================================================
' Close currently active project
' ========================================================================================
function OnCommand_ProjectClose( byval hwnd As HWND ) As LRESULT
    gConfig.wszLastActiveSession = ""
    gConfig.SaveConfigFile()

    ' Save the project configuration to disk and any dirty files
    ' Do the actual saving to disk of the Project file
    gApp.SaveProject(false)
    if OnCommand_FileClose( hwnd, EFC_CLOSEALL ) = false then exit function
    
    ' If the entire app is closing down then we do not have to process the
    ' remaining project cleanup commands. 
    if gApp.IsShutdown then return true
        
    ' If we get this far then we can safely remove all of the open pDocs
    gApp.RemoveAllDocuments()
    
    ' If the Find/Replace window is open then close it.
    FindReplaceControls_Hide()

    ' Reset the Project variables
    gApp.IsProjectActive      = false
    gApp.ProjectName          = ""
    gApp.ProjectFilename      = ""
    gApp.ProjectBuild         = ""
    gApp.ProjectOther32       = ""
    gApp.ProjectOther64       = ""
    gApp.ProjectManifest      = false
    gApp.ProjectNotes         = ""
    gApp.ProjectCommandLine   = ""
    gApp.wszLastOpenFolder    = ""
    
    gApp.wszPanelText = ""
    gApp.hIconPanel = 0

    ' Clear any previous info from the Output windows
    frmOutput_ResetAllControls
    
    ' Reset the ProjectNotes text box with the generic non-project related notes
    AfxSetWindowText(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), gApp.NonProjectNotes)

    ' Clear content from functions and bookmarks listboxes
    LoadFilesAndFunctions()
    LoadBookmarksFiles()
    
    ' Set Explorer as panel in view and then populate it.
    OnCommand_ViewExplorer()
    LoadExplorerFiles()
    
    frmMain_SetStatusbar
    frmMain_SetFocusToCurrentCodeWindow
    
    function = true
end function


' ========================================================================================
' Create a new project
' ========================================================================================
function OnCommand_ProjectNew( byval hwnd As HWND ) As LRESULT
    
    ' Ensure that the Project Manager window has been created (show it now as well)
    frmProjectOptions_Show( hwnd, true ) 
    
    if gApp.IsNewProjectFlag then 
        ' Need to remove resource file (if created) from the document list otherwise it
        ' will be loaded again and it will display twice in the Explorer.
        gApp.RemoveAllDocuments()
        
        ' Open the new project so it displays in the Explorer treeview. Pass the 
        ' variable wText because passing gApp.ProjectFilename will cause that variable
        ' to get reset because the parameter is byref and eventually that variable
        ' gets assigned a null.
        dim as DWSTRING wszText = gApp.ProjectFilename
        frmMain_OpenProjectSafely(HWND_FRMMAIN, wszText) 
    end if
        
    gApp.IsNewProjectFlag = false
    gApp.IsProjectLoading = false

    frmExplorer_PositionWindows
    frmMain_PositionWindows
    
    ' Refresh the Project Explorer file name list
    LoadExplorerFiles()
    LoadFunctionsFiles()

    ' This will update the main window to show the project name in the window caption
    frmMain_SetFocusToCurrentCodeWindow
    
    function = 0
end function


' ========================================================================================
' Open a Project
' ========================================================================================
function OnCommand_ProjectOpen( byval hwnd As HWND ) as LRESULT

    ' Display the Open File Dialog
    dim pwszName as wstring ptr = AfxIFileOpenDialogW(hwnd, IDM_PROJECTOPEN)
    if pwszName then 
        ' Pass the info to our generic project open function to handle everything.
        frmMain_OpenProjectSafely(HWND_FRMMAIN, *pwszName) 
        CoTaskMemFree(pwszName)
    end if
    frmMain_SetFocusToCurrentCodeWindow

    function = 0
end function


' ========================================================================================
' Set the file type based on file extension
' ========================================================================================
function OnCommand_ProjectSetFileType( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    ' This code also handles right click messages from the Explorer Treeview. 
    dim as DWSTRING wszFileType

    select case id
        case IDM_SETFILENORMAL, IDM_SETFILENORMAL_EXPLORERTREEVIEW 
            wszFileType = FILETYPE_NORMAL
        case IDM_SETFILEMODULE, IDM_SETFILEMODULE_EXPLORERTREEVIEW
            wszFileType = FILETYPE_MODULE
        case IDM_SETFILEMAIN, IDM_SETFILEMAIN_EXPLORERTREEVIEW
            wszFileType = FILETYPE_MAIN
        case IDM_SETFILERESOURCE, IDM_SETFILERESOURCE_EXPLORERTREEVIEW
            wszFileType = FILETYPE_RESOURCE
        case IDM_SETFILEHEADER, IDM_SETFILEHEADER_EXPLORERTREEVIEW
            wszFileType = FILETYPE_HEADER
        case is > IDM_SETCATEGORY
            wszFileType = gConfig.Cat(id-IDM_SETCATEGORY).idFileType
    end select
    
    if pDoc = 0 then exit function

    if pDoc->ProjectFileType <> wszFileType then
        ' If attempting to set MAIN or RESOURCE then we need to move any existing
        ' pDoc that have that filetype over to the NORMAL branch.
        if (wszFileType = FILETYPE_MAIN) or (wszFileType = FILETYPE_RESOURCE) then
            dim pDoc2 as clsDocument ptr = iif(wszFileType = FILETYPE_MAIN, gApp.GetMainDocumentPtr, gApp.GetResourceDocumentPtr)
            if pDoc2 then
                pDoc2->ProjectFileType = FILETYPE_NORMAL
            end if   
        end if
        ' Set the new FileType for the currently selected document
        gApp.ProjectSetFileType( pDoc, wszFileType )
        LoadExplorerFiles()
        ' Highlight the selected tab file in the Explorer listbox
        frmExplorer_SelectItemData( pDoc )
        frmTopTabsInfo_PositionWindows()
    end if

    function = 0
end function


' ========================================================================================
' Remove file from the project
' ========================================================================================
function OnCommand_ProjectRemove( _
            byval id as long, _
            byval pDoc as clsDocument ptr _
            ) as LRESULT

    if pDoc = 0 then exit function
    if gTTabCtl.SetTabIndexByDocumentPtr( pDoc ) <> -1 then
        if OnCommand_FileClose( HWND_FRMMAIN, EFC_CLOSECURRENT ) = false then exit function
        ' The FileClose function will also remove any files with pDoc->IsNewFlag = true.
        ' and invalidate the pDoc memory so check that pDoc is valid if proceeding further.
    end if  

    ' If the pDoc is still valid the FileClose
    if gApp.IsValidDocumentPointer(pDoc) then
        gdb2.dbDelete( pDoc->DiskFilename )
        gApp.RemoveDocument( pDoc )
        LoadExplorerFiles()
    end if
    
    if gTTabCtl.IsSafeIndex(gTTabCtl.CurSel) then
        frmExplorer_SelectItemData( gTTabCtl.tabs(gTTabCtl.CurSel).pDoc )
    end if

    function = 0
end function
