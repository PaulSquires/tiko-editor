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
''  modProjectFolders.bi
''
''  User-created subfolders inside the Explorer's root groups.
''
''  A folder here is VIRTUAL -- a grouping that lives in the .tiko file, never a directory on
''  disk. Nothing is created, moved or deleted in the filesystem by any of this.
''
''  IDENTITY IS (category, path), not path alone. "net" under Module and "net" under Normal
''  are two unrelated folders, which is what lets a file carry its folder without also having
''  to carry which group it belongs to -- the group is already in its ProjectFileType.
''
''  THE TABLE EXISTS SO THAT AN EMPTY FOLDER SURVIVES A RELOAD. Folder membership could
''  otherwise be derived entirely from the files' own paths, which is smaller and needs no
''  persistence of its own -- but then a folder the user creates and has not yet filled
''  vanishes the next time the project is opened, and creating a folder before moving files
''  into it is the obvious order to work in.
''
''  WHY THIS LIVES IN ITS OWN MODULE rather than in frmExplorer: clsConfig.inc reads and
''  writes the .tiko and is included long before any frm* file, so it cannot name anything
''  the Explorer declares. This module is included ahead of clsConfig.inc for that reason.
''

#pragma once

' The path separator. "/" deliberately, not "\": these are not disk paths and should not
' read as though they were, and it keeps the fbc "/ vs \" confusion out of the comparisons.
#define PROJECTFOLDER_SEP   "/"


type PROJECT_FOLDER
    catIndex as long        ' CATINDEX_HEADER / CATINDEX_MODULE / CATINDEX_NORMAL
    wszPath  as DWSTRING    ' "net" or "net/http". Never empty, never with a leading,
                            ' trailing or doubled separator -- Normalize guarantees it.
end type

' The live table for the open workspace. Cleared and refilled by the project load, so an
' index into it is only valid until the next load -- resolve a row's path from its index
' BEFORE mutating the table, never after.
dim shared gProjectFolders(any) as PROJECT_FOLDER


' --- pure path helpers (no table access, so they are assertable on their own) -------------
declare function ProjectFolders_Normalize( byval wszPath as DWSTRING ) as DWSTRING
declare function ProjectFolders_ParentPath( byval wszPath as DWSTRING ) as DWSTRING
declare function ProjectFolders_LeafName( byval wszPath as DWSTRING ) as DWSTRING
declare function ProjectFolders_Depth( byval wszPath as DWSTRING ) as long
declare function ProjectFolders_Combine( byval wszParent as DWSTRING, byval wszLeaf as DWSTRING ) as DWSTRING
declare function ProjectFolders_IsDescendantOf( byval wszPath as DWSTRING, byval wszAncestor as DWSTRING ) as boolean
declare function ProjectFolders_IsValidName( byval wszName as DWSTRING ) as boolean
declare function ProjectFolders_CatAllowsFolders( byval catIndex as long ) as boolean

' --- the table ---------------------------------------------------------------------------
declare sub      ProjectFolders_Clear()
declare function ProjectFolders_Count() as long
declare function ProjectFolders_Find( byval catIndex as long, byval wszPath as DWSTRING ) as long
declare function ProjectFolders_Add( byval catIndex as long, byval wszPath as DWSTRING ) as long
declare function ProjectFolders_RemoveAt( byval idx as long ) as boolean
declare function ProjectFolders_Exists( byval catIndex as long, byval wszPath as DWSTRING ) as boolean

' Map one path under a rename/dissolve. Pure, so the rule is assertable without a table.
declare function ProjectFolders_RebasePath( byval wszPath as DWSTRING, byval wszOld as DWSTRING, _
                                            byval wszNew as DWSTRING ) as DWSTRING
' Rename a folder, or -- with an empty wszNewPath -- dissolve it into its parent. Rewrites
' every descendant folder AND every affected document. See its definition.
declare function ProjectFolders_Rebase( byval catIndex as long, byval wszOldPath as DWSTRING, _
                                        byval wszNewPath as DWSTRING ) as boolean
' Move a folder and its whole subtree, possibly into another root group. See its definition.
declare function ProjectFolders_MoveFolder( byval catFrom as long, byval wszPath as DWSTRING, _
                                            byval catTo as long, byval wszNewParent as DWSTRING ) as boolean
