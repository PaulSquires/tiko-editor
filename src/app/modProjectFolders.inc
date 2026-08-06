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
''  modProjectFolders.inc
''

#include once "modProjectFolders.bi"


' ========================================================================================
' Put a path into the one form every comparison here assumes: no leading, trailing or
' doubled separator, no surrounding whitespace, and backslashes folded onto the separator
' (a user typing "net\http" means the same thing they mean by "net/http", and letting both
' spellings into the table would produce two folders that render identically).
'
' The empty string is legal and means "the group's own top level" -- not a folder.
' ========================================================================================
function ProjectFolders_Normalize( byval wszPath as DWSTRING ) as DWSTRING
    dim as DWSTRING wszOut, wszSeg
    dim as long nLen

    wszPath = PsTrim(wszPath)
    nLen = PsLen(wszPath)
    if nLen = 0 then return ""

    ' Walk segment by segment rather than pattern-replacing: it collapses runs of
    ' separators and drops empty segments in one pass, so "//a///b//" needs no cleanup after.
    for i as long = 1 to nLen + 1
        dim as DWSTRING wszCh = iif( i > nLen, DWSTRING(PROJECTFOLDER_SEP), PsMid(wszPath, i, 1) )
        dim as boolean bIsSep = false
        if wszCh = PROJECTFOLDER_SEP then bIsSep = true
        if wszCh = "\" then bIsSep = true
        if bIsSep then
            ' PsTrim/PsLen, not trim/len. The intrinsics work on AfxNova's DWSTRING
            ' because it EXTENDS WSTRING; PsCore's does not, and `len` on it is
            ' the silent one -- it compiles and returns SizeOf, a plausible 24.
            ' Both forward through PsCompat.bi in the tiko build.
            wszSeg = PsTrim(wszSeg)
            if PsLen(wszSeg) then
                if PsLen(wszOut) then wszOut = wszOut + PROJECTFOLDER_SEP
                wszOut = wszOut + wszSeg
            end if
            wszSeg = ""
        else
            wszSeg = wszSeg + wszCh
        end if
    next

    return wszOut
end function


' ========================================================================================
' The path of the folder one level up, or "" when the folder sits at a group's top level.
' ========================================================================================
function ProjectFolders_ParentPath( byval wszPath as DWSTRING ) as DWSTRING
    wszPath = ProjectFolders_Normalize( wszPath )
    dim as long nPos = PsInStrRev( wszPath, PROJECTFOLDER_SEP )
    if nPos = 0 then return ""
    return PsLeft( wszPath, nPos - 1 )
end function


' ========================================================================================
' The folder's own name, without any of its ancestors.
' ========================================================================================
function ProjectFolders_LeafName( byval wszPath as DWSTRING ) as DWSTRING
    wszPath = ProjectFolders_Normalize( wszPath )
    dim as long nPos = PsInStrRev( wszPath, PROJECTFOLDER_SEP )
    if nPos = 0 then return wszPath
    return PsMid( wszPath, nPos + 1 )
end function


' ========================================================================================
' How many levels deep the folder is. "" is 0 (a group's top level is not a folder).
' ========================================================================================
function ProjectFolders_Depth( byval wszPath as DWSTRING ) as long
    wszPath = ProjectFolders_Normalize( wszPath )
    if PsLen(wszPath) = 0 then return 0
    dim as long nCount = 1
    for i as long = 1 to PsLen(wszPath)
        if PsMid(wszPath, i, 1) = PROJECTFOLDER_SEP then nCount += 1
    next
    return nCount
end function


' ========================================================================================
' Append a leaf name to a parent path. An empty parent yields the leaf on its own, which is
' what makes "" usable as the top level without a special case at every call site.
' ========================================================================================
function ProjectFolders_Combine( byval wszParent as DWSTRING, byval wszLeaf as DWSTRING ) as DWSTRING
    wszParent = ProjectFolders_Normalize( wszParent )
    wszLeaf   = ProjectFolders_Normalize( wszLeaf )
    if PsLen(wszParent) = 0 then return wszLeaf
    if PsLen(wszLeaf) = 0 then return wszParent
    return wszParent + PROJECTFOLDER_SEP + wszLeaf
