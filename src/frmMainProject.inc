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
' TIKO_WORKSPACE_SELFTEST=1 -- the WIRING half.
'
' Runs from MSG_USER_PROCESS_COMMANDLINE, because that is where the workspace is actually
' established. Everything here is an invariant of the live workspace, not arithmetic.
'
' WHAT THIS SUITE IS STRUCTURALLY BLIND TO, and it is most of what matters:
'   1. THE EXIT PATH. Every write of the untitled workspace's file happens in frmMain's
'      WM_CLOSE, and a suite that runs at startup cannot enter it. A save that aborts and
'      leaves the workspace unwritten passes every assertion below.
'   2. THE SECOND LAUNCH. Persistence is a claim about the NEXT process. Nothing here
'      re-reads what was written, and {CURDRIVE} is a no-op in-process by construction.
'   3. WHETHER ANYTHING RENDERED. These read the model. The Explorer painting nothing at
'      all would not disturb one of them.
' Only launch -> open files -> exit -> relaunch, by hand, reaches any of that.
' ========================================================================================
dim shared as long gWorkspaceTestPass, gWorkspaceTestFail
' What startup was ASKED to restore, snapshotted by frmMain_ProcessCommandLine before
' Workspace_EstablishUntitled overwrites wszLastActiveProject. Exists purely so the suite
' can assert that a requested project actually got opened -- see the note on that
' assertion for why nothing else could have caught that failure.
dim shared as DWSTRING gWorkspaceRestoreTarget

private sub Workspace_Check( byref wszName as wstring, byval bCond as boolean, byref wszDetail as wstring = "" )
    dim as string sLine
    if bCond then
        gWorkspaceTestPass += 1
        sLine = "PASS  " & str(wszName)
    else
        gWorkspaceTestFail += 1
        sLine = "FAIL  " & str(wszName)
    end if
    if len(wszDetail) then sLine = sLine & "   [" & str(wszDetail) & "]"
    ' Build a `string` first -- printing a DWSTRING, or a `string & wstring` concat,
    ' interleaves nulls.
    print sLine
end sub

private function Workspace_TestUntitledNotInMRU() as boolean
    for i as long = 0 to 9
        dim as DWSTRING wszEntry = ProcessFromCurdriveApp( gConfig.MRUProject(i) )
        if len(wszEntry) = 0 then continue for
        if ucase(wszEntry) = ucase(gConfig.UntitledProjectFilename) then return false
    next
    return true
end function

