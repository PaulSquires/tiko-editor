'' ========================================================================================
'' modPaths -- the portable half of the {CURDRIVE} conversions.
''
'' ONE FUNCTION, AND IT MOVED BECAUSE SOMETHING FINALLY NEEDED IT. app/modPaths.bi has
'' declared all five of these since 7c step 1; four bodies stayed in src/modPaths.inc with
'' the rest of the shell. _check_app_standalone's own header named this one as movable --
'' "ProcessFromCurdriveApp (already pure PsCore), moves down as soon as someone does it" --
'' and step 5's scanner is who did: app/clsSymbolDb.inc calls it at three sites to find the
'' compiler's inc directory, and including that body in a second binary is what made the
'' reference undefined rather than merely inherited.
''
'' PsExePath, PsUCase, PsLeft, PsMid. No Win32, no AfxNova, nothing to convert.
''
'' THE OTHER FOUR STAY, and not for the same reason each:
''   * FilenameOriginalCase is real Win32 (CreateFileW + GetFinalPathNameByHandleW) and
''     needs a PsCore canonical-path call before it can move. It is on the link-debt list.
''   * ProcessToCurdriveApp / To- and FromCurdriveProject read gApp.ProjectFilename and
''     friends, which is a larger question than this commit.
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
