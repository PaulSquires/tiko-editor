'' ========================================================================================
'' modPaths -- the portable half of the {CURDRIVE} conversions.
''
'' TWO FUNCTIONS NOW. app/modPaths.bi has declared all five since 7c step 1; the bodies come
'' down one at a time, as something needs them.
''
''   * ProcessFromCurdriveApp came down in step 5, when app/clsSymbolDb.inc started calling
''     it at three sites and a second binary including that body made the reference
''     undefined rather than merely inherited.
''   * FilenameOriginalCase came down in step 8. See its own header below: it was on the
''     link-debt list waiting for "a PsCore canonical-path call", and that call already
''     existed.
''
'' PsExePath, PsUCase, PsLeft, PsMid, PsFileRealCase, PsPathToNative. No Win32, no AfxNova.
''
'' THE OTHER THREE STAY: ProcessToCurdriveApp and To-/FromCurdriveProject read
'' gApp.ProjectFilename and friends, which is a larger question than any of these commits.
'' ========================================================================================

#pragma once

#include once "modPaths.bi"


' ========================================================================================
' Process prefix {CURDRIVE} and convert to the current drive letter.
'
' MOVED VERBATIM from src/modPaths.inc -- same body, same comments, so a diff of the two
' shows nothing but the relocation.
' ========================================================================================
function ProcessFromCurdriveApp( byval wszFilename as DWSTRING ) as DWSTRING
    ' for each folder location determine if it resides on the same drive as
    ' the editor. if it does then substitute the replaceable parameter
    ' {CURDRIVE} for the drive letter. This allows you to easily run the editor
    ' on different media (eg. thumb drive) that may be assigned a different
    ' drive letter.
    dim as DWSTRING wszText = PsExePath

    if PsUCase(PsLeft(wszFilename, 10)) = wstr("{CURDRIVE}") then
        wszFilename = PsLeft(wszText, 1) & PsMid(wszFilename, 11)
    end if

    return wszFilename
end function


' ========================================================================================
' Get a path's ORIGINAL CASE as the filesystem holds it.
'
' WHY ANYTHING CALLS THIS: the fbcParser scans intern filenames UPPERCASED, so every
' scan-sourced name that reaches the UI -- panel captions, tab captions, TODO rows, search
' results -- is normalised through here or it SHOUTS.
'
' ---- MOVED DOWN IN 7c STEP 8, AND THE BLOCKER HAD ALREADY GONE -------------------------
'
' This was real Win32 -- CreateFileW plus GetFinalPathNameByHandleW -- and sat on
' _check_app_standalone's link-debt list under the note "needs a PsCore canonical-path call
' first". PsCore had one the whole time: PsFileRealCase walks each component against the
' real directory entries and prefers an exact match, so a directory legitimately holding
' both README and readme resolves to the one that was asked for.
'
' TWO BINARIES WERE ANSWERING THIS DIFFERENTLY, which is the reason to move it rather than
' to copy it. tiko resolved case; the portable shell returned its argument. Both key the
' same filename-keyed symbol database, and a key is not a place for two opinions.
'
' ---- WHAT CHANGED FOR tiko, STATED PLAINLY --------------------------------------------
'
' GetFinalPathNameByHandleW(FILE_NAME_NORMALIZED) resolved SYMLINKS, JUNCTIONS and SUBST'd
' drives to their final target as well as fixing the case. This does not. Open a file
' through a junction and tiko now shows the path you gave it rather than the target it
' points at -- which is arguably the better answer for a caption, and is a change either
' way. Decided deliberately, not inherited.
'
' ---- THE FALLBACK IS THE CONTRACT ------------------------------------------------------
'
' PsFileRealCase returns EMPTY when a component does not exist, and the files this is asked
' about are routinely gone: deleted since the scan, or synthetic names like UNTITLED that
' were never on disk. An empty answer therefore means KEEP WHAT WE WERE GIVEN -- exactly
' what the Win32 version did on INVALID_HANDLE_VALUE.
'
' ---- AND THE SEPARATORS GO BACK --------------------------------------------------------
'
' PsCore's canonical form is FORWARD SLASHES, everywhere, including Windows. Every consumer
' of this result stores or compares BACKSLASHED paths: gApp.GetDocumentPtrByFilename matches
' clsDocument.DiskFilename, which came from a command line or a dialog. Hand back
' "C:/dev/tiko/src/tiko.bas" and the lookup misses -- so a panel opens a SECOND copy of a
' file that is already in a tab, which is not an error anywhere.
'
' ---- WHAT IT COSTS ---------------------------------------------------------------------
'
' ONE DIRECTORY LISTING PER PATH COMPONENT, where the Win32 version was one open and one
' query. Fine for a click; the Functions pane calls it once per file in the project, so the
' cost is measured there rather than assumed here.
' ========================================================================================
function FilenameOriginalCase( byval wszFilename as DWSTRING ) as DWSTRING
    if PsLen( wszFilename ) = 0 then return wszFilename
    dim as DWSTRING wszReal = PsFileRealCase( wszFilename )
    if PsLen( wszReal ) = 0 then return wszFilename
    return PsPathToNative( wszReal )
end function
