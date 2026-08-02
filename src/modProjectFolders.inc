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

    wszPath = trim(wszPath)
    nLen = len(wszPath)
    if nLen = 0 then return ""

    ' Walk segment by segment rather than pattern-replacing: it collapses runs of
    ' separators and drops empty segments in one pass, so "//a///b//" needs no cleanup after.
    for i as long = 1 to nLen + 1
        dim as DWSTRING wszCh = iif( i > nLen, DWSTRING(PROJECTFOLDER_SEP), mid(wszPath, i, 1) )
        dim as boolean bIsSep = false
        if wszCh = PROJECTFOLDER_SEP then bIsSep = true
        if wszCh = "\" then bIsSep = true
        if bIsSep then
            wszSeg = trim(wszSeg)
            if len(wszSeg) then
                if len(wszOut) then wszOut = wszOut + PROJECTFOLDER_SEP
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
    dim as long nPos = instrrev( wszPath, PROJECTFOLDER_SEP )
    if nPos = 0 then return ""
    return left( wszPath, nPos - 1 )
end function


' ========================================================================================
' The folder's own name, without any of its ancestors.
' ========================================================================================
function ProjectFolders_LeafName( byval wszPath as DWSTRING ) as DWSTRING
    wszPath = ProjectFolders_Normalize( wszPath )
    dim as long nPos = instrrev( wszPath, PROJECTFOLDER_SEP )
    if nPos = 0 then return wszPath
    return mid( wszPath, nPos + 1 )
end function


' ========================================================================================
' How many levels deep the folder is. "" is 0 (a group's top level is not a folder).
' ========================================================================================
function ProjectFolders_Depth( byval wszPath as DWSTRING ) as long
    wszPath = ProjectFolders_Normalize( wszPath )
    if len(wszPath) = 0 then return 0
    dim as long nCount = 1
    for i as long = 1 to len(wszPath)
        if mid(wszPath, i, 1) = PROJECTFOLDER_SEP then nCount += 1
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
    if len(wszParent) = 0 then return wszLeaf
    if len(wszLeaf) = 0 then return wszParent
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

    if len(wszPath) = 0 then return false
    if len(wszAncestor) = 0 then return true
    if ucase(wszPath) = ucase(wszAncestor) then return false

    dim as DWSTRING wszPrefix = ucase(wszAncestor) + PROJECTFOLDER_SEP
    if len(wszPath) <= len(wszPrefix) then return false
    if left( ucase(wszPath), len(wszPrefix) ) = wszPrefix then return true
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
    wszName = trim(wszName)
    if len(wszName) = 0 then return false
    if instr( wszName, PROJECTFOLDER_SEP ) > 0 then return false
    if instr( wszName, "\" ) > 0 then return false
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
    if len(wszPath) = 0 then return -1
    dim as DWSTRING wszWanted = ucase(wszPath)
    for i as long = lbound(gProjectFolders) to ubound(gProjectFolders)
        if gProjectFolders(i).catIndex = catIndex then
            if ucase(gProjectFolders(i).wszPath) = wszWanted then return i
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
    if len(wszPath) = 0 then return -1

    dim as long nExisting = ProjectFolders_Find( catIndex, wszPath )
    if nExisting >= 0 then return nExisting

    ' Ancestors first, so the table is never in a state where a child is present and its
    ' parent is not. The recursion is bounded by the path's depth.
    dim as DWSTRING wszParent = ProjectFolders_ParentPath( wszPath )
    if len(wszParent) then ProjectFolders_Add( catIndex, wszParent )

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