end function


' ========================================================================================
' Is wszPath somewhere BELOW wszAncestor?
'
' A folder is not a descendant of itself -- the callers that matter are "may this folder be
' dropped into that one" and "does deleting this take that with it", and both want a strict
' test.
'
' THE COMPARISON MUST LAND ON A SEPARATOR BOUNDARY. A plain prefix test says "ab/c" is
' inside "a", which is the classic way this function is written wrongly: the drop validator
' would then refuse a perfectly legal move between two folders whose names merely share a
' first letter. Comparing against ancestor + separator is what forces the boundary.
'
' The empty ancestor is every folder's ancestor -- it is the group's top level.
' ========================================================================================
function ProjectFolders_IsDescendantOf( byval wszPath as DWSTRING, byval wszAncestor as DWSTRING ) as boolean
    wszPath     = ProjectFolders_Normalize( wszPath )
    wszAncestor = ProjectFolders_Normalize( wszAncestor )

    if PsLen(wszPath) = 0 then return false
    if PsLen(wszAncestor) = 0 then return true
    if PsUCase(wszPath) = PsUCase(wszAncestor) then return false

    dim as DWSTRING wszPrefix = PsUCase(wszAncestor) + PROJECTFOLDER_SEP
    if PsLen(wszPath) <= PsLen(wszPrefix) then return false
    if PsLeft( PsUCase(wszPath), PsLen(wszPrefix) ) = wszPrefix then return true
    return false
end function


