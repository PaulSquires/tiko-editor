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

''
''  modExplorerSelfTest.inc   --   TIKO_EXPLORER_SELFTEST=1
''
''  Two halves. The first is pure -- the folder path algebra, which is where the subtle
''  mistakes live and which needs no window. The second DRIVES THE REAL LOAD: it puts
''  folders in the table, calls LoadExplorerFiles, and reads the resulting tree back out of
''  the control. That half exists because every path helper can be correct while the
''  recursive walk that uses them emits the wrong shape, and no pure assertion can tell the
''  difference.
''
''  WHAT THIS SUITE CANNOT SEE, said here rather than left to be discovered:
''    - the .tiko round trip. Nothing here saves or reloads a project, so a folder that
''      persists correctly and one that is silently dropped on save look identical from in
''      here. That is an interactive check: create a folder, reopen the project.
''    - anything visual, and every gesture.
''

#pragma once

dim shared as long gExpTestPass, gExpTestFail

private sub Exp_Check( byval wszName as DWSTRING, byval bCond as boolean, byval wszDetail as DWSTRING = "" )
    dim as string sLine = "  " & iif(bCond, "PASS", "FAIL") & "  " & str(wszName)
    if len(wszDetail) then sLine = sLine & "   [" & str(wszDetail) & "]"
    print sLine
    if bCond then gExpTestPass += 1 else gExpTestFail += 1
end sub