sub Workspace_RunSelfTest()
    if environ("TIKO_WORKSPACE_SELFTEST") <> "1" then exit sub

    gWorkspaceTestPass = 0: gWorkspaceTestFail = 0
    print "--- TIKO_WORKSPACE_SELFTEST ---"

    ' --- there is ALWAYS a workspace ------------------------------------------------------
    ' The single invariant the whole refactor rests on. If this fails, every save path is
    ' silently writing nothing: ProjectSaveToFile opens ProjectFilename and returns false.
    Workspace_Check( "a workspace is always established", _
        len(gApp.ProjectFilename) > 0, "ProjectFilename=" & gApp.ProjectFilename )

    Workspace_Check( "the last-active slot names the live workspace", _
        ucase(gConfig.wszLastActiveProject) = ucase(gApp.ProjectFilename) )

    ' --- named vs untitled agree with the path -------------------------------------------
    dim as boolean bIsUntitled = ( ucase(gApp.ProjectFilename) = ucase(gConfig.UntitledProjectFilename) )
    Workspace_Check( "IsProjectNamed disagrees with the untitled path", _
        gApp.IsProjectNamed() <> bIsUntitled, _
        iif( bIsUntitled, "untitled", "named" ) )

    ' The caption's "- [name]" half is gated on ProjectName, so an untitled workspace
    ' must leave it empty or the title bar reads " - []".
    if bIsUntitled then
        Workspace_Check( "an untitled workspace has no display name", _
            len(gApp.ProjectName) = 0, "ProjectName=" & gApp.ProjectName )
    else
        Workspace_Check( "a named workspace has a display name", _
            len(gApp.ProjectName) > 0, "ProjectName=" & gApp.ProjectName )
    end if

    ' The untitled workspace must never be offered as something to reopen by name.
    Workspace_Check( "the untitled path is never in Recent Projects", _
        Workspace_TestUntitledNotInMRU() )

    ' THE ASSERTION THIS SUITE WAS MISSING. Everything above describes the workspace that
    ' WAS established and says nothing about whether it was the RIGHT one -- an untitled
    ' workspace is entirely self-consistent, so all of it passed while startup silently
    ' ignored the user's last project. (Cause: establish writes wszLastActiveProject, and
    ' the restore decision used to read that same field afterwards.)
    dim as boolean bHasTarget   = ( len(gWorkspaceRestoreTarget) > 0 )
    dim as boolean bNotUntitled = ( ucase(gWorkspaceRestoreTarget) <> ucase(gConfig.UntitledProjectFilename) )
    dim as boolean bRestoreOn   = ( gConfig.RestoreSession <> 0 )
    dim as boolean bTargetOnDisk = ( AfxFileExists(gWorkspaceRestoreTarget) <> 0 )
    dim as boolean bWantedNamed = bHasTarget andalso bNotUntitled
    if bRestoreOn andalso bWantedNamed andalso bTargetOnDisk then
        Workspace_Check( "a restorable named project was actually restored", _
            ucase(gApp.ProjectFilename) = ucase(gWorkspaceRestoreTarget), _
            "wanted=" & gWorkspaceRestoreTarget & " got=" & gApp.ProjectFilename )
    else
        ' Say so rather than passing quietly -- a vacuous pass here reads as coverage.
        print "NOTE  no restorable named project on this launch; the restore assertion did not run"
    end if

    ' --- every document is classifiable ---------------------------------------------------
    ' The Explorer groups strictly by category. A document left FILETYPE_UNDEFINED matches
    ' no header and vanishes from the list while keeping its tab -- which is exactly what a
    ' File > New buffer used to do.
    dim as long nDocs, nUndefined, nMain, nResource
    dim pDoc as clsDocument ptr = gApp.pDocList
    do until pDoc = 0
        nDocs += 1
        if pDoc->ProjectFileType = FILETYPE_UNDEFINED then nUndefined += 1
        if pDoc->ProjectFileType = FILETYPE_MAIN      then nMain += 1
        if pDoc->ProjectFileType = FILETYPE_RESOURCE  then nResource += 1
        pDoc = pDoc->pDocNext
    loop

    Workspace_Check( "no live document is left uncategorized", _
        nUndefined = 0, str(nUndefined) & " of " & str(nDocs) & " undefined" )

    ' --- the Explorer shows every document, under a fixed set of headers -------------------
    ' The category range is unconditional now, so the row count is exactly the five headers
    ' plus one row per document. This is what catches a document that is in the workspace
    ' but falls through every header -- it would still be counted above if its type merely
    ' failed to MATCH a Cat() row rather than being FILETYPE_UNDEFINED.
    dim as long nHeaders = ( ubound(gConfig.Cat) - CATINDEX_MAIN ) + 1
    dim as long nRows = PsListTree_GetCount( HWND_FRMEXPLORER )
    Workspace_Check( "the Explorer lists every document under a header", _
        nRows = nHeaders + nDocs, _
        "rows=" & str(nRows) & " expected=" & str(nHeaders + nDocs) & _
        " (" & str(nHeaders) & " headers + " & str(nDocs) & " docs)" )
    Workspace_Check( "at most one Main document", nMain <= 1, str(nMain) )
    Workspace_Check( "at most one Resource document", nResource <= 1, str(nResource) )

    ' --- the extension -> category rule (pure, so assertable both ways) --------------------
    Workspace_Check( "a .bas becomes Main when the workspace has none", _
        Workspace_DeriveFileType( ".bas", false ) = FILETYPE_MAIN )
    Workspace_Check( "a .bas is Normal once a Main exists", _
        Workspace_DeriveFileType( ".bas", true ) = FILETYPE_NORMAL )
    ' The promotion is restricted to .bas. Nothing else may ever become Main by accident --
    ' a header or an include is not a program entry point and an .rc has its own category.
    Workspace_Check( "a .bi is a Header, Main or not", _
        ( Workspace_DeriveFileType( ".bi", false ) = FILETYPE_HEADER ) andalso _
        ( Workspace_DeriveFileType( ".bi", true )  = FILETYPE_HEADER ) )
    Workspace_Check( "an .inc is Normal, Main or not", _
        ( Workspace_DeriveFileType( ".inc", false ) = FILETYPE_NORMAL ) andalso _
        ( Workspace_DeriveFileType( ".inc", true )  = FILETYPE_NORMAL ) )
    Workspace_Check( "an .rc is a Resource, Main or not", _
        ( Workspace_DeriveFileType( ".rc", false ) = FILETYPE_RESOURCE ) andalso _
        ( Workspace_DeriveFileType( ".rc", true )  = FILETYPE_RESOURCE ) )
    Workspace_Check( "an unknown extension is Normal, Main or not", _
        ( Workspace_DeriveFileType( ".txt", false ) = FILETYPE_NORMAL ) andalso _
        ( Workspace_DeriveFileType( ".txt", true )  = FILETYPE_NORMAL ) )
    ' Extensions arrive from AfxStrPathname in whatever case the file has.
    Workspace_Check( "the rule is case-insensitive", _
        ( Workspace_DeriveFileType( ".BAS", false ) = FILETYPE_MAIN ) andalso _
        ( Workspace_DeriveFileType( ".Bi", true )   = FILETYPE_HEADER ) )

    ' --- the category table bridges the two numbering schemes -----------------------------
    ' Cat() is indexed by CATINDEX_* (display order) but its .idFileType holds a FILETYPE_*
    ' STRING. That bridge is the only thing tying a document to a header, it is loaded from
    ' a text file the user can edit, and it has never been asserted.
    Workspace_Check( "the category table covers the display range", _
        ubound(gConfig.Cat) >= CATINDEX_NORMAL, "ubound=" & str(ubound(gConfig.Cat)) )
    if ubound(gConfig.Cat) >= CATINDEX_NORMAL then
        Workspace_Check( "Cat(MAIN) bridges to FILETYPE_MAIN", _
            gConfig.Cat(CATINDEX_MAIN).idFileType = FILETYPE_MAIN )
        Workspace_Check( "Cat(RESOURCE) bridges to FILETYPE_RESOURCE", _
            gConfig.Cat(CATINDEX_RESOURCE).idFileType = FILETYPE_RESOURCE )
        Workspace_Check( "Cat(HEADER) bridges to FILETYPE_HEADER", _
            gConfig.Cat(CATINDEX_HEADER).idFileType = FILETYPE_HEADER )
        Workspace_Check( "Cat(MODULE) bridges to FILETYPE_MODULE", _
            gConfig.Cat(CATINDEX_MODULE).idFileType = FILETYPE_MODULE )
        Workspace_Check( "Cat(NORMAL) bridges to FILETYPE_NORMAL", _
            gConfig.Cat(CATINDEX_NORMAL).idFileType = FILETYPE_NORMAL )
    end if

    print "--- TIKO_WORKSPACE_SELFTEST: " & str(gWorkspaceTestPass) & " passed, " & _
          str(gWorkspaceTestFail) & " failed ---"
end sub


' ========================================================================================
' Establish the UNTITLED workspace.
'
' Every workspace is a project now, so there is no "no project open" state to fall back
' to -- when there is no named project there is an untitled one, backed by a file under
' settings\ that nothing but this function and IsProjectNamed knows the name of.
'
' The file is deliberately NOT written here. An install launched once and closed with
' nothing open should not leave a file behind, and at this point there is no content to
' carry anyway. It gets written by the ordinary save paths.
' ========================================================================================
function Workspace_EstablishUntitled() as boolean

    dim as boolean bLoaded

    gApp.ProjectFilename = gConfig.UntitledProjectFilename
    ' An empty ProjectName is what suppresses the "- [name]" half of the window caption.
    ' IsProjectNamed() reads the FILENAME, so this is presentation only.
    gApp.ProjectName     = ""

    if AfxFileExists( gConfig.UntitledProjectFilename ) then
        ' Check the result. ProjectLoadFromFile assigns ProjectFilename/ProjectName BEFORE
        ' it opens the stream, so a corrupt file leaves gApp pointing at a project that was
        ' never loaded. That was rare when a project had to be opened deliberately; it is
        ' reachable at every startup now.
        bLoaded = gConfig.ProjectLoadFromFile( gConfig.UntitledProjectFilename )
        ' ProjectLoadFromFile overwrites ProjectName from the path, which for the untitled
        ' workspace would put "default" in the caption. Put it back.
        gApp.ProjectName = ""
    end if

    if bLoaded = false then
        ' Fresh workspace. Seed the per-workspace compiler settings from the machine-wide
        ' defaults in Options -- those globals are seeds now, not a parallel store.
        gApp.ProjectBuild       = gConfig.CompilerBuild
        gApp.ProjectOther32     = gConfig.CompilerSwitches
        gApp.ProjectOther64     = gConfig.CompilerSwitches
        gApp.ProjectCommandLine = ""
        gApp.ProjectManifest    = false
        ' One-shot migration of the loose-file notes. They used to live in settings.ini's
        ' [Notes] block as gApp.NonProjectNotes, which is going away -- without this the
        ' user's notes would be destroyed silently the day that field is deleted.
        ' LoadConfigFile has already populated NonProjectNotes by the time we run.
        gApp.ProjectNotes       = gApp.NonProjectNotes
    end if

    gConfig.wszLastActiveProject = gApp.ProjectFilename

    function = true
end function


' ========================================================================================
' Save the current workspace and close every document in it.
'
' The half of the old OnCommand_ProjectClose that every caller wanted. Returns FALSE if
' the user cancelled a save prompt, in which case the CALLER must abandon whatever it was
' going to do next -- this is what makes "Cancel" work on app exit and on project open.
'
' Callers pick their own successor: app exit does nothing, project open loads another
' project, Project > Close establishes a fresh untitled workspace.
' ========================================================================================
function Workspace_SaveAndCloseAll( byval hwnd as HWND ) as boolean

    ' Do the actual saving to disk of the Project file and any dirty documents
    gApp.SaveProject(false)
    if OnCommand_FileClose( hwnd, EFC_CLOSEALL ) = false then return false

    ' If the entire app is closing down then the remaining cleanup is pointless.
    if gApp.IsShutdown then return true

    ' EFC_CLOSEALL only closes TABS. Since a closed tab no longer removes its document
    ' from the workspace, emptying the workspace has to be said explicitly.
    gApp.RemoveAllDocuments()

    ' If the Find/Replace window is open then close it.
    FindReplaceControls_Hide()

    function = true
end function


' ========================================================================================
' Close the current project and return to a fresh untitled workspace
' ========================================================================================
function OnCommand_ProjectClose( byval hwnd As HWND ) As LRESULT

    if Workspace_SaveAndCloseAll( hwnd ) = false then exit function

    ' If the entire app is closing down then we do not have to process the
    ' remaining project cleanup commands.
    if gApp.IsShutdown then return true

    ' Reset the Project variables. ProjectFilename/ProjectName are deliberately NOT
    ' cleared here -- Workspace_EstablishUntitled below owns both, and blanking them
    ' first would leave a window in which ProcessToCurdriveProject has no drive to
    ' key off.
    gApp.ProjectBuild         = ""
    gApp.ProjectOther32       = ""
    gApp.ProjectOther64       = ""
    gApp.ProjectManifest      = false
    gApp.ProjectNotes         = ""
    gApp.ProjectCommandLine   = ""
    gApp.wszLastOpenFolder    = ""

    gApp.hIconPanel = 0

    ' Clear any previous info from the Output windows
    frmOutput_ResetAllControls

    ' There is always a workspace, so closing one means starting the next.
    Workspace_EstablishUntitled()

    ' Show the (now untitled) workspace's notes.
    AfxSetWindowText(GetDlgItem(HWND_FRMOUTPUT, IDC_FRMOUTPUT_TXTNOTES), gApp.ProjectNotes)

    ' Evict the closed project's scans from the symbol database BEFORE reloading
    ' the panels below - they repopulate from gSymDb, so a still-installed set
    ' would faithfully rebuild the old project's lists. Both tiers go: every
    ' document was closed above, so the buffer set refers to a closed file. The
    ' displaced sets are retired for same-thread freeing by the scan worker.
    dim as PARSERESULTSET ptr pStale = gSymDb.InstallSet( ScanTierProject, 0 )
    if pStale then gScanMgr.RetireResult( pStale )
    pStale = gSymDb.InstallSet( ScanTierBuffer, 0 )
    if pStale then gScanMgr.RetireResult( pStale )

    ' The TODO store outlives the sets that populated it (project sweeps add
    ' entries for files never opened as documents), so empty it explicitly.
    erase gTodoItems
    gTodoCount = 0
    frmOutput_UpdateToDoListview()

    ' Clear content from functions and bookmarks listboxes
    ' (LoadFunctionsFiles dispatches on the current tree/list view state)
    LoadFunctionsFiles()
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

    frmMain_PositionWindows
    
    ' Refresh the Project Explorer file name list
    LoadExplorerFiles()
    LoadFunctionsFiles()
    LoadBookmarksFiles()

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
        frmMain_PositionWindows()
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
        gApp.RemoveDocument( pDoc )
        LoadExplorerFiles()
    end if
    
    if gTTabCtl.IsValidTab( gTTabCtl.GetCurSel() ) then
        frmExplorer_SelectItemData( gTTabCtl.GetpDoc(gTTabCtl.GetCurSel()) )
    end if

    function = 0
end function
