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
    ' The workspace's own folders are SAVED AND RESTORED around everything below, rather than
    ' the suite refusing to run when it finds any.
    '
    ' Refusing was the first design, and it quietly turned itself off the moment the feature
    ' was actually used: a real project has folders, so 58 of these assertions simply stopped
    ' running and said so in a NOTE nobody would read twice. A suite that disables itself once
    ' the code it covers is in use is worse than no suite.
    '
    ' Restored VERBATIM, not through ProjectFolders_Add: Add normalizes and synthesizes
    ' ancestors, so round-tripping through it could hand back a table subtly unlike the one
    ' the project loaded. The documents' folders are saved too -- Rebase rewrites those as
    ' well, and a document left pointing at a test folder would be pulled to its group's top
    ' level by the next normalization pass and the user's file would silently move.
    redim savedFolders(any) as PROJECT_FOLDER
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        redim preserve savedFolders(i)
        savedFolders(i) = gProjectFolders(i)
    next
    dim as long nSavedFolders = ProjectFolders_Count()

    redim savedDocs(any) as clsDocument ptr
    redim savedDocFolders(any) as DWSTRING
    dim pSaveDoc as clsDocument ptr = gApp.pDocList
    do until pSaveDoc = 0
        dim as long ub = ubound(savedDocs) + 1
        redim preserve savedDocs(ub)
        redim preserve savedDocFolders(ub)
        savedDocs(ub) = pSaveDoc
        savedDocFolders(ub) = pSaveDoc->docData.wszFolder
        pSaveDoc = pSaveDoc->pDocNext
    loop

    ProjectFolders_Clear()
    if true then
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

        ' --- reloading must not change the panel's visibility ---------------------------------
        ' LoadExplorerFiles hides the control while it refills. It used to leave it hidden and
        ' rely on frmPanel_ShowPanel to bring it back, which held only while every caller was a
        ' project load. Folder create/rename/delete all rebuild the tree with nothing behind
        ' them, so the whole panel went blank on the first click of the add-folder icon.
        '
        ' Checked on the control's OWN style bit, deliberately, because that is what the fix
        ' reads: IsWindowVisible walks the ancestor chain and would answer false at startup for
        ' a control that is visible in its own right.
        ' BOTH STATES ARE FORCED, and that is the whole assertion. Simply reading the
        ' visibility, reloading, and comparing is VACUOUS here: this suite runs at startup
        ' before frmPanel has shown the Explorer, so the bit is already false, and "hide and
        ' never restore" is then indistinguishable from "hide and restore". Measured -- the
        ' A/B that deletes the restore left that version passing at before=false after=false.
        dim as boolean bVisOrig = ( (GetWindowLongPtrW( HWND_FRMEXPLORER, GWL_STYLE ) and WS_VISIBLE) <> 0 )

        ShowWindow( HWND_FRMEXPLORER, SW_SHOW )
        LoadExplorerFiles()
        Exp_Check( "reloading leaves a visible Explorer visible", _
            (GetWindowLongPtrW( HWND_FRMEXPLORER, GWL_STYLE ) and WS_VISIBLE) <> 0 )

        ' The other direction matters too: the Explorer is legitimately hidden whenever the
        ' panel is showing Functions or Bookmarks, and a reload must not yank it forward.
        ShowWindow( HWND_FRMEXPLORER, SW_HIDE )
        LoadExplorerFiles()
        Exp_Check( "reloading leaves a hidden Explorer hidden", _
            (GetWindowLongPtrW( HWND_FRMEXPLORER, GWL_STYLE ) and WS_VISIBLE) = 0 )

        if bVisOrig then ShowWindow( HWND_FRMEXPLORER, SW_SHOW ) else ShowWindow( HWND_FRMEXPLORER, SW_HIDE )

        ' --- the icon rects ------------------------------------------------------------------
        ' Which icons a row offers is expressed by the SAME call that places them, so these
        ' assertions cover the offer rule and the geometry at once. rcRow is a synthetic
        ' full-width row; only x matters, which is all the hit test compares.
        '
        ' rcRow is built from the SURFACE'S OWN client rect, not from a made-up width. It has
        ' to be: IconHitTest derives its row rect that way, so a synthetic rect here would
        ' place the icons somewhere the hit test never looks and the agreement assertions
        ' below would compare two different geometries. (They did, first time round, and
        ' failed -- which is the assertion doing its job on the test rather than the code.)
        ' The remaining link, that the painter's p->rc spans the same x, holds because
        ' PsListTree's paint loop and its own hit test both read GetClientRect on this same
        ' surface -- that one is verified by reading the control, not from here.
        dim as RECT rcRowT, rcAddT, rcRenT, rcDelT
        dim as HWND hSurfT = GetDlgItem( HWND_FRMEXPLORER, IDC_FRMEXPLORER_LISTBOX )
        GetClientRect( hSurfT, @rcRowT )
        rcRowT.top = 0
        rcRowT.bottom = PsListTree_GetRowHeight( HWND_FRMEXPLORER )
        ' Precondition, so nothing below passes vacuously on a zero-width surface: two icons
        ' in a 0px row would sit at negative coordinates and still "agree" with each other.
        Exp_Check( "the Explorer surface has a measurable width", rcRowT.right > 60, _
            "width=" & str(rcRowT.right) )

        frmExplorer_IconRects( HWND_FRMEXPLORER, nRootNormal, rcRowT, rcAddT, rcRenT, rcDelT )
        Exp_Check( "a foldering group offers Add", IsRectEmpty( @rcAddT ) = 0 )
        Exp_Check( "a group offers no Delete", IsRectEmpty( @rcDelT ) <> 0 )
        Exp_Check( "a group offers no Rename", IsRectEmpty( @rcRenT ) <> 0 )

        frmExplorer_IconRects( HWND_FRMEXPLORER, nRowAlpha, rcRowT, rcAddT, rcRenT, rcDelT )
        Exp_Check( "a folder offers all three icons", _
            (IsRectEmpty( @rcAddT ) = 0) andalso (IsRectEmpty( @rcRenT ) = 0) andalso _
            (IsRectEmpty( @rcDelT ) = 0) )
        ' Reading order left to right is Add, Rename, Delete.
        Exp_Check( "Rename sits between Add and Delete", _
            (rcAddT.right <= rcRenT.left) andalso (rcRenT.right <= rcDelT.left), _
            "add.right=" & str(rcAddT.right) & " ren=[" & str(rcRenT.left) & "," & _
            str(rcRenT.right) & "] del.left=" & str(rcDelT.left) )
        Exp_Check( "both icons stay inside the row", rcDelT.right <= rcRowT.right )
        ' The hit test must answer with what was drawn, AT THE DRAWN EDGES.
        '
        ' Probing each icon's centre is not enough and was measured not to be: an A/B that
        ' gave the hit test its own inlined copy of the arithmetic, 2px adrift, left every
        ' centre probe still inside its icon and the suite fully green. A centre probe
        ' tolerates drift up to half an icon width, so it cannot detect the very thing these
        ' assertions are named for. Both outer edges, and the pixel just past each, are what
        ' make any drift at all fail.
        Exp_Check( "Delete answers at its left edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcDelT.left ) = EXPICON_DELETE )
        Exp_Check( "Delete answers at its right edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcDelT.right - 1 ) = EXPICON_DELETE )
        Exp_Check( "nothing answers past Delete's right edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcDelT.right ) = EXPICON_NONE )
        Exp_Check( "Rename answers at its left edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcRenT.left ) = EXPICON_RENAME )
        Exp_Check( "Rename answers at its right edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcRenT.right - 1 ) = EXPICON_RENAME )
        Exp_Check( "Add answers at its left edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcAddT.left ) = EXPICON_ADD )
        Exp_Check( "Add answers at its right edge", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcAddT.right - 1 ) = EXPICON_ADD )
        Exp_Check( "nothing answers just left of Add", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, rcAddT.left - 1 ) = EXPICON_NONE )
        Exp_Check( "the caption area hits no icon", _
            frmExplorer_IconHitTest( HWND_FRMEXPLORER, nRowAlpha, 0 ) = EXPICON_NONE )

        ' A group that takes no folders must offer nothing at all, or Main would sprout an
        ' Add icon that creates a folder the model refuses.
        dim as long nRootMain = -1
        for i as long = 0 to PsListTree_GetCount( HWND_FRMEXPLORER ) - 1
            if frmExplorer_KindFromRow( HWND_FRMEXPLORER, i ) <> EXPKIND_ROOT then continue for
            if PsListTree_GetItemData( HWND_FRMEXPLORER, i ) = CATINDEX_MAIN then nRootMain = i
        next
        if nRootMain >= 0 then
            frmExplorer_IconRects( HWND_FRMEXPLORER, nRootMain, rcRowT, rcAddT, rcRenT, rcDelT )
            Exp_Check( "a non-foldering group offers no icons", _
                (IsRectEmpty( @rcAddT ) <> 0) andalso (IsRectEmpty( @rcDelT ) <> 0) )
        end if

        ' --- the editor lands where the caption will render --------------------------------------
        ' The painter indents rows by hand; the CONTROL positions the in-place editor from its
        ' own tree indent, which returns 0 flat while bTreeIndent is off. Both must be on and
        ' both must use the same unit, or a rename opens against the left margin -- which is
        ' how it shipped. Asserted against the control rather than against the constant, since
        ' the constant agreeing with itself proves nothing.
        Exp_Check( "the control's tree indent is enabled for the editor", _
            PsListTree_GetTreeIndent( HWND_FRMEXPLORER ) )
        Exp_Check( "the control indents by the same unit the painter does", _
            PsListTree_GetIndentWidth( HWND_FRMEXPLORER ) = EXPLORER_INDENT_UNITS, _
            "control=" & str(PsListTree_GetIndentWidth( HWND_FRMEXPLORER )) & _
            " painter=" & str(EXPLORER_INDENT_UNITS) )
        ' A nested folder's editor must start further right than a top-level one's, which is
        ' the property the whole arrangement exists for and is not implied by the two above.
        Exp_Check( "a deeper folder indents further than a shallower one", _
            PsListTree_GetLevel( HWND_FRMEXPLORER, nRowDeep ) > _
            PsListTree_GetLevel( HWND_FRMEXPLORER, nRowAlpha ) )

        ' --- the tooltip backend is reachable ----------------------------------------------------
        ' The per-icon tips are re-resolved by calling PsTooltip_SetToolIndex on the PsTooltip
        ' window when the cursor crosses from one icon to the next. If that handle is zero the
        ' call is silently skipped and the ONLY remaining re-resolve is the control's own, which
        ' fires on a ROW change -- exactly the "leave the row and come back" symptom.
        Exp_Check( "the Explorer is on the PsTooltip backend", _
            PsListTree_GetTooltipMode( HWND_FRMEXPLORER ) = PSTIP_MODE_PS, _
            "mode=" & str(PsListTree_GetTooltipMode( HWND_FRMEXPLORER )) )
        Exp_Check( "the PsTooltip window exists to be re-indexed", _
            PsListTree_GetPsTooltipHandle( HWND_FRMEXPLORER ) <> 0 )

        ' The re-offer toggles the index through -1 and back, because SetToolIndex opens with
        ' "if nToolIndex = idx then exit sub" and crossing between two icons never changes the
        ' row. What CANNOT be allowed to change is where the index ends up: PsListTree resolves
        ' its tip text by casting that index straight to a row, so a toggle that stopped at -1
        ' would make every row tip resolve against row -1. Not a test of the early return --
        ' that needs the tip's own 100 ms clock -- but it does pin the invariant a future
        ' "simplify this to one call" edit would break.
        dim as HWND hTipT = PsListTree_GetPsTooltipHandle( HWND_FRMEXPLORER )
        if hTipT then
            PsTooltip_SetToolIndex( hTipT, 7 )
            PsTooltip_SetToolIndex( hTipT, -1 )
            PsTooltip_SetToolIndex( hTipT, 7 )
            Exp_Check( "re-offering a tool leaves the index on that tool", _
                PsTooltip_GetToolIndex( hTipT ) = 7, _
                "idx=" & str(PsTooltip_GetToolIndex( hTipT )) )
            PsTooltip_SetToolIndex( hTipT, -1 )
        end if

        ' --- the icons' tooltips ---------------------------------------------------------------
        ' The tip is resolved from the tracked zone, not from the row alone, because the
        ' callback is handed no x and this list defines no columns for col to distinguish.
        ' Driven here by setting the zone the way the mouse-move arm does.
        gExpTipRow = nRowAlpha
        gExpTipIcon = EXPICON_ADD
        Exp_Check( "the Add icon names itself", _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) = L(462,"New Folder"), _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) )
        gExpTipIcon = EXPICON_DELETE
        Exp_Check( "the Delete icon names itself", _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) = L(463,"Delete Folder"), _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) )
        ' The two tips must actually DIFFER. Both resolving to the same string would satisfy
        ' each assertion above on its own if the ids ever collided or one went blank.
        dim as DWSTRING wszTipDel = frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 )
        gExpTipIcon = EXPICON_ADD
        Exp_Check( "the two icon tips are different text", _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) <> wszTipDel )
        ' A stale zone must never leak onto another row -- the row is stored with it for
        ' exactly this reason.
        Exp_Check( "an icon tip does not leak onto another row", _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRootNormal, 0 ) <> L(462,"New Folder") )
        gExpTipIcon = EXPICON_RENAME
        Exp_Check( "the Rename icon names itself", _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) = L(464,"Rename Folder"), _
            frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) )
        gExpTipIcon = EXPICON_NONE
        gExpTipRow = -1
        Exp_Check( "with no icon under the cursor a folder row has no tip", _
            len( frmExplorer_TooltipCallback( HWND_FRMEXPLORER, nRowAlpha, 0 ) ) = 0 )

        ' --- rebase: the rule shared by rename and delete -------------------------------------
        Exp_Check( "rebasing maps the folder itself", _
            ProjectFolders_RebasePath("a/b", "a/b", "a/x") = "a/x" )
        Exp_Check( "rebasing carries descendants", _
            ProjectFolders_RebasePath("a/b/c", "a/b", "a/x") = "a/x/c" )
        Exp_Check( "rebasing leaves unrelated paths alone", _
            ProjectFolders_RebasePath("a/z", "a/b", "a/x") = "a/z" )
        ' The same trap as descent: a plain prefix would rewrite "a/bb" when renaming "a/b".
        Exp_Check( "rebasing does not touch a shared name prefix", _
            ProjectFolders_RebasePath("a/bb", "a/b", "a/x") = "a/bb", _
            ProjectFolders_RebasePath("a/bb", "a/b", "a/x") )
        ' Dissolving is rebasing onto the parent -- which is what makes delete and rename one
        ' implementation instead of two path walks that could disagree.
        Exp_Check( "dissolving empties the folder path", _
            ProjectFolders_RebasePath("a/b", "a/b", "a") = "a" )
        Exp_Check( "dissolving lifts descendants one level", _
            ProjectFolders_RebasePath("a/b/c", "a/b", "a") = "a/c" )
        Exp_Check( "dissolving a top-level folder empties its own path", _
            ProjectFolders_RebasePath("b", "b", "") = "" )
        Exp_Check( "dissolving a top-level folder lifts its children to the top", _
            ProjectFolders_RebasePath("b/c", "b", "") = "c" )

        ' --- rename and delete against the real table -----------------------------------------
        ProjectFolders_Clear()
        ProjectFolders_Add( catTest, "one/two/three" )
        ProjectFolders_Rebase( catTest, "one/two", "one/deux" )
        Exp_Check( "renaming a folder renames it", _
            ProjectFolders_Exists( catTest, "one/deux" ) andalso _
            ( ProjectFolders_Exists( catTest, "one/two" ) = false ) )
        Exp_Check( "renaming a folder carries its subtree", _
            ProjectFolders_Exists( catTest, "one/deux/three" ), "count=" & str(ProjectFolders_Count()) )

        ' Deleting re-parents rather than destroying: "three" must survive one level up.
        ProjectFolders_Rebase( catTest, "one/deux", ProjectFolders_ParentPath("one/deux") )
        Exp_Check( "deleting a folder removes it", ProjectFolders_Exists( catTest, "one/deux" ) = false )
        Exp_Check( "deleting a folder re-parents its children", _
            ProjectFolders_Exists( catTest, "one/three" ), "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "deleting leaves the surviving ancestor alone", ProjectFolders_Exists( catTest, "one" ) )

        ' A re-parented child landing on an existing name MERGES, which is the only sensible
        ' reading -- and must not leave two records claiming one path.
        ProjectFolders_Clear()
        ProjectFolders_Add( catTest, "p/dup" )
        ProjectFolders_Add( catTest, "dup" )
        ProjectFolders_Rebase( catTest, "p", "" )
        Exp_Check( "a collision on re-parent merges to one record", _
            ProjectFolders_Count() = 1, "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "the merged record is the surviving path", ProjectFolders_Exists( catTest, "dup" ) )

        ' --- the drop legality matrix is GONE --------------------------------------------------
        ' frmExplorer_ExtAllowedInCat and the fourteen assertions over its (extension x group)
        ' table were deleted with the rule itself: any file may now be dragged into any group.
        ' Nothing replaces them, because "everything is allowed" has no table to assert. What
        ' still constrains a drop -- one file in Main/Resource, no folders in either, no folder
        ' into its own descendant -- is asserted where those rules live.

        ' --- moving a folder subtree ----------------------------------------------------------
        ProjectFolders_Clear()
        ProjectFolders_Add( CATINDEX_NORMAL, "src/net/http" )
        ProjectFolders_Add( CATINDEX_NORMAL, "lib" )

        ' Into its own descendant is the classic tree-move corruption: the new parent is about
        ' to stop existing at that path.
        Exp_Check( "a folder cannot move into its own descendant", _
            ProjectFolders_MoveFolder( CATINDEX_NORMAL, "src", CATINDEX_NORMAL, "src/net" ) = false )
        Exp_Check( "a folder cannot move into itself", _
            ProjectFolders_MoveFolder( CATINDEX_NORMAL, "src", CATINDEX_NORMAL, "src" ) = false )
        Exp_Check( "the refused moves changed nothing", _
            ProjectFolders_Exists( CATINDEX_NORMAL, "src/net/http" ) )

        Exp_Check( "a folder moves under a new parent", _
            ProjectFolders_MoveFolder( CATINDEX_NORMAL, "src/net", CATINDEX_NORMAL, "lib" ) )
        Exp_Check( "moving a folder carries its whole subtree", _
            ProjectFolders_Exists( CATINDEX_NORMAL, "lib/net" ) andalso _
            ProjectFolders_Exists( CATINDEX_NORMAL, "lib/net/http" ) andalso _
            ( ProjectFolders_Exists( CATINDEX_NORMAL, "src/net" ) = false ), _
            "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "moving a folder leaves its old parent standing", _
            ProjectFolders_Exists( CATINDEX_NORMAL, "src" ) )

        ' Across groups the CATEGORY has to move too, not just the path -- identity is
        ' (category, path), so a subtree left holding the old category would vanish from the
        ' destination and reappear under the source.
        Exp_Check( "a folder moves to another group", _
            ProjectFolders_MoveFolder( CATINDEX_NORMAL, "lib/net", CATINDEX_MODULE, "" ) )
        Exp_Check( "a cross-group move carries the subtree's category", _
            ProjectFolders_Exists( CATINDEX_MODULE, "net" ) andalso _
            ProjectFolders_Exists( CATINDEX_MODULE, "net/http" ) andalso _
            ( ProjectFolders_Exists( CATINDEX_NORMAL, "lib/net" ) = false ), _
            "count=" & str(ProjectFolders_Count()) )
        Exp_Check( "a folder cannot move into a group that takes none", _
            ProjectFolders_MoveFolder( CATINDEX_MODULE, "net", CATINDEX_MAIN, "" ) = false )
        ProjectFolders_Clear()

        ' --- the localized strings --------------------------------------------------------------
        ' Printed, not merely length-checked: LoadLocalizationFile fills a blank key with the
        ' English text, so len() > 0 passes under every language even when that language has
        ' the slot empty. The value is what distinguishes a translation from the fallback.
        Exp_Check( "the New Folder caption resolves", len(L(462,"New Folder")) > 0, L(462,"") )
        Exp_Check( "the Delete Folder caption resolves", len(L(463,"Delete Folder")) > 0, L(463,"") )

    end if

    ' --- put the workspace back exactly as it was found -------------------------------------
    ProjectFolders_Clear()
    for i as long = lbound(savedFolders) to ubound(savedFolders)
        if nSavedFolders = 0 then exit for
        redim preserve gProjectFolders(i)
        gProjectFolders(i) = savedFolders(i)
    next
    for i as long = lbound(savedDocs) to ubound(savedDocs)
        if savedDocs(i) then savedDocs(i)->docData.wszFolder = savedDocFolders(i)
    next
    LoadExplorerFiles()

    Exp_Check( "the suite restores the workspace's own folders", _
        ProjectFolders_Count() = nSavedFolders, _
        "restored=" & str(ProjectFolders_Count()) & " saved=" & str(nSavedFolders) )
    ' Count alone would be satisfied by the right NUMBER of wrong folders, so the paths are
    ' checked too -- the suite renames and dissolves folders, and a restore that put back
    ' three records with rebased paths would look correct by count.
    dim as boolean bPathsOK = true
    for i as long = 0 to nSavedFolders - 1
        if ProjectFolders_Exists( savedFolders(i).catIndex, savedFolders(i).wszPath ) = false then bPathsOK = false
    next
    Exp_Check( "the restored folders have their original paths", bPathsOK )
    dim as boolean bDocsOK = true
    for i as long = lbound(savedDocs) to ubound(savedDocs)
        if savedDocs(i) = 0 then continue for
        if savedDocs(i)->docData.wszFolder <> savedDocFolders(i) then bDocsOK = false
    next
    Exp_Check( "the restored documents keep their own folders", bDocsOK )

    ' Through a `dim as string` first, like Exp_Check does. A direct print of a concat can
    ' come out UTF-16 with interleaved nulls, which renders fine in a console and is
    ' invisible to every text tool that reads the redirected output -- so the one line that
    ' reports whether the suite passed was the one line that could not be grepped.
    dim as string sTotals = "--- TIKO_EXPLORER_SELFTEST: " & str(gExpTestPass) & _
                            " passed, " & str(gExpTestFail) & " failed ---"
    print sTotals
end sub