sub frmExplorer_RunSelfTest()
    if environ("TIKO_EXPLORER_SELFTEST") <> "1" then exit sub

    gExpTestPass = 0: gExpTestFail = 0
    print "--- TIKO_EXPLORER_SELFTEST ---"

    ' --- normalization --------------------------------------------------------------------
    Exp_Check( "an empty path stays empty", ProjectFolders_Normalize("") = "" )
    Exp_Check( "a plain path is unchanged", ProjectFolders_Normalize("net") = "net" )
    Exp_Check( "surrounding separators are stripped", _
        ProjectFolders_Normalize("/net/http/") = "net/http", ProjectFolders_Normalize("/net/http/") )
    Exp_Check( "runs of separators collapse", _
        ProjectFolders_Normalize("net///http") = "net/http", ProjectFolders_Normalize("net///http") )
    ' A backslash means the same thing to a user typing it, and letting both spellings into
    ' the table would give two folders that render identically and never match each other.
    Exp_Check( "backslashes fold onto the separator", _
        ProjectFolders_Normalize("net\http") = "net/http", ProjectFolders_Normalize("net\http") )
    Exp_Check( "whitespace-only segments are dropped", _
        ProjectFolders_Normalize("net/   /http") = "net/http", ProjectFolders_Normalize("net/   /http") )

    ' --- parent / leaf / depth / combine ----------------------------------------------------
    Exp_Check( "the parent of a nested path drops one level", _
        ProjectFolders_ParentPath("net/http/v2") = "net/http" )
    Exp_Check( "a top-level folder has no parent", ProjectFolders_ParentPath("net") = "" )
    Exp_Check( "the leaf of a nested path is its last segment", _
        ProjectFolders_LeafName("net/http/v2") = "v2" )
    Exp_Check( "the leaf of a top-level folder is itself", ProjectFolders_LeafName("net") = "net" )
    Exp_Check( "the top level has depth 0", ProjectFolders_Depth("") = 0 )
    Exp_Check( "a top-level folder has depth 1", ProjectFolders_Depth("net") = 1 )
    Exp_Check( "a nested folder depth counts its segments", ProjectFolders_Depth("net/http/v2") = 3 )
    Exp_Check( "combining onto the top level yields the leaf", _
        ProjectFolders_Combine("", "net") = "net" )
    Exp_Check( "combining nests the leaf under its parent", _
        ProjectFolders_Combine("net", "http") = "net/http" )
    ' Round trip: whatever Combine builds, Parent and Leaf must take apart again.
    Exp_Check( "parent and leaf invert combine", _
        ( ProjectFolders_ParentPath( ProjectFolders_Combine("a/b", "c") ) = "a/b" ) andalso _
        ( ProjectFolders_LeafName( ProjectFolders_Combine("a/b", "c") ) = "c" ) )

    ' --- descent, including the boundary trap -----------------------------------------------
    Exp_Check( "a nested folder descends from its ancestor", _
        ProjectFolders_IsDescendantOf("net/http/v2", "net") )
    Exp_Check( "a folder does not descend from itself", _
        ProjectFolders_IsDescendantOf("net", "net") = false )
    ' THE ONE THAT MATTERS. A plain prefix test says "network/http" is inside "net", and the
    ' drop validator would then refuse a legal move between two folders whose names merely
    ' share a first letter. The comparison has to land on a separator boundary.
    Exp_Check( "a shared name prefix is not descent", _
        ProjectFolders_IsDescendantOf("network/http", "net") = false, "network/http vs net" )
    Exp_Check( "descent is case-insensitive", _
        ProjectFolders_IsDescendantOf("NET/http", "net") )
    Exp_Check( "everything descends from the top level", _
        ProjectFolders_IsDescendantOf("net", "") )
    Exp_Check( "the top level descends from nothing", _
        ProjectFolders_IsDescendantOf("", "net") = false )

    ' --- name validity and which groups take folders ----------------------------------------
    Exp_Check( "an ordinary name is valid", ProjectFolders_IsValidName("net") )
    Exp_Check( "an empty name is refused", ProjectFolders_IsValidName("") = false )
    Exp_Check( "a whitespace name is refused", ProjectFolders_IsValidName("   ") = false )
    Exp_Check( "a name containing a separator is refused", ProjectFolders_IsValidName("net/http") = false )
    Exp_Check( "a name containing a backslash is refused", ProjectFolders_IsValidName("net\http") = false )
    Exp_Check( "Header takes folders", ProjectFolders_CatAllowsFolders(CATINDEX_HEADER) )
    Exp_Check( "Module takes folders", ProjectFolders_CatAllowsFolders(CATINDEX_MODULE) )
    Exp_Check( "Normal takes folders", ProjectFolders_CatAllowsFolders(CATINDEX_NORMAL) )
    Exp_Check( "Main takes no folders", ProjectFolders_CatAllowsFolders(CATINDEX_MAIN) = false )
    Exp_Check( "Resource takes no folders", ProjectFolders_CatAllowsFolders(CATINDEX_RESOURCE) = false )

    ' --- the table ---------------------------------------------------------------------------
    ' Only safe to drive when the workspace has no folders of its own to destroy. Nothing
    ' creates them yet, so this guard is belt and braces -- but it stops the suite quietly
    ' eating the user's data the moment folder creation lands.
    if ProjectFolders_Count() <> 0 then
        print "  NOTE  table and tree tests skipped: the workspace already has folders"
    else
        dim as long idxA = ProjectFolders_Add( CATINDEX_NORMAL, "alpha" )
        Exp_Check( "adding a folder returns its index", idxA >= 0 )
        Exp_Check( "adding the same folder twice is idempotent", _
            ProjectFolders_Add( CATINDEX_NORMAL, "alpha" ) = idxA, "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "the table now holds exactly one folder", ProjectFolders_Count() = 1 )
        ' Identity is (category, path): the same name under another group is another folder.
        ProjectFolders_Add( CATINDEX_MODULE, "alpha" )
        Exp_Check( "the same name under another group is a different folder", _
            ProjectFolders_Count() = 2 )
        Exp_Check( "a folder is found case-insensitively", _
            ProjectFolders_Find( CATINDEX_NORMAL, "ALPHA" ) = idxA )
        Exp_Check( "a folder is not found under the wrong group", _
            ProjectFolders_Find( CATINDEX_HEADER, "alpha" ) = -1 )
        ' Ancestors are implied, so a deep path may be added without walking it first.
        ProjectFolders_Add( CATINDEX_NORMAL, "one/two/three" )
        Exp_Check( "adding a deep path creates its ancestors", _
            ProjectFolders_Exists( CATINDEX_NORMAL, "one" ) andalso _
            ProjectFolders_Exists( CATINDEX_NORMAL, "one/two" ) andalso _
            ProjectFolders_Exists( CATINDEX_NORMAL, "one/two/three" ), _
            "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "a group that takes no folders refuses one", _
            ProjectFolders_Add( CATINDEX_MAIN, "nope" ) = -1 )
        ' Removal COMPACTS, so an ancestor still precedes its descendants afterwards.
        dim as long nBefore = ProjectFolders_Count()
        ProjectFolders_RemoveAt( ProjectFolders_Find( CATINDEX_MODULE, "alpha" ) )
        Exp_Check( "removing a folder shrinks the table by one", _
            ProjectFolders_Count() = nBefore - 1 )
        Exp_Check( "removing one folder leaves the others findable", _
            ProjectFolders_Exists( CATINDEX_NORMAL, "one/two/three" ) andalso _
            ProjectFolders_Exists( CATINDEX_NORMAL, "alpha" ) )
        ProjectFolders_Clear()
        Exp_Check( "clearing empties the table", ProjectFolders_Count() = 0 )

        ' --- the recursive walk, driven for real --------------------------------------------
        ' Every path helper above can be correct while AddFolderLevel emits the wrong shape.
        ' This builds a known tree, runs the real LoadExplorerFiles, and reads the rows back
        ' out of the control.
        ' Build the tree under whichever foldering-capable group ALREADY HAS A FILE, falling
        ' back to Normal. Hardcoding Normal makes the folders-before-files assertion skip
        ' itself on any project whose Normal group happens to be empty -- which is how it
        ' first ran here, reporting a NOTE instead of testing the ordering rule. Nothing is
        ' mutated to achieve this: the suite adapts to the workspace rather than reshaping it.
        dim as long catTest = CATINDEX_NORMAL
        dim as long catCandidates(2) = { CATINDEX_NORMAL, CATINDEX_MODULE, CATINDEX_HEADER }
        for c as long = 0 to 2
            dim as boolean bFound = false
            dim pScan as clsDocument ptr = gApp.pDocList
            do until pScan = 0
                if pScan->ProjectFiletype = gConfig.Cat( catCandidates(c) ).idFileType then bFound = true
                pScan = pScan->pDocNext
            loop
            if bFound then catTest = catCandidates(c): exit for
        next

        ProjectFolders_Add( catTest, "zeta" )
        ProjectFolders_Add( catTest, "alpha" )
        ProjectFolders_Add( catTest, "alpha/deep" )
        LoadExplorerFiles()

        dim as long nRowAlpha = -1, nRowZeta = -1, nRowDeep = -1, nRootNormal = -1
        for i as long = 0 to PsListTree_GetCount( HWND_FRMEXPLORER ) - 1
            select case frmExplorer_KindFromRow( HWND_FRMEXPLORER, i )
                case EXPKIND_ROOT
                    if PsListTree_GetItemData( HWND_FRMEXPLORER, i ) = catTest then nRootNormal = i
                case EXPKIND_FOLDER
                    dim as long fi = PsListTree_GetItemData( HWND_FRMEXPLORER, i )
                    if (fi >= lbound(gProjectFolders)) andalso (fi <= ubound(gProjectFolders)) then
                        select case ucase( gProjectFolders(fi).wszPath )
                            case "ALPHA":      nRowAlpha = i
                            case "ZETA":       nRowZeta  = i
                            case "ALPHA/DEEP": nRowDeep  = i
                        end select
                    end if
            end select
        next

        Exp_Check( "every created folder reached the tree", _
            (nRowAlpha >= 0) andalso (nRowZeta >= 0) andalso (nRowDeep >= 0), _
            "alpha=" & str(nRowAlpha) & " zeta=" & str(nRowZeta) & " deep=" & str(nRowDeep) )
        Exp_Check( "a top-level folder is a child of its group", _
            PsListTree_GetParent( HWND_FRMEXPLORER, nRowAlpha ) = nRootNormal )
        Exp_Check( "a nested folder is a child of its own parent folder", _
            PsListTree_GetParent( HWND_FRMEXPLORER, nRowDeep ) = nRowAlpha, _
            "parent=" & str(PsListTree_GetParent( HWND_FRMEXPLORER, nRowDeep )) )
        Exp_Check( "a nested folder sits one level deeper than its parent", _
            PsListTree_GetLevel( HWND_FRMEXPLORER, nRowDeep ) = _
            PsListTree_GetLevel( HWND_FRMEXPLORER, nRowAlpha ) + 1 )
        Exp_Check( "sibling folders are alphabetical", nRowAlpha < nRowZeta, _
            "alpha=" & str(nRowAlpha) & " zeta=" & str(nRowZeta) )
        ' A folder's whole subtree is emitted before its next sibling, which is what makes
        ' "alpha before zeta" a statement about ordering rather than an accident of nesting.
        Exp_Check( "a folder subtree precedes its next sibling", nRowDeep < nRowZeta )

        ' Folders before files at every level: walk the Normal group's own direct children
        ' and check that no file precedes a folder among them.
        dim as long nFirstFile = -1, nLastFolder = -1
        for i as long = 0 to PsListTree_GetCount( HWND_FRMEXPLORER ) - 1
            if PsListTree_GetParent( HWND_FRMEXPLORER, i ) <> nRootNormal then continue for
            select case frmExplorer_KindFromRow( HWND_FRMEXPLORER, i )
                case EXPKIND_FOLDER: nLastFolder = i
                case EXPKIND_FILE:   if nFirstFile = -1 then nFirstFile = i
            end select
        next
        if (nFirstFile >= 0) andalso (nLastFolder >= 0) then
            Exp_Check( "folders sort before files at a level", nLastFolder < nFirstFile, _
                "lastFolder=" & str(nLastFolder) & " firstFile=" & str(nFirstFile) )
        else
            print "  NOTE  folders-before-files not exercised: no foldering group has a file"
        end if

        ' A file pointing at a folder that does not exist must be pulled back to its group's
        ' top level, NOT dropped from the list -- the tree is built by asking each folder
        ' which files it holds, so a file nobody asks for renders nowhere while remaining in
        ' the workspace. Reaching this needs a document to corrupt.
        dim pDocAny as clsDocument ptr = gApp.pDocList
        if pDocAny <> 0 then
            dim as long nRowsBefore = PsListTree_GetCount( HWND_FRMEXPLORER )
            pDocAny->docData.wszFolder = "no/such/folder"
            LoadExplorerFiles()
            Exp_Check( "a file with an unresolvable folder is not lost", _
                PsListTree_GetCount( HWND_FRMEXPLORER ) = nRowsBefore, _
                "before=" & str(nRowsBefore) & " after=" & str(PsListTree_GetCount( HWND_FRMEXPLORER )) )
            Exp_Check( "an unresolvable folder is cleared from the document", _
                len(pDocAny->docData.wszFolder) = 0, "folder=" & pDocAny->docData.wszFolder )
        else
            print "  NOTE  folder normalization not exercised: the workspace has no documents"
        end if

        ' Leave the workspace exactly as it was found.
        ProjectFolders_Clear()
        LoadExplorerFiles()
        Exp_Check( "the suite leaves no folders behind", ProjectFolders_Count() = 0 )
    end if

    print "--- TIKO_EXPLORER_SELFTEST: " & str(gExpTestPass) & " passed, " & _
          str(gExpTestFail) & " failed ---"
end sub