' ========================================================================================
' Is this acceptable as a single folder name (one segment, not a path)?
'
' Only the separator characters are refused, plus the empty name. These folders are virtual,
' so nothing else the filesystem would object to matters here -- a name with a colon or a
' question mark in it is never used to open anything.
' ========================================================================================
function ProjectFolders_IsValidName( byval wszName as DWSTRING ) as boolean
    wszName = PsTrim(wszName)
    if PsLen(wszName) = 0 then return false
    if PsInStr( wszName, PROJECTFOLDER_SEP ) > 0 then return false
    if PsInStr( wszName, "\" ) > 0 then return false
    return true
end function


' ========================================================================================
' May this root group hold subfolders?
'
' Main and Resource hold exactly one file each (clsApp demotes any previous holder), so a
' folder under either of them would be a container for at most one thing. Deliberately
' excluded rather than merely discouraged: it keeps the add-folder icon off those two rows
' and keeps the drop validator from having to reason about a folder that cannot be useful.
' ========================================================================================
function ProjectFolders_CatAllowsFolders( byval catIndex as long ) as boolean
    select case catIndex
        case CATINDEX_HEADER, CATINDEX_MODULE, CATINDEX_NORMAL: return true
        case else: return false
    end select
end function


' ========================================================================================
' Drop every folder. Called when a workspace closes or before another one loads.
' ========================================================================================
sub ProjectFolders_Clear()
    erase gProjectFolders
end sub


function ProjectFolders_Count() as long
    return ubound(gProjectFolders) - lbound(gProjectFolders) + 1
end function


' ========================================================================================
' The table index of (category, path), or -1.
'
' Case-INSENSITIVE, because these are shown alongside filenames on Windows and a user who
' typed "Net" once and "net" later means one folder both times. The stored spelling is the
' one first created; nothing rewrites it.
' ========================================================================================
function ProjectFolders_Find( byval catIndex as long, byval wszPath as DWSTRING ) as long
    wszPath = ProjectFolders_Normalize( wszPath )
    if PsLen(wszPath) = 0 then return -1
    dim as DWSTRING wszWanted = PsUCase(wszPath)
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex = catIndex then
            if PsUCase(gProjectFolders(i).wszPath) = wszWanted then return i
        end if
    next
    return -1
end function


function ProjectFolders_Exists( byval catIndex as long, byval wszPath as DWSTRING ) as boolean
    return ( ProjectFolders_Find( catIndex, wszPath ) >= 0 )
end function


' ========================================================================================
' Add a folder, and every ancestor it needs. Returns its table index, or -1 if refused.
'
' IDEMPOTENT -- adding one that already exists returns the existing index rather than a
' duplicate, which is what lets the .tiko loader add "a/b" and "a" in either order and still
' end up with one of each.
'
' Ancestors are created implicitly, so a path may be added without walking it first. This is
' the load path's safety net as much as a convenience: a .tiko carrying "a/b" but missing
' its "a" record (hand-edited, or written by a build that only stored occupied folders)
' would otherwise render "a/b" as an orphan with no parent to sit under.
' ========================================================================================
function ProjectFolders_Add( byval catIndex as long, byval wszPath as DWSTRING ) as long
    if ProjectFolders_CatAllowsFolders( catIndex ) = false then return -1

    wszPath = ProjectFolders_Normalize( wszPath )
    if PsLen(wszPath) = 0 then return -1

    dim as long nExisting = ProjectFolders_Find( catIndex, wszPath )
    if nExisting >= 0 then return nExisting

    ' Ancestors first, so the table is never in a state where a child is present and its
    ' parent is not. The recursion is bounded by the path's depth.
    dim as DWSTRING wszParent = ProjectFolders_ParentPath( wszPath )
    if PsLen(wszParent) then ProjectFolders_Add( catIndex, wszParent )

    dim as long ub = ubound(gProjectFolders) + 1
    redim preserve gProjectFolders(ub)
    gProjectFolders(ub).catIndex = catIndex
    gProjectFolders(ub).wszPath  = wszPath
    return ub
end function


' ========================================================================================
' Remove one folder record. The caller owns everything else -- re-parenting the children,
' moving the files -- because only it knows what the gesture meant.
'
' COMPACTS rather than swapping with the last entry, so the table keeps its insertion order
' and an ancestor always precedes its descendants. Note the corollary: every index at or
' above idx shifts down, so an index read before this call is stale after it.
' ========================================================================================
function ProjectFolders_RemoveAt( byval idx as long ) as boolean
    if idx < lbound(gProjectFolders) then return false
    if idx > ubound(gProjectFolders) then return false

    for i as long = idx to ubound(gProjectFolders) - 1
        gProjectFolders(i) = gProjectFolders(i + 1)
    next

    dim as long ub = ubound(gProjectFolders) - 1
    if ub < 0 then
        erase gProjectFolders
    else
        redim preserve gProjectFolders(ub)
    end if
    return true
end function


' ========================================================================================
' Map one path through a rename or a dissolve, leaving unrelated paths alone.
'
'   P = old              -> new          (the folder itself)
'   P below old          -> new + P's tail beneath old
'   anything else        -> unchanged
'
' An empty wszNew DISSOLVES: "a/b" with old="a/b" new="" yields "", and "a/b/c" yields "c".
' That is what makes delete and rename one implementation rather than two -- deleting "a/b"
' is exactly rebasing it onto its own parent, and the record that maps onto the parent is
' then a duplicate and is dropped.
'
' Pure. The table is not consulted, so the rule can be asserted on its own.
' ========================================================================================
function ProjectFolders_RebasePath( byval wszPath as DWSTRING, byval wszOld as DWSTRING, _
                                    byval wszNew as DWSTRING ) as DWSTRING
    wszPath = ProjectFolders_Normalize( wszPath )
    wszOld  = ProjectFolders_Normalize( wszOld )
    wszNew  = ProjectFolders_Normalize( wszNew )

    if PsLen(wszOld) = 0 then return wszPath
    if PsUCase(wszPath) = PsUCase(wszOld) then return wszNew
    if ProjectFolders_IsDescendantOf( wszPath, wszOld ) then
        ' +2 skips the ancestor and the separator that follows it. IsDescendantOf has
        ' already guaranteed that separator is really there.
        return ProjectFolders_Combine( wszNew, PsMid( wszPath, PsLen(wszOld) + 2 ) )
    end if
    return wszPath
end function


' ========================================================================================
' Rename a folder, or dissolve it into its parent when wszNewPath is empty.
'
' Rewrites the folder's own record, EVERY DESCENDANT FOLDER, and every document sitting in
' any of them. Missing any of those three leaves the workspace inconsistent in a way that
' only shows up later: an orphaned subfolder renders nowhere, and a document left pointing
' at the old path is silently pulled back to its group's top level by the Explorer's
' normalization pass.
'
' Paths can COLLIDE as a result -- dissolving "a/b" turns "a/b/c" into "a/c", which may
' already exist -- and the two folders then MERGE, which is the only sensible reading of the
' gesture. The merge is done by dropping the duplicate record; the documents in both already
' name the same path by then, so nothing else has to move.
' ========================================================================================
' pDocHead and wszFileType are PASSED IN rather than read from the two shell
' globals this used to reach for. Those are declared in tiko.bas, so touching
' them tied this file to the shell -- and src/app exists to not be. The document
' TYPE is fine: since clsDocument.bi stopped naming Win32, the app layer sees it.
function ProjectFolders_Rebase( byval catIndex as long, byval wszOldPath as DWSTRING, _
                                byval wszNewPath as DWSTRING, _
                                byval pDocHead as clsDocument ptr, _
                                byval wszFileType as DWSTRING ) as boolean
    wszOldPath = ProjectFolders_Normalize( wszOldPath )
    wszNewPath = ProjectFolders_Normalize( wszNewPath )
    if PsLen(wszOldPath) = 0 then return false
    if ProjectFolders_Find( catIndex, wszOldPath ) < 0 then return false

    ' Documents first, while the old paths are still in the table to be recognised.
    dim pDoc as clsDocument ptr = pDocHead
    do until pDoc = 0
        if pDoc->ProjectFiletype = wszFileType then
            if PsLen(pDoc->docData.wszFolder) then
                pDoc->docData.wszFolder = _
                    ProjectFolders_RebasePath( pDoc->docData.wszFolder, wszOldPath, wszNewPath )
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    ' The folder and its descendants, in place -- rewriting rather than removing and
    ' re-adding keeps the table's insertion order, which is what keeps an ancestor ahead of
    ' its descendants.
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex <> catIndex then continue for
        gProjectFolders(i).wszPath = _
            ProjectFolders_RebasePath( gProjectFolders(i).wszPath, wszOldPath, wszNewPath )
    next

    ' Sweep out anything the rewrite emptied (the dissolved folder itself) or duplicated
    ' (a descendant that landed on an existing path). Backwards, so removal's compaction
    ' cannot skip an entry.
    for i as long = ubound(gProjectFolders) to lbound(gProjectFolders) step -1
        if gProjectFolders(i).catIndex <> catIndex then continue for
        dim as boolean bDrop = false
        if PsLen(gProjectFolders(i).wszPath) = 0 then bDrop = true
        if bDrop = false then
            for j as long = lbound(gProjectFolders) to i - 1
                if gProjectFolders(j).catIndex <> catIndex then continue for
                if PsUCase(gProjectFolders(j).wszPath) = PsUCase(gProjectFolders(i).wszPath) then
                    bDrop = true
                    exit for
                end if
            next
        end if
        if bDrop then ProjectFolders_RemoveAt( i )
    next

    return true
end function


' ========================================================================================
' Move a folder -- and everything beneath it -- to a new parent, possibly under a different
' root group.
'
' Same-group moves are the ordinary drag; CROSS-GROUP moves are why this is not just
' ProjectFolders_Rebase with a different argument. A folder's identity is (category, path),
' so carrying it into another group has to rewrite the CATEGORY of the folder and of every
' descendant as well as their paths -- and the documents inside them need their
' ProjectFileType changed too, which is the CALLER's job because only it can route that
' through gApp.ProjectSetFileType.
'
' The caller must therefore collect the affected documents BEFORE calling this: afterwards
' their folders name the destination and there is no longer anything to identify them by.
'
' Refuses a move INTO ITS OWN SUBTREE, which would otherwise reparent a folder under a child
' that is about to stop existing at that path -- the classic tree-move corruption.
'
' Paths can collide with something already at the destination, and they MERGE, exactly as a
' dissolve does; the duplicate record is dropped and the documents in both already name the
' same path by then.
' ========================================================================================
' Same inversion as ProjectFolders_Rebase above.
function ProjectFolders_MoveFolder( byval catFrom as long, byval wszPath as DWSTRING, _
                                    byval catTo as long, byval wszNewParent as DWSTRING, _
                                    byval pDocHead as clsDocument ptr, _
                                    byval wszFileType as DWSTRING ) as boolean
    wszPath      = ProjectFolders_Normalize( wszPath )
    wszNewParent = ProjectFolders_Normalize( wszNewParent )

    if PsLen(wszPath) = 0 then return false
    if ProjectFolders_CatAllowsFolders( catTo ) = false then return false
    if ProjectFolders_Find( catFrom, wszPath ) < 0 then return false

    ' Into itself or into its own descendant -- only meaningful within one group, since a
    ' path under another category is a different folder however alike it reads.
    if catFrom = catTo then
        if PsUCase(wszNewParent) = PsUCase(wszPath) then return false
        if ProjectFolders_IsDescendantOf( wszNewParent, wszPath ) then return false
    end if

    dim as DWSTRING wszNewPath = ProjectFolders_Combine( wszNewParent, ProjectFolders_LeafName( wszPath ) )

    ' A same-group move that changes nothing is a no-op rather than a failure: dropping a
    ' folder back where it already sits is a legitimate gesture with no effect.
    if (catFrom = catTo) andalso (PsUCase(wszNewPath) = PsUCase(wszPath)) then return true

    ' Documents first, while their folders still name the source paths.
    dim pDoc as clsDocument ptr = pDocHead
    do until pDoc = 0
        if pDoc->ProjectFiletype = wszFileType then
            if PsLen(pDoc->docData.wszFolder) then
                dim as DWSTRING wszWas = pDoc->docData.wszFolder
                dim as boolean bInSubtree = false
                if PsUCase(wszWas) = PsUCase(wszPath) then bInSubtree = true
                if ProjectFolders_IsDescendantOf( wszWas, wszPath ) then bInSubtree = true
                if bInSubtree then
                    pDoc->docData.wszFolder = ProjectFolders_RebasePath( wszWas, wszPath, wszNewPath )
                end if
            end if
        end if
        pDoc = pDoc->pDocNext
    loop

    ' Then the folder records: path AND category, in place so the table keeps its insertion
    ' order and an ancestor still precedes its descendants.
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex <> catFrom then continue for
        dim as DWSTRING wszWas = gProjectFolders(i).wszPath
        dim as boolean bMine = false
        if PsUCase(wszWas) = PsUCase(wszPath) then bMine = true
        if ProjectFolders_IsDescendantOf( wszWas, wszPath ) then bMine = true
        if bMine = false then continue for
        gProjectFolders(i).wszPath  = ProjectFolders_RebasePath( wszWas, wszPath, wszNewPath )
        gProjectFolders(i).catIndex = catTo
    next

    ' Any ancestor the destination needs but does not have -- dropping into a group's top
    ' level needs none, but a path can arrive ahead of its parent when groups differ.
    if PsLen(wszNewParent) then ProjectFolders_Add( catTo, wszNewParent )

    ' Sweep duplicates created by a collision at the destination. Backwards, so removal's
    ' compaction cannot skip an entry.
    for i as long = ubound(gProjectFolders) to lbound(gProjectFolders) step -1
        dim as boolean bDrop = false
        if PsLen(gProjectFolders(i).wszPath) = 0 then bDrop = true
        if bDrop = false then
            for j as long = lbound(gProjectFolders) to i - 1
                if gProjectFolders(j).catIndex <> gProjectFolders(i).catIndex then continue for
                if PsUCase(gProjectFolders(j).wszPath) = PsUCase(gProjectFolders(i).wszPath) then
                    bDrop = true
                    exit for
                end if
            next
        end if
        if bDrop then ProjectFolders_RemoveAt( i )
    next

    return true
end function
