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

    ' The slot names a NAMED project or nothing. It must never name the untitled
    ' workspace: settings.ini persists it, so one such write makes every later launch come
    ' up untitled forever, with the user's project unreachable except through Recent
    ' Projects. This assertion is the one that pins that down.
    if gApp.IsProjectNamed() then
        Workspace_Check( "the slot names the live named project", _
            ucase(gConfig.wszLastActiveProject) = ucase(gApp.ProjectFilename), _
            "slot=" & gConfig.wszLastActiveProject )
    else
        Workspace_Check( "an untitled workspace leaves the slot empty", _
            len(gConfig.wszLastActiveProject) = 0, _
            "slot=" & gConfig.wszLastActiveProject )
    end if
    Workspace_Check( "the slot never names the untitled workspace", _
        ucase(gConfig.wszLastActiveProject) <> ucase(gConfig.UntitledProjectFilename) )

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
    dim as long nPromote = iif( gApp.IsProjectNamed(), 0, 1 )
    ' User subfolders each take a row too. Every folder in the table is displayed --
    ' ProjectFolders_Add refuses a category that takes none -- so the count needs no filter.
    ' This term is not optional: without it the assertion passes only on a workspace that has
    ' no folders, which is every workspace until the first one is created and none after.
    dim as long nFolders = ProjectFolders_Count()
    dim as long nRows = PsListTree_GetCount( HWND_FRMEXPLORER )
    Workspace_Check( "the Explorer lists every document and folder under a group", _
        nRows = nHeaders + nDocs + nPromote + nFolders, _
        "rows=" & str(nRows) & " expected=" & str(nHeaders + nDocs + nPromote + nFolders) & _
        " (" & str(nHeaders) & " groups + " & str(nDocs) & " docs + " & _
        str(nPromote) & " promote + " & str(nFolders) & " folders)" )

    ' The promotion row exists exactly when the workspace is untitled, and it is pinned at
    ' index 0. Both halves matter: offering it on a named project would be nonsense, and a
    ' row that is present but buried below the headers is not the affordance it claims to
    ' be. Checked by ROW KIND, not by caption -- the caption is localized.
    dim as boolean bPromoteAtTop = false
    if PsListTree_GetCount( HWND_FRMEXPLORER ) > 0 then
        bPromoteAtTop = ( frmExplorer_KindFromRow( HWND_FRMEXPLORER, 0 ) = EXPKIND_PROMOTE )
    end if
    ' EVERY Explorer row resolves to a real document or to nothing. This is the invariant
    ' that a hand-written guard at each call site could not deliver: a row's itemData means
    ' whatever its kind says it means, so a reader that casts it to a pointer without
    ' consulting the kind is reading one type as another. Under the previous scheme -- a bare
    ' sentinel value living in itemData -- that dereferenced address 1: three readers were
    ' guarded by hand and the tooltip callback was missed, giving a GPF on hover.
    ' Asserted over all rows rather than over the row that happens to be special today.
    dim as long nBadRow = -1
    for i as long = 0 to PsListTree_GetCount( HWND_FRMEXPLORER ) - 1
        ' Deliberately NOT skipping group rows first. Excluding them by IsHeader before
        ' asking would leave DocFromRow's whole job -- refusing to resolve a non-file row --
        ' untested in a named workspace, where every non-file row IS a group row. Offering
        ' it every row is what makes this assertion sensitive to the kind mechanism.
        dim as clsDocument ptr pRowDoc = frmExplorer_DocFromRow( HWND_FRMEXPLORER, i )
        if pRowDoc = 0 then continue for
        if gApp.IsValidDocumentPointer( pRowDoc ) = false then nBadRow = i: exit for
    next
    Workspace_Check( "every Explorer row resolves to a live document or to nothing", _
        nBadRow = -1, iif( nBadRow = -1, "all rows clean", "row " & str(nBadRow) ) )

    ' --- the Explorer is a TREE, not a flat list ------------------------------------------
    ' The row COUNT cannot see this. Group rows became plain tree parents and files became
    ' their children; get the parenting wrong -- AddString instead of AddNode, say, which
    ' derives its level from the preceding row and would leave every file at level 0 -- and
    ' the count is unchanged, every row still resolves, and nothing above goes red. What
    ' would break is collapsing a group, which is not reachable from a headless suite.
    ' The loop deliberately runs to completion and only REMEMBERS the first offending row,
    ' rather than exiting on it. Bailing early would leave nRoots short and fail the
    ' unrelated category assertion below as collateral -- measured: the A/B that reverts the
    ' file insert to AddString reported 2 failures that way, only one of them a finding.
    dim as long nBadTree = -1
    dim as long nRoots = 0
    for i as long = 0 to PsListTree_GetCount( HWND_FRMEXPLORER ) - 1
        dim as boolean bBad = false
        select case frmExplorer_KindFromRow( HWND_FRMEXPLORER, i )
            case EXPKIND_ROOT
                nRoots += 1
                ' A group sits at the top level, and must not be selectable: WM_LBUTTONDOWN
                ' computes canDrag from bSelectable, so a selectable group row would be
                ' draggable once drag-reorder is on.
                if PsListTree_GetLevel( HWND_FRMEXPLORER, i ) <> 0 then bBad = true
                if PsListTree_GetRowSelectable( HWND_FRMEXPLORER, i ) then bBad = true
            case EXPKIND_FILE
                ' A file is a child OF A GROUP -- both halves checked, since a file at
                ' level 1 under the wrong kind of parent would still read as depth-correct.
                if PsListTree_GetLevel( HWND_FRMEXPLORER, i ) <> 1 then bBad = true
                dim as long nParent = PsListTree_GetParent( HWND_FRMEXPLORER, i )
                if frmExplorer_KindFromRow( HWND_FRMEXPLORER, nParent ) <> EXPKIND_ROOT then
                    bBad = true
                end if
        end select
        if bBad andalso (nBadTree = -1) then nBadTree = i
    next
    Workspace_Check( "every Explorer file is a child of a group row", _
        nBadTree = -1, iif( nBadTree = -1, "tree well-formed", "row " & str(nBadTree) ) )
    Workspace_Check( "the Explorer shows one row per category", _
        nRoots = nHeaders, "roots=" & str(nRoots) & " categories=" & str(nHeaders) )

    Workspace_Check( "the promotion row is present iff the workspace is untitled", _
        bPromoteAtTop = ( gApp.IsProjectNamed() = false ), _
        iif( gApp.IsProjectNamed(), "named", "untitled" ) & _
        ", row0 is promote = " & str(bPromoteAtTop) )
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

    ' --- the CLOSE transition -------------------------------------------------------------
    ' TIKO_WORKSPACE_TESTCLOSE=1, and only from a NAMED project, because it really does
    ' close the live one. Same reasoning as the promotion block below: closing is a
    ' transition, and both sides of it are self-consistent, so nothing checked at startup
    ' can tell whether the restore slot was actually released.
    if environ("TIKO_WORKSPACE_TESTCLOSE") = "1" then
        if gApp.IsProjectNamed() = false then
            print "NOTE  close test skipped: the workspace is not a named project"
        else
            dim as DWSTRING wszWas = gApp.ProjectFilename
            OnCommand_ProjectClose( HWND_FRMMAIN )

            Workspace_Check( "closing a project leaves an untitled workspace", _
                gApp.IsProjectNamed() = false, "now=" & gApp.ProjectFilename )
            ' THE ONE THAT MATTERS: a closed project must not reopen on the next launch.
            Workspace_Check( "closing a project releases the restore slot", _
                len(gConfig.wszLastActiveProject) = 0, _
                "was=" & wszWas & " slot=" & gConfig.wszLastActiveProject )

            ' AND IT HAS TO SURVIVE THE ROUND TRIP TO DISK. The in-memory check above is
            ' not the user-visible claim -- the next launch reads settings.ini, so what
            ' matters is what SaveConfigFile actually wrote. Persist and read it back.
            gConfig.SaveConfigFile()
            gConfig.LoadConfigFile( true )   ' true = hot reload; leaves geometry alone
            Workspace_Check( "the released slot survives a settings.ini round trip", _
                len(gConfig.wszLastActiveProject) = 0, _
                "slot after reload=" & gConfig.wszLastActiveProject )

            ' THE ONE THAT REPRODUCES THE REPORTED BUG. Put a project path on DISK while
            ' memory holds the released (empty) slot, then hot-reload. ReloadConfigFileTest
            ' does exactly this whenever settings.ini's timestamp moves, and re-reading
            ' LastActiveProjectFile used to resurrect the closed project -- so exit wrote it
            ' back out and the next launch reopened a project the user had closed.
            gConfig.wszLastActiveProject = wszWas
            gConfig.SaveConfigFile()                 ' disk now names the closed project
            gConfig.wszLastActiveProject = ""        ' memory says: released
            gConfig.LoadConfigFile( true )
            Workspace_Check( "a hot reload does not resurrect a closed project", _
                len(gConfig.wszLastActiveProject) = 0, _
                "slot after hot reload=" & gConfig.wszLastActiveProject )

            ' Leave the file consistent with memory rather than holding the stale path.
            gConfig.SaveConfigFile()
        end if
    end if

    ' --- the PROMOTION transition ---------------------------------------------------------
    ' Only reachable with TIKO_WORKSPACE_TESTPROMOTE=1, and only from an untitled workspace,
    ' because it really does promote the live one. It exists because promotion is a
    ' TRANSITION and the state is self-consistent on BOTH sides of it -- every assertion
    ' above passes before and after -- so two defects escaped: the restore slot was never
    ' repointed (the new project silently did not reopen on the next launch) and the
    ' Explorer was never refreshed (the "Save as Project..." row lingered on a named
    ' project). Neither is visible from a steady-state check.
    if environ("TIKO_WORKSPACE_TESTPROMOTE") = "1" then
        if gApp.IsProjectNamed() then
            print "NOTE  promotion test skipped: the workspace is already named"
        else
            dim as DWSTRING wszScratch = AfxGetExePathName & "_workspace_selftest.tiko"
            if AfxFileExists( wszScratch ) then AfxDeleteFile( wszScratch )

            dim as boolean bPromoted = gApp.SaveProject( true, wszScratch )

            Workspace_Check( "promotion reports success", bPromoted )
            Workspace_Check( "promotion writes the named file", _
                AfxFileExists( wszScratch ) <> 0 )
            Workspace_Check( "promotion makes the workspace named", _
                gApp.IsProjectNamed() )
            Workspace_Check( "promotion repoints the restore slot to the new project", _
                ucase(gConfig.wszLastActiveProject) = ucase(wszScratch), _
                "slot=" & gConfig.wszLastActiveProject )
            Workspace_Check( "promotion retires the untitled workspace file", _
                AfxFileExists( gConfig.UntitledProjectFilename ) = 0 )
            Workspace_Check( "promotion removes the Explorer's promote row", _
                frmExplorer_KindFromRow( HWND_FRMEXPLORER, 0 ) <> EXPKIND_PROMOTE )

            AfxDeleteFile( wszScratch )
        end if
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
'
' bRestoreFromFile says whether the backing file's CONTENT is wanted. Only STARTUP wants
' it -- that is the session-restore of whatever was open when the app last quit untitled.
' Every other caller is establishing a workspace to REPLACE one just closed, and reading
' the file there resurrects the last untitled session's documents into what the user asked
' to be an empty editor. Hence the default is false: restoring is the special case.
' ========================================================================================
function Workspace_EstablishUntitled( byval bRestoreFromFile as boolean = false ) as boolean

    dim as boolean bLoaded

    gApp.ProjectFilename = gConfig.UntitledProjectFilename
    ' An empty ProjectName is what suppresses the "- [name]" half of the window caption.
    ' IsProjectNamed() reads the FILENAME, so this is presentation only.
    gApp.ProjectName     = ""

    dim as boolean bFileExists = ( AfxFileExists( gConfig.UntitledProjectFilename ) <> 0 )

    if bRestoreFromFile andalso bFileExists then
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

    ' Do NOT put this workspace's path in the restore slot. The slot names the NAMED
    ' project to reopen; the untitled workspace is the fallback when there is none, so it
    ' clears the slot rather than claiming it. Claiming it was sticky: once settings.ini
    ' recorded settings\default.tiko as the last project, every later launch read that
    ' back, saw a path that is not a named project, and came up untitled forever.
    gConfig.wszLastActiveProject = ""

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

    ' There is always a workspace, so closing one means starting the next. The next one is
    ' EMPTY -- do not read the untitled backing file, or "New Project"/"Close" would reopen
    ' the documents of whatever untitled session last wrote it.
    Workspace_EstablishUntitled( false )

    ' Persist the released restore slot NOW rather than leaving it to app exit. Until this
    ' runs, settings.ini still names the project that was just closed, and anything that
    ' reads the file back -- ReloadConfigFileTest, a second instance -- sees a closed
    ' project as the one to reopen.
    gConfig.SaveConfigFile()

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
    gTodoItems.Clear( )
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
' Start a new, NAMED project.
'
' Close the current workspace, then ask for a name straight away. This is deliberately
' NOT the old pre-rewrite dialog: nothing here collects compiler switches or build
' settings, only a path, and the workspace it names is a perfectly ordinary one that the
' user could equally have reached by adding files and choosing "Save as Project...".
'
' It used to stop after the close, leaving a fresh untitled workspace -- correct under
' the one-workspace model, and indistinguishable from a dead menu item when the workspace
' being replaced was ALREADY untitled and empty. "New Project..." now earns its ellipsis.
'
' CANCELLING THE NAME DIALOG LEAVES THE UNTITLED WORKSPACE STANDING, not the workspace
' that was just closed -- the close has already happened by then and is not undone. That
' is the same outcome the command had before, so nothing is lost that the user had not
' already agreed to close.
' ========================================================================================
function OnCommand_ProjectNew( byval hwnd As HWND ) As LRESULT

    ' Save the current workspace, close it, establish a fresh untitled one. Cancelling a
    ' save prompt in there aborts the whole thing -- ProjectClose returns 0 and no name is
    ' asked for.
    if OnCommand_ProjectClose( hwnd ) = 0 then exit function

    ' Promote the empty workspace to a named project. SaveProject owns the dialog, the
    ' MRU entry, the retirement of the untitled backing file and the Explorer refresh;
    ' a cancelled dialog returns false having changed nothing.
    gApp.SaveProject( true )

    frmMain_SetStatusbar
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
        ' The Functions panel lists the Explorer's files, so a document leaving the
        ' workspace has to drop out of both.
        LoadFunctionsFiles()
    end if
    
    if gTTabCtl.IsValidTab( gTTabCtl.GetCurSel() ) then
        frmExplorer_SelectItemData( gTTabCtl.GetpDoc(gTTabCtl.GetCurSel()) )
    end if

    function = 0
end function
